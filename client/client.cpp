
//客户端：提供响应请求的机器
#pragma once
#include "TCP_Client.h"
#include "SocketRAII.h"
#include "KeyBoard.h"
#include "MOUSE_TYPE.h"
#include <atlimage.h>
#pragma comment(lib, "ws2_32.lib")

#define _CRT_SECURE_NO_WARNINGS

constexpr uint32_t RECV_BUFFER_LEN = 1024 * 1024 * 3;
HWND g_hwnd = NULL;
CImage g_image;
int g_remote_width = -1;
int g_remote_height = -1;
CRITICAL_SECTION g_cri_sec;
SOCKET g_sock;
static ULONGLONG g_mouse_tick = GetTickCount64();
//客户端默认只有一条流水线用来处理UI
//如果使用recv，是阻塞的，会卡死
//需要重新开一条流水线

//1.客服端窗口程序
//链接器的入口点改为WinMainCRTStartup
//链接器所有选项中的子系统改为窗口
//main是控制台主函数

int InitWindow(HINSTANCE, int);
DWORD WINAPI SendScreenCallBack(LPVOID );
void MouseHandler(MOUSE_TYPE, HWND, WPARAM, LPARAM);
void SendKey(int, bool);

LRESULT CALLBACK winProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		//绘制UI
		//拿到image
		if (!g_image.IsNull()) {
			//做一个缩放，因为客户端和服务端窗口大小不一样
			RECT rect;
			GetClientRect(hwnd, &rect);
			int client_width = rect.right - rect.left;
			int client_height = rect.bottom - rect.top;

			//远程图片的宽和高

			//设置拉升模式(HALFTONE：高清)
			int oldMode = SetStretchBltMode(hdc, HALFTONE);
			//设置画刷，刷远点，选择高清模式必须设置
			SetBrushOrgEx(hdc, 0, 0, NULL);
			EnterCriticalSection(&g_cri_sec);
			int remote_width = g_image.GetWidth();
			int remote_height = g_image.GetHeight();
			g_image.StretchBlt(hdc, 0, 0, client_width, client_height, 0, 0, remote_width, remote_height, SRCCOPY);
			LeaveCriticalSection(&g_cri_sec);
			//恢复默认的拉伸模式
			SetStretchBltMode(hdc, oldMode);
		}
		EndPaint(hwnd, &ps);
		break;
	}
	case WM_MOUSEMOVE: {
		MouseHandler(MOUSE_MOVE, hwnd, wParam, lParam);
		break;
	}
	case WM_LBUTTONDOWN: {
		MouseHandler(MOUSE_LDOWN, hwnd, wParam, lParam);
		break;
	}
	case WM_LBUTTONUP: {
		MouseHandler(MOUSE_LUP, hwnd, wParam, lParam);
		break;
	}
	case WM_RBUTTONDOWN: {
		MouseHandler(MOUSE_RDOWN, hwnd, wParam, lParam);
		break;
	}
	case WM_RBUTTONUP: {
		MouseHandler(MOUSE_RUP, hwnd, wParam, lParam);
		break;
	}
	case WM_MBUTTONDOWN: {
		MouseHandler(MOUSE_MDOWN, hwnd, wParam, lParam);
		break;
	}
	//  键盘
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		SendKey((int)wParam, true);
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP: {
		SendKey((int)wParam, false);
		break;
	}
	case WM_DESTROY: {
		auto pkt = pack_packet((uint32_t)CMD_TYPE::CMD_DISCONNECT, {});
		send(g_sock, (char*)pkt.data(), pkt.size(), 0);
		PostQuitMessage(0);
		break;
	}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
		break;
	}
	return 0;
}


//窗口主函数
//1.创建窗口的入口函数
int WINAPI WinMain(
	HINSTANCE hInstance,	//当前实例句柄
	HINSTANCE hPreventInstance, //前一个实例句柄，一般为空(NULL)
	PSTR pCmdLine,	//命令行参数
	int nCmdShow //窗口显示方式
) 
{	
	AllocConsole();  // 创建控制台

	FILE* fp;

	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONIN$", "r", stdin);

	//初始化关键代码段
	InitializeCriticalSection(&g_cri_sec);

	InitWindow(hInstance, nCmdShow);
	// ===== 初始化网络 =====
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	g_sock = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8765);
	std::string server_ip;
	std::cout << "请输入server的ip:";
	std::cin >> server_ip;
	addr.sin_addr.S_un.S_addr = inet_addr(server_ip.c_str());

	if (connect(g_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cout << "connected to server failed\n";
		return 0;
	}
	//连接服务器
	unsigned long send_screen_thread_id = 0;
	//开辟了一条流水线
	HANDLE handle_send_screen = CreateThread(NULL, 0, SendScreenCallBack, NULL, 0, &send_screen_thread_id);
	
	//创新消息循环队列
	MSG msg;
	//while就不断绘制界面
	while (GetMessageW(&msg, NULL, 0, 0)) {
		//翻译消息
		TranslateMessage(&msg);
		//分发消息
		//回调函数WinProc收到消息
		DispatchMessageW(&msg);
	}

}	

