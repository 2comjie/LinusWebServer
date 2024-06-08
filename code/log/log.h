#ifndef __LOG_H__
#define __LOG_H__

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <mutex>
#include <string>
#include <thread>

#include "../buffer/buffer.h"
#include "blockqueue.h"

class Log {
   public:
    void init(int t_level, const char* t_path = "./log",
              const char* t_suffix = ".log",
              int t_maxQueueCapacity = 1024);

    static Log* instance();
    // writeThread运行的函数
    static void flushLogThread();

    void write(int t_level, const char* t_format, ...);
    void flush();

    bool isOpen() { return m_isOpen; }

    void setLevel(int t_level);
    int getLevel();

   private:
    Log();
    ~Log();
    void appendLogLevelTitle(int t_level);
    void asyncWrite();

   private:
    static const int LOG_PATH_LEN = 256;
    static const int LOG_NAME_LEN = 256;
    static const int MAX_LINES = 50000;

   private:
    const char* m_path;
    const char* m_suffix;

    int m_level;
    int m_MAX_LINES;
    int m_lineCount;
    int m_toDay;

    bool m_isOpen;

    Buffer m_buff;
    bool m_isAsync;

    FILE* m_fp;
    std::unique_ptr<BlockDeque<std::string>> m_deque;
    std::unique_ptr<std::thread> m_writeThread;
    std::mutex m_mutex;
};

#define LOG_BASE(level, format, ...)                     \
    do {                                                 \
        Log* log = Log::instance();                      \
        if (log->isOpen() && log->getLevel() <= level) { \
            log->write(level, format, ##__VA_ARGS__);    \
            log->flush();                                \
        }                                                \
    } while (0);

#define LOG_DEBUG(format, ...)             \
    do {                                   \
        LOG_BASE(0, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_INFO(format, ...)              \
    do {                                   \
        LOG_BASE(1, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_WARN(format, ...)              \
    do {                                   \
        LOG_BASE(2, format, ##__VA_ARGS__) \
    } while (0);
#define LOG_ERROR(format, ...)             \
    do {                                   \
        LOG_BASE(3, format, ##__VA_ARGS__) \
    } while (0);

#endif  // __LOG_H__