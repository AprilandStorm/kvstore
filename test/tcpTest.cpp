#include "../include/EventLoop.h"
#include "../include/TcpServer.h"
#include "../include/KVService.h" 
#include "../utils/Logger.h"
#include "event2/thread.h"
#include "event2/event.h"
#include <iostream>
#include <csignal>

using namespace yjKvs;

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "   yjKvs High-Performance Engine Boot  " << std::endl;
    std::cout << "=======================================" << std::endl;

    //开启底层多线程防弹衣
    evthread_use_pthreads();

    //组装心脏 (Main Loop) 和 大堂经理 (Server)
    EventLoop mainLoop;
    TcpServer server(&mainLoop, 9999);
    
    // 配置多核引擎
    server.SetThreadNum(4); 

    //组装KVService，设置LRU缓存容量为10000个Key
    //此时它已经把网络层和LRU内存层彻底连接在了一起
    KVService kvService(&server, 10000, "./db_data");
    kvService.Start(); 

    //注册Ctrl+C优雅退出
    event* sigintEvent = evsignal_new(mainLoop.GetBase(), SIGINT, 
        [](evutil_socket_t fd, short events, void* arg) {
            LOG_INFO << "Caught SIGINT (Ctrl+C)! Shutting down gracefully...";
            static_cast<EventLoop*>(arg)->Quit(); 
        }, &mainLoop);
    event_add(sigintEvent, nullptr);

    //启动服务器
    server.Start();
    mainLoop.Loop(); 

    //安全退出
    event_free(sigintEvent); 
    LOG_INFO << "Engine stopped safely.";
    return 0;
}