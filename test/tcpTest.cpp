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

    evthread_use_pthreads();
    EventLoop mainLoop;

    // 【修改点】：使用智能指针包裹，交出生命周期的手动控制权
    auto server = std::make_unique<TcpServer>(&mainLoop, 9999);
    server->SetThreadNum(4); 
    
    auto kvService = std::make_unique<KVService>(server.get(), 10000, "./db_data");
    
    kvService->Start(); 
    server->Start();

    event* sigintEvent = evsignal_new(mainLoop.GetBase(), SIGINT, 
        [](evutil_socket_t fd, short events, void* arg) {
            LOG_INFO << "Caught SIGINT! Commencing Graceful Shutdown...";
            static_cast<EventLoop*>(arg)->Quit(); 
        }, &mainLoop);
    event_add(sigintEvent, nullptr);

    mainLoop.Loop(); 

    // =====================================
    // 优雅停机
    // =====================================
    
    //先关闭网络大门，停掉所有网络IO线程！
    //这样绝对不会再有任何新请求被投递给业务层。
    server.reset(); 
    LOG_INFO << "Network layer (TcpServer) has been fully shut down.";

    //安心关闭业务层（等待积压任务处理完，Flusher刷盘结束）
    kvService.reset();
    LOG_INFO << "Business layer (KVService) has been safely destroyed.";

    // =====================================
    event_free(sigintEvent); 
    LOG_INFO << "Engine stopped safely.";
    return 0;
}