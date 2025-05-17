#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <ctime>
#include <functional>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <typeinfo>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#define INF 0
#define DBG 1
#define ERR 2
#define LOG_LEVEL DBG

#define LOG(level, format, ...)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        if (level < LOG_LEVEL)                                                                                         \
            break;                                                                                                     \
        time_t t = time(NULL);                                                                                         \
        struct tm *ltm = localtime(&t);                                                                                \
        char tmp[32] = {0};                                                                                            \
        strftime(tmp, 31, "%H:%M:%S", ltm);                                                                            \
        fprintf(stdout, "[%p %s %s:%d] " format "\n", (void *)pthread_self(), tmp, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define INF_LOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DBG_LOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ERR_LOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)
#define BUFFER_DEFAULT_SIZE 1024
class Buffer
{
private:
    std::vector<char> _buffer;
    uint64_t _reader_idx;
    uint64_t _writer_idx;

public:
    Buffer() : _buffer(BUFFER_DEFAULT_SIZE), _reader_idx(0), _writer_idx(0) {}
    char *Begin() // 缓冲区的开始,这里应该返回什么呢?
    {
        // return &*_buffer.begin();
        // 这里可以用data(),直接获取指向数组的指针
        return _buffer.data();
    }
    char *WritePosition() // 返回写的位置
    {
        return Begin() + _writer_idx;
    }
    char *ReadPosition()
    {
        return Begin() + _reader_idx;
    }
    uint64_t TailIdleSize() // 返回末尾的空闲区域大小
    {
        return &*_buffer.end() - WritePosition();
    }
    uint64_t HeadIdleSize() // 头部空闲区域大小
    {
        return _reader_idx;
    }
    uint64_t ReadAbleSize() // 可读区域大小
    {
        return _writer_idx - _reader_idx;
    }
    void MoveReadOffset(uint64_t len) // 读后移
    {
        if (!len)
        {
            return;
        }
        assert(len <= ReadAbleSize());
        _reader_idx += len;
    }
    void MoveWriteOffset(uint64_t len) // 写后移
    {
        assert(len <= TailIdleSize());
        _writer_idx += len;
    }
    void EnsureWriteSpace(uint64_t len) // 确保写入空间足够
    {
        if (len < TailIdleSize() + HeadIdleSize())
        {
            if (len > TailIdleSize())
            {
                uint64_t rsz = ReadAbleSize();
                std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
                _reader_idx = 0;
                _writer_idx = rsz;
            }
            else
            {
                return;
            }
        }
        else
        {
            _buffer.resize(_writer_idx + len);
        }
    }
    void Write(const void *data, uint64_t len) // 写入
    {
        if (len == 0)
        {
            return;
        }
        EnsureWriteSpace(len);
        const char *d = (const char *)data;
        std::copy(d, d + len, WritePosition());
    }
    void WriteAndPush(const void *data, uint64_t len)
    {
        Write(data, len);
        MoveWriteOffset(len);
    }
    void WriteString(const std::string &data)
    {
        Write(data.c_str(), data.size());
        return;
    }
    void WriteStringAndPush(const std::string &data)
    {
        WriteString(data);
        MoveWriteOffset(data.size());
        return;
    }
    void WriteBuffer(Buffer &data)
    {
        Write(data.ReadPosition(), data.ReadAbleSize());
    }
    void WriteBufferAndPush(Buffer &data)
    {
        WriteBuffer(data);
        MoveWriteOffset(data.ReadAbleSize());
    }
    void Read(void *buf, uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::copy(ReadPosition(), ReadPosition() + len, (char *)buf);
    }
    void ReadAndPop(void *buf, uint64_t len)
    {
        Read(buf, len);
        MoveReadOffset(len);
    }
    std::string ReadAsString(uint64_t len)
    {
        assert(len <= ReadAbleSize());
        std::string ans;
        ans.resize(len);
        Read(&ans[0], len); // mark
        return ans;
    }
    std::string ReadAsStringAndPop(uint64_t len)
    {
        std::string ans = ReadAsString(len);
        MoveReadOffset(len);
        return ans;
    }
    char *FindCRLF()
    {
        char *res = (char *)memchr(ReadPosition(), '\n', ReadAbleSize());
        return res;
    }
    std::string GetLine()
    {
        char *pos = FindCRLF();
        if (pos == NULL)
        {
            return "";
        }
        // +1是为了把换行字符也取出来。
        return ReadAsString(pos - ReadPosition() + 1);
    }
    std::string GetLineAndPop()
    {
        std::string ans = GetLine();
        MoveReadOffset(ans.size());
        return ans;
    }
    void Clear()
    {
        _reader_idx = 0;
        _writer_idx = 0;
    }
};

// #define MAX_LISTEN 1024
// class Socket
// {
// private:
//     int _sockfd;

// public:
//     Socket() : _sockfd(-1) {}
//     Socket(int fd) : _sockfd(fd) {}
//     ~Socket()
//     {
//         std::cout << "socket close~" << std::endl;
//         Close();
//     }
//     int Fd() { return _sockfd; }
//     // 创建套接字
//     bool Create()
//     {
//         // int socket(int domain, int type, int protocol)
//         _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//         if (_sockfd < 0)
//         {
//             ERR_LOG("创建失败了f!!");
//             return false;
//         }
//         return true;
//     }
//     // 绑定地址信息
//     bool Bind(const std::string &ip, uint16_t port)
//     {
//         struct sockaddr_in addr;
//         addr.sin_family = AF_INET;                    // 设置IPV4
//         addr.sin_port = htons(port);                  // 设置端口号
//         addr.sin_addr.s_addr = inet_addr(ip.c_str()); // 设置IP地址(点分十进制->网络字节序)
//         socklen_t len = sizeof(struct sockaddr_in);
//         // int bind(int sockfd, struct sockaddr*addr, socklen_t len);
//         int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
//         if (ret < 0)
//         {
//             ERR_LOG("BIND ADDRESS FAILED!");
//             return false;
//         }
//         return true;
//     }
//     // 开始监听
//     bool Listen(int backlog = MAX_LISTEN)
//     {
//         // int listen(int backlog)
//         int ret = listen(_sockfd, backlog);
//         if (ret < 0)
//         {
//             ERR_LOG("SOCKET LISTEN FAILED!");
//             return false;
//         }
//         return true;
//     }
//     // 向服务器发起连接
//     bool Connect(const std::string &ip, uint16_t port)
//     {
//         struct sockaddr_in addr;
//         addr.sin_family = AF_INET;
//         addr.sin_port = htons(port);
//         addr.sin_addr.s_addr = inet_addr(ip.c_str());
//         socklen_t len = sizeof(struct sockaddr_in);
//         // int connect(int sockfd, struct sockaddr*addr, socklen_t len);
//         int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
//         if (ret < 0)
//         {
//             ERR_LOG("CONNECT SERVER FAILED!");
//             return false;
//         }
//         return true;
//     }
//     // 获取新连接
//     int Accept()
//     {
//         // int accept(int sockfd, struct sockaddr *addr, socklen_t *len);
//         int newfd = accept(_sockfd, NULL, NULL);
//         if (newfd < 0)
//         {
//             ERR_LOG("SOCKET ACCEPT FAILED!");
//             return -1;
//         }
//         return newfd;
//     }

