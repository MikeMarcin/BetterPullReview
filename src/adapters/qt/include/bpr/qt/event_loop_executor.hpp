#pragma once

#include "bpr/application/port.hpp"

#include <QObject>

namespace bpr::qt
{
class event_loop_executor final : public application::decision_executor
{
public:
    void post(std::function<void()> task) override;

private:
    QObject Context;
};
}
