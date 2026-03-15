#ifndef YJ_KVS_LRU_CACHE_H
#define YJ_KVS_LRU_CACHE_H

#include <string>
#include <unordered_map>
#include <mutex> 

namespace yjKvs{
    struct CacheNode{
        std::string key;
        std::string value;
        CacheNode* prev;
        CacheNode* next;

        CacheNode() : prev(nullptr), next(nullptr) {}
        CacheNode(std::string k, std::string v)
            : key(std::move(k)), value(std::move(v)), prev(nullptr), next(nullptr) {}
    };

    class LRUCache{
    public:
        explicit LRUCache(size_t capacity);
        ~LRUCache();

        bool Get(const std::string& key, std::string& value);

        void Put(const std::string& key, const std::string& value);

        void Delete(const std::string& key);
    
    private:
        void MoveToHead(CacheNode* node);

        void AddToHead(CacheNode* node);

        void MoveNode(CacheNode* node);

        CacheNode* MoveTail();
    
    private:
        size_t capacity_;
        size_t size_;

        std::unordered_map<std::string, CacheNode*> cacheMap_;

        CacheNode* dummyHead_;
        CacheNode* dummyTail_;

        mutable std::mutex mtx_;
    };
}

#endif