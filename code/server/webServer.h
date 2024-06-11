#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <unordered_map>

#include "../http/httpconn.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../timer/heaptimer.h"
#include "epoller.h"

class WebServer {
   public:
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    ~WebServer();

    void start();

   private:
    bool initSocket();
    void initEventMode(int trigMode);
    void addClient(int fd, sockaddr_in addr);

    void dealListen();
    void dealWrite(HttpConn* client);
    void dealRead(HttpConn* client);

    void sendError(int fd, const char* info);
    void extentTime(HttpConn* client);
    void closeConn(HttpConn* client);

    void onRead(HttpConn* client);
    void onWrite(HttpConn* client);
    void onProcess(HttpConn* client);

    static const int MAX_FD = 65536;

    static int setFdNonblock(int fd);

   private:
    int m_port;
    bool m_openLinger;
    int m_timeoutMS;
    bool m_isClose;
    int m_listenFd;
    char* m_srcDir;

    uint32_t m_listenEvent;
    uint32_t m_connEvent;

    std::unique_ptr<HeapTimer> m_timer;
    std::unique_ptr<ThreadPool> m_threadPool;
    std::unique_ptr<Epoller> m_epoller;
    std::unordered_map<int, HttpConn> m_users;
};
#endif  // __WEBSERVER_H__