#include "github_application/window_composition.hpp"

#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/qt/diff_workbench.hpp"

#include <QComboBox>
#include <QDesktopServices>
#include <QObject>
#include <QTextStream>

#include <algorithm>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/managed_clone.hpp"
	#include "bpr/application/review_brief.hpp"
	#include "bpr/core/agent_comment_suggestion.hpp"
	#include "bpr/core/agent_entry.hpp"
	#include "bpr/core/agent_wrapper.hpp"
	#include "bpr/core/markdown_code.hpp"
	#include "bpr/core/review_skill.hpp"
	#include "bpr/qt/agent_run_settings_picker.hpp"
	#include "bpr/qt/agent_setup_settings.hpp"
	#include "bpr/qt/agent_setup_view.hpp"
	#include <QChar>
	#include <QDir>
	#include <QLatin1Char>
	#if defined(BPR_BUILD_TESTING)
		#include <thread>
	#endif
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/workbench_preferences_host.hpp"
	#include "bpr/qt/settings_window.hpp"
	#include "github_application_fixture_access.hpp"
#endif

namespace bpr::app {
	/// [[BPR-1179]]. Whether an agent can take a request now: never, in a
	/// graph built without agent integrations.
	auto github_application::implementation::agent_idle() const -> bool
	{
#if defined(BPR_AGENT_INTEGRATIONS)
		return AgentHost && AgentHost->is_quiescent();
#else
		return false;
#endif
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// [[BPR-1301]], owner 2026-10-07 ("Yes, search PATH (Recommended)").
	/// Finds the `claude` a launch with no wrapper runs, from this
	/// application's own `PATH` and home folder as they are when asked;
	/// no shell is started to ask for a login `PATH`.
	auto github_application::implementation::locate_claude() const -> application::claude_locator
	{
	#if defined(BPR_BUILD_TESTING)
		if (ClaudeSearch) {
			return [search = *ClaudeSearch] { return adapters::agent::locate_claude(search.PathVariable, std::span<const std::string>{ search.UsualFolders }); };
		}
	#endif
		return [] {
			const auto path = qEnvironmentVariable(std::string{ core::agent_search_path_variable() }.c_str()).toStdString();
			const auto home = QDir::toNativeSeparators(QDir::homePath()).toStdString();
			return adapters::agent::locate_claude(path, std::string_view{ home });
		};
	}

	/// [[BPR-1748]], RFC-BPR-0092 Q5 (owner, 2026-10-22). Finds the
	/// `codex` a Codex launch with no saved program runs, exactly as
	/// `locate_claude` finds `claude`: this application's own `PATH` and
	/// home folder as they are when asked, and Codex's usual folders.
	auto github_application::implementation::locate_agent_program() const -> application::agent_program_locator
	{
	#if defined(BPR_BUILD_TESTING)
		// A fixture course finds only what it put on its own search PATH,
		// never the developer's own `codex`; an attended look is a person's
		// session and finds theirs ([[BPR-1755]]).
		if (!AttendedAgentSearch && (CodexSearch || ClaudeSearch || Fixture)) {
			return [codex = CodexSearch, claude = ClaudeSearch](const core::agent_program aProgram) -> std::optional<core::agent_search_candidate> {
				const auto& search = aProgram == core::agent_program::codex ? codex : claude;
				if (!search) {
					return std::nullopt;
				}
				return adapters::agent::locate_agent_program(aProgram, search->PathVariable, std::span<const std::string>{ search->UsualFolders });
			};
		}
	#endif
		return [](const core::agent_program aProgram) {
			const auto path = qEnvironmentVariable(std::string{ core::agent_search_path_variable() }.c_str()).toStdString();
			const auto home = QDir::toNativeSeparators(QDir::homePath()).toStdString();
			return adapters::agent::locate_agent_program(aProgram, path, std::string_view{ home });
		};
	}

	void github_application::implementation::resume_card_open()
	{
		if (Closing || Window == nullptr || !PendingCardOpen) {
			return;
		}
		const auto* const surface  = Window->comparison_surface();
		const auto&       finding  = PendingCardOpen->Request.Finding;
		const auto        onScreen = SelectedDiffPublication && SelectedDiffPublication->Request.File.PresentationPath == finding.Path && surface != nullptr
			&& surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key;
		if (onScreen) {
			auto request = std::move(PendingCardOpen->Request);
			auto report  = std::move(PendingCardOpen->Report);
			PendingCardOpen.reset();
			act_on_agent_card(request, std::move(report));
			return;
		}
		if (++PendingCardOpen->Tries > 40) {
			const auto request = std::move(PendingCardOpen->Request);
			const auto report  = std::move(PendingCardOpen->Report);
			PendingCardOpen.reset();
			const auto why = QStringLiteral("%1 did not open, so nothing was made.").arg(QString::fromStdString(request.Finding.Path));
			if (report) {
				report(qt::agent_finding_card_state::refused, why, 0U);
			} else {
				Window->agent_tab()->present_finding_card_state(request.Session, request.Ordinal, qt::agent_finding_card_state::refused, why);
			}
			return;
		}
		QTimer::singleShot(250, Window.get(), [this] { resume_card_open(); });
	}

	/// [[BPR-1753]], owner decision 2026-10-23. A private note from
	/// the Agent tab: on one line of the shown comparison when `aLine`
	/// names one, otherwise on the review. It belongs to the review's
	/// private state, not to any agent session.
	void github_application::implementation::save_agent_note(std::string aText, std::optional<std::pair<core::diff_side, std::uint32_t>> aLine, std::string aPath, qt::agent_answer_note_done aDone)
	{
		const auto answer = [aDone](const bool aSaved, const QString& aWhy, const std::uint64_t aNote = 0U) {
			if (aDone) {
				aDone(aSaved, aWhy, aNote);
			}
		};
		if (Closing || Window == nullptr || !ReviewLedgerHost || !ReviewLedgerAvailable || aText.empty()) {
			answer(false, QStringLiteral("no review is open"));
			return;
		}
		auto note      = application::local_note_record{ };
		note.Id        = next_private_note_id();
		note.Note.Text = std::move(aText);
		if (aLine) {
			const auto        attachment = current_note_attachment();
			const auto* const surface    = Window->comparison_surface();
			const auto        shown      = attachment && surface != nullptr && surface->current_result() != nullptr && surface->current_result()->Alignment.Digest == attachment->Comparison
				? core::note_line_anchor_at(surface->current_result()->Alignment, aLine->first, aLine->second)
				: std::nullopt;
			if (!shown) {
				answer(false, QStringLiteral("that line is not on screen"));
				return;
			}
			note.Attachment       = *attachment;
			note.Attachment->Line = shown;
			if (!aPath.empty() && aPath.size() <= application::maximum_note_path_bytes) {
				note.FilePath = std::move(aPath);
			}
		}
		ReviewLedgerHost->persist(note, std::nullopt, ledger_answer([this, answer](const std::expected<application::local_note_record, application::local_note_failure>& aStored) {
									  if (!aStored) {
										  answer(false, QString::fromUtf8(application::name(aStored.error().Reason).data(), static_cast<qsizetype>(application::name(aStored.error().Reason).size())));
										  return;
									  }
									  refresh_agent_private_context_choices();
									  // Owner look v004: Go to note opens the note made.
									  answer(true, { }, aStored->Id.value());
								  }));
	}

	/// [[BPR-1753]], owner decision 2026-10-23. Add as draft comment,
	/// Save as note and Dismiss on one card. A draft and a note are made
	/// only at lines of the file on screen; for another file the card says
	/// to open it first. Nothing is published.
	void github_application::implementation::act_on_agent_card(const qt::agent_finding_card_request& aRequest, card_report aReport)
	{
		auto* const tab   = Window != nullptr ? Window->agent_tab() : nullptr;
		const auto  state = [tab, aRequest, aReport](const qt::agent_finding_card_state aState, const QString& aWhy = { }, const std::uint64_t aNote = 0U) {
			if (aReport) {
				aReport(aState, aWhy, aNote);
			} else if (tab != nullptr) {
				tab->present_finding_card_state(aRequest.Session, aRequest.Ordinal, aState, aWhy, aNote);
			}
		};
		if (Closing || tab == nullptr) {
			return;
		}
		const auto& finding = aRequest.Finding;
		if (aRequest.Action == qt::agent_finding_card_action::dismiss || aRequest.Action == qt::agent_finding_card_action::undo_dismiss) {
			if (aRequest.Action == qt::agent_finding_card_action::dismiss && FindingHost != nullptr) {
				FindingHost->act({ .Session = core::agent_session_id{ aRequest.Session }, .Ordinal = aRequest.Ordinal, .Action = application::agent_finding_action::dismiss });
			}
			state(aRequest.Action == qt::agent_finding_card_action::dismiss ? qt::agent_finding_card_state::dismissed : qt::agent_finding_card_state::open);
			return;
		}
		// The finding's first line, in the comparison on screen when that
		// is the finding's file.
		auto* const surface  = Window->comparison_surface();
		const auto  side     = finding.Anchor.Line ? finding.Anchor.Line->Side : core::diff_side::right;
		const auto  onScreen = SelectedDiffPublication && SelectedDiffPublication->Request.File.PresentationPath == finding.Path && surface != nullptr
			&& surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key;
		// Owner decision 2026-10-23: another file is opened in Review first,
		// then the draft or note is made at its line.
		if (!onScreen) {
			if (!Window->open_file_in_review(QString::fromStdString(finding.Path))) {
				state(qt::agent_finding_card_state::refused, QStringLiteral("%1 is not in this pull request at its current head, so there is no line to put it on.").arg(QString::fromStdString(finding.Path)));
				return;
			}
			state(qt::agent_finding_card_state::working);
			PendingCardOpen = pending_card_open{ .Request = aRequest, .Report = aReport };
			QTimer::singleShot(250, Window.get(), [this] { resume_card_open(); });
			return;
		}
		// The line must be one the file has at the current head.
		const auto& shownAlignment = surface->current_result()->Alignment;
		const auto  lineCount      = side == core::diff_side::left ? shownAlignment.LeftLineCount : shownAlignment.RightLineCount;
		if (finding.Lines.Begin >= lineCount) {
			state(qt::agent_finding_card_state::refused,
				  QStringLiteral("Line %1 is not in %2 at the current head, so nothing was made.").arg(finding.Lines.Begin + 1U).arg(QString::fromStdString(finding.Path)));
			return;
		}
		const auto at = core::source_line_ref{ .Comparison = surface->current_result()->Alignment.Digest, .Side = side, .Line = finding.Lines.Begin };
		if (aRequest.Action == qt::agent_finding_card_action::save_note) {
			auto text = finding.Claim;
			if (finding.SuggestedComment && !finding.SuggestedComment->empty()) {
				text += "\n\n" + *finding.SuggestedComment;
			}
			save_agent_note(std::move(text), std::pair{ side, finding.Lines.Begin }, finding.Path, [state](const bool aSaved, const QString& aWhy, const std::uint64_t aNote) {
				state(aSaved ? qt::agent_finding_card_state::noted : qt::agent_finding_card_state::refused, aSaved ? QString{ } : QStringLiteral("Not saved: %1.").arg(aWhy), aNote);
			});
			return;
		}
		// [[BPR-1861]]. The draft opens with the Conventional Comments
		// label the agent wrote already in the label control.
		const auto text    = core::read_agent_comment_suggestion(finding.SuggestedComment && !finding.SuggestedComment->empty() ? *finding.SuggestedComment : finding.Claim).Body;
		auto       refused = [state] { state(qt::agent_finding_card_state::refused, QStringLiteral("Not drafted: resolve the draft open at that line first.")); };
		auto       done    = [state] { state(qt::agent_finding_card_state::drafted); };
		if (ReviewDraft && ReviewDraft->Anchor.Comparison == at.Comparison && ReviewDraft->Anchor.Side == at.Side && ReviewDraft->Anchor.Line == at.Line) {
			promote_private_item(text, std::move(refused), std::move(done));
			return;
		}
		// The line's own draft opens once its line is chosen, as Draft
		// comment on a note does ([[BPR-1592]]).
		PendingNotePromotion = pending_note_promotion{ .Text = text, .At = at, .Refused = std::move(refused), .Done = std::move(done) };
		if (!surface->select_conversation_line(at.Side, at.Line)) {
			PendingNotePromotion.reset();
			state(qt::agent_finding_card_state::refused, QStringLiteral("Not drafted: line %1 is not in the comparison on screen.").arg(at.Line + 1U));
		}
	}

	/// [[BPR-1753]], owner amendment 2026-10-23. What the picker
	/// offers: each added agent the host offers a slot for, in the list's
	/// order and under its name; a test's own built-in programs as well.
	auto github_application::implementation::offered_agent_profiles() const -> std::vector<core::agent_profile>
	{
		auto offered = std::vector<core::agent_profile>{ };
		if (AgentSetup) {
			const auto listed = AgentSetup->projection().Preferences.Agents;
			for (const auto& entry : listed) {
				const auto slot = core::entry_profile_id(entry);
				if (std::ranges::find(HostOfferedProfiles, slot, &core::agent_profile::Id) != HostOfferedProfiles.end()) {
					offered.push_back(core::entry_profile(entry));
				}
			}
		}
		if (BuiltInProfilesOffered) {
			for (const auto& profile : HostOfferedProfiles) {
				if (!core::is_agent_entry_profile(profile.Id)) {
					offered.push_back(profile);
				}
			}
		}
		return offered;
	}

	/// Owner decision 2026-10-23: the run time limit the reviewer set, or
	/// none (the default), when the run goes until the agent finishes.
	auto github_application::implementation::run_limit_milliseconds() const -> std::optional<int>
	{
		const auto minutes = AgentSetup ? AgentSetup->projection().Preferences.RunLimitMinutes : 0U;
		return minutes == 0U ? std::nullopt : std::optional{ static_cast<int>(minutes) * 60'000 };
	}

	/// Owner decision 2026-10-23 ("separate session for each agent"): the
	/// agent chosen now is the one whose conversation the thread shows.
	auto github_application::implementation::current_conversation_agent() const -> std::uint32_t
	{
		if (Window == nullptr || !AgentSetup) {
			return 0U;
		}
		const auto  selected = Window->agent_panel()->selected_profile();
		const auto  agents   = AgentSetup->projection().Preferences.Agents;
		const auto* entry    = selected ? core::entry_of_profile(agents, *selected) : nullptr;
		return entry != nullptr ? entry->Id : 0U;
	}

	void github_application::implementation::observe_agent_native_history(std::optional<std::string> aChoice)
	{
		if (Closing || !Window || !AgentSetup || !AgentHost || !Window->agent_panel()->selected_profile()) {
			return;
		}
		const auto  profile     = *Window->agent_panel()->selected_profile();
		const auto  preferences = AgentSetup->projection().Preferences;
		const auto* entry       = core::entry_of_profile(preferences.Agents, profile);
		if (!entry) {
			AgentNativeHistory = { };
			return;
		}
		AgentHost->restore_native(profile, entry->Identity, "agent-tab", std::move(aChoice), AgentConversation ? AgentConversation->projection().Key : std::nullopt);
	}
	void github_application::implementation::present_agent_native_history(const application::native_agent_history_projection& aView)
	{
		if (Closing || !Window) {
			return;
		}
		if (aView.ReadOnly) {
			Window->agent_tab()->present_native_history(aView);
			return;
		}
		AgentNativeHistory = aView;
		auto* tab          = Window->agent_tab();
		tab->present_native_history(aView);
		tab->ask_button()->setProperty("conversationLoading", aView.Loading);
		if (aView.Loading) {
			tab->ask_button()->setEnabled(false);
			return;
		}
		if (!aView.Failure.empty()) {
			present_agent_refusal("session history", aView.Failure);
			return;
		}
		if (ChatHost) {
			ChatHost->reset_conversation();
		}
		LatestAgentSession.reset();
		NativeFreshAtCurrentRevision.reset();
		auto turns = std::vector<qt::agent_thread_turn>{ };
		for (const auto& turn : aView.Turns) {
			turns.push_back({ .Question = QString::fromStdString(turn.Asked), .Answer = QString::fromStdString(turn.Answered), .Footer = QString::fromStdString(turn.Ending), .Outcome = turn.Outcome });
		}
		tab->present_thread(turns);
		present_agent_pull_request();
	}

	void github_application::implementation::observe_conversation_agent()
	{
		if (!Closing && AgentConversation) {
			AgentConversation->switch_agent(current_conversation_agent());
			observe_agent_native_history();
			if (Window && Window->tour_editor()->side_chat()->current()) {
				observe_tour_native_history(*Window->tour_editor()->side_chat()->current());
			}
			if (AgentHost && AgentSetup && Window && Window->agent_panel()->selected_profile()) {
				const auto profile     = *Window->agent_panel()->selected_profile();
				const auto preferences = AgentSetup->projection().Preferences;
				if (const auto* const entry = core::entry_of_profile(preferences.Agents, profile); entry != nullptr) {
					AgentHost->restore_native(profile, entry->Identity, "agent-tab");
				}
			}
		}
		// Send to Agent defaults to the agent the pull request's session uses.
		if (!Closing && CycleDesktop && Window != nullptr) {
			if (const auto selected = Window->agent_panel()->selected_profile(); selected && core::is_agent_entry_profile(*selected)) {
				CycleDesktop->prefer_send_profile(selected);
			}
		}
	}

	/// [[BPR-1753]], owner decision (b) 2026-10-23. The pull request
	/// now open, for its conversation.
	void github_application::implementation::observe_conversation()
	{
		if (!AgentConversation) {
			return;
		}
		if (!AgentReview) {
			AgentConversation->close();
			return;
		}
		AgentConversation->open(
			{ .Host = AgentReview->Hostname, .Owner = AgentReview->Locator.Owner, .Name = AgentReview->Locator.Name, .Number = AgentReview->Number.value(), .Agent = current_conversation_agent() });
	}

	/// Owner look v004. The Agent tab's header names the pull request, and
	/// its answers' inline code is coloured in the language most of the
	/// pull request's files are in.
	void github_application::implementation::present_agent_pull_request()
	{
		if (Closing || Window == nullptr || WindowRead == nullptr) {
			return;
		}
		auto* const tab      = Window->agent_tab();
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (!snapshot) {
			tab->set_session_title({ });
			tab->set_inline_code_file({ });
			dismiss_asked_conversation();
			return;
		}
		tab->set_session_title(QStringLiteral("#%1 %2").arg(snapshot->PullRequest.Number.value()).arg(QString::fromStdString(snapshot->PullRequest.Title)));
		// [[BPR-1773]]. A conversation of another pull request is not
		// this one's.
		if (AskedConversation && AskedConversation->PullRequest != snapshot->PullRequest.Number.value()) {
			dismiss_asked_conversation();
		}
		auto paths = std::vector<std::string>{ };
		paths.reserve(snapshot->Files.size());
		for (const auto& file : snapshot->Files) {
			paths.push_back(file.Path);
		}
		tab->set_inline_code_file(QString::fromStdString(core::markdown_inline_code_selector(paths)));
	}

	/// [[BPR-1773]]. Ask agent on a review thread: the Agent tab opens
	/// with the conversation attached to the next question. Nothing is
	/// sent until the reviewer asks.
	void github_application::implementation::ask_agent_about_thread(const std::string& aIdentity)
	{
		if (Closing || Window == nullptr || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto* const thread = accepted_thread(aIdentity);
		if (thread == nullptr) {
			return;
		}
		const auto input = application::read_conversation(*thread);
		if (input.Comments.empty()) {
			return;
		}
		AskedConversation = asked_conversation{ .Identity = aIdentity, .PullRequest = WindowRead->accepted_snapshot()->PullRequest.Number.value() };
		const auto path   = QString::fromStdString(input.Path);
		auto       where  = path;
		if (input.Line) {
			where = input.StartLine ? QStringLiteral("%1 lines %2–%3").arg(path).arg(*input.StartLine).arg(*input.Line) : QStringLiteral("%1 line %2").arg(path).arg(*input.Line);
		}
		AskedConversation->Replies = thread->ViewerReplyPermission != application::github_permission_state::denied;
		const auto count           = input.Comments.size();
		auto       chip            = qt::agent_conversation_chip{
			.Key     = aIdentity,
			.Text    = QStringLiteral("%1, %2").arg(where, count == 1U ? QStringLiteral("1 comment") : QStringLiteral("%1 comments").arg(count)),
			.Detail  = QStringLiteral("The agent gets this conversation's comments and the lines it is on, as the head has them. Nothing is published."),
			.Replies = thread->ViewerReplyPermission != application::github_permission_state::denied,
		};
		Window->agent_tab()->set_conversation_chip(std::move(chip));
		Window->open_agent_question();
	}

	/// [[BPR-1773]]. Ask agent on a saved private note: as a thread's,
	/// but the note's words reach the agent only as the copy saved in
	/// Private history when the question is sent.
	void github_application::implementation::ask_agent_about_note(const application::local_note_record& aNote)
	{
		if (Closing || Window == nullptr || !WindowRead->accepted_snapshot()) {
			return;
		}
		AskedConversation = asked_conversation{ .Identity = "note:" + std::to_string(aNote.Id.value()), .PullRequest = WindowRead->accepted_snapshot()->PullRequest.Number.value(), .Note = aNote.Id };
		auto where        = QStringLiteral("this pull request");
		if (aNote.FilePath) {
			const auto path = QString::fromStdString(*aNote.FilePath);
			where           = aNote.Attachment && aNote.Attachment->Line ? QStringLiteral("%1 line %2").arg(path).arg(aNote.Attachment->Line->Line + 1U) : path;
		}
		Window->agent_tab()->set_conversation_chip(qt::agent_conversation_chip{
			.Key     = AskedConversation->Identity,
			.Text    = QStringLiteral("Your note on %1").arg(where),
			.Detail  = QStringLiteral("When you ask, the note is saved in Private history as it is then, and the agent gets that copy and the lines it is on. Nothing is published."),
			.Replies = false,
		});
		Window->open_agent_question();
	}

	auto github_application::implementation::asked_note() const -> const application::local_note_record*
	{
		if (!AskedConversation || !AskedConversation->Note) {
			return nullptr;
		}
		const auto found = std::ranges::find(AcceptedPrivateNotes, *AskedConversation->Note, &application::local_note_record::Id);
		return found == AcceptedPrivateNotes.end() ? nullptr : &*found;
	}

	/// [[BPR-1773]], owner look 2026-10-27. Where a question sent now
	/// would have its answer drafted: core places it from the review
	/// thread asked about and the lines selected beside the code (none
	/// on Overview); the turn keeps the key.
	auto github_application::implementation::decide_answer_draft() -> qt::agent_answer_draft_target
	{
		auto       draft  = agent_answer_draft{ };
		const auto thread = AskedConversation && !AskedConversation->Note && AskedConversation->Replies;
		auto       lines  = false;
		if (!thread && !Closing && Window != nullptr && SelectedDiffPublication && !Window->showing_overview()) {
			if (const auto* const surface = Window->comparison_surface(); surface != nullptr && surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key) {
				if (const auto range = surface->line_draft_range(); range && range->first.Side == range->second.Side) {
					draft.Path  = SelectedDiffPublication->Request.File.PresentationPath;
					draft.Side  = range->first.Side;
					draft.First = std::min(range->first.Line, range->second.Line);
					draft.Last  = std::max(range->first.Line, range->second.Line);
					lines       = true;
				}
			}
		}
		draft.Place = core::answer_draft_place_for(thread, lines);
		auto tip    = QString{ };
		switch (draft.Place) {
		case core::answer_draft_place::thread_reply:
			draft.Thread = AskedConversation->Identity;
			tip          = QStringLiteral("Draft this reply, or the part of it you selected, as your reply to the conversation it is about. Nothing is posted until you post it.");
			break;
		case core::answer_draft_place::line_comment: {
			const auto path = QString::fromStdString(draft.Path);
			const auto at   = draft.First == draft.Last ? QStringLiteral("%1 line %2").arg(path).arg(draft.First + 1U) : QStringLiteral("%1 lines %2–%3").arg(path).arg(draft.First + 1U).arg(draft.Last + 1U);
			tip             = QStringLiteral("Draft this reply, or the part of it you selected, as a comment on %1, the lines it was asked about. Nothing is posted until you post it.").arg(at);
			break;
		}
		case core::answer_draft_place::pull_request_comment:
			tip = QStringLiteral("Draft this reply, or the part of it you selected, as a comment on the pull request. Nothing is posted until you post it.");
			break;
		}
		auto key = "answer-draft:" + std::to_string(++NextAgentAnswerDraft);
		AgentAnswerDrafts.insert_or_assign(key, std::move(draft));
		return { .Key = std::move(key), .Tip = tip };
	}

	/// [[BPR-1773]]. Draft comment on an answer: a thread's reply, as
	/// Reply opens it; a line comment by a finding card's way to its
	/// line; or Overview's new pull request comment ([[BPR-1781]]).
	/// Each is a private draft; nothing is published.
	void github_application::implementation::draft_agent_answer(const std::string& aKey, const QString& aText, qt::agent_answer_draft_done aDone)
	{
		const auto found = AgentAnswerDrafts.find(aKey);
		const auto draft = found != AgentAnswerDrafts.end() ? found->second : agent_answer_draft{ };
		// [[BPR-1862]]. The turn keeps the draft it wrote, so that once
		// the draft is deleted the turn offers Draft comment again.
		const auto answer = [this, aDone, aKey, draft](const bool aDrafted, const QString& aWhy = { }) {
			if (aDrafted && !Closing && ReviewDraft) {
				const auto wrote = draft.Place == core::answer_draft_place::thread_reply ? ReviewDraft->ReplyToThread == std::optional{ draft.Thread }
					: draft.Place == core::answer_draft_place::line_comment              ? !ReviewDraft->ReplyToThread && ReviewDraft->Anchor.Line == draft.First
																						 : false;
				if (const auto kept = AgentAnswerDrafts.find(aKey); wrote && kept != AgentAnswerDrafts.end()) {
					kept->second.Drafted = ReviewDraft->Id;
				}
			}
			if (aDone) {
				aDone(aDrafted, aWhy);
			}
		};
		if (Closing || Window == nullptr || aText.trimmed().isEmpty()) {
			answer(false, QStringLiteral("there is nothing to draft"));
			return;
		}
		// [[BPR-1861]]. An answer that opens with a Conventional Comments
		// header drafts with its label in the label control. The line
		// comment reads it on the card's way below.
		const auto labelled = QString::fromStdString(core::read_agent_comment_suggestion(aText.toStdString()).Body);
		switch (draft.Place) {
		case core::answer_draft_place::thread_reply:
			Window->draft_thread_reply(draft.Thread, labelled, [answer](const bool aWritten) { answer(aWritten, aWritten ? QString{ } : QStringLiteral("the conversation's reply did not open; the words are on the clipboard")); });
			return;
		case core::answer_draft_place::line_comment: {
			auto finding             = core::agent_finding{ };
			finding.Path             = draft.Path;
			finding.Lines            = core::line_span{ .Begin = draft.First, .End = draft.Last + 1U };
			finding.Anchor.Line      = core::source_line_ref{ .Comparison = { }, .Side = draft.Side, .Line = draft.First };
			finding.Claim            = aText.toStdString();
			finding.SuggestedComment = aText.toStdString();
			act_on_agent_card({ .Session = 0U, .Ordinal = 0U, .Finding = std::move(finding), .Action = qt::agent_finding_card_action::draft_comment },
							  [answer](const qt::agent_finding_card_state aState, const QString& aWhy, std::uint64_t) {
								  if (aState == qt::agent_finding_card_state::drafted) {
									  answer(true);
								  } else if (aState == qt::agent_finding_card_state::refused) {
									  auto why = aWhy;
									  if (why.startsWith(QStringLiteral("Not drafted: "))) {
										  why.remove(0, QStringLiteral("Not drafted: ").size());
									  }
									  if (why.endsWith(QLatin1Char{ '.' })) {
										  why.chop(1);
									  }
									  answer(false, why);
								  }
							  });
			return;
		}
		case core::answer_draft_place::pull_request_comment:
			if (Window->draft_pull_request_comment(labelled)) {
				answer(true);
			} else {
				answer(false, QStringLiteral("the pull request's comment could not be opened"));
			}
			return;
		}
	}

	/// [[BPR-1773]]. Go to draft: the draft Draft comment wrote.
	void github_application::implementation::go_to_agent_answer_draft(const std::string& aKey)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto found = AgentAnswerDrafts.find(aKey);
		const auto draft = found != AgentAnswerDrafts.end() ? found->second : agent_answer_draft{ };
		switch (draft.Place) {
		case core::answer_draft_place::thread_reply:
			Window->open_thread_reply(draft.Thread);
			return;
		case core::answer_draft_place::line_comment:
			static_cast<void>(Window->go_to_file_line(QString::fromStdString(draft.Path), draft.First + 1U, draft.Side == core::diff_side::left));
			return;
		case core::answer_draft_place::pull_request_comment:
			Window->open_pull_request_comment();
			return;
		}
	}

	void github_application::implementation::dismiss_asked_conversation()
	{
		AskedConversation.reset();
		if (!Closing && Window != nullptr) {
			Window->agent_tab()->set_conversation_chip(std::nullopt);
		}
	}

	/// [[BPR-1773]]. The conversation an `ask_about_conversation` turn
	/// carries, bounded, with the lines it is on when its file is the
	/// one compared; none when it is no longer in this pull request.
	auto github_application::implementation::read_asked_conversation() const -> std::optional<core::supplement_conversation>
	{
		if (!AskedConversation || Window == nullptr) {
			return std::nullopt;
		}
		if (AskedConversation->Note) {
			const auto* const note = asked_note();
			if (note == nullptr) {
				return std::nullopt;
			}
			auto input = core::conversation_input{ .Subject = core::conversation_subject::private_note, .Path = note->FilePath.value_or(std::string{ }) };
			if (note->Attachment && note->Attachment->Line && note->FilePath) {
				const auto line       = note->Attachment->Line->Line + 1U;
				const auto side       = note->Attachment->Line->Side;
				input.Line            = line;
				constexpr auto around = 6U;
				if (const auto lines = Window->compared_lines(*note->FilePath, side, line > around ? line - around : 1U, line + around); lines && !lines->second.empty()) {
					input.Excerpt = core::conversation_excerpt{ .FirstLine = lines->first, .Lines = lines->second, .AnchorFirst = line, .AnchorLast = line, .Before = side == core::diff_side::left };
				}
			}
			return core::bound_conversation(std::move(input));
		}
		const auto* const thread = accepted_thread(AskedConversation->Identity);
		if (thread == nullptr) {
			return std::nullopt;
		}
		auto input = application::read_conversation(*thread);
		if (input.Line && !input.Outdated) {
			constexpr auto around = 6U;
			const auto     first  = input.StartLine.value_or(*input.Line);
			const auto     side   = thread->DiffSide == application::github_diff_side::left ? core::diff_side::left : core::diff_side::right;
			if (const auto lines = Window->compared_lines(input.Path, side, first > around ? first - around : 1U, *input.Line + around); lines && !lines->second.empty()) {
				input.Excerpt = core::conversation_excerpt{
					.FirstLine   = lines->first,
					.Lines       = lines->second,
					.AnchorFirst = first,
					.AnchorLast  = std::min<std::uint32_t>(*input.Line, lines->first + static_cast<std::uint32_t>(lines->second.size()) - 1U),
					.Before      = side == core::diff_side::left,
				};
			}
		}
		return core::bound_conversation(std::move(input));
	}

	/// The open pull request's kept thread, shown in the Agent tab.
	void github_application::implementation::present_conversation(const application::agent_conversation_projection& aView)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (aView.SaveFailed) {
			present_agent_refusal("history storage", "This conversation could not be saved. Existing histories were kept. Free storage before starting another conversation.");
		}
		// [[BPR-1965]]. Saving the visible turn reports durability, not a
		// restored transcript: rebuilding it would lose About and DraftTarget.
		if (aView.Update == application::agent_conversation_update::storage_status) {
			return;
		}
		if (Window->agent_panel()->selected_profile() && AgentSetup) {
			const auto preferences = AgentSetup->projection().Preferences;
			if (const auto* entry = core::entry_of_profile(preferences.Agents, *Window->agent_panel()->selected_profile()); entry) {
				observe_agent_native_history();
				return;
			}
		}
		// A late history load must settle before a new visible turn starts;
		// otherwise presenting the loaded thread can replace that live turn.
		Window->agent_tab()->ask_button()->setProperty("conversationLoading", !aView.Loaded);
		auto shown = std::vector<qt::agent_thread_turn>{ };
		shown.reserve(aView.Turns.size());
		for (const auto& turn : aView.Turns) {
			shown.push_back({ .Question = QString::fromStdString(turn.Asked), .Answer = QString::fromStdString(turn.Answered), .Footer = QString::fromStdString(turn.Ending), .Outcome = turn.Outcome });
		}
		Window->agent_tab()->present_thread(shown);
		present_agent_pull_request();
		// [[BPR-1823]], owner ruling 2026-10-29: opening a pull request
		// never switches the picker to the agent it was last asked with.
		// The thread follows the picker: the chosen agent's own
		// conversation about it is the one shown.
		if (aView.Loaded && aView.Key && AgentConversation) {
			if (const auto chosen = current_conversation_agent(); chosen != 0U && aView.Key->Agent != chosen) {
				AgentConversation->switch_agent(chosen);
			}
		}
	}

	/// [[BPR-1750]], RFC-BPR-0092 D2. The repository of the review
	/// now open, for its remembered clone.
	void github_application::implementation::observe_clone_repository()
	{
		// [[BPR-1859]]. Whether Check agent also makes a workspace
		// follows the open review.
		present_settings_check();
		if (!CloneLinks) {
			return;
		}
		if (!AgentReview) {
			CloneLinks->observe_repository(std::nullopt);
			refresh_skills();
			return;
		}
		auto repository       = application::clone_repository{ };
		repository.Host       = AgentReview->Hostname;
		repository.Repository = AgentReview->Repository.value();
		repository.Owner      = AgentReview->Locator.Owner;
		repository.Name       = AgentReview->Locator.Name;
		// [[BPR-1879]]. A sparse review's `gh` lease can keep a clone;
		// GitHub's `size` says roughly what it downloads.
		repository.CanKeep          = AgentReview->Sparse;
		repository.ApproximateBytes = AgentReview->ApproximateBytes;
	#if defined(BPR_BUILD_TESTING)
		// [[BPR-1879]], owner look v005. A fixture course has no `gh` lease,
		// so keeping a clone there is a rehearsal (see prepare_kept_clone):
		// the frozen look can show the choice and its progress.
		if (Fixture != nullptr) {
			repository.ApproximateBytes = repository.ApproximateBytes.value_or(48'000'000U);
		}
	#endif
		CloneLinks->observe_repository(std::move(repository));
		refresh_skills();
	}

	/// [[BPR-1879]], RFC-BPR-0092. The download of the clone
	/// BetterPullReview keeps for the review on screen, bound to that
	/// review's authority, session and head now, on the decision lane; run
	/// later on the clone link host's own worker. Nothing when the review
	/// is not a sparse one of this repository.
	auto github_application::implementation::prepare_kept_clone(const application::clone_repository& aRepository, const std::string& aDestination) -> application::clone_link_dependencies::keep_job
	{
	#if defined(BPR_BUILD_TESTING)
		// [[BPR-1879]], owner look v005. A fixture course rehearses the
		// download: bytes arrive at about 2 MB a second, on past the stated
		// size as a real clone with no limit may, until Cancel, which ends
		// it as a cancel. Nothing is written; no clone is made.
		if (Fixture != nullptr) {
			if (Closing || !AgentReview || AgentReview->Repository.value() != aRepository.Repository) {
				return { };
			}
			return [](const application::git_smart_clone_progress& aProgress, const std::stop_token aStop) -> std::expected<void, application::managed_clone_error> {
				auto received = std::uint64_t{ 0U };
				while (!aStop.stop_requested()) {
					std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
					received += 500'000U;
					if (aProgress) {
						aProgress(received);
					}
				}
				return std::unexpected{ application::managed_clone_error{ application::managed_clone_failure::cancelled,
																		  { } } };
			};
		}
	#endif
		if (Closing || EpochHost == nullptr || !AgentReview || !AgentReview->Sparse || AgentReview->Repository.value() != aRepository.Repository) {
			return { };
		}
		const auto& state = EpochHost->state();
		if (!state.Authority || !state.AcceptedRevision) {
			return { };
		}
		auto request = application::managed_clone_request{
			.ApplicationSession = state.ApplicationSession,
			.Authority          = *state.Authority,
			.Head               = state.AcceptedRevision->Head,
			.Destination        = std::filesystem::path{ aDestination }.lexically_normal(),
		};
		if (!application::valid(request)) {
			return { };
		}
		return [host = EpochHost.get(), bound = std::move(request)](const application::git_smart_clone_progress& aProgress, std::stop_token aStop) {
			return host->keep_managed_clone(bound, aProgress, std::move(aStop));
		};
	}

	/// [[BPR-1749]], RFC-BPR-0092. What setup decided reaches the
	/// window: the saved or chosen agent is the one the Agent tab and Send to
	/// Agent offer first.
	/// [[BPR-1751]], RFC-BPR-0092 Q7. The linked clone a review's
	/// workspace borrows history from: the observed repository's serving
	/// link, never for tool-free writing and never the private copy.
	auto github_application::implementation::linked_history_clone(const bool aWritingOnly) const -> std::optional<std::filesystem::path>
	{
		if (aWritingOnly || !CloneLinks || !CloneLinks->repository_chosen()) {
			return std::nullopt;
		}
		const auto view = CloneLinks->projection();
		if (!view.Linked || view.Linked->PrivateCopy || view.Linked->Path.empty()) {
			return std::nullopt;
		}
		return std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(view.Linked->Path.data()),
													 view.Linked->Path.size() } };
	}

	/// [[BPR-1884]]. Whether that clone is the one BetterPullReview
	/// keeps, which the host may fetch into; a reviewer's never is.
	auto github_application::implementation::linked_history_clone_managed(const bool aWritingOnly) const -> bool
	{
		if (!linked_history_clone(aWritingOnly)) {
			return false;
		}
		const auto view = CloneLinks->projection();
		return view.Linked && view.Linked->Managed;
	}

	/// [[BPR-1751]], RFC-BPR-0092 Check it works. One Check runs the
	/// agent's own status command and makes, then removes, the workspace an
	/// Ask would make for the review on screen.
	/// [[BPR-1859]]. Settings' Check agent runs this same check when the
	/// core offers it; with no review open there is no workspace to make,
	/// so its row is left not run rather than failed.
	void github_application::implementation::check_agent_setup(const bool aWorkspace)
	{
		if (Closing || !AgentSetup || AgentSetup->projection().Checking || AgentSetup->projection().State.WorkspaceCheck.State == core::setup_check_state::running) {
			return;
		}
		AgentSetup->check();
		if (aWorkspace) {
			check_agent_workspace();
			return;
		}
		LastWorkspaceProbeReason.reset();
		AgentSetup->set_workspace_check({ });
	}

	/// Stop, from the setup or Settings: the agent's check and the
	/// workspace's.
	void github_application::implementation::stop_agent_setup_check()
	{
		if (!Closing && AgentSetup) {
			AgentSetupWorkspaceStop.request_stop();
			AgentSetup->stop();
		}
	}

	auto github_application::implementation::settings_check_decision() const -> core::agent_check_decision
	{
		return AgentSetup ? core::decide_agent_check(application::agent_check_facts_of(AgentSetup->projection(), static_cast<bool>(AgentReview))) : core::agent_check_decision{ };
	}

	/// [[BPR-1859]]. Settings › Agents & context: Check agent as the
	/// core offers it, with the setup's rows as the host published them.
	void github_application::implementation::check_agent_from_settings()
	{
		if (Closing || !AgentSetup) {
			return;
		}
		const auto decision = settings_check_decision();
		if (decision.Offer != core::agent_check_offer::available) {
			return;
		}
		check_agent_setup(decision.ChecksWorkspace);
	}

	void github_application::implementation::present_settings_check()
	{
		if (Closing || Window == nullptr || !AgentSetup) {
			return;
		}
		auto shown            = qt::agent_check_presentation{ };
		shown.Setup           = AgentSetup->projection();
		shown.Decision        = settings_check_decision();
		shown.WorkspaceReason = LastWorkspaceProbeReason;
		Window->agent_programs()->present_check(shown);
	}

	void github_application::implementation::check_agent_workspace()
	{
		if (Closing || !AgentSetup) {
			return;
		}
		const auto failed = [this](const std::optional<application::agent_workspace_unavailable_reason> aReason, std::string aDetail = { }) {
			LastWorkspaceProbeReason = aReason;
			AgentSetup->set_workspace_check(core::fail_check(core::agent_setup_failure::workspace_unavailable), std::move(aDetail));
		};
		if (AgentHost == nullptr || EpochHost == nullptr || !AgentReview) {
			failed(std::nullopt);
			return;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority || !state.AcceptedRevision || !state.AcceptedComparison) {
			failed(std::nullopt);
			return;
		}
		const auto& authority  = *state.Authority;
		const auto& revision   = *state.AcceptedRevision;
		const auto& comparison = *state.AcceptedComparison;
		auto        binding    = core::agent_revision_binding{
			.Host        = authority.Host,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.BaseOid     = revision.Base,
			.HeadOid     = revision.Head,
			.LeftEpoch   = comparison.Left.Identity.Epoch,
			.RightEpoch  = comparison.Right.Identity.Epoch,
		};
		AgentSetupWorkspaceStop = std::stop_source{ };
		const auto generation   = ++AgentSetupWorkspaceGeneration;
		const auto stop         = AgentSetupWorkspaceStop.get_token();
		LastWorkspaceProbeReason.reset();
		AgentSetup->set_workspace_check({ .State = core::setup_check_state::running });
		AgentHost->probe_workspace(state.ApplicationSession, authority, std::move(binding), linked_history_clone(false), [this, failed, stop, generation](std::expected<application::prepared_workspace, application::agent_workspace_error> aProbed) {
									   if (Closing || !AgentSetup || generation != AgentSetupWorkspaceGeneration) {
										   return;
									   }
									   if (stop.stop_requested()) {
										   LastWorkspaceProbeReason.reset();
										   AgentSetup->set_workspace_check({ });
										   return;
									   }
									   if (aProbed) {
										   LastWorkspaceProbeReason.reset();
										   AgentSetup->set_workspace_check({ .State = core::setup_check_state::passed });
										   return;
									   }
									   failed(aProbed.error().Code == application::agent_workspace_failure::unavailable ? std::optional{ aProbed.error().Reason } : std::nullopt, aProbed.error().Detail); }, stop);
	}

	/// [[BPR-1750]]. A repository with a serving link or the private
	/// copy has its Repository step done.
	void github_application::implementation::present_clone_links(const application::clone_link_projection& aProjection)
	{
		if (Closing || !CloneLinks || !AgentSetup) {
			return;
		}
		if (Window != nullptr) {
			Window->clone_folders()->present(aProjection);
		}
		// [[BPR-1879]], owner look v006. A review that holds every file needs
		// no clone: its Repository step is done without one, never a question.
		AgentSetup->set_repository_chosen(CloneLinks->repository_chosen() || (AgentReview && !AgentReview->Sparse));
		present_setup_view();
		// [[BPR-1916]]. A linked, changed or forgotten clone changes which
		// repository skills there are.
		refresh_skills();
	}

	/// [[BPR-1753]], RFC-BPR-0092. The Agent tab's setup and its
	/// Settings rows: every press is a request to the setup, clone link or
	/// review skill host, and their projections are all the view shows.
	void github_application::implementation::compose_agent_setup_view()
	{
		if (Window == nullptr || !AgentSetup || !CloneLinks || !ReviewSkill) {
			return;
		}
		auto* const tab = Window->agent_tab();
		tab->adopt_setup(*Window->agent_setup());
		tab->set_run_settings_dispatch([this](const std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort) {
			return apply_run_settings(aProfile, std::move(aModel), std::move(aEffort));
		});
		// [[BPR-1753]], owner decision (b). Each turn that ends is
		// kept for the pull request, and Clear session keeps none.
		tab->set_thread_observer([this](std::vector<qt::agent_thread_turn> aTurns) {
			if (Closing || !AgentConversation) {
				return;
			}
			auto kept = std::vector<application::agent_conversation_turn>{ };
			kept.reserve(aTurns.size());
			for (const auto& turn : aTurns) {
				kept.push_back({ .Asked = turn.Question.toStdString(), .Answered = turn.Answer.toStdString(), .Ending = turn.Footer.toStdString(), .Outcome = turn.Outcome });
			}
			if (AgentNativeHistory.Scope.empty()) {
				AgentConversation->record(std::move(kept));
			}
		});
		// Owner decision 2026-10-23 ("Yep build it like that"): a card's
		// three actions, and Save as note on a plain reply. Each makes a
		// private draft or note in the review, never part of a session.
		// Owner decision 2026-10-23: switching agents shows that agent's own
		// conversation about the open pull request.
		QObject::connect(Window->agent_panel()->profile_selector(), &QComboBox::currentIndexChanged, Window.get(), [this] { observe_conversation_agent(); });
		tab->set_finding_card_dispatch([this](const qt::agent_finding_card_request& aRequest) { act_on_agent_card(aRequest); });
		tab->set_answer_note_dispatch([this](const QString& aText, qt::agent_answer_note_done aDone) { save_agent_note(aText.toStdString(), std::nullopt, { }, std::move(aDone)); });
		// [[BPR-1773]]. Ask agent on a thread attaches it; the chip's
		// close removes it. Owner look 2026-10-27: every answer's Draft
		// comment writes it as a private draft where core places it when
		// it was asked, and Go to draft opens that draft.
		Window->set_conversation_ask_dispatch([this](const std::string& aIdentity) { ask_agent_about_thread(aIdentity); });
		tab->set_conversation_dismiss([this] { dismiss_asked_conversation(); });
		tab->set_answer_draft_target([this] { return decide_answer_draft(); });
		tab->set_answer_draft_dispatch([this](const std::string& aKey, const QString& aText, qt::agent_answer_draft_done aDone) { draft_agent_answer(aKey, aText, std::move(aDone)); });
		tab->set_draft_navigation([this](const std::string& aKey) { go_to_agent_answer_draft(aKey); });
		// Owner look v004. Go to note; the files and lines a question or an
		// answer names open in Review; the suggestions' toggle is kept.
		tab->set_note_navigation([this](const std::uint64_t aNote) {
			if (!Closing && Window != nullptr) {
				Window->go_to_private_note(aNote);
			}
		});
		tab->set_line_links(
			[this](const QString& aPath) -> std::optional<QString> { return Closing || Window == nullptr ? std::nullopt : Window->review_file_named(aPath); },
			[this](const QString& aPath, const std::uint32_t aLine, const bool aBefore) {
				if (!Closing && Window != nullptr) {
					static_cast<void>(Window->go_to_file_line(aPath, aLine, aBefore));
				}
			});
		// Owner look v006. The agent shown is asked what its harness
		// offers; Reset agents clears every agent setting and the setup
		// starts again, leaving notes, drafts and conversations.
		tab->set_catalog_dispatch([this](const std::uint64_t aProfile) {
			if (Closing || !AgentSetup) {
				return;
			}
			const auto agents = AgentSetup->projection().Preferences.Agents;
			if (const auto* const entry = core::entry_of_profile(agents, core::agent_profile_id{ aProfile }); entry != nullptr) {
				AgentSetup->load_catalog(entry->Id);
			}
		});
		tab->set_reset_dispatch([this] {
			if (Closing || !AgentSetup) {
				return;
			}
			AgentSetupWorkspaceStop.request_stop();
			++AgentSetupWorkspaceGeneration;
			// [[BPR-1769]]. The old preferences are migration inputs, not
			// a second agent list. Reset clears them durably as well.
			static_cast<void>(Profile->WorkbenchPreferences->apply_claude_wrapper({ }));
			static_cast<void>(Profile->WorkbenchPreferences->apply_claude_model({ }));
			AgentSetup->reset();
			// [[BPR-1823]]. No choice outlives Reset: a new agent may
			// reuse an old one's id.
			Window->agent_panel()->forget_choice();
			// The fresh setup looks for agents again, as Set up did.
			AgentSetup->restart();
			// A new agent's id may be an old one's: no pull request opens
			// on the agent it was last asked with. Its turns stay.
			if (AgentConversation) {
				AgentConversation->forget_agent_choices();
			}
			if (CloneLinks) {
				const auto links = CloneLinks->projection().Links;
				for (const auto& link : links) {
					CloneLinks->forget(link);
				}
				CloneLinks->set_clone_folders({ });
			}
			if (ReviewSkill) {
				const auto skill = ReviewSkill->projection();
				if (skill.Source != core::review_skill_source::built_in || skill.CopyBaseVersion) {
					ReviewSkill->use_built_in(true);
				}
			}
		});
		tab->set_suggestions_observer([this](const bool aShown) {
			if (!Closing && AgentSetup) {
				AgentSetup->set_suggestions_hidden(!aShown);
			}
		});
		tab->set_native_history_dispatch([this](std::optional<std::string> aChoice) { observe_agent_native_history(std::move(aChoice)); });
		tab->set_clear_session_dispatch([this] {
			if (!Closing && AgentConversation) {
				observe_agent_native_history(std::string{ });
			}
		});
		// Owner decision (a). The suggested prompts name the open file and
		// the selected lines, one-based.
		tab->set_suggestion_context([this] {
			auto context = qt::agent_suggestion_context{ };
			// [[BPR-1781]]. Asked on Overview, a question is about the pull
			// request: no open file, no lines.
			if (Closing || Window == nullptr || !SelectedDiffPublication || Window->showing_overview()) {
				return context;
			}
			context.OpenFile = QString::fromStdString(SelectedDiffPublication->Request.File.PresentationPath);
			if (const auto* const surface = Window->comparison_surface(); surface != nullptr && surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key) {
				if (const auto range = surface->line_draft_range(); range && range->first.Side == range->second.Side) {
					const auto first = std::min(range->first.Line, range->second.Line) + 1U;
					const auto last  = std::max(range->first.Line, range->second.Line) + 1U;
					context.Lines    = std::pair{ first, last };
				}
			}
			return context;
		});
		// Looking for agents waits for the tab: showing the setup is what
		// may look (owner answer Q2), and looking reads file metadata only.
		tab->set_first_shown([this] {
			if (!Closing && AgentSetup) {
				AgentSetup->detect();
			}
		});
		Window->agent_setup()->set_intents(qt::agent_setup_intents{
			.Choose =
				[this](const core::setup_agent aAgent) {
					if (!Closing && AgentSetup) {
						AgentSetup->choose(aAgent);
						take_setup_choice();
					}
				},
			.FindAgents             = [this] { if (!Closing && AgentSetup) { AgentSetup->detect(); } },
			.FindClones             = [this] { if (!Closing && CloneLinks) { CloneLinks->look_again(); } },
			.OpenSettings           = [this] { Window->open_settings(qt::settings_page::agents_and_context); },
			.OpenRepositorySettings = [this] { Window->open_settings(qt::settings_page::repositories); },
			.OpenInstallPage =
				[this](const core::setup_agent aAgent) {
					if (Profile->WindowBrowser == nullptr) {
						return;
					}
					const auto* const address = aAgent == core::setup_agent::codex ? "https://github.com/openai/codex" : "https://docs.anthropic.com/en/docs/claude-code/setup";
					try {
						static_cast<void>(Profile->WindowBrowser->open(address));
					} catch (...) { }
				},
			.ChooseClone =
				[this](std::string aPath) {
					if (!Closing && CloneLinks) {
						CloneLinks->choose_clone(std::move(aPath));
					}
				},
			.AddCloneFolder =
				[this](std::string aFolder) {
					if (!Closing && CloneLinks) {
						CloneLinks->add_clone_folder(std::move(aFolder));
					}
				},
			.SetCloneFolders =
				[this](std::vector<std::string> aFolders) {
					if (!Closing && CloneLinks) {
						CloneLinks->set_clone_folders(std::move(aFolders));
					}
				},
			.UsePrivateCopy =
				[this] {
					if (!Closing && CloneLinks && AgentSetup) {
						const auto retry = AgentSetup->projection().State.WorkspaceCheck.State == core::setup_check_state::failed;
						CloneLinks->use_private_copy();
						const auto linked = CloneLinks->projection().Linked;
						if (retry && linked && linked->PrivateCopy) {
							check_agent_setup();
						}
					}
				},
			// [[BPR-1879]]. BetterPullReview keeps a clone, or stops.
			.KeepClone =
				[this] {
					if (!Closing && CloneLinks) {
						CloneLinks->keep_clone();
					}
				},
			.CancelKeeping =
				[this] {
					if (!Closing && CloneLinks) {
						CloneLinks->cancel_keeping();
					}
				},
			.Check = [this] { check_agent_setup(); },
			.Stop  = [this] { stop_agent_setup_check(); },
			.AddCustomAgent =
				[this](core::agent_entry aEntry) {
					if (!Closing && AgentSetup) {
						AgentSetup->add_agent(std::move(aEntry), true, [this](const std::optional<core::agent_entry_refusal> aRefusal) {
							if (aRefusal && !Closing && Window != nullptr) {
								Window->agent_panel()->refuse(std::string{ core::describe(*aRefusal) });
							} else if (!aRefusal) {
								take_setup_choice();
							}
						});
					}
				},
		});
		Window->agent_programs()->set_dispatch(qt::agent_programs_panel::dispatch{
			// [[BPR-1859]]. Check agent is the setup's check, and its Stop
			// the setup's Stop.
			.Check     = [this] { check_agent_from_settings(); },
			.StopCheck = [this] { stop_agent_setup_check(); },
			.Reset =
				[this] {
					if (!Closing && Window != nullptr) {
						Window->agent_tab()->reset_agents();
					}
				},
			// [[BPR-1753]], owner amendment 2026-10-23. The list's edits.
			.Add =
				[this](core::agent_entry aEntry, qt::agent_programs_panel::answer aAnswer) {
					if (!Closing && AgentSetup) {
						AgentSetup->add_agent(std::move(aEntry), false, std::move(aAnswer));
					}
				},
			.Update =
				[this](core::agent_entry aEntry, qt::agent_programs_panel::answer aAnswer) {
					if (!Closing && AgentSetup) {
						AgentSetup->update_agent(std::move(aEntry), std::move(aAnswer));
					}
				},
			.Remove =
				[this](const std::uint32_t aId) {
					if (!Closing && AgentSetup) {
						AgentSetup->remove_agent(aId);
					}
				},
			.Move =
				[this](const std::uint32_t aId, const int aDelta) {
					if (!Closing && AgentSetup) {
						AgentSetup->move_agent(aId, aDelta);
					}
				},
			.SetRunLimit = [this](const std::uint32_t aMinutes) { return !Closing && AgentSetup && AgentSetup->set_run_limit(aMinutes); },
			.SetProgram =
				[this](const core::setup_agent aAgent, std::string aPath) {
					if (Closing || !AgentSetup) {
						return;
					}
					AgentSetup->set_program(aAgent, std::move(aPath), [this, aAgent](const std::optional<core::agent_wrapper_refusal> aRefusal) {
						if (!Closing && Window != nullptr) {
							Window->agent_programs()->show_answer(aAgent, aRefusal);
						}
					});
				},
			.SetProtocol =
				[this](const core::codex_protocol_choice aChoice) {
					if (!Closing && AgentSetup) {
						AgentSetup->set_codex_protocol(aChoice);
					}
				},
		});
		Window->clone_folders()->set_dispatch(qt::clone_folders_panel::dispatch{
			.SetFolders =
				[this](std::vector<std::string> aFolders) {
					if (!Closing && CloneLinks) {
						CloneLinks->set_clone_folders(std::move(aFolders));
					}
				},
			.Forget =
				[this](const application::repository_link& aLink) {
					if (!Closing && CloneLinks) {
						CloneLinks->forget(aLink);
					}
				},
		});
		const auto answered = [this](const application::review_skill_answer aAnswer) {
			if (!Closing && Window != nullptr) {
				Window->review_skill()->show_answer(aAnswer);
			}
		};
		Window->review_skill()->set_dispatch(qt::review_skill_panel::dispatch{
			.Save =
				[this, answered](std::string aText) {
					if (!Closing && ReviewSkill) {
						ReviewSkill->save(std::move(aText), answered);
					}
				},
			.UseBuiltIn =
				[this, answered](const bool aSaveCopy) {
					if (!Closing && ReviewSkill) {
						ReviewSkill->use_built_in(aSaveCopy, answered);
					}
				},
			.KeepMine =
				[this, answered] {
					if (!Closing && ReviewSkill) {
						ReviewSkill->keep_mine(answered);
					}
				},
		});
		Window->review_skill()->present(ReviewSkill->projection(), std::string{ application::built_in_review_skill() });
		Window->writing_hooks()->present_review_skill_changed(Window->review_skill()->changed());
		present_setup_view();
	}

	/// [[BPR-1753]]. The setup's facts beside the two projections,
	/// and the ready line once it finishes.
	void github_application::implementation::present_setup_view()
	{
		present_settings_check();
		if (Closing || Window == nullptr || !AgentSetup || !CloneLinks || Window->agent_setup() == nullptr) {
			return;
		}
		auto facts = qt::agent_setup_facts{ };
		if (AgentReview) {
			facts.Repository = AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name;
			facts.Sparse     = AgentReview->Sparse;
		}
		const auto* const state = EpochHost != nullptr ? &EpochHost->state() : nullptr;
		if (state != nullptr && state->AcceptedRevision) {
			facts.Head = std::string{ core::canonical_hex(state->AcceptedRevision->Head).view() }.substr(0U, 7U);
		}
		facts.WorkspaceReason = LastWorkspaceProbeReason;
		auto* const setup     = Window->agent_setup();
		setup->present(AgentSetup->projection(), CloneLinks->projection(), facts);
		if (!setup->take_ready()) {
			return;
		}
		auto gets   = qt::agent_ready_facts{ .Repository = facts.Repository, .Head = facts.Head };
		gets.Sparse = facts.Sparse;
		if (const auto& snapshot = WindowRead->accepted_snapshot(); snapshot) {
			gets.Number     = snapshot->PullRequest.Number.value();
			gets.Title      = snapshot->PullRequest.Title;
			gets.BaseBranch = snapshot->PullRequest.BaseRef;
			gets.Files      = snapshot->Files.size();
			gets.Threads    = snapshot->ReviewThreads.size();
			gets.Unresolved = static_cast<std::size_t>(std::ranges::count_if(snapshot->ReviewThreads, [](const auto& aThread) { return !aThread.Resolved; }));
		}
		if (state != nullptr && state->AcceptedRevision) {
			gets.Base = std::string{ core::canonical_hex(state->AcceptedRevision->Base).view() }.substr(0U, 7U);
		}
		if (const auto linked = linked_history_clone(false)) {
			gets.Clone = linked->string();
		}
		Window->agent_tab()->present_ready(setup->ready_title(), setup->ready_text(), qt::agent_ready_gets(gets));
		Window->agent_tab()->focus_question();
	}

	/// Owner look v004 ("The models aren't listing their version numbers").
	/// Claude Code's `--model` takes a model's full name; the list is the
	/// current models, newest first in each line, by name and version.
	auto github_application::implementation::claude_models() -> std::vector<std::pair<std::string, QString>>
	{
		return { { "claude-opus-5-5", QStringLiteral("Opus 5.5") },
				 { "claude-sonnet-5-5", QStringLiteral("Sonnet 5.5") },
				 { "claude-haiku-5-5", QStringLiteral("Haiku 5.5") },
				 { "claude-fable-5-1",
				   QStringLiteral("Fable 5.1") } };
	}

	/// [[BPR-1753]], owner look v002. What each agent runs with, for
	/// the Agent tab's model and effort control and its /model and /effort.
	void github_application::implementation::present_run_settings()
	{
		if (Closing || Window == nullptr || !AgentSetup) {
			return;
		}
		const auto setup = AgentSetup->projection();
		// [[BPR-1753]], owner amendment 2026-10-23. One per added
		// agent, from its entry; its protocol says which settings apply.
		auto shown = std::vector<qt::agent_run_settings>{ };
		for (const auto& entry : setup.Preferences.Agents) {
			auto settings    = qt::agent_run_settings{ };
			settings.Profile = core::entry_profile_id(entry).value();
			settings.Agent   = QString::fromStdString(entry.Name);
			auto program     = entry.Program.empty() ? (entry.Preset == core::setup_agent::codex ? QStringLiteral("the codex found on this computer") : QStringLiteral("the claude found on this computer"))
													 : QString::fromStdString(entry.Program);
			for (const auto& argument : entry.Arguments) {
				program += QStringLiteral(" ") + QString::fromStdString(argument);
			}
			settings.Program = program;
			settings.Model   = entry.Model;
			settings.Effort  = entry.Effort;
			// Owner look v006 ("Codex is totally missing its model settings.
			// And its effort is missing max"): what the harness itself said
			// it offers, once asked; the known levels until then.
			const auto  catalogAt = setup.Catalogs.find(entry.Id);
			const auto* catalog   = catalogAt == setup.Catalogs.end() ? nullptr : &catalogAt->second;
			settings.Listing      = std::ranges::contains(setup.Cataloguing, entry.Id);
			switch (core::entry_settings_program(entry.Protocol).value_or(core::agent_program::claude)) {
			case core::agent_program::claude:
				settings.Models  = claude_models();
				settings.Efforts = catalog != nullptr && !catalog->Efforts.empty() ? catalog->Efforts : core::known_agent_efforts(core::agent_program::claude);
				break;
			case core::agent_program::codex: {
				settings.Efforts = core::known_agent_efforts(core::agent_program::codex);
				if (catalog == nullptr) {
					break;
				}
				for (const auto& model : catalog->Models) {
					settings.Models.emplace_back(model.Model, QString::fromStdString(model.DisplayName));
				}
				// The chosen model's levels, else the default model's, else
				// any model's.
				const auto chosen = std::ranges::find(catalog->Models, entry.Model, &application::agent_model_option::Model);
				const auto named  = std::ranges::find(catalog->Models, true, &application::agent_model_option::IsDefault);
				if (chosen != catalog->Models.end() && !chosen->Efforts.empty()) {
					settings.Efforts = chosen->Efforts;
				} else if (entry.Model.empty() && named != catalog->Models.end() && !named->Efforts.empty()) {
					settings.Efforts = named->Efforts;
				} else if (!catalog->Efforts.empty()) {
					settings.Efforts = catalog->Efforts;
				}
				break;
			}
			}
			const auto choosable     = core::entry_settings_program(entry.Protocol).has_value();
			settings.ModelChoosable  = choosable;
			settings.EffortChoosable = choosable;
			if (!choosable) {
				settings.Models.clear();
				settings.Efforts.clear();
				settings.Note = QStringLiteral("A plain-text harness chooses its own model and effort.");
			}
			shown.push_back(std::move(settings));
		}
		if (!BuiltInProfilesOffered) {
			Window->agent_tab()->present_run_settings(std::move(shown));
			return;
		}
		const auto program = [&setup](const core::setup_agent aAgent) {
			const auto found = std::ranges::find_if(setup.State.Rows, [aAgent](const core::agent_setup_row& aRow) { return aRow.Agent == aAgent; });
			return found == setup.State.Rows.end() ? QString{ } : QString::fromStdString(found->Path);
		};
		auto claude            = qt::agent_run_settings{ };
		claude.Profile         = core::claude_stream_json_profile_id.value();
		claude.Agent           = QStringLiteral("Claude Code");
		claude.Program         = program(core::setup_agent::claude_code);
		claude.ModelChoosable  = true;
		claude.EffortChoosable = true;
		claude.Model           = ClaudeModel;
		claude.Effort          = setup.Preferences.ClaudeEffort;
		claude.Models          = claude_models();
		claude.Efforts         = core::known_agent_efforts(core::agent_program::claude);
		auto codex             = qt::agent_run_settings{ };
		codex.Agent            = QStringLiteral("Codex");
		codex.Program          = program(core::setup_agent::codex);
		codex.ModelChoosable   = true;
		codex.EffortChoosable  = true;
		codex.Model            = setup.Preferences.CodexModel;
		codex.Effort           = setup.Preferences.CodexEffort;
		codex.Efforts          = core::known_agent_efforts(core::agent_program::codex);
		auto appServer         = codex;
		appServer.Profile      = core::codex_app_server_profile_id.value();
		auto jsonl             = codex;
		jsonl.Profile          = core::codex_jsonl_profile_id.value();
		auto harness           = qt::agent_run_settings{ };
		harness.Profile        = core::external_terminal_profile_id.value();
		harness.Agent          = QStringLiteral("The plain-text harness");
		harness.Program        = QString::fromStdString(setup.Preferences.HarnessProgram);
		harness.Note           = QStringLiteral("A plain-text harness chooses its own model and effort.");
		for (auto& builtIn : { claude, appServer, jsonl, harness }) {
			shown.push_back(builtIn);
		}
		Window->agent_tab()->present_run_settings(std::move(shown));
	}

	auto github_application::implementation::apply_run_settings(const std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort) -> bool
	{
		if (Closing || !AgentSetup) {
			return false;
		}
		const auto preferences = AgentSetup->projection().Preferences;
		// [[BPR-1753]]. An added agent keeps its model and effort.
		if (const auto* const entry = core::entry_of_profile(preferences.Agents, core::agent_profile_id{ aProfile }); entry != nullptr) {
			auto edited = *entry;
			if (aModel) {
				edited.Model = std::move(*aModel);
			}
			if (aEffort) {
				edited.Effort = std::move(*aEffort);
			}
			if (core::inspect_agent_entry(edited)) {
				return false;
			}
			AgentSetup->update_agent(std::move(edited));
			return true;
		}
		if (aProfile == core::claude_stream_json_profile_id.value()) {
			if (aModel && !core::storable_claude_model(*aModel)) {
				return false;
			}
			if (aEffort && !aEffort->empty() && !core::valid_agent_effort(core::agent_program::claude, *aEffort)) {
				return false;
			}
			if (aModel && !Profile->WorkbenchPreferences->apply_claude_model(*aModel)) {
				return false;
			}
			if (aModel) {
				ClaudeModel = *aModel;
			}
			const auto taken = !aEffort || AgentSetup->set_run_settings(core::setup_agent::claude_code, { }, std::move(*aEffort));
			present_run_settings();
			return taken;
		}
		if (aProfile == core::codex_app_server_profile_id.value() || aProfile == core::codex_jsonl_profile_id.value()) {
			const auto taken = AgentSetup->set_run_settings(core::setup_agent::codex, aModel.value_or(preferences.CodexModel), aEffort.value_or(preferences.CodexEffort));
			present_run_settings();
			return taken;
		}
		return false;
	}

	/// [[BPR-1749]]. The agent setup saved: its preferred entry, else the
	/// agent it chose.
	auto github_application::implementation::setup_saved_profile(const application::agent_setup_projection& aProjection) -> std::optional<core::agent_profile_id>
	{
		const auto& preferences = aProjection.Preferences;
		const auto  preferredAt = std::ranges::find(preferences.Agents, preferences.PreferredAgent, &core::agent_entry::Id);
		if (preferences.PreferredAgent != 0U && preferredAt != preferences.Agents.end()) {
			return core::entry_profile_id(*preferredAt);
		}
		const auto chosen = aProjection.State.Chosen ? aProjection.State.Chosen : preferences.Chosen;
		return chosen ? std::optional{ core::setup_profile(*chosen, preferences.CodexProtocol) } : std::nullopt;
	}

	/// [[BPR-1823]], owner ruling 2026-10-29. The setup wizard's choice
	/// (an agent picked or added there) is the one the picker takes
	/// without the reviewer choosing it in the picker itself.
	void github_application::implementation::take_setup_choice()
	{
		if (Closing || Window == nullptr || !AgentSetup) {
			return;
		}
		if (const auto saved = setup_saved_profile(AgentSetup->projection())) {
			Window->agent_panel()->choose_profile_from_setup(*saved);
		}
	}

	void github_application::implementation::present_agent_setup(const application::agent_setup_projection& aProjection)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto found = std::vector<core::agent_detection>{ };
		for (const auto& row : aProjection.State.Rows) {
			found.push_back({ .Agent = row.Agent, .Path = row.Path, .How = row.How });
		}
		Window->agent_programs()->present(aProjection.Preferences, std::move(found));
		// [[BPR-1753]], owner amendment 2026-10-23. The launches use
		// the list as published, and the picker offers it.
		AgentEntries->set(aProjection.Preferences.Agents);
		if (aProjection.Preferences.AgentsMoved) {
			// Canonical entries name their program. A legacy fallback must
			// neither replace one nor resurrect a wrapper after Reset.
			ClaudeWrapperSource->set({ });
		}
		Window->agent_panel()->set_run_limit_minutes(aProjection.Preferences.RunLimitMinutes);
		Window->agent_tab()->set_suggestions_shown(!aProjection.Preferences.SuggestionsHidden);
		// [[BPR-1823]], owner ruling 2026-10-29. The saved agent is the
		// first pick only; a setup update never moves the reviewer's
		// choice. The wizard's own choice (take_setup_choice) does.
		const auto saved = setup_saved_profile(aProjection);
		Window->agent_panel()->prefer_profile(saved);
		Window->agent_panel()->offer_profiles(offered_agent_profiles());
		// Owner decision 2026-10-23: Send to Agent offers the same list.
		if (CycleDesktop) {
			CycleDesktop->offer_agents(aProjection.Preferences.Agents);
		}
		present_run_settings();
		present_setup_view();
		if (CycleDesktop) {
			CycleDesktop->prefer_send_profile(saved);
		}
		observe_conversation_agent();
	}
#endif
}
