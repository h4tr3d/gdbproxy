#include "target_mb_freertos.h"

target_mb_freertos::target_mb_freertos(connection &conn)
    : m_conn(conn)
{
}

bool target_mb_freertos::process_request(const gdb_packet &/*req_pkt*/)
{
    return false;
}
