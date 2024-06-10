#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "../../pool/threadpool.h"
#include "../httpconn.h"
void setnonblocking(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

int main(void) {
    int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(8888);

    int reuse = 1;
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(serv_sock, 5);
    int epfd = epoll_create1(0);
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.fd = serv_sock;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &ev);
    struct epoll_event events[1024];
    HttpConn users[1024];

    char* srcDir_ = getcwd(nullptr, 256);

    strncat(srcDir_, "/resources/", 16);
    HttpConn::userCount = 0;
    HttpConn::srcDir = srcDir_;
    Log::instance()->init(0);
    SqlConnPool::instance()->init("localhost", 3306, "jie", "jie", "webserver");
    ThreadPool threadPool = ThreadPool(8);
    while (true) {
        int num = epoll_wait(epfd, events, 1024, -1);
        for (int i = 0; i < num; ++i) {
            int fd = events[i].data.fd;
            if (fd == serv_sock) {
                struct sockaddr_in clnt_addr;
                socklen_t clnt_len = sizeof(clnt_addr);
                int clnt_sock = accept(fd, (struct sockaddr*)&clnt_addr, &clnt_len);
                ev.data.fd = clnt_sock;
                ev.events = EPOLLIN | EPOLLET;
                setnonblocking(clnt_sock);
                epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &ev);
                users[clnt_sock].init(clnt_sock, clnt_addr);
            } else if (events[i].events & EPOLLIN) {
                int saveErrno;
                int readLen = users[fd].read(&saveErrno);
                if (readLen > 0) {
                    threadPool.addTask([users = users, fd = fd]() {
                        int saveErrno;
                        users[fd].process();
                        users[fd].write(&saveErrno);
                    });
                } else if (readLen == 0) {
                    users[fd].close();
                } else {
                    printf("something error\n");
                }
            } else {
            }
        }
    }

    close(epfd);
    close(serv_sock);
    return 0;
}