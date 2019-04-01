#include <iostream>
#include <iomanip>
#include <string>

#include "gdb-packet.h"

gdb_packet::~gdb_packet() = default;

gdb_packet::gdb_packet(gdb_packet_type type) noexcept
{
    char data = '\0';
    switch (type) {
        case gdb_packet_type::ack:
            data = '+';
            break;
        case gdb_packet_type::nak:
            data = '-';
            break;
        case gdb_packet_type::brk:
            data = '\03';
            break;
        case gdb_packet_type::dat:
            data = '$';
            break;
    }
    if (data)
        parse(&data, 1);
}

gdb_packet_type gdb_packet::type() const noexcept
{
    if (m_data.size()) {
        switch (m_data[0]) {
            case '$':
                return gdb_packet_type::dat;
            case '+':
                return gdb_packet_type::ack;
            case '-':
                return gdb_packet_type::nak;
            case '\03':
                return gdb_packet_type::brk;
        }
    }
    return gdb_packet_type::invalid;
}

bool gdb_packet::is_complete() const noexcept
{
    return m_state == state::complete;
}

std::string_view gdb_packet::data() const noexcept
{
    if (m_state != state::complete || type() != gdb_packet_type::dat)
        return std::string_view();
    // return only data, without header and CRC
    return std::string_view(m_data.data() + 1, m_data.size() - 1 - 3);
}

std::string_view gdb_packet::raw_data() const noexcept
{
    return m_state == state::invalid ?
                std::string_view() :
                std::string_view(m_data.data(), m_data.size());
}

uint8_t gdb_packet::data_crc() const
{
    if (type() != gdb_packet_type::dat && m_state != state::complete)
        return 0;
    auto const str = std::string(m_data.data() + m_data.size() - 2, 2);
    return static_cast<uint8_t>(std::stoi(str, nullptr, 16));
}

uint8_t gdb_packet::calc_crc() const noexcept
{
    return m_csum;
}

bool gdb_packet::is_invalid() const noexcept
{
    return m_state == state::invalid;
}

enum gdb_packet::state gdb_packet::state() const noexcept
{
    return m_state;
}

size_t gdb_packet::parse(const char *data, size_t size)
{
    if (m_state == state::complete)
        return 0;

    if (m_data.capacity() < m_data.size() + size) {
        m_data.reserve(m_data.size() + size);
    }

    const auto orig_size = size;

    // check packet start
    if (is_invalid()) {
        switch (*data) {
            case '$':
            case '+':
            case '-':
            case '\03':
                m_state = state::data;
                m_data.push_back(*data++);
                size--;
                // Ack and Nak packets follows without data
                if (m_data[0] != '$') {
                    m_state = state::complete;
                    return 1;
                }
                break;
            default:
                return 0;
        }
    }

    while (size --> 0) {
        const auto ch = *data++;
        m_data.push_back(ch);

        switch (m_state) {
            case state::data:
                if (ch == '#') {
                    m_state = state::crc;
                } else {
                    m_csum += ch;
                }
                break;

            case state::crc:
                if (m_data[m_data.size() - 2 - 1] == '#') {
                    m_state = state::complete;

                    std::clog << "pkt crc " << std::hex << (int)m_csum << " / " << std::string_view(m_data.data() + m_data.size() - 2, 2) << std::endl;

                    return orig_size - size;
                }
                break;
        }
    }

    return orig_size - size - 1;
}

bool gdb_packet::finalize()
{
    // Only Data packets can be finalized
    if (type() != gdb_packet_type::dat)
        return false;
    // Complete, CRC and invalid packets state unsupported
    if (m_state != state::data)
        return false;

    static const char hex[] = "0123456789abcdef";
    char buf[3] = {'#', 0, 0};
    buf[1] = hex[(m_csum >> 4) & 0xf];
    buf[2] = hex[(m_csum & 0xf)];

    return parse(buf, sizeof(buf)) == sizeof(buf);
}

void gdb_packet::reset() noexcept
{
    m_data.clear();
    m_state = state::invalid;
    m_csum = 0;
}
