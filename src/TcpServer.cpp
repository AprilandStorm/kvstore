#include "../include/TcpServer.h"
#include "../include/Connection.h"
#include "../utils/Logger.h"
#include <arpa/inet.h>
#include <cstring>

namespace yjKvs {

    TcpServer::TcpServer(EventLoop* loop, int port)
        : loop_(loop), port_(port), listener_(nullptr) {
            // 【对象池初始化】：预先捏好 5000 个空壳！
            for (int i = 0; i < 5000; ++i) {
                connectionPool_.push(new Connection(nullptr)); 
            }
    }

    TcpServer::~TcpServer() {
        while (!connectionPool_.empty()) {
            delete connectionPool_.front();
            connectionPool_.pop();
        }
        LOG_INFO << "TcpServer shut down.";
    }

    void TcpServer::Start() {
        if(threadPool_){
            threadPool_->Start();
        }

        // 配置 IPv4 的地址和端口
        sockaddr_in sin{};
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port_);
        sin.sin_addr.s_addr = htonl(INADDR_ANY); // 监听本机所有网卡

        // 绑定并立刻开始监听
        listener_ = evconnlistener_new_bind(loop_->GetBase(), ListenerCallback, this,
                                            LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
                                            -1, (sockaddr*)&sin, sizeof(sin));

        if (!listener_) {
            LOG_ERROR << "TcpServer failed to start listening on port " << port_;
            return;
        }

        LOG_INFO << "TcpServer is listening on port " << port_ << " !";
    }

    void TcpServer::SetThreadNum(int threadsNum) {
        // 实例化底层的事件循环线程池，传入 Main Reactor (loop_) 作为兜底，
        // 并设定要开启的 Sub Reactor 后台线程数量。
        threadPool_ = std::make_unique<EventLoopThreadPool>(loop_, threadsNum);
    }

    //静态回调
    void TcpServer::ListenerCallback(evconnlistener* listener, evutil_socket_t fd,
                                     sockaddr* address, int socklen, void* ctx) {
        
        //把 void* ctx 强转回 TcpServer 实例指针
        TcpServer* server = static_cast<TcpServer*>(ctx);

        //解析客户端的 IP 和 端口
        sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(address);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(sin->sin_addr), ip, INET_ADDRSTRLEN);
        int port = ntohs(sin->sin_port);

        //抛给真正的面向对象处理逻辑
        server->HandleNewConnection(fd, ip, port);
    }

    //创建连接、注册回调、存入花名册
    void TcpServer::HandleNewConnection(evutil_socket_t fd, const std::string& ip, int port) {
        LOG_INFO << "New connection established: " << ip << ":" << port << " (fd: " << fd << ")";

        //从池子里挑一个Sub Reactor
        EventLoop* ioLoop = threadPool_ ? threadPool_->GetNextLoop() : loop_;

        ConnectionPtr conn = GetConnectionFromPool(ioLoop);
        conn->Bind(fd, ip, port);

        //把业务层交给TcpServer的回调函数，继续向下传给Connection
        conn->SetConnectionCallback(connectionCallback_);
        conn->SetMessageCallback(messageCallback_);

        //当Connection断开时，它需要通知TcpServer把自己从花名册里踢掉
        //使用lambda表达式捕获this指针
        conn->SetCloseCallback([this](const ConnectionPtr& c) {
            this->RemoveConnection(c);
        });

        //把智能指针强行塞进map里
        {
            std::lock_guard<std::mutex> lock(mapMutex_);
            connections_[fd] = conn;
        }

        //通知Connection开始监听底层的读写事件，并触发上层的ConnectionCallback
        conn->ConnectionEstablished();
    }

    // 连接销毁
    void TcpServer::RemoveConnection(const ConnectionPtr& conn) {
        evutil_socket_t fd = conn->GetFd();
        LOG_INFO << "TcpServer removing connection (fd: " << fd << ") from map.";
        
        // 从 map 中移除！
        // 此时，如果没有任何业务层（比如线程池）还在持有这个conn的shared_ptr，
        // 它的引用计数就会瞬间归零，Connection的析构函数会被安全触发，内存完美回收！
        {
            std::lock_guard<std::mutex> lock(mapMutex_);
            size_t n = connections_.erase(fd);
            if (n != 1) {
                LOG_ERROR << "TcpServer tried to remove a non-existent connection!";
            }
        }
    }

    std::shared_ptr<Connection> TcpServer::GetConnectionFromPool(EventLoop* ioLoop) {
        Connection* rawConn = nullptr;
        {
            std::lock_guard<std::mutex> lock(poolMutex_);
            if (!connectionPool_.empty()) {
                rawConn = connectionPool_.front();
                connectionPool_.pop();
            }
        }

        // 如果池子枯竭了（极端并发超过 5000），临时 new 一个保底
        if (!rawConn) {
            rawConn = new Connection(nullptr);
        }

        // 修改它的所属 Loop
        rawConn->SetLoop(ioLoop);

        // 【核心神技：自定义删除器 Custom Deleter】
        // 当这个 shared_ptr 引用计数归零（连接彻底断开）时，不要 delete！
        // 而是把它洗干净（Clear），然后塞回池子里复用！
        return std::shared_ptr<Connection>(rawConn, [this](Connection* ptr) {
            ptr->Clear(); 
            std::lock_guard<std::mutex> lock(this->poolMutex_);
            this->connectionPool_.push(ptr);
        });
    }

}