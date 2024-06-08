#include "../log/log.h"
#include "../sqlconnRAII.h"
#include "../sqlconnpool.h"
int main(void) {
    Log::instance()->init(0);
    LOG_INFO("启动");
    SqlConnPool* pool = SqlConnPool::instance();
    pool->init("localhost", 3306, "jie", "jie", "webserver");
    MYSQL* sql = pool->getConn();
    return 0;
}