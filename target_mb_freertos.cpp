#include <cstdlib>
#include <cinttypes>
#include <stdexcept>

#include "gdb_packets.h"

#include "target_mb_freertos.h"

using namespace std;

// Parameters...
#define POINTER_SIZE             address_t(4)
#define THREAD_COUNTER_SIZE      address_t(4)
#define LIST_NEXT_OFFSET         address_t(16)
#define LIST_WIDTH               address_t(20)
#define LIST_ELEM_NEXT_OFFSET    address_t(8)
#define LIST_ELEM_CONTENT_OFFSET address_t(12)
#define THREAD_STACK_OFFSET      address_t(0)
#define THREAD_NAME_OFFSET       address_t(52)
#define THREAD_NAME_SIZE         address_t(10) // make configurable
#define ENDIANNESS// TBD
// TBD stacking information, Microblaze-specific

// Must be specified via command line:
#define MAX_PRIORITIES (8) // Must be related to configMAX_PRIORITIES

#define CONTEXT_SIZE_NON_FPU size_t(132)
#define CONTEXT_SIZE_FPU     size_t(136) // with FPU

/* Offsets from the stack pointer at which saved registers are placed. */
#define portR31_OFFSET	4
#define portR30_OFFSET	8
#define portR29_OFFSET	12
#define portR28_OFFSET	16
#define portR27_OFFSET	20
#define portR26_OFFSET	24
#define portR25_OFFSET	28
#define portR24_OFFSET	32
#define portR23_OFFSET	36
#define portR22_OFFSET	40
#define portR21_OFFSET	44
#define portR20_OFFSET	48
#define portR19_OFFSET	52
#define portR18_OFFSET	56
#define portR17_OFFSET	60
#define portR16_OFFSET	64
#define portR15_OFFSET	68
#define portR14_OFFSET	72
#define portR13_OFFSET	76
#define portR12_OFFSET	80
#define portR11_OFFSET	84
#define portR10_OFFSET	88
#define portR9_OFFSET	92
#define portR8_OFFSET	96
#define portR7_OFFSET	100
#define portR6_OFFSET	104
#define portR5_OFFSET	108
#define portR4_OFFSET	112
#define portR3_OFFSET	116
#define portR2_OFFSET	120
#define portCRITICAL_NESTING_OFFSET 124
#define portMSR_OFFSET 128
#define portFSR_OFFSET 132

#define portREG_SIZE 32

