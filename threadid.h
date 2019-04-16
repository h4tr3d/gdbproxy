//
// Created by hatred on 15.04.19.
//

#pragma once

#include <type_traits>
#include <string_view>
#include <string>
#include <cinttypes>

//
// Referece for parse / compose:
//  https://www.sourceware.org/gdb/onlinedocs/gdb.html#thread_002did-syntax
//

template<typename AddressSpace>
class threadid
{
public:
    using threadid_t = int64_t;
    using threadid_unsigned_t = uint64_t;

    constexpr static AddressSpace mask = std::make_unsigned_t<AddressSpace>(-1);

    enum ids : threadid_t {
        any = 0,
        all = -1,
    };


private:
    static threadid_t parse_id(std::string_view view)
    {
        try {
            bool sign = false;
            if (view.at(0) == '-') {
                sign = true;
                view = view.substr(1);
                // -1 - border case
                if (view.compare(0, 1, "1", 1) == 0)
                    return all;
            }

            std::string str(view);
            auto const value = static_cast<threadid_unsigned_t>(static_cast<threadid_t>(std::stoull(str, nullptr, 16)));

            return sign ? mask - value : value;
        } catch (...) {
            // TBD:
            return 0;
        }
    }

    static std::string make_id(threadid_t id)
    {
        std::string result = id < 0 ? "-" : "";

        // change sign
        auto const new_id = id < 0 ? -1 * id : id;

        char buf[sizeof(new_id)*2 + 1]{0};
        auto size = snprintf(buf, sizeof(buf), "%" PRIx64, static_cast<uint64_t>(new_id));
        result.append(buf, size_t(size));

        return result;
    }


public:

    threadid() = default;

    explicit threadid(std::string_view id)
    {
        std::string_view view = id;
        if (id[0] == 'p') {
            m_multiprocess = true;
            view = id.substr(1);

            auto pos = view.find_first_of('.');
            m_process = parse_id(view.substr(0, pos));

            if (pos == std::string_view::npos) {
                // According GDB docs, it mean 'pPID.-1'
                m_threadid = all;
                return;
            }

            view = view.substr(pos + 1);
        }

        m_threadid = parse_id(view);
    }

    explicit threadid(threadid_t tid)
        : m_multiprocess(false), m_threadid(tid)
    {
    }

    threadid(threadid_t pid, threadid_t tid)
        : m_multiprocess(true), m_process(pid), m_threadid(tid)
    {
    }

    threadid(bool multiprocess, threadid_t pid, threadid_t tid)
        : m_multiprocess(multiprocess), m_process(pid), m_threadid(tid)
    {
    }

    bool multiprocess() const noexcept
    {
        return m_multiprocess;
    }

    threadid_t pid() const noexcept
    {
        return m_process;
    }

    threadid_t tid() const noexcept
    {
        return m_threadid;
    }

    std::string to_string() const
    {
        std::string result;

        if (m_multiprocess) {
            result += 'p' + make_id(m_process) + '.';
        }

        result += make_id(m_threadid);
        return result;
    }

    bool operator==(const threadid& rhs) const noexcept
    {
        return m_threadid == rhs.m_threadid && (m_multiprocess ? m_process == rhs.m_process : true);
    }

    bool operator!=(const threadid& rhs) const noexcept
    {
        return !operator==(rhs);
    }

private:
    bool m_multiprocess = false;
    threadid_t m_process = 0;
    threadid_t m_threadid = 0;
};
