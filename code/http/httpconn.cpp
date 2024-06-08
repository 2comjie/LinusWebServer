#include "httpconn.h"

const char* HttpConn::srcDir;
std::atomic<int> HttpConn::userCount;
bool HttpConn::isET;

HttpConn::HttpConn() {
    m_fd = -1;
    m_addr = {0};
    m_isClose = true;
}

HttpConn::~HttpConn() {
    close();
}

void HttpConn::init(int fd, const sockaddr_in& addr) {
    assert(fd > 0);
    userCount++;
    m_addr = addr;
    m_fd = fd;
    m_writeBuff.retrieveAll();
    m_readBuff.retrieveAll();
    m_isClose = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", m_fd, getIP(), getPort(), (int)userCount);
}

void HttpConn::close() {
    m_response.unmapFile();
    if (m_isClose == false) {
        m_isClose = true;
        userCount--;
        ::close(m_fd);
        LOG_INFO("Client[%d](%s:%d) quit, UserCount:%d", m_fd, getIP(), getPort(), (int)userCount);
    }
}

int HttpConn::getFd() const {
    return m_fd;
};

struct sockaddr_in HttpConn::getAddr() const {
    return m_addr;
}

const char* HttpConn::getIP() const {
    return inet_ntoa(m_addr.sin_addr);
}

int HttpConn::getPort() const {
    return m_addr.sin_port;
}

ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = m_readBuff.readFd(m_fd, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET);
    return len;
}

ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        len = writev(m_fd, m_iov, m_iovCnt);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        if (m_iov[0].iov_len + m_iov[1].iov_len == 0) {
            break;
        } /* 传输结束 */
        else if (static_cast<size_t>(len) > m_iov[0].iov_len) {
            m_iov[1].iov_base = (uint8_t*)m_iov[1].iov_base + (len - m_iov[0].iov_len);
            m_iov[1].iov_len -= (len - m_iov[0].iov_len);
            if (m_iov[0].iov_len) {
                m_writeBuff.retrieveAll();
                m_iov[0].iov_len = 0;
            }
        } else {
            m_iov[0].iov_base = (uint8_t*)m_iov[0].iov_base + len;
            m_iov[0].iov_len -= len;
            m_writeBuff.retrieve(len);
        }
    } while (isET || toWriteBytes() > 10240);
    return len;
}

bool HttpConn::process() {
    m_request.init();
    if (m_readBuff.readAbleBytes() <= 0) {
        return false;
    } else if (m_request.parse(m_readBuff)) {
        LOG_DEBUG("%s", m_request.path().c_str());
        m_response.init(srcDir, m_request.path(), m_request.isKeepAlive(), 200);
    } else {
        m_response.init(srcDir, m_request.path(), false, 400);
    }

    m_response.makeResponse(m_writeBuff);
    /* 响应头 */
    m_iov[0].iov_base = const_cast<char*>(m_writeBuff.peek());
    m_iov[0].iov_len = m_writeBuff.readAbleBytes();
    m_iovCnt = 1;

    /* 文件 */
    if (m_response.fileLen() > 0 && m_response.file()) {
        m_iov[1].iov_base = m_response.file();
        m_iov[1].iov_len = m_response.fileLen();
        m_iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, %d  to %d", m_response.fileLen(), m_iovCnt, toWriteBytes());
    return true;
}