//     // 接收数据
//     ssize_t Recv(void *buf, size_t len, int flag = 0)
//     {
//         // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
//         // ssize_t ret = recv(_sockfd, buf, len, flag);
//         // if (ret <= 0)
//         // {
//         //     // EAGAIN 当前socket的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
//         //     // EINTR  表示当前socket的阻塞等待，被信号打断了，
//         //     if (errno == EAGAIN || errno == EINTR)
//         //     {
//         //         return 0; // 表示这次接收没有接收到数据
//         //     }
//         //     ERR_LOG("SOCKET RECV FAILED!!");
//         //     return -1;
//         // }
//         // return ret; // 实际接收的数据长度
//         ssize_t ret = recv(_sockfd, buf, len, flag);
//         if (ret == 0)
//         {
//             return 0;
//         }
//         if (ret < 0)
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//             {
//                 // EWOULDBLOCK 通常等于 EAGAIN
//                 // 非阻塞调用，现在没有数据可用
//                 // 返回 -1，调用者 **必须** 检查 errno
//                 return -1;
//             }
//             else if (errno == EINTR)
//             {
//                 // 被信号中断
//                 // 返回 -1，调用者 **必须** 检查 errno
//                 return -1;
//             }
//             // 真正的错误
//             ERR_LOG("SOCKET RECV 失败!! sockfd: %d, errno: %d, message: %s", _sockfd, errno, strerror(errno));
//             return -1;
//         }
//         return ret; // 实际接收的数据长度
//     }
//     ssize_t NonBlockRecv(void *buf, size_t len)
//     {
//         return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
//     }
//     // 发送数据
//     ssize_t Send(const void *buf, size_t len, int flag = 0)
//     {
//         // ssize_t send(int sockfd, void *data, size_t len, int flag);
//         ssize_t ret = send(_sockfd, buf, len, flag);
//         if (ret < 0)
//         {
//             if (errno == EAGAIN || errno == EINTR)
//             {
//                 return 0;
//             }
//             ERR_LOG("SOCKET SEND FAILED!!");
//             return -1;
//         }
//         return ret; // 实际发送的数据长度
//     }
//     ssize_t NonBlockSend(void *buf, size_t len)
//     {
//         if (len == 0)
//             return 0;
//         return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前发送为非阻塞。
//     }
//     // 关闭套接字
//     void Close()
//     {
//         if (_sockfd != -1)
//         {
//             close(_sockfd);
//             _sockfd = -1;
//         }
//     }
//     // 创建一个服务端连接
//     bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
//     {
//         // 1. 创建套接字，2. 绑定地址，3. 开始监听，4. 设置非阻塞， 5. 启动地址重用
//         if (Create() == false)
//             return false;
//         if (block_flag)
//             NonBlock();
//         if (Bind(ip, port) == false)
//             return false;
//         if (Listen() == false)
//             return false;
//         ReuseAddress();
//         return true;
//     }
//     // 创建一个客户端连接
//     bool CreateClient(uint16_t port, const std::string &ip)
//     {
//         // 1. 创建套接字，2.指向连接服务器
//         if (Create() == false)
//             return false;
//         if (Connect(ip, port) == false)
//             return false;
//         return true;
//     }
//     // 设置套接字选项---开启地址端口重用
//     void ReuseAddress()
//     {
//         // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
//         int val = 1;
//         setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
//         val = 1;
//         setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
//     }
//     // 设置套接字阻塞属性-- 设置为非阻塞
//     void NonBlock()
//     {
//         // int fcntl(int fd, int cmd, ... /* arg */ );
//         int flag = fcntl(_sockfd, F_GETFL, 0);
//         fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
//     }
// };
#define MAX_LISTEN 1024
class Socket
{
private:
    int _sockfd;

public:
    Socket() : _sockfd(-1) {}
    Socket(int fd) : _sockfd(fd) {}
    ~Socket() { Close(); }
    int Fd() { return _sockfd; }
    // 创建套接字
    bool Create()
    {
        // int socket(int domain, int type, int protocol)
        _sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_sockfd < 0)
        {
            ERR_LOG("CREATE SOCKET FAILED!!");
            return false;
        }
        std::cout << "socket创建成功" << std::endl;
        return true;
    }
    // 绑定地址信息
    bool Bind(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int bind(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = bind(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("BIND ADDRESS FAILED!");
            return false;
        }
        return true;
    }
    // 开始监听
    bool Listen(int backlog = MAX_LISTEN)
    {
        // int listen(int backlog)
        int ret = listen(_sockfd, backlog);
        if (ret < 0)
        {
            ERR_LOG("SOCKET LISTEN FAILED!");
            return false;
        }
        return true;
    }
    // 向服务器发起连接
    bool Connect(const std::string &ip, uint16_t port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        socklen_t len = sizeof(struct sockaddr_in);
        // int connect(int sockfd, struct sockaddr*addr, socklen_t len);
        int ret = connect(_sockfd, (struct sockaddr *)&addr, len);
        if (ret < 0)
        {
            ERR_LOG("CONNECT SERVER FAILED!");
            return false;
        }
        return true;
    }
    // 获取新连接
    int Accept()
    {
        // int accept(int sockfd, struct sockaddr *addr, socklen_t *len);
        int newfd = accept(_sockfd, NULL, NULL);
        if (newfd < 0)
        {
            ERR_LOG("SOCKET ACCEPT FAILED!");
            return -1;
        }
        return newfd;
    }
    // 接收数据
    ssize_t Recv(void *buf, size_t len, int flag = 0)
    {
        // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
        ssize_t ret = recv(_sockfd, buf, len, flag);
        if (ret <= 0)
        {
            // EAGAIN 当前socket的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
            // EINTR  表示当前socket的阻塞等待，被信号打断了，
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0; // 表示这次接收没有接收到数据
            }
            ERR_LOG("SOCKET RECV FAILED!!");
            return -1;
        }
        return ret; // 实际接收的数据长度
    }
    // 非阻塞接收
    ssize_t NonBlockRecv(void *buf, size_t len)
    {
        return Recv(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
    }
    // 发送数据
    ssize_t Send(const void *buf, size_t len, int flag = 0)
    {
        // ssize_t send(int sockfd, void *data, size_t len, int flag);
        ssize_t ret = send(_sockfd, buf, len, flag);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                return 0;
            }
            ERR_LOG("SOCKET SEND FAILED!!");
            return -1;
        }
        return ret; // 实际发送的数据长度
    }
    ssize_t NonBlockSend(void *buf, size_t len)
    {
        if (len == 0)
            return 0;
        return Send(buf, len, MSG_DONTWAIT); // MSG_DONTWAIT 表示当前发送为非阻塞。
    }
    // 关闭套接字
    void Close()
    {
        if (_sockfd != -1)
        {
            close(_sockfd);
            _sockfd = -1;
        }
    }
    // 创建一个服务端连接
    bool CreateServer(uint16_t port, const std::string &ip = "0.0.0.0", bool block_flag = false)
    {
        // 1. 创建套接字，2. 绑定地址，3. 开始监听，4. 设置非阻塞， 5. 启动地址重用
        if (Create() == false)
            return false;
        if (block_flag)
            NonBlock();
        if (Bind(ip, port) == false)
            return false;
        if (Listen() == false)
            return false;
        ReuseAddress();
        return true;
    }
    // 创建一个客户端连接
    bool CreateClient(uint16_t port, const std::string &ip)
    {
        // 1. 创建套接字，2.指向连接服务器
        if (Create() == false)
            return false;
        if (Connect(ip, port) == false)
            return false;
        return true;
    }
    // 设置套接字选项---开启地址端口重用
    void ReuseAddress()
    {
        // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
        int val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&val, sizeof(int));
        val = 1;
        setsockopt(_sockfd, SOL_SOCKET, SO_REUSEPORT, (void *)&val, sizeof(int));
    }
    // 设置套接字阻塞属性-- 设置为非阻塞
    void NonBlock()
    {
        // int fcntl(int fd, int cmd, ... /* arg */ );
        int flag = fcntl(_sockfd, F_GETFL, 0);
        fcntl(_sockfd, F_SETFL, flag | O_NONBLOCK);
    }
};
class Poller;
class EventLoop;
/*
Channel目的: 代表一个文件描述符（如 Socket fd）及其关注的事件和事件发生时的回调函数
它是 EventLoop 和具体 I/O 操作之间的桥梁。
Channel 不拥有文件描述符，它只是封装了 fd 的事件处理逻辑。
*/
class Channel
{
private:
    int _fd;
    EventLoop *_loop;  // 裸指针不代表拥有,仅有观察权
    uint32_t _events;  // 当前需要监控的事件
    uint32_t _revents; // 当前连接触发的事件
    using EventCallback = std::function<void()>;
    // Channel知道他有哪些回调函数,但是他不知道哪个线程调用回调函数
    // 回调函数的调用在Channel内部完成,但是Channel不知道回调函数的实现
    // 可以把Channel理解为一个"黑盒"存放处,存放和调用都在这里进行
    EventCallback _read_callback;  // 可读事件被触发的回调函数
    EventCallback _write_callback; // 可写事件被触发的回调函数
    EventCallback _error_callback; // 错误事件被触发的回调函数
    EventCallback _close_callback; // 连接断开事件被触发的回调函数
    EventCallback _event_callback; // 任意事件被触发的回调函数
public:
    Channel(EventLoop *loop, int fd) : _fd(fd), _events(0), _revents(0), _loop(loop) {}
    int Fd() { return _fd; }
    uint32_t Events() { return _events; }
    uint32_t Revents() const { return _revents; }
    void SetRevents(uint32_t events) { _revents = events; }
    void SetReadCallback(const EventCallback &cb) { _read_callback = cb; }
    void SetWriteCallback(const EventCallback &cb) { _write_callback = cb; }
    void SetErrorCallback(const EventCallback &cb) { _error_callback = cb; }
    void SetCloseCallback(const EventCallback &cb) { _close_callback = cb; }
    void SetEventCallback(const EventCallback &cb) { _event_callback = cb; }
    bool ReadAble() { return (_events & EPOLLIN); }
    bool WriteAble() { return (_events & EPOLLOUT); }
    void Remove();
    void Update();
    void EnableRead() // 启动读事件监控
    {
        _events |= EPOLLIN;
        Update();
    }
    void EnableWrite() // 启动写事件监控
    {
        _events |= EPOLLOUT;
        Update();
    }
    void DisableRead() // 关闭读事件监控
    {
        _events &= ~EPOLLIN;
        Update();
    }
    void DisableWrite() // 关闭写事件监控
    {
        _events &= ~EPOLLOUT;
        Update();
    }
    void CloseAll() // 关闭所有事件监控
    {
        _events = 0;
        Update();
    }
    void HandleEvent()
    {
        if ((_revents & EPOLLIN) || (_revents & EPOLLRDHUP) || (_revents & EPOLLPRI))
        {
            // 可读事件的回调是一定触发的
            if (_read_callback)
                _read_callback();
        }
        if (_revents & EPOLLOUT)
        {
            if (_write_callback)
                _write_callback();
        }
        else if (_revents & EPOLLERR)
        {
            // 一旦出错，就会释放连接，因此要放到前边调用任意回调
            if (_error_callback)
                _error_callback();
        }
        else if (_revents & EPOLLHUP)
        {
            if (_close_callback)
                _close_callback();
        }
        if (_event_callback)
            _event_callback();
    }
};
/*
目的: 封装 Linux epoll I/O 多路复用机制。
它负责监控多个 Channel 的事件，并在事件发生时通知它们。
*/
#define MAX_EPOLLEVENTS 1024
class Poller
{
private:
    int _epfd;
    // epoll_event结构体里面的两个成员uint32_t events和epoll_data_t data
    // events实际上是位掩码,通过EPOLLIN/EPOLLOUT等表示状态
    // epoll_wait 返回时，内核会填充 events 数组，告知哪些 fd 触发了哪些事件。
    struct epoll_event _evs[MAX_EPOLLEVENTS];
    std::unordered_map<int, Channel *> _channels;
    // 用哈希存Channel*的目的:一个Channel被创建且需要被监控的时候,就会调EventLoop::UpdateEvent
    // 然后就会调用Poller::UpdateEvent,在channels里面加入事件
    // 此时就有问题了,为什么一个Poller里面存多个Channel呢,一个Channel不能存放多个回调吗
    // Channel和事件描述符一一对应,每个事件描述符都有可能有其对应的回调函数
    // 所以要有多个Channel来对应多个文件描述符,这里也要提一下,EventLoop-Poller-线程是一一对应的
    void Update(Channel *channel, int op)
    {
        int fd = channel->Fd();
        // 直接访问用户空间数据可能导致安全问题,故这里使用临时变量来传入内核
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = channel->Events();
        int ret = epoll_ctl(_epfd, op, fd, &ev); // epoll_ctl实现增删改,op是具体实现的参数
        if (ret < 0)
        {
            ERR_LOG("EPOLLCTL FAILED!");
        }
        return;
    }
    bool HashChannel(Channel *channel)
    {
        auto it = _channels.find(channel->Fd());
        if (it == _channels.end())
        {
            return false;
        }
        return true;
    }

public:
    Poller()
    {
        _epfd = epoll_create(MAX_EPOLLEVENTS);
        if (_epfd < 0)
        {
            ERR_LOG("EPOLL CREATE FAILED!!");
            abort();
        }
    }
    // 添加或修改监控事件
    void UpdateEvent(Channel *channel)
    {
        bool ret = HashChannel(channel);
        if (!ret)
        {
            _channels.insert(std::make_pair(channel->Fd(), channel));
            Update(channel, EPOLL_CTL_ADD);
            return;
        }

        Update(channel, EPOLL_CTL_MOD);
    }
    // 移除监控
    void RemoveEvent(Channel *channel)
    {
        bool ret = HashChannel(channel);
        if (!ret)
            return;
        else
            _channels.erase(channel->Fd());
        Update(channel, EPOLL_CTL_DEL);
    }
    // 开始监控，返回活跃连接
    void Poll(std::vector<Channel *> *active)
    {
        int ew = epoll_wait(_epfd, _evs, MAX_EPOLLEVENTS, -1); // epoll_wait等待并获取已就绪的 I/O 事件
        if (ew < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("EPOLL WAIT ERROR:%s\n", strerror(errno));
            abort();
        }
        for (int i = 0; i < ew; ++i)
        {
            auto it = _channels.find(_evs[i].data.fd);
            assert(it != _channels.end());
            it->second->SetRevents(_evs[i].events);
            active->push_back(it->second);
        }
        return;
    }
};
using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask
{
private:
    uint64_t _id;         // 定时器任务对象ID
    uint32_t _timeout;    // 定时任务的超时时间
    bool _canceled;       // false-表示没有被取消， true-表示被取消
    TaskFunc _task_cb;    // 定时器对象要执行的定时任务
    ReleaseFunc _release; // 用于删除TimerWheel中保存的定时器对象信息
public:
    TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb) : _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
    ~TimerTask()
    {
        if (_canceled == false)
            _task_cb();
        _release();
    }
    void Cancel() { _canceled = true; }
    void SetRelease(const ReleaseFunc &cb) { _release = cb; }
    uint32_t DelayTime() { return _timeout; }
};

