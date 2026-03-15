#include "ThreadPool.h"
#include "../utils/Logger.h"
#include <iostream>
#include <chrono>

using namespace yjKvs;

int main(){
    LOG_INFO << "--- TEST START ---";

    ThreadPool pool(4, 16);

    pool.Start();
    std::cout << "Pool started." << std::endl;
    
    for(int i = 0; i < 5; i++) {
        // 1. 先把任务逻辑定义好，赋值给一个叫 myTask 的变量
        // 注意：[i] 表示把当前循环的 i 的值“按值捕获”到函数内部
        auto myTask = [i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            LOG_INFO << "Normal task: " << i << " running in thread: ";
        };

        // 2. 然后再干干净净地把 myTask 提交进线程池
        bool res = pool.Submit(myTask);
        
        if (!res) {
            LOG_ERROR << "Failed to submit task " << i;
        }
    }

    pool.Submit([]() {
        LOG_INFO << "Evil task is starting... I will sleep for 20 seconds!";
        // 睡 20 秒，远超 ThreadPool 默认的 10 秒超时时间！
        std::this_thread::sleep_for(std::chrono::seconds(20)); 
    });

    LOG_INFO << "All tasks submitted, main thread waiting 2s...";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    LOG_INFO << "Calling Stop(10000ms)...";
    pool.Stop();
    
    LOG_INFO << "Main thread forcefully exits and survives!";

    return 0;
}
