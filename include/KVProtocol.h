#ifndef YJ_KVS_KV_PROTOCOL_H
#define YJ_KVS_KV_PROTOCOL_H

#include "Command.h"
#include <string>
#include <vector>

namespace yjKvs {

    // 协议解析器：只包含静态方法，因为它是一个无状态的工具类
    class KVProtocol {
    public:
        // 核心功能：把网络收到的raw message解析成Command对象
        static Command Parse(const std::string& message, size_t& consumedBytes);

    private:
        // 辅助工具：按空白字符（空格、Tab等）切割字符串
        static std::vector<std::string> Split(const std::string& str);
    };

} 

#endif 