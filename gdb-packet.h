#pragma once

#include <vector>
#include <string_view>

enum class gdb_packet_type
{
    ack,
    nak,
    brk,
    generic,
    ext,
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

    virtual ~gdb_packet();

    virtual gdb_packet_type type() const noexcept
    {
        if (m_data.size()) {
            switch (m_data[0]) {
                case '$':
                    return gdb_packet_type::generic;
                case '+':
                    return gdb_packet_type::ack;
                case '-':
                    return gdb_packet_type::nak;
                case '\03':
                    return gdb_packet_type::brk;
            }
        }
        return gdb_packet_type::generic;
    }

    bool is_complete() const noexcept
    {
        return m_state == state::complete;
    }

    std::string_view data() const 
    {
        return std::string_view(m_data.data() + 1, m_data.size() - 1);
    }

    std::string_view raw_data() const
    {
        return std::string_view(m_data.data(), m_data.size());
    }

    bool is_invalid() const
    {
        return m_state == state::invalid;
    }

    state state() const noexcept
    {
        return m_state;
    }

    size_t push_back(const char *data, size_t size)
    {
        //m_data.push_back(data, size);
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
                    }
                    break;
            
                case state::crc:
                    if (m_data[m_data.size() - 2 - 1] == '#') {
                        m_state = state::complete;
                        return orig_size - size;
                    }
                    break;
            }
        }

        return orig_size - size;
    }

    void reset() noexcept
    {
        m_data.clear();
        m_state = state::invalid;
    }

protected:
    std::vector<char> m_data;
    enum state m_state = state::invalid;
};
