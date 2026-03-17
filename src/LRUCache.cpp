#include "../include/LRUCache.h"

namespace yjKvs {

    LRUCache::LRUCache(size_t capacity) 
        : capacity_(capacity), 
          head_(-1), 
          tail_(-1), 
          freeListHead_(0) {
        
        if (capacity_ == 0){
            return;
        }

        //初始化物理内存连续的Object Pool
        //resize()将在启动时一次性向操作系统申请完整的连续内存段
        pool_.resize(capacity_);
        
        //初始化空闲资源单向链表 (Free List)
        //将所有预分配的节点在逻辑上串联起来，等待被激活分配
        for (int32_t i = 0; i < capacity_ - 1; ++i) {
            pool_[i].next = i + 1;
        }
        //尾节点边界处理
        pool_[capacity_ - 1].next = -1; 

        //预先为哈希表分配足够的桶 (Buckets) 空间
        //此举旨在防止程序运行期间触发昂贵的Rehash动作导致性能抖动
        map_.reserve(capacity_);
    }

    bool LRUCache::Get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false; //Cache Miss
        }

        //获取该键在物理数组中的真实索引
        int32_t index = it->second;
        value = pool_[index].value;
        
        //更新LRU活跃权重，将其提升至双向链表头部
        MoveToHead(index);
        return true;
    }

    void LRUCache::Put(const std::string& key, const std::string& value) {
        if (capacity_ == 0) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it != map_.end()) {
            //命中活跃缓存场景
            //执行原地值更新操作，并重置时间权重
            int32_t index = it->second;
            pool_[index].value = value;
            MoveToHead(index);
        } else {
            //未命中活跃缓存，需进行节点分配或复用
            int32_t newNodeIndex = -1;

            if (freeListHead_ != -1) {
                //缓存未达容量阈值，从空闲单链表剥离一个干净的节点
                newNodeIndex = freeListHead_;
                freeListHead_ = pool_[freeListHead_].next; //更新Free List头指针
            } else {
                //缓存已满，触发LRU淘汰协议与尾节点就地复用
                newNodeIndex = tail_; 
                
                //解除该内存块与旧Key的映射关系
                map_.erase(pool_[newNodeIndex].key);
                
                //将其从活跃双向链表的尾部逻辑剥离
                RemoveNode(newNodeIndex); 
            }

            //无论节点来源于Free List还是淘汰复用，均执行原地覆盖更新
            //彻底规避底层的malloc/free系统调用
            pool_[newNodeIndex].key = key;
            pool_[newNodeIndex].value = value;

            //重建新键的路由索引，并将该节点挂载为活跃头节点
            map_[key] = newNodeIndex;
            AddToHead(newNodeIndex);
        }
    }

    void LRUCache::Delete(const std::string& key) {
        if (capacity_ == 0) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_.find(key);
        if (it == map_.end()) {
            return; //根本不存在，直接返回
        }

        //锁定目标节点的数组下标
        int32_t index = it->second;

        //抹除路由表索引
        map_.erase(it);

        //从活跃LRU双向链表中剥离
        RemoveNode(index);

        //清空残余数据
        //这一步是为了释放底层std::string可能占用的堆内存，防止内存虚高
        pool_[index].key.clear();
        pool_[index].value.clear();

        //头插法归还给Free List
        //这个节点等待下一个Put操作认领
        pool_[index].next = freeListHead_;
        freeListHead_ = index;
    }

    //内部双向链表拓扑关系重构：剥离指定节点
    void LRUCache::RemoveNode(int32_t index) {
        int32_t prevIndex = pool_[index].prev;
        int32_t nextIndex = pool_[index].next;

        if (prevIndex != -1) {
            pool_[prevIndex].next = nextIndex;
        } else {
            //若该节点原本为头节点，则需顺延head_游标
            head_ = nextIndex; 
        }

        if (nextIndex != -1) {
            pool_[nextIndex].prev = prevIndex;
        } else {
            //若该节点原本为尾节点，则需前移tail_游标
            tail_ = prevIndex; 
        }

        pool_[index].prev = -1;
        pool_[index].next = -1;
    }

    //内部双向链表拓扑关系重构：前置插入节点
    void LRUCache::AddToHead(int32_t index) {
        pool_[index].prev = -1;
        pool_[index].next = head_;

        if (head_ != -1) {
            pool_[head_].prev = index;
        }
        head_ = index;

        //边界态处理：若链表为空，则注入的首个节点同时兼任尾节点
        if (tail_ == -1) {
            tail_ = index; 
        }
    }

    //内部双向链表拓扑关系重构：节点提权
    void LRUCache::MoveToHead(int32_t index) {
        if (index == head_) {
            return; //命中已经是最高权重的节点，提前阻断不必要的CPU开销
        }
        RemoveNode(index);
        AddToHead(index);
    }

}