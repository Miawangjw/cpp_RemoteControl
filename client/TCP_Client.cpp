#pragma once
#include "TCP_Client.h"
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
#pragma comment(lib, "ws2_32.lib")


// 外部变量（你现在的设计）
extern HWND g_hwnd;
extern CImage g_image;
extern CRITICAL_SECTION g_cri_sec;
extern int g_remote_width;
extern int g_remote_height;


void TCP_Client::init_handlers() {
	handlers_[CMD_TYPE::CMD_SCREEN] = [](const Packet& pkt) {

		const auto& body = pkt.body;
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, body.size());
		if (!hMem) return;

		void* pData = GlobalLock(hMem);
		memcpy(pData, body.data(), body.size());
		GlobalUnlock(hMem);

		IStream* pStream = NULL;
		if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {

			LARGE_INTEGER lg = { 0 };
			pStream->Seek(lg, STREAM_SEEK_SET, NULL);

			EnterCriticalSection(&g_cri_sec);

			if (!g_image.IsNull()) {
				g_image.Destroy();
			}

			g_image.Load(pStream);
			if (g_remote_width == -1 && g_remote_height == -1) {
				g_remote_width = g_image.GetWidth();
				g_remote_height = g_image.GetHeight();
			}
			LeaveCriticalSection(&g_cri_sec);

			InvalidateRect(g_hwnd, NULL, FALSE);

			pStream->Release();
		}
		};
}
void TCP_Client::handle_packet(const Packet& pkt, SOCKET server) {

	auto it = handlers_.find(static_cast<CMD_TYPE>(pkt.header.cmd));

	if (it != handlers_.end()) {
		it->second(pkt);
	}
}
