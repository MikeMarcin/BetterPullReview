#include "../../src/app/src/profile_window_layout_port.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "bpr/application/manual_executor.hpp"
#include "bpr/application/workbench_application_host.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <semaphore>
#include <thread>

namespace {
	using namespace bpr;
	using tests::expect;

	class no_observations final : public application::workbench_observation_port
	{
	public:
		auto assemble(const application::workbench_observation_request& aRequest, std::stop_token)
			-> application::workbench_file_observation override
		{
			return { .Context = aRequest.Context, .Authority = aRequest.Authority };
		}
	};

	class recording_projections final : public application::workbench_projection_port
	{
	public:
		void        publish(const application::workbench_projection&) override { ++Publications; }
		void        fail(const application::workbench_failure_publication&) override { ++Failures; }
		std::size_t Publications = 0;
		std::size_t Failures     = 0;
	};

	/// Holds the real profile service's serialization lock until released.
	class held_layout_service final
	{
	public:
		explicit held_layout_service(const std::shared_ptr<app::profile_window_layout_service>& aService)
			: Worker{ [this, aService] {
				Result = aService->perform<application::workbench_settings_result<void>>(core::bpr_window_id{ 1 }, { }, [this](auto&) -> application::workbench_settings_result<void> {
					Entered.release();
					ReleasedInTime = Release.try_acquire_for(std::chrono::seconds{ 10 });
					return { };
				});
			} }
		{
			expect(Entered.try_acquire_for(std::chrono::seconds{ 5 }), "real layout service entered the held effect");
		}
		~held_layout_service()
		{
			if (Worker.joinable()) {
				Release.release();
				Worker.join();
			}
		}
		void release()
		{
			Release.release();
			Worker.join();
			expect(Result.has_value() && ReleasedInTime, "held service released without a timeout or storage refusal");
		}

	private:
		std::binary_semaphore                        Entered{ 0 };
		std::binary_semaphore                        Release{ 0 };
		application::workbench_settings_result<void> Result;
		bool                                         ReleasedInTime = false;
		std::jthread                                 Worker;
	};

