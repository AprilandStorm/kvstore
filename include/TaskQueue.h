#ifndef YJ_KVS_TASK_QUEUE_H
#define YJ_KVS_TASK_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace yjKvs{
    //定义任务类型，无参数、无返回值的可调用对象
    /** 将数据和操作绑定在一起的执行逻辑
      * (使用了 std::function<void()>，主 Reactor 线程只需要利用 Lambda 表达式，
      * 把“要处理的数据”和“处理的代码”打包成一个黑盒（Task）扔进队列。
      * 如果用模板队列存数据，消费者线程就会变成一个极其臃肿的怪胎，需要判断各种类型。
    */
    using Task = std::function<void()>;
    
    class TaskQueue{
    public:
        //防雪崩队列容量
        explicit TaskQueue(size_t capacity);
        ~TaskQueue();

        //添加任务
        bool Push(Task task);

        //获取任务
        bool Pop(Task& task);

        //停止队列，唤醒所有阻塞的线程
        void Stop();
        
        size_t Size();
    
    private:
        std::queue<Task> queue_;
        size_t capacity_;
        bool stop_;

        std::mutex mtx_;
        std::condition_variable notEmpty_;//队列非空条件变量
        std::condition_variable notFull_;//队列非满条件变量
    };
}

#endif