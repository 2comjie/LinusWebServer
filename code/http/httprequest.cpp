#include "httprequest.h"

const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index",
    "/register",
    "/login",
    "/welcome",
    "/video",
    "/picture",
};

const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

void HttpRequest::init() {
    m_method = m_path = m_version = m_body = "";
    m_state = REQUEST_LINE;
    m_header.clear();
    m_post.clear();
}

bool HttpRequest::isKeepAlive() const {
    if (m_header.count("Connection") == 1) {
        return m_header.find("Connection")->second == "keep-alive" && m_version == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if (buff.readAbleBytes() <= 0) {
        return false;
    }

    while (buff.readAbleBytes() && m_state != FINISH) {
        const char* lineEnd = std::search(buff.peek(), buff.beginWriteConst(), CRLF, CRLF + 2);
        std::string line(buff.peek(), lineEnd);
        switch (m_state) {
            case REQUEST_LINE:
                if (!parseRequestLine(line)) {
                    return false;
                }
                parsePath();
                break;
            case HEADERS:
                parseHeader(line);
                if (buff.readAbleBytes() <= 2) {
                    m_state = FINISH;
                }
                break;
            case BODY:
                parseBody(line);
                break;
            default:
                break;
        }

        if (lineEnd == buff.beginWrite()) {
            break;
        }
        buff.retrieveUntil(lineEnd + 2);
    }
    LOG_DEBUG("[%s], [%s], [%s]", m_method.c_str(), m_path.c_str(), m_version.c_str());
    return true;
}

void HttpRequest::parsePath() {
    if (m_path == "/") {
        m_path = "/index.html";
    } else {
        for (auto& item : DEFAULT_HTML) {
            if (item == m_path) {
                m_path += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    std::regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, patten)) {
        m_method = subMatch[1];
        m_path = subMatch[2];
        m_version = subMatch[3];
        m_state = HEADERS;
        return true;
    }
    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::parseHeader(const std::string& line) {
    std::regex patten("^([^:]*): ?(.*)$");
    std::smatch subMatch;
    if (std::regex_match(line, subMatch, patten)) {
        m_header[subMatch[1]] = subMatch[2];
    } else {
        m_state = BODY;
    }
}

void HttpRequest::parseBody(const std::string& line) {
    m_body = line;
    parsePost();
    m_state = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

int HttpRequest::converHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::parsePost() {
    if (m_method == "POST" && m_header["Content-Type"] == "application/x-www-form-urlencoded") {
        parseFromUrlencoded();
        if (DEFAULT_HTML_TAG.count(m_path)) {
            int tag = DEFAULT_HTML_TAG.find(m_path)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                if (userVerify(m_post["username"], m_post["password"], isLogin)) {
                    m_path = "/welcome.html";
                } else {
                    m_path = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::parseFromUrlencoded() {
    if (m_body.size() == 0) {
        return;
    }
    // 检查请求体是否为空，如果是，则返回

    std::string key, value;
    int num = 0;
    int n = m_body.size();
    int i = 0, j = 0;
    // 初始化一些变量：key 和 value 用于存储键值对，num 用于临时存储转换后的十六进制数，n 是请求体的长度，i 和 j 用于遍历请求体

    for (; i < n; i++) {
        char ch = m_body[i];
        switch (ch) {
            case '=':
                key = m_body.substr(j, i - j);
                j = i + 1;
                break;
                // 当遇到 '=' 时，提取当前键并将 j 指向下一个字符的位置

            case '+':
                m_body[i] = ' ';
                break;
                // 将 '+' 替换为空格

            case '%':
                num = converHex(m_body[i + 1]) * 16 + converHex(m_body[i + 2]);
                m_body[i + 2] = num % 10 + '0';
                m_body[i + 1] = num / 10 + '0';
                i += 2;
                break;
                // 处理 URL 编码的字符，将 '%' 后面的两位十六进制数转换为对应的字符

            case '&':
                value = m_body.substr(j, i - j);
                j = i + 1;
                m_post[key] = value;
                LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
                // 当遇到 '&' 时，提取当前值并将 j 指向下一个字符的位置，将键值对存储在 m_post 中并输出调试信息

            default:
                break;
                // 其他情况不处理
        }
    }
    assert(j <= i);
    // 断言确保 j 不大于 i，即确保在处理完所有字符后 j 指向的位置合法

    if (m_post.count(key) == 0 && j < i) {
        value = m_body.substr(j, i - j);
        m_post[key] = value;
    }
    // 如果最后一个键值对未存储（即 j 和 i 不相等），则将最后一个键值对存储在 m_post 中
}

bool HttpRequest::userVerify(const std::string& name, const std::string& pwd, bool isLogin) {
    if (name == "" || pwd == "") {
        return false;
    }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    MYSQL* sql;
    SqlConnRII(&sql, SqlConnPool::instance());
    assert(sql);

    bool flag = false;
    char order[256] = {0};
    MYSQL_RES* res = nullptr;

    if (!isLogin) {
        flag = true;
    }
    /* 查询用户及密码 */
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);

    if (mysql_query(sql, order)) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(sql);
    mysql_num_fields(res);
    mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        /* 注册行为 且 用户名未被使用*/
        if (isLogin) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        } else {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if (!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG("%s", order);
        if (mysql_query(sql, order)) {
            LOG_DEBUG("Insert error!");
            flag = false;
        }
        flag = true;
    }
    SqlConnPool::instance()->freeConn(sql);
    LOG_DEBUG("UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const {
    return m_path;
}

std::string& HttpRequest::path() {
    return m_path;
}

std::string HttpRequest::method() const {
    return m_method;
}

std::string HttpRequest::version() const {
    return m_version;
}

std::string HttpRequest::getPost(const std::string& key) const {
    assert(key != "");
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}

std::string HttpRequest::getPost(const char* key) const {
    assert(key != nullptr);
    if (m_post.count(key) == 1) {
        return m_post.find(key)->second;
    }
    return "";
}