// Taken from GDB
enum microblaze_regnum
{
    MICROBLAZE_R0_REGNUM,
    MICROBLAZE_R1_REGNUM, MICROBLAZE_SP_REGNUM = MICROBLAZE_R1_REGNUM,
    MICROBLAZE_R2_REGNUM,
    MICROBLAZE_R3_REGNUM, MICROBLAZE_RETVAL_REGNUM = MICROBLAZE_R3_REGNUM,
    MICROBLAZE_R4_REGNUM,
    MICROBLAZE_R5_REGNUM, MICROBLAZE_FIRST_ARGREG = MICROBLAZE_R5_REGNUM,
    MICROBLAZE_R6_REGNUM,
    MICROBLAZE_R7_REGNUM,
    MICROBLAZE_R8_REGNUM,
    MICROBLAZE_R9_REGNUM,
    MICROBLAZE_R10_REGNUM, MICROBLAZE_LAST_ARGREG = MICROBLAZE_R10_REGNUM,
    MICROBLAZE_R11_REGNUM,
    MICROBLAZE_R12_REGNUM,
    MICROBLAZE_R13_REGNUM,
    MICROBLAZE_R14_REGNUM,
    MICROBLAZE_R15_REGNUM,
    MICROBLAZE_R16_REGNUM,
    MICROBLAZE_R17_REGNUM,
    MICROBLAZE_R18_REGNUM,
    MICROBLAZE_R19_REGNUM,
    MICROBLAZE_R20_REGNUM,
    MICROBLAZE_R21_REGNUM,
    MICROBLAZE_R22_REGNUM,
    MICROBLAZE_R23_REGNUM,
    MICROBLAZE_R24_REGNUM,
    MICROBLAZE_R25_REGNUM,
    MICROBLAZE_R26_REGNUM,
    MICROBLAZE_R27_REGNUM,
    MICROBLAZE_R28_REGNUM,
    MICROBLAZE_R29_REGNUM,
    MICROBLAZE_R30_REGNUM,
    MICROBLAZE_R31_REGNUM,
    MICROBLAZE_PC_REGNUM,
    MICROBLAZE_MSR_REGNUM,
    MICROBLAZE_EAR_REGNUM,
    MICROBLAZE_ESR_REGNUM,
    MICROBLAZE_FSR_REGNUM,
    MICROBLAZE_BTR_REGNUM,
    MICROBLAZE_PVR0_REGNUM,
    MICROBLAZE_PVR1_REGNUM,
    MICROBLAZE_PVR2_REGNUM,
    MICROBLAZE_PVR3_REGNUM,
    MICROBLAZE_PVR4_REGNUM,
    MICROBLAZE_PVR5_REGNUM,
    MICROBLAZE_PVR6_REGNUM,
    MICROBLAZE_PVR7_REGNUM,
    MICROBLAZE_PVR8_REGNUM,
    MICROBLAZE_PVR9_REGNUM,
    MICROBLAZE_PVR10_REGNUM,
    MICROBLAZE_PVR11_REGNUM,
    MICROBLAZE_REDR_REGNUM,
    MICROBLAZE_RPID_REGNUM,
    MICROBLAZE_RZPR_REGNUM,
    MICROBLAZE_RTLBX_REGNUM,
    MICROBLAZE_RTLBSX_REGNUM,
    MICROBLAZE_RTLBLO_REGNUM,
    MICROBLAZE_RTLBHI_REGNUM,
    MICROBLAZE_SLR_REGNUM, MICROBLAZE_NUM_CORE_REGS = MICROBLAZE_SLR_REGNUM,
    MICROBLAZE_SHR_REGNUM,
    MICROBLAZE_NUM_REGS
};

// From GDB iteraction logs
static_assert(MICROBLAZE_NUM_REGS == 59, "");