class TimerWheel
{
private:
    using WeakTask = std::weak_ptr<TimerTask>;
    using PtrTask = std::shared_ptr<TimerTask>;
    int _tick;     // 当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
    int _capacity; // 表盘最大数量---其实就是最大延迟时间
    std::vector<std::vector<PtrTask>> _wheel;
    std::unordered_map<uint64_t, WeakTask> _timers;

    EventLoop *_loop;
    int _timerfd; // 定时器描述符--可读事件回调就是读取计数器，执行定时任务
    std::unique_ptr<Channel> _timer_channel;

private:
    void RemoveTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it != _timers.end())
        {
            _timers.erase(it);
        }
    }
    static int CreateTimerfd()
    {
        int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
        if (timerfd < 0)
        {
            ERR_LOG("TIMERFD CREATE FAILED!");
            abort();
        }
        // int timerfd_settime(int fd, int flags, struct itimerspec *new, struct itimerspec *old);
        struct itimerspec itime;
        itime.it_value.tv_sec = 1;
        itime.it_value.tv_nsec = 0; // 第一次超时时间为1s后
        itime.it_interval.tv_sec = 1;
        itime.it_interval.tv_nsec = 0; // 第一次超时后，每次超时的间隔时
        timerfd_settime(timerfd, 0, &itime, NULL);
        return timerfd;
    }
    int ReadTimefd()
    {
        uint64_t times;
        // 有可能因为其他描述符的事件处理花费事件比较长，然后在处理定时器描述符事件的时候，有可能就已经超时了很多次
        // read读取到的数据times就是从上一次read之后超时的次数
        int ret = read(_timerfd, &times, 8);
        if (ret < 0)
        {
            ERR_LOG("READ TIMEFD FAILED!");
            abort();
        }
        return times;
    }
    // 这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
    void RunTimerTask()
    {
        _tick = (_tick + 1) % _capacity;
        _wheel[_tick].clear(); // 清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
    }
    void OnTime()
    {
        // 根据实际超时的次数，执行对应的超时任务
        int times = ReadTimefd();
        for (int i = 0; i < times; i++)
        {
            RunTimerTask();
        }
    }
    void TimerAddInLoop(uint64_t id, uint32_t delay, const TaskFunc &cb)
    {
        PtrTask pt(new TimerTask(id, delay, cb));
        pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
        _timers[id] = WeakTask(pt);
    }
    void TimerRefreshInLoop(uint64_t id)
    {
        // 通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock(); // lock获取weak_ptr管理的对象对应的shared_ptr
        int delay = pt->DelayTime();
        int pos = (_tick + delay) % _capacity;
        _wheel[pos].push_back(pt);
    }
    void TimerCancelInLoop(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return; // 没找着定时任务，没法刷新，没法延迟
        }
        PtrTask pt = it->second.lock();
        if (pt)
            pt->Cancel();
    }

