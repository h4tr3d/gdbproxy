#pragma once

#include "target.h"
#include "proxy-conn.hpp"
#include "gdb_packets.h"

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
    Symbol_xTopReadyPriority,
    Symbol_Count
};
} // ::freertos


// Stacking constants
enum class stack_growth {
    down = -1,
    up = 1,
    def = down,
};

enum : ssize_t {
    register_unused = -1,
    register_sp = -2,
    register_unavail = -3,
};

struct register_stack_offset
{
    // offset in bytes, -1 - register unstacked, -2 - SP register
    ssize_t offset;
    //
    unsigned width_bits;
};

struct register_stacking
{
    size_t context_size;
    stack_growth growth_direction;
    const register_stack_offset* registers;
    const size_t registers_count;
};

class target_mb_freertos : public target
{
public:
    target_mb_freertos(connection& conn);


    // target interface
public:
    bool process_request(const gdb_packet &pkt) override;

private:
    // Handlers
    bool handle_query_packet(const gdb_packet& pkt);
    bool handle_stop_reply_packets(const gdb_packet& pkt);

    // Processors
    void request_symbol();

    //
    // Thread update sequence
    //
    void update_threads(std::function<void()> done_cb);
    void get_threads_count(int idx = 0);
    void get_current_thread();
    void get_thread_lists_count();
    void get_thread_list_data();
    // thread list data processing
    void get_list_threads_count();
    void get_list_elem_ptr();
    // list elements
    void get_list_elem_thread_id();
    void get_list_elem_thread_name();
    void get_list_elem_next();
    // done
    void update_threads_done();

    //
    // Thread retrivie registers list
    //
    bool get_thread_reg_list(address_t thread_id);
    void get_thread_stack_ptr(address_t thread_id);
    void get_thread_registers();
    void get_thread_reg_list_done();

private:
    connection& m_conn;

    bool m_process_symbols = false;
    size_t m_current_symbol = 0;

    bool m_rtos_avail = false;

    std::array<address_t, freertos::Symbol_Count> m_symbols;

    // Threading
    struct thread_info
    {
        threadid_t threadid = 0;
        bool exists = false;
        std::string name;
        std::string extra;
    };
    struct thread_list
    {
        thread_list(address_t address)
            : address(address) {}
        address_t address = 0;
        size_t count = 0;
        address_t list_elem_ptr = 0;
        address_t list_elem_ptr_prev = address_t(-1);
    };
    struct threading
    {
        size_t     threads_count = 0;
        threadid_t current_thread = 0;
        std::deque<thread_info> info;

        std::vector<thread_list> lists;
        decltype (lists)::iterator list_it;

        std::function<void()> done_cb;
    };
    threading m_threading;

    gdb_packet m_stop_reply_response;


    struct registers
    {
        const register_stacking* stacking = nullptr;
        address_t stack_ptr = 0;

    };
    registers m_registers;
};
