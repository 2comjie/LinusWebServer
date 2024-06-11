#include "webServer.h"

WebServer::WebServer(
    int port, int trigMode, int timeoutMS, bool OptLinger,
    int sqlPort, const char* sqlUser, const char* sqlPwd,
    const char* dbName, int connPoolNum, int threadNum,
    bool openLog, int logLevel, int logQueSize) : m_port(port),
                                                  m_timeoutMS(timeoutMS),
                                                  m_openLinger(OptLinger),
                                                  m_isClose(false),
                                                  m_timer(new HeapTimer()),
                                                  m_threadPool(new ThreadPool(threadNum)),
                                                  m_epoller(new Epoller()) {
    m_srcDir = getcwd(nullptr, 256);
    assert(m_srcDir);
    strncat(m_srcDir, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = m_srcDir;
    SqlConnPool::instance()->init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    initEventMode(trigMode);
    if (!initSocket()) {
        m_isClose = true;
    }

    if (openLog) {
        Log::instance()->init(logLevel, "./.log", ".log", logQueSize);
        if (m_isClose) {
            LOG_ERROR("========== Server init error!==========");
        } else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", m_port, OptLinger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (m_listenEvent & EPOLLET ? "ET" : "LT"),
                     (m_connEvent & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(m_listenFd);
    m_isClose = true;
    free(m_srcDir);
    SqlConnPool::instance()->closePool();
}

void WebServer::initEventMode(int trigMode) {
    m_listenEvent = EPOLLRDHUP;
    m_connEvent = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigMode) {
        case 0:
            break;
        case 1:
            m_connEvent |= EPOLLET;
            break;
        case 2:
            m_listenEvent |= EPOLLET;
            break;
        case 3:
            m_listenEvent |= EPOLLET;
            m_connEvent |= EPOLLET;
            break;
        default:
            m_listenEvent |= EPOLLET;
            m_connEvent |= EPOLLET;
            break;
    }
    HttpConn::isET = (m_connEvent & EPOLLET);
}

void WebServer::start() {
    int timeMS = -1;
    if (!m_isClose) {
        LOG_INFO("========== Server start ==========");
    }

    while (!m_isClose) {
        if (m_timeoutMS > 0) {
            timeMS = m_timer->getNextTick();
        }
        int eventCnt = m_epoller->wait(timeMS);
        for (int i = 0; i < eventCnt; ++i) {
            int fd = m_epoller->getEventFd(i);          // int fd = events[i].data.fd
            uint32_t events = m_epoller->getEvents(i);  // uint32_t events = events[i].events
            if (fd == m_listenFd) {
                dealListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  // 断开连接
                assert(m_users.count(fd) > 0);
                closeConn(&m_users[fd]);
            } else if (events & EPOLLIN) {
                assert(m_users.count(fd) > 0);
                dealRead(&m_users[fd]);
            } else if (events & EPOLLOUT) {
                assert(m_users.count(fd) > 0);
                dealWrite(&m_users[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void WebServer::sendError(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

void WebServer::closeConn(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->getFd());
    m_epoller->delFd(client->getFd());
    client->close();
}

void WebServer::addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    m_users[fd].init(fd, addr);
    if (m_timeoutMS > 0) {
        m_timer->add(fd, m_timeoutMS, std::bind(&WebServer::closeConn, this, &m_users[fd]));
    }
    m_epoller->addFd(fd, EPOLLIN | m_connEvent);
    setFdNonblock(fd);
    LOG_INFO("Client[%d] in!", m_users[fd].getFd());
}

void WebServer::dealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(m_listenFd, (struct sockaddr*)&addr, &len);
        if (fd <= 0) {
            return;
        } else if (HttpConn::userCount >= MAX_FD) {
            sendError(fd, "Server busy");
            LOG_WARN("Clients is full!");
            return;
        }
        addClient(fd, addr);
    } while (m_listenEvent & EPOLLET);
}

void WebServer::dealRead(HttpConn* client) {
    assert(client);
    extentTime(client);
    m_threadPool->addTask(std::bind(&WebServer::onRead, this, client));
}

void WebServer::dealWrite(HttpConn* client) {
    assert(client);
    extentTime(client);
    m_threadPool->addTask(std::bind(&WebServer::onWrite, this, client));
}

void WebServer::extentTime(HttpConn* client) {
    assert(client);
    if (m_timeoutMS > 0) {
        m_timer->adjust(client->getFd(), m_timeoutMS);
    }
}

void WebServer::onRead(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConn(client);
        return;
    }
    onProcess(client);
}

void WebServer::onProcess(HttpConn* client) {
    if (client->process()) {
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
    } else {
        m_epoller->modFd(client->getFd(), m_connEvent | EPOLLIN);
    }
}

void WebServer::onWrite(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);
    if (client->toWriteBytes() == 0) {
        if (client->isKeepAlive()) {
            onProcess(client);
            return;
        }
    } else if (ret < 0) {
        if (writeErrno == EAGAIN) {
            /* 继续传输 */
            m_epoller->modFd(client->getFd(), m_connEvent | EPOLLOUT);
            return;
        }
    }
    closeConn(client);
}

bool WebServer::initSocket() {
    int ret;
    struct sockaddr_in addr;
    if (m_port > 65535 || m_port < 1024) {
        LOG_ERROR("Port:%d error!", m_port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);
    struct linger optLinger = {0};
    if (m_openLinger) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        LOG_ERROR("Port:%d Create socket error!", m_port);
        return false;
    }

    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(m_listenFd);
        LOG_ERROR("Port:%d Init linger error!", m_port);
        return false;
    }

    int optval = 1;
    ret = setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if (ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(m_listenFd);
        return false;
    }

    ret = bind(m_listenFd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    ret = listen(m_listenFd, 6);
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", m_port);
        close(m_listenFd);
        return false;
    }

    ret = m_epoller->addFd(m_listenFd, m_listenEvent | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(m_listenFd);
        return false;
    }

    setFdNonblock(m_listenFd);
    LOG_INFO("Server port:%d", m_port);
    return true;
}

int WebServer::setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}