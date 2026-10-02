#include "cycle_session.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/core/sha256.hpp"
#include "tour_cancellation.hpp"
#include <QRandomGenerator>
#include <future>
#include <vector>
namespace bpr::app {
	auto cycle_command_cancelled() noexcept -> bool
	{
		return tour_cancelled();
	}

	auto cycle_session::open(const std::filesystem::path& aBase, const std::string& aProfile) -> tour_cli_result<std::unique_ptr<cycle_session>>
	{
		// Admission blocks on private storage, so it runs where store calls do.
		auto lane     = worker_executor{ };
		auto task     = std::packaged_task<tour_cli_result<profile_admission>()>{ [&] { return admit_profile(aBase, aProfile); } };
		auto admitted = task.get_future();
		lane.post([&task] { task(); });
		auto admission = admitted.get();
		lane.close_and_drain();
		if (!admission) {
			return std::unexpected(admission.error());
		}
		if (!admission->Lease) {
			return std::unexpected(tour_cli_error{ "capability_unavailable",
												   "Another BetterPullReview process owns this profile, and cycle commands cannot reach it yet. Close the desktop (or the other bpr command) and "
												   "retry; nothing was opened a second time.",
												   5 });
		}
		return std::unique_ptr<cycle_session>{ new cycle_session{ std::move(*admission) } };
	}

	cycle_session::cycle_session(profile_admission aAdmission)
		: Admission{ std::move(aAdmission) }
		, Source{ adapters::workspace::make_local_workspace_source() }
		, Runtime{ std::make_unique<application::review_cycle_runtime>(adapters::sqlite::make_review_cycle_store(Admission.Root, Admission.Profile), Effects, Decisions) }
	{
		// Operation identities must not repeat across runs of the same
		// profile; the global generator is securely seeded.
		for (auto& byte : Seed) {
			byte = static_cast<std::uint8_t>(QRandomGenerator::global()->bounded(256U));
		}
	}

	cycle_session::~cycle_session()
	{
		Runtime->shutdown();
		while (!Runtime->is_quiescent()) {
			Decisions.drain();
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
		Decisions.drain();
		Effects.close_and_drain();
		Decisions.drain();
		// The runtime and its store go before the lease is released.
		Runtime.reset();
	}

	auto cycle_session::operation() -> application::cycle_operation_id
	{
		auto bytes = std::vector<std::uint8_t>(Seed.begin(), Seed.end());
		++Counter;
		for (auto shift = 56; shift >= 0; shift -= 8) {
			bytes.push_back(static_cast<std::uint8_t>(Counter >> static_cast<unsigned>(shift)));
		}
		return application::cycle_operation_id{ core::sha256(bytes) };
	}
}
