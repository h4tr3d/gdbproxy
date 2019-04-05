#include <cstdlib>
#include <cinttypes>
#include <stdexcept>

#include "gdb_packets.h"

#include "target_mb_freertos.h"

using namespace std;

static constexpr threadid_t CURRENT_EXECUTION_THREAD_ID = 1u;
static constexpr threadid_t INVALID_THREAD_ID = 0u;

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
#define portPC_OFFSET   portR14_OFFSET
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
        [MICROBLAZE_PC_REGNUM ] = {portPC_OFFSET,   portREG_SIZE}, //        MICROBLAZE_PC_REGNUM,
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
    // Threading will be updated after symbols resolving
    // currently there is at least one "thread" - Current Execution
    make_empty_threading();
}

static void push_client_reply(connection& conn, const char* ptr, size_t size)
{
    auto rep = gdb_packet::packet_data();
    rep.parse(ptr, size);
    rep.finalize();
    conn.push_internal_response(std::move(rep));
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
                clog << "current selected thread: " << m_current_thread_id
                     << ", current RTOS thread: " << m_threading.current_thread_id
                     << '\n';

                if (m_current_thread_id == make_threadid(THREAD_ID_CURRENT_EXECUTION)) {
                    return false;
                }

                if (m_rtos_avail) {
                    if (m_current_thread_id == m_threading.current_thread_id) {
                        return false;
                    }

                    auto thread_tcb = parse_threadid(m_current_thread_id);
                    if (thread_tcb > THREAD_ID_CURRENT_EXECUTION)
                        return get_thread_reg_list(static_cast<address_t>(thread_tcb));
                }
                break;
            }

            // Thread alive
            case 'T':
            {
                if (!m_rtos_avail)
                    break;

                auto view = pkt.data().substr(1);

                if (view == make_threadid(THREAD_ID_CURRENT_EXECUTION))
                    return false;

                auto it = m_threading.info.end();
                if (m_rtos_avail && m_threading.threads_count) {
                    for (it = m_threading.info.begin(); it != m_threading.info.end(); ++it) {
                        if (it->thread_id == view && it->exists) {
                            break;
                        }
                    }
                }

                if (it != m_threading.info.end()) {
                    push_client_reply(m_conn, "OK", 2);
                } else {
                    push_client_reply(m_conn, "E01", 3);
                }

                return true;
            }

            // Set current thread
            case 'H':
            {
                if (pkt.data()[1] == 'g') {
                    bool handled;
                    auto view = pkt.data().substr(2);
                    m_current_thread_id = string(view);
                    if (view == make_threadid(THREAD_ID_CURRENT_EXECUTION)) {
                        // bypass to target
                        handled = false;
                    } else {
                        push_client_reply(m_conn, "OK", 2);
                        handled = true;
                    }

                    clog << "thread id: " << view << ", " << m_current_thread_id << '\n';

                    return handled;
                }

                break;
            }

            // extended restart packet, may be need to handle by threading subsystem
            case 'R':
                break;
        }
    }

    return false;
}

