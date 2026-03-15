#include "TaskQueue.h"
#include <utility>

namespace yjKvs{
    TaskQueue::TaskQueue(size_t capacity) 
        : capacity_(capacity), stop_(false) {}
    
    TaskQueue::~TaskQueue(){
        Stop();
    }

    bool TaskQueue::Push(Task task){
        std::unique_lock<std::mutex> lock(mtx_);
        
        //用while防止条件变量的虚假唤醒
        notFull_.wait(lock, [this](){
            return stop_ || queue_.size() < capacity_;
        });

        //系统正在停止，拒绝新任务
        if(stop_){
            return false;
        }

        queue_.push(std::move(task));
        notEmpty_.notify_one();
        
        return true;
    }

    bool TaskQueue::Pop(Task& task){
        std::unique_lock<std::mutex> lock(mtx_);
        
        notEmpty_.wait(lock, [this](){
            return stop_ || !queue_.empty();
        });

        //直到收到停止信号且队列里的任务都已经处理完了才真正退出
        if(stop_ && queue_.empty()){
            return false;
        }

        task = std::move(queue_.front());
        queue_.pop();
        notFull_.notify_one();//唤醒一个正在等待push的生产者线程

        return true;
    }

    void TaskQueue::Stop(){
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;

        //唤醒所有阻塞的线程，避免线程永久挂起
        notFull_.notify_all();
        notEmpty_.notify_all();
    }

    size_t TaskQueue::Size(){
        std::lock_guard<std::mutex> lock(mtx_);

        return queue_.size();
    }
}

/*
- std::lock_guard 的设计目标就是：“构造时加锁，析构时解锁”，适用于函数作用域内全程持锁的简单场景。
- 特点：加锁后一直到函数结束才释放; 不需要中途解锁或条件等待; 没有异常安全问题（RAII 自动管理）
- std::lock_guard: 最简单的 RAII 锁包装器;不可移动; 构造时必须加锁;不支持手动解锁; 不支持支持条件变量;几乎无额外开销;适用场景为简单的函数级临界区保护	
- std::unique_lock：更灵活的通用锁管理器；可移动（moveable）需要转移所有权（move）；支持 defer_lock 延迟加锁；支持 unlock() / lock()手动解锁加锁；
  支持添加变量，但必须配合 condition_variable::wait()；内部有布尔标志记录状态，性能开销略高；适用于复杂控制（如条件等待、分段加锁）
*/

/**notify_all() (惊群效应)假设队列空了，10 个工作线程都在睡觉。
 * 此时生产者推进来 1 个任务，调用 notify_all()。
 * 10 个线程会同时惊醒，同时去抢同一把互斥锁 mutex_。
 * 最终只有 1 个线程抢到了锁并拿走了任务，剩下 9 个线程发现队列又空了，只好垂头丧气地回去接着睡。
 * 这 9 个线程的唤醒、抢锁、再睡眠，白白消耗了大量 CPU 资源。
*/

/**使用 notify_one() 必须建立在一个前提下：所有等待该条件变量的线程，做的是完全相同的事情。
 * 完美避免了惊群效应。推进来 1 个任务，就只叫醒 1 个线程起来干活，其他 9 个线程继续安稳睡觉。
 * 这在像你这种单机 QPS 要求达到 3w+ 的高并发底层存储引擎中，是压榨极限性能的必选项。
 * 在我们的线程池里，所有的 Worker 线程都是等价的（都是去拿任务执行），所以叫醒谁都一样。
 * 但如果在更复杂的业务中，线程被唤醒后处理逻辑不同，notify_one() 可能会叫醒错误的线程，导致死锁。
*/