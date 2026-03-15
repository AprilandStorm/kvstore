#include "../include/KVService.h"
#include "../include/KVProtocol.h"
#include "../utils/Logger.h"
#include "../include/Connection.h"

namespace yjKvs {

    KVService::KVService(TcpServer* server, size_t cacheCapacity, const std::string& dbPath)
        : server_(server), 
          cache_(std::make_unique<HashLRUCache>(cacheCapacity)),
          store_(std::make_unique<LevelDBStore>(dbPath)),
          workerPool_(std::make_unique<ThreadPool>(4, 10000)) {
            // 1. 先启动各种线程
            workerPool_->Start(); 
            RegisterHandlers();   
            flusherRunning_ = true;
            flusherThread_ = std::thread(&KVService::FlusherLoop, this);
            
            //冷启动预热
            LOG_INFO << "Starting Cache Warmup from LevelDB...";
            int warmupCount = 0;
            
            // 获取一个LevelDB迭代器，遍历整个数据库
            leveldb::Iterator* it = store_->GetDB()->NewIterator(leveldb::ReadOptions());
            for (it->SeekToFirst(); it->Valid(); it->Next()) {
                // 将数据塞入到内存Cache中
                cache_->Put(it->key().ToString(), it->value().ToString());
                warmupCount++;
                
                //安全限制：防止数据量太大把内存撑爆，最多只预热capacity个
                if (warmupCount >= cacheCapacity) break; 
            }
            delete it;
            LOG_INFO << "Cache Warmup complete. Loaded " << warmupCount << " keys into memory.";
        }

    KVService::~KVService() {        
        flusherRunning_ = false;
        flusherCond_.notify_all();
        if (flusherThread_.joinable()) {
            flusherThread_.join();
        }
    }

    void KVService::RegisterHandlers() {
        handlers_[CommandType::SET]  = [this](auto conn, auto cmd) { HandleSet(conn, cmd); };
        handlers_[CommandType::GET]  = [this](auto conn, auto cmd) { HandleGet(conn, cmd); };
        handlers_[CommandType::DEL]  = [this](auto conn, auto cmd) { HandleDel(conn, cmd); };
        handlers_[CommandType::PING] = [this](auto conn, auto cmd) { HandlePing(conn, cmd); };
        handlers_[CommandType::QUIT] = [](auto conn, auto cmd) { 
            conn->Send("+OK\r\n"); 
            conn->Shutdown(); 
        };
    }

    void KVService::Start() {
        //Lambda绑定
        //将KVService的成员函数，伪装成普通的function塞给TcpServer
        server_->SetConnectionCallback(
            [this](const ConnectionPtr& conn) { this->OnConnection(conn); }
        );
        server_->SetMessageCallback(
            [this](const ConnectionPtr& conn, const Command& cmd) { this->OnMessage(conn, cmd); }
        );
        LOG_INFO << "KVService successfully hooked into TcpServer networking layer.";
    }

    void KVService::OnConnection(const ConnectionPtr& conn) {
        if (conn->IsConnected()) {
            LOG_INFO << "[KVService] Client connected: " << conn->GetPeerIp() << ":" << conn->GetPeerPort();
            //连接建立后是静默的，不需要弹Welcome消息。
        } else {
            LOG_INFO << "[KVService] Client disconnected: " << conn->GetPeerIp() << ":" << conn->GetPeerPort();
        }
    }

    void KVService::OnMessage(const ConnectionPtr& conn, const Command& cmd) {
        //查路由表
        auto it = handlers_.find(cmd.GetType());
        if (it != handlers_.end()) {
            auto handler = it->second;
            
            //网络IO线程直接把这个任务包装成Lambda，扔进阻塞队列
            //网络线程瞬间返回，继续去接收下一个客户端的请求
            if (cmd.GetType() == CommandType::GET || 
                cmd.GetType() == CommandType::PING || 
                cmd.GetType() == CommandType::QUIT) {
                //纯内存读取操作，拒绝线程切换，直接在当前网络线程光速执行
                handler(conn, cmd); 
            } else {
                //涉及LevelDB磁盘I/O的写操作
                std::string key = cmd.GetKey();
                // 算出这个Key命中哪一个线程
                size_t workerIndex = std::hash<std::string>{}(key) % workerPool_->GetThreadCount();
        
                workerPool_->SubmitTo(workerIndex, [handler, conn, cmd]() {
                    handler(conn, cmd); 
                });
            }
        } else {
            HandleUnknown(conn, cmd);
        }
    }

