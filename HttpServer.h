#ifndef __HTTP_SERVER_H__
#define __HTTP_SERVER_H__

#include <memory> // unique_ptr, shared_ptr
#include <mutex>

const int TIMEOUTMS = -1; // epoll_wait超时时间，-1表示不设超时
const int CONNECT_TIMEOUT = 500; // 连接默认超时时间
const int NUM_WORKERS = 4; // 线程池大小

namespace swings {
    // 前置声明，不需要包含HttpRequest.h和Epoll.h
    class HttpRequest;
    class Epoll;
    class ThreadPool;
    class TimerManager;

    class HttpServer {
    public:
        HttpServer(int port, int numThread);
        ~HttpServer();
        void run(); // 启动HTTP服务器

    private:
        void __acceptConnection(); // 接受新连接
        void __closeConnection(HttpRequest * request); // 关闭连接
        void __doRequest(HttpRequest * request); // 处理HTTP请求报文，这个函数由线程池调用
        void __doResponse(HttpRequest * request); // 对请求做出响应

    private:
        using ListenRequestPtr = std::unique_ptr<HttpRequest>;
        using EpollPtr = std::unique_ptr<Epoll>;
        using TimerManagerPtr = std::unique_ptr<TimerManager>;
        using ThreadPoolPtr = std::shared_ptr<ThreadPool>;
        
        int port_; // 监听端口
        int listenFd_; // 监听套接字
        ListenRequestPtr listenRequest_; // 监听套接字的HttpRequest实例
        EpollPtr epoll_; // epoll实例
        TimerManagerPtr timerManager_; // 定时器管理器实例
        ThreadPoolPtr threadPool_; // 线程池实例
    };  // class HttpServer
} // namespace swings

#endif
