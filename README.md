
# 高并发C++ HTTP服务器 (仿Muduo)

这是一个基于C++11/14实现的高并发HTTP服务器，其设计思想借鉴了著名的Muduo网络库。它采用Reactor模式，利用Epoll进行I/O多路复用，并结合线程池来处理客户端连接和请求，实现了高效的事件驱动模型。

## 主要特性

*   **事件驱动架构**：基于Reactor模式，主Reactor负责监听和接受连接，子Reactors（工作线程）负责处理已连接套接字的I/O事件。
*   **I/O多路复用**：使用Linux下的`epoll`机制，高效管理大量并发连接。
*   **线程池**：使用`EventLoopThreadPool`管理一组工作线程，每个工作线程拥有自己的`EventLoop`，实现"One loop per thread"模型，避免了线程间共享数据的复杂性。
*   **自定义缓冲区**：`Buffer`类用于高效地处理网络数据的读写，支持自动扩容和头部空间复用。
*   **HTTP协议处理**：
    *   解析HTTP请求行、请求头和请求体 (`HttpRequest`, `HttpContext`)。
    *   构建HTTP响应 (`HttpResponse`)。
    *   支持常见的HTTP方法 (GET, POST, PUT, DELETE)。
    *   基于正则表达式的路由分发机制。
*   **静态文件服务**：能够提供指定目录下的静态文件访问（如HTML, CSS, JS, 图片等）。
*   **连接管理**：
    *   `Connection`类封装TCP连接，管理其生命周期。
    *   支持非活跃连接超时关闭（基于时间轮`TimerWheel`）。
*   **模块化设计**：
    *   `Socket`类封装套接字操作。
    *   `Channel`类封装文件描述符及其关注的事件和回调。
    *   `Poller`类封装`epoll`的底层操作。
    *   `Acceptor`类专门用于接受新连接。
    *   `TimerWheel`实现高效的定时器功能。
*   **日志系统**：简单的宏定义日志系统，区分不同日志级别。
*   **工具类**：`Util`类提供字符串分割、文件读写、URL编解码、MIME类型识别等常用功能。
*   **通用数据容器**：`Any`类用于在不同组件间传递任意类型的数据（例如在`Connection`中存储`HttpContext`）。

## 项目结构

```
.
├── http.hpp        # HTTP协议处理相关类 (HttpRequest, HttpResponse, HttpContext, HttpServer)
├── main            # 编译后的可执行文件
├── main.cc         # 服务器主程序入口，示例路由配置
├── Makefile        # 项目构建文件
├── server.hpp      # 网络库核心组件 (Buffer, Socket, Channel, Poller, EventLoop, Connection, Acceptor, TimerWheel, TcpServer等)
└── wwwroot         # 静态资源存放目录
    └── index.html  # 示例静态页面
```

## 依赖与环境

*   C++ 编译器 (支持 C++11/14 标准，如 g++)
*   Make 构建工具
*   Linux 操作系统 (使用了 epoll, eventfd, timerfd 等Linux特有API)

## 构建项目

在项目根目录下执行：

```bash
make
```

这将编译源代码并生成可执行文件 `main`。

## 运行服务器

编译成功后，运行：

```bash
./main
```

服务器默认将在 `8500` 端口启动，并使用 `3` 个工作线程（在 `main.cc` 中配置）。
静态资源根目录设置为 `./wwwroot/`。

你可以通过浏览器或curl等工具访问：

*   `http://localhost:8500/` (应显示 `wwwroot/index.html` 内容)
*   `http://localhost:8500/hello` (GET请求，将由 `Hello` 函数处理)
*   `curl -X POST http://localhost:8500/login` (POST请求，将由 `Login` 函数处理)
*   `curl -X PUT --data "some content" http://localhost:8500/1234.txt` (PUT请求，将创建或覆盖 `wwwroot/1234.txt`)
*   `curl -X DELETE http://localhost:8500/1234.txt` (DELETE请求，将由 `DelFile` 函数处理)

## 核心组件简介

*   **`EventLoop`**: 事件循环的核心，每个线程最多拥有一个。负责驱动`Poller`进行事件监听，并分发事件到对应的`Channel`。同时处理任务队列和定时器事件。
*   **`Channel`**: 代表一个文件描述符（如socket fd, eventfd, timerfd）及其关注的事件（读、写等）和事件发生时的回调函数。
*   **`Poller`**: `epoll`的封装，负责向内核注册/修改/删除`Channel`关注的事件，并等待事件发生。
*   **`Buffer`**: 应用层缓冲区，用于网络数据的收发，提供方便的读写接口。
*   **`Socket`**: 对底层socket API的简单封装。
*   **`Acceptor`**: 用于服务端，封装了监听socket，当有新连接到来时，通过回调通知上层。
*   **`Connection`**: 代表一个TCP连接，封装了连接的socket、输入输出缓冲区、连接状态以及各种事件回调。生命周期由`shared_ptr`管理。
*   **`TcpServer`**: TCP服务器的封装，管理`Acceptor`和所有`Connection`。负责将新连接分发给线程池中的某个`EventLoop`。
*   **`LoopThreadPool`**: `EventLoop`线程池，用于创建和管理一组工作线程及其对应的`EventLoop`。
*   **`TimerWheel`**: 基于时间轮算法的定时器，用于高效管理大量定时事件，如连接超时。
*   **`HttpRequest`, `HttpResponse`, `HttpContext`**: HTTP协议相关的类，分别用于表示HTTP请求、HTTP响应以及在处理HTTP请求过程中的上下文信息。
*   **`HttpServer`**: 基于`TcpServer`构建的HTTP服务器，负责HTTP请求的解析、路由和响应。

## 待办事项/未来改进

*   [ ] 更完善的HTTP/1.1特性支持 (如 Keep-Alive 超时细化控制, Chunked transfer encoding, Pipelining)
*   [ ] HTTPS 支持 (集成OpenSSL)
*   [ ] WebSocket 支持
*   [ ] 更健壮的错误处理和异常捕获
*   [ ] 单元测试和集成测试
*   [ ] 性能基准测试与调优
*   [ ] 更详细和灵活的日志配置

## 致谢

本项目的网络库部分设计思想深受 [Muduo](https://github.com/chenshuo/muduo) 网络库的启发。

## 许可证

本项目使用 MIT 许可证。详情请见 [LICENSE](LICENSE) 文件。
