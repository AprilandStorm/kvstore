#ifndef YJ_KVS_EVENT_LOOP_THREAD_
#define YJ_KVS_EVENT_LOOP_THREAD_

#include "EventLoop.h"
#include <mutex>
#include <thread>
#include <condition_variable>

namespace yjKvs{
    
    class EventLoopThread{
    public:
        EventLoopThread();
        ~EventLoopThread();

        //启动后台线程，并返回该线程内跑起来的EventLoop指针
        EventLoop* StartLoop();

    private:
        void ThreadFunc();

        EventLoop* loop_;//指向后台线程里创建的那个EventLoop
        bool exiting_;
        std::thread thread_;
        std::mutex mutex_;
        std::condition_variable cond_;
    };
}

#endif