    /* ==========================================
        命令执行逻辑区
    ==========================================*/

    void KVService::HandleSet(const ConnectionPtr& conn, const Command& cmd) {
        if (!cmd.IsValid()) {
            conn->Send("-ERR wrong number of arguments for 'set' command\r\n");
            return;
        }
        
        //先存入磁盘 LevelDB
        const std::string& key = cmd.GetKey();
        const std::string& val = cmd.GetValue();

        //写内存：更新极速LRU缓存
        cache_->Put(key, val);

        //回复客户端：让它以为磁盘写得跟内存一样快
        conn->Send("+OK\r\n");

        //异步推入脏数据篮子
        {
            std::lock_guard<std::mutex> lock(flusherMutex_);
            dirtyQueue_.push_back({WriteOpType::PUT, key, val});
            
            // 篮子满了1000个立刻摇醒刷盘去落盘
            if (dirtyQueue_.size() >= 1000) {
                flusherCond_.notify_one();
            }
        }
    }

    void KVService::HandleGet(const ConnectionPtr& conn, const Command& cmd) {
        if (!cmd.IsValid()) {
            conn->Send("-ERR wrong number of arguments for 'get' command\r\n");
            return;
        }
        
        const std::string& key = cmd.GetKey();
        std::string val;

        //查内存缓存
        if (cache_->Get(key, val)) {
            conn->Send("+" + val + "\r\n");
            return; //缓存命中
        }

        //缓存没命中，去LevelDB
        std::string dbVal;
        if (store_->Get(key, dbVal)) {
            //磁盘里有则回写到缓存里
            cache_->Put(key, dbVal); 
            conn->Send("+" + dbVal + "\r\n");
        } else {
            //磁盘里也没有
            conn->Send("$-1\r\n"); //返回$-1
        }
    }

    void KVService::HandleDel(const ConnectionPtr& conn, const Command& cmd) {
        if (!cmd.IsValid()) {
            conn->Send("-ERR wrong number of arguments for 'del' command\r\n");
            return;
        }

        const std::string& key = cmd.GetKey();

        cache_->Delete(key); 

        //回复客户端
        conn->Send("+OK\r\n");

        //异步推入脏数据篮子，打上DEL标签
        {
            std::lock_guard<std::mutex> lock(flusherMutex_);
            //指明这是一个DEL操作，val空着就行
            dirtyQueue_.push_back({WriteOpType::DEL, key, ""});
            
            if (dirtyQueue_.size() >= 1000) {
                flusherCond_.notify_one();
            }
        }
    }

    void KVService::HandlePing(const ConnectionPtr& conn, const Command& cmd) {
        conn->Send("+PONG\r\n");
    }

    void KVService::HandleUnknown(const ConnectionPtr& conn, const Command& cmd) {
        conn->Send("-ERR unknown command\r\n");
    }

    void KVService::FlusherLoop() {
        //只要还在运行，或者篮子里还有遗留数据没写完，就一直干活
        while (flusherRunning_ || !dirtyQueue_.empty()) {
            std::vector<WriteOperation> localBatch; 
            
            {
                std::unique_lock<std::mutex> lock(flusherMutex_);
                flusherCond_.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                    return dirtyQueue_.size() >= 1000 || !flusherRunning_;
                });

                if (dirtyQueue_.empty()) continue;
                localBatch.swap(dirtyQueue_);
            } 

            if (!localBatch.empty()) {
                store_->WriteBatchOps(localBatch); 
            }
        }
    }

}