#include "../include/EventLoopThreadPool.h"
#include "../utils/Logger.h"

namespace yjKvs{
    EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, int threadsNum)
        : baseLoop_(baseLoop), threadsNum_(threadsNum), next_(0) {}
        
    void EventLoopThreadPool::Start(){
        for(int i = 0; i < threadsNum_; i++){
            auto t = std::make_unique<EventLoopThread>();

            //启动子线程，并把拿到的subreactor指针存起来
            loops_.push_back(t->StartLoop());
            threads_.push_back(std::move(t));
        }

        LOG_INFO << "EventLoopThreadPool started with " << threadsNum_ << " Sub-Reactors.";
    }

    EventLoop* EventLoopThreadPool::GetNextLoop(){
        EventLoop* loop = baseLoop_;

        //轮询，如果有子线程，就依次发牌
        if(!loops_.empty()){
            loop = loops_[next_];
            next_ = (next_ + 1) % loops_.size();
        }

        return loop;//如果loops_为empty，则退化为单Reactor模型
    }
}