#ifndef YJ_KVS_COMMAND_H
#define YJ_KVS_COMMAND_H

#include <string>
#include <vector>

namespace yjKvs {

    //定义KV引擎支持的所有指令
    enum class CommandType {
        SET,        //写入数据:SET key value
        GET,        //读取数据:GET key
        DEL,        //删除数据:DEL key
        PING,       //PING测试
        INCOMPLETE,  //表示半包数据，还没收完
        QUIT,       //退出
        UNKNOWN     //无法解析的非法指令
    };

    //封装了客户端的意图
    class Command {
    public:
        //默认构造一个未知命令
        Command() : type_(CommandType::UNKNOWN) {}
        
        //传入类型和参数构造
        Command(CommandType type, std::vector<std::string> args)
            : type_(type), args_(std::move(args)) {}

        //获取命令类型
        CommandType GetType() const { return type_; }

        //获取所有参数
        const std::vector<std::string>& GetArgs() const { return args_; }

        // --- 提供极其便利的业务层接口 ---

        //安全地获取Key
        std::string GetKey() const {
            if (args_.size() > 0) return args_[0];
            return "";
        }

        //安全地获取Value
        std::string GetValue() const {
            if (args_.size() > 1) return args_[1];
            return "";
        }

        //判断这个命令是否合法
        bool IsValid() const {
            switch (type_) {
                case CommandType::SET:  return args_.size() == 2; // SET必须有key和value
                case CommandType::GET:  return args_.size() == 1; // GET必须有key
                case CommandType::DEL:  return args_.size() == 1; // DEL必须有key
                case CommandType::PING: return true;              // PING可以没参数
                case CommandType::QUIT: return true;
                default:                return false;
            }
        }

    private:
        CommandType type_;                  //命令的类型
        std::vector<std::string> args_;     //命令的参数列表
    };
}

#endif