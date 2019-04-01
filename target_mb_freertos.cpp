#include <cstdlib>

#include "gdb_packets.h"

#include "target_mb_freertos.h"

using namespace std;

static const char *s_symbols[] = {
    "_start", // "_start" hex-encoded
    "pxCurrentTCB",
    "pxReadyTasksLists",
    "xDelayedTaskList1",
    "xDelayedTaskList2",
    "pxDelayedTaskList",
    "pxOverflowDelayedTaskList",
    "xPendingReadyList",
    "xTasksWaitingTermination", // Only if INCLUDE_vTaskDelete
    "xSuspendedTaskList",       // Only if INCLUDE_vTaskSuspend
    "uxCurrentNumberOfTasks",
    "uxTopUsedPriority",        // Unavailable since v7.5.3
};

static const size_t s_symbols_count = std::size(s_symbols);

static_assert (freertos::Symbol_Count == s_symbols_count, "Wrong count of FreeRTOS symbols");

target_mb_freertos::target_mb_freertos(connection &conn)
    : m_conn(conn)
{
}

bool target_mb_freertos::process_request(const gdb_packet &req_pkt)
{
    // Resolve needed symbols
    if (req_pkt.data() == "qSymbol::") {
        process_symbols = true;
        current_symbol = 0;

        // reset symbols addresses
        m_symbols.fill(0);

        request_symbol();

        return true;
    }

    if (req_pkt.data().compare(0, 8, "qSymbol:") == 0 && process_symbols) {

        // TBD: process current symbol response
        auto addr = std::strtoull(req_pkt.data().data() + 8, nullptr, 16);
        if (addr == ULLONG_MAX && errno == ERANGE)
            addr = 0;

        m_symbols[current_symbol] = static_cast<uintptr_t>(addr);

        clog << "FreeRTOS symbol: " << s_symbols[current_symbol] << " = 0x" << hex << addr << '\n';

        // Try next symbols
        current_symbol++;
        if (current_symbol == freertos::Symbol_Count) {
            process_symbols = false;

            // Re-send qSymbols:: to the remote
            auto pkt = gdb_packet::packet_data();
            pkt.parse("qSymbol::");
            pkt.finalize();

            m_conn.push_request(std::move(pkt));
        } else {
            request_symbol();
        }

        return true;
    }

    return false;
}

void target_mb_freertos::request_symbol()
{
    auto pkt = gdb_packet::packet_data();
    pkt.parse("qSymbol:");
    std::string hex;
    bin2hex(s_symbols[current_symbol], strlen(s_symbols[current_symbol]), hex);
    pkt.parse(hex.data(), hex.size());
    pkt.finalize();

    m_conn.push_internal_response(std::move(pkt));
}