public:
    TimerWheel(EventLoop *loop) : _capacity(60), _tick(0), _wheel(_capacity), _loop(loop),
                                  _timerfd(CreateTimerfd()), _timer_channel(new Channel(_loop, _timerfd))
    {
        _timer_channel->SetReadCallback(std::bind(&TimerWheel::OnTime, this));
        _timer_channel->EnableRead(); // 启动读事件监控
    }
    /*定时器中有个_timers成员，定时器信息的操作有可能在多线程中进行，因此需要考虑线程安全问题*/
    /*如果不想加锁，那就把对定期的所有操作，都放到一个线程中进行*/
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb);
    void TimerRefresh(uint64_t id);
    void TimerCancel(uint64_t id);
    /*这个接口存在线程安全问题--这个接口实际上不能被外界使用者调用，只能在模块内，在对应的EventLoop线程内执行*/
    bool HasTimer(uint64_t id)
    {
        auto it = _timers.find(id);
        if (it == _timers.end())
        {
            return false;
        }
        return true;
    }
};
/*
目的: 事件循环的核心，驱动整个事件处理流程。通常每个线程拥有一个 EventLoop 对象。它负责：
运行事件循环 (Start())。
管理 Channel 的添加、更新、移除 (通过 Poller)。
处理 I/O 事件和定时器事件。
执行跨线程提交的任务。
*/
class EventLoop
{
private:
    using function = std::function<void()>;
    std::thread::id _thread_id;              // 线程ID
    int _event_fd;                           // eventfd唤醒IO事件监控有可能导致的阻塞
    Poller _poller;                          // 进行所有描述符的事件监控
    std::vector<function> _tasks;            // 任务池
    std::mutex _mutex;                       // 实现任务池操作的线程安全
    std::unique_ptr<Channel> _event_channel; // EventLoop被销毁的时候,指针也一起被销毁
    TimerWheel _timer_wheel;                 // 定时器模块
    // 这里要着重说一下std::unique_ptr<Channel> _event_channel;
    // 这个指向的实际上是监控这个EventLoop的_event_fd本身的
    // 因为EventLoop也有自己的文件描述符,也有自己的回调函数,所以也需要Channel来监控

public:
    EventLoop() : _thread_id(std::this_thread::get_id()),
                  _event_fd(CreateEventFd()),
                  _event_channel(new Channel(this, _event_fd)),
                  _timer_wheel(this)
    {
        // 这里为什么要设置读回调呢?让我查查代码......
        // 这里是把std::bind(&EventLoop::ReadEventfd, this)作为参数传进去...
        // 也就是让_read_callback设为std::bind(&EventLoop::ReadEventfd, this)
        // std::bind(&EventLoop::ReadEventfd, this)这个函数等效于ReadEventfd()?
        // 也就是说这是把_read_callback设为ReadEventfd()了
        // 那...别的不需要这样设置吗?
        // mark一下
        // 2025-05-08 : 别的回调函数不在这里设置,会在未来设置,但读回调是固定的
        // 2025-05-12 : 回过头来继续看才发现,读回调才是一定会被执行的那个
        _event_channel->SetReadCallback(std::bind(&EventLoop::ReadEventfd, this));
        _event_channel->EnableRead();
    }
    ~EventLoop() { close(_event_fd); }
    // void quit() { _quit = true; WeakUpEventfd(); }
    void RunAllTasks()
    {
        // if (_tasks.empty()) return; // 错误写法,不加锁的情况下直接访问_task线程不安全
        std::vector<function> tmp;
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            // if (_tasks.empty()) return; // 这才是正确写法,但这里仅仅是先让他能跑就行了,暂不优化过多
            _tasks.swap(tmp);
        }
        for (auto &f : tmp)
        {
            f();
        }
        return;
    }
    void Start()
    {
        while (1)
        {
            std::vector<Channel *> actives;
            _poller.Poll(&actives);
            // 处理事件
            for (auto &f : actives)
            {
                f->HandleEvent();
            }
            RunAllTasks();
        }
    }

    static int CreateEventFd()
    {
        int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK); // eventfd创建一个事件文件描述符
        if (efd < 0)
        {
            ERR_LOG("CREATE EVENTFD FAILED!!");
            abort(); // 让程序异常退出
        }
        return efd;
    }
    // 读eventfd
    void ReadEventfd()
    {
        uint64_t res = 0;
        int ret = read(_event_fd, &res, sizeof(res));
        if (ret < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    // 向_event_fd文件描述符写入数据
    // 其目的是为了唤醒EventLoop对应的线程,使其从epoll_wait状态出来
    // _event_fd是一个特殊的文件描述符,是内核的eventfd的外在表现形式
    // eventfd的创建来源于CreateEventFd()的eventfd()函数
    // 这个函数不仅返回一个int值,也在内核里面创建一个eventfd,外界的_event_fd是外在表现
    // 当eventfd为0的时候就是不可读的,当写入一个数据的时候,其计数器就会+1
    // 非0的时候就是可读的了,所以QueueInLoop()函数里面需要加上这一步唤醒线程
    void WeakUpEventfd()
    {
        uint64_t val = 1;
        int ret = write(_event_fd, &val, sizeof(val));
        // write写入一个八字节的数据,当向_eventfd写入数据的时候,其内部计数器会增加,并且变为可读
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                return;
            }
            ERR_LOG("READ EVENTFD FAILED!");
            abort();
        }
        return;
    }
    bool IsInLoop()
    {
        return (_thread_id == std::this_thread::get_id());
    }
    void AssertInLoop()
    {
        assert(_thread_id == std::this_thread::get_id());
    }
    // 判断将要执行的任务是否处于当前线程中，如果是则执行，不是则压入队列。
    // 重看EVentLoop,RunInLoop是绝对核心,在Connection,TcpServer,TimerWheel里面都有体现
    // 首先连接建立的时候,RunInLoop(或者说是QueueInLoop)把EstablishedInLoop推入任务队列
    // 然后,发送,关闭,释放,非活跃销毁,切换协议,都依靠这个加入EVentLoop的任务队列
    // 这么一看EVentLoop才是这个组件真正的核心,Connection也是围绕着EventLoop进行的
    // 那是否测试函数里面应该先执行的是EVentLoop而不是Connection或者Tcpserver呢?
    void RunInLoop(const function &cb)
    {
        if (IsInLoop())
        {
            return cb();
        }
        return QueueInLoop(cb);
    }
    // 把任务压入任务队列
    void QueueInLoop(const function &cb)
    {
        {
            std::unique_lock<std::mutex> _lock(_mutex);
            _tasks.push_back(cb);
        }
        WeakUpEventfd();
        return;
    }
    void UpdateEvent(Channel *channel) { return _poller.UpdateEvent(channel); }
    // 移除描述符的监控
    void RemoveEvent(Channel *channel) { return _poller.RemoveEvent(channel); }
    void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) { return _timer_wheel.TimerAdd(id, delay, cb); }
    void TimerRefresh(uint64_t id) { return _timer_wheel.TimerRefresh(id); }
    void TimerCancel(uint64_t id) { return _timer_wheel.TimerCancel(id); }
    bool HasTimer(uint64_t id) { return _timer_wheel.HasTimer(id); }
};
void Channel::Remove() { return _loop->RemoveEvent(this); }
void Channel::Update() { return _loop->UpdateEvent(this); }

