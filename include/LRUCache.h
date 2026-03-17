#ifndef YJ_KVS_LRU_CACHE_H
#define YJ_KVS_LRU_CACHE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace yjKvs {

    //静态连续内存池节点
    struct LRUNode {
        std::string key;
        std::string value;
        //使用32位整型索引替代指针，实现侵入式链表
        //-1代表nullptr(无效索引)
        int32_t prev; 
        int32_t next;

        LRUNode() : prev(-1), next(-1) {}
    };

    class LRUCache {
    public:
        //构造函数显式要求传入缓存容量阈值
        explicit LRUCache(size_t capacity);
        ~LRUCache() = default; //std::vector与unordered_map的RAII机制将自动安全释放内存

        //核心读写接口
        bool Get(const std::string& key, std::string& value);
        void Put(const std::string& key, const std::string& value);
        void Delete(const std::string& key);

    private:
        //内部侵入式双向链表操作逻辑（调用方需保证已持有互斥锁）
        void MoveToHead(int32_t index);
        void AddToHead(int32_t index);
        void RemoveNode(int32_t index);

        size_t capacity_;
        std::mutex mutex_;

        //预分配的连续物理内存块
        std::vector<LRUNode> pool_;
        
        //活跃双向链表的虚拟头尾索引
        int32_t head_;
        int32_t tail_;
        
        //空闲节点单向链表 (Free List) 的头索引
        int32_t freeListHead_;

        //O(1)检索路由表：Key->内存池中的数组下标
        std::unordered_map<std::string, int32_t> map_;
    };

}

#endif