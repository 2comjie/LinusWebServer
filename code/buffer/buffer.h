#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <vector>

class Buffer {
   private:
    std::vector<char> buffer;
    std::atomic<size_t> readPos;
    std::atomic<size_t> writePos;

   private:
    // 返回缓冲区的起始指针
    char* beginPtr();
    const char* beginPtr() const;
    // 确保缓冲区有足够的空间
    void makeSpace(size_t len);

   public:
    Buffer(int bufferSize = 1024);
    ~Buffer() = default;

    // 返回可写入的字节数
    size_t writAbleBytes() const;
    // 返回可读取的字节数
    size_t readAbleBytes() const;
    // 返回预留空间的字节数
    size_t prependableBytes() const;

    // 返回缓冲区头指针
    const char* peek() const;
    // 确保可写入指定长度的数据
    void ensureWriteable(size_t len);
    // 标记已写入指定长度的数据
    void hasWritten(size_t len);

    // 读取指定长度的数据
    void retrieve(size_t len);
    // 读取直到指定结束字符的数据
    void retrieveUntil(const char* end);

    // 读取所有数据
    void retrieveAll();
    // 读取所有数据转换成字符串
    std::string retrieveAllToStr();

    // 返回写入数据的起始指针
    const char* beginWriteConst() const;
    // 返回写入数据的起始指针
    char* beginWrite();

    // 追加数据：字符串、字符数组、任意数据、另一个buffer
    void append(const std::string& str);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buff);

    // 从文件描述符中读取数据
    ssize_t readFd(int fd, int* saveErrno);
    // 写入数据到文件描述符
    ssize_t writeFd(int fd, int* saveErrno);
};

#endif  // __BUFFER_H__