#pragma once

#include <vector>
#include <string_view>

enum class gdb_packet_type
{
    invalid,
    ack, // '+'
    nak, // '-'
    brk, // '\03'
    dat, // "$packet-data#cc", where CC - two hex digit CRC
};

class gdb_packet
{
public:
    enum class state {
        invalid,
        data,
        crc,
        complete
    };

    static gdb_packet packet_ack()
    {
        return gdb_packet_type::ack;
    }

    static gdb_packet packet_nak()
    {
        return gdb_packet_type::nak;
    }

    static gdb_packet packet_break()
    {
        return gdb_packet_type::brk;
    }

    static gdb_packet packet_data()
    {
        return gdb_packet_type::dat;
    }

    static gdb_packet packet_empty()
    {
        gdb_packet pkt(gdb_packet_type::dat);
        pkt.finalize();
        return pkt;
    }


    gdb_packet() = default;

    gdb_packet(gdb_packet_type type) noexcept;

    ~gdb_packet();

    gdb_packet_type type() const noexcept;

    bool is_complete() const noexcept;

    std::string_view data() const noexcept;

    std::string_view raw_data() const noexcept;

    uint8_t data_crc() const;
    uint8_t calc_crc() const noexcept;

    bool is_invalid() const noexcept;

    state state() const noexcept;

    size_t parse(const char *data, size_t size);
    size_t parse(const char *data);

    bool finalize();

    void reset() noexcept;

protected:
    std::vector<char> m_data;
    enum state m_state = state::invalid;
    uint8_t m_csum = 0;
};
