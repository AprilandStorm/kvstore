#ifndef YJ_KVS_KV_SERVICE_H
#define YJ_KVS_KV_SERVICE_H

#include "TcpServer.h"
#include "HashLRUCache.h"
#include "Command.h"
#include "LevelDBStore.h"
#include "ThreadPool.h"
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace yjKvs {

    //核心业务管家：网络层和内存缓存的中间层
    class KVService {
    public:
        using ConnectionPtr = std::shared_ptr<Connection>;
        //定义命令处理函数的签名 (Handler)
        using CommandHandler = std::function<void(const ConnectionPtr&, const Command&)>;

        //构造函数，注入TcpServer并初始化底层分片缓存的容量
        KVService(TcpServer* server, size_t cacheCapacity, const std::string& dbPath);
        ~KVService();

        //启动业务引擎，将处理逻辑挂载到网络层
        void Start();

    private:
        //专门应付网络层的回调函数
        void OnConnection(const ConnectionPtr& conn);
        void OnMessage(const ConnectionPtr& conn, const Command& cmd);

        // 初始化路由表
        void RegisterHandlers();

        /* ==========================================
                    具体指令的执行单元
         ==========================================*/
        void HandleSet(const ConnectionPtr& conn, const Command& cmd);
        void HandleGet(const ConnectionPtr& conn, const Command& cmd);
        void HandleDel(const ConnectionPtr& conn, const Command& cmd);
        void HandlePing(const ConnectionPtr& conn, const Command& cmd);
        void HandleUnknown(const ConnectionPtr& conn, const Command& cmd);
        void FlusherLoop();

    private:
        TcpServer* server_; //引用底层的网络管理者
        
        std::unique_ptr<HashLRUCache> cache_; 
        std::unique_ptr<LevelDBStore> store_;

        //命令路由表
        std::unordered_map<CommandType, CommandHandler> handlers_;

        //业务处理线程池
        std::unique_ptr<ThreadPool> workerPool_;

        //异步刷盘
        std::vector<WriteOperation> dirtyQueue_; //存放脏数据
        std::mutex flusherMutex_;
        std::condition_variable flusherCond_;
        std::thread flusherThread_;
        std::atomic<bool> flusherRunning_;
    };

}

#endif