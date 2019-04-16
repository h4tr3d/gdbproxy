#pragma once

#include <memory>
#include <unordered_map>
#include <functional>

#include "gdb-packet.h"
#include "target.h"

class target_null : public target
{
public:
    virtual ~target_null() = default;
    virtual bool process_request(const gdb_packet& req_pkt) final {
        return false;
    };
};

