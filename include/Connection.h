#ifndef YJ_KVS_CONNECTION_H
#define YJ_KVS_CONNECTION_H

#include "Command.h"
#include "event2/bufferevent.h"
#include <string>
#include <functional>
#include <memory>

namespace yjKvs {

    class EventLoop; // 前向声明，避免头文件交叉包含

    class Connection;
    
    // 统一定义回调函数签名（使用 shared_ptr 保证异步安全）
    using ConnectionPtr = std::shared_ptr<Connection>;
    using ConnectionCallback = std::function<void(const ConnectionPtr&)>;
    using MessageCallback = std::function<void(const ConnectionPtr&, const Command&)>;
    using CloseCallback = std::function<void(const ConnectionPtr&)>; // 供 TcpServer 内部使用

    // 继承 enable_shared_from_this，允许在成员函数内部获取指向自身的 shared_ptr
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        //接管底层的socket fd
        Connection(EventLoop* loop);
        ~Connection();

        //当从池子里拿出来时，注入实际的客户端信息
        void Bind(evutil_socket_t fd, const std::string& peerIp, int peerPort);
        //当连接断开，扔回池子前，清理遗留数据
        void Clear();

        // API
        void Send(const std::string& message);
        void Send(const char* data, size_t len);
        void Shutdown(); // 主动断开连接

        // 状态获取
        std::string GetPeerIp() const { 
            return peerIp_; 
        }
        int GetPeerPort() const { 
            return peerPort_; 
        }
        evutil_socket_t GetFd() const { 
            return fd_; 
        }
        bool IsConnected() const { 
            return state_ == kConnected; 
        }

        // 回调注册接口
        void SetConnectionCallback(const ConnectionCallback& cb) { 
            connectionCallback_ = cb; 
        }
        void SetMessageCallback(const MessageCallback& cb) { 
            messageCallback_ = cb; 
        }
        void SetCloseCallback(const CloseCallback& cb) { 
            closeCallback_ = cb; 
        }

        // 连接建立和销毁的触发器（由 TcpServer 调度）
        void ConnectionEstablished();
        void ConnectionDestroyed();

        void SetLoop(EventLoop* loop) { 
            loop_ = loop; 
        }

    private:
        // libevent 风格静态回调
        static void ReadCallback(bufferevent* bev, void* ctx);
        static void WriteCallback(bufferevent* bev, void* ctx);
        static void EventCallback(bufferevent* bev, short events, void* ctx);

        //内部面向对象处理逻辑
        void HandleRead();
        void HandleWrite();
        void HandleEvent(short events);

        // 连接状态机
        enum StateE { 
            kConnecting, 
            kConnected, 
            kDisconnecting, 
            kDisconnected };
        void SetState(StateE s) { 
            state_ = s; 
        }

    private:
        EventLoop* loop_;
        evutil_socket_t fd_;
        bufferevent* bev_;       //libevent的带缓冲事件对象
        StateE state_;           //当前连接状态

        std::string peerIp_;     //对端IP
        int peerPort_;           //对端端口
        std::string readBuffer_; //TCP粘包/半包的蓄水池

        //业务层注册进来的回调函数
        ConnectionCallback connectionCallback_;
        MessageCallback messageCallback_;
        CloseCallback closeCallback_; //通知TcpServer把它从管理Map中踢除
    };

}

#endif