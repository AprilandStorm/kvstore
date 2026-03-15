#ifndef YJ_KVS_TCP_SERVER_H
#define YJ_KVS_TCP_SERVER_H

#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "Command.h"
#include "event2/listener.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <queue>

namespace yjKvs {

    // 提前声明 Connection 类，避免头文件循环依赖
    class Connection; 

    class TcpServer {
    public:
        // 定义回调函数类型
        using ConnectionPtr = std::shared_ptr<Connection>;
        // 当有新连接到来、或连接断开时触发
        using ConnectionCallback = std::function<void(const ConnectionPtr&)>;
        // 当收到客户端发来的消息时触发
        using MessageCallback = std::function<void(const ConnectionPtr&, const Command&)>;

        //绑定到一个EventLoop上，并指定监听端口
        TcpServer(EventLoop* loop, int port);
        ~TcpServer();

        // 启动服务器监听
        void Start();

        // 设置多线程数量 (开启 Multi-Reactor)
        void SetThreadNum(int threadsNum);

        // 注册业务回调函数
        void SetConnectionCallback(const ConnectionCallback& cb) { 
            connectionCallback_ = cb; 
        }
        void SetMessageCallback(const MessageCallback& cb) { 
            messageCallback_ = cb; 
        }

    private:
        // libevent风格静态回调函数
        static void ListenerCallback(evconnlistener* listener, evutil_socket_t fd,
                                     sockaddr* address, int socklen, void* ctx);
        
        //内部处理逻辑
        void HandleNewConnection(evutil_socket_t fd, const std::string& ip, int port);

        void RemoveConnection(const ConnectionPtr& conn);
        
        //从池中获取
        std::shared_ptr<Connection> GetConnectionFromPool(EventLoop* ioLoop);

    private:
        EventLoop* loop_;           // 所属的事件循环（不拥有它的生命周期）
        int port_;                  // 监听端口
        evconnlistener* listener_;  // libevent 的监听器
        std::unique_ptr<EventLoopThreadPool> threadPool_;
        
        // 花名册,管理所有正在营业的连接
        // 只要连接还在这个map里，它的shared_ptr引用计数就至少是1，绝对不会被销毁！
        std::unordered_map<evutil_socket_t, ConnectionPtr> connections_;

        // 业务层注册进来的回调函数
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;

        std::mutex mapMutex_;

        //定长连接对象池
        std::queue<Connection*> connectionPool_;
        std::mutex poolMutex_;
        
    };

}
#endif