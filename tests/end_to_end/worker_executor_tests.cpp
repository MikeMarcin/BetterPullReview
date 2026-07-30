#include "bpr/app/worker_executor.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
[[noreturn]] void fail(const std::string_view message)
{
    std::cerr << "FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(const bool condition, const std::string_view message)
{
    if(!condition)
    {
        fail(message);
    }
}

void test_fifo_drain_and_failure_containment()
{
    bpr::app::worker_executor executor;
    auto order = std::vector<int>{};
    auto order_mutex = std::mutex{};
    auto completed = std::promise<void>{};
    auto future = completed.get_future();

    executor.post(
        [&order, &order_mutex]
        {
            const std::scoped_lock lock{order_mutex};
            order.push_back(1);
        });
    executor.post([] { throw std::runtime_error{"synthetic effect failure"}; });
    executor.post(
        [&order, &order_mutex, &completed]
        {
            {
                const std::scoped_lock lock{order_mutex};
                order.push_back(2);
            }
            completed.set_value();
        });

    expect(
        future.wait_for(std::chrono::seconds{5})
            == std::future_status::ready,
        "worker reaches the terminal task");
    executor.close_and_drain();
    expect(order == std::vector{1, 2} && executor.idle()
               && !executor.accepting(),
           "worker preserves FIFO around an isolated task failure");

    auto rejected = false;
    try
    {
        executor.post([] {});
    }
    catch(const std::runtime_error&)
    {
        rejected = true;
    }
    expect(rejected, "closed worker rejects new effects");
    executor.close_and_drain();
}

void test_destructor_drains_accepted_work()
{
    auto calls = std::atomic<std::size_t>{0U};
    {
        bpr::app::worker_executor executor;
        for(auto index = 0U; index < 256U; ++index)
        {
            executor.post([&calls] { calls.fetch_add(1U); });
        }
    }
    expect(calls.load() == 256U,
           "destruction executes every accepted effect exactly once");
}
}

auto main() -> int
{
    test_fifo_drain_and_failure_containment();
    test_destructor_drains_accepted_work();
    std::cout << "worker-executor-tests:pass\n";
    return EXIT_SUCCESS;
}
