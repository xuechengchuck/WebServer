#include "Timer.h"
#include "HttpRequest.h"

#include <cassert>

using namespace swings;

void TimerManager::addTimer(HttpRequest * request, const int & timeout, const TimeOutCallBack & cb)
{
    std::unique_lock<std::mutex> lock(lock_);
    assert(request != nullptr);
    updateTime();
    Timer * timer = new Timer(now_ + MS(timeout), cb);
    timerQueue_.push(timer);
    if(request -> getTimer() != nullptr)
        delTimer(request);
    request -> setTimer(timer);
}

void TimerManager::delTimer(HttpRequest * request)
{
    assert(request != nullptr);
    Timer * timer = request -> getTimer();
    if(timer == nullptr)
        return;
    timer -> del();
    request -> setTimer(nullptr); // 防止成为悬垂指针
}

void TimerManager::handleExpireTimers()
{
    std::unique_lock<std::mutex> lock(lock_);
    updateTime();
    while (!timerQueue_.empty())
    {
        Timer * timer = timerQueue_.top();
        assert(timer != nullptr);
        if(timer -> isDeleted()) {
            timerQueue_.pop();
            delete timer;
            //timer = nullptr;
            continue;
        }
        if(std::chrono::duration_cast<MS>(timer -> getExpireTime() - now_ ).count() > 0)
            return;
        timer -> runCallBack();
        timerQueue_.pop();
        delete timer;
        //timer = nullptr;
    }
}

int TimerManager::getNextExpireTime()
{
    std::unique_lock<std::mutex> lock(lock_);
    updateTime();
    int res = -1;
    while(!timerQueue_.empty()) {
        Timer * timer = timerQueue_.top();
        assert(timer != nullptr);
        if(timer -> isDeleted()) {
            timerQueue_.pop();
            delete timer;
            //timer = nullptr;
            continue;
        }
        res = std::chrono::duration_cast<MS>(timer -> getExpireTime() - now_).count();
        res = (res < 0) ? 0 : res;
        break;
    }
    return res;
}