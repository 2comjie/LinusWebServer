#ifndef __SQLCONNPOOL_H__
#define __SQLCONNPOOL_H__

#include <mysql/mysql.h>
#include <semaphore.h>

#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "../log/log.h"

class SqlConnPool {
   public:
    // 单例模式
    static SqlConnPool* instance();

    // 获取sql连接
    MYSQL* getConn();
    // 释放连接
    void freeConn(MYSQL* conn);
    // 获取空闲的连接数量
    int getFreeConnCount();

    // 初始化，主机、端口、用户、密码、数据库名字、连接数量
    void init(const char* host, int port,
              const char* user, const char* pwd,
              const char* dbName, int connSize = 10);

    // 关闭连接池
    void closePool();

   private:
    SqlConnPool();
    ~SqlConnPool();

    int m_MAX_CONN;
    int m_useCount;
    int m_freeCount;

    std::queue<MYSQL*> m_connQue;
    std::mutex m_mutex;
    sem_t m_semId;
};

#endif  // __SQLCONNPOOL_H__