/*
 * @Author       : jie
 * @Date         : 2024-06-08
 * @copyleft Apache 2.0
 * @brief 解析HTTP请求
 */
#ifndef __HTTPREQUEST_H__
#define __HTTPREQUEST_H__

#include <errno.h>
#include <mysql/mysql.h>

#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
   public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,     // 无请求
        GET_REQUEST,        // GET 请求
        BAD_REQUEST,        // 错误请求
        NO_RESOURSE,        // 无资源
        FORBIDDEN_REQUEST,  // 禁止请求
        FILE_REQUEST,       // 文件请求
        INTERNAL_ERROR,     // 内部错误
        CLOSED_CONNECTION,  // 连接关闭
    };

    HttpRequest() { init(); }
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& t_buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string getPost(const std::string& key) const;
    std::string getPost(const char* key) const;

    bool isKeepAlive() const;

    // TODO: 解析Json

   private:
    // 解析请求行
    bool parseRequestLine(const std::string& line);
    // 解析请求头
    void parseHeader(const std::string& line);
    // 解析请求体
    void parseBody(const std::string& line);

    // 解析路径
    void parsePath();
    // 解析post请求
    void parsePost();
    // 从 URL 编码格式中解析数据
    void parseFromUrlencoded();

    // FIXME: 单一职责原则，另外一个类做业务
    static bool userVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE m_state;  // 请求解析状态
    std::string m_method, m_path, m_version, m_body;
    std::unordered_map<std::string, std::string> m_header;  // 头部信息映射
    std::unordered_map<std::string, std::string> m_post;    // POST 数据映射

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int converHex(char ch);
};

#endif  // __HTTPREQUEST_H__