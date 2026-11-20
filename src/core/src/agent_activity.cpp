#include "bpr/core/agent_activity.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		/// Cuts at a UTF-8 boundary, never inside a character.
		[[nodiscard]] auto bounded(std::string aText, const std::size_t aBytes = maximum_agent_activity_subject_bytes) -> std::string
		{
			if (aText.size() <= aBytes) {
				return aText;
			}
			auto cut = aBytes;
			while (cut > 0U && (static_cast<unsigned char>(aText[cut]) & 0xC0U) == 0x80U) {
				--cut;
			}
			aText.resize(cut);
			return aText;
		}

		/// [[BPR-2285]]. A command is kept whole; any other subject is a name.
		[[nodiscard]] constexpr auto subject_bytes(const agent_doing aDoing) noexcept -> std::size_t
		{
			return aDoing == agent_doing::running ? maximum_agent_command_bytes : maximum_agent_activity_subject_bytes;
		}

		[[nodiscard]] auto find_helper(agent_turn_activity& aActivity, const std::string_view aId) -> agent_helper*
		{
			const auto found = std::ranges::find(aActivity.Helpers, aId, &agent_helper::Id);
			return found == aActivity.Helpers.end() ? nullptr : &*found;
		}

		/// The helper named `aId`, started at `aAtMs` when it is new; null
		/// past the bound, where it is counted only.
		[[nodiscard]] auto adopt_helper(agent_turn_activity& aActivity, const std::string& aId, const std::uint64_t aAtMs) -> agent_helper*
		{
			if (auto* const known = find_helper(aActivity, aId)) {
				return known;
			}
			if (aActivity.Helpers.size() >= maximum_agent_helpers) {
				++aActivity.UnlistedHelpers;
				return nullptr;
			}
			auto helper      = agent_helper{ };
			helper.Id        = aId;
			helper.StartedMs = aAtMs;
			aActivity.Helpers.push_back(std::move(helper));
			return &aActivity.Helpers.back();
		}
	}

	auto stable_name(const agent_doing aDoing) noexcept -> std::string_view
	{
		switch (aDoing) {
		case agent_doing::starting:   return "starting";
		case agent_doing::thinking:   return "thinking";
		case agent_doing::reading:    return "reading";
		case agent_doing::searching:  return "searching";
		case agent_doing::running:    return "running";
		case agent_doing::editing:    return "editing";
		case agent_doing::using_tool: return "using_tool";
		case agent_doing::writing:    return "writing";
		case agent_doing::waiting:    return "waiting";
		}
		return "unknown";
	}

	auto stable_name(const agent_helper_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case agent_helper_state::running:   return "running";
		case agent_helper_state::finished:  return "finished";
		case agent_helper_state::failed:    return "failed";
		case agent_helper_state::abandoned: return "abandoned";
		}
		return "unknown";
	}

	auto note_agent_frame(agent_turn_activity aActivity, const std::uint64_t aAtMs) -> agent_turn_activity
	{
		if (aAtMs > aActivity.LastFrameMs) {
			aActivity.LongestSilenceMs = std::max(aActivity.LongestSilenceMs, aAtMs - aActivity.LastFrameMs);
			aActivity.LastFrameMs      = aAtMs;
		}
		return aActivity;
	}

	auto apply_agent_activity(agent_turn_activity aActivity, const agent_activity_update& aUpdate, const std::uint64_t aAtMs) -> agent_turn_activity
	{
		aActivity = note_agent_frame(std::move(aActivity), aAtMs);
		// Each read and each command counts once, when it starts, the
		// agent's own and its helpers' alike.
		if (aUpdate.Kind == agent_activity_kind::doing || aUpdate.Kind == agent_activity_kind::helper_progress) {
			if (aUpdate.Doing == agent_doing::reading) {
				++aActivity.FilesRead;
			} else if (aUpdate.Doing == agent_doing::running) {
				++aActivity.Commands;
				// [[BPR-2007]]. The folded line lists what ran.
				if (auto command = bounded(aUpdate.Subject, maximum_agent_command_bytes); !command.empty() && aActivity.CommandsRun.size() < maximum_agent_activity_listed
					&& !std::ranges::contains(aActivity.CommandsRun, command)) {
					aActivity.CommandsRun.push_back(std::move(command));
				}
			}
		}
		switch (aUpdate.Kind) {
		case agent_activity_kind::doing:
			aActivity.Doing   = aUpdate.Doing;
			aActivity.Subject = bounded(aUpdate.Subject, subject_bytes(aUpdate.Doing));
			break;
		case agent_activity_kind::helper_started:
			if (auto* const helper = adopt_helper(aActivity, aUpdate.Helper, aAtMs); helper != nullptr && helper->Description.empty()) {
				helper->Description = bounded(aUpdate.Subject);
			}
			break;
		case agent_activity_kind::helper_progress:
			if (auto* const helper = adopt_helper(aActivity, aUpdate.Helper, aAtMs); helper != nullptr && helper->State == agent_helper_state::running && !aUpdate.Subject.empty()) {
				helper->Last = bounded(aUpdate.Subject, subject_bytes(aUpdate.Doing));
			}
			break;
		case agent_activity_kind::helper_ended:
			if (auto* const helper = adopt_helper(aActivity, aUpdate.Helper, aAtMs); helper != nullptr && helper->State == agent_helper_state::running) {
				helper->State   = aUpdate.Ended == agent_helper_state::running ? agent_helper_state::finished : aUpdate.Ended;
				helper->EndedMs = aAtMs;
				if (!aUpdate.Subject.empty()) {
					helper->Last = bounded(aUpdate.Subject);
				}
			}
			break;
		}
		return aActivity;
	}

	auto end_agent_activity(agent_turn_activity aActivity, const std::uint64_t aAtMs) -> agent_turn_activity
	{
		for (auto& helper : aActivity.Helpers) {
			if (helper.State == agent_helper_state::running) {
				helper.State   = agent_helper_state::abandoned;
				helper.EndedMs = aAtMs;
			}
		}
		return aActivity;
	}

	auto outstanding_agent_helpers(const agent_turn_activity& aActivity) noexcept -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count(aActivity.Helpers, agent_helper_state::running, &agent_helper::State));
	}

	auto tally_agent_helpers(const agent_turn_activity& aActivity) noexcept -> agent_helper_tally
	{
		auto tally    = agent_helper_tally{ };
		tally.Started = static_cast<std::uint32_t>(aActivity.Helpers.size()) + aActivity.UnlistedHelpers;
		for (const auto& helper : aActivity.Helpers) {
			switch (helper.State) {
			case agent_helper_state::running:   break;
			case agent_helper_state::finished:  ++tally.Finished; break;
			case agent_helper_state::failed:    ++tally.Failed; break;
			case agent_helper_state::abandoned: ++tally.Abandoned; break;
			}
		}
		return tally;
	}

	auto agent_run_is_quiet(const std::uint64_t aSinceFrameMs, const std::optional<std::uint64_t> aSinceKeptWaitingMs) noexcept -> bool
	{
		return aSinceFrameMs >= agent_quiet_after_ms && (!aSinceKeptWaitingMs || *aSinceKeptWaitingMs >= agent_quiet_after_ms);
	}
}
