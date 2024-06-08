/*
 * @Author       : jie
 * @Date         : 2024-06-08
 * @copyleft Apache 2.0
 * @brief HTTP连接
 */

#ifndef __HTTPCONN_H__
#define __HTTPCONN_H__

#include <arpa/inet.h>  // sockaddr_in
#include <errno.h>
#include <stdlib.h>  // atoi()
#include <sys/types.h>
#include <sys/uio.h>  // readv/writev

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "httprequest.h"
#include "httpresponse.h"

class HttpConn {
   public:
    HttpConn();
    ~HttpConn();

    void init(int t_sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);
    ssize_t write(int* saveErrno);

    void close();

    int getFd() const;
    int getPort() const;
    const char* getIP() const;
    sockaddr_in getAddr() const;

    bool process();

    int toWriteBytes() {
        return m_iov[0].iov_len + m_iov[1].iov_len;
    }
    bool isKeepAlive() const {
        return m_request.isKeepAlive();
    }

    static bool isET;
    static const char* srcDir;
    static std::atomic<int> userCount;

   private:
    int m_fd;
    struct sockaddr_in m_addr;
    bool m_isClose;
    int m_iovCnt;
    struct iovec m_iov[2];
    Buffer m_readBuff;
    Buffer m_writeBuff;

    HttpRequest m_request;
    HttpResponse m_response;
};

#endif  // __HTTPCONN_H__