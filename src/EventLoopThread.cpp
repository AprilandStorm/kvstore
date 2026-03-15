#include "../include/EventLoopThread.h"
#include "../utils/Logger.h"

namespace  yjKvs{

    EventLoopThread::EventLoopThread()
        : loop_(nullptr), exiting_(false) {}

    EventLoopThread::~EventLoopThread(){
        exiting_ = true;
        if(loop_){
            loop_->Quit();//通知子线程退出循环
        }
        if(thread_.joinable()){
            thread_.join();//等待子线程安全退出
        }
    }

    EventLoop* EventLoopThread::StartLoop(){
        //启动系统线程，执行threadfunc
        thread_ = std::thread(&EventLoopThread::ThreadFunc, this);

        //主线程阻塞等待，直到子线程真正把eventloop初始化完毕
        EventLoop* loop = nullptr;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this]{
                return loop_ != nullptr;
            });
            loop = loop_;
        }
        return loop;
    }

    void EventLoopThread::ThreadFunc(){
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            loop_ = &loop;
            cond_.notify_one();//唤醒正在等待的主线程
        }

        LOG_INFO << "Sub-Reactor thread started successfully!";

        //子线程处理自己客户端的读写事件
        loop.Loop();

        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = nullptr;
    }
}