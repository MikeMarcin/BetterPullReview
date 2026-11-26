#include "bpr/core/chat_transcript.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto trimmed(const std::string_view aText) -> std::string
		{
			constexpr auto blank = std::string_view{ " \t\r\n" };
			const auto     first = aText.find_first_not_of(blank);
			if (first == std::string_view::npos) {
				return { };
			}
			return std::string{ aText.substr(first, aText.find_last_not_of(blank) - first + 1U) };
		}

		[[nodiscard]] auto outcome_of(const tour_agent_run_state aState) noexcept -> agent_turn_outcome
		{
			switch (aState) {
			case tour_agent_run_state::answered:  return agent_turn_outcome::answered;
			case tour_agent_run_state::cancelled: return agent_turn_outcome::stopped;
			case tour_agent_run_state::running:
			case tour_agent_run_state::failed:    return agent_turn_outcome::exited_with_error;
			}
			return agent_turn_outcome::exited_with_error;
		}

		/// What a run that did not finish says, and the run's own words for
		/// Copy.
		void say_why_not(const tour_agent_run& aRun, chat_turn& aTurn)
		{
			const auto ended    = std::ranges::find(aRun.Entries, tour_transcript_kind::ended, &tour_transcript_entry::Kind);
			aTurn.FailureDetail = ended == aRun.Entries.end() ? std::string{ } : trimmed(ended->Text);
			const auto why      = std::string_view{ aTurn.FailureDetail }.substr(0U, aTurn.FailureDetail.find('\n'));
			aTurn.Failure       = aRun.State == tour_agent_run_state::cancelled ? std::string{ "Stopped. Nothing in the tour changed." }
				: why.empty()                                                   ? std::string{ "It didn't finish." }
																				: "Didn't finish: " + std::string{ why };
		}

		/// The attachments a whole tour's turn carries after its ending.
		void hang_whole_tour_parts(const tour_agent_session& aSession, const tour_agent_run& aRun, const tour_thread_facts& aFacts, chat_turn& aTurn)
		{
			const auto answered = aRun.State == tour_agent_run_state::answered;
			if (!aRun.FocusSet.empty()) {
				aTurn.Attachments.push_back({ .Key = "focus", .Payload = chat_tour_focus{ .Run = aRun.Id, .Focus = aRun.FocusSet, .Undone = aRun.FocusUndone, .Undoable = tour_focus_undoable(aSession, aRun) }, .Place = chat_attachment_place::after_ending });
			}
			if (aFacts.Proposal) {
				auto revision = chat_tour_revision{ };
				revision.Run  = aRun.Id;
				for (std::size_t index = 0U; index < aFacts.Proposal->Items.size(); ++index) {
					if (aFacts.Proposal->Items[index].Run == aRun.Id) {
						revision.Items.push_back(index);
						revision.Decisions.push_back(aFacts.Proposal->Items[index].Decision);
					}
				}
				if (!revision.Items.empty()) {
					aTurn.Attachments.push_back({ .Key = "revision", .Payload = std::move(revision), .Place = chat_attachment_place::after_ending });
				}
			}
			if (answered && !aRun.Proposed.Refused.empty()) {
				aTurn.Attachments.push_back({ .Key = "refused", .Payload = chat_tour_refused{ .Parts = aRun.Proposed.Refused }, .Place = chat_attachment_place::after_ending });
			}
		}

		/// Each part a part's turn proposed, in the proposal's order.
		void hang_proposed_parts(const tour_agent_run& aRun, const tour_proposal& aProposal, chat_turn& aTurn)
		{
			for (std::size_t index = 0U; index < aProposal.Items.size(); ++index) {
				if (aProposal.Items[index].Run == aRun.Id) {
					aTurn.Attachments.push_back({ .Key = "proposal:" + std::to_string(index), .Payload = chat_tour_proposal{ .Index = index, .Item = aProposal.Items[index] }, .Place = chat_attachment_place::after_ending });
				}
			}
		}

		[[nodiscard]] auto turn_of(const tour_agent_session& aSession, const tour_agent_run& aRun, const bool aWholeTour, const bool aLatest, const tour_thread_facts& aFacts) -> chat_turn
		{
			auto turn     = chat_turn{ };
			turn.Id       = "tour-run:" + std::to_string(aRun.Id.value());
			turn.Question = aRun.Question;
			turn.Steered  = aRun.Steered;
			if (aRun.State == tour_agent_run_state::running) {
				turn.Running = true;
				return turn;
			}
			const auto answered = aRun.State == tour_agent_run_state::answered;
			if (answered) {
				turn.Reply = aRun.Reply;
			} else {
				say_why_not(aRun, turn);
			}
			auto       ending = chat_ending{ };
			const auto who    = aFacts.Agents.find(aRun.Id.value());
			ending.Agent      = who == aFacts.Agents.end() || who->second.empty() ? std::string{ tour_chat_default_agent } : who->second;
			ending.Outcome    = outcome_of(aRun.State);
			if (aRun.Seconds) {
				ending.Seconds = static_cast<std::int64_t>(*aRun.Seconds);
			}
			ending.Cost         = aRun.Cost;
			turn.Ending         = std::move(ending);
			turn.Actions        = decide_agent_turn_actions({
				.Outcome = outcome_of(aRun.State),
				.HasText = answered ? !aRun.Reply.empty() : !turn.FailureDetail.empty(),
				.Latest  = aLatest,
				.Working = aFacts.Running,
			});
			turn.Actions.Delete = turn.Actions.Delete && aFacts.Deletes;
			if (const auto worked = aFacts.Activity.find(aRun.Id.value()); worked != aFacts.Activity.end()) {
				turn.Activity = worked->second;
			}
			// RFC-BPR-0120. A local draft's check is what the reply is.
			if (answered && aRun.Check) {
				turn.Attachments.push_back({ .Key = "check", .Payload = chat_tour_check{ .Run = aRun.Id, .Check = *aRun.Check, .Blocked = aRun.CheckBlocked, .Latest = aRun.CheckLatest, .Scratch = aFacts.CheckScratch, .Builds = aFacts.CheckBuilds }, .Place = chat_attachment_place::after_reply });
			}
			if (aWholeTour) {
				hang_whole_tour_parts(aSession, aRun, aFacts, turn);
			}
			// [[BPR-1937]], RFC-BPR-0100 AC02. A change described only in words
			// changes nothing: under the newest answer that proposed nothing,
			// the way to ask for the change itself.
			if (aLatest && answered && aRun.Proposed == tour_proposal_tally{ } && aRun.FocusSet.empty()) {
				turn.Attachments.push_back({ .Key = "unchanged", .Payload = chat_tour_unchanged{ .Run = aRun.Id }, .Place = chat_attachment_place::after_ending });
			}
			if (!aWholeTour && aFacts.Proposal) {
				hang_proposed_parts(aRun, *aFacts.Proposal, turn);
			}
			return turn;
		}
	}

	auto project_tour_thread(const tour_agent_session& aSession, const tour_thread& aThread, const tour_thread_facts& aFacts) -> chat_transcript
	{
		auto       transcript = chat_transcript{ };
		const auto runs       = thread_runs(aSession, aThread);
		const auto wholeTour  = aThread.Kind == tour_thread_kind::tour;
		for (const auto* const run : runs) {
			transcript.Turns.push_back(turn_of(aSession, *run, wholeTour, run == runs.back(), aFacts));
		}
		// [[BPR-1930]] AC3. Feedback given while the draft runs, waiting for
		// its proposal: the author's words, and when they go.
		if (wholeTour && aSession.Waiting) {
			auto waiting         = chat_turn{ };
			waiting.Id           = "tour-waiting";
			waiting.Question     = *aSession.Waiting;
			waiting.QuestionNote = "Sent when the draft is ready, to revise it.";
			transcript.Turns.push_back(std::move(waiting));
		}
		return transcript;
	}
}
