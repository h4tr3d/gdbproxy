#pragma once

#include <string>
#include <vector>

#include "gdb-packet.h"

using threadid_t = uint64_t;
using address_t = uint64_t;

int fromhex (int a);
uint8_t tohex (uint8_t nib);

size_t hex2bin(std::string_view hex, void *data, size_t size);
size_t hex2bin(std::string_view hex, std::vector<uint8_t> &bin);

void bin2hex(const void *ptr, size_t size, std::string& str);

gdb_packet gdb_memory_read_req(address_t address, address_t size);
size_t gdb_memory_read_reply(const gdb_packet& pkt, void *ptr, size_t size);
uint64_t gdb_memory_read_reply(const gdb_packet& pkt);
