#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer(initBuffSize), readPos(0), writePos(0) {
}

size_t Buffer::readAbleBytes() const {
    return writePos - readPos;
}

size_t Buffer::writAbleBytes() const {
    return buffer.size() - writePos;
}

size_t Buffer::prependableBytes() const {
    return readPos;
}

const char* Buffer::peek() const {
    return beginPtr() + readPos;
}

void Buffer::retrieve(size_t len) {
    assert(len <= readAbleBytes());
    readPos += len;
}

void Buffer::retrieveUntil(const char* end) {
    assert(peek() <= end);
    retrieve(end - peek());
}

void Buffer::retrieveAll() {
    bzero(&buffer[0], buffer.size());
    readPos = 0;
    writePos = 0;
}

std::string Buffer::retrieveAllToStr() {
    std::string str(peek(), readAbleBytes());
    retrieveAll();
    return str;
}

const char* Buffer::beginWriteConst() const {
    return beginPtr() + writePos;
}

char* Buffer::beginWrite() {
    return beginPtr() + writePos;
}

void Buffer::hasWritten(size_t len) {
    writePos += len;
}

void Buffer::append(const std::string& str) {
    append(str.data(), str.length());
}

void Buffer::append(const void* data, size_t len) {
    assert(data);
    append(static_cast<const char*>(data), len);
}

void Buffer::append(const char* str, size_t len) {
    assert(str);
    ensureWriteable(len);
    std::copy(str, str + len, beginWrite());
    hasWritten(len);
}

void Buffer::append(const Buffer& buff) {
    append(buff.peek(), buff.readAbleBytes());
}

void Buffer::ensureWriteable(size_t len) {
    if (writAbleBytes() < len) {
        makeSpace(len);
    }
    assert(writAbleBytes() >= len);
}

ssize_t Buffer::readFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writeable = writAbleBytes();
    // 分散读，保证数据全部读取完
    iov[0].iov_base = beginPtr() + writePos;
    iov[0].iov_len = writeable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *saveErrno = errno;
    } else if (static_cast<size_t>(len) <= writeable) {
        writePos += len;
    } else {
        writePos = buffer.size();
        append(buff, len - writeable);
    }

    return len;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno) {
    size_t readSize = readAbleBytes();
    ssize_t len = write(fd, peek(), readSize);
    if (len < 0) {
        *saveErrno = errno;
        return len;
    }
    readPos += len;
    return len;
}

char* Buffer::beginPtr() {
    return &*buffer.begin();
}

const char* Buffer::beginPtr() const {
    return &*buffer.begin();
}

void Buffer::makeSpace(size_t len) {
    if (writAbleBytes() + prependableBytes() < len) {
        buffer.resize(writePos + len + 1);
    } else {
        size_t readable = readAbleBytes();
        std::copy(beginPtr() + readPos, beginPtr() + writePos, beginPtr());
        readPos = 0;
        writePos = readPos + readable;
        assert(readable == readAbleBytes());
    }
}
