/// \file
/// \brief [[BPR-2286]] (owner, 2026-11-19). Ask follow-up on a finding (a
/// report's or an Agent tab card's) and Ask about this on a thread or a draft
/// composer: the item read from what the window holds, the follow-up host's
/// rounds shown in the item's side chat beside the report or the code
/// ([[BPR-2322]], owner 2026-11-20: "something akin to the side chat from the
/// tour onto the report tab"), and Use putting the proposal in place (the
/// finding's suggested comment, the draft's field, the thread's reply
/// composer). Nothing is posted: the reviewer publishes, as ever.

#include "github_application/window_composition.hpp"

#include "bpr/application/review_brief.hpp"
#include "bpr/application/skill_guidance.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/qt/agent_report_view.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/ask_menu.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/item_side_chat.hpp"

#include <array>
#include <string_view>

namespace bpr::app {
#if defined(BPR_AGENT_INTEGRATIONS)
	namespace {
		/// A finding card's follow-up key: its delivery and place, which a
		/// report's finding at the card's place shares.
		[[nodiscard]] auto card_key(const std::uint64_t aSession, const std::size_t aOrdinal) -> std::string { return "finding:" + std::to_string(aSession) + ":" + std::to_string(aOrdinal); }

		/// A report finding's key: its card's, or the report's turn and row.
		[[nodiscard]] auto report_key(const std::uint64_t aTurn, const qt::report_draft& aDraft) -> std::string
		{
			if (aDraft.Card) {
				return card_key(aDraft.Card->Session, aDraft.Card->Ordinal);
			}
			return "report:" + std::to_string(aTurn) + ":" + std::to_string(aDraft.Outline);
		}

		/// A whole decimal number, or none.
		[[nodiscard]] auto number_of(const std::string_view aDigits) -> std::optional<std::uint64_t>
		{
			if (aDigits.empty() || aDigits.size() > 19U) {
				return std::nullopt;
			}
			auto value = std::uint64_t{ 0U };
			for (const auto digit : aDigits) {
				if (digit < '0' || digit > '9') {
					return std::nullopt;
				}
				value = value * 10U + static_cast<std::uint64_t>(digit - '0');
			}
			return value;
		}

		/// The delivery and place a card key names.
		[[nodiscard]] auto card_of(const std::string_view aKey) -> std::optional<std::pair<std::uint64_t, std::size_t>>
		{
			constexpr auto prefix = std::string_view{ "finding:" };
			if (!aKey.starts_with(prefix)) {
				return std::nullopt;
			}
			const auto rest    = aKey.substr(prefix.size());
			const auto colon   = rest.find(':');
			const auto session = colon == std::string_view::npos ? std::nullopt : number_of(rest.substr(0U, colon));
			const auto ordinal = colon == std::string_view::npos ? std::nullopt : number_of(rest.substr(colon + 1U));
			if (!session || !ordinal) {
				return std::nullopt;
			}
			return std::pair{ *session, static_cast<std::size_t>(*ordinal) };
		}
		/// Why no round was sent: the agent chosen cannot read the code.
		[[nodiscard]] auto follow_up_agent_words() -> QString
		{
			return QStringLiteral("The side chat reads the pull request's code, so it needs an agent with a read-only mode. Choose Claude or Codex with the agent picker in the window's footer. Nothing was sent.");
		}
	} // namespace

