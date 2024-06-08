#include "log.h"

Log::Log() {
    m_MAX_LINES = MAX_LINES;
    m_lineCount = 0;
    m_toDay = 0;
    m_isOpen = false;
    m_isAsync = false;
    m_fp = nullptr;
    m_deque = nullptr;
    m_writeThread = nullptr;
}

Log::~Log() {
    if (m_writeThread && m_writeThread->joinable()) {
        while (!m_deque->empty()) {
            m_deque->flush();
        }
        m_deque->close();
        m_writeThread->join();
    }
    if (m_fp) {
        std::lock_guard<std::mutex> locker(m_mutex);
        flush();
        fclose(m_fp);
    }
}

void Log::init(int t_level, const char* t_path,
               const char* t_suffix,
               int t_maxQueueCapacity) {
    m_isOpen = true;
    m_level = t_level;

    if (t_maxQueueCapacity > 0) {
        m_isAsync = true;
        if (!m_deque) {
            // 初始化阻塞队列
            std::unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            m_deque = std::move(newDeque);

            // 初始化异步log的线程
            std::unique_ptr<std::thread> newThead(new std::thread(flushLogThread));
            m_writeThread = std::move(newThead);
        }
    } else {
        m_isAsync = false;
    }

    m_lineCount = 0;
    time_t timer = time(nullptr);
    // 通过tm结构体转换成年月日时分秒的格式
    struct tm* sysTime = localtime(&timer);
    struct tm t = *sysTime;

    m_path = t_path;
    m_suffix = t_suffix;
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s",
             m_path, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, m_suffix);
    m_toDay = t.tm_mday;

    {
        std::lock_guard<std::mutex> locker(m_mutex);
        m_buff.retrieveAll();
        if (m_fp) {
            flush();
            fclose(m_fp);
        }

        m_fp = fopen(fileName, "a");
        if (m_fp == nullptr) {
            mkdir(m_path, 0777);
            m_fp = fopen(fileName, "a");
        }
        assert(m_fp != nullptr);
    }
}

int Log::getLevel() {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_level;
}

void Log::setLevel(int t_level) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_level = t_level;
}

void Log::write(int t_level, const char* t_format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm* sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    if (m_toDay != t.tm_mday || (m_lineCount && (m_lineCount % MAX_LINES == 0))) {
        std::unique_lock<std::mutex> locker(m_mutex);
        locker.unlock();

        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (m_toDay != t.tm_mday) {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", m_path, tail, m_suffix);
            m_toDay = t.tm_mday;
            m_lineCount = 0;
        } else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", m_path, tail, (m_lineCount / MAX_LINES), m_suffix);
        }

        locker.lock();
        flush();
        fclose(m_fp);
        m_fp = fopen(newFile, "a");
        assert(m_fp != nullptr);
    }

    {
        std::unique_lock<std::mutex> locker(m_mutex);
        m_lineCount++;
        int n = snprintf(m_buff.beginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                         t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                         t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        m_buff.hasWritten(n);
        appendLogLevelTitle(t_level);
        va_start(vaList, t_format);
        int m = vsnprintf(m_buff.beginWrite(), m_buff.writAbleBytes(), t_format, vaList);
        va_end(vaList);
        m_buff.hasWritten(m);
        m_buff.append("\n\0", 2);

        if (m_isAsync && m_deque && !m_deque->full()) {
            m_deque->pushBack(m_buff.retrieveAllToStr());
        } else {
            fputs(m_buff.peek(), m_fp);
        }
        m_buff.retrieveAll();
    }
}

void Log::appendLogLevelTitle(int t_level) {
    switch (t_level) {
        case 0:
            m_buff.append("[debug]: ", 9);
            break;
        case 1:
            m_buff.append("[info] : ", 9);
            break;
        case 2:
            m_buff.append("[warn] : ", 9);
            break;
        case 3:
            m_buff.append("[error]: ", 9);
            break;
        default:
            m_buff.append("[info] : ", 9);
            break;
    }
}

void Log::flush() {
    if (m_isAsync) {
        m_deque->flush();
    }
    fflush(m_fp);
}

void Log::asyncWrite() {
    std::string str = "";
    while (m_deque->pop(str)) {
        std::lock_guard<std::mutex> locker(m_mutex);
        fputs(str.c_str(), m_fp);
    }
}

Log* Log::instance() {
    static Log inst;
    return &inst;
}

void Log::flushLogThread() {
    Log::instance()->asyncWrite();
}