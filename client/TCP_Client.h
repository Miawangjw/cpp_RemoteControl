#pragma once
#include <functional>
#include <vector>
#include "Packet.h"
#include <unordered_map>
#include <Windows.h>
#include <iostream>
#include <optional>
#include <string>
#include "CMD_type.h"
#pragma comment(lib, "ws2_32.lib")
class TCP_Client {
public:
    void init_handlers();
    void handle_packet(const Packet& pkt, SOCKET server);
private:
    using Handler = std::function<void(const Packet&)>;
    std::unordered_map<CMD_TYPE, std::function<void(const Packet&)>> handlers_;
};

