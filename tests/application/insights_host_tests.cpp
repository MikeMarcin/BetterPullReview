#include "../support/test_expectation.hpp"
#include "bpr/application/insights_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <algorithm>
#include <iostream>

namespace {
	class local_storage final : public bpr::application::insights_storage_port
	{
	public:
		std::vector<bpr::core::insights_work_sample>   Work;
		void                                           record_work(const std::vector<bpr::core::insights_work_sample>& aSamples) override { Work.insert(Work.end(), aSamples.begin(), aSamples.end()); }
		int                                            Measurements = 0;
		std::vector<bpr::core::insights_event>         Recorded;
		std::vector<bpr::core::insights_energy_sample> Energy;
		void                                           record_energy(const std::vector<bpr::core::insights_energy_sample>& aSamples) override { Energy.insert(Energy.end(), aSamples.begin(), aSamples.end()); }
		void                                           record(const std::vector<bpr::core::insights_event>& aEvents, std::optional<bpr::core::insights_scope>) override { Recorded.insert(Recorded.end(), aEvents.begin(), aEvents.end()); }
		auto                                           measure() -> bpr::application::insights_snapshot override
		{
			++Measurements;
			bpr::application::insights_snapshot snapshot;
			snapshot.TotalBytes = 12345;
			snapshot.Storage.push_back({ "insights.sqlite3", 12345, std::nullopt, false });
			return snapshot;
		}
	};
	class metrics final : public bpr::application::process_metrics_port
	{
	public:
		std::vector<std::optional<bpr::core::insights_process_counters>> Values;
		std::size_t                                                      At = 0;
		auto                                                             read() -> std::optional<bpr::core::insights_process_counters> override { return Values.at(At++); }
	};

}

int main()
{
	using namespace bpr;
	using tests::expect;
	application::manual_executor   effects;
	application::manual_executor   decisions;
	auto                           storage      = std::make_shared<local_storage>();
	int                            publications = 0;
	application::insights_snapshot last;
	auto                           host = std::make_shared<application::insights_host>(storage, effects, decisions, [&](const auto& aSnapshot) { ++publications; last = aSnapshot; });
	host->refresh(false, "Idle");
	host->refresh(true, "Must be skipped");
	expect(effects.pending() == 1 && storage->Measurements == 0 && publications == 0, "one pending snapshot; no work on the decision lane");
	effects.drain();
	expect(storage->Measurements == 0 && publications == 0, "a non-storage view performs no measurement and publishes only on completion lane");
	decisions.drain();
	expect(publications == 1 && !last.TotalBytes, "no measured zero is invented");
	host->refresh(true, "Idle");
	effects.drain();
	decisions.drain();
	expect(storage->Measurements == 1 && last.TotalBytes == 12345 && last.StorageUnixSeconds != 0, "Storage reaches the projection through effects and completion");
	for (int refresh = 0; refresh < 3; ++refresh) {
		host->refresh(true, "Idle");
		effects.drain();
		decisions.drain();
	}
	expect(storage->Measurements == 1 && last.TotalBytes == 12345, "three refreshes inside a minute reuse the measured snapshot");
	host->refresh(true, "Idle");
	effects.drain();
	const auto beforeShutdown = publications;
	host->shutdown();
	decisions.drain();
	host->refresh(true, "Idle");
	expect(host->quiescent() && publications == beforeShutdown && effects.pending() == 0, "shutdown drains the exact terminal without a late publication or new effect");
	auto recorder   = std::make_shared<application::insights_recorder>(effects);
	auto problemLog = application::observe_insights_problems({ }, recorder);
	static_cast<void>(problemLog->record({ .Outcome = application::diagnostic_outcome::failed }));
	recorder->attach(storage);
	effects.drain();
	expect(effects.pending() == 0 && storage->Recorded.size() == 1 && storage->Recorded.front().Kind == core::insights_event_kind::problem, "problem tallies work without a diagnostic logger and drain one bounded effect task");

	expect(application::local_insights_day(1793521800) == 20758 && application::local_insights_day(1793525400) == 20758, "both occurrences of 1:30 in the fall DST overlap share one local day");
	expect(application::local_insights_day(1793606399) == 20758 && application::local_insights_day(1793606400) == 20759, "the 25-hour day ends at local midnight, not a fixed UTC boundary");
	expect(application::local_insights_day(1772963940) == 20520 && application::local_insights_day(1772964060) == 20520, "the spring-forward gap stays in its local day");

	auto                            native = std::make_shared<metrics>();
	core::insights_process_counters reading;
	reading.UnixMicroseconds   = 1770000000000000LL;
	reading.SteadyMicroseconds = 1000000;
	reading.CpuMicroseconds    = 1000;
	reading.Wakeups            = 0;
	native->Values.push_back(reading);
	reading.UnixMicroseconds += 10000000;
	reading.SteadyMicroseconds += 10000000;
	reading.CpuMicroseconds += 500;
	reading.Wakeups = 2;
	native->Values.push_back(reading);
	reading.UnixMicroseconds += 20000000;
	reading.SteadyMicroseconds += 20000000;
	reading.CpuMicroseconds += 1500;
	reading.Wakeups = 5;
	native->Values.push_back(reading);
	core::insights_work_sample work;
	work.Kind                 = core::insights_work_kind::latency;
	work.DurationMicroseconds = 250000;
	recorder->record_work(work);
	effects.drain();
	expect(storage->Work.size() == 1 && storage->Work.front().At > 0, "passive Work intake supplies its clock and drains off-lane");

	auto sampler = std::make_shared<application::insights_energy_sampler>(native, recorder, effects);
	sampler->observe(false);
	const auto backgroundChild = application::capture_insights_child(recorder, 0);
	sampler->observe(true);
	backgroundChild.record(12345);
	sampler->observe(false);
	effects.drain();
	expect(storage->Energy.size() == 3 && !storage->Energy[0].Foreground, "a background-launched child retains its state after foreground activation");
	const auto bgInterval = std::ranges::find_if(storage->Energy, [](const auto& aSample) { return aSample.Samples && !aSample.Foreground; });
	const auto fgInterval = std::ranges::find_if(storage->Energy, [](const auto& aSample) { return aSample.Samples && aSample.Foreground; });
	expect(bgInterval != storage->Energy.end() && bgInterval->WallMicroseconds == 10000000 && bgInterval->CpuMicroseconds == 500 && fgInterval != storage->Energy.end() && fgInterval->WallMicroseconds == 20000000 && fgInterval->CpuMicroseconds == 1500, "queued transitions split both intervals before switching attribution");
	expect(storage->Energy[0].ChildCpuMicroseconds[0] == 12345 && storage->Energy[0].Samples == 0, "child CPU stays separate from application counters and wall time");
	const auto currentChild = application::capture_insights_child(recorder, 1);
	currentChild.record(std::nullopt);
	effects.drain();
	expect(storage->Energy.back().UnmeasuredChildren[1] == 1, "unavailable native child counters remain explicit rather than measured zero");
	sampler->shutdown();
	const auto finalTasks = effects.pending();
	sampler->observe(true);
	expect(effects.pending() == finalTasks && finalTasks == 1, "shutdown flushes one final interval and admits no further sampling");
	effects.drain();
	expect(application::insights_energy_sampler::interval_seconds(true) == 30 && application::insights_energy_sampler::interval_seconds(false) == 60, "foreground and background schedules match the RFC");

	std::cout << "insights-host:pass\n";
}
