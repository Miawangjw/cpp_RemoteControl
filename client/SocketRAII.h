#pragma once
#include <Windows.h>
class SocketRAII {
private:
	SOCKET sock{ INVALID_SOCKET };
public:
	SocketRAII() = default;
	explicit SocketRAII(SOCKET s) : sock(s) {}

	~SocketRAII() {
		if (sock != INVALID_SOCKET) {
			closesocket(sock);
		}
	}

	SOCKET get() const { return sock; }

	SocketRAII(const SocketRAII&) = delete;
	SocketRAII& operator=(const SocketRAII&) = delete;

	SocketRAII(SocketRAII&& other) noexcept {
		sock = other.sock;
		other.sock = INVALID_SOCKET;
	}

	SocketRAII& operator=(SocketRAII&& other) noexcept {
		if (this != &other) {
			closesocket(sock);
			sock = other.sock;
			other.sock = INVALID_SOCKET;
		}
		return *this;
	}
};
