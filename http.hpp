#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <sys/stat.h>
#include "server.hpp"

#define DEFALT_TIMEOUT 10

std::unordered_map<int, std::string> _statu_msg = {
    {100, "Continue"},
    {101, "Switching Protocol"},
    {102, "Processing"},
    {103, "Early Hints"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},
    {300, "Multiple Choice"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {306, "unused"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}};

std::unordered_map<std::string, std::string> _mime_msg = {
    {".aac", "audio/aac"},
    {".abw", "application/x-abiword"},
    {".arc", "application/x-freearc"},
    {".avi", "video/x-msvideo"},
    {".azw", "application/vnd.amazon.ebook"},
    {".bin", "application/octet-stream"},
    {".bmp", "image/bmp"},
    {".bz", "application/x-bzip"},
    {".bz2", "application/x-bzip2"},
    {".csh", "application/x-csh"},
    {".css", "text/css"},
    {".csv", "text/csv"},
    {".doc", "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".eot", "application/vnd.ms-fontobject"},
    {".epub", "application/epub+zip"},
    {".gif", "image/gif"},
    {".htm", "text/html"},
    {".html", "text/html"},
    {".ico", "image/vnd.microsoft.icon"},
    {".ics", "text/calendar"},
    {".jar", "application/java-archive"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".jsonld", "application/ld+json"},
    {".mid", "audio/midi"},
    {".midi", "audio/x-midi"},
    {".mjs", "text/javascript"},
    {".mp3", "audio/mpeg"},
    {".mpeg", "video/mpeg"},
    {".mpkg", "application/vnd.apple.installer+xml"},
    {".odp", "application/vnd.oasis.opendocument.presentation"},
    {".ods", "application/vnd.oasis.opendocument.spreadsheet"},
    {".odt", "application/vnd.oasis.opendocument.text"},
    {".oga", "audio/ogg"},
    {".ogv", "video/ogg"},
    {".ogx", "application/ogg"},
    {".otf", "font/otf"},
    {".png", "image/png"},
    {".pdf", "application/pdf"},
    {".ppt", "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".rar", "application/x-rar-compressed"},
    {".rtf", "application/rtf"},
    {".sh", "application/x-sh"},
    {".svg", "image/svg+xml"},
    {".swf", "application/x-shockwave-flash"},
    {".tar", "application/x-tar"},
    {".tif", "image/tiff"},
    {".tiff", "image/tiff"},
    {".ttf", "font/ttf"},
    {".txt", "text/plain"},
    {".vsd", "application/vnd.visio"},
    {".wav", "audio/wav"},
    {".weba", "audio/webm"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".xhtml", "application/xhtml+xml"},
    {".xls", "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".xml", "application/xml"},
    {".xul", "application/vnd.mozilla.xul+xml"},
    {".zip", "application/zip"},
    {".3gp", "video/3gpp"},
    {".3g2", "video/3gpp2"},
    {".7z", "application/x-7z-compressed"}};

class Util
{
public:
    static size_t split(const std::string &src, const std::string &sep, std::vector<std::string> &arr)
    {
        // 起始位置
        size_t offset = 0;
        while (offset < src.size())
        {
            size_t pos = src.find(sep, offset);
            if (pos == std::string::npos)
            {
                arr.push_back(src.substr(offset));
                return arr.size();
            }
            if (pos == offset)
            {
                offset = pos + sep.size();
                continue;
            }
            arr.push_back(src.substr(offset, pos - offset));
            offset = pos + sep.size();
        }
        return arr.size();
    }
    // 读取文件内容
    static bool ReadFile(const std::string &filename, std::string &buf)
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs.is_open())
        {
            std::cerr << "OPEN " << filename << " FAILED" << std::endl;
            return false;
        }
        size_t fsize = 0;
        // seekg设置输入流的读取位置,第一个参数为偏移量,第二个参数为起始位置
        ifs.seekg(0, ifs.end);
        // tellg返回当前输入流的读取位置相对于文件起始位置的偏移量
        fsize = ifs.tellg();
        ifs.seekg(0, ifs.beg); // 再跳转到起始位置
        buf.resize(fsize);
        // ifs.read(&(*buf)[0], fsize);  这是源文档的写法
        // &(*buf)[0]是为了获取起始地址的,但是这么写太难看了,C++11后使用&front()获取
        // 顺带一提的是,这里原文档传的buf是指针,我改成引用了,都啥年代了还传指针......
        ifs.read(&buf.front(), fsize);
        if (!ifs.good())
        {
            std::cerr << "READ " << filename << " FAILED" << std::endl;
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }
    // 向文件写入内容
    static bool WriteFile(const std::string &filename, const std::string &buf)
    {
        // std::ios_base::trunc: 如果文件已存在，则将其内容截断（清空）到零长度
        // ofstream类是隐式包含std::ios::out的
        std::ofstream ofs(filename, std::ios::binary | std::ios::trunc);
        ofs.write(buf.c_str(), buf.size());
        if (!ofs.good())
        {
            std::cerr << "WRITE " << filename << " FAILED" << std::endl;
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
    // URL编码
    static std::string UrlEncode(const std::string &url, bool convert_space_to_plus)
    {
        std::string res;
        for (auto &c : url)
        {
            // 这几个不需要编码
            if (c == '.' || c == '-' || c == '_' || c == '~' || isalnum(c))
            {
                res += c;
                continue;
            }
            if (c == ' ' && convert_space_to_plus == true)
            {
                res += '+';
                continue;
            }
            char tmp[4] = {0};
            snprintf(tmp, 4, "%%%02X", c);
            res += tmp;
        }
        return res;
    }
    // 十六进制变整数
    static char HEXTOI(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c >= 'a' && c <= 'z')
        {
            return c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'Z')
        {
            return c - 'A' + 10;
        }
        std::cerr << "%后面不是有效字符" << std::endl;
        return -1;
    }
    // URL解码
    // 在 Util 类中
    static std::string UrlDecode(const std::string &url, bool convert_plus_to_space)
    {
        std::string res;
        for (int i = 0; i < url.size(); ++i)
        {
            if (url[i] == '+' && convert_plus_to_space)
            {
                res += ' ';
            }
            else if (url[i] == '%' && (i + 2) < url.size())
            {
                int v1 = HEXTOI(url[i + 1]);
                int v2 = HEXTOI(url[i + 2]);
                if (v1 != -1 && v2 != -1)
                {                                                          // 检查 HEXTOI 的返回值是否有效
                    char decoded_char = static_cast<char>((v1 << 4) | v2); // 使用位运算或 v1 * 16 + v2
                    res += decoded_char;
                    i += 2; // 成功解码，跳过 xx
                }
                else
                {
                    // 解码失败，只添加 '%'，让循环在下一次迭代通过 ++i 处理 url[i+1]
                    res += url[i];
                    std::cerr << "ERR_URL: %" << url[i + 1] << url[i + 2] << std::endl;
                }
            }
            else
            {
                res += url[i];
            }
        }
        return res;
    }
    // 通过HTTP状态码获取描述信息
    static std::string StatuDesc(int statu)
    {
        auto it = _statu_msg.find(statu);
        if (it != _statu_msg.end())
        {
            return it->second;
        }
        return "Unknow";
    }
    // 根据文件后缀名获取文件mime
    static std::string ExtMime(const std::string &filename)
    {
        size_t pos = filename.find_last_of('.'); // find的从后向前形式
        if (pos == std::string::npos)
        {
            return "application/octet-stream";
        }
        std::string ext = filename.substr(pos);
        auto it = _mime_msg.find(ext);
        if (it == _mime_msg.end())
        {
            return "application/octet-stream";
        }
        return it->second;
    }
    // 判断文件是否是目录
    static bool IsDir(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            std::cerr << "stat failed for " << filename << " in IsDir: " << strerror(errno) << std::endl;
            return false;
        }
        return S_ISDIR(st.st_mode);
    }
    // 判断文件是否为普通文件
    static bool IsRegular(const std::string &filename)
    {
        struct stat st;
        int ret = stat(filename.c_str(), &st);
        if (ret < 0)
        {
            std::cerr << "stat failed for " << filename << " in IsRegular: " << strerror(errno) << std::endl;
            return false;
        }
        return S_ISREG(st.st_mode);
    }
    // http请求的资源路径有效性判断
    static bool ValidPath(const std::string &path)
    {
        std::vector<std::string> subdir;
        split(path, "/", subdir);
        int height = 0;
        for (auto &dir : subdir)
        {
            if (dir == "..")
            {
                height--;
                if (height < 0)
                    return false;
                continue;
            }
            height++;
        }
        return true;
    }
};
// 解析和存储 HTTP 请求的信息
class HttpRequest
{
    /*
    借着这里顺便复习一下HTTP请求的结构(来自AI)
    1.请求行:方法(GET,POST等),URI-请求的资源路径,版本协议
    2.一系列键值对,提供请求的附加信息.例如客户端类型.每个头部字段占一行,以键:值的形式表示
    3.一个空行(只有回车换行符\r\n)用于分隔请求头部和请求正文
    4.请求正文
    */
public:
    std::string _method;                                   // 请求方法
    std::string _path;                                     // 资源路径
    std::string _version;                                  // 协议版本
    std::string _body;                                     // 请求正文
    std::smatch _matches;                                  // 资源路径的正则提取数据
    std::unordered_map<std::string, std::string> _headers; // 头部字段
    std::unordered_map<std::string, std::string> _params;  // 查询字符串
public:
    HttpRequest() : _version("HTTP/1.1") {}
    void ReSet()
    {
        _method.clear();
        _path.clear();
        _version = "HTTP/1.1";
        _body.clear();
        std::smatch match;
        _matches.swap(match);
        _headers.clear();
        _params.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }
    bool HaveHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }
        return true;
    }
    std::string GetHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    // 插入查询字符串
    void SetParam(const std::string &key, const std::string &val)
    {
        _params.insert(std::make_pair(key, val));
    }
    // 判断是否有某个指定的查询字符串
    bool HaveParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return false;
        }
        return true;
    }
    // 获取指定的查询字符串
    std::string GetParam(const std::string &key) const
    {
        auto it = _params.find(key);
        if (it == _params.end())
        {
            return "";
        }
        return it->second;
    }
    // 获取正文长度
    size_t ContentLength() const
    {
        bool ret = HaveHeader("Content-Length");
        if (!ret)
        {
            return 0;
        }
        std::string clen = GetHeader("Content-Length");
        return std::stol(clen);
    }
    // 判断是否是短链接
    bool Close() const
    {
        // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
        if (HaveHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

// HTTP响应
class HttpResponse
{
public:
    int _statu;                // HTTP状态码
    bool _redirect_flag;       // 是否需要重定向
    std::string _body;         // 正文内容
    std::string _redirect_url; // 重定向的URL
    // 存储 HTTP 响应的头部字段
    std::unordered_map<std::string, std::string> _headers;

public:
    HttpResponse() : _redirect_flag(false), _statu(200) {}
    HttpResponse(int statu) : _redirect_flag(false), _statu(statu) {}
    void ReSet()
    {
        _statu = 200;
        _redirect_flag = false;
        _body.clear();
        _redirect_url.clear();
        _headers.clear();
    }
    // 插入头部字段
    void SetHeader(const std::string &key, const std::string &val)
    {
        _headers.insert(std::make_pair(key, val));
    }
    bool HaveHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return false;
        }
        return true;
    }
    // 获取指定头部字段的值
    std::string GetHeader(const std::string &key)
    {
        auto it = _headers.find(key);
        if (it == _headers.end())
        {
            return "";
        }
        return it->second;
    }
    void SetContent(const std::string &body, const std::string &type = "text/html")
    {
        _body = body;
        SetHeader("Content-Type", type);
    }
    void SetRedirect(const std::string &url, int statu = 302)
    {
        _statu = statu;
        _redirect_flag = true;
        _redirect_url = url;
    }
    // 判断是否是短链接
    bool Close()
    {
        // 没有Connection字段，或者有Connection但是值是close，则都是短链接，否则就是长连接
        if (HaveHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

typedef enum
{
    RECV_HTTP_ERROR,
    RECV_HTTP_LINE, // 正在接受起始行
    RECV_HTTP_HEAD, // 正在接受请求头
    RECV_HTTP_BODY, // 正在接受正文
    RECV_HTTP_OVER  // 结束阶段
} HttpRecvStatu;

// 记录HTTP请求处理
#define MAX_LINE 8192
class HttpContext
{
private:
    int _resp_statu;           // 响应状态码
    HttpRecvStatu _recv_statu; // 当前接受/解析的状态
    HttpRequest _request;      // 已经解析得到的信息
private:
    // 根据当前的 _recv_statu 状态，分阶段调用内部的解析函数
    bool RecvHttpLine(Buffer &buf)
    {
        if (_recv_statu != RECV_HTTP_LINE)
            return false;
        std::string line = buf.GetLineAndPop();
        if (line.size() == 0)
        {
            // 这里结合GetLine就可以知道是返回了一个空字符串,也就是没找到\n
            // 那就只有两种情况了
            // 1.已经把MAX_LINE堆满了还没找到\n,这样的是无效请求
            // 2.MAX_LINE没有满,但是数据还没用完全输送过来
            if (buf.ReadAbleSize() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }
            return true;
        }
        if (line.size() > MAX_LINE)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 414; // URI TOO LONG
            return false;
        }
        bool ret = ParseHttpline(line);
        if (!ret)
        {
            return false;
        }
        _recv_statu = RECV_HTTP_HEAD;
        return true;
    }
    bool ParseHttpline(const std::string &line)
    {
        std::smatch matches;
        std::regex e("(GET|HEAD|POST|PUT|DELETE) ([^?]*)(?:\\?(.*))? (HTTP/1\\.[01])(?:\n|\r\n)?", std::regex::icase);
        bool ret = std::regex_match(line, matches, e);
        if (!ret)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400;
            return false;
        }
        _request._method = matches[1];
        // transform()转大写
        std::transform(_request._method.begin(), _request._method.end(),
                       _request._method.begin(), ::toupper);
        _request._path = Util::UrlDecode(matches[2], false);
        _request._version = matches[4];
        std::vector<std::string> query_string_array;
        std::string query_string = matches[3];
        Util::split(query_string, "&", query_string_array);
        for (auto &str : query_string_array)
        {
            size_t pos = str.find("=");
            if (pos == std::string::npos)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 400; // BAD REQUEST
                return false;
            }
            std::string key = Util::UrlDecode(str.substr(0, pos), true);
            std::string val = Util::UrlDecode(str.substr(pos + 1), true);
            _request.SetParam(key, val);
        }
        return true;
    }
    bool RecvHttpHead(Buffer &buf)
    {
        if (_recv_statu != RECV_HTTP_HEAD)
            return false;
        while (1)
        {
            std::string line = buf.GetLineAndPop();
            if (line.size() == 0)
            {
                if (buf.ReadAbleSize() > MAX_LINE)
                {
                    _recv_statu = RECV_HTTP_ERROR;
                    _resp_statu = 414; // URI TOO LONG
                    return false;
                }
                return true;
            }
            if (line.size() > MAX_LINE)
            {
                _recv_statu = RECV_HTTP_ERROR;
                _resp_statu = 414; // URI TOO LONG
                return false;
            }
            if (line == "\n" || line == "\r\n")
            {
                break;
            }
            bool ret = ParseHttpHead(line);
            if (ret == false)
            {
                return false;
            }
        }
        _recv_statu = RECV_HTTP_BODY;
        return true;
    }
    bool ParseHttpHead(std::string &line)
    {
        if (line.back() == '\n')
            line.pop_back();
        if (line.back() == '\r')
            line.pop_back();
        size_t pos = line.find(": ");
        if (pos == std::string::npos)
        {
            _recv_statu = RECV_HTTP_ERROR;
            _resp_statu = 400;
            return false;
        }
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 2);
        _request.SetHeader(key, val);
        return true;
    }
    bool RecvHttpBody(Buffer &buf)
    {
        if (_recv_statu != RECV_HTTP_BODY)
            return false;
        // 1.获取正文长度
        size_t content_length = _request.ContentLength();
        if (content_length == 0)
        {
            // 没有正文，则请求接收解析完毕
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }
        size_t need_len = content_length - _request._body.size();
        if (buf.ReadAbleSize() >= need_len)
        {
            _request._body.append(buf.ReadPosition(), need_len);
            buf.MoveReadOffset(need_len);
            _recv_statu = RECV_HTTP_OVER;
            return true;
        }
        _request._body.append(buf.ReadPosition(), buf.ReadAbleSize());
        buf.MoveReadOffset(buf.ReadAbleSize());
        return true;
    }

