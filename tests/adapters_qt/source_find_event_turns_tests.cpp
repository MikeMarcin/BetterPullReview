#include "../support/source_find_event_turns.hpp"
#include "../support/test_expectation.hpp"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMetaObject>
#include <QObject>

#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {
	using namespace bpr;
	using tests::expect;

	void queued_projection_does_not_starve_a_ready_terminal()
	{
		QObject receiver;
		QAction next;
		auto    actions            = std::size_t{ 0U };
		auto    projections        = std::size_t{ 0U };
		auto    elapsed            = std::int64_t{ 0 };
		auto    observedGeneration = std::uint64_t{ 0U };
		auto    waiting            = false;
		auto    postsAccepted      = true;
		QObject::connect(&next, &QAction::triggered, &receiver, [&] {
			++actions;
			postsAccepted = QMetaObject::invokeMethod(&receiver, [&] {
				++projections;
				if (waiting) {
					// Model R157's approximately 24-second first batch with
					// 1,000 queued callbacks, each costing 24 logical ms. The
					// production batch's event composition was not measured.
					elapsed += 24;
				} }, Qt::QueuedConnection) && postsAccepted;
		});
		tests::dispatch_source_find_navigation([&] { next.trigger(); });
		expect(actions == tests::source_find_navigation_actions, "all 1,000 public navigation actions remain present");
		waiting          = true;
		postsAccepted    = QMetaObject::invokeMethod(&receiver, [&] {
			// This terminal arrives during the initial posted-event batch,
			// just as the real worker's sub-millisecond answer did in R157.
			postsAccepted = QMetaObject::invokeMethod(&receiver, [&] { observedGeneration = 18U; }, Qt::QueuedConnection) && postsAccepted; }, Qt::QueuedConnection) && postsAccepted;
		const auto ready = tests::wait_for_source_find_condition([&] { return observedGeneration == 18U; }, [&] { return elapsed; }, [&] {
			QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
			++elapsed; });
		std::cout << "source-find-event-turns: actions=" << actions << " projections=" << projections
				  << " elapsed_ms=" << elapsed << " generation=" << observedGeneration << '\n';
		expect(postsAccepted && projections == tests::source_find_navigation_actions, "every exact projection and terminal post is retained");
		expect(ready && observedGeneration == 18U && elapsed < tests::source_find_wait_milliseconds,
			   "a queued terminal receives an event turn within the unchanged 20-second allowance");
	}

	void missing_and_wrong_terminals_keep_the_original_deadline()
	{
		QObject    receiver;
		auto       elapsed            = std::int64_t{ 0 };
		auto       turns              = std::size_t{ 0U };
		auto       observedGeneration = std::uint64_t{ 0U };
		const auto pump               = [&] {
			QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
			++elapsed;
			++turns;
		};
		const auto missing = tests::wait_for_source_find_condition([&] { return observedGeneration == 18U; }, [&] { return elapsed; }, pump);
		expect(!missing && elapsed == 20000 && turns == 20000U, "a missing terminal still exhausts exactly the original 20-second allowance");
		elapsed = 0;
		turns   = 0U;
		expect(QMetaObject::invokeMethod(&receiver, [&] { observedGeneration = 17U; }, Qt::QueuedConnection), "the wrong-generation control is queued");
		const auto wrong = tests::wait_for_source_find_condition([&] { return observedGeneration == 18U; }, [&] { return elapsed; }, pump);
		expect(!wrong && observedGeneration == 17U && elapsed == 20000 && turns == 20000U, "another generation cannot satisfy the exact terminal predicate");
		expect(QMetaObject::invokeMethod(&receiver, [&] { observedGeneration = 18U; }, Qt::QueuedConnection), "the late exact terminal is queued");
		const auto late = tests::wait_for_source_find_condition([&] { return observedGeneration == 18U; }, [&] { return elapsed; }, pump);
		expect(!late && observedGeneration == 17U && turns == 20000U, "an expired allowance gets no extra event turn");
		QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
		expect(observedGeneration == 18U && !late, "later diagnostic delivery cannot turn the captured refusal into success");
	}
}

int main(int aArgc, char** aArgv)
{
	QApplication application{ aArgc, aArgv };
	try {
		queued_projection_does_not_starve_a_ready_terminal();
		missing_and_wrong_terminals_keep_the_original_deadline();
	} catch (const bpr::tests::expectation_failed&) {
		return 1;
	}
	std::cout << "SOURCE_FIND_EVENT_TURNS_OK\n";
	return 0;
}
