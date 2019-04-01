#pragma once

#include <unordered_map>

#include "target.h"
#include "proxy-conn.hpp"

namespace freertos {
enum symbols_values
{
    Symbol_start,
    Symbol_pxCurrentTCB,
    Symbol_pxReadyTasksLists,
    Symbol_xDelayedTaskList1,
    Symbol_xDelayedTaskList2,
    Symbol_pxDelayedTaskList,
    Symbol_pxOverflowDelayedTaskList,
    Symbol_xPendingReadyList,
    Symbol_xTasksWaitingTermination,
    Symbol_xSuspendedTaskList,
    Symbol_uxCurrentNumberOfTasks,
    Symbol_uxTopUsedPriority,
    Symbol_Count
};
} // ::freertos


class target_mb_freertos : public target
{
public:
    target_mb_freertos(connection& conn);


    // target interface
public:
    bool process_request(const gdb_packet &req_pkt) override;

private:
    void request_symbol();

private:
    connection& m_conn;

    bool process_symbols = false;
    size_t current_symbol = 0;

    std::array<uintptr_t, freertos::Symbol_Count> m_symbols;
};
