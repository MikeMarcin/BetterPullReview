#include "capabilities.hpp"

namespace probe {
	struct domain_clock
	{
		static constexpr int now()
		{
			return 7;
		}
	};

	auto aliased_clock()
	{
		return probe_alias::wall_clock::now();
	}

	auto macro_clock()
	{
		return BPR_PROBE_WALL_NOW();
	}

	int supplied_value()
	{
		return domain_clock::now();
	}
}