//返回值 调用约定 函数名(参数列表){函数体}
DWORD WINAPI SendScreenCallBack(LPVOID lpThreadParam) {
	std::vector<uint8_t> recv_buffer(RECV_BUFFER_LEN);
	//不停发送和解析数据
	TCP_Client client;
	client.init_handlers();
	
	size_t data_len = 0;
	while (true) {
		Sleep(25);
		auto packet = pack_packet(static_cast<int>(CMD_TYPE::CMD_SCREEN), {});
		int send_len = send(g_sock,
			reinterpret_cast<const char*>(packet.data()), 
			packet.size(), 
			0);

		int len = recv(g_sock,
			(char*)recv_buffer.data() + data_len,
			recv_buffer.size() - data_len,
			0);
		if (len <= 100) {
			continue;
		}
		data_len += len;
		// ⭐ 用offset解析（无拷贝）
		size_t offset = 0;

		while (true) {
			auto pkt_opt = try_parse_packet(recv_buffer, offset, data_len);
			if (!pkt_opt) {
				break;
			}
			client.handle_packet(*pkt_opt, g_sock);
		}

		// ⭐ 移动剩余数据（关键）
		if (offset > 0) {
			memmove(recv_buffer.data(),
				recv_buffer.data() + offset,
				data_len - offset);

			data_len -= offset;
		}
	}
}

int InitWindow(HINSTANCE hInstance, int nCmdShow) {
	//1.注册一个窗口类
	WNDCLASS ws = {};
	LPCSTR CLASS_NAME = "MainWindow";
	ws.lpfnWndProc = winProc;	//窗口消息的处理函数
	ws.hInstance = hInstance;	//实例句柄
	ws.lpszClassName = CLASS_NAME;
	ws.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	ws.hCursor = LoadCursor(NULL, IDC_ARROW);	//光标
	ws.hIcon = LoadIconA(NULL, IDI_APPLICATION);	//图标
	ws.style = CS_HREDRAW | CS_VREDRAW; //窗口大小发送变化时会重绘窗口
	if (!RegisterClass(&ws)) {
		MessageBox(NULL, "窗口注册失败", "错误", MB_OK | MB_ICONERROR);
		return 0;
	}
	//2.创建窗口
	//#define CreateWindowA(lpClassName, lpWindowName, dwStyle, x, y,\
	//	nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)\
	//	CreateWindowExA(0L, lpClassName, lpWindowName, dwStyle, x, y,\
	//	nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam)

	g_hwnd = CreateWindow(
		CLASS_NAME, //窗口类名
		"远程控制",	//窗口标题
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,	//窗口x位置
		CW_USEDEFAULT,	//窗口y位置
		600, 400,		//窗口宽和高
		NULL,		//父窗口的句柄
		NULL,		//菜单句柄
		hInstance,	//实例句柄
		NULL		//附加参数
	);	//窗口样式
	if (g_hwnd == NULL) {
		//创建失败弹出一个选择框
		MessageBox(NULL, "窗口创建失败", "错误", MB_OK | MB_ICONERROR);
		return 0;
	}
	//3.显示窗口
	ShowWindow(g_hwnd, nCmdShow);
	//4.更新窗口
	UpdateWindow(g_hwnd);
}


void MouseHandler(MOUSE_TYPE type, HWND hwnd, WPARAM wParam, LPARAM lParam) {
	//if (GetTickCount() - g_mouse_tick < 30) {
	//	return;
	//}
	//鼠标移动
	//WPARAM wParam, LPARAM lParam
	//拿到鼠标位置，位于客户区
	int xPos = LOWORD(lParam);//低二字节是x
	int yPos = HIWORD(lParam);//高二字节是y
	RECT client_rect;
	GetClientRect(hwnd, &client_rect);
	int client_width = client_rect.right - client_rect.left;
	int client_height = client_rect.bottom - client_rect.top;
	if (g_remote_height != -1 && g_remote_width != -1) {
		int rxPos = xPos * g_remote_width / client_width;
		int ryPos = yPos * g_remote_height / client_height;
		//发送数据
		Mouse mouse{};
		mouse.action = type;
		mouse.ptXY.x = rxPos;
		mouse.ptXY.y = ryPos;
		std::vector<uint8_t> mouseData(sizeof(Mouse));
		memcpy(mouseData.data(), &mouse, sizeof(Mouse));
		auto packet = pack_packet(static_cast<int>(CMD_TYPE::CMD_MOUSE), mouseData);
		send(g_sock, (char*)packet.data(), packet.size(), 0);
		g_mouse_tick = GetTickCount64();
	}
}

void SendKey(int vk, bool is_down) {
	if (vk == VK_LWIN || vk == VK_RWIN) return;
	KeyBoard key{};
	key.virtual_code = vk;
	key.key_status = is_down ? 0 : KEYEVENTF_KEYUP;

	std::vector<uint8_t> body(sizeof(KeyBoard));
	memcpy(body.data(), &key, sizeof(KeyBoard));

	auto pkt = pack_packet((uint32_t)CMD_TYPE::CMD_KEYBOARD, body);
	send(g_sock, (char*)pkt.data(), pkt.size(), 0);
}
//int main() {
//	TCP_Client client;
//	client.run();
//	
//}


