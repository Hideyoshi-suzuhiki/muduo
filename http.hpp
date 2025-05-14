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
        // &(*buf)[0]是为了获取起始地址的,但是这么写太难看了,C++11后使用data()获取
        // 顺带一提的是,这里原文档传的buf是指针,我改成引用了,都啥年代了还传指针......
        ifs.read(buf.data(), fsize);
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
    static std::string urldecode(const std::string url, bool convert_plus_to_space)
    {
        std::string res;
        for (int i = 0; i < url.size(); ++i)
        {
            if (url[i] == '+' && convert_plus_to_space == true)
            {
                res += ' ';
                continue;
            }
            if (url[i] == '%' && (i + 2) < url.size())
            {
                int v1 = HEXTOI(url[i + 1]);
                int v2 = HEXTOI(url[i + 2]);

                // 检查 HEXTOI 的返回值
                if (v1 != -1 && v2 != -1)
                {
                    char decoded_char = static_cast<char>(v1 * 16 + v2);
                    res += decoded_char;
                    i += 2;
                    continue;
                }
                else
                {
                    res += url[i];
                    std::cerr << "ERR_URL: %" << url[i + 1] << url[i + 2] << std::endl;
                    continue;
                }
            }
            res += url[i];
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
            return false;
        }
        return S_ISREG(st.st_mode);
    }
    // http请求的资源路径有效性判断
    static bool Validpath(const std::string &path)
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
    HttpRequest() : _version("HTTP/1.1");
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
    void SetHeader(const std::string &key, const std :; string & val)
    {
        _headers.insert(std::make_pair(key, val));
    }
    bool HaveHeader(const std::string &key) const
    {
        auto it = _headers.find(key);
        if (it == _headres.end())
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
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};

// HTTP响应
class HTTPResponse
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
    HttpResponse(int statu) : _redirect_flag(false), _statu(_statu) {}
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
        _header.insert(std::make_pair(key, val));
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
    void SetContent(cinst std::string &body, const std::stirng &type = "text/html")
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
        if (HasHeader("Connection") == true && GetHeader("Connection") == "keep-alive")
        {
            return false;
        }
        return true;
    }
};