	/// [[BPR-2370]] (owner: "Why does the report side chat not have access to
	/// the code? How am I supposed to ask serious follow-ups if it cant read
	/// the workspace?"). The item side chat's own session, composed as the
	/// tour session is: its rounds are fresh, read-only runs in the pull
	/// request's workspace, and nothing of them reaches the Agent tab or the
	/// tour.
	void github_application::implementation::compose_follow_up_session(const std::vector<core::agent_profile>& aProfiles, const std::vector<core::agent_host_policy>& aPolicies)
	{
		FollowUpEffects = std::make_unique<worker_executor>();
		FollowUpProcess = std::make_unique<adapters::agent::agent_process_launcher>();
	#if defined(BPR_BUILD_TESTING)
		FollowUpProcessProbe = std::make_unique<composition_agent_process_launcher_probe>(*FollowUpProcess);
		// [[BPR-2177]]. A tests-on build's rounds keep to the fixture agent.
		if (Fixture) {
			FollowUpProcessProbe->set_admission(fixture_agent_admission());
		}
		auto& launcher = static_cast<application::agent_process_port&>(*FollowUpProcessProbe);
	#else
		auto& launcher = static_cast<application::agent_process_port&>(*FollowUpProcess);
	#endif
		FollowUpClaudeProcess    = std::make_unique<application::wrapped_claude_process>(launcher, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		FollowUpLocatedProcess   = std::make_unique<application::located_agent_process>(*FollowUpClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper, locate_agent_program());
		FollowUpEntryProcess     = std::make_unique<application::agent_entry_process>(launcher, *FollowUpLocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper, locate_claude(),
																					  locate_agent_program());
		FollowUpDiagnosedProcess = std::make_unique<application::diagnosed_agent_process>(*FollowUpEntryProcess, DiagnosticLog);
		FollowUpProjection       = std::make_unique<tour_session_projection>(
			[](const application::agent_invocation_projection_identity&) { },
			[](const application::agent_invocation_projection_identity&, const application::prepared_workspace&) { },
			[this](const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult) { follow_up_round_finished(aIdentity, aResult); },
			[this](const std::string_view aReason) {
				answer_follow_up_round(std::unexpected{ aReason == "agent_invocation_already_running" ? std::string{ "The side chat is still answering another item. Ask again when it ends; nothing was sent." }
																									  : "The side chat could not start (" + std::string{ aReason } + "). Nothing was sent." });
			});
		FollowUpSession = std::make_unique<application::agent_invocation_host>(
			aProfiles, aPolicies, AgentWorkspaceNeighbourhood, *this, *EpochHost, *FollowUpDiagnosedProcess,
			[](const std::string& aName) -> std::optional<std::string> {
				if (!qEnvironmentVariableIsSet(aName.c_str())) {
					return std::nullopt;
				}
				return qEnvironmentVariable(aName.c_str()).toStdString();
			},
			*FollowUpEffects, DecisionLane, *FollowUpProjection, nullptr, NativeAgentSessions.get(), DiagnosticLog.get());
		// A round asked while the last one retired starts once it has.
		FollowUpSession->set_retired_observer([this] {
			if (!Closing) {
				start_follow_up_round();
			}
		});
	}

	void github_application::implementation::answer_follow_up_round(std::expected<std::string, std::string> aAnswer)
	{
		if (!FollowUpRound) {
			return;
		}
		auto round = std::move(*FollowUpRound);
		FollowUpRound.reset();
		round.Stop.reset();
		if (round.Answer) {
			round.Answer(std::move(aAnswer));
		}
	}

	/// [[BPR-2370]]. A round goes as a tour part's chat turn does: the
	/// follow-up's message is the prompt, its instructions the system prompt,
	/// and the run reads the pull request's workspace with read-only tools.
	/// It is fresh: the message carries the item and the earlier rounds.
	void github_application::implementation::launch_follow_up_round(const application::follow_up_agent& aAgent, const std::string& aMessage, std::stop_token aStop, application::follow_up_answer aAnswer)
	{
		if (Closing || FollowUpSession == nullptr || FollowUpRound) {
			aAnswer(std::unexpected{ FollowUpRound ? std::string{ "The side chat is still answering another item. Ask again when it ends; nothing was sent." }
												   : std::string{ "The side chat cannot ask an agent now. Nothing was sent." } });
			return;
		}
		if (aStop.stop_requested()) {
			aAnswer(std::unexpected{ std::string{ "Stopped." } });
			return;
		}
		// The skills the question named ride inline, and the prompt ends by
		// saying what they are, as on the tool-free runner before; the round
		// keeps the Writing style it was asked with.
		auto launched = tour_launch{ .ReadOnly = true, .Path = core::agent_run_path::follow_up, .SystemInstructions = aAgent.Instructions, .FollowUp = true, .WritingStyle = aAgent.WritingStyle };
		auto prompt   = aMessage;
		if (!aAgent.Skills.empty()) {
			launched.Skills.Inline = aAgent.Skills;
			prompt += application::inline_skill_directive();
		}
		FollowUpRound = follow_up_round{ .Answer = std::move(aAnswer), .Stop = nullptr, .Start = [this, profile = core::agent_profile_id{ aAgent.Profile }, message = std::move(prompt), launch = std::move(launched)] {
											return dispatch_agent_invocation(profile, { }, { }, false, message, std::nullopt, core::agent_entry_point::ask_about_selection, { }, true, launch);
										} };
		// Stop ends the run, or the round still waiting for the session; its
		// ending answers the round as stopped.
		FollowUpRound->Stop = std::make_unique<std::stop_callback<std::function<void()>>>(std::move(aStop), std::function<void()>{ [this] {
																							  if (FollowUpRound && FollowUpRound->Start) {
																								  answer_follow_up_round(std::unexpected{ std::string{ "Stopped." } });
																							  } else if (FollowUpSession) {
																								  FollowUpSession->cancel();
																							  }
																						  } });
		// The session's last round may still be retiring (its workspace
		// removed, its helpers settled): this one starts when it has.
		if (FollowUpRound && FollowUpSession->is_quiescent()) {
			start_follow_up_round();
		}
	}

	void github_application::implementation::start_follow_up_round()
	{
		if (!FollowUpRound || !FollowUpRound->Start) {
			return;
		}
		auto start = std::exchange(FollowUpRound->Start, nullptr);
		if (Closing || !start()) {
			answer_follow_up_round(std::unexpected{ std::string{ "The side chat could not start: the pull request's review is not ready. Nothing was sent." } });
		}
	}

	void github_application::implementation::follow_up_round_finished(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult)
	{
		if (aResult.Outcome == application::agent_run_outcome::success && !aResult.AssistantItems.empty()) {
			// The answer is the run's last message; what it said while it
			// read is its working, as a tour part's chat keeps it.
			answer_follow_up_round(aResult.AssistantItems.back());
			return;
		}
		if (aResult.Outcome == application::agent_run_outcome::cancelled) {
			answer_follow_up_round(std::unexpected{ std::string{ "Stopped." } });
			return;
		}
		const auto name = Closing || Window == nullptr ? QStringLiteral("The agent") : agent_display_name(aIdentity.Session.Profile);
		const auto why  = aResult.RefusalReason.empty() ? qt::agent_tab_view::run_failure_words(aResult.Outcome, name) : QString::fromStdString(aResult.RefusalReason);
		answer_follow_up_round(std::unexpected{ why.toStdString() });
	}

	void github_application::implementation::bind_follow_up()
	{
		if (Window == nullptr || Window->agent_tab() == nullptr) {
			return;
		}
		Window->agent_tab()->set_follow_up_dispatch([this](const qt::agent_finding_card_request& aRequest) {
			auto subject  = core::follow_up_subject{ };
			subject.Kind  = core::follow_up_subject_kind::suggested_comment;
			subject.Key   = card_key(aRequest.Session, aRequest.Ordinal);
			subject.Path  = aRequest.Finding.Path;
			subject.First = aRequest.Finding.Lines.Begin + 1U;
			subject.Last  = std::max(subject.First, aRequest.Finding.Lines.End);
			subject.Title = aRequest.Finding.Claim;
			subject.Text  = aRequest.Finding.SuggestedComment.value_or(aRequest.Finding.Claim);
			open_follow_up(std::move(subject));
		});
		if (auto* const report = Window->report_view(); report != nullptr) {
			report->set_revision_source([this, report](const qt::report_draft& aDraft) -> std::optional<QString> {
				const auto found = FollowUpRevisions.find(report_key(report->document().Turn, aDraft));
				return found == FollowUpRevisions.end() ? std::nullopt : std::optional{ QString::fromStdString(found->second) };
			});
			report->set_follow_up_dispatch([this, report](const qt::report_draft& aDraft) {
				auto subject  = core::follow_up_subject{ };
				subject.Kind  = core::follow_up_subject_kind::suggested_comment;
				subject.Key   = report_key(report->document().Turn, aDraft);
				subject.Path  = aDraft.Path.toStdString();
				subject.First = aDraft.First;
				subject.Last  = aDraft.Last;
				subject.Title = aDraft.Card ? aDraft.Card->Finding.Claim : std::string{ };
				subject.Text  = aDraft.Comment.toStdString();
				open_follow_up(std::move(subject));
			});
		}
		Window->item_side_chat()->set_intents({
			.Ask     = [this](const QString& aQuestion) { ask_follow_up(aQuestion); },
			.Propose = [this](const core::follow_up_apply aApply) { propose_follow_up(aApply); },
			.Use     = [this](const std::uint64_t aProposal) { use_follow_up(aProposal); },
			.Discard = [this](const std::uint64_t aProposal) { static_cast<void>(FollowUp != nullptr && FollowUp->discard(aProposal)); },
			.Stop    = [this] {
				if (FollowUp != nullptr) {
					FollowUp->stop();
				} },
			.Closed  = [this] {
				if (FollowUp != nullptr && !Closing) {
					FollowUp->close();
				} },
			.Reveal  = [this](const core::follow_up_subject& aSubject) {
				if (!Closing && Window != nullptr && !aSubject.Path.empty() && aSubject.First != 0U) {
					static_cast<void>(Window->go_to_file_line(QString::fromStdString(aSubject.Path), aSubject.First, false, aSubject.Last));
				} },
		});
		Window->conversation_panel()->cards()->set_ask_about_dispatch([this](const std::string& aIdentity, const QString& aText) { follow_up_conversation(aIdentity, aText); });
	}

	/// Ask about this on a composer or a thread's Ask menu: the text in the
	/// field makes it a draft (Update draft); an empty reply field makes it the
	/// thread (Draft reply).
	void github_application::implementation::follow_up_conversation(const std::string& aIdentity, const QString& aText)
	{
		if (Closing || Window == nullptr || !WindowRead->accepted_snapshot()) {
			return;
		}
		auto subject = core::follow_up_subject{ };
		subject.Key  = aIdentity;
		subject.Text = aText.toStdString();
		if (const auto* const thread = accepted_thread(aIdentity); thread != nullptr) {
			const auto input = application::read_conversation(*thread);
			subject.Path     = input.Path;
			subject.First    = input.StartLine.value_or(input.Line.value_or(0U));
			subject.Last     = input.Line.value_or(0U);
			for (const auto& comment : input.Comments) {
				subject.Thread.push_back({ .Author = comment.Author, .Body = comment.Body });
			}
			if (!input.Comments.empty()) {
				subject.Title = input.Comments.front().Body.substr(0U, 160U);
			}
			if (aText.trimmed().isEmpty()) {
				subject.Kind    = core::follow_up_subject_kind::thread;
				subject.Replies = thread->ViewerReplyPermission != application::github_permission_state::denied;
				subject.Text.clear();
			} else {
				subject.Kind = core::follow_up_subject_kind::draft;
			}
		} else {
			// The draft being written: a new comment on its lines.
			subject.Kind = core::follow_up_subject_kind::draft;
			if (SelectedDiffPublication) {
				subject.Path = SelectedDiffPublication->Request.File.PresentationPath;
			}
			if (ReviewDraft && !ReviewDraft->ReplyToThread) {
				subject.Last  = ReviewDraft->Anchor.Line + 1U;
				subject.First = ReviewDraft->StartLine ? *ReviewDraft->StartLine + 1U : subject.Last;
			}
		}
		if (subject.Kind == core::follow_up_subject_kind::draft && aText.trimmed().isEmpty()) {
			Window->agent_panel()->refuse("Write something in the comment first, then Ask about this.");
			return;
		}
		open_follow_up(std::move(subject));
	}

	/// Opens the item's side chat beside the report or the code. Asked from
	/// the Agent tab while neither shows, the code at the item's place is
	/// brought up so the chat stands beside it.
	void github_application::implementation::open_follow_up(core::follow_up_subject aSubject)
	{
		if (Closing || Window == nullptr || FollowUp == nullptr) {
			return;
		}
		auto       path  = QString::fromStdString(aSubject.Path);
		const auto first = aSubject.First;
		const auto last  = aSubject.Last;
		if (!FollowUp->open(std::move(aSubject))) {
			return;
		}
		auto* const chat = Window->item_side_chat();
		chat->set_column_open(true);
		if (!chat->isVisible() && !path.isEmpty() && first != 0U) {
			static_cast<void>(Window->go_to_file_line(path, first, false, last));
			chat->set_column_open(true);
		}
	}

	void github_application::implementation::present_follow_up(const application::follow_up_projection& aProjection)
	{
		if (Closing || Window == nullptr || !aProjection.Open) {
			return;
		}
		const auto profile = writing_agent();
		Window->item_side_chat()->present(*aProjection.Open, profile ? agent_display_name(*profile) : QString{ }, QString::fromStdString(aProjection.RunFailure));
	}

	/// [[BPR-2370]]. An agent the side chat can ask reads the workspace with
	/// read-only tools, as a tour part's chat's must.
	auto github_application::implementation::follow_up_agent() -> std::optional<application::follow_up_agent>
	{
		const auto  profile = writing_agent();
		const auto& prefs   = Profile->WorkbenchPreferences->accepted();
		if (!profile || !prefs || !reads_read_only(*profile)) {
			return std::nullopt;
		}
		auto agent = application::follow_up_agent{
			.Agent        = "profile-" + std::to_string(profile->value()),
			.AgentName    = agent_display_name(*profile).toStdString(),
			.Profile      = profile->value(),
			.Instructions = std::string{ core::follow_up_instructions },
			.WritingStyle = application::writing_style_for(prefs->WritingHooks, true),
		};
		if (AgentSetup) {
			if (const auto* const entry = core::entry_of_profile(AgentSetup->projection().Preferences.Agents, *profile); entry != nullptr) {
				agent.Delivery = entry->Instructions;
			}
		}
		return agent;
	}

	void github_application::implementation::ask_follow_up(const QString& aQuestion)
	{
		if (Closing || FollowUp == nullptr || Window == nullptr) {
			return;
		}
		auto agent = follow_up_agent();
		if (!agent || !FollowUp->available()) {
			Window->item_side_chat()->show_status(follow_up_agent_words());
			return;
		}
		const auto question = aQuestion.toStdString();
		const auto texts    = std::array{ std::string_view{ question } };
		auto       skills   = prepare_skill_run(SkillLibrary, texts);
		agent->Skills       = std::move(skills.Inline);
		if (FollowUp->ask(std::move(*agent), question) == core::follow_up_refusal::none && skills.Note) {
			Window->item_side_chat()->show_status(QString::fromStdString(*skills.Note));
		}
	}

	/// The text an apply would replace, as it is now: the finding's suggested
	/// comment (as a follow-up last updated it), or the draft's field; none
	/// when the draft is no longer the one open.
	auto github_application::implementation::follow_up_target(const core::follow_up_state& aState, const core::follow_up_apply aApply) const -> std::optional<std::string>
	{
		switch (aApply) {
		case core::follow_up_apply::update_suggestion: {
			const auto found = FollowUpRevisions.find(aState.Subject.Key);
			return found != FollowUpRevisions.end() ? found->second : aState.Subject.Text;
		}
		case core::follow_up_apply::update_draft:
		case core::follow_up_apply::draft_reply:  {
			const auto text = Window->conversation_panel()->cards()->field_text(aState.Subject.Key);
			if (!text) {
				return aApply == core::follow_up_apply::draft_reply ? std::optional{ std::string{ } } : std::nullopt;
			}
			return text->toStdString();
		}
		}
		return std::nullopt;
	}

	void github_application::implementation::propose_follow_up(const core::follow_up_apply aApply)
	{
		if (Closing || FollowUp == nullptr || Window == nullptr || !FollowUp->projection().Open) {
			return;
		}
		auto agent = follow_up_agent();
		if (!agent || !FollowUp->available()) {
			Window->item_side_chat()->show_status(follow_up_agent_words());
			return;
		}
		auto base = follow_up_target(*FollowUp->projection().Open, aApply);
		if (!base) {
			Window->item_side_chat()->show_status(QStringLiteral("Open the draft in Comments to update it. Nothing was sent."));
			return;
		}
		static_cast<void>(FollowUp->propose(std::move(*agent), aApply, std::move(*base)));
	}

	void github_application::implementation::use_follow_up(const std::uint64_t aProposal)
	{
		if (Closing || FollowUp == nullptr || Window == nullptr || !FollowUp->projection().Open || !FollowUp->projection().Open->Proposal) {
			return;
		}
		const auto apply   = FollowUp->projection().Open->Proposal->Apply;
		const auto current = follow_up_target(*FollowUp->projection().Open, apply);
		if (!current) {
			Window->item_side_chat()->show_status(QStringLiteral("Open the draft in Comments to update it. Nothing was replaced."));
			return;
		}
		const auto base    = FollowUp->projection().Open->Proposal->Base;
		const auto decided = FollowUp->apply(aProposal, *current);
		if (decided.Refusal != core::follow_up_refusal::none) {
			return;
		}
		const auto text = QString::fromStdString(decided.Text);
		switch (decided.Apply) {
		case core::follow_up_apply::update_suggestion: {
			FollowUpRevisions[decided.Key] = decided.Text;
			if (const auto card = card_of(decided.Key)) {
				static_cast<void>(Window->agent_tab()->revise_finding(card->first, card->second, text));
			}
			// [[BPR-2376]]. The report finding's draft follows the update
			// while the reviewer has not changed it.
			auto draft = core::finding_draft_revision_kind::none;
			if (auto* const report = Window->report_view(); report != nullptr) {
				for (const auto& finding : report->findings()) {
					if (const auto request = report->finding_request(finding.Outline); request && report_key(report->document().Turn, *request) == decided.Key) {
						draft = revise_finding_draft(*request, *current);
					}
				}
				report->refresh_keeps();
			}
			Window->item_side_chat()->show_status(draft == core::finding_draft_revision_kind::replace ? QStringLiteral("Suggested comment and its draft updated. Nothing is posted.")
													  : draft == core::finding_draft_revision_kind::edited
													  ? QStringLiteral("Suggested comment updated. Its draft, which you changed, was left as it is: Update draft on the finding replaces it. Nothing is posted.")
													  : QStringLiteral("Suggested comment updated. Draft comment drafts it from now on; nothing is posted."));
			return;
		}
		case core::follow_up_apply::update_draft:
			if (Window->conversation_panel()->cards()->replace_field_text(decided.Key, QString::fromStdString(base), text)) {
				Window->item_side_chat()->show_status(QStringLiteral("Draft updated. Undo in the comment takes it back; nothing is posted."));
			} else {
				Window->item_side_chat()->show_status(QStringLiteral("The draft cannot be edited now, so nothing was replaced."));
			}
			return;
		case core::follow_up_apply::draft_reply:
			Window->draft_thread_reply(decided.Key, text, [this, alive = std::weak_ptr<const int>{ WindowLifetime }](const bool aWritten) {
				if (alive.expired() || Closing || Window == nullptr) {
					return;
				}
				Window->item_side_chat()->show_status(aWritten ? QStringLiteral("The reply is in the conversation's reply composer. Nothing is posted until you publish it.")
															   : QStringLiteral("The reply composer did not open; the words are on the clipboard."));
			});
			return;
		}
	}
#endif
} // namespace bpr::app