public:
    HttpContext() : _resp_statu(200), _recv_statu(RECV_HTTP_LINE) {}
    void ReSet()
    {
        _resp_statu = 200;
        _recv_statu = RECV_HTTP_LINE;
        _request.ReSet();
    }
    int GetRespStatu() { return _resp_statu; }
    HttpRecvStatu GetRecvStatu() { return _recv_statu; }
    HttpRequest &GetRequest() { return _request; }
    // 接收并解析HTTP请求
    void RecvHttpRequest(Buffer &buf)
    {
        // 不同的状态，做不同的事情，但是这里不要break， 因为处理完请求行后，应该立即处理头部，而不是退出等新数据
        switch (_recv_statu)
        {
        case RECV_HTTP_LINE:
            RecvHttpLine(buf);
        case RECV_HTTP_HEAD:
            RecvHttpHead(buf);
        case RECV_HTTP_BODY:
            RecvHttpBody(buf);
        }
        return;
    }
};

class HttpServer
{
private:
    using Handler = std::function<void(const HttpRequest &, HttpResponse *)>;
    using Handlers = std::vector<std::pair<std::regex, Handler>>;
    Handlers _get_route;
    Handlers _post_route;
    Handlers _put_route;
    Handlers _delete_route;
    std::string _basedir;
    TcpServer _server;

private:
    void ErrorHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        // 1. 组织一个错误展示页面
        std::string body;
        body += "<html>";
        body += "<head>";
        body += "<meta http-equiv='Content-Type' content='text/html;charset=utf-8'>";
        body += "</head>";
        body += "<body>";
        body += "<h1>";
        body += std::to_string(rsp->_statu);
        body += " ";
        body += Util::StatuDesc(rsp->_statu);
        body += "</h1>";
        body += "</body>";
        body += "</html>";
        // 2. 将页面数据，当作响应正文，放入rsp中
        rsp->SetContent(body, "text/html");
    }
    // 将HttpResponse的要素按照Http协议组织,发送
    void WriteReponse(const PtrConnection &conn, const HttpRequest &req, HttpResponse &rsp)
    {
        // 1.HTTP头部
        // 判断是不是短连接
        if (req.Close())
        {
            rsp.SetHeader("Connection", "close");
        }
        else
        {
            rsp.SetHeader("Connection", "keep-alive");
        }
        // 如果正文内容非空而且用户也没有进行设置长度时
        if (!rsp._body.empty() && !rsp.HaveHeader("Content-Length"))
        {
            rsp.SetHeader("Content-Length", std::to_string(rsp._body.size()));
        }
        // 看有没有重定向
        if (rsp._redirect_flag)
        {
            rsp.SetHeader("Location", rsp._redirect_url);
        }
        // 2.按照HTTP协议格式进行组织
        std::stringstream rsp_str;
        // 协议版本->HTTP状态码(string)->HTTP状态码获取到的信息
        rsp_str << req._version << " " << std::to_string(rsp._statu) << " " << Util::StatuDesc(rsp._statu) << "\r\n";
        // HTTP头
        for (auto &head : rsp._headers)
        {
            rsp_str << head.first << ": " << head.second << "\r\n";
        }
        rsp_str << "\r\n";
        // 正文部分,如HTML
        rsp_str << rsp._body;
        // 3.发送
        conn->Send(rsp_str.str().c_str(), rsp_str.str().size());
    }
    // 静态资源请求处理
    bool IsFileHandler(const HttpRequest &req)
    {
        if (_basedir.empty())
        {
            return false;
        }
        // 请求方法必须是GET/HEAD(HEAD类似于GET,但只请求头部)
        if (req._method != "GET" && req._method != "HEAD")
        {
            return false;
        }
        if (Util::ValidPath(req._path) == false)
        {
            return false;
        }
        std::string tmp_path = _basedir + req._path;
        if (req._path.back() == '/')
        {
            tmp_path += "index.html";
        }
        if (!Util::IsRegular(tmp_path))
        {
            return false;
        }
        return true;
    }
    // 静态资源的请求处理->读取静态资源文件放到正文里面
    void FileHandler(const HttpRequest &req, HttpResponse *rsp)
    {
        std::string req_path = _basedir + req._path;
        if (req_path.back() == '/')
        {
            req_path += "index.html";
        }
        bool ret = Util::ReadFile(req_path, rsp->_body);
        if (!ret)
        {
            return;
        }
        std::string mime = Util::ExtMime(req_path);
        rsp->SetHeader("Content-Type", mime);
        return;
    }
    // 在路由表里面进行查找
    void Dispatcher(HttpRequest &req, HttpResponse *rsp, Handlers &handlers)
    {
        for (auto &handler : handlers)
        {
            const std::regex &re = handler.first;
            const Handler &functor = handler.second;
            bool ret = std::regex_match(req._path, req._matches, re);
            if (!ret)
            {
                continue;
            }
            return functor(req, rsp);
        }
        rsp->_statu = 404;
    }
    void Route(HttpRequest &req, HttpResponse *rsp)
    {
        if (IsFileHandler(req) == true)
        {
            // 是一个静态资源请求, 则进行静态资源请求的处理
            return FileHandler(req, rsp);
        }
        if (req._method == "GET" || req._method == "HEAD")
        {
            return Dispatcher(req, rsp, _get_route);
        }
        else if (req._method == "POST")
        {
            return Dispatcher(req, rsp, _post_route);
        }
        else if (req._method == "PUT")
        {
            return Dispatcher(req, rsp, _put_route);
        }
        else if (req._method == "DELETE")
        {
            return Dispatcher(req, rsp, _delete_route);
        }
        rsp->_statu = 405; // Method Not Allowed
        return;
    }
    // 当TcpServer接受一个新的TCP连接的时候,就会调用这个函数
    // 为新连接设置一个HttpContext对象,存储该连接上HTTP请求的解析状态/数据
    void OnConnected(const PtrConnection &conn)
    {
        std::cout << "!!!!!! [HttpServer::OnConnected] START - New connection: " << conn.get() << std::endl;
        conn->SetContext(HttpContext()); // 这一步本身也可能出问题，如果 HttpContext 构造函数复杂或 PtrConnection 实现有误
        std::cout << "!!!!!! [HttpServer::OnConnected] END - Context set for: " << conn.get() << std::endl;
        // DBG_LOG("NEW CONNECTION %p", conn.get()); // 你原来的日志
    }
    // 缓冲区数据解析+处理
    void OnMessage(const PtrConnection &conn, Buffer *buf)
    {
        while (buf->ReadAbleSize() > 0)
        {
            // 1.获取上下文
            HttpContext *context = conn->GetContext()->get<HttpContext>();
            // 2.对缓冲区数据进行解析
            context->RecvHttpRequest(*buf);
            // 初始化响应
            HttpRequest &req = context->GetRequest();
            HttpResponse rsp(context->GetRespStatu());
            if (context->GetRespStatu() > 400)
            {
                // 进行错误响应，关闭连接
                ErrorHandler(req, &rsp);                  // 填充一个错误显示页面数据到rsp中
                WriteReponse(conn, req, rsp);             // 把错误界面发送给客户端
                context->ReSet();                         // 重置context以便处理下一个请求
                buf->MoveReadOffset(buf->ReadAbleSize()); // 出错了就把缓冲区数据清空
                conn->Shutdown();                         // 关闭连接
                return;
            }
            if (context->GetRecvStatu() != RECV_HTTP_OVER)
            {
                // 当前请求还没有接收完整,则退出，等新数据到来再重新继续处理
                return;
            }
            // 3.路由
            Route(req, &rsp);
            // 4.响应
            WriteReponse(conn, req, rsp);
            // 5.重置上下文
            context->ReSet();
            // 6.根据长短链接判断是否关闭
            if (rsp.Close())
            {
                conn->Shutdown();
            }
        }
    }

public:
    HttpServer(int port, int timeout = DEFALT_TIMEOUT) : _server(port)
    {
        _server.EnableInactiveRelease(timeout);
        _server.SetConnectedCallback(std::bind(&HttpServer::OnConnected, this, std::placeholders::_1));
        _server.SetMessageCallback(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
    }
    void SetBaseDir(const std::string &path)
    {
        assert(Util::IsDir(path) == true);
        _basedir = path;
    }
    void Get(const std::string &pattern, const Handler &handler)
    {
        _get_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Post(const std::string &pattern, const Handler &handler)
    {
        _post_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Put(const std::string &pattern, const Handler &handler)
    {
        _put_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void Delete(const std::string &pattern, const Handler &handler)
    {
        _delete_route.push_back(std::make_pair(std::regex(pattern), handler));
    }
    void SetThreadCount(int count)
    {
        _server.SetThreadCount(count);
    }
    void Listen()
    {
        _server.Start();
    }
};
