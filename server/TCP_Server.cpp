#pragma once
#include "TCP_Server.h"
#include "SocketRAII.h"
#include <functional>
#include <vector>
#include "Packet.h"
#include <unordered_map>
#include <Windows.h>
#include <iostream>
#include <optional>
#include <string>
#include <atlimage.h>
#include <ShellScalingApi.h>
#include "MOUSE_TYPE.h"
#include "KeyBoard.h"
#pragma comment(lib, "ws2_32.lib")
void release_all_input() {
    // 鼠标左键
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, 0);

    // 常见键盘
    for (int vk = 0; vk < 256; vk++) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vk;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}
constexpr uint32_t MAGIC = 0x55AA77CC;
std::optional<Packet> try_parse_packet(std::vector<uint8_t>& buffer) {
    if (buffer.size() < sizeof(PacketHeader)) {
        return std::nullopt;
    }
    // 查找 magic（同步头）
    size_t pos = 0;
    while (pos + sizeof(uint32_t) <= buffer.size()) {
        uint32_t m;
        memcpy(&m, buffer.data() + pos, sizeof(uint32_t));
        if (m == MAGIC) break;
        ++pos;
    }
    //先删除缓冲区前面没有意义的数据
    if (pos > 0) {
        buffer.erase(buffer.begin(), buffer.begin() + pos);
    }

    if (buffer.size() < sizeof(PacketHeader)) {
        return std::nullopt;
    }

    PacketHeader header;
    //包头数据拷贝
    memcpy(&header, buffer.data(), sizeof(header));

    size_t total_len = sizeof(PacketHeader) + header.body_len;
    if (buffer.size() < total_len) {
        return std::nullopt;  // 半包
    }

    Packet pkt;
    pkt.header = header;
    pkt.body.resize(header.body_len);

    if (header.body_len > 0) {
        memcpy(pkt.body.data(), buffer.data() + sizeof(PacketHeader), header.body_len);
    }
    buffer.erase(buffer.begin(), buffer.begin() + total_len);
    return pkt;
}

//在现代c++中使用字节流std::vector<uint8_t>传输pakcet，实际返回类型是Packet，被std::vector<uint8_t>包装过了
std::vector<uint8_t> pack_packet(uint32_t cmd, const std::vector<uint8_t>& body) {
    PacketHeader header;
    header.magic = MAGIC;
    header.cmd = cmd;
    header.body_len = static_cast<uint32_t>(body.size());

    std::vector<uint8_t> buffer(sizeof(PacketHeader) + body.size());

    //先拷贝包头
    memcpy(buffer.data(), &header, sizeof(PacketHeader));
    //再通过body拷贝包数据
    if (!body.empty()) {
        memcpy(buffer.data() + sizeof(PacketHeader), body.data(), body.size());
    }

    return buffer;
}


void TCP_Server::run() {
    //1.初始化网络环境
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    //2.创建socket
    SocketRAII server(socket(AF_INET, SOCK_STREAM, 0));
    //3.给服务器绑定地址
    // 准备一个地址
    SOCKADDR_IN addr{};
    addr.sin_family = AF_INET;	//使用的协议
    addr.sin_port = htons(8765);		//使用的端口号0-65535
    addr.sin_addr.S_un.S_addr = inet_addr("0.0.0.0"); // 监听服务器上的所有网卡，因为不止一个网卡
    //绑定到服务器上
    if (bind(server.get(), (sockaddr*)&addr, sizeof(SOCKADDR_IN)) == SOCKET_ERROR) {
        std::cout << "绑定服务器socket失败\r\n";
        return;
    }
    //4.开启服务器监听
    //backlog：允许完成三次握手的客户端数量
    if (listen(server.get(), 1) == SOCKET_ERROR) {
        std::cout << "开启监听失败\r\n";
        return;
    }
    init_handlers();  // ⭐ 初始化 handler
    //5.等待客服端连接
    std::cout << "waiting client...\n";
    SOCKADDR_IN client_addr{};
    int client_addr_len = sizeof(SOCKADDR_IN);

    while (true) {            
        //会阻塞在等待客户端连接
        std::cout << "等待客户端连接\r\n";
        SocketRAII client(accept(server.get(), (sockaddr*)&client_addr, &client_addr_len));
        std::cout << "客服端连接成功\r\n";
        std::vector<uint8_t> recv_buffer(1024 * 1024 * 3);
        size_t data_len = 0;

        while (true) {
            int n = recv(client.get(),
                (char*)recv_buffer.data() + data_len,
                recv_buffer.size() - data_len,
                0);
            if (n <= 0) {
                std::cout << "客户端断开连接\r\n";
                release_all_input();  // ⭐关键
                break;
            }
            bool flag = false;
            data_len += n;
            std::vector<uint8_t> temp(recv_buffer.begin(), recv_buffer.begin() + data_len);
            // ⭐ 多包解析
            while (true) {
                auto pkt_opt = try_parse_packet(temp);
                if (pkt_opt == std::nullopt) {
                    break;
                }
                //7.发送数据
                if (handle_packet(*pkt_opt, client.get()) == false) {
                    release_all_input();  // ⭐关键
                    flag = true;
                    break;
                }
            }
            if (flag == true) {
                break;
            }
            data_len = temp.size();
            memcpy(recv_buffer.data(), temp.data(), data_len);
        }
    }
    WSACleanup();
}

