/// \file
/// \brief [[BPR-2004]], [[BPR-2005]], RFC-BPR-0103. What a running turn is
/// doing, its helpers, and when it goes quiet.

#include "bpr/core/agent_activity.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto doing(const core::agent_doing aDoing, std::string aSubject = { }) -> core::agent_activity_update
	{
		auto update    = core::agent_activity_update{ };
		update.Kind    = core::agent_activity_kind::doing;
		update.Doing   = aDoing;
		update.Subject = std::move(aSubject);
		return update;
	}

	[[nodiscard]] auto helper(const core::agent_activity_kind aKind, std::string aId, std::string aSubject = { }, const core::agent_helper_state aEnded = core::agent_helper_state::finished,
							  const core::agent_doing aDoing = core::agent_doing::thinking) -> core::agent_activity_update
	{
		auto update    = core::agent_activity_update{ };
		update.Kind    = aKind;
		update.Helper  = std::move(aId);
		update.Subject = std::move(aSubject);
		update.Ended   = aEnded;
		update.Doing   = aDoing;
		return update;
	}

	void test_three_helpers_one_turn()
	{
		// AC02's script: three helpers start, the agent's message ends, they
		// report back one by one.
		using kind = core::agent_activity_kind;
		auto turn  = core::agent_turn_activity{ };
		expect(turn.Doing == core::agent_doing::starting, "a turn starts with nothing from the runtime");
		turn = core::apply_agent_activity(turn, doing(core::agent_doing::thinking), 1'000U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_started, "a", "Rendering path"), 2'000U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_started, "b", "Tests"), 2'100U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_started, "c", "Public interface"), 2'200U);
		turn = core::apply_agent_activity(turn, doing(core::agent_doing::waiting), 3'000U);
		expect(turn.Helpers.size() == 3U && core::outstanding_agent_helpers(turn) == 3U, "three rows, all running");
		turn = core::apply_agent_activity(turn, helper(kind::helper_progress, "a", "frame_pool.cpp", { }, core::agent_doing::reading), 4'000U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_progress, "c", "git diff", { }, core::agent_doing::running), 4'100U);
		expect(turn.Helpers[0].Last == "frame_pool.cpp" && turn.Helpers[2].Last == "git diff", "each row says what it last did");
		expect(turn.FilesRead == 1U && turn.Commands == 1U, "a helper's read and command are counted");
		turn = core::apply_agent_activity(turn, helper(kind::helper_ended, "b", "2 findings"), 5'000U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_ended, "a"), 6'000U);
		turn = core::apply_agent_activity(turn, helper(kind::helper_ended, "c", { }, core::agent_helper_state::failed), 7'000U);
		expect(core::outstanding_agent_helpers(turn) == 0U, "none outstanding");
		expect(turn.Helpers[1].State == core::agent_helper_state::finished && turn.Helpers[1].Last == "2 findings" && turn.Helpers[1].EndedMs == 5'000U, "finished with its last word and its end");
		expect(turn.Helpers[2].State == core::agent_helper_state::failed, "a failed helper says so");
		// A late progress for an ended helper changes nothing.
		const auto before = turn;
		turn              = core::apply_agent_activity(turn, helper(kind::helper_progress, "b", "late"), 7'500U);
		expect(turn.Helpers == before.Helpers, "an ended helper stays ended");
		const auto tally = core::tally_agent_helpers(turn);
		expect(tally.Started == 3U && tally.Finished == 2U && tally.Failed == 1U && tally.Abandoned == 0U, "the tally");
	}

	void test_an_unreported_start_is_adopted_and_ending_abandons()
	{
		using kind = core::agent_activity_kind;
		auto turn  = core::agent_turn_activity{ };
		turn       = core::apply_agent_activity(turn, helper(kind::helper_progress, "x", "reading a.txt"), 500U);
		expect(turn.Helpers.size() == 1U && turn.Helpers[0].StartedMs == 500U, "progress for an unknown helper starts it");
		turn = core::apply_agent_activity(turn, helper(kind::helper_started, "x", "Count lines"), 600U);
		expect(turn.Helpers[0].Description == "Count lines", "and its description arrives later");
		turn = core::end_agent_activity(turn, 9'000U);
		expect(turn.Helpers[0].State == core::agent_helper_state::abandoned && turn.Helpers[0].EndedMs == 9'000U, "the turn ended under it: abandoned");
		expect(core::tally_agent_helpers(turn).Abandoned == 1U, "counted abandoned");
	}

	void test_the_bounds()
	{
		using kind = core::agent_activity_kind;
		auto turn  = core::agent_turn_activity{ };
		for (std::size_t index = 0U; index < core::maximum_agent_helpers + 3U; ++index) {
			turn = core::apply_agent_activity(turn, helper(kind::helper_started, std::to_string(index), "h"), index);
		}
		expect(turn.Helpers.size() == core::maximum_agent_helpers && turn.UnlistedHelpers == 3U, "rows are bounded; the rest are counted");
		expect(core::tally_agent_helpers(turn).Started == core::maximum_agent_helpers + 3U, "and all are tallied as started");
		turn = core::apply_agent_activity(turn, doing(core::agent_doing::reading, std::string(239U, 'b') + "\xC3\xA9"), 70'000U);
		expect(turn.Subject.size() == 239U, "a subject is cut before a character, never inside one");
	}

	void test_silence_and_quiet()
	{
		// AC05, AC06. "Last activity" restarts on every frame; the longest
		// silence is measured between frames.
		auto turn = core::agent_turn_activity{ };
		turn      = core::note_agent_frame(turn, 1'000U);
		turn      = core::note_agent_frame(turn, 61'000U);
		turn      = core::note_agent_frame(turn, 62'000U);
		expect(turn.LastFrameMs == 62'000U && turn.LongestSilenceMs == 60'000U, "the last frame and the longest silence");
		turn = core::note_agent_frame(turn, 50'000U);
		expect(turn.LastFrameMs == 62'000U, "time does not run backwards");
		expect(!core::agent_run_is_quiet(119'999U, std::nullopt), "not quiet before two minutes");
		expect(core::agent_run_is_quiet(120'000U, std::nullopt), "quiet at two minutes");
		expect(!core::agent_run_is_quiet(300'000U, 10'000U), "Keep waiting puts the notice away");
		expect(core::agent_run_is_quiet(300'000U, 120'000U), "and it returns after another two minutes of silence");
		expect(!core::agent_run_is_quiet(5'000U, 200'000U), "a frame clears it");
	}

	void test_names()
	{
		expect(core::stable_name(core::agent_doing::waiting) == "waiting" && core::stable_name(core::agent_helper_state::abandoned) == "abandoned", "stable names");
	}
}

auto main() -> int
{
	test_three_helpers_one_turn();
	test_an_unreported_start_is_adopted_and_ending_abandons();
	test_the_bounds();
	test_silence_and_quiet();
	test_names();
	std::cout << "agent-activity-tests:pass\n";
	return EXIT_SUCCESS;
}
