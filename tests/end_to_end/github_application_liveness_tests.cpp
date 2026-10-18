#include "bpr/app/github_application.hpp"

#include "../support/test_expectation.hpp"
#include "private_data_root.hpp"
#include "public_path_poll.hpp"

#include <QApplication>
#include <QByteArray>
#include <QTemporaryDir>
#include <QTimer>
#include <QtEnvironmentVariables>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace {
	using bpr::tests::expect;
	using bpr::tests::fail;

	// Starts the application with no fixture over a private canonical data
	// root and hands it to `aInspect`. The startup handle and the data root
	// outlive `aInspect`, so whatever it destroys is destroyed first.
	template <typename Inspect>
	void with_fixtureless_application(Inspect&& aInspect)
	{
		auto dataRoot = QTemporaryDir{ };
		expect(dataRoot.isValid(), "the run owns a writable data root");
		// [[BPR-0832]], [[BPR-1050]]. Canonical, because the composition refuses
		// a data root that is not its own canonical form, and on macOS a
		// temporary directory lives under `/var`, which is a symlink to
		// `/private/var`. Owner-private by the stores' own definition, and one
		// owned level below the temporary directory, because the agent workspace
		// neighbourhood beside the root trusts only a private parent.
		const auto privateRoot = bpr::e2e::private_data_root(dataRoot).value_or(std::filesystem::path{ });
		expect(!privateRoot.empty(), "the data root is owner-private");

		std::unique_ptr<bpr::app::github_application> application;
		auto                                          startupTerminal = false;
		auto                                          startup         = bpr::app::github_application::start(
			bpr::app::github_application_config{
				.ApplicationDataRoot = privateRoot },
			[&](auto aResult) {
				startupTerminal = true;
				if (aResult) {
					application = std::move(*aResult);
				}
			});
		expect(bpr::e2e::wait_until_plain([&] {
				   return startupTerminal && startup && startup->is_quiescent();
			   },
										  bpr::e2e::default_queue_budget),
			   "application startup reaches a quiescent terminal");
		expect(application != nullptr, "application startup succeeds");
		std::forward<Inspect>(aInspect)(application);
	}

	// The condition BPR-0215 and BPR-0217 both crashed on, forced rather than
	// waited for. Five runs of the command that produced the second crash never
	// reproduced it, because it needs a tick still armed at the moment `exec()`
	// returns and that configuration does not reliably create one. Destroying
	// the application with a tick deliberately armed creates it every time. At
	// the time `~implementation` pumped `processEvents` after libc++ had nulled
	// the owning handle; since BPR-0705 the shutdown drain in
	// `~github_application` pumps over windows that are already shutting down.
	// Either way the token must be clear before the pump runs. BPR-0705 cleared
	// it after the drain, and BPR-0827 moved it back in front.
	void test_a_tick_armed_before_destruction_reaches_nothing()
	{
		with_fixtureless_application([](auto& aPplication) {
			// A token born expired would make every production poller a permanent
			// no-op and every assertion below vacuously true.
			expect(!aPplication->liveness().expired(),
				   "a usable application reports itself usable");

			auto delivered         = false;
			auto expiredAtDelivery = false;
			auto reentered         = false;
			// The production pollers' shape: zero-millisecond single shot, the
			// window as receiver context, the liveness token captured by value.
			// `status()` and `window()` are the two members the BPR-0215 and
			// BPR-0217 traces name as the faulting frame.
			QTimer::singleShot(
				0,
				&aPplication->window(),
				[&delivered, &expiredAtDelivery, &reentered, &app = *aPplication,
				 alive = aPplication->liveness()] {
					delivered         = true;
					expiredAtDelivery = alive.expired();
					if (alive.expired()) {
						return;
					}
					reentered = true;
					(void)app.status();
					(void)app.window();
				});

			aPplication.reset();

			// Anti-vacuity anchor. The guard is only load-bearing while something
			// still delivers the tick, so a refactor that removes the pump inside
			// destruction must fail here rather than quietly turn the other two
			// assertions into statements about an event that never happened.
			expect(delivered,
				   "the shutdown drain inside destruction delivers a tick armed "
				   "before destruction");
			expect(expiredAtDelivery,
				   "the liveness token has expired by the time that tick runs");
			expect(!reentered,
				   "the tick reaches nothing back inside the destroyed application");
		});
	}

	// [[BPR-0802]]. An in-process composition root answers the automated-test
	// boundary question the way the application binary does, because the
	// composition reads the marker itself. This file's `start` call used to be
	// the counterexample: `src/app/main.cpp` was the only reader, so a test
	// composing the application here was outside the boundary whatever its
	// CTest row said, and bound the real browser opener.
	//
	// Only the inside answer is asserted here. The outside answer is the one
	// the tree's single designated outside row,
	// `application_composition_capabilities_product_run`, exists to show, and
	// that row is visible to the manifest gate; erasing the marker inside this
	// process would not be.
	void test_an_in_process_composition_is_inside_the_boundary()
	{
		with_fixtureless_application([](auto& aPplication) {
			const auto status = aPplication->status();
			expect(status.InsideAutomatedTestBoundary,
				   "a fixture-less in-process composition inside the marker "
				   "reports that it is inside the boundary");
			expect(status.DeviceAuthorizationBrowser
					   == bpr::app::composed_device_browser::recording,
				   "a fixture-less in-process composition inside the marker "
				   "binds the recording browser, not the system opener");
			expect(!status.ComposedCliReadAvailable,
				   "a fixture-less in-process composition inside the marker "
				   "offers no GitHub CLI read");
			aPplication.reset();
		});
	}
}

auto main(int argc, char** argv) -> int
try {
	// [[BPR-0802]]. Every composition this process starts is inside the
	// automated test boundary, whatever its environment. The CTest row already
	// carries the marker from the registration seam; setting it here as well,
	// before anything composes and never clearing it, means this binary run by
	// hand cannot compose the real browser opener or the real GitHub CLI
	// factory either.
	if (!qputenv("BPR_AUTOMATED_TEST_BOUNDARY", QByteArrayLiteral("1"))) {
		fail("the automated-test boundary marker can be set");
	}
	const QApplication qtApplication{ argc, argv };
	// The boundary check runs first: each test ends the run on its first
	// failure, and the safety property should not wait behind the destructor.
	test_an_in_process_composition_is_inside_the_boundary();
	std::cout << "github-application-boundary-tests:pass\n";
	test_a_tick_armed_before_destruction_reaches_nothing();
	std::cout << "github-application-liveness-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
