#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include <assert.h>

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
class ThreadPool {
   public:
    explicit ThreadPool(size_t threadCount = 8) : m_pool(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; ++i) {
            std::thread([pool = m_pool] {
                std::unique_lock<std::mutex> locker(pool->m_mutex);
                while (true) {
                    if (!pool->m_tasks.empty()) {
                        auto task = std::move(pool->m_tasks.front());
                        pool->m_tasks.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    } else if (pool->m_isClosed) {
                        break;
                    } else {
                        pool->m_cond.wait(locker);
                    }
                }
            }).detach();
        }
    }

    ThreadPool() = default;

    ~ThreadPool() {
        if (static_cast<bool>(m_pool)) {
            {
                std::lock_guard<std::mutex> locker(m_pool->m_mutex);
                m_pool->m_isClosed = true;
            }
            m_pool->m_cond.notify_all();
        }
    }

    template <class F>
    void addTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(m_pool->m_mutex);
            m_pool->m_tasks.push(std::forward<F>(task));
        }
        // 唤醒一个消费者线程
        m_pool->m_cond.notify_one();
    }

   private:
    struct Pool {
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_isClosed;
        std::queue<std::function<void()>> m_tasks;
    };
    std::shared_ptr<Pool> m_pool;
};

#endif  // __THREADPOOL_H__