	auto wait_for_attempt(const app::profile_window_layout_port& aPort, std::size_t aAttempt) -> bool
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{ 5 };
		while (aPort.writes_attempted() < aAttempt && std::chrono::steady_clock::now() < deadline) {
			std::this_thread::yield();
		}
		return aPort.writes_attempted() == aAttempt;
	}

	void test_refusal_and_recovery_are_distinct_from_in_flight()
	{
		const auto root = tests::sqlite_contract::scratch_root{ "layout-status" };
		expect(adapters::sqlite::create_storage_root_owner_private(root.file("data")), "private data root exists");
		auto service = std::make_shared<app::profile_window_layout_service>(root.file("data"), std::vector{ core::bpr_window_id{ 1 } });
		auto port    = app::profile_window_layout_port{ service, core::bpr_window_id{ 1 } };
		expect(!port.write_failed() && !port.last_write_failure() && port.writes_in_flight() == 0, "unused port reports neither refusal nor an outstanding store");
		auto stop = std::stop_source{ };
		stop.request_stop();
		const auto cancelled = port.store({ }, stop.get_token());
		expect(!cancelled && cancelled.error().Failure == application::workbench_settings_failure::cancelled
				   && port.write_failed() && port.last_write_failure() == application::workbench_settings_failure::cancelled
				   && port.writes_attempted() == 1 && port.writes_completed() == 0 && port.writes_in_flight() == 0,
			   "completed refusal has a reason but no outstanding write, despite unequal counters");

		auto       held         = held_layout_service{ service };
		auto       result       = application::workbench_settings_result<void>{ };
		auto       writer       = std::jthread{ [&] { result = port.store({ .FilesWidth = 480 }, { }); } };
		const auto entered      = wait_for_attempt(port, 2);
		const auto priorRefusal = port.write_failed() && port.last_write_failure() == application::workbench_settings_failure::cancelled;
		const auto inFlight     = port.writes_in_flight();
		held.release();
		writer.join();
		expect(entered && priorRefusal && inFlight == 1, "new in-flight store retains the previous terminal refusal separately");
		expect(result.has_value() && !port.write_failed() && !port.last_write_failure()
				   && port.writes_attempted() == 2 && port.writes_completed() == 1 && port.writes_in_flight() == 0,
			   "successful recovery clears the refusal and finishes the outstanding store");
	}

	void test_orderly_shutdown_drains_an_in_flight_write_and_the_latest_layout()
	{
		const auto root = tests::sqlite_contract::scratch_root{ "layout-drain" };
		expect(adapters::sqlite::create_storage_root_owner_private(root.file("data")), "private data root exists");
		auto service      = std::make_shared<app::profile_window_layout_service>(root.file("data"), std::vector{ core::bpr_window_id{ 1 } });
		auto port         = app::profile_window_layout_port{ service, core::bpr_window_id{ 1 } };
		auto observations = no_observations{ };
		auto projections  = recording_projections{ };
		auto effects      = application::manual_executor{ };
		auto lane         = application::manual_executor{ };
		auto host         = application::workbench_application_host{ { }, observations, port, effects, lane, projections, core::application_session_id{ 1 } };
		auto held         = held_layout_service{ service };
		host.resize(core::workbench_drawer::files, 400);
		auto       writer                 = std::jthread{ [&] { effects.run_next(); } };
		const auto entered                = wait_for_attempt(port, 1);
		const auto inFlightWithoutFailure = port.writes_in_flight() == 1 && !port.write_failed() && !port.last_write_failure();
		host.resize(core::workbench_drawer::files, 500);
		host.resize(core::workbench_drawer::files, 600);
		host.shutdown_orderly();
		host.resize(core::workbench_drawer::files, 700);
		const auto waiting = host.is_closing() && !host.is_quiescent() && host.layout().FilesWidth == 600;
		held.release();
		writer.join();
		expect(entered && inFlightWithoutFailure && waiting, "Quit waits for an actual in-flight write without labelling it a refusal");
		expect(!host.is_quiescent() && port.writes_in_flight() == 0 && port.writes_completed() == 1,
			   "finished effect still needs its decision terminal before shutdown can drain");
		lane.drain();
		expect(!host.is_quiescent() && effects.pending() == 1, "terminal admits only the latest coalesced write");
		effects.drain();
		lane.drain();
		const auto loaded = port.load({ });
		expect(host.is_quiescent() && loaded && loaded->FilesWidth == 600
				   && port.writes_attempted() == 2 && port.writes_completed() == 2 && port.writes_in_flight() == 0
				   && !port.write_failed() && projections.Failures == 0,
			   "orderly drain preserves the final accepted layout in real SQLite with no cleanup refusal");
	}

	void test_orderly_refusal_finishes_without_retry()
	{
		const auto root = tests::sqlite_contract::scratch_root{ "layout-refusal" };
		// A regular file cannot be the private storage root.
		const auto unavailable = root.file("unavailable");
		{
			std::ofstream file{ unavailable };
			file << "not a directory";
		}
		auto service      = std::make_shared<app::profile_window_layout_service>(unavailable, std::vector{ core::bpr_window_id{ 1 } });
		auto port         = app::profile_window_layout_port{ service, core::bpr_window_id{ 1 } };
		auto observations = no_observations{ };
		auto projections  = recording_projections{ };
		auto effects      = application::manual_executor{ };
		auto lane         = application::manual_executor{ };
		auto host         = application::workbench_application_host{ { }, observations, port, effects, lane, projections, core::application_session_id{ 1 } };
		host.resize(core::workbench_drawer::files, 550);
		host.shutdown_orderly();
		effects.drain();
		lane.drain();
		expect(host.is_quiescent() && projections.Failures == 1 && effects.pending() == 0
				   && port.write_failed() && port.last_write_failure() == application::workbench_settings_failure::unavailable
				   && port.writes_attempted() == 1 && port.writes_completed() == 0 && port.writes_in_flight() == 0,
			   "actual refusal drains once, preserves its unavailable reason and never masquerades as in flight");
	}
}

auto main() -> int
try {
	test_refusal_and_recovery_are_distinct_from_in_flight();
	test_orderly_shutdown_drains_an_in_flight_write_and_the_latest_layout();
	test_orderly_refusal_finishes_without_retry();
	std::cout << "profile-window-layout-drain:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
