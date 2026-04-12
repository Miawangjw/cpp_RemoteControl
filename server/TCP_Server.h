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
class TCP_Server {
public:
    void run();

private:
    void init_handlers();
    bool handle_packet(const Packet& pkt, SOCKET client);
private:
    using Handler = std::function<std::vector<uint8_t>(const Packet& pkt)>;
    std::unordered_map<CMD_TYPE, Handler> handlers_;
};

