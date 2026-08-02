#include "bpr/application/manual_executor.hpp"

#include <stdexcept>
#include <utility>

namespace bpr::application {
	void manual_executor::post(std::function<void()> aTask)
	{
		Tasks.push_back(std::move(aTask));
	}

	auto manual_executor::pending() const noexcept -> std::size_t
	{
		return Tasks.size();
	}

	void manual_executor::run(const std::size_t aIndex)
	{
		if (aIndex >= Tasks.size()) {
			throw std::out_of_range{ "Manual executor task index is out of range" };
		}

		auto task = std::move(Tasks[aIndex]);
		Tasks.erase(Tasks.begin() + static_cast<std::ptrdiff_t>(aIndex));
		task();
	}

	void manual_executor::run_next()
	{
		run(0);
	}

	void manual_executor::drain()
	{
		while (!Tasks.empty()) {
			run_next();
		}
	}
}
