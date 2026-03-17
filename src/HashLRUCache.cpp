#include "../include/HashLRUCache.h"
#include <functional> //使用std::hash

namespace yjKvs {

    HashLRUCache::HashLRUCache(size_t capacity, size_t sliceNum) 
        : sliceNum_(sliceNum > 0 ? sliceNum : 16) { //防止传入0
        
        //向上取整计算每个分片的容量，保证总容量不缩水
        size_t perShardCapacity = (capacity + sliceNum_ - 1) / sliceNum_;
        if (perShardCapacity == 0) {
            perShardCapacity = 1; //每个分片至少能存1个元素
        }

        //实例化底层的sliceNum个LRUCache分片
        for (size_t i = 0; i < sliceNum_; ++i) {
            sliceCaches_.push_back(std::make_unique<LRUCache>(perShardCapacity));
        }
    }

    size_t HashLRUCache::GetSliceIndex(const std::string& key) const {
        //C++标准库自带的字符串哈希函数
        std::hash<std::string> hash_fn;
        //计算哈希值并对分片数量取模
        return hash_fn(key) & (sliceNum_ - 1);
    }

    bool HashLRUCache::Get(const std::string& key, std::string& value) {
        return sliceCaches_[GetSliceIndex(key)]->Get(key, value);
    }

    void HashLRUCache::Put(const std::string& key, const std::string& value) {
        sliceCaches_[GetSliceIndex(key)]->Put(key, value);
    }

    void HashLRUCache::Delete(const std::string& key) {
        sliceCaches_[GetSliceIndex(key)]->Delete(key);
    }

}