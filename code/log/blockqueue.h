#ifndef __BLOCKQUEUE_H__
#define __BLOCKQUEUE_H__

#include <assert.h>
#include <sys/time.h>

#include <condition_variable>
#include <deque>
#include <mutex>

// 阻塞队列
template <class T>
class BlockDeque {
   private:
    std::deque<T> deq;
    size_t m_capacity;
    std::mutex mtx;
    bool isClose;

    std::condition_variable condConsumer;  // 消费者
    std::condition_variable condProducer;  // 生产者

   public:
    explicit BlockDeque(size_t maxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void pushBack(const T& item);

    void pushFront(const T& item);

    bool pop(T& item);

    bool pop(T& item, int timeout);

    void flush();
};

template <class T>
BlockDeque<T>::BlockDeque(size_t maxCapacity) : m_capacity(maxCapacity) {
    assert(maxCapacity > 0);
    isClose = false;
}

template <class T>
BlockDeque<T>::~BlockDeque() {
    close();
}

template <class T>
void BlockDeque<T>::close() {
    {
        std::lock_guard<std::mutex> locker(mtx);
        deq.clear();
        isClose = true;
    }
    condProducer.notify_all();
    condConsumer.notify_all();
}

template <class T>
void BlockDeque<T>::flush() {
    // 唤醒所有生产者
    condProducer.notify_one();
}

template <class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx);
    deq.clear();
}

template <class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.back();
}

template <class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.size();
}

template <class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx);
    return m_capacity;
}

template <class T>
void BlockDeque<T>::pushBack(const T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while (deq.size() >= m_capacity) {
        condProducer.wait(locker);  // 生产者等待
    }
    deq.push_back(item);
    condConsumer.notify_one();  // 唤醒消费者
}

template <class T>
void BlockDeque<T>::pushFront(const T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while (deq.size() >= m_capacity) {
        condProducer.wait(locker);
    }
    deq.push_front(item);
    condConsumer.notify_one();  // 唤醒消费者
}

template <class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.empty();
}

template <class T>
bool BlockDeque<T>::full() {
    std::lock_guard<std::mutex> locker(mtx);
    return deq.size() >= m_capacity;
}

template <class T>
bool BlockDeque<T>::pop(T& item) {
    std::unique_lock<std::mutex> locker(mtx);
    while (deq.empty()) {
        condConsumer.wait(locker);
        if (isClose) {
            return false;
        }
    }
    item = deq.front();
    deq.pop_front();
    condProducer.notify_one();  // 唤醒生产者
    return true;
}

template <class T>
bool BlockDeque<T>::pop(T& item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx);
    while (deq.empty()) {
        if (condConsumer.wait_for(locker, std::chrono::seconds(timeout)) == std::cv_status::timeout) {
            return false;
        } else if (isClose) {
            return false;
        }
    }
    item = deq.front();
    deq.pop_front();
    condProducer.notify_one();  // 唤醒生产者
    return true;
}

#endif  // __BLOCKQUEUE_H__