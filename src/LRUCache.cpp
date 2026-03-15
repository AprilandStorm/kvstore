#include "../include/LRUCache.h"
#include <mutex>

namespace yjKvs {

    LRUCache::LRUCache(size_t capacity) : capacity_(capacity), size_(0) {
        // 初始化虚拟头尾节点
        dummyHead_ = new CacheNode();
        dummyTail_ = new CacheNode();
        dummyHead_->next = dummyTail_;
        dummyTail_->prev = dummyHead_;
    }

    LRUCache::~LRUCache() {
        CacheNode* curr = dummyHead_;
        while (curr != nullptr) {
            CacheNode* nextNode = curr->next;
            delete curr;
            curr = nextNode;
        }
    }

    bool LRUCache::Get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = cacheMap_.find(key);
        if (it == cacheMap_.end()) {
            return false; // 没找到
        }

        CacheNode* node = it->second;
        value = node->value;
        MoveToHead(node);
        
        return true;
    }

    void LRUCache::Put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mtx_);

        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            CacheNode* node = it->second;
            node->value = value;
            MoveToHead(node);
        } else {
            CacheNode* newNode = new CacheNode(key, value);
            cacheMap_[key] = newNode;
            AddToHead(newNode);
            size_++;

            // 如果超出了最大容量，执行淘汰策略
            if (size_ > capacity_) {
                CacheNode* tail = MoveTail();
                cacheMap_.erase(tail->key); // 从哈希表中抹除
                delete tail;                // 彻底释放内存
                size_--;
            }
        }
    }

    void LRUCache::Delete(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        auto it = cacheMap_.find(key);
        if (it != cacheMap_.end()) {
            CacheNode* node = it->second;
            MoveNode(node);
            cacheMap_.erase(key);
            delete node;
            size_--;
        }
    }

    void LRUCache::MoveNode(CacheNode* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void LRUCache::AddToHead(CacheNode* node) {
        // 把它插在 dummyHead_ 和 原来的第一个真实节点之间
        node->prev = dummyHead_;
        node->next = dummyHead_->next;
        
        dummyHead_->next->prev = node;
        dummyHead_->next = node;
    }

    void LRUCache::MoveToHead(CacheNode* node) {
        MoveNode(node);
        AddToHead(node);
    }

    CacheNode* LRUCache::MoveTail() {
        CacheNode* res = dummyTail_->prev;
        MoveNode(res);
        return res;
    }

}