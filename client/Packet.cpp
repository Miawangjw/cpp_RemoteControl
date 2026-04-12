#include "Packet.h"
#include <iostream>
constexpr uint32_t MAGIC = 0x55AA77CC;
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

std::optional<Packet> try_parse_packet(
	std::vector<uint8_t>& buffer,
	size_t& offset,
	size_t data_len
) {
	// 不够一个包头
	if (offset + sizeof(PacketHeader) > data_len) {
		return std::nullopt;
	}

	PacketHeader header;
	memcpy(&header, buffer.data() + offset, sizeof(header));

	// 校验magic（建议加）
	if (header.magic != MAGIC) {
		offset++; // 跳过一个字节继续找
		return std::nullopt;
	}

	size_t total_len = sizeof(PacketHeader) + header.body_len;
	// 半包
	if (offset + total_len > data_len) {
		return std::nullopt;
	}

	Packet pkt;
	pkt.header = header;
	pkt.body.resize(header.body_len);

	memcpy(pkt.body.data(),
		buffer.data() + offset + sizeof(PacketHeader),
		header.body_len);

	offset += total_len;

	return pkt;
}