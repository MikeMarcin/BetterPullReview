#pragma once

#include "bpr/application/port.hpp"

#include <cstddef>
#include <deque>

namespace bpr::application
{
class manual_executor final : public decision_executor
{
public:
    void post(std::function<void()> task) override;

    [[nodiscard]] auto pending() const noexcept -> std::size_t;
    void run(std::size_t index);
    void run_next();
    void drain();

private:
    std::deque<std::function<void()>> Tasks;
};
}
