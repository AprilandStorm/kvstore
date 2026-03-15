#include "../include/Connection.h"
#include "../include/EventLoop.h"
#include "../utils/Logger.h"
#include "../include/Command.h"
#include "../include/KVProtocol.h"
#include "event2/buffer.h"
#include <iostream>

namespace yjKvs {

    Connection::Connection(EventLoop* loop)
        : loop_(loop), fd_(-1), state_(kDisconnected), bev_(nullptr) {}

    // 初始化
    void Connection::Bind(evutil_socket_t fd, const std::string& peerIp, int peerPort) {
        fd_ = fd;
        peerIp_ = peerIp;
        peerPort_ = peerPort;
        state_ = kConnecting;
        readBuffer_.clear(); //清空上次残留数据

        //重新申请底层bufferevent
        bev_ = bufferevent_socket_new(loop_->GetBase(), fd_, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
        bufferevent_setcb(bev_, ReadCallback, WriteCallback, EventCallback, this);
    }

    //回收前清理
    void Connection::Clear() {
        if (bev_) {
            bufferevent_free(bev_); //释放底层资源并自动close(fd)
            bev_ = nullptr;
        }
        fd_ = -1;
        state_ = kDisconnected;
    }

    Connection::~Connection() {
        LOG_INFO << "Connection destroyed. Reclaiming resources for " << peerIp_ << ":" << peerPort_;
        Clear();
    }

    // 生命周期管理 (由TcpServer调度)
    void Connection::ConnectionEstablished() {
        SetState(kConnected);
        
        // 开启读写事件监听。在此之前，即使客户端发了数据，底层也不会触发读回调
        bufferevent_enable(bev_, EV_READ | EV_WRITE);
        
        // 触发业务层注册的连接回调
        if (connectionCallback_) {
            //传递shared_from_this()，确保回调执行期间当前对象绝对存活
            connectionCallback_(shared_from_this()); 
        }
    }

    void Connection::ConnectionDestroyed() {
        if (state_ == kConnected) {
            SetState(kDisconnected);
            bufferevent_disable(bev_, EV_READ | EV_WRITE);
            
            // 触发业务层的回调，通知他们客户端断开了
            if (connectionCallback_) {
                connectionCallback_(shared_from_this());
            }
        }
    }

    //静态回调
    void Connection::ReadCallback(bufferevent* bev, void* ctx) {
        Connection* conn = static_cast<Connection*>(ctx);
        conn->HandleRead();
    }

    void Connection::WriteCallback(bufferevent* bev, void* ctx) {
        Connection* conn = static_cast<Connection*>(ctx);
        conn->HandleWrite();
    }

    void Connection::HandleWrite() {
        //如果当前业务层已经下达了“断开指令”
        if (state_ == kDisconnecting) {
            struct evbuffer* output = bufferevent_get_output(bev_);
            //并且底层缓冲区的数据已经全部真正发到了网线上
            if (evbuffer_get_length(output) == 0) {
                LOG_INFO << "All remaining data sent. Safely closing connection (" 
                         << peerIp_ << ":" << peerPort_ << ").";
                //通知TcpServer销毁本对象
                if (closeCallback_) {
                    closeCallback_(shared_from_this());
                }
            }
        }
    }

    void Connection::EventCallback(bufferevent* bev, short events, void* ctx) {
        Connection* conn = static_cast<Connection*>(ctx);
        conn->HandleEvent(events);
    }

    // 真正的面向对象处理逻辑
    void Connection::HandleRead() {
        evbuffer* input = bufferevent_get_input(bev_);
        size_t len = evbuffer_get_length(input);
        if (len == 0) return;

        //把新收到的数据追加到蓄水池里
        std::string newData(len, 0);
        evbuffer_remove(input, &newData[0], len);
        readBuffer_ += newData;

        //只要蓄水池里还有数据，就一直尝试解析
        while (!readBuffer_.empty()) {
            // 如果数据不够一条完整的命令，它应该返回CommandType::INCOMPLETE
            // 如果解析成功，还需要通知它“吃掉”了多少字节，以便从buffer里截断它。
            size_t consumedBytes = 0; 
            Command cmd = KVProtocol::Parse(readBuffer_, consumedBytes);

            if (cmd.GetType() == CommandType::INCOMPLETE || consumedBytes == 0) {
                //数据不够（半包），跳出循环，等下次网络事件来了凑齐再说
                break; 
            }

            // 成功解析出一条命令，把这段数据从蓄水池里删掉
            readBuffer_.erase(0, consumedBytes);

            // 抛给上层业务处理
            if (messageCallback_) {
                messageCallback_(shared_from_this(), cmd);
            }
        }
    }

    void Connection::HandleEvent(short events) {
        if (events & BEV_EVENT_EOF) {
            LOG_INFO << "Connection (" << peerIp_ << ":" << peerPort_ << ") disconnected normally.";
        } else if (events & BEV_EVENT_ERROR) {
            LOG_ERROR << "Connection (" << peerIp_ << ":" << peerPort_ << ") network error!";
        }

        //只要遇到断开或异常，立刻启动销毁流程
        if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            SetState(kDisconnecting);
            
            //通知业务层连接已失效
            if (connectionCallback_) {
                connectionCallback_(shared_from_this());
            }

            //通知TcpServer,从map除名
            //这一步执行完，如果没有别的线程就会立刻触发析构函数
            if (closeCallback_) {
                closeCallback_(shared_from_this());
            }
        }
    }

    //主动发送数据的API
    void Connection::Send(const std::string& message) {
        Send(message.data(), message.size());
    }

    void Connection::Send(const char* data, size_t len) {
        if (state_ == kConnected) {
            //哪怕底层网卡满了，也会先把data存进底层的output evbuffer里，
            //然后等网卡空闲了，自动在后台发出去，绝对不会阻塞当前线程
            bufferevent_write(bev_, data, len);
        } else {
            LOG_WARN << "Attempted to send data to a disconnected client (" << peerIp_ << ":" << peerPort_ << ")";
        }
    }

    void Connection::Shutdown() {
        if (state_ == kConnected) {
            state_ = kDisconnecting;
        
            // 不再接收新消息
            bufferevent_disable(bev_, EV_READ);
        
            //检查输出缓冲区里，还有没有没发完的残留数据
            struct evbuffer* output = bufferevent_get_output(bev_);
            if (evbuffer_get_length(output) == 0) {
                // 如果缓冲区是空的,通知server删除该线程
                if (closeCallback_) {
                    closeCallback_(shared_from_this());
                }
            }
        }
    }

} 