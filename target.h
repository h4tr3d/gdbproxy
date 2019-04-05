#pragma once

#include <memory>
#include <unordered_map>
#include <functional>

#include "gdb-packet.h"

class target
{
public:
    virtual ~target();
    virtual bool process_request(const gdb_packet& req_pkt) = 0;
};