class Any
{
private:
    class holder
    {
    public:
        virtual ~holder() {}
        virtual const std::type_info &type() = 0;
        virtual holder *clone() = 0;
    };
    template <class T>
    class placeholder : public holder
    {
    public:
        placeholder(const T &val) : _val(val) {}
        // 获取子类对象保存的数据类型
        virtual const std::type_info &type() { return typeid(T); }
        // 针对当前的对象自身，克隆出一个新的子类对象
        virtual holder *clone() { return new placeholder(_val); }

    public:
        T _val;
    };
    holder *_content;

public:
    Any() : _content(NULL) {}
    template <class T>
    Any(const T &val) : _content(new placeholder<T>(val)) {}
    Any(const Any &other) : _content(other._content ? other._content->clone() : NULL) {}
    ~Any() { delete _content; }

    Any &swap(Any &other)
    {
        std::swap(_content, other._content);
        return *this;
    }

    // 返回子类对象保存的数据的指针
    template <class T>
    T *get()
    {
        // 想要获取的数据类型，必须和保存的数据类型一致
        assert(typeid(T) == _content->type());
        return &((placeholder<T> *)_content)->_val;
    }
    // 赋值运算符的重载函数
    template <class T>
    Any &operator=(const T &val)
    {
        // 为val构造一个临时的通用容器，然后与当前容器自身进行指针交换，临时对象释放的时候，原先保存的数据也就被释放
        Any(val).swap(*this);
        return *this;
    }
    Any &operator=(const Any &other)
    {
        Any(other).swap(*this);
        return *this;
    }
};

