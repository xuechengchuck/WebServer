#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "myUtils.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Timer.h"

#include <iostream>
#include <functional> // function

#include <cassert>
#include <cstring> // strerror
#include <unistd.h> // close, read
#include <sys/socket.h> // sockaddr_in

using namespace swings;

HttpServer::HttpServer(int port, int numThread)
    : port_(port),
      listenFd_(myUtils::createListenFd(port_)),
      listenRequest_(new HttpRequest(listenFd_)),
      epoll_(new Epoll()),
      threadPool_(new ThreadPool(numThread)),
      timerManager_(new TimerManager())
{
    assert(listenFd_ >= 0);
}

HttpServer::~HttpServer()
{
    //::close(listenFd_);
}

void HttpServer::run()
{
    // 注册监听套接字到epoll（可读事件，ET模式）
    epoll_ -> add(listenFd_, listenRequest_.get(), (EPOLLIN | EPOLLET));
    epoll_ -> setOnConnection(std::bind(&HttpServer::__acceptConnection, this));
    epoll_ -> setOnClsoeConnection(std::bind(&HttpServer::__closeConnection, this, std::placeholders::_1));
    epoll_ -> setOnRequest(std::bind(&HttpServer::__doRequest, this, std::placeholders::_1));
    epoll_ -> setOnResponse(std::bind(&HttpServer::__doResponse, this, std::placeholders::_1));

    while (1)
    {
        int timeMS = timerManager_ -> getNextExpireTime();
        int eventsNum = epoll_ -> wait(timeMS);

        if(eventsNum > 0) {
            epoll_ -> handleEvent(listenFd_, threadPool_, eventsNum); // 分发事件处理
        }
        timerManager_ -> handleExpireTimers();
    }
    
}

void HttpServer::__acceptConnection()
{
    while (1) {
        int acceptFd = ::accept4(listenFd_, nullptr, nullptr, (SOCK_NONBLOCK | SOCK_CLOEXEC));
        if(acceptFd == -1) {
            if(errno == EAGAIN)
                break;
            printf("[HttpServer::__acceptConnection] accept : %s\n", strerror(errno));
            break;
        }

        HttpRequest * request = new HttpRequest(acceptFd);
        timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this,  request));
        epoll_ -> add(acceptFd, request, (EPOLLIN | EPOLLONESHOT)); // 为该连接注册EPOLLIN和EPOLLONESHOT事件
    }
}

void HttpServer::__closeConnection(HttpRequest * request)
{
    int fd = request -> fd();
    if(request -> isWorking())
        return;
    timerManager_ -> delTimer(request);
    epoll_ -> del(fd, request, 0);
    delete request;
    request = nullptr;
    // close(fd);
}

void HttpServer::__doRequest(HttpRequest * request)
{
    timerManager_ -> delTimer(request);
    assert(request != nullptr);
    int fd = request -> fd();

    int readErrno;
    int nRead;
    nRead = request -> read(&readErrno);
    if(nRead == 0) {
        request -> setNoWorking();
        __closeConnection(request);
        return;
    }

    if(nRead < 0 && (readErrno != EAGAIN)) {
        request -> setNoWorking();
        __closeConnection(request);
        return;
    }

    if(nRead < 0 && (readErrno == EAGAIN)) {
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
        //timerManager_ -> delTimer(request);
        request -> setNoWorking();
        timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        return;
    }

    if(!request ->parseRequest()) {
        HttpResponse response(400, "", false);
        request ->appendOutBuffer(response.makeResponse());

        int writeErrno;
        request -> write(&writeErrno);
        request -> setNoWorking();
        __closeConnection(request);
        return;
    }

    if(request -> parseFinish()) {
        HttpResponse response(200, request -> getPath(), request -> keepAlive());
        request -> appendOutBuffer(response.makeResponse());
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    }
}

void HttpServer::__doResponse(HttpRequest * request)
{
    timerManager_ -> delTimer(request);
    assert(request != nullptr);
    int fd = request -> fd();

    int toWrite = request -> writableBytes(); // 一个请求对象有两个缓冲区，一个输入缓冲区，一个输出缓冲区，当客户端向服务端发数据时，将数据放到输入缓冲区中，服务端做出响应后，将响应内容放到输出缓冲区中
    if(toWrite == 0) {
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT, EPOLLONESHOT));
        timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        return;
    }

    int writeErrno;
    int nWrite;
    nWrite = request -> write(&writeErrno);

    if(nWrite < 0 && (writeErrno != EAGAIN)) {
        request -> setNoWorking();
        __closeConnection(request);
        return;
    }

    if(nWrite < 0 && (writeErrno == EAGAIN)) {
        epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
        return;
    }

    if(nWrite == toWrite) { // 服务端响应做完了
        if(request -> keepAlive()) {
            request -> resetParse();
            epoll_ -> mod(fd, request, (EPOLLIN | EPOLLONESHOT));
            request -> setNoWorking();
            timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
        } else {
            request -> setNoWorking();
            __closeConnection(request);
        }
        return;
    }

    // LT模式但是是EPOLLONESHOT，没有写完需要修改epoll事件表，这样可以重新通知发送数据，将没发送的数据继续发送
    epoll_ -> mod(fd, request, (EPOLLIN | EPOLLOUT | EPOLLONESHOT));
    request -> setNoWorking();
    timerManager_ -> addTimer(request, CONNECT_TIMEOUT, std::bind(&HttpServer::__closeConnection, this, request));
    return;
}