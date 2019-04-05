#include <iostream>
#include <cinttypes>
#include <cstdlib>
#include <climits>
#include <system_error>

#include "gdb_packets.h"

using namespace std;

int fromhex(int a)
{
    if (a >= '0' && a <= '9')
        return a - '0';
    else if (a >= 'a' && a <= 'f')
        return a - 'a' + 10;
    else if (a >= 'A' && a <= 'F')
        return a - 'A' + 10;
    else
        cerr << "Invalid HEX digit: '" << (char)a << "'\n";
    return 0;
}

uint8_t tohex(uint8_t nib)
{
    if (nib < 10)
        return '0' + nib;
    else
        return 'a' + nib - 10;
}

size_t hex2bin(string_view hex, void *data, size_t size)
{
    size_t i;
    auto *bin = static_cast<uint8_t*>(data);
    const size_t count = std::min(hex.size()/2, size);

    for (i = 0; i < count; i++) {
        if (hex[i*2] == 0 || hex[i*2+1] == 0) {
            /* Hex string is short, or of uneven length.
             Return the count that has been converted so far.  */
            return i;
        }
        *bin++ = fromhex (hex[i*2+0]) * 16 + fromhex (hex[i*2+1]);
    }
    return i;
}

//size_t hex2bin(string_view hex, vector<uint8_t>& bin)
//{
//    bin.resize(hex.size()/2);
//    return hex2bin(hex, bin.data(), bin.size());
//}

void bin2hex(const void *ptr, size_t size, string &str)
{
    auto bin = static_cast<const uint8_t*>(ptr);
    while (size --> 0) {
        auto ch = *bin++;
        uint8_t h1 = tohex(uint8_t(ch >> 4u) & 0xfu);
        uint8_t h2 = tohex(ch & 0xfu);
        str.push_back(h1);
        str.push_back(h2);
    }
}


gdb_packet gdb_memory_read_req(address_t address, address_t size)
{
    auto pkt = gdb_packet::packet_data();
    char buf[2 + 8*2 + 1] {0};
    auto sz = snprintf(buf, sizeof(buf), "m%" PRIx64 ",%" PRIx64, address, size);
    if (sz < 0) {
        throw std::system_error(errno, std::generic_category(), "gdb_memory_read_req");
    }

    clog << "memory read packet: " << buf << '\n';

    pkt.parse(buf, static_cast<size_t>(sz));
    pkt.finalize();
    return pkt;
}

size_t gdb_memory_read_reply(const gdb_packet &pkt, void *ptr, size_t size)
{
    auto hex = pkt.data();
    return hex2bin(hex, ptr, size);
}

uint64_t gdb_memory_read_reply(const gdb_packet &pkt)
{
    auto hex = pkt.data();
    uint64_t value = 0;
    hex2bin(hex, &value, sizeof(value));
    // TBD: little<->big endian
    return value;
}