class Connection;
typedef enum
{
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING
} ConnStatu;
using PtrConnection = std::shared_ptr<Connection>;
class Connection : public std::enable_shared_from_this<Connection>
{
    uint64_t _conn_id;             // Connection的文件描述符
    int _sockfd;                   // 连接关联的文件描述符
    bool _enable_inactive_release; // 是否启动非活跃销毁
    EventLoop *_loop;
    ConnStatu _statu; // Q:这个statu和之前的_enable_inactive_release的区别是什么呢?
    Socket _socket;
    Channel _channel;
    Buffer _in_buffer;
    Buffer _out_buffer;
    Any _context; // 请求的接受处理上下文

    using ConnectCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;

    ConnectCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;
    ClosedCallback _server_closed_callback;

private:
    // 回调函数设置
    void HandleRead()
    {
        // 1. 接收socket的数据，放到缓冲区
        char buf[65536];
        ssize_t ret = _socket.NonBlockRecv(buf, 65535);
        if (ret < 0)
        {
            // 出错了,不能直接关闭连接
            return ShutdownInloop();
        }
        // 这里的等于0表示的是没有读取到数据，而并不是连接断开了，连接断开返回的是-1
        // 将数据放入输入缓冲区,写入之后顺便将写偏移向后移动
        _in_buffer.WriteAndPush(buf, ret);
        // 2. 调用message_callback进行业务处理
        if (_in_buffer.ReadAbleSize() > 0)
        {
            // shared_from_this--从当前对象自身获取自身的shared_ptr管理对象
            return _message_callback(shared_from_this(), &_in_buffer);
        }
    }

    void HandleWrite()
    {
        // out_buffer里面存了要发送的数据
        ssize_t ret = _socket.NonBlockSend(_out_buffer.ReadPosition(), _out_buffer.ReadAbleSize());
        if (ret < 0)
        {
            if (_in_buffer.ReadAbleSize() > 0)
            {
                _message_callback(shared_from_this(), &_in_buffer);
            }
            return Release();
        }
        // 读偏移后移
        _out_buffer.MoveReadOffset(ret);
        if (_out_buffer.ReadAbleSize() == 0)
        {
            _channel.DisableWrite(); // 没有数据待发送了，关闭写事件监控
            // 如果当前是连接待关闭状态，则有数据，发送完数据释放连接，没有数据则直接释放
            if (_statu == DISCONNECTING)
            {
                return Release();
            }
        }
        return;
    }
    // 描述符触发挂断事件
    void HandleClose()
    {
        if (_in_buffer.ReadAbleSize() > 0)
        {
            _message_callback(shared_from_this(), &_in_buffer);
        }
        return Release();
    }
    void HandleError()
    {
        return HandleClose();
    }
    void HandleEvent()
    {
        if (_enable_inactive_release == true)
        {
            _loop->TimerRefresh(_conn_id);
        }
        if (_event_callback)
        {
            _event_callback(shared_from_this());
        }
    }

    // 建立连接
    void EstablishedInLoop()
    {
        // 1. 修改连接状态；  2. 启动读事件监控；  3. 调用回调函数
        assert(_statu == CONNECTING); // 当前的状态必须一定是上层的半连接状态
        _statu = CONNECTED;           // 当前函数执行完毕，则连接进入已完成连接状态
        // 一旦启动读事件监控就有可能会立即触发读事件，如果这时候启动了非活跃连接销毁
        _channel.EnableRead();
        if (_connected_callback)
            _connected_callback(shared_from_this());
    }
    // 释放(释放什么的?)
    void ReleaseInLoop()
    {
        _statu = DISCONNECTED;
        _channel.Remove();
        _socket.Close();
        if (_loop->HasTimer(_conn_id))
        {
            CancelInactiveReleaseInLoop();
        }
        if (_closed_callback)
        {
            _closed_callback(shared_from_this());
        }
        if (_server_closed_callback)
        {
            _server_closed_callback(shared_from_this());
        }
    }
    // 将数据加入缓冲区
    void SendInLoop(Buffer &buf)
    {
        if (_statu == DISCONNECTED)
        {
            return;
        }
        _out_buffer.WriteBufferAndPush(buf);
        if (_channel.WriteAble() == false)
        {
            // 在这里启动写事件监控
            _channel.EnableWrite();
        }
    }

