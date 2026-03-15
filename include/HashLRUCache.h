#ifndef YJ_KVS_HASH_LRU_CACHE_H
#define YJ_KVS_HASH_LRU_CACHE_H

#include "LRUCache.h"
#include <vector>
#include <memory>
#include <string>

namespace yjKvs {

    // 分片LRU缓存管理类
    class HashLRUCache {
    public:
        //传入系统的总容量和想要切分的分片数量（默认 16）
        explicit HashLRUCache(size_t capacity, size_t shardNum = 16);
        
        ~HashLRUCache() = default;

        bool Get(const std::string& key, std::string& value);
        void Put(const std::string& key, const std::string& value);
        void Delete(const std::string& key);

    private:
        //根据Key计算出它应该去哪个分片
        size_t GetSliceIndex(const std::string& key) const;

        size_t sliceNum_;
        std::vector<std::unique_ptr<LRUCache>> sliceCaches_;
    };

} 

#endif