#pragma once
#include <stdint.h>
#include <vector>
#include <optional>
#pragma once
//结构体按照1字节对齐
#pragma pack(push, 1)
struct PacketHeader {
	uint32_t magic;		//4字节包头
	uint32_t cmd;		//4字节命令号
	uint32_t body_len;	//数据长度
};


#pragma pack(pop)
struct Packet {
	PacketHeader header{};		//包头
	std::vector<uint8_t> body;	//数据包
};

std::vector<uint8_t> pack_packet(uint32_t, const std::vector<uint8_t>&);
std::optional<Packet> try_parse_packet(std::vector<uint8_t>&);
std::optional<Packet> try_parse_packet(
	std::vector<uint8_t>& ,
	size_t& ,
	size_t 
);