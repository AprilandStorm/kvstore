#include "../include/EventLoop.h"
#include "../utils/Logger.h" 

namespace yjKvs {

    EventLoop::EventLoop() {
        //创建libevent的核心调度器
        //底层会自动探测当前系统最高效的I/O多路复用机制
        base_ = event_base_new();
        if (!base_) {
            LOG_ERROR << "EventLoop: Failed to initialize event_base!";
            //直接抛出异常或exit
            exit(EXIT_FAILURE); 
        }
        
        //打印底层实际使用的机制
        LOG_INFO << "EventLoop initialized. Method: " << event_base_get_method(base_);
    }

    EventLoop::~EventLoop() {
        if (base_) {
            event_base_free(base_);
            LOG_INFO << "EventLoop destroyed. Cleaned up event_base.";
        }
    }

    void EventLoop::Loop() {
        if (!base_) return;

        LOG_INFO << "EventLoop starting dispatch loop...";
        
        // 【核心阻塞点】：启动事件循环！
        // 线程会一直卡在这里，内部死循环调用 epoll_wait。
        // 只有当没有任何事件监听，或者调用了 Quit() 时，这个函数才会返回。
        int ret = event_base_loop(base_, EVLOOP_NO_EXIT_ON_EMPTY);
        
        if (ret == 1) {
            LOG_WARN << "EventLoop stopped: No pending or active events.";
        } else if (ret == -1) {
            LOG_ERROR << "EventLoop stopped with an error!";
        } else {
            LOG_INFO << "EventLoop stopped gracefully.";
        }
    }

    void EventLoop::Quit() {
        if (base_) {
            // 处理完当前正在执行的活动事件后，再退出循环
            event_base_loopexit(base_, nullptr);
            LOG_INFO << "EventLoop Quit requested.";
        }
    }

}