void TCP_Server::init_handlers() {
    handlers_[CMD_TYPE::CMD_SCREEN] = [](const Packet& pkt) {
        //获取本地屏幕
        //创建一个image对象
        CImage image;
        //2.拿到屏幕的上下文，DC：device context
        HDC hScreen = GetDC(NULL);
        //3.拿到屏幕的像素位宽 
        int bitWidth = GetDeviceCaps(hScreen, BITSPIXEL);
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
//        std::cout << "bitWidth:" << bitWidth << "\r\n";
        //4.拿到屏幕的宽高（像素为单位）
        //这里是逻辑分辨率
        int sWidth = GetSystemMetrics(SM_CXSCREEN);
        int sHeight = GetSystemMetrics(SM_CYSCREEN);
//        std::cout << "sWidth:" << sWidth << " sHeight:" << sHeight << "\r\n";
        //5.把屏幕数据复制到image里面
        image.Create(sWidth, sHeight, bitWidth);
        BitBlt(image.GetDC(), 0, 0, sWidth, sHeight, hScreen, 0, 0, SRCCOPY);
        //释放屏幕的上下文
        ReleaseDC(NULL, hScreen);
        //image.Save("./test.png", ::Gdiplus::ImageFormatPNG);
        //转换为网络流
        //从堆上申请一个可以变化的空间
        HGLOBAL hMen = GlobalAlloc(GMEM_MOVEABLE, 0);
        //if (hMen == NULL) {
        //    return std::vector<uint8_t>{'f', 'a', 'i', 'l', ' ', 
        //        't', 'o',' ', 
        //        'c', 'r', 'e', 'a', 't', 'e'};
        //}
        //创建一个内存流
        IStream* pStream = NULL;
        HRESULT ret = CreateStreamOnHGlobal(hMen, true, &pStream);
        if (ret == S_OK) {
            //将文件保存到内存流里
            image.Save(pStream, ::Gdiplus::ImageFormatJPEG);
            //将流指针放到开头
            //pStream = schar, STREAM_SEEK_SET：开头
            LARGE_INTEGER lg = { 0 };
            pStream->Seek(lg, STREAM_SEEK_SET, NULL);
            //获取这个流指针
            char * pdata = (char*)GlobalLock(hMen);
            int len = GlobalSize(hMen);
            std::vector<uint8_t> body(pdata, pdata + len);
            GlobalUnlock(hMen);
            //释放流指针
            pStream->Release();
            //释放全局内存
            GlobalFree(hMen);
            //释放图像DC
            image.ReleaseDC();
            return body;
        }

        //释放流指针
        pStream->Release();
        //释放全局内存
        GlobalFree(hMen);
        //释放图像DC
        image.ReleaseDC();
        return std::vector<uint8_t>{};
        };

    handlers_[CMD_TYPE::CMD_MOUSE] = [](const Packet& pkt) {
        Mouse mouse{};
        memcpy(&mouse.action, pkt.body.data(), pkt.body.size());
        //在模拟鼠标事件
        //设置鼠标位置
        SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
        switch (mouse.action) {
        case MOUSE_TYPE::MOUSE_MOVE:
            SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
            break;
        case MOUSE_TYPE::MOUSE_LDOWN:
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_LUP:
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_RDOWN:
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_RUP:
            mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_MDOWN:
            mouse_event(MOUSEEVENTF_MIDDLEDOWN, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_MUP:
            mouse_event(MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_LCLICK:
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_RCLICK:
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_MCLICK:
            mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break; 
        case MOUSE_TYPE::MOUSE_LDCLICK:            
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_RDCLICK:
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        case MOUSE_TYPE::MOUSE_MDCLICK:
            mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            mouse_event(MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_MIDDLEUP, 0, 0, 0, GetMessageExtraInfo());
            break;
        default:
            break;
        }
        return std::vector<uint8_t>{};
        };

    handlers_[CMD_TYPE::CMD_KEYBOARD] = [](const Packet& pkt) {
        //if (pkt.body.size() != sizeof(KeyBoard)) {
        //    std::cout << "keyboard数据不对" << '\n';
        //    return std::vector<uint8_t>{};
        //}
        KeyBoard key_board;
        memcpy(&key_board, pkt.body.data(), sizeof(KeyBoard));
        INPUT input{ 0 };
        input.ki.wVk = key_board.virtual_code;
        input.ki.wScan = 0;
        input.ki.time = 0;
        input.ki.dwFlags = key_board.key_status;       //按钮状态
        input.ki.dwExtraInfo = 0;
        input.type = INPUT_KEYBOARD;
        int ret = SendInput(1, &input, sizeof(INPUT));
        if (ret > 0) {
            std::cout << "成功执行键盘事件："<< key_board.virtual_code << '\n';
        }
        return std::vector<uint8_t>{};
        };
    handlers_[CMD_TYPE::CME_DISCONNECT] = [](const Packet& pkt) {
        std::string str = "disconnect";
        return std::vector<uint8_t>(str.begin(), str.end());
        };
}


bool TCP_Server::handle_packet(const Packet& pkt, SOCKET client) {
    std::vector<uint8_t> response;
    //std::cout << pkt.header.cmd <<  " " << pkt.header.body_len <<  " ";
    //for (int ch : pkt.body) {
    //    std::cout << ch << " ";
    //}
    //std::cout << "\n";
    //std::unordered_map<CMD_type, Handler>::iterator 
    auto it = handlers_.find(static_cast<CMD_TYPE>(pkt.header.cmd));
    if (it != handlers_.end()) {
        Handler func = it->second;
        response = func(pkt);
    }
    if (response.size() == 10 && response[0] == 'd' && response[1] == 'i' && response[2] == 's') {
        return false;
    }
    auto send_buf = pack_packet(pkt.header.cmd, response);

    send(client,
        (char*)send_buf.data(),
        send_buf.size(),
        0);
    return true;
}
