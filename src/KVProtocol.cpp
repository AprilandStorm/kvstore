#include "../include/KVProtocol.h"
#include <sstream>
#include <algorithm>

namespace yjKvs {

    Command KVProtocol::Parse(const std::string& message, size_t& consumedBytes) {
        //寻找这一行命令的终点 (Redis 协议通常以 \r\n 结尾)
        size_t pos = message.find('\n');
        
        //如果在蓄水池里根本没找到\n，说明命令还不完整
        if (pos == std::string::npos) {
            consumedBytes = 0; //一字节都没消耗
            return Command(CommandType::INCOMPLETE, {}); 
        }

        //计算本次吃掉了多少字节（包括 \n 本身）
        consumedBytes = pos + 1;

        //把这行完整的命令提取出来，并去掉末尾可能的 \r
        size_t lineLen = pos;
        if (lineLen > 0 && message[lineLen - 1] == '\r') {
            lineLen--;
        }
        std::string line = message.substr(0, lineLen);

        //切割字符串（比如把 "SET key value" 切成3个token）
        std::vector<std::string> tokens;
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            tokens.push_back(word);
        }

        //如果发来个空行，算作未知命令
        if (tokens.empty()) {
            return Command(CommandType::UNKNOWN, tokens);
        }

        //提取主命令并转大写，方便路由匹配
        std::string cmdName = tokens[0];
        for (char& c : cmdName) {
            c = toupper(c);
        }

        //生成包装好的Command 对象返回给KVService
        if (cmdName == "SET") {
            return Command(CommandType::SET, tokens);
        } else if (cmdName == "GET") {
            return Command(CommandType::GET, tokens);
        } else if (cmdName == "DEL") {
            return Command(CommandType::DEL, tokens);
        } else if (cmdName == "PING") {
            return Command(CommandType::PING, tokens);
        } else if (cmdName == "QUIT" || cmdName == "EXIT") {
            return Command(CommandType::QUIT, tokens);
        }

        return Command(CommandType::UNKNOWN, tokens);
    }

    std::vector<std::string> KVProtocol::Split(const std::string& str) {
        std::vector<std::string> result;
        // 使用C++标准库的stringstream
        std::istringstream iss(str);
        std::string token;
        while (iss >> token) {
            result.push_back(token);
        }
        return result;
    }

}