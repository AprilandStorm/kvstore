#ifndef YJ_KVS_LOGGER_H
#define YJ_KVS_LOGGER_H

#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include <chrono>
#include <iomanip>
#include <cstring>

namespace yjKvs{
    
    //定义日志级别
    enum class LogLevel{
        DEBUG,
        INFO,
        WARN,
        ERROR
    };

    //核心日志类
    class Logger{
    public:
        Logger(LogLevel level, const char* file, int line){
            FormatPrefix(level, file, line);
        }

        ~Logger(){
            stream_ << "\n";//自动换行

            //获取全局单例互斥锁，保证输出到终端的过程绝对不会被其他线程打断
            std::lock_guard<std::mutex> lock(GetMutex());
            std::cout << stream_.str();

            //如果是ERROR级别，顺便刷新一下底层的缓冲区,直接输出
            if(level_ == LogLevel::ERROR){
                std::cout << std::flush;
            }
        }

        //运算符重载，支持像std::cout 一样的流式输出
        template <typename T>
        Logger& operator << (const T& value){
            stream_ << value;
            return * this; //返回自身引用，支持链是调用
        }
    
    private:
        static std::mutex& GetMutex(){
            static std::mutex mtx;
            return mtx;
        }

        //格式化日志前缀
        void FormatPrefix(LogLevel level, const char* file, int line){
           level_ = level;

           //提取文件名
           const char* slash = strrchr(file, '/');
           if(!slash){
            slash = strrchr(file, '\\');//兼容windows路径
           }

           const char* fileName = slash ? (slash + 1) : file;

           auto now = std::chrono::system_clock::now();
           auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);

           auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
           
           std::tm tmInfo;
           localtime_r(&nowAsTimeT, &tmInfo);

           stream_ << "[" << LevelToString(level) << "] "
                   << "[" << std::put_time(&tmInfo, "%Y-%m-%d %H:%M:%S")// 格式化年月日时分秒
                   << "." << std::setfill('0') << std::setw(3) << nowMs.count() << "] "//格式化毫秒
                   << "[" << std::this_thread::get_id() << "] "
                   << "[" << fileName << ":" << line << "] ";
        }

        const char* LevelToString(LogLevel level){
            switch(level){
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO: return "INFO";
                case LogLevel::WARN: return "WARN";
                case LogLevel::ERROR: return "ERROR";
                default: return "UNKNOWN";
            }
        }

        LogLevel level_;
        std::ostringstream stream_;//线程局部的字符串串流，用于暂存业务输出
    };
}

#ifdef NDEBUG
    // 生产环境 (Release模式): 屏蔽DEBUG和INFO，只保留WARN和ERROR救命用
    #define LOG_DEBUG if(0) yjKvs::Logger(yjKvs::LogLevel::DEBUG, __FILE__, __LINE__)
    #define LOG_INFO  if(0) yjKvs::Logger(yjKvs::LogLevel::INFO, __FILE__, __LINE__)
    
    #define LOG_WARN  yjKvs::Logger(yjKvs::LogLevel::WARN, __FILE__, __LINE__)
    #define LOG_ERROR yjKvs::Logger(yjKvs::LogLevel::ERROR, __FILE__, __LINE__)
#else
    // 开发环境 (Debug模式): 全量输出
    #define LOG_DEBUG yjKvs::Logger(yjKvs::LogLevel::DEBUG, __FILE__, __LINE__)
    #define LOG_INFO  yjKvs::Logger(yjKvs::LogLevel::INFO, __FILE__, __LINE__)
    #define LOG_WARN  yjKvs::Logger(yjKvs::LogLevel::WARN, __FILE__, __LINE__)
    #define LOG_ERROR yjKvs::Logger(yjKvs::LogLevel::ERROR, __FILE__, __LINE__)
#endif

#endif

/**诞生（构造）：LOG_INFO 宏展开为 yjKvs::Logger(INFO, __FILE__, __LINE__)。
 * 此时，在当前执行线程的栈内存中，瞬间创建了一个没有名字的临时 Logger 对象。
 * 构造函数被触发，把 [INFO] [线程号] [文件名:行号] 写进了这个对象私有的 stream_ 内存流里。
 * 此时完全没有加锁！
 * 
 * 组装（流式输出）： 紧接着，<< "Task " << i << " done" 被执行。
 * 这些业务数据也被全部塞进了这个临时对象的 stream_ 里。此时依然没有加锁！ 
 * 意味着 100 个线程可以同时、毫无阻塞地在各自的内存里拼装字符串。
 * 
 * 毁灭与发射（析构）：当这行代码执行到末尾的分号 ; 时，
 * 这个临时 Logger 对象的生命周期结束了。编译器自动调用它的析构函数 ~Logger()。
 * 在析构函数里：申请全局唯一的 mutex 锁。把刚才在内存里拼装好的一大串完整字符串，一口气吐给屏幕。
 * 析构函数结束，mutex 锁自动释放。
 * 
 * 利用临时对象生命周期 + 栈内存隔离 + 析构函数加锁
*/