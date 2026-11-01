#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <sqlite3.h>

namespace connectx {

class Database {
public:
    explicit Database(const std::string& dbPath = "connectx.db");
    ~Database();

    // Prevent copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool open(const std::string& dbPath);
    void close();

    bool initSchema();
    bool migrateSchema();
    bool hasColumn(const std::string& tableName, const std::string& columnName);

    sqlite3* getHandle() const { return db_; }
    std::mutex& getMutex() { return mutex_; }

    bool execute(const std::string& sql);
    bool beginTransaction(bool immediate = true);
    bool commitTransaction();
    void rollbackTransaction();

private:
    sqlite3* db_ = nullptr;
    std::string dbPath_;
    std::mutex mutex_;
};

// RAII helper for sqlite3 transaction
class Transaction {
public:
    explicit Transaction(Database& db, bool immediate = true) : db_(db) {
        active_ = db_.beginTransaction(immediate);
    }

    ~Transaction() {
        if (active_) {
            db_.rollbackTransaction();
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool isStarted() const { return active_; }

    bool commit() {
        if (!active_) return false;
        bool ok = db_.commitTransaction();
        if (ok) {
            active_ = false;
        } else {
            db_.rollbackTransaction();
            active_ = false;
        }
        return ok;
    }

    void rollback() {
        if (active_) {
            db_.rollbackTransaction();
            active_ = false;
        }
    }

private:
    Database& db_;
    bool active_ = false;
};

// RAII helper for sqlite3_stmt
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool isValid() const { return stmt_ != nullptr; }
    sqlite3_stmt* get() const { return stmt_; }

    bool bindText(int index, const std::string& val);
    bool bindInt64(int index, int64_t val);
    bool bindNull(int index);

    int step();
    bool reset();

    std::string getColumnText(int col);
    int64_t getColumnInt64(int col);

private:
    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace connectx
