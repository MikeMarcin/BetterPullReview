#include "cycle_session.hpp"
#include "bpr/adapters/sqlite/review_cycle_store.hpp"
#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/adapters/xdiff/xdiff_line_matcher.hpp"
#include "bpr/core/sha256.hpp"
#include <vector>
namespace bpr::app {
	auto cycle_operation_seed(const std::string_view aOperation, const std::uint64_t aStep) -> std::array<std::uint8_t, 32>
	{
		constexpr std::string_view domain{ "bpr.cycle.operation.v1" };
		auto                       bytes = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0U);
		bytes.insert(bytes.end(), aOperation.begin(), aOperation.end());
		bytes.push_back(0U);
		for (auto shift = 56; shift >= 0; shift -= 8) {
			bytes.push_back(static_cast<std::uint8_t>(aStep >> static_cast<unsigned>(shift)));
		}
		return core::sha256(bytes);
	}

	cycle_session::cycle_session(profile_admission aAdmission, const std::array<std::uint8_t, 32>& aSeed, std::function<bool()> aCancelled)
		: Admission{ std::move(aAdmission) }
		, Cancelled{ std::move(aCancelled) }
		, Source{ adapters::workspace::make_local_workspace_source() }
		, Runtime{ std::make_unique<application::review_cycle_runtime>(adapters::sqlite::make_review_cycle_store(Admission.Root, Admission.Profile), Effects, Decisions,
																	   nullptr, adapters::xdiff::make_xdiff_line_matcher()) }
		, Seed{ aSeed }
	{
	}

	cycle_session::cycle_session(std::filesystem::path aRoot, const core::bpr_profile_id aProfile, const std::array<std::uint8_t, 32>& aSeed, std::function<bool()> aCancelled)
		: cycle_session{ profile_admission{ .Root = std::move(aRoot), .Profile = aProfile, .Lease = { } }, aSeed, std::move(aCancelled) }
	{
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
		// The runtime and its store go before any lease is released.
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
