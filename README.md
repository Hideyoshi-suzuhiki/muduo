# C++ 事件驱动网络库

这是一个使用 C++ (主要基于 C++11 及以上特性) 编写的、基于事件驱动（Reactor 模式）的网络编程库。它旨在提供一个简洁、易用且具备一定性能基础的 TCP 服务端开发框架。

## 主要特性

*   **事件驱动**: 基于 Epoll (Linux) 实现 I/O 多路复用，高效处理并发连接。
*   **线程模型**: 支持 "one loop per thread" 的多线程模型，主线程 (BaseLoop) 负责接受新连接，然后将连接分发给子线程 (SubLoop) 的 `EventLoop` 进行处理，充分利用多核 CPU。
*   **TCP 服务封装**:
    *   `TcpServer`: 高层 TCP 服务器封装，简化服务器搭建。
    *   `Connection`: TCP 连接的抽象，管理连接的生命周期和数据收发。
    *   `Acceptor`: 用于接受新的 TCP 连接。
*   **缓冲区管理**:
    *   `Buffer`: 内置可自动增长的环形缓冲区，方便网络数据的读写和管理。
*   **定时器**:
    *   `TimerWheel`: 基于时间轮实现的定时器，可用于处理连接超时、心跳检测等周期性或延时任务。
*   **异步任务队列**:
    *   `EventLoop` 内置任务队列，支持将任务从其他线程安全地提交到 `EventLoop` 所在线程执行。
*   **日志系统**:
    *   提供简单的宏定义日志工具，方便调试和追踪。

## 核心组件

*   `Socket`: 对底层 Socket API 的基本封装（创建、绑定、监听、连接、读写、关闭等）。
*   `Channel`: 代表一个文件描述符（如 Socket fd）及其关注的事件和事件发生时的回调函数。它是 `EventLoop` 和具体 I/O 操作之间的桥梁。
*   `Poller`: `epoll` 的封装，负责监控多个 `Channel` 的事件。
*   `EventLoop`: 事件循环的核心，驱动整个事件处理流程。每个线程通常拥有一个 `EventLoop` 对象。
*   `Buffer`: 高效的读写缓冲区。
*   `Connection`: TCP 连接的抽象，封装了连接状态、输入输出缓冲区以及相关的回调。
*   `Acceptor`: 负责监听端口，接受新连接，并将新连接的 `fd` 交给 `TcpServer` 处理。
*   `TcpServer`: TCP 服务器的高级封装，整合了 `Acceptor`、`EventLoop` 和 `Connection` 管理。
*   `TimerTask` & `TimerWheel`: 定时器任务和时间轮实现。
*   `LoopThread` & `LoopThreadPool`: 用于创建和管理 "one loop per thread" 的线程池。
*   `Any`: 一个简单的类型安全容器，用于在 `Connection` 中存储用户自定义上下文数据。

## 如何编译与运行

1.  **环境要求**:
    *   Linux 操作系统 (因为使用了 `epoll`, `eventfd`, `timerfd`)
    *   支持 C++11 或更高版本的 C++ 编译器 (如 G++)
    *   需要链接 `pthread` 库。

2.  **编译**:
    由于这是一个库性质的代码，你可以将其源文件加入到你的项目中进行编译。一个简单的编译命令示例（假设所有 `.cpp` 文件都在当前目录，并且你有一个 `main.cpp` 来使用这个库）：

    ```bash
    g++ -o my_server main.cpp *.cpp -std=c++17 -pthread -Wall -g
    ```
    *   `-std=c++17`: 根据代码中使用的特性（如 `std::bind` 占位符等），C++11/14/17 应该都可以。
    *   `-pthread`: 链接 POSIX 线程库。
    *   `-Wall`: 开启所有警告。
    *   `-g`: 生成调试信息。
    *(请根据你的实际文件组织和需求调整编译命令)*

3.  **运行**:
    编译成功后，会生成一个可执行文件 (例如 `my_server`)。
    ```bash
    ./my_server
    ```

## 使用示例

你需要创建一个 `main` 函数来实例化并启动 `TcpServer`。以下是一个非常基础的示例框架：

```cpp
#include "server.hpp"
#include <iostream>

void onConnection(const PtrConnection &conn)
{
    if (conn->Connected())
    {
        std::cout << "New connection: " << conn->id() << " from " << "some_ip_port_info_here" << std::endl;
    }
    else
    {
        std::cout << "Connection closed: " << conn->id() << std::endl;
    }
}

void onMessage(const PtrConnection &conn, Buffer *buf)
{
    std::string msg = buf->ReadAsStringAndPop(buf->ReadAbleSize());
    std::cout << "Received from " << conn->id() << ": " << msg << std::endl;
    conn->Send(msg.c_str(), msg.length());
}

int main()
{
    int port = 8080;
    TcpServer server(port);

    server.SetThreadCount(4); // 设置工作线程数量
    server.SetConnectedCallback(onConnection);
    server.SetMessageCallback(onMessage);
    // server.SetClosedCallback(...); // 如果需要单独处理关闭回调
    // server.EnableInactiveRelease(60); // 60秒无通信则断开
    std::cout << "Server starting on port " << port << std::endl;
    server.Start(); // 启动服务器的事件循环
    return 0;
}