bool target_mb_freertos::handle_query_packet(const gdb_packet &pkt)
{
    if (pkt.data().compare(0, 10, "qSupported", 10) == 0) {
        // Retransmit to the target. Note, avoid complex iteraction during AckMode is active. Currently it supported to ugly
        m_conn.push_request(gdb_packet(pkt),
                            [this](auto&, auto& reply) {
            clog << "qSupported reply: " << reply.data() << '\n';
            if (reply.data().find(";multiprocess+") != string_view::npos) {
                clog << "RTOS multiprocess feature is supported\n";
                m_multiprocess = true;
            }
        });
        return true;
    }

    if (pkt.data().compare(0, 17, "qThreadExtraInfo,", 17) == 0) {
        if (!m_rtos_avail || !m_threading.threads_count)
            return false;

        auto tid = pkt.data().substr(17);
        clog << "qThreadEtraInfo thread-id: " << tid << '\n';

        // Bypass current execution to the target system
        if (tid == make_threadid(THREAD_ID_CURRENT_EXECUTION)) {
            return false;
        }

        auto it = m_threading.info.begin();
        for (;it != m_threading.info.end() && tid != it->thread_id; ++it);

        auto rep = gdb_packet::packet_data();
        if (it == m_threading.info.end()) {
            rep.parse("E01", 3);
        } else {
            size_t total_len = it->name.size() + it->extra.size() + 9;
            string extra_info;
            extra_info.reserve(total_len);
            if (!it->name.empty()) {
                extra_info += "Name: ";
                extra_info += it->name;
            }
            if (!it->extra.empty()) {
                if (!extra_info.empty())
                    extra_info += ", ";
                extra_info += it->extra;
            }

            string hex;
            bin2hex(extra_info.data(), extra_info.size(), hex);

            clog << hex << '\n';

            rep.parse(hex.data(), hex.size());
        }

        clog << "qThreadExtraInfo rep: " << rep.data() << '\n';

        rep.finalize();
        m_conn.push_internal_response(std::move(rep));
        return true;
    }

    // Request thread ids
    if (pkt.data().compare(0, 12, "qfThreadInfo", 12) == 0) {
        if (!m_rtos_avail)
            return false;

        auto rep = gdb_packet::packet_data();

        if (m_threading.threads_count) {
            // Assume thread id are 16+1+3. 1 for ',' or 'm'. 3 - for possible "p1."
            rep.reserve(20 * m_threading.threads_count);
            rep.parse("m");
            for (auto it = m_threading.info.begin(); it != m_threading.info.end(); ++it) {
                // add comma after previous entry
                if (it != m_threading.info.begin())
                    rep.parse(",", 1);
                rep.parse(it->thread_id.data(), it->thread_id.size());
            }
        }
        rep.finalize();

        clog << "qfThreadInfo internal response: " << rep.data() << '\n';

        m_conn.push_internal_response(std::move(rep));
        return true;
    }

    auto push_reply = [this](const char* ptr, size_t size) {
        auto rep = gdb_packet::packet_data();
        rep.parse(ptr, size);
        rep.finalize();
        m_conn.push_internal_response(std::move(rep));
    };

#if 0
    // TODO: is it needed to be handled?
    if (pkt.data().compare(0, 12, "qsThreadInfo", 12) == 0) {
        push_reply("l", 1);
        return true;
    }

    // TODO: is it needed to be handled?
    if (pkt.data().compare(0, 9, "qAttached", 9) == 0) {
        push_reply("1", 1);
        return true;
    }

    // TODO: is it needed to be handled?
    if (pkt.data().compare(0, 8, "qOffsets", 8) == 0) {
        const char offsets[] = "Text=0;Data=0;Bss=0";
        push_reply(offsets, sizeof(offsets) - 1);
        return true;
    }
#endif

    if (pkt.data().compare(0, 5, "qCRC:", 5) == 0) {
        // check before "qC" to omit incorrect handling
        return false;
    }

    if (pkt.data().compare(0, 2, "qC", 2) == 0) {
        if (!m_rtos_avail)
            return false;

        string rep = "QC" + m_threading.current_thread_id;
        push_reply(rep.data(), rep.size());

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
            auto new_resp = gdb_packet::packet_data();
            auto signal = m_stop_reply_response.data().substr(0, 3); // "Txx"

            new_resp.parse(signal);

            auto payload = m_stop_reply_response.data().substr(3); // ;-delimited list

            size_t start = 0;
            size_t end = 0;

            while ((end = payload.find(';', start)) != string_view::npos) {
                if (payload.compare(start, 7, "thread:", 7) == 0) {
                    string str = "thread:" + m_current_thread_id + ";";
                    new_resp.parse(str.data(), str.size());
                } else {
                    auto sub = payload.substr(start, end - start + 1);
                    new_resp.parse(sub);
                }
                start = end + 1;
            }

            new_resp.finalize();

            clog << "new stop-packet reply response" << new_resp.data() << '\n';

            m_conn.push_internal_response(std::move(new_resp));
            m_stop_reply_response.reset();
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
    // internal to handle set-current thread commands and so on

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

        m_threading.current_thread_tcb = val;
        if (val)
            m_threading.current_thread_id = make_threadid(threadtcb_t(val));

        //if (m_threading.threads_count == 0 || m_threading.current_thread_tcb == 0)
        // always add current execution
        {
            // Either : No RTOS threads - there is always at least the current execution though
            // OR     : No current thread - all threads suspended - show the current execution
            //          of idling

            m_threading.threads_count++;

            add_current_execution();

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

        info.thread_tcb = val;
        info.thread_id = make_threadid(val);

        get_list_elem_thread_name();
    });

}

void target_mb_freertos::get_list_elem_thread_name()
{
    auto & info = m_threading.info.back();
    const auto address = info.thread_tcb + THREAD_NAME_OFFSET;

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

        if (m_threading.current_thread_tcb == info.thread_tcb) {
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

void target_mb_freertos::add_current_execution()
{
    auto& info = m_threading.info.emplace_back();

    info.thread_tcb = THREAD_ID_CURRENT_EXECUTION;
    info.thread_id = make_threadid(THREAD_ID_CURRENT_EXECUTION);
    info.exists = true;
    info.name = "Current Execution";
}

void target_mb_freertos::make_empty_threading()
{
    m_threading = threading();
    m_threading.threads_count = 1u;
    m_threading.current_thread_tcb = THREAD_ID_CURRENT_EXECUTION;
    m_threading.current_thread_id = make_threadid(m_threading.current_thread_tcb);
    m_current_thread_id = m_threading.current_thread_id;
    add_current_execution();
}

std::string target_mb_freertos::make_threadid(threadtcb_t tcb)
{
    // RTOS with multiple processes? Really??? Assume only one process and forman thread-id according GDB thread-id
    // conventions:
    //  https://www.sourceware.org/gdb/onlinedocs/gdb.html#thread_002did-syntax
    // Note, PID and TID - in HEX form. Only one special value: -1 is in decimal one
    string id = m_multiprocess ? "p1." : "";

    if (tcb < 0) {
        id += to_string(-1);
    } else {
        char buf[sizeof(tcb)*2 + 1]{0};
        auto size = snprintf(buf, sizeof(buf), "%" PRIx64, tcb);
        id.append(buf, size_t(size));
    }

    return id;
}

threadtcb_t target_mb_freertos::parse_threadid(std::string_view threadid)
{
    // skip "p1."
    auto view = m_multiprocess ? threadid.substr(3) : threadid;
    try {
        if (view.at(0) == '-')
            return THREAD_ID_ALL;
        string str(view);
        return static_cast<threadtcb_t>(stoull(str, nullptr, 16));
    } catch (...) {
        return THREAD_ID_INVALID;
    }
}

