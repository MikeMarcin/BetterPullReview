#include "bpr/application/manual_executor.hpp"

#include <stdexcept>
#include <utility>

namespace bpr::application
{
void manual_executor::post(std::function<void()> task)
{
    Tasks.push_back(std::move(task));
}

auto manual_executor::pending() const noexcept -> std::size_t
{
    return Tasks.size();
}

void manual_executor::run(const std::size_t index)
{
    if(index >= Tasks.size())
    {
        throw std::out_of_range{"Manual executor task index is out of range"};
    }

    auto task = std::move(Tasks[index]);
    Tasks.erase(Tasks.begin() + static_cast<std::ptrdiff_t>(index));
    task();
}

void manual_executor::run_next()
{
    run(0);
}

void manual_executor::drain()
{
    while(!Tasks.empty())
    {
        run_next();
    }
}
}