static const register_stack_offset microblaze_non_fpu_registers[] = {
        [MICROBLAZE_R0_REGNUM ] = {register_unused, portREG_SIZE}, //        MICROBLAZE_R0_REGNUM,
        [MICROBLAZE_R1_REGNUM ] = {register_sp,     portREG_SIZE}, //        MICROBLAZE_R1_REGNUM, MICROBLAZE_SP_REGNUM = MICROBLAZE_R1_REGNUM,
        [MICROBLAZE_R2_REGNUM ] = {portR2_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R2_REGNUM,
        [MICROBLAZE_R3_REGNUM ] = {portR3_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R3_REGNUM, MICROBLAZE_RETVAL_REGNUM = MICROBLAZE_R3_REGNUM,
        [MICROBLAZE_R4_REGNUM ] = {portR4_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R4_REGNUM,
        [MICROBLAZE_R5_REGNUM ] = {portR5_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R5_REGNUM, MICROBLAZE_FIRST_ARGREG = MICROBLAZE_R5_REGNUM,
        [MICROBLAZE_R6_REGNUM ] = {portR6_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R6_REGNUM,
        [MICROBLAZE_R7_REGNUM ] = {portR7_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R7_REGNUM,
        [MICROBLAZE_R8_REGNUM ] = {portR8_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R8_REGNUM,
        [MICROBLAZE_R9_REGNUM ] = {portR9_OFFSET,   portREG_SIZE}, //        MICROBLAZE_R9_REGNUM,
        [MICROBLAZE_R10_REGNUM] = {portR10_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R10_REGNUM, MICROBLAZE_LAST_ARGREG = MICROBLAZE_R10_REGNUM,
        [MICROBLAZE_R11_REGNUM] = {portR11_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R11_REGNUM,
        [MICROBLAZE_R12_REGNUM] = {portR12_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R12_REGNUM,
        [MICROBLAZE_R13_REGNUM] = {portR13_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R13_REGNUM,
        [MICROBLAZE_R14_REGNUM] = {portR14_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R14_REGNUM,
        [MICROBLAZE_R15_REGNUM] = {portR15_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R15_REGNUM,
        [MICROBLAZE_R16_REGNUM] = {portR16_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R16_REGNUM,
        [MICROBLAZE_R17_REGNUM] = {portR17_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R17_REGNUM,
        [MICROBLAZE_R18_REGNUM] = {portR18_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R18_REGNUM,
        [MICROBLAZE_R19_REGNUM] = {portR19_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R19_REGNUM,
        [MICROBLAZE_R20_REGNUM] = {portR20_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R20_REGNUM,
        [MICROBLAZE_R21_REGNUM] = {portR21_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R21_REGNUM,
        [MICROBLAZE_R22_REGNUM] = {portR22_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R22_REGNUM,
        [MICROBLAZE_R23_REGNUM] = {portR23_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R23_REGNUM,
        [MICROBLAZE_R24_REGNUM] = {portR24_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R24_REGNUM,
        [MICROBLAZE_R25_REGNUM] = {portR25_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R25_REGNUM,
        [MICROBLAZE_R26_REGNUM] = {portR26_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R26_REGNUM,
        [MICROBLAZE_R27_REGNUM] = {portR27_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R27_REGNUM,
        [MICROBLAZE_R28_REGNUM] = {portR28_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R28_REGNUM,
        [MICROBLAZE_R29_REGNUM] = {portR29_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R29_REGNUM,
        [MICROBLAZE_R30_REGNUM] = {portR30_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R30_REGNUM,
        [MICROBLAZE_R31_REGNUM] = {portR31_OFFSET,  portREG_SIZE}, //        MICROBLAZE_R31_REGNUM,
        [MICROBLAZE_PC_REGNUM ] = {portR14_OFFSET,  portREG_SIZE}, //        MICROBLAZE_PC_REGNUM,
        [MICROBLAZE_MSR_REGNUM] = {portMSR_OFFSET,  portREG_SIZE}, //        MICROBLAZE_MSR_REGNUM,
        // Next registers currently unavail
        [MICROBLAZE_EAR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_EAR_REGNUM,
        [MICROBLAZE_ESR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_ESR_REGNUM,
        [MICROBLAZE_FSR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_FSR_REGNUM,
        [MICROBLAZE_BTR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_BTR_REGNUM,
        [MICROBLAZE_PVR0_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR0_REGNUM,
        [MICROBLAZE_PVR1_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR1_REGNUM,
        [MICROBLAZE_PVR2_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR2_REGNUM,
        [MICROBLAZE_PVR3_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR3_REGNUM,
        [MICROBLAZE_PVR4_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR4_REGNUM,
        [MICROBLAZE_PVR5_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR5_REGNUM,
        [MICROBLAZE_PVR6_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR6_REGNUM,
        [MICROBLAZE_PVR7_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR7_REGNUM,
        [MICROBLAZE_PVR8_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR8_REGNUM,
        [MICROBLAZE_PVR9_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR9_REGNUM,
        [MICROBLAZE_PVR10_REGNUM ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR10_REGNUM,
        [MICROBLAZE_PVR11_REGNUM ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_PVR11_REGNUM,
        [MICROBLAZE_REDR_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_REDR_REGNUM,
        [MICROBLAZE_RPID_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RPID_REGNUM,
        [MICROBLAZE_RZPR_REGNUM  ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RZPR_REGNUM,
        [MICROBLAZE_RTLBX_REGNUM ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RTLBX_REGNUM,
        [MICROBLAZE_RTLBSX_REGNUM] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RTLBSX_REGNUM,
        [MICROBLAZE_RTLBLO_REGNUM] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RTLBLO_REGNUM,
        [MICROBLAZE_RTLBHI_REGNUM] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_RTLBHI_REGNUM,
        [MICROBLAZE_SLR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_SLR_REGNUM, MICROBLAZE_NUM_CORE_REGS = MICROBLAZE_SLR_REGNUM,
        [MICROBLAZE_SHR_REGNUM   ] = {register_unavail, portREG_SIZE}, //        MICROBLAZE_SHR_REGNUM,
};

static_assert(std::size(microblaze_non_fpu_registers) == MICROBLAZE_NUM_REGS, "Wrong Microblaze registers mapping");

const static register_stacking microblaze_non_fpu_stacking = {
    CONTEXT_SIZE_NON_FPU,
    stack_growth::down,
    microblaze_non_fpu_registers,
    std::size(microblaze_non_fpu_registers)
};

enum class symbol_avail
{
    optional,
    required,
};

struct symbol_info
{
    const char* name;
    symbol_avail requirement;
};

static const symbol_info s_symbols_desc[] = {
    {"_start", symbol_avail::optional}, // just test
    {"pxCurrentTCB", symbol_avail::required},
    {"pxReadyTasksLists", symbol_avail::required},
    {"xDelayedTaskList1", symbol_avail::required},
    {"xDelayedTaskList2", symbol_avail::required},
    {"pxDelayedTaskList", symbol_avail::required},
    {"pxOverflowDelayedTaskList", symbol_avail::required},
    {"xPendingReadyList", symbol_avail::required},
    {"xTasksWaitingTermination", symbol_avail::optional}, // Only if INCLUDE_vTaskDelete
    {"xSuspendedTaskList", symbol_avail::optional},       // Only if INCLUDE_vTaskSuspend
    {"uxCurrentNumberOfTasks", symbol_avail::required},
    {"uxTopUsedPriority", symbol_avail::optional},        // Unavailable since v7.5.3
    {"xTopReadyPriority", symbol_avail::optional},        // Alternative to uxTopUsedPriority on newer versions

};

static const size_t s_symbols_count = std::size(s_symbols_desc);

static_assert (freertos::Symbol_Count == s_symbols_count, "Wrong count of FreeRTOS symbols");

target_mb_freertos::target_mb_freertos(connection &conn)
    : m_conn(conn)
{
}

bool target_mb_freertos::process_request(const gdb_packet &pkt)
{
    // Just route packets
    if (pkt.type() == gdb_packet_type::dat) {
        switch (pkt.data()[0]) {
            case 'q':
                return handle_query_packet(pkt);

            case 'C':
            case 'c':
            case 'S':
            case 's':
            //case '?': // TBD
                return handle_stop_reply_packets(pkt);

            case 'v': // series of vXXX packets
                if ((pkt.data().at(5) != '?' && pkt.data().compare(1, 4, "Cont", 4) == 0) || // Skip "vCont?" request
                     pkt.data().compare(1, 6, "Attach", 6) == 0 ||
                     pkt.data().compare(1, 3, "Run", 3) == 0 ||
                     pkt.data().compare(1, 7, "Stopped", 7) == 0) {
                    return handle_stop_reply_packets(pkt);
                }
                break;

            // registers read request
            case 'g':
            {

                break;
            }
        }
    }

    return false;
}

bool target_mb_freertos::handle_query_packet(const gdb_packet &pkt)
{
    if (pkt.data().compare(0, 17, "qThreadExtraInfo,", 17) == 0) {
        return false;
        if (!m_rtos_avail || !m_threading.threads_count)
            return false;

        threadid_t threadid;
        auto data = pkt.data().substr(17);

        clog << "qThreadEtraInfo data: " << data << '\n';
    }

    if (pkt.data().compare(0, 12, "qfThreadInfo", 12) == 0) {
        return false;
        auto rep = gdb_packet::packet_data();
        if (m_rtos_avail && m_threading.threads_count) {
            // Assume thread id are 16+1 for ','

        } else {
            rep.parse("l", 1);
        }
        rep.finalize();
        m_conn.push_internal_response(std::move(rep));
        return true;
    }

    //
    // Resolve needed symbols
    //

    // Handle initial symbols request
    if (pkt.data() == "qSymbol::") {
        m_process_symbols = true;
        m_current_symbol = 0;
        m_rtos_avail = false;

        // reset symbols addresses
        m_symbols.fill(0);

        request_symbol();

        return true;
    }

    // Handle symbol request with data
    if (pkt.data().compare(0, 8, "qSymbol:") == 0 && m_process_symbols) {

        auto addr = std::strtoull(pkt.data().data() + 8, nullptr, 16);
        if (addr == ULLONG_MAX && errno == ERANGE)
            addr = 0;

        m_symbols[m_current_symbol] = static_cast<address_t>(addr);

        clog << "FreeRTOS symbol: " << s_symbols_desc[m_current_symbol].name << " = 0x" << hex << addr << '\n';

        // Try next symbols
        m_current_symbol++;
        if (m_current_symbol == freertos::Symbol_Count) {
            m_process_symbols = false;

            // Check RTOS avail
            m_rtos_avail = true;
            for (size_t i = 0; i < m_symbols.size(); ++i) {
                // there is no symbol
                if (m_symbols[i] == 0 && s_symbols_desc[i].requirement == symbol_avail::required) {
                    clog << "RTOS required symbol '" << s_symbols_desc[i].name << "' not found\n";
                    m_rtos_avail = false;
                    break;
                }
            }

            // Re-send qSymbols:: to the remote
            auto reply = gdb_packet::packet_data();
            reply.parse("qSymbol::");
            reply.finalize();

            m_conn.push_request(std::move(reply));
        } else {
            request_symbol();
        }

        return true;
    }

    return false;
}

bool target_mb_freertos::handle_stop_reply_packets(const gdb_packet &pkt)
{
    // Bypass packet as-is to the remote, handle response, store it, update-threading info,
    // update response with threading info and re-transmit it to the client
    // TBD: non-stop mode unhandled

    clog << "handle stop-reply packet\n";

    m_conn.push_internal_request(gdb_packet(pkt), [this] (auto&, auto& reply) {
        // Reply must be 'T'-type
        clog << "stop-packet reply: " << reply.data() << '\n';

        if (reply.data()[0] != 'T') {
            throw  system_error(make_error_code(errc::invalid_argument), "stop-packet reply assumes 'T'-type");
        }

        // store reply
        m_stop_reply_response = reply;

        // Update threads and re-send reply packet
        update_threads([this]() {
            // TBD: update it according threading info
            m_conn.push_internal_response(std::move(m_stop_reply_response));
        });
    });

    return true;
}

void target_mb_freertos::request_symbol()
{
    auto pkt = gdb_packet::packet_data();
    pkt.parse("qSymbol:");
    std::string hex;
    bin2hex(s_symbols_desc[m_current_symbol].name, strlen(s_symbols_desc[m_current_symbol].name), hex);
    pkt.parse(hex.data(), hex.size());
    pkt.finalize();

    m_conn.push_internal_response(std::move(pkt));
}

void target_mb_freertos::update_threads(std::function<void()> done_cb)
{
    if (!m_rtos_avail) {
        // run done_cb directly, may be some action is expected
        if (done_cb)
            done_cb();
        return;
    }

    // clean
    m_threading = threading();
    m_threading.done_cb = std::move(done_cb);

    get_threads_count();
}

void target_mb_freertos::get_threads_count(int idx)
{
    // Read count of threads
    m_conn.push_internal_request(gdb_memory_read_req(m_symbols[freertos::Symbol_uxCurrentNumberOfTasks], THREAD_COUNTER_SIZE),
                                 [this, idx](auto &/*req*/, auto &reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS threads count: " << val << '\n';

        m_threading.threads_count = val;

        // counter update requires time
        //if (idx >= 10)
            get_current_thread();
        //else
        //    get_threads_count(idx + 1);
    });
}

void target_mb_freertos::get_current_thread()
{
    const auto address = m_symbols[freertos::Symbol_pxCurrentTCB];
    m_conn.push_internal_request(gdb_memory_read_req(address, POINTER_SIZE),
                                 [this](auto & /*req*/, auto &reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS current thread: 0x" << hex << val << '\n';

        m_threading.current_thread = val;

        if (m_threading.threads_count == 0 || m_threading.current_thread == 0) {
            // Either : No RTOS threads - there is always at least the current execution though
            // OR     : No current thread - all threads suspended - show the current execution
            //          of idling

            m_threading.threads_count++;

            auto & info = m_threading.info.emplace_back();
            info.threadid = 1;
            info.name = "Current Execution";
            info.exists = true;

            if (m_threading.threads_count == 1) {
                update_threads_done();
                return;
            }
        }

        // next processing
        get_thread_lists_count();
    });
}

void target_mb_freertos::get_thread_lists_count()
{
    auto process_ready_thread_lists_count = [this](uint64_t count) {
        clog << "RTOS ready thread lists count: " << count << '\n';

        //m_threading.list_current = 0;
        m_threading.lists.reserve(count + 1 + 5);

        for (size_t i = 0; i < count; i++) {
            m_threading.lists.push_back(m_symbols[freertos::Symbol_pxReadyTasksLists] + i * LIST_WIDTH);
        }

        m_threading.lists.push_back(m_symbols[freertos::Symbol_xDelayedTaskList1]);
        m_threading.lists.push_back(m_symbols[freertos::Symbol_xDelayedTaskList2]);
        m_threading.lists.push_back(m_symbols[freertos::Symbol_xPendingReadyList]);
        m_threading.lists.push_back(m_symbols[freertos::Symbol_xTasksWaitingTermination]);
        m_threading.lists.push_back(m_symbols[freertos::Symbol_xSuspendedTaskList]);

        clog << "RTOS: thread lists count: " << dec << m_threading.lists.size() << '\n';

        m_threading.list_it = m_threading.lists.begin();

        // next
        get_thread_list_data();
    };

    address_t address = 0;
    if (m_symbols[freertos::Symbol_uxTopUsedPriority]) {
        address = m_symbols[freertos::Symbol_uxTopUsedPriority];
    } else if (m_symbols[freertos::Symbol_xTopReadyPriority]) {
        address = m_symbols[freertos::Symbol_xTopReadyPriority];
    } else {
        // TODO: read from configuration
        process_ready_thread_lists_count(MAX_PRIORITIES);
        return;
    }

    m_conn.push_internal_request(gdb_memory_read_req(address, THREAD_COUNTER_SIZE),
                                 [this, process_ready_thread_lists_count](auto & /*req*/, auto & reply) {
        auto const val = gdb_memory_read_reply(reply);
        process_ready_thread_lists_count(val);
    });
}

void target_mb_freertos::get_thread_list_data()
{
    do {
        if (m_threading.list_it == m_threading.lists.end()) {
            // threading processing done

            // TBD: we must done thread_update pipeline
            update_threads_done();
            return;
        }

        if (!m_threading.list_it->address) {
            m_threading.list_it++;
            continue;
        }
        break;
    } while (true);

    get_list_threads_count();
}

void target_mb_freertos::get_list_threads_count()
{
    auto & list = *m_threading.list_it;
    const auto address = list.address;
    m_conn.push_internal_request(gdb_memory_read_req(address, THREAD_COUNTER_SIZE),
                                 [this,&list](auto&, auto& reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS thread list threads: " << val << '\n';

        list.count = val;

        if (list.count == 0) {
            // continue
            m_threading.list_it++;
            get_thread_list_data();
            return;
        }

        get_list_elem_ptr();
    });
}

void target_mb_freertos::get_list_elem_ptr()
{
    auto & list = *m_threading.list_it;
    const auto address = list.address + LIST_NEXT_OFFSET;
    m_conn.push_internal_request(gdb_memory_read_req(address, POINTER_SIZE),
                                 [this,&list](auto&, auto& reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS thread list elems ptr: 0x" << hex << val << '\n';

        list.list_elem_ptr = val;

        get_list_elem_thread_id();
    });
}

void target_mb_freertos::get_list_elem_thread_id()
{
    auto & list = *m_threading.list_it;
    const auto address = list.list_elem_ptr + LIST_ELEM_CONTENT_OFFSET;

    if (list.count == 0 ||
        list.list_elem_ptr == 0 ||
        list.list_elem_ptr == list.list_elem_ptr_prev) {
        // done list processing
        m_threading.list_it++;
        get_thread_list_data();
        return;
    }

    m_conn.push_internal_request(gdb_memory_read_req(address, POINTER_SIZE),
                                 [this](auto&, auto& reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS thread_id: 0x" << hex << val << '\n';

        auto& info = m_threading.info.emplace_back();

        info.threadid = val;

        get_list_elem_thread_name();
    });

}

void target_mb_freertos::get_list_elem_thread_name()
{
    auto & info = m_threading.info.back();
    const auto address = info.threadid + THREAD_NAME_OFFSET;

    m_conn.push_internal_request(gdb_memory_read_req(address, THREAD_NAME_SIZE),
                                 [this,&info](auto&, auto& reply) {

        vector<uint8_t> bin;
        hex2bin(reply.data(), bin);
        bin.push_back('\0');

        if (bin[0] == '\0') {
            info.name = "No Name";
        } else {
            info.name = reinterpret_cast<const char*>(bin.data());
        }

        if (m_threading.current_thread == info.threadid) {
            info.extra = "State: Running";
        }

        info.exists = true;

        clog << "RTOS thread name: " << info.name << '\n';

        get_list_elem_next();
    });
}

void target_mb_freertos::get_list_elem_next()
{
    auto & list = *m_threading.list_it;

    list.count--;
    const auto address = list.list_elem_ptr + LIST_ELEM_NEXT_OFFSET;
    m_conn.push_internal_request(gdb_memory_read_req(address, POINTER_SIZE),
                                 [this,&list](auto&, auto& reply) {
        auto const val = gdb_memory_read_reply(reply);
        clog << "RTOS thread list elems ptr: 0x" << hex << val << '\n';

        list.list_elem_ptr_prev = list.list_elem_ptr;
        list.list_elem_ptr = val;

        get_list_elem_thread_id();
    });

}

void target_mb_freertos::update_threads_done()
{
    clog << "RTOS threads update done... Found " << m_threading.info.size() << " tasks of " << m_threading.threads_count << '\n';
    m_threading.lists.clear();
    m_threading.threads_count = m_threading.info.size();

    if (m_threading.done_cb) {
        m_threading.done_cb();
    }
}

bool target_mb_freertos::get_thread_reg_list(address_t thread_id)
{
    if (thread_id == 0) {
        return false;
    }

    // work only for non-current thread
    if (m_threading.current_thread == thread_id) {
        return false;
    }

    // Clean register information
    m_registers = registers();

    // TBD: make configurable
    m_registers.stacking = &microblaze_non_fpu_stacking;

    get_thread_stack_ptr(thread_id);

    return true;
}

void target_mb_freertos::get_thread_stack_ptr(address_t thread_id)
{
    m_conn.push_internal_request(gdb_memory_read_req(thread_id + THREAD_STACK_OFFSET, POINTER_SIZE),
                                 [this](auto&, auto& reply) {
        auto const val = gdb_memory_read_reply(reply);

        m_registers.stack_ptr = val;

        // next
        get_thread_registers();
    });
}

void target_mb_freertos::get_thread_registers()
{
    const auto address = m_registers.stacking->growth_direction == stack_growth::up ?
                            m_registers.stack_ptr - m_registers.stacking->context_size :
                            m_registers.stack_ptr;
    m_conn.push_internal_request(gdb_memory_read_req(address, m_registers.stacking->context_size),
                                 [this,address](auto&, auto& reply) {
        std::vector<uint8_t> bin;
        hex2bin(reply.data(), bin);

        address_t new_stack_ptr = address -
                static_cast<ssize_t>(m_registers.stacking->growth_direction) * m_registers.stacking->context_size;

        auto pkt = gdb_packet::packet_data();

        uint8_t stack_ptr_buf[4]{0};
        // TBD Little-Big Endian support
        memcpy(&stack_ptr_buf, &new_stack_ptr, sizeof(stack_ptr_buf));

        for (size_t i = 0; i < m_registers.stacking->registers_count; ++i) {
            for (size_t j = 0; j < m_registers.stacking->registers[i].width_bits / 8; ++j) {
                const auto offset = m_registers.stacking->registers[i].offset;
                char buf[3]{0};
                switch (offset) {
                    case register_unused:
                        snprintf(buf, sizeof(buf), "%02x", 0);
                        break;
                    case register_unavail:
                        snprintf(buf, sizeof(buf), "xx");
                        break;
                    case register_sp:
                        snprintf(buf, sizeof(buf), "%02x", stack_ptr_buf[j]);
                        break;
                    default:
                        snprintf(buf, sizeof(buf), "%02x", bin[offset + j]);
                        break;
                }
                pkt.parse(buf, 2);
            }
        }

        pkt.finalize();

        m_conn.push_internal_response(std::move(pkt));
    });
}

