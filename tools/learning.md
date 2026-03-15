推荐项目代码结构
kvstore/

│

├── CMakeLists.txt

│

├── src/

│

│ ├── main.cpp

│

│ ├── network/ # 网络层

│ │ ├── EventLoop.h

│ │ ├── EventLoop.cpp

│ │ ├── TcpServer.h

│ │ ├── TcpServer.cpp

│ │ ├── Connection.h

│ │ └── Connection.cpp

│

│ ├── protocol/ # 协议解析

│ │ ├── KVProtocol.h

│ │ ├── KVProtocol.cpp

│ │ ├── Command.h

│ │ └── CommandParser.cpp

│

│ ├── cache/ # 内存缓存

│ │ ├── LRUCache.h

│ │ └── LRUCache.cpp

│

│ ├── storage/ # 存储层

│ │ ├── LevelDBStore.h

│ │ └── LevelDBStore.cpp

│

│ ├── thread/ # 并发模块

│ │ ├── ThreadPool.h

│ │ ├── ThreadPool.cpp

│ │ ├── TaskQueue.h

│ │ └── TaskQueue.cpp

│

│ ├── service/ # 业务处理

│ │ ├── KVService.h

│ │ └── KVService.cpp

│

│ └── utils/

│ ├── Logger.h

│ └── Config.h

│

└── tests/


逻辑结构：
┌───────────────┐

│ Client │

│ (CLI / SDK) │

└───────┬───────┘

│ TCP

▼

┌────────────────┐

│ 网络层 (Reactor) │

│ libevent + epoll │

│ 事件循环/连接管理 │

└───────┬────────┘

│

▼

┌────────────────┐

│ 协议解析层 │

│ KV Protocol │

│ GET / SET / DEL │

└───────┬────────┘

│

▼

┌────────────────┐

│ 业务处理层 │

│ Command Handler │

└───────┬────────┘

│

┌──────────┴──────────┐

▼ ▼

┌───────────────┐ ┌────────────────┐

│ LRU Cache │ │ Async TaskQueue │

│ 内存热点缓存 │ │ 后台任务队列 │

└───────┬────────┘ └────────┬───────┘

│ │

▼ ▼

┌──────────────┐ ┌───────────────┐

│ LevelDB │ │ WorkerThread │

│ 持久化存储 │ │ 后台写入线程 │

└──────────────┘ └───────────────┘


代码数量 ：

threadPool : 127
taskqueue : 110
log : 103
LRU : 225

eventloop : 85
tcpserver : 186
eventloopthread : 81
eventloopthreadpool : 60
connection : 235

command: 67
kvprotocol: 80
kvservice : 182
leveldbstore : 85
 
total: 2000+


设计并实现了基于 Hash 分片的高并发 LRU 缓存引擎。通过引入细粒度分片机制（16 Shards），彻底消除了传统全局锁在多线程环境下的竞争瓶颈。在 8 核并发环境下的 4000 万次读写压测中，系统吞吐量实现 8.25 倍的超线性提升，压测耗时从 66 秒陡降至 8 秒。

我们的架构和 muduo 接近吗？
在“宏观设计思想”上，简直是一模一样！
你的目录结构（EventLoop、TcpServer、Connection）完全复刻了 muduo 最核心的 “Reactor 模式 + One Loop Per Thread” 思想。
EventLoop 就是那个永不停止的事件分发器。
TcpServer 负责大盘管理和新连接的接收。
Connection 负责具体某一个客户端的读写。

[main.cpp]
   │
   ├── 创建 Main EventLoop (主反应堆，专门 accept)
   │
   └── 创建 TcpServer (把 Main EventLoop 交给它)
          │
          ├── 1. 内部创建 EventLoopThreadPool (后台创建 4 个 Sub EventLoop 线程)
          │
          ├── 2. 监听到新 fd (客人来了)
          │
          ├── 3. 找 ThreadPool 要一个 Sub EventLoop
          │
          ├── 4. 创建 Connection(fd, Sub EventLoop) 
          │      └─> 此时，这个 Connection 的后续所有收发数据，
          │          都由这个后台 Sub EventLoop 线程接管了！Main 线程立刻解脱！
          │
          └── 5. 把 Connection 放入 connections_ map (登记花名册，保活)


=====================================
  yjKvs Benchmark Tool Started...
  Threads (Connections): 50
  Requests per thread  : 2000
  Total Requests       : 100000
=====================================
[TEST COMPLETE]
Time taken : 6.776 seconds
Total Req  : 100000
QPS        : 14758 req/sec
Avg Latency: 3.25282 ms


=====================================
  yjKvs Benchmark Tool Started...
  Threads (Connections): 200
  Requests per thread  : 1000
  Total Requests       : 200000
=====================================

[TEST COMPLETE]
Time taken : 1.31 seconds
Total Req  : 200000
QPS        : 152672 req/sec
Avg Latency: 1.17118 ms


编译项目时，在 CMakeLists.txt 里控制 CMAKE_BUILD_TYPE 即可。
测试时：cmake -DCMAKE_BUILD_TYPE=Debug .. （会打印日志）
压测/上线时：cmake -DCMAKE_BUILD_TYPE=Release .. （CMake 会自动注入 -DNDEBUG 宏，所有 LOG_INFO 瞬间消失，性能拉满！）


# 1. 初始化
git init

# 2. 排除掉不该上传的二进制文件（非常重要！）
echo "build/" > .gitignore
echo "db_data/" >> .gitignore
echo "bin/" >> .gitignore

# 3. 添加文件
git add .
git commit -m "feat: 实现异步批量写回、同Key路由一致性及对象池优化，QPS破15万"

# 4. 关联远程仓库并推送
git branch -M main
git remote add origin https://github.com/你的用户名/kvstore.git
git push -u origin main


直接修改现有地址（最推荐，一击必杀）
这就好比你直接在快递单上把旧地址划掉，写上新地址。

1. 查看现在的地址（确认一下当前绑定的库）：

Bash
git remote -v
(你应当会看到刚才绑定的 https://github.com/AprilandStorm/kvstore.git)

2. 直接修改 origin 的指向：

Bash
git remote set-url origin https://github.com/你的新用户名/你的新仓库名.git
(把后面的网址换成你刚才在 GitHub 上新建的、真正想推送的空仓库地址)

3. 重新推送并绑定：

Bash
git push -u origin main
搞定！以后的代码就会全部发往你的新仓库了。