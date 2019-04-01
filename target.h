#pragma once

#include "gdb-packet.h"

class target
{
public:
    virtual ~target();
    virtual bool process_request(const gdb_packet& req_pkt) = 0;
};

