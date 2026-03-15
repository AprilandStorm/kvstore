#include "../include/LevelDBStore.h"
#include "../utils/Logger.h"
#include "leveldb/write_batch.h"

namespace yjKvs {

    LevelDBStore::LevelDBStore(const std::string& dbPath) : db_(nullptr) {
        leveldb::Options options;
        options.create_if_missing = true;
        
        leveldb::Status status = leveldb::DB::Open(options, dbPath, &db_);
        if (!status.ok()) {
            LOG_ERROR << "LevelDBStore: Failed to open DB at " << dbPath << " - " << status.ToString();
            exit(1); 
        }
        LOG_INFO << "LevelDBStore: Successfully opened DB at " << dbPath;
    }

    LevelDBStore::~LevelDBStore() {
        if (db_) {
            delete db_;
        }
    }

    bool LevelDBStore::Put(const std::string& key, const std::string& value) {
        leveldb::WriteOptions writeOpts;
        leveldb::Status status = db_->Put(writeOpts, key, value);
        if (!status.ok()) {
            LOG_ERROR << "LevelDBStore Put Error: " << status.ToString();
            return false;
        }
        return true;
    }

    bool LevelDBStore::Get(const std::string& key, std::string& value) {
        leveldb::ReadOptions readOpts;
        leveldb::Status status = db_->Get(readOpts, key, &value);
        if (status.ok()) {
            return true;
        } else if (!status.IsNotFound()) {
            //如果不是因为没找到，而是发生了磁盘损坏等严重错误，打印日志
            LOG_ERROR << "LevelDBStore Get Error: " << status.ToString();
        }
        return false;
    }

    bool LevelDBStore::Delete(const std::string& key) {
        leveldb::WriteOptions writeOpts;
        leveldb::Status status = db_->Delete(writeOpts, key);
        if (status.ok() || status.IsNotFound()) {
            return true;
        }
        LOG_ERROR << "LevelDBStore Delete Error: " << status.ToString();
        return false;
    }

    bool LevelDBStore::WriteBatchOps(const std::vector<WriteOperation>& ops) {
        if (ops.empty()) return true;

        leveldb::WriteBatch batch;
        for (const auto& op : ops) {
            if (op.type == WriteOpType::PUT) {
                batch.Put(op.key, op.value);
            } else if (op.type == WriteOpType::DEL) {
                batch.Delete(op.key); //如果是删除任务，直接调用Delete
            }
        }
        
        leveldb::WriteOptions writeOpts;
        leveldb::Status status = db_->Write(writeOpts, &batch);
        
        if (!status.ok()) {
            LOG_ERROR << "LevelDBStore WriteBatchOps Error: " << status.ToString();
            return false;
        }
        return true;
    }

}