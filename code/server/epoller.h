#ifndef __EPOLLER_H__
#define __EPOLLER_H__

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <vector>

class Epoller {
   public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    bool addFd(int fd, uint32_t t_events);

    bool modFd(int fd, uint32_t t_events);

    bool delFd(int fd);

    int wait(int timeoutMs = -1);

    int getEventFd(size_t i) const;

    uint32_t getEvents(size_t i) const;

   private:
    int m_epfd;
    std::vector<struct epoll_event> m_events;
};

#endif  // __EPOLLER_H__