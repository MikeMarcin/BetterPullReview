#pragma once

#include "bpr/core/model.hpp"

namespace bpr::application
{
class request_port
{
public:
    virtual ~request_port() = default;

    virtual void submit(const core::host_request& request) = 0;
    virtual void cancel(core::request_id request) = 0;
};
}
