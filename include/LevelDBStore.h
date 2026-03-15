#ifndef YJ_KVS_LEVELDB_STORE_H
#define YJ_KVS_LEVELDB_STORE_H

#include "leveldb/db.h"
#include <string>
#include <vector>
#include <utility>

namespace yjKvs {

    enum class WriteOpType { 
        PUT, 
        DEL 
    };

    struct WriteOperation {
        WriteOpType type;
        std::string key;
        std::string value; //如果是DEL，这个字段就空着不用管
    };

    class LevelDBStore {
    public:
        //构造时打开数据库
        explicit LevelDBStore(const std::string& dbPath);
        ~LevelDBStore();

        //API设计：对外只暴露接口，隐藏底层的leveldb::Status
        bool Put(const std::string& key, const std::string& value);
        
        //返回true表示找到，false表示没找到或出错
        bool Get(const std::string& key, std::string& value); 
        
        bool Delete(const std::string& key);

        bool WriteBatchOps(const std::vector<WriteOperation>& ops);//批量写

        leveldb::DB* GetDB() const { return db_;}

    private:
        leveldb::DB* db_;
    };

}

#endif