    void ShutdownInloop()
    {
        _statu = DISCONNECTING; // Q:DISCONNECTING和DISCONNECTED的区别是什么呢
        if (_in_buffer.ReadAbleSize() > 0)
        {
            if (_message_callback)
            {
                _message_callback(shared_from_this(), &_in_buffer);
                // Q:这个share_from_this又是什么......
            }
        }
        if (_out_buffer.ReadAbleSize() > 0)
        {
            if (_channel.WriteAble() == false)
            {
                _channel.EnableWrite();
            }
        }
        if (_out_buffer.ReadAbleSize() == 0)
        {
            Release();
        }
    }

    void EnableInactiveReleaseInLoop(int sec)
    {
        // 设置非活跃销毁标志
        _enable_inactive_release = true;
        // 刷新延迟
        if (_loop->HasTimer(_conn_id))
        {
            return _loop->TimerRefresh(_conn_id);
        }
        // 这里面的realese是什么来着???
        _loop->TimerAdd(_conn_id, sec, std::bind(&Connection::Release, this));
    }
    // 在EventLoop线程中取消非活跃连接销毁
    void CancelInactiveReleaseInLoop()
    {
        _enable_inactive_release = false;
        if (_loop->HasTimer(_conn_id))
        {
            return _loop->TimerCancel(_conn_id);
        }
    }
    void UpgradeInLoop(const Any &context,
                       const ConnectCallback &conn,
                       const MessageCallback &msg,
                       const ClosedCallback &closed,
                       const AnyEventCallback &event)
    {
        _context = context;
        _connected_callback = conn;
        _message_callback = msg;
        _closed_callback = closed;
        _event_callback = event;
    }

public:
    Connection(EventLoop *loop, uint64_t conn_id, int sockfd) : _conn_id(conn_id), _sockfd(sockfd), _enable_inactive_release(false),
                                                                _loop(loop), _statu(CONNECTING), _socket(_sockfd), _channel(loop, _sockfd)
    {
        _channel.SetCloseCallback(std::bind(&Connection::HandleClose, this));
        _channel.SetEventCallback(std::bind(&Connection::HandleEvent, this));
        _channel.SetReadCallback(std::bind(&Connection::HandleRead, this));
        _channel.SetWriteCallback(std::bind(&Connection::HandleWrite, this));
        _channel.SetErrorCallback(std::bind(&Connection::HandleError, this));
    }
    ~Connection() { DBG_LOG("RELEASE CONNECTION:%p", this); }
    int fd() { return _sockfd; }
    int id() { return _conn_id; }
    bool Connected() { return (_statu == CONNECTED); }
    void SetContext(const Any &context) { _context = context; }
    Any* GetContext() { return &_context; }
    void SetConnectedCallback(const ConnectCallback &cb) { _connected_callback = cb; }
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    void SetAnyEventCallback(const AnyEventCallback &cb) { _event_callback = cb; }
    void SetSrvClosedCallback(const ClosedCallback &cb) { _server_closed_callback = cb; }

    // 连接建立就绪后，进行channel回调设置，启动读监控，调用_connected_callback
    // void Established()
    // {
    //     _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, this));
    // }
    void Established()
    {
        _loop->RunInLoop(std::bind(&Connection::EstablishedInLoop, shared_from_this())); // 注意这里用 shared_from_this()
    }
    // 发送数据,把数据放到发送缓冲区
    void Send(const char *data, ssize_t len)
    {
        // Q:这里是怎么启动写事件监控的呢......
        Buffer buf;
        buf.WriteAndPush(data, len);
        _loop->RunInLoop(std::bind(&Connection::SendInLoop, this, std::move(buf)));
    }
    // 关闭?
    void Shutdown()
    {
        _loop->RunInLoop(std::bind(&Connection::ShutdownInloop, this));
    }
    void Release()
    {
        _loop->QueueInLoop(std::bind(&Connection::ReleaseInLoop, this));
    }
    void EnableInactiveRelease(int sec)
    {
        _loop->RunInLoop(std::bind(&Connection::EnableInactiveReleaseInLoop, this, sec));
    }
    // 取消非活跃销毁
    void CancelInactiveRelease()
    {
        _loop->RunInLoop(std::bind(&Connection::CancelInactiveReleaseInLoop, this));
    }
    // 切换协议---重置上下文以及阶段性回调处理函数 -- 而是这个接口必须在EventLoop线程中立即执行
    // 防备新的事件触发后，处理的时候，切换任务还没有被执行--会导致数据使用原协议处理了。
    void Upgrade(const Any &context, const ConnectCallback &conn, const MessageCallback &msg,
                 const ClosedCallback &closed, const AnyEventCallback &event)
    {
        _loop->AssertInLoop();
        _loop->RunInLoop(std::bind(&Connection::UpgradeInLoop, this, context, conn, msg, closed, event));
    }
};
class Acceptor
{
private:
    Socket _socket;
    EventLoop *_loop; // 监控事件
    Channel _channel; // 事件管理
    using AcceptCallback = std::function<void(int)>;
    AcceptCallback _accept_callback;
    void HandleRead()
    {
        int newfd = _socket.Accept();
        if (newfd < 0)
        {
            return;
        }
        if (_accept_callback)
            _accept_callback(newfd);
    }

    int CreateServer(int port)
    {
        bool ret = _socket.CreateServer(port);
        assert(ret == true);
        return _socket.Fd();
    }

public:
    Acceptor(EventLoop *loop, int port) : _socket(CreateServer(port)),
                                          _loop(loop), _channel(loop, _socket.Fd())
    {
        _channel.SetReadCallback(std::bind(&Acceptor::HandleRead, this));
    }
    void SetAcceptCallback(const AcceptCallback &cb)
    {
        _accept_callback = cb;
    }
    void Listen()
    {
        _channel.EnableRead();
    }
};

class LoopThread
{
private:
    // 用于实现_loop获取的同步关系,避免线程创建了,但_loop还没有实例化之前去获取_loop
    std::mutex _mutex;
    std::condition_variable _cond; // 条件变量
    EventLoop *_loop;
    std::thread _thread; // 对应的线程
    // 实例化EventLoop,开始运行EventLoop的功能
    void ThreadEntry()
    {
        EventLoop loop;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _loop = &loop;
            _cond.notify_all(); // 通知主线程EventLoop已经创建
        }
        loop.Start();
    }

