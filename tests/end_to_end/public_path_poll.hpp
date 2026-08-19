#pragma once

/// \file
/// \brief The Qt half of the shared public-path wait.
///
/// \details Separate from `public_path_wait.hpp` on purpose: the report
/// formatting is pure standard C++, so a Qt-free executable can compile it and
/// assert what a timed-out wait says without needing a `QApplication` or an event
/// loop. Only the polling needs Qt, and only that lives here.
///
/// Two courses used to carry textually identical copies of this loop, differing
/// only in whether the budget was a parameter. Both are gone; the budget is always
/// a parameter and every call site keeps the value it had.

#include "public_path_wait.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

#include <chrono>

namespace bpr::e2e {
	/// \brief The budget the queue course has always used.
	inline constexpr auto default_queue_budget = std::chrono::milliseconds{ 20'000 };

	/// \brief The budget the agent-invocation course has always used.
	inline constexpr auto default_agent_budget = std::chrono::milliseconds{ 30'000 };

	/// \brief The longer budget six agent-invocation sites have always used.
	inline constexpr auto extended_agent_budget = std::chrono::milliseconds{ 45'000 };

	/// \brief Polls `aPredicate` until it holds or the budget elapses.
	///
	/// \details On timeout the predicate is evaluated once more against
	/// `aTerms`, so the report describes the state at the moment waiting stopped
	/// rather than an arbitrary earlier poll. `aTerms` is left untouched on
	/// success, so a caller cannot accidentally print a report for a wait that
	/// worked.
	///
	/// \returns Whether the predicate held before the budget elapsed.
	template <typename Predicate>
	[[nodiscard]] auto wait_until(
		wait_terms&                     aTerms,
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget) -> bool
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		while (timer.elapsed() < aBudget.count()) {
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			auto ignored = wait_terms{ };
			if (aPredicate(ignored)) {
				return true;
			}
			QThread::msleep(5);
		}
		// One final evaluation, into the caller's recorder, so what is reported is
		// the state that ran out of time.
		return aPredicate(aTerms);
	}

	/// \brief The same wait for a condition with nothing worth reporting.
	template <typename Predicate>
	[[nodiscard]] auto wait_until_plain(
		Predicate aPredicate, const std::chrono::milliseconds aBudget) -> bool
	{
		auto terms = wait_terms{ };
		return wait_until(
			terms,
			[&aPredicate](wait_terms&) { return aPredicate(); },
			aBudget);
	}
} // namespace bpr::e2e
