#ifndef YJ_KVS_THREAD_POOL_H
#define YJ_KVS_THREAD_POOL_H

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace yjKvs {

    class ThreadPool {
    public:
        using Task = std::function<void()>;

        explicit ThreadPool(size_t threadsNum, size_t maxQueueSize = 10000);
        ~ThreadPool();

        void Start();
        void Stop();

        //根据路由索引，塞给指定的队列
        bool SubmitTo(size_t index, Task task);

        size_t GetThreadCount() const { return workers_.size(); }

    private:
        //为了避免多个线程竞争同一把锁，把锁和队列打包成一个结构体
        struct WorkerData {
            std::queue<Task> taskQueue;
            std::mutex mtx;
            std::condition_variable cond;
            bool running = false;
        };

        std::vector<std::thread> workers_;
        std::vector<std::unique_ptr<WorkerData>> workerData_; //每个线程一套独立的队列和锁
        
        size_t maxQueueSize_;
        std::atomic<bool> isRunning_;
    };

}
#endif