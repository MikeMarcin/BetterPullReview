#ifndef BPR_TESTS_SUPPORT_SOURCE_FIND_EVENT_TURNS_HPP
#define BPR_TESTS_SUPPORT_SOURCE_FIND_EVENT_TURNS_HPP

#include <QCoreApplication>

#include <cstddef>

namespace bpr::tests {
	inline constexpr auto source_find_wait_milliseconds  = 20000;
	inline constexpr auto source_find_navigation_actions = std::size_t{ 1000U };

	/// The public course supplies its wall clock and event turn. The contract
	/// control supplies a logical clock to exercise the same deadline without
	/// making a deliberately missing terminal cost twenty real seconds.
	template <class TCondition, class TElapsed, class TPump>
	auto wait_for_source_find_condition(TCondition aCondition, TElapsed aElapsed, TPump aPump) -> bool
	{
		while (!aCondition() && aElapsed() < source_find_wait_milliseconds) {
			aPump();
		}
		return aCondition();
	}

	/// All of the original public navigation gestures, in their original order.
	template <class TTrigger>
	void dispatch_source_find_navigation(TTrigger aTrigger)
	{
		for (auto index = std::size_t{ 0U }; index < source_find_navigation_actions; ++index) {
			aTrigger();
		}
	}
}

#endif
