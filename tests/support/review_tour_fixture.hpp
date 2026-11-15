#ifndef BPR_TESTS_SUPPORT_REVIEW_TOUR_FIXTURE_HPP
#define BPR_TESTS_SUPPORT_REVIEW_TOUR_FIXTURE_HPP
#include "bpr/core/review_tour.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
namespace bpr::tests {
	/// [[BPR-2014]]. Every stop of `aTour` accepted as it is now, at its head.
	inline auto accept_tour_fixture(core::review_tour aTour) -> core::review_tour
	{
		const auto snapshot = aTour;
		for (auto& stop : aTour.Stops) {
			if (core::tour_stop_accept_need(stop) == core::tour_accept_need::nothing) {
				stop.Acceptance = core::accept_tour_stop(snapshot, stop, { });
			}
		}
		return aTour;
	}
	inline auto review_tour_fixture() -> core::review_tour
	{
		auto bytes                         = std::array<std::uint8_t, 32>{ };
		bytes[0]                           = 1;
		const auto              comparison = core::file_comparison_digest{ bytes };
		const auto              commit     = *core::parse_git_commit_oid("1111111111111111111111111111111111111111");
		core::tour_source_range source{ { comparison, core::diff_side::right, 3 }, { comparison, core::diff_side::right, 5 }, commit, { }, "src/item.cpp" };
		auto                    tour = core::review_tour{ .Id = core::tour_draft_id{ 1 }, .Owner = { core::bpr_profile_id{ 1 }, core::git_host_id{ 1 }, core::host_authority_fingerprint{ bytes }, core::github_account_id{ 3 }, core::repository_id{ 4 }, core::pull_request_id{ 5 } }, .Base = commit, .Head = commit, .Problem = "Crash when reading data that's been concurrently deleted.", .Solution = "Retain a snapshot.", .ProblemBaseline = core::tour_field_baseline{ "## Problem\r\nOriginal\r\n", "PR_fixture", "version1", std::pair<std::size_t, std::size_t>{ 12, 20 }, 1 }, .Stops = { { .Id = core::tour_stop_id{ 1 }, .Title = "Why", .Body = "Keep **raw** text\n<details>hidden</details>", .Original = source, .Mapping = core::tour_mapping::exact_original, .Related = { source } } }, .LastStopId = 1 };
		// [[BPR-2014]]. The fixture's stop is accepted as it stands, as a
		// publishable stop was before acceptance existed.
		return accept_tour_fixture(std::move(tour));
	}
	inline auto tour_edit(const core::review_tour& aTour, std::string aText) -> core::tour_command
	{
		return { .Draft = aTour.Id, .Owner = aTour.Owner, .ExpectedRevision = aTour.Revision, .Kind = core::tour_command_kind::problem, .Text = std::move(aText) };
	}
}
#endif
