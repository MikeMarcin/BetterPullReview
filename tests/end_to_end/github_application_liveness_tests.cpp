#include "bpr/app/github_application.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>

namespace {
	[[noreturn]] void fail(const std::string_view aMessage)
	{
		std::cerr << "FAILED: " << aMessage << '\n';
		std::exit(EXIT_FAILURE);
	}

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			fail(aMessage);
		}
	}

	// The condition BPR-0215 and BPR-0217 both crashed on, forced rather than
	// waited for. Five runs of the command that produced the second crash never
	// reproduced it, because it needs a tick still armed at the moment `exec()`
	// returns and that configuration does not reliably create one. Destroying
	// the application with a tick deliberately armed creates it every time:
	// `~github_application` clears the token, then `~implementation` pumps
	// `processEvents` in its own shutdown loop and delivers the tick while
	// libc++ has already nulled the owning handle.
	void test_a_tick_armed_before_destruction_reaches_nothing()
	{
		auto dataRoot = QTemporaryDir{ };
		expect(dataRoot.isValid(), "the run owns a writable data root");
		// Canonical, not merely absolute. The composition refuses a data root
		// that is not its own canonical form, and on macOS a temporary
		// directory lives under `/var`, which is a symlink to `/private/var`.
		// The fixture resolves the same way for the same reason.
		const auto canonicalDataRoot
			= QFileInfo{ dataRoot.path() }.canonicalFilePath();
		expect(!canonicalDataRoot.isEmpty(),
			   "the data root resolves to a canonical path");

		auto application = std::make_unique<bpr::app::github_application>(
			bpr::app::github_application_config{
				.ApplicationDataRoot
				= std::filesystem::path{ canonicalDataRoot.toStdU16String() } });
		// A token born expired would make every production poller a permanent
		// no-op and every assertion below vacuously true.
		expect(!application->liveness().expired(),
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
			&application->window(),
			[&delivered, &expiredAtDelivery, &reentered, &app = *application,
			 alive = application->liveness()] {
				delivered         = true;
				expiredAtDelivery = alive.expired();
				if (alive.expired()) {
					return;
				}
				reentered = true;
				(void)app.status();
				(void)app.window();
			});

		application.reset();

		// Anti-vacuity anchor. The guard is only load-bearing while something
		// still delivers the tick, so a refactor that removes the pump inside
		// `~implementation` must fail here rather than quietly turn the other
		// two assertions into statements about an event that never happened.
		expect(delivered,
			   "the pump inside ~implementation delivers a tick armed before "
			   "destruction");
		expect(expiredAtDelivery,
			   "the liveness token has expired by the time that tick runs");
		expect(!reentered,
			   "the tick reaches nothing back inside the destroyed application");
	}
}

auto main(int argc, char** argv) -> int
{
	const QApplication qtApplication{ argc, argv };
	test_a_tick_armed_before_destruction_reaches_nothing();
	std::cout << "github-application-liveness-tests:pass\n";
	return EXIT_SUCCESS;
}
