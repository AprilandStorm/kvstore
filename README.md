# yjKvs: yj KV Storage Engine

yjKvs是一款基于C++17开发的高性能分布式Key-Value存储引擎。项目融合了Reactor网络模型、多级缓存架构以及异步批量持久化技术。

## 性能指标
在200并发连接的压测环境下，单机性能表现如下：
SET(异步落盘) 约140,000 QPS，平均延迟 < 1.3ms

<!-- ## 核心特性
* **Multi-Reactor 网络模型:** 基于`libevent`实现主从 Reactor 架构。Main Reactor 负责连接分发，Sub Reactor 线程池利用`epoll`监听IO事件，充分榨干多核CPU性能。
* **双缓冲异步写回 (Write-Back):** 引入后台`Flusher`线程，通过 `std::vector::swap`魔法实现无锁化数据交换，结合LevelDB的 `WriteBatch` 批量落盘，将IO频率降低。
* **强路由一致性 (Hash Routing):** 针对写操作（SET/DEL），基于Key的哈希值进行线程池精准路由，确保同一Key的操作序列绝对串行，彻底避免并发乱序问题。
* **零碎化内存管理:** 实现 **Object Pool (对象池)** 管理`Connection`对象。利用智能指针自定义删除器 (Custom Deleter)，消除运行时内存碎片。
* **分片加锁 LRU 缓存:** 手写16段分片 `HashLRUCache`，极大降低高并发下的锁竞争。
* **Redis 协议兼容:** 完美支持RESP通信协议，具备Pipeline 粘包/半包循环解析能力。 -->

## 构建与运行

### 依赖环境
* Linux (Ubuntu 20.04+ 推荐)
* CMake (3.10+)
* libevent
* LevelDB

### 编译步骤
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```
### 启动服务
```
./tcptest
```

### 运行压测
```
cd tools
./benchmark
```
