#include "sqlconnpool.h"

SqlConnPool::SqlConnPool() {
    m_useCount = 0;
    m_freeCount = 0;
}

SqlConnPool::~SqlConnPool() {
    closePool();
}

SqlConnPool* SqlConnPool::instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::init(const char* host, int port,
                       const char* user, const char* pwd,
                       const char* dbName, int connSize) {
    assert(connSize > 0);
    for (int i = 0; i < connSize; ++i) {
        MYSQL* sql = nullptr;
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        sql = mysql_real_connect(sql, host, user, pwd, dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("Mysql Connect error!");
        }
        m_connQue.push(sql);
    }
    m_MAX_CONN = connSize;
    sem_init(&m_semId, 0, m_MAX_CONN);
}

MYSQL* SqlConnPool::getConn() {
    MYSQL* sql = nullptr;
    if (m_connQue.empty()) {
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    // 请求一个连接
    sem_wait(&m_semId);
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        sql = m_connQue.front();
        m_connQue.pop();
    }
    return sql;
}

void SqlConnPool::freeConn(MYSQL* sql) {
    assert(sql);
    std::lock_guard<std::mutex> locker(m_mutex);
    m_connQue.push(sql);
    // 释放一个连接
    sem_post(&m_semId);
}

void SqlConnPool::closePool() {
    std::lock_guard<std::mutex> locker(m_mutex);
    while (!m_connQue.empty()) {
        MYSQL* item = m_connQue.front();
        m_connQue.pop();
        mysql_close(item);
    }
    mysql_library_end();
}

int SqlConnPool::getFreeConnCount() {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_connQue.size();
}