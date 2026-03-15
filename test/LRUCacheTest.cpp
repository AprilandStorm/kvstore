#include "../include/LRUCache.h"
#include "../include/HashLRUCache.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <string>
#include <iomanip>
#include <random>

using namespace yjKvs;
using namespace std::chrono;

// --- 压测参数配置 ---
const int THREAD_NUM = 8;          // 模拟8个并发工作线程
const int OP_PER_THREAD = 5000000;  // 每个线程执行50万次操作（总计400万次）
const int KEY_POOL_SIZE = 10000;   // 缓存键池大小（模拟一定的哈希冲突和淘汰）

// 预生成测试数据，避免在压测计时中混入字符串拼接的开销
std::vector<std::string> keyPool;
void PrepareData() {
    std::cout << "Preparing test data..." << std::endl;
    for (int i = 0; i < KEY_POOL_SIZE; ++i) {
        keyPool.push_back("key_" + std::to_string(i));
    }
}

// 核心压测模板函数
template<typename CacheType>
double RunBenchmark(CacheType& cache, const std::string& cacheName) {
    std::vector<std::thread> threads;
    
    // 记录开始时间
    auto start = high_resolution_clock::now();

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back([&cache, i]() {
            // 每个线程使用独立的随机数种子
            std::mt19937 rng(12345 + i); 
            std::uniform_int_distribution<int> dist(0, KEY_POOL_SIZE - 1);
            
            std::string value;
            for (int j = 0; j < OP_PER_THREAD; ++j) {
                // 随机获取一个 key
                const std::string& key = keyPool[dist(rng)];
                
                // 模拟真实的混合读写场景：先 Get，如果没有就 Put
                if (!cache.Get(key, value)) {
                    cache.Put(key, "dummy_value");
                }
            }
        });
    }

    // 等待所有线程执行完毕
    for (auto& t : threads) {
        t.join();
    }

    // 记录结束时间
    auto end = high_resolution_clock::now();
    duration<double> diff = end - start;
    
    return diff.count();
}

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "      LRU vs HashLRU Benchmark         " << std::endl;
    std::cout << "=======================================" << std::endl;
    std::cout << "Threads: " << THREAD_NUM << std::endl;
    std::cout << "Total Operations: " << THREAD_NUM * OP_PER_THREAD << std::endl;
    
    PrepareData();
    std::cout << "Data ready. Starting benchmark...\n" << std::endl;

    // 1. 测试传统全局锁 LRU
    LRUCache lru(5000); 
    double lruTime = RunBenchmark(lru, "Standard LRU");

    // 2. 测试 16 分片 HashLRU
    HashLRUCache hashLru(5000, 16);
    double hashLruTime = RunBenchmark(hashLru, "16-Shard HashLRU");

    // --- 打印霸气的性能对比表格 ---
    std::cout << "\n---------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "Algorithm" << "| " << "Time (Seconds)" << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    std::cout << std::left << std::setw(20) << "Standard LRU" << "| " << std::fixed << std::setprecision(4) << lruTime << " s" << std::endl;
    std::cout << std::left << std::setw(20) << "HashLRU (16 Shards)" << "| " << std::fixed << std::setprecision(4) << hashLruTime << " s" << std::endl;
    std::cout << "---------------------------------------" << std::endl;
    
    double speedup = lruTime / hashLruTime;
    std::cout << "\n PERFORMANCE LEAP: HashLRU is " << std::fixed << std::setprecision(2) << speedup << "x FASTER!" << std::endl;
    std::cout << "=======================================\n" << std::endl;

    return 0;
}


/*=======================================
      LRU vs HashLRU Benchmark         
=======================================
Threads: 8
Total Operations: 40000000
Preparing test data...
Data ready. Starting benchmark...


---------------------------------------
Algorithm           | Time (Seconds)
---------------------------------------
Standard LRU        | 66.3169 s
HashLRU (16 Shards) | 8.0413 s
---------------------------------------

PERFORMANCE LEAP: HashLRU is 8.25x FASTER!
=======================================*/