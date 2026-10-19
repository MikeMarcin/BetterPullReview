#include "../support/test_expectation.hpp"
#include "bpr/application/insights_host.hpp"
#include "bpr/application/manual_executor.hpp"

#include <iostream>

namespace {
	class local_storage final : public bpr::application::insights_storage_port
	{
	public:
		int                                    Measurements = 0;
		std::vector<bpr::core::insights_event> Recorded;
		void                                   record(const std::vector<bpr::core::insights_event>& aEvents, std::optional<bpr::core::insights_scope>) override { Recorded.insert(Recorded.end(), aEvents.begin(), aEvents.end()); }
		auto                                   measure() -> bpr::application::insights_snapshot override
		{
			++Measurements;
			bpr::application::insights_snapshot snapshot;
			snapshot.TotalBytes = 12345;
			snapshot.Storage.push_back({ "insights.sqlite3", 12345, std::nullopt, false });
			return snapshot;
		}
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

	std::cout << "insights-host:pass\n";
}
