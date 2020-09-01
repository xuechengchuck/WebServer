#if !defined(__TIMER_H__)
#define __TIMER_H__

#include <iostream>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <cassert>

namespace swings{
    using Clock = std::chrono::high_resolution_clock;
    using TimeOutCallBack = std::function<void()>;
    using MS = std::chrono::milliseconds;
    using TimeStamp = Clock::time_point;

    class HttpRequest; 

    class Timer
    {
    private:
        /* data */
        TimeStamp expireTime_;
        bool delete_;
        TimeOutCallBack callBack_;
    public:
        Timer(const TimeStamp & when, const TimeOutCallBack & cb)
            : expireTime_(when),
              callBack_(cb),
              delete_(false)
            {
            }
        ~Timer() {}
        void del() { delete_ = true; } // 惰性删除
        TimeStamp getExpireTime() const { return expireTime_; }
        void runCallBack() { callBack_(); }
        bool isDeleted () { return delete_; }
    }; // class Timer

    struct cmp
    {
        /* data */
        bool operator()(Timer * a, Timer * b) {
            assert(a != nullptr && b != nullptr);
            return (a -> getExpireTime()) > (b -> getExpireTime()); // >是升序，即小顶堆，<是降序，即大顶堆
        }
    };
    

    class TimerManager
    {
    private:
        using TimerQueue = std::priority_queue<Timer*, std::vector<Timer*>, cmp>;
        TimeStamp now_;
        TimerQueue timerQueue_;
        std::mutex lock_;
    public:
        TimerManager()
            : now_(Clock::now()) {}
        ~TimerManager() {}

        void updateTime() { now_ = Clock::now(); }

        void addTimer(HttpRequest * request, const int & timeout, const TimeOutCallBack & cb);
        void delTimer(HttpRequest * request);
        int getNextExpireTime();
        void handleExpireTimers();
    }; // class TimerManager
    
}

#endif // __TIMER_H__