#include "../include/ThreadPool.h"

namespace yjKvs {

    ThreadPool::ThreadPool(size_t threadsNum, size_t maxQueueSize) 
        : maxQueueSize_(maxQueueSize), isRunning_(false) {
        
        //为每个线程初始化独立的队列和锁
        for (size_t i = 0; i < threadsNum; ++i) {
            workerData_.push_back(std::make_unique<WorkerData>());
        }
    }

    ThreadPool::~ThreadPool() {
        Stop();
    }

    void ThreadPool::Start() {
        isRunning_ = true;
        for (size_t i = 0; i < workerData_.size(); ++i) {
            workerData_[i]->running = true;
            //启动线程，传入索引i
            workers_.emplace_back([this, i]() {
                auto& data = workerData_[i];
                while (data->running) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(data->mtx);
                        data->cond.wait(lock, [&data] { 
                            return !data->taskQueue.empty() || !data->running; 
                        });

                        if (!data->running && data->taskQueue.empty()) {
                            return; //线程安全退出
                        }

                        task = std::move(data->taskQueue.front());
                        data->taskQueue.pop();
                    }
                    //离开锁的作用域，执行任务
                    if (task) {
                        task();
                    }
                }
            });
        }
    }

    //精准投递任务
    bool ThreadPool::SubmitTo(size_t index, Task task) {
        if (!isRunning_ || index >= workerData_.size()) return false;

        auto& data = workerData_[index];
        {
            std::lock_guard<std::mutex> lock(data->mtx);
            if (data->taskQueue.size() >= maxQueueSize_) {
                return false; //队列满了，拒绝服务
            }
            data->taskQueue.push(std::move(task));
        }
        data->cond.notify_one(); //只唤醒这一个特定线程
        return true;
    }

    void ThreadPool::Stop() {
        if (!isRunning_) return;
        isRunning_ = false;

        for (auto& data : workerData_) {
            {
                std::lock_guard<std::mutex> lock(data->mtx);
                data->running = false;
            }
            data->cond.notify_all();
        }

        for (auto& thread : workers_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
}