#ifndef YJ_KVS_EVENT_LOOP_THREAD_POOL_H
#define YJ_KVS_EVENT_LOOP_THREAD_POOL_H

#include "EventLoopThread.h"
#include <vector>
#include <memory>

namespace yjKvs{

    class EventLoopThreadPool{
    public:
        EventLoopThreadPool(EventLoop* baseLoop, int threadsNum);
        ~EventLoopThreadPool() = default;

        void Start();

        //轮询获取下一个sub reactor
        EventLoop* GetNextLoop();

    private:
        EventLoop* baseLoop_;
        int threadsNum_;
        int next_;//轮询的游标
        std::vector<std::unique_ptr<EventLoopThread>> threads_;
        std::vector<EventLoop*> loops_;//缓存子循环的指针
    };
}

#endif