#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

using namespace std;
using namespace std::chrono;

// 全局原子统计变量
atomic<int> total_requests(0);
atomic<long long> total_latency_us(0); // 记录总延迟(微秒)

// 每个线程扮演一个疯狂的客户端
void client_thread(const char* ip, int port, int requests_per_thread) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        return;
    }

    char buffer[256];
    string cmd = "SET bench_key benchmark_val\r\n";

    for (int i = 0; i < requests_per_thread; ++i) {
        auto start = high_resolution_clock::now();
        
        // 发送命令
        send(sock, cmd.c_str(), cmd.length(), 0);
        // 阻塞等待服务器回应 (模拟真实同步等待)
        recv(sock, buffer, sizeof(buffer), 0);
        
        auto end = high_resolution_clock::now();
        total_latency_us += duration_cast<microseconds>(end - start).count();
        total_requests++;
    }
    
    // 测试完断开
    string quit_cmd = "QUIT\r\n";
    send(sock, quit_cmd.c_str(), quit_cmd.length(), 0);
    close(sock);
}

int main(int argc, char* argv[]) {
    int thread_count = 200;       // 开启200个并发连接
    int req_per_thread = 1000;   // 每个连接连续发1000条请求 (总计20万次请求)
    
    cout << "=====================================\n";
    cout << "  yjKvs Benchmark Tool Started...\n";
    cout << "  Threads (Connections): " << thread_count << "\n";
    cout << "  Requests per thread  : " << req_per_thread << "\n";
    cout << "  Total Requests       : " << thread_count * req_per_thread << "\n";
    cout << "=====================================\n";

    auto global_start = high_resolution_clock::now();

    vector<thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(client_thread, "127.0.0.1", 9999, req_per_thread);
    }

    // 等待所有线程轰炸完毕
    for (auto& t : threads) {
        t.join();
    }

    auto global_end = high_resolution_clock::now();
    double total_time_sec = duration_cast<milliseconds>(global_end - global_start).count() / 1000.0;
    
    // 计算指标
    int final_reqs = total_requests.load();
    double qps = final_reqs / total_time_sec;
    double avg_latency_ms = (total_latency_us.load() / (double)final_reqs) / 1000.0;

    cout << "\n[TEST COMPLETE]\n";
    cout << "Time taken : " << total_time_sec << " seconds\n";
    cout << "Total Req  : " << final_reqs << "\n";
    cout << "QPS        : " << qps << " req/sec\n";
    cout << "Avg Latency: " << avg_latency_ms << " ms\n";

    return 0;
}