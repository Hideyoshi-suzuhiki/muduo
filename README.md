
# C++ Network Library

这是一个基于 C++ 实现的简单网络库，旨在提供一个基础的 TCP 服务器框架。该库利用 epoll 进行 I/O 多路复用，并包含了一个简单的定时器轮机制用于处理非活跃连接。

## 项目结构

项目的目录结构如下：

```
.
├── echo.hpp
├── main.cc
├── Makefile
└── server.hpp
```

*   `echo.hpp`: 包含一个简单的 EchoServer 示例，演示了如何使用该网络库。
*   `main`: 编译生成的可执行文件。
*   `main.cc`: 项目的入口文件，实例化并启动 EchoServer。
*   `Makefile`: 用于编译项目的 Makefile。
*   `server.hpp`: 包含网络库的核心组件，如 Buffer, Socket, Channel, Poller, TimerTask, TimerWheel, EventLoop, LoopThread, LoopThreadPool 和 TcpServer。

## 构建和运行

### 依赖

*   C++11 或更高版本的编译器 (如 g++)
*   Linux 环境 (由于使用了 epoll)

### 构建

在项目根目录下执行以下命令进行编译：

```bash
make
```

这将生成一个名为 `main` 的可执行文件。

### 运行

执行以下命令启动 EchoServer：

```bash
./main
```

默认情况下，EchoServer 将监听 8080 端口。你可以使用 `telnet` 或其他客户端工具连接到服务器进行测试。例如：

```bash
telnet 127.0.0.1 8080
```

在 telnet 客户端中输入任何文本，服务器会将相同的文本回显给你。

### 清理

执行以下命令清理编译生成的文件：

```bash
make clean
```

## 代码说明

### 核心组件

*   **Buffer**: 实现了一个动态大小的缓冲区，用于存储网络数据的读写。
*   **Socket**: 封装了套接字相关的系统调用，提供了创建、绑定、监听、连接、发送和接收等功能。
*   **Channel**: 代表一个文件描述符及其关注的事件和事件发生时的回调函数。它是 EventLoop 和具体 I/O 操作之间的桥梁。
*   **Poller**: 封装了 Linux epoll I/O 多路复用机制，负责监控多个 Channel 的事件，并在事件发生时通知它们。
*   **TimerTask / TimerWheel**: 实现了一个简单的定时器轮机制，用于管理定时任务，例如非活跃连接的超时销毁。
*   **EventLoop**: 事件循环的核心，通常每个线程拥有一个 EventLoop 对象。它负责运行事件循环、管理 Channel、处理 I/O 事件和定时器事件，以及执行跨线程提交的任务。
*   **LoopThread / LoopThreadPool**: 用于创建和管理 EventLoop 线程池，实现多线程处理网络连接。
*   **TcpServer**: 提供了 TCP 服务器的基本框架，负责接受新连接、管理连接以及分发事件回调。

### EchoServer 示例

`EchoServer` 是一个简单的示例应用，演示了如何使用 `TcpServer` 构建一个回显服务器。它设置了连接建立、消息接收和连接关闭的回调函数。

*   `OnConnected`: 在新连接建立时调用。
*   `OnClosed`: 在连接关闭时调用。
*   `OnMessage`: 在接收到客户端消息时调用，并将接收到的数据回显给客户端，然后关闭连接。

## 特点

*   基于 epoll 的高性能 I/O 多路复用。
*   多线程支持，通过 EventLoop 线程池处理并发连接。
*   简单的定时器轮机制，支持非活跃连接超时销毁。
*   模块化的设计，易于扩展和修改。

## 待办事项和潜在改进

*   增加对 UDP 的支持。
*   实现更完善的错误处理和日志记录。
*   增加对 TLS/SSL 的支持。
*   实现更高级的协议解析和处理机制。
*   进行性能测试和优化。
*   编写单元测试。

## 贡献

欢迎对本项目做出贡献。如果你发现任何 bug 或有改进建议，请提交 issue 或 pull request。

## 许可证

本项目采用 [MIT 许可证](LICENSE) 开源。

