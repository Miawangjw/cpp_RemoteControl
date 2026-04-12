#pragma once
#include "TCP_Server.h"
#pragma comment(lib, "ws2_32.lib")
//初始化socket环境
//客户端编程步骤：创建Socket连接	
// //连接服务器
int main() {
	TCP_Server server;
	server.run();
}

