#ifndef __SQLCONNRAII_H__
#define __SQLCONNRAII_H__

#include "sqlconnpool.h"

// 资源在对象构造初始化 析构时释放
class SqlConnRII {
   public:
    SqlConnRII(MYSQL** sql, SqlConnPool* connPool) {
        assert(connPool);
        *sql = connPool->getConn();
        m_sql = *sql;
        m_connPool = connPool;
    }

    ~SqlConnRII() {
        if (m_sql) {
            m_connPool->freeConn(m_sql);
        }
    }

   private:
    MYSQL* m_sql;
    SqlConnPool* m_connPool;
};

#endif  // __SQLCONNRAII_H__