public:
    // 创建线程,设定线程入口函数
    LoopThread() : _loop(NULL), _thread(std::thread(&LoopThread::ThreadEntry, this)) {}
    EventLoop *GetLoop()
    {
        EventLoop *loop = NULL;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _cond.wait(lock, [&]()
                       { return _loop != NULL; });
            loop = _loop;
        }
        return loop;
    }
};
class LoopThreadPool
{
    int _thread_count;
    int _next_idx;
    EventLoop *_baseloop;
    std::vector<LoopThread *> _threads;
    std::vector<EventLoop *> _loops;

public:
    LoopThreadPool(EventLoop *baseloop) : _thread_count(0), _next_idx(0), _baseloop(baseloop) {}
    void SetThreadCount(int count)
    {
        _thread_count = count;
    }
    void Create()
    {
        if (_thread_count > 0)
        {
            _threads.resize(_thread_count);
            _loops.resize(_thread_count);
            for (int i = 0; i < _thread_count; ++i)
            {
                // 这里已经通过Echoserver的set函数设置好了线程数量
                // 用两个数组是为了方便快速地访问和分配EventLoop
                // new LoopThread的时候会执行构造函数里面的ThreadEntry
                // 这就意味着会分配对应的EventLoop给线程,并且在最后执行EventLoop::Start()
                // 从属EventLoop得以开始循环
                _threads[i] = new LoopThread();
                _loops[i] = _threads[i]->GetLoop();
            }
        }
    }
    EventLoop *NextLoop()
    {
        if (_thread_count == 0)
        {
            return _baseloop;
        }
        _next_idx = (_next_idx + 1) % _thread_count;
        return _loops[_next_idx];
    }
};

class TcpServer
{
private:
    uint64_t _next_id; // 这是一个自动增长的连接ID，
    int _port;
    int _timeout;                                       // 这是非活跃连接的统计时间---多长时间无通信就是非活跃连接
    bool _enable_inactive_release;                      // 是否启动了非活跃连接超时销毁的判断标志
    EventLoop _baseloop;                                // 这是主线程的EventLoop对象，负责监听事件的处理
    Acceptor _acceptor;                                 // 这是监听套接字的管理对象
    LoopThreadPool _pool;                               // 这是从属EventLoop线程池
    std::unordered_map<uint64_t, PtrConnection> _conns; // 保存管理所有连接对应的shared_ptr对象

    using ConnectedCallback = std::function<void(const PtrConnection &)>;
    using MessageCallback = std::function<void(const PtrConnection &, Buffer *)>;
    using ClosedCallback = std::function<void(const PtrConnection &)>;
    using AnyEventCallback = std::function<void(const PtrConnection &)>;
    using Functor = std::function<void()>;
    ConnectedCallback _connected_callback;
    MessageCallback _message_callback;
    ClosedCallback _closed_callback;
    AnyEventCallback _event_callback;

private:
    void RunAfterInLoop(const Functor &task, int delay)
    {
        _next_id++;
        _baseloop.TimerAdd(_next_id, delay, task);
    }
    // 为新连接构造一个Connection进行管理
    void NewConnection(int fd)
    {
        _next_id++;
        PtrConnection conn(new Connection(_pool.NextLoop(), _next_id, fd));
        conn->SetMessageCallback(_message_callback);
        conn->SetClosedCallback(_closed_callback);
        conn->SetConnectedCallback(_connected_callback);
        conn->SetAnyEventCallback(_event_callback);
        conn->SetSrvClosedCallback(std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
        if (_enable_inactive_release)
            conn->EnableInactiveRelease(_timeout); // 启动非活跃超时销毁
        conn->Established();                       // 就绪初始化
        _conns.insert(std::make_pair(_next_id, conn));
    }
    void RemoveConnectionInLoop(const PtrConnection &conn)
    {
        int id = conn->id();
        auto it = _conns.find(id);
        if (it != _conns.end())
        {
            _conns.erase(it);
        }
    }
    // 从管理Connection的_conns中移除连接信息
    void RemoveConnection(const PtrConnection &conn)
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RemoveConnectionInLoop, this, conn));
    }

public:
    TcpServer(int port) : _port(port),
                          _next_id(0),
                          _enable_inactive_release(false),
                          _acceptor(&_baseloop, port),
                          _pool(&_baseloop)
    {
        _acceptor.SetAcceptCallback(std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
        _acceptor.Listen(); // 将监听套接字挂到baseloop上
    }
    // 这五个函数都会在echoserver开始的时候作为构造函数的一部分自动执行的,直接影响了
    // 设置从属线程池的线程数量
    void SetThreadCount(int count) { return _pool.SetThreadCount(count); }
    // 设置连接建立回调函数
    void SetConnectedCallback(const ConnectedCallback &cb) { _connected_callback = cb; }
    // 设置接受信息的回调函数
    void SetMessageCallback(const MessageCallback &cb) { _message_callback = cb; }
    // 设置连接关闭的回调函数
    void SetClosedCallback(const ClosedCallback &cb) { _closed_callback = cb; }
    // 设置任意事件的回调函数
    void SetAnyEventCallback(const AnyEventCallback &cb) { _event_callback = cb; }
    // 启动非活跃连接释放功能,并且设置超时时间
    void EnableInactiveRelease(int timeout)
    {
        _timeout = timeout;
        _enable_inactive_release = true;
    }
    // 用于添加一个定时任务
    void RunAfter(const Functor &task, int delay)
    {
        _baseloop.RunInLoop(std::bind(&TcpServer::RunAfterInLoop, this, task, delay));
    }
    // 整个代码的运行,要从这里开始...
    // 一个LoopThreadPool和EventLoop的运行开始
    void Start()
    {
        _pool.Create();
        // 这个是主EventLoop的开始运行
        _baseloop.Start();
    }
};

void TimerWheel::TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerAddInLoop, this, id, delay, cb));
}
// 刷新/延迟定时任务
void TimerWheel::TimerRefresh(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerRefreshInLoop, this, id));
}
void TimerWheel::TimerCancel(uint64_t id)
{
    _loop->RunInLoop(std::bind(&TimerWheel::TimerCancelInLoop, this, id));
}

class NetWork
{
public:
    NetWork()
    {
        DBG_LOG("SIGPIPE INIT");
        signal(SIGPIPE, SIG_IGN);
    }
};
static NetWork nw;