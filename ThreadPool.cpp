#include "ThreadPool.h"

#include <iostream>
#include <cassert>

using namespace swings;

ThreadPool::ThreadPool(int numWorkers)
    : stop_(false)
    {
        numWorkers = numWorkers <= 0 ? 0 : numWorkers;
        threads_.emplace_back([this]() {
            JobFunction func;
            {
                std::unique_lock<std::mutex> lock(lock_);
                while(jobs_.empty() && !stop_)
                    cond_.wait(lock);
                if(jobs_.empty() && stop_)
                    return;
                func = jobs_.front();
                jobs_.pop();
            }
            if(func) {
                func();
            }
        });
    }

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        stop_ = true;
    }
    cond_.notify_all();
    for(auto & thread : threads_) {
        thread.join();
    }
}

void ThreadPool::pushJob(const JobFunction & job)
{
    {
        std::unique_lock<std::mutex> lock(lock_);
        jobs_.push(job);
    }
    cond_.notify_one();
}