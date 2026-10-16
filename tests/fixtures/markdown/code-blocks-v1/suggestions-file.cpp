#include "cache.hpp"

#include <mutex>
#include <utility>

namespace sample {
	void cache::refresh()
	{
		auto next  = read_snapshot();
		auto guard = std::lock_guard{ Mutex };
		Snapshot   = std::move(next);
		Generation += 1;
	}

	auto cache::generation() const -> std::uint64_t
	{
		auto guard = std::lock_guard{ Mutex };
		return Generation;
	}

	void cache::clear()
	{
		auto guard = std::lock_guard{ Mutex };
		Snapshot   = { };
		Generation = 0;
	}
} // namespace sample
