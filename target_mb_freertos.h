#pragma once

#include "target.h"
#include "proxy-conn.hpp"

class target_mb_freertos : public target
{
public:
    target_mb_freertos(connection& conn);


    // target interface
public:
    bool process_request(const gdb_packet &req_pkt) override;

private:
    connection& m_conn;
};
