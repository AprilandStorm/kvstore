#ifndef YJ_KVS_EVENT_LOOP_H
#define YJ_KVS_EVENT_LOOP_H

#include "event2/event.h"

namespace yjKvs {

    //  Reactor 核心事件循环
    class EventLoop {
    public:
        EventLoop();
        ~EventLoop();

        // 启动事件循环
        void Loop();

        // 退出事件循环
        void Quit();

        // 给TcpServer和Connection获取底层event_base的接口
        event_base* GetBase() const { 
            return base_; 
        }

    private:
        event_base* base_;
    };

}

#endif