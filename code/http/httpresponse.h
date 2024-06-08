/*
 * @Author       : jie
 * @Date         : 2024-06-08
 * @copyleft Apache 2.0
 * @brief HTTP响应
 */

#ifndef __HTTPRESPONSE_H__  // 防止头文件重复包含
#define __HTTPRESPONSE_H__

#include <fcntl.h>     // 文件操作函数
#include <sys/mman.h>  // 内存映射函数
#include <sys/stat.h>  // 文件状态结构体
#include <unistd.h>    // POSIX 系统服务函数

#include <unordered_map>  // 无序映射容器

#include "../buffer/buffer.h"  // 自定义的缓冲区处理类
#include "../log/log.h"        // 日志处理类

class HttpResponse {
   public:
    HttpResponse();   // 构造函数
    ~HttpResponse();  // 析构函数

    /**
     * @brief 初始化 HttpResponse 对象
     * @param srcDir 资源文件路径
     * @param path 请求资源路径
     * @param isKeepAlive 是否保持连接
     * @param code 响应状态码
     */
    void init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);

    /**
     * @brief 生成 HTTP 响应
     * @param buff 待发送的数据缓冲区
     */
    void makeResponse(Buffer& buff);

    /**
     * @brief 取消文件映射
     */
    void unmapFile();
    /**
     * @brief 获取文件映射指针
     */
    char* file();

    /**
     * @brief 获取文件大小
     */
    size_t fileLen() const;

    /**
     * @brief 生成错误内容
     */
    void errorContent(Buffer& buff, std::string message);

    /**
     * @brief 获取响应状态码
     */
    int code() const { return m_code; }

   private:
    /**
     * @brief 添加状态行到缓冲区中
     * @param buff 待发送的数据缓冲区
     */
    void addStateLine(Buffer& buff);

    /**
     * @brief 添加响应头到缓冲区中
     * @param buff 待发送的数据缓冲区
     */
    void addHeader(Buffer& buff);

    /**
     * @brief 添加响应内容到缓冲区中
     * @param buff 待发送的数据缓冲区
     */
    void addContent(Buffer& buff);

    /**
     * @brief 生成错误页面 HTML
     */
    void errorHtml();

    /**
     * @brief 获取文件类型
     * @return 文件类型
     */
    std::string getFileType();

   private:
    int m_code;          // 响应状态码
    bool m_isKeepAlive;  // 是否保持连接

    std::string m_path;    // 请求资源路径
    std::string m_srcDir;  // 静态资源目录

    char* m_mmFile;            // 文件映射指针
    struct stat m_mmFileStat;  // 映射文件的状态信息

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 文件后缀名与类型的映射
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 状态码与状态描述的映射
    static const std::unordered_map<int, std::string> CODE_PATH;            // 状态码与文件路径的映射
};

#endif  // __HTTPRESPONSE_H__