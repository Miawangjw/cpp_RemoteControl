# cpp_RemoteControl（远程控制系统）

## 项目介绍
基于 C++ 和 Win32 API 实现的远程桌面控制系统，支持屏幕传输、鼠标控制和键盘控制。

## 技术点
- TCP 自定义协议（解决粘包问题）
- GDI 屏幕采集 + JPEG 压缩
- 鼠标/键盘远程控制（SendInput）
- 采用双缓冲区设计（避免 memmove）
- 低延迟流式传输（丢帧优化）

## 项目结构
- client/：客户端
- server/：服务端

## 运行方式
1. 运行 server
2. 运行 client
3. 输入服务器IP连接
4. 可能需要服务端执行`netsh advfirewall firewall add rule name="Open Port 8765" dir=in action=allow protocol=TCP localport=8765`
