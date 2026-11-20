#include "github_application/window_composition.hpp"

#include "bpr/application/agent_scratch_folder.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/local_comment.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/quick_ask.hpp"
#include "bpr/qt/agent_configuration_dialogs.hpp"
#include "bpr/qt/conversation_cards_view.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/window_file_drop.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLocale>
#include <QMimeData>
#include <QObject>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <filesystem>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <limits>
#include <utility>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/managed_clone.hpp"
	#include "bpr/application/review_brief.hpp"
	#include "bpr/core/agent_answer_findings.hpp"
	#include "bpr/core/agent_comment_suggestion.hpp"
	#include "bpr/core/agent_entry.hpp"
	#include "bpr/core/agent_finding_card_memory.hpp"
	#include "bpr/core/agent_report_keep.hpp"
	#include "bpr/core/agent_suggestion_range.hpp"
	#include "bpr/core/agent_wrapper.hpp"
	#include "bpr/core/markdown_code.hpp"
	#include "bpr/core/review_comment_range.hpp"
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
		return AgentHost && agent_turn_quiescent();
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

	#if defined(BPR_BUILD_TESTING)
	/// [[BPR-2177]] (owner, 2026-11-14). The folders this run named for its
	/// agents: the fixture agent's, each executable a course set, and each
	/// search path. A program named `claude` or `codex` elsewhere (the person's
	/// own) is refused; in a person's session, so is any program elsewhere.
	auto github_application::implementation::fixture_agent_admission() const -> std::function<std::optional<std::string>(const std::string&)>
	{
		auto folders = std::vector<std::filesystem::path>{ };
		#if defined(BPR_FIXTURE_AGENT_DIRECTORY)
		folders.emplace_back(BPR_FIXTURE_AGENT_DIRECTORY);
		#endif
		for (const auto& executable : { ClaudeAgentExecutable, CodexAgentExecutable, ExternalAgentExecutable }) {
			if (executable) {
				folders.push_back(executable->parent_path());
			}
		}
		for (const auto& search : { ClaudeSearch, CodexSearch }) {
			if (!search) {
				continue;
			}
			auto        start = std::size_t{ 0 };
			const auto& path  = search->PathVariable;
			while (start <= path.size()) {
				const auto end = std::min(path.find_first_of(":;", start), path.size());
				if (end > start) {
					folders.emplace_back(path.substr(start, end - start));
				}
				start = end + 1U;
			}
		}
		for (auto& folder : folders) {
			auto error = std::error_code{ };
			if (auto canonical = std::filesystem::weakly_canonical(folder, error); !error) {
				folder = std::move(canonical);
			}
		}
		return [allowed = std::move(folders), person = FixtureAgentSession](const std::string& aExecutable) -> std::optional<std::string> {
			const auto program = std::filesystem::path{ aExecutable };
			const auto stem    = program.stem().string();
			const auto agent   = stem == "claude" || stem == "codex";
			if (!agent && !person) {
				return aExecutable;
			}
			if (!program.is_absolute()) {
				// A bare name starts only from a folder this run named, never
				// from the PATH the process inherited.
				for (const auto& folder : allowed) {
					auto error = std::error_code{ };
					if (const auto candidate = folder / program; std::filesystem::is_regular_file(candidate, error)) {
						return candidate.string();
					}
				}
				return std::nullopt;
			}
			auto error = std::error_code{ };
			auto where = std::filesystem::weakly_canonical(program, error).parent_path();
			if (error) {
				where = program.parent_path();
			}
			if (std::ranges::find(allowed, where) != allowed.end()) {
				return aExecutable;
			}
			return std::nullopt;
		};
	}
	#endif

	void github_application::implementation::resume_card_open()
	{
		if (Closing || Window == nullptr || !PendingCardOpen) {
			return;
		}
		const auto* const surface  = Window->comparison_surface();
		const auto&       finding  = PendingCardOpen->Request.Finding;
		const auto        onScreen = SelectedDiffPublication && SelectedDiffPublication->Request.File.PresentationPath == finding.Path && surface != nullptr
			&& surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key
			&& !(ReportKeepBehind && (TabLoad || TabDriver));
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

	/// [[BPR-1753]], owner decision 2026-10-23. A Local comment from the
	/// Agent tab (RFC-BPR-0127): on one line of the shown comparison when
	/// `aLine` names one; on the whole file when `aPath` names the file
	/// shown; otherwise on the pull request. It belongs to the review's
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
		// The store gives the note its identity ([[BPR-2187]]).
		auto note = application::local_comment_record{ };
		note.Body = std::move(aText);
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
		} else if (!aPath.empty()) {
			// RFC-BPR-0127. The whole file, which must be the one shown.
			const auto attachment = current_note_attachment();
			if (!attachment || !SelectedDiffPublication || SelectedDiffPublication->Request.File.PresentationPath != aPath || aPath.size() > application::maximum_note_path_bytes) {
				answer(false, QStringLiteral("the file is not open"));
				return;
			}
			note.Attachment = *attachment;
			note.FilePath   = std::move(aPath);
		}
		ReviewLedgerHost->persist(note, std::nullopt, ledger_answer([this, answer](const std::expected<application::local_comment_record, application::local_comment_failure>& aStored) {
									  if (!aStored) {
										  answer(false, QString::fromUtf8(application::name(aStored.error().Reason).data(), static_cast<qsizetype>(application::name(aStored.error().Reason).size())));
										  return;
									  }
									  refresh_agent_private_context_choices();
									  // Owner look v004: Go to comment opens the comment made.
									  answer(true, { }, aStored->Id.value());
								  }));
	}

	/// [[BPR-1753]], owner decision 2026-10-23. The keep action and
	/// Dismiss on one card. A draft is made only at lines of the file on
	/// screen; another file is opened first. RFC-BPR-0127: Keep local, where
	/// no line takes a draft, keeps a Local comment on the finding's file
	/// (opened first) or, for a file the pull request does not change, on
	/// the pull request. Nothing is published.
	namespace {
		/// \brief [[BPR-2298]]. Where a finding's draft goes in the shown
		/// comparison, and what its card says once drafted.
		struct agent_draft_place
		{
			/// The last line, which GitHub anchors a comment to.
			core::source_line_ref        Anchor;
			std::optional<std::uint32_t> StartLine;
			/// The card's words once drafted; empty for the usual ones.
			QString Said;
		};

		/// \brief The finding's lines as a draft's (owner, 2026-11-19: a
		/// suggestion block must be on "the entire block that needs to be
		/// replaced"): its first to last line, widened to the lines its
		/// suggestion replaces when the suggestion rewrites lines beside them
		/// (`decide_agent_suggestion_range`), as one GitHub multi-line
		/// comment where `decide_comment_range` takes it, else its first line
		/// as before. `aLines.Begin` is in the file; the caller checked.
		[[nodiscard]] auto agent_draft_lines(const core::computed_file_diff& aShown, const core::diff_side aSide, const core::line_span aLines, const std::string& aText) -> agent_draft_place
		{
			const auto digest    = aShown.Alignment.Digest;
			const auto lineCount = aSide == core::diff_side::left ? aShown.Alignment.LeftLineCount : aShown.Alignment.RightLineCount;
			auto       first     = aLines.Begin;
			auto       last      = std::min(std::max(aLines.End, aLines.Begin + 1U) - 1U, lineCount - 1U);
			auto       widened   = false;
			auto       unchecked = std::uint32_t{ 0U };
			if (aSide == core::diff_side::right && aShown.RightDocument) {
				const auto range = core::decide_agent_suggestion_range(aText, first + 1U, last + 1U, *aShown.RightDocument);
				if (range.Reading == core::agent_suggestion_range_reading::widened) {
					first   = range.First - 1U;
					last    = range.Last - 1U;
					widened = true;
				} else if (range.Reading == core::agent_suggestion_range_reading::unverified) {
					unchecked = range.SuggestedLines;
				}
			}
			const auto lines = [](const std::uint32_t aFirst, const std::uint32_t aLast) {
				return aFirst == aLast ? QStringLiteral("line %1").arg(aLast + 1U) : QStringLiteral("lines %1–%2").arg(aFirst + 1U).arg(aLast + 1U);
			};
			const auto check = [unchecked](const QString& aWhere) {
				return unchecked == 0U ? QString{ }
									   : QStringLiteral("Drafted on %1. Its suggestion has %2 lines: check which lines it replaces before you publish it.").arg(aWhere).arg(unchecked);
			};
			if (first == last) {
				return { .Anchor = { .Comparison = digest, .Side = aSide, .Line = first }, .StartLine = std::nullopt, .Said = check(lines(first, last)) };
			}
			const auto decision = core::decide_comment_range(aShown.Alignment, { .Comparison = digest, .Side = aSide, .Line = first }, { .Comparison = digest, .Side = aSide, .Line = last });
			if (decision.Refusal != core::comment_range_refusal::none) {
				// GitHub takes several lines only inside one changed region.
				const auto only = QStringLiteral("Drafted on %1 only: GitHub takes a comment on several lines only inside one changed region, and %2 are not.").arg(lines(aLines.Begin, aLines.Begin), lines(first, last));
				return { .Anchor = { .Comparison = digest, .Side = aSide, .Line = aLines.Begin }, .StartLine = std::nullopt, .Said = only };
			}
			const auto said = widened ? QStringLiteral("Drafted on %1, the lines its suggestion replaces. It is a private draft until you publish it.").arg(lines(first, last))
									  : check(lines(first, last));
			return { .Anchor = decision.Anchor, .StartLine = decision.StartLine, .Said = said };
		}
	} // namespace

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
		CardRefusalGone = false;
		if (Closing || tab == nullptr) {
			return;
		}
		const auto& finding   = aRequest.Finding;
		const auto  keepLocal = aRequest.Action == qt::agent_finding_card_action::keep_local;
		const auto  keepText  = [&finding] {
			auto text = finding.Claim;
			if (finding.SuggestedComment && !finding.SuggestedComment->empty()) {
				text += "\n\n" + *finding.SuggestedComment;
			}
			return text;
		};
		const auto kept = [state](const bool aKept, const QString& aWhy, const std::uint64_t aNote) {
			state(aKept ? qt::agent_finding_card_state::noted : qt::agent_finding_card_state::refused, aKept ? QString{ } : QStringLiteral("Not kept: %1.").arg(aWhy), aNote);
		};
		if (keepLocal && WindowRead != nullptr && WindowRead->accepted_snapshot()
			&& core::decide_keep({ .ThreadReply = false, .LineTakesDraft = false, .FileInPullRequest = std::ranges::any_of(WindowRead->accepted_snapshot()->Files, [&finding](const auto& aFile) { return aFile.Path == finding.Path; }) }).Local
				== core::keep_local_place::pull_request) {
			save_agent_note(keepText(), std::nullopt, { }, kept);
			return;
		}
		if (aRequest.Action == qt::agent_finding_card_action::dismiss || aRequest.Action == qt::agent_finding_card_action::undo_dismiss) {
			// A restored card's delivery is not the finding host's to act on.
			if (aRequest.Action == qt::agent_finding_card_action::dismiss && FindingHost != nullptr && !core::is_restored_finding_session(aRequest.Session)) {
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
			&& surface->current_result() != nullptr && surface->current_result()->Alignment.Key == SelectedDiffPublication->Key
			&& !(ReportKeepBehind && (TabLoad || TabDriver));
		// Owner decision 2026-10-23: another file is opened in Review first,
		// then the draft or note is made at its line. [[BPR-2258]]: behind a
		// report the file's own tab is opening already.
		if (!onScreen) {
			if (!ReportKeepBehind && !Window->open_file_in_review(QString::fromStdString(finding.Path))) {
				CardRefusalGone = true;
				state(qt::agent_finding_card_state::refused, QStringLiteral("%1 is not in this pull request at its current head, so there is no line to put it on.").arg(QString::fromStdString(finding.Path)));
				return;
			}
			state(qt::agent_finding_card_state::working);
			PendingCardOpen = pending_card_open{ .Request = aRequest, .Report = aReport };
			QTimer::singleShot(250, Window.get(), [this] { resume_card_open(); });
			return;
		}
		if (keepLocal) {
			save_agent_note(keepText(), std::nullopt, finding.Path, kept);
			return;
		}
		// The line must be one the file has at the current head.
		const auto& shownAlignment = surface->current_result()->Alignment;
		const auto  lineCount      = side == core::diff_side::left ? shownAlignment.LeftLineCount : shownAlignment.RightLineCount;
		if (finding.Lines.Begin >= lineCount) {
			CardRefusalGone = true;
			state(qt::agent_finding_card_state::refused,
				  QStringLiteral("Line %1 is not in %2 at the current head, so nothing was made.").arg(finding.Lines.Begin + 1U).arg(QString::fromStdString(finding.Path)));
			return;
		}
		// [[BPR-1861]]. The draft opens with the Conventional Comments
		// label the agent wrote already in the label control.
		const auto text    = core::read_agent_comment_suggestion(finding.SuggestedComment && !finding.SuggestedComment->empty() ? *finding.SuggestedComment : finding.Claim).Body;
		const auto lines   = agent_draft_lines(*surface->current_result(), side, finding.Lines, text);
		const auto at      = lines.Anchor;
		auto       refused = [state] { state(qt::agent_finding_card_state::refused, QStringLiteral("Not drafted: resolve the draft open at that line first.")); };
		auto       done    = [state, said = lines.Said] { state(qt::agent_finding_card_state::drafted, said); };
		if (ReviewDraft && ReviewDraft->Anchor.Comparison == at.Comparison && ReviewDraft->Anchor.Side == at.Side && ReviewDraft->Anchor.Line == at.Line) {
			promote_private_item(text, std::move(refused), std::move(done), lines.StartLine);
			return;
		}
		// The line's own draft opens once its line is chosen, as Draft
		// comment on a note does ([[BPR-1592]]). [[BPR-2298]]: a range's
		// draft is the one on its last line, which GitHub anchors it to.
		PendingNotePromotion = pending_note_promotion{ .Text = text, .At = at, .Refused = std::move(refused), .Done = std::move(done), .StartLine = lines.StartLine };
		if (!surface->select_conversation_line(at.Side, at.Line)) {
			PendingNotePromotion.reset();
			CardRefusalGone = true;
			state(qt::agent_finding_card_state::refused, QStringLiteral("Not drafted: line %1 is not in the comparison on screen.").arg(at.Line + 1U));
		}
	}

	/// [[BPR-2258]]. A tab gesture made now, as the window's own tab
	/// dispatch makes it on the decision lane.
	void github_application::implementation::apply_tab_now(const core::document_tab_intent& aIntent)
	{
		cancel_review_navigation();
		observe_tab_reading();
		cancel_restoration(false, true);
		TabHost->apply(aIntent);
		if (TabLoad && TabHost->state().Pending != TabLoad) {
			abandon_tab_load();
		}
	}

	auto github_application::implementation::open_behind_report(const std::string& aPath) -> std::uint64_t
	{
		close_behind_report();
		if (Closing || Window == nullptr || TabHost == nullptr || WorkbenchHost == nullptr || !Window->report_shown() || aPath.empty()) {
			return 0U;
		}
		const auto  file   = core::file_identity_of(aPath);
		const auto& tabs   = TabHost->state();
		const auto* report = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		if (!file.valid() || !WorkbenchHost->contains_file(file) || report == nullptr || !report->report()) {
			return 0U;
		}
		const auto reportTab = report->Id;
		auto       before    = std::vector<core::document_tab_id>{ };
		for (const auto& each : tabs.Tabs) {
			before.push_back(each.Id);
		}
		Window->hold_report(true);
		apply_tab_now(core::open_in_new_tab_intent{ .File = file });
		const auto& after = TabHost->state().Tabs;
		const auto  added = std::ranges::find_if(after, [&before](const core::document_tab& aTab) { return std::ranges::find(before, aTab.Id) == before.end(); });
		if (added == after.end()) {
			// At the tab limit the report's own way is kept: nothing opened.
			Window->hold_report(false);
			return 0U;
		}
		const auto serial = ++ReportKeepBehindSerial;
		ReportKeepBehind  = report_keep_behind{ .Serial = serial, .Report = reportTab, .Opened = added->Id };
		apply_tab_now(core::activate_tab_intent{ .Tab = added->Id });
		// A keep that never ends still gives the window back.
		QTimer::singleShot(30'000, Window.get(), [this, serial] { close_behind_report(serial); });
		return serial;
	}

	void github_application::implementation::close_behind_report(const std::uint64_t aSerial)
	{
		if (!ReportKeepBehind || (aSerial != 0U && ReportKeepBehind->Serial != aSerial)) {
			return;
		}
		const auto behind = *ReportKeepBehind;
		ReportKeepBehind.reset();
		if (!Closing && TabHost != nullptr) {
			if (core::find_tab(TabHost->state(), behind.Report) != nullptr) {
				apply_tab_now(core::activate_tab_intent{ .Tab = behind.Report });
			}
			if (core::find_tab(TabHost->state(), behind.Opened) != nullptr) {
				apply_tab_now(core::close_tab_intent{ .Tab = behind.Opened });
			}
		}
		if (!Closing && Window != nullptr) {
			Window->hold_report(false);
		}
	}

	/// [[BPR-2061]]. Whether a finding's lines can take a draft now: its
	/// file is in the pull request at the current head and, when that file is
	/// the one on screen, its first line is in it. A file not on screen is
	/// opened when its turn comes, and a line found gone then is counted so.
	auto github_application::implementation::agent_finding_placeable(const core::agent_finding& aFinding) const -> bool
	{
		if (Closing || Window == nullptr || WindowRead == nullptr || !WindowRead->accepted_snapshot()) {
			return false;
		}
		const auto& files = WindowRead->accepted_snapshot()->Files;
		if (std::ranges::none_of(files, [&aFinding](const auto& aFile) { return aFile.Path == aFinding.Path; })) {
			return false;
		}
		const auto* const surface  = Window->comparison_surface();
		const auto        onScreen = SelectedDiffPublication && SelectedDiffPublication->Request.File.PresentationPath == aFinding.Path && surface != nullptr && surface->current_result() != nullptr
			&& surface->current_result()->Alignment.Key == SelectedDiffPublication->Key;
		if (!onScreen) {
			return true;
		}
		const auto  side      = aFinding.Anchor.Line ? aFinding.Anchor.Line->Side : core::diff_side::right;
		const auto& alignment = surface->current_result()->Alignment;
		return aFinding.Lines.Begin < (side == core::diff_side::left ? alignment.LeftLineCount : alignment.RightLineCount);
	}

	/// [[BPR-2061]], owner 2026-11-10: "Can we get a draft all suggested
	/// comments from an agent response with several." Each card is drafted
	/// in turn through `act_on_agent_card`, exactly as its own Add as draft
	/// comment does, so a batch makes the drafts a reviewer pressing each
	/// card would. Nothing is published.
	void github_application::implementation::draft_all_agent_cards(qt::agent_draft_all_request aRequest, qt::agent_draft_all_done aDone, std::vector<core::report_finding_key> aReportKeys, const std::uint64_t aReportTurn)
	{
		if (Closing || Window == nullptr || DraftAllRun) {
			if (aDone) {
				auto outcome = aRequest.Planned;
				for (std::size_t index = 0U; index < aRequest.Cards.size(); ++index) {
					outcome = core::settle_draft_all(outcome, core::draft_all_result::refused);
				}
				aDone(outcome, false);
			}
			return;
		}
		DraftAllRun = draft_all_run{ .Batch = aRequest.Batch, .Queue = std::move(aRequest.Cards), .Outcome = aRequest.Planned, .Done = std::move(aDone), .ReportKeys = std::move(aReportKeys), .ReportTurn = aReportTurn };
		next_draft_all_card();
	}

	void github_application::implementation::next_draft_all_card()
	{
		if (!DraftAllRun) {
			return;
		}
		auto* const tab = Window != nullptr ? Window->agent_tab() : nullptr;
		if (Closing || tab == nullptr || DraftAllRun->Next >= DraftAllRun->Queue.size()) {
			auto run = std::move(*DraftAllRun);
			DraftAllRun.reset();
			for (; run.Next < run.Queue.size(); ++run.Next) {
				run.Outcome = core::settle_draft_all(run.Outcome, core::draft_all_result::refused);
			}
			const auto report   = !run.ReportKeys.empty();
			const auto undoable = !run.Made.empty() && !report;
			if (report) {
				close_behind_report();
			}
			if (undoable) {
				DraftAllBatches[run.Batch] = std::move(run.Made);
			}
			// Drafting opened each line's draft in Comments; the reply that
			// says what was drafted is shown again. A report's Draft all
			// goes back to the report ([[BPR-2232]]).
			if (report) {
				if (auto* const view = Window != nullptr && !Closing ? Window->report_view() : nullptr; view != nullptr && view->document().Turn == run.ReportTurn) {
					const auto shown = view->document();
					Window->open_report("turn:" + std::to_string(shown.Turn), shown);
				}
			} else if (tab != nullptr && !Closing && Window->open_agent_action() != nullptr) {
				Window->open_agent_action()->trigger();
			}
			if (run.Done) {
				run.Done(run.Outcome, undoable);
			}
			return;
		}
		const auto serial   = ++DraftAllSerial;
		DraftAllRun->Serial = serial;
		const auto request  = DraftAllRun->Queue[DraftAllRun->Next];
		// A card whose draft never settles (its line's editor never opened)
		// is given up after a while, so the batch still ends and says so.
		QTimer::singleShot(20'000, Window.get(), [this, serial] { settle_draft_all_card(serial, core::draft_all_result::refused); });
		const auto reportKey = DraftAllRun->Next < DraftAllRun->ReportKeys.size() ? std::optional{ DraftAllRun->ReportKeys[DraftAllRun->Next] } : std::nullopt;
		// [[BPR-2258]]. A report's Draft all drafts behind the report too.
		const auto behind = reportKey.has_value() ? open_behind_report(request.Finding.Path) : std::uint64_t{ 0U };
		act_on_agent_card(request, [this, tab, serial, reportKey, behind, session = request.Session, ordinal = request.Ordinal](const qt::agent_finding_card_state aState, const QString& aWhy, const std::uint64_t aNote) {
			if (Closing) {
				return;
			}
			if (behind != 0U && aState != qt::agent_finding_card_state::working) {
				close_behind_report(behind);
			}
			const auto gone = CardRefusalGone;
			tab->present_finding_card_state(session, ordinal, aState, aWhy, aNote);
			if (reportKey) {
				settle_report_finding(*reportKey, aState, aWhy, aNote);
			}
			if (aState == qt::agent_finding_card_state::drafted) {
				if (DraftAllRun && DraftAllRun->Serial == serial && ReviewDraft) {
					DraftAllRun->Made.push_back({ .Session = session, .Ordinal = ordinal, .Draft = ReviewDraft->Id, .Revision = ReviewDraftRevision });
				}
				settle_draft_all_card(serial, core::draft_all_result::drafted);
			} else if (aState == qt::agent_finding_card_state::refused) {
				// A line found gone is not offered again.
				if (gone) {
					tab->mark_finding_gone(session, ordinal);
				}
				settle_draft_all_card(serial, gone ? core::draft_all_result::not_in_diff : core::draft_all_result::refused);
			}
		});
	}

	void github_application::implementation::settle_draft_all_card(const std::uint64_t aSerial, const core::draft_all_result aResult)
	{
		if (!DraftAllRun || DraftAllRun->Serial != aSerial) {
			return;
		}
		DraftAllRun->Serial  = 0U;
		DraftAllRun->Outcome = core::settle_draft_all(DraftAllRun->Outcome, aResult);
		++DraftAllRun->Next;
		// The next card starts once this one's draft has settled its editor.
		QTimer::singleShot(0, Window.get(), [this] { next_draft_all_card(); });
	}

	/// [[BPR-2061]]. One Undo removes the batch: each draft it made that is
	/// still at the revision it was made at, and still a draft, is discarded
	/// in one write and its card is open again. A draft edited since is kept.
	void github_application::implementation::undo_draft_all(const std::uint64_t aBatch, qt::agent_draft_all_undo_done aDone)
	{
		const auto found = DraftAllBatches.find(aBatch);
		if (Closing || !SharedDrafts || found == DraftAllBatches.end()) {
			if (aDone) {
				aDone(0U, 0U);
			}
			return;
		}
		auto made = std::move(found->second);
		DraftAllBatches.erase(found);
		auto commands = std::vector<core::revisioned_review_draft_command>{ };
		auto removed  = std::vector<draft_all_made>{ };
		auto kept     = std::size_t{ 0U };
		for (const auto& draft : made) {
			const auto read = SharedDrafts->read(DraftConsumer, draft.Draft);
			if (!read.Accepted || read.Accepted->Revision != draft.Revision || read.Accepted->Draft.State != core::review_draft_state::drafting) {
				++kept;
				continue;
			}
			auto command             = core::revisioned_review_draft_command{ };
			command.Command          = core::review_draft_command{ .Id = draft.Draft, .Kind = core::review_draft_command_kind::discard };
			command.ExpectedRevision = draft.Revision;
			command.Scope            = read.Accepted->Scope;
			commands.push_back(std::move(command));
			removed.push_back(draft);
		}
		if (commands.empty()) {
			if (aDone) {
				aDone(0U, kept);
			}
			return;
		}
		change_review_drafts(std::move(commands), [this, gone = std::move(removed), kept, done = std::move(aDone)](const bool aChanged) {
			if (Closing || Window == nullptr) {
				return;
			}
			if (aChanged) {
				for (const auto& draft : gone) {
					Window->agent_tab()->present_finding_card_state(draft.Session, draft.Ordinal, qt::agent_finding_card_state::open);
				}
				leave_deleted_draft();
			}
			if (done) {
				done(aChanged ? gone.size() : 0U, aChanged ? kept : kept + gone.size());
			}
		});
	}

	namespace {
		[[nodiscard]] auto shown_state(const core::agent_finding_card_disposition aKept) -> qt::agent_finding_card_state
		{
			switch (aKept) {
			case core::agent_finding_card_disposition::open:      return qt::agent_finding_card_state::open;
			case core::agent_finding_card_disposition::drafted:   return qt::agent_finding_card_state::drafted;
			case core::agent_finding_card_disposition::noted:     return qt::agent_finding_card_state::noted;
			case core::agent_finding_card_disposition::dismissed: return qt::agent_finding_card_state::dismissed;
			case core::agent_finding_card_disposition::refused:   return qt::agent_finding_card_state::refused;
			}
			return qt::agent_finding_card_state::open;
		}

		[[nodiscard]] auto kept_state(const qt::agent_finding_card_state aShown) -> std::optional<core::agent_finding_card_disposition>
		{
			switch (aShown) {
			case qt::agent_finding_card_state::open:      return core::agent_finding_card_disposition::open;
			case qt::agent_finding_card_state::working:   return std::nullopt;
			case qt::agent_finding_card_state::drafted:   return core::agent_finding_card_disposition::drafted;
			case qt::agent_finding_card_state::noted:     return core::agent_finding_card_disposition::noted;
			case qt::agent_finding_card_state::dismissed: return core::agent_finding_card_disposition::dismissed;
			case qt::agent_finding_card_state::refused:   return core::agent_finding_card_disposition::refused;
			}
			return std::nullopt;
		}
	} // namespace

	auto github_application::implementation::restored_finding_cards(const std::string& aAnswer) const -> std::vector<qt::agent_restored_finding_card>
	{
		auto cards = std::vector<qt::agent_restored_finding_card>{ };
		if (!AgentFindingCards || aAnswer.find(core::agent_findings_fence) == std::string::npos) {
			return cards;
		}
		const auto restored = application::restore_agent_findings(aAnswer, AgentFindingCards->memory());
		if (!restored) {
			return cards;
		}
		cards.reserve(restored->Cards.size());
		for (const auto& card : restored->Cards) {
			cards.push_back({ .Session = restored->Session,
							  .Ordinal = card.Ordinal,
							  .Finding = card.Finding,
							  .State   = shown_state(card.Kept.State),
							  .Why     = QString::fromStdString(card.Kept.Why),
							  .Note    = card.Kept.Note });
		}
		return cards;
	}

	void github_application::implementation::remember_finding_card(const qt::agent_finding_card_change& aChange)
	{
		const auto state = kept_state(aChange.State);
		if (Closing || !AgentFindingCards || !state || aChange.FindingsBlock.empty() || aChange.Ordinal > std::numeric_limits<std::uint32_t>::max()) {
			return;
		}
		const auto findings = core::digest_agent_findings(aChange.FindingsBlock);
		AgentFindingCards->remember({ .Findings = findings, .Ordinal = static_cast<std::uint32_t>(aChange.Ordinal), .State = *state, .Why = aChange.Why.toStdString(), .Note = aChange.Note });
		// [[BPR-2231]]. A report finding that is this card says so too.
		if (Window != nullptr && Window->report_view() != nullptr) {
			Window->report_view()->settle_keep({ .Findings = findings, .Ordinal = static_cast<std::uint32_t>(aChange.Ordinal), .Card = true });
		}
	}

	void github_application::implementation::settle_report_finding(const core::report_finding_key& aKey, const qt::agent_finding_card_state aState, const QString& aWhy, const std::uint64_t aNote)
	{
		const auto made = aState == qt::agent_finding_card_state::drafted || aState == qt::agent_finding_card_state::noted;
		if (Closing || !AgentFindingCards || (!made && aState != qt::agent_finding_card_state::refused)) {
			return;
		}
		AgentFindingCards->remember(core::report_keep_record(aKey, aState == qt::agent_finding_card_state::noted ? core::report_keep::keep_local : core::report_keep::draft_comment, made, aWhy.toStdString(), aNote));
		if (Window != nullptr && Window->report_view() != nullptr) {
			Window->report_view()->settle_keep(aKey);
		}
	}

	auto github_application::implementation::named_turns(std::vector<qt::agent_thread_turn> aTurns) const -> std::vector<qt::agent_thread_turn>
	{
		if (!PromptTurnNames) {
			return aTurns;
		}
		for (auto& turn : aTurns) {
			if (turn.Name.isEmpty()) {
				if (const auto name = PromptTurnNames->names().recall(turn.Question.toStdString())) {
					turn.Name = QString::fromStdString(*name);
				}
			}
		}
		return aTurns;
	}

	void github_application::implementation::present_remembered_turn_names()
	{
		if (Closing || Window == nullptr || Window->agent_tab() == nullptr || Window->agent_tab()->state() == qt::agent_tab_state::working) {
			return;
		}
		auto* const tab   = Window->agent_tab();
		const auto  shown = tab->thread_turns();
		if (auto named = named_turns(shown); named != shown) {
			tab->present_thread(named);
		}
	}

	void github_application::implementation::present_remembered_finding_cards()
	{
		if (Closing || Window == nullptr || !AgentFindingCards) {
			return;
		}
		// A thread shown before the kept states were read made its cards
		// open; each kept one takes its state now.
		for (const auto& kept : AgentFindingCards->memory().records()) {
			Window->agent_tab()->present_finding_card_state(core::restored_finding_session(kept.Findings), kept.Ordinal, shown_state(kept.State), QString::fromStdString(kept.Why), kept.Note);
		}
		// [[BPR-2231]]. A report shown before they were read says them now.
		if (Window->report_view() != nullptr) {
			Window->report_view()->refresh_keeps();
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
	#if defined(BPR_BUILD_TESTING)
		// [[BPR-2177]], look ask-v002: the picker took the first agent it
		// offered (Codex, which nothing answered) before the setup had chosen.
		// A person's session on a tests-on build offers the fixture agent
		// alone, Claude's protocol first.
		if (FixtureAgentSession) {
			std::erase_if(offered, [](const core::agent_profile& aProfile) { return !aProfile.DisplayName.starts_with("Fixture agent"); });
			// [[BPR-2153]]: its added entry first (the one a person's setup
			// would choose), then the built-in Claude.
			std::ranges::stable_partition(offered, [](const core::agent_profile& aProfile) { return core::is_agent_entry_profile(aProfile.Id) || aProfile.Id == core::claude_stream_json_profile_id; });
		}
	#endif
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
		AgentWorkspaceSweepAfterHistory = AgentWorkspaceSweepAfterHistory || aChoice.has_value();
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
		// [[BPR-2293]]. A chosen or new conversation replaced the one before:
		// a workspace no selected conversation reaches now expires.
		if (AgentWorkspaceSweepAfterHistory && !aView.Loading) {
			AgentWorkspaceSweepAfterHistory = false;
			Profile->sweep_agent_workspaces(application::retained_workspace_sweep::policy);
		}
		auto* tab = Window->agent_tab();
		tab->present_native_history(aView);
		// Native history and the legacy conversation settle independently.
		// Both must be ready before the existing launch guard can admit Send.
		const auto loading = aView.Loading || (AgentConversation && !AgentConversation->is_loaded());
		tab->ask_button()->setProperty("conversationLoading", loading);
		tab->refresh();
		if (aView.Loading) {
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
			turns.push_back({ .Question = QString::fromStdString(turn.Asked),
							  .Answer   = QString::fromStdString(turn.Answered),
							  .Footer   = QString::fromStdString(turn.Ending),
							  .Outcome  = turn.Outcome,
							  .Cards    = restored_finding_cards(turn.Answered),
							  .Steered  = turn.Steered,
							  .Seconds  = turn.Seconds ? std::optional<qint64>{ *turn.Seconds } : std::nullopt });
		}
		tab->present_thread(named_turns(std::move(turns)));
		// [[BPR-1999]]. The conversation's cost, kept with its history.
		tab->present_session_cost(aView.SessionCost);
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
		// [[BPR-2007]]; re-review N3, N4. This pull request's folders, as
		// the store and the disk have them; another's stay known.
		derive_agent_scratch();
		present_run_settings();
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
		// RFC-BPR-0127. Ask agent on my own unsent comment, Local or Draft:
		// one path, as a saved note was asked about.
		if (const auto& draft = qt::conversation_cards_view::draft_identity(); aIdentity == draft) {
			if (ReviewDraft) {
				ask_agent_about_draft(*ReviewDraft);
			}
			return;
		} else if (aIdentity.starts_with(draft + ":")) {
			auto       id     = std::uint64_t{ 0U };
			const auto digits = std::string_view{ aIdentity }.substr(draft.size() + 1U);
			if (const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), id); error == std::errc{ } && end == digits.data() + digits.size()) {
				const auto drafts = saved_draft_values();
				if (const auto found = std::ranges::find(drafts, core::review_draft_id{ id }, &core::review_comment_draft::Id); found != drafts.end()) {
					ask_agent_about_draft(*found);
				}
			}
			return;
		}
		if (const auto local = qt::conversation_cards_view::local_comment_of(aIdentity)) {
			const auto found = std::ranges::find(AcceptedPrivateNotes, application::local_comment_id{ *local }, &application::local_comment_record::Id);
			if (found != AcceptedPrivateNotes.end()) {
				ask_agent_about_note(*found);
			}
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
			// RFC-BPR-0116. Its prompts offer Was this addressed? when it is.
			.Addressed = ReturningReview.Threads.contains(aIdentity),
		};
		Window->agent_tab()->set_conversation_chip(std::move(chip));
		Window->open_agent_question();
	}

	/// [[BPR-1773]]. Ask agent on a saved private note: as a thread's,
	/// but the note's words reach the agent only as the copy saved in
	/// Private history when the question is sent.
	void github_application::implementation::ask_agent_about_note(const application::local_comment_record& aNote)
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
			.Text    = QStringLiteral("Your local comment on %1").arg(where),
			.Detail  = QStringLiteral("When you ask, the comment is saved in Private history as it is then, and the agent gets that copy and the lines it is on. Nothing is published."),
			.Replies = false,
		});
		Window->open_agent_question();
	}

	/// RFC-BPR-0127. Ask agent on my draft, as on my Local comment: its
	/// words reach the agent only as the copy saved in Private history when
	/// the question is sent.
	void github_application::implementation::ask_agent_about_draft(const core::review_comment_draft& aDraft)
	{
		if (Closing || Window == nullptr || !WindowRead->accepted_snapshot() || aDraft.shared()) {
			return;
		}
		const auto path   = SelectedDiffPublication ? SelectedDiffPublication->Request.File.PresentationPath : std::string{ };
		AskedConversation = asked_conversation{
			.Identity    = "draft:" + std::to_string(aDraft.Id.value()),
			.PullRequest = WindowRead->accepted_snapshot()->PullRequest.Number.value(),
			.Draft       = aDraft.Id,
			.DraftPath   = path,
			.DraftLine   = path.empty() || aDraft.ReplyToThread ? std::nullopt : std::optional{ std::pair{ aDraft.Anchor.Side, aDraft.Anchor.Line + 1U } },
		};
		auto where = path.empty() ? QStringLiteral("this pull request") : QString::fromStdString(path);
		if (AskedConversation->DraftLine) {
			where = QStringLiteral("%1 line %2").arg(where).arg(AskedConversation->DraftLine->second);
		}
		Window->agent_tab()->set_conversation_chip(qt::agent_conversation_chip{
			.Key     = AskedConversation->Identity,
			.Text    = QStringLiteral("Your draft on %1").arg(where),
			.Detail  = QStringLiteral("When you ask, the comment is saved in Private history as it is then, and the agent gets that copy and the lines it is on. Nothing is published."),
			.Replies = false,
		});
		Window->open_agent_question();
	}

	auto github_application::implementation::asked_note() const -> const application::local_comment_record*
	{
		if (!AskedConversation || !AskedConversation->Note) {
			return nullptr;
		}
		const auto found = std::ranges::find(AcceptedPrivateNotes, *AskedConversation->Note, &application::local_comment_record::Id);
		return found == AcceptedPrivateNotes.end() ? nullptr : &*found;
	}

	auto github_application::implementation::own_comment_asked() const -> std::optional<asked_own_comment>
	{
		if (!AskedConversation) {
			return std::nullopt;
		}
		if (const auto* const note = asked_note(); note != nullptr) {
			auto own = asked_own_comment{ .Body = note->Body, .Path = note->FilePath.value_or(std::string{ }) };
			if (note->Attachment && note->Attachment->Line && note->FilePath) {
				own.Line = std::pair{ note->Attachment->Line->Side, note->Attachment->Line->Line + 1U };
			}
			return own;
		}
		if (AskedConversation->Draft) {
			const auto drafts = saved_draft_values();
			const auto found  = std::ranges::find(drafts, *AskedConversation->Draft, &core::review_comment_draft::Id);
			if (found != drafts.end() && !found->shared()) {
				return asked_own_comment{ .Body = found->Body, .Path = AskedConversation->DraftPath, .Line = AskedConversation->DraftLine };
			}
			if (ReviewDraft && ReviewDraft->Id == *AskedConversation->Draft && !ReviewDraft->shared()) {
				return asked_own_comment{ .Body = ReviewDraft->Body, .Path = AskedConversation->DraftPath, .Line = AskedConversation->DraftLine };
			}
		}
		return std::nullopt;
	}

	/// [[BPR-1773]], owner look 2026-10-27. Where a question sent now
	/// would have its answer drafted: core places it from the review
	/// thread asked about and the lines selected beside the code (none
	/// on Overview); the turn keeps the key.
	auto github_application::implementation::decide_answer_draft() -> qt::agent_answer_draft_target
	{
		auto       draft  = agent_answer_draft{ };
		const auto thread = AskedConversation && !AskedConversation->Note && !AskedConversation->Draft && AskedConversation->Replies;
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
		// RFC-BPR-0127. The answer's one keep action: a draft where its
		// thread or lines take one, else Keep local on the pull request.
		const auto keep = core::decide_keep({ .ThreadReply = thread, .LineTakesDraft = lines, .FileInPullRequest = false });
		auto       tip  = QString{ };
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
		return { .Key = std::move(key), .Tip = tip, .Local = keep.Kind == core::keep_kind::local };
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
		if (AskedConversation->Note || AskedConversation->Draft) {
			const auto own = own_comment_asked();
			if (!own) {
				return std::nullopt;
			}
			auto input = core::conversation_input{ .Subject = core::conversation_subject::private_note, .Path = own->Path };
			if (own->Line && !own->Path.empty()) {
				const auto line       = own->Line->second;
				const auto side       = own->Line->first;
				input.Line            = line;
				constexpr auto around = 6U;
				if (const auto lines = Window->compared_lines(own->Path, side, line > around ? line - around : 1U, line + around); lines && !lines->second.empty()) {
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
				// Refresh here too: an identical native-history observation can
				// coalesce without publishing another completion.
				auto* tab = Window->agent_tab();
				tab->ask_button()->setProperty("conversationLoading", !aView.Loaded || AgentNativeHistory.Loading);
				tab->refresh();
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
			shown.push_back({ .Question = QString::fromStdString(turn.Asked),
							  .Answer   = QString::fromStdString(turn.Answered),
							  .Footer   = QString::fromStdString(turn.Ending),
							  .Outcome  = turn.Outcome,
							  .Cards    = restored_finding_cards(turn.Answered),
							  .Steered  = turn.Steered,
							  .Seconds  = turn.Seconds ? std::optional<qint64>{ *turn.Seconds } : std::nullopt });
		}
		Window->agent_tab()->present_thread(named_turns(std::move(shown)));
		if (aView.Loaded) {
			// [[BPR-2181]]. Turns the session's runtime ran with no window join it.
			adopt_unshown_turns();
		}
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
			if (std::exchange(AgentSetupWorkspaceWaiting, false)) {
				AgentSetup->set_workspace_check({ });
			}
			AgentSetup->stop();
		}
	}

	auto github_application::implementation::settings_check_decision() const -> core::agent_check_decision
	{
		return AgentSetup ? core::decide_agent_check(application::agent_check_facts_of(AgentSetup->projection(), static_cast<bool>(AgentReview))) : core::agent_check_decision{ };
	}

	/// [[BPR-1859]]. Settings › Agents: Check agent as the
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
		AgentSetupWorkspaceWaiting = false;
		if (AgentHost == nullptr || EpochHost == nullptr || !AgentReview) {
			failed(std::nullopt);
			return;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority || !state.AcceptedRevision || !state.AcceptedComparison
			|| state.Authority->Host != AgentReview->Host || state.Authority->Repository != AgentReview->Repository || state.Authority->PullRequest != AgentReview->PullRequest) {
			// [[BPR-2256]]. The setup has this review's policy before its
			// comparison is accepted, so a check can ask now: the workspace
			// waits for it (refresh_agent_authority) instead of failing.
			AgentSetupWorkspaceWaiting = true;
			LastWorkspaceProbeReason.reset();
			AgentSetup->set_workspace_check({ .State = core::setup_check_state::running });
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
		// [[BPR-2261]]. The result is kept under the key the pull request's
		// workspace is kept under, across restarts.
		auto workspaceKey = application::agent_workspace_key(authority, binding);
		LastWorkspaceProbeReason.reset();
		AgentSetup->set_workspace_check({ .State = core::setup_check_state::running });
		AgentHost->probe_workspace(state.ApplicationSession, authority, std::move(binding), linked_history_clone(false), [this, failed, stop, generation, key = std::move(workspaceKey)](std::expected<application::prepared_workspace, application::agent_workspace_error> aProbed) {
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
										   AgentSetup->settle_workspace_check(key, { .State = core::setup_check_state::passed });
										   return;
									   }
									   AgentSetup->settle_workspace_check(key, core::fail_check(core::agent_setup_failure::workspace_unavailable));
									   failed(aProbed.error().Code == application::agent_workspace_failure::unavailable ? std::optional{ aProbed.error().Reason } : std::nullopt, aProbed.error().Detail); }, stop);
	}

	/// [[BPR-1750]]. A repository with a serving link or the private
	/// copy has its Repository step done.
	void github_application::implementation::present_clone_links(const application::clone_link_projection& aProjection)
	{
		// [[BPR-2290]]. The review's history acquisitions read the linked
		// clone before they ask the host.
		if (!Closing && CloneLinks && EpochHost && AgentReview) {
			EpochHost->set_history_clone(AgentReview->Repository, linked_history_clone(false));
		}
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
		// [[BPR-2007]]. The footer picker's grant item and Open scratch folder.
		tab->run_settings_source()->ToggleScratch = [this](const std::uint64_t aProfile, const bool aOn) { toggle_agent_scratch(aProfile, aOn); };
		tab->run_settings_source()->OpenScratch   = [this](const std::uint64_t aProfile) { open_agent_scratch(aProfile); };
		tab->run_settings_source()->ToggleBuilds  = [this](const std::uint64_t aProfile, const bool aOn) { toggle_agent_builds(aProfile, aOn); };
		// [[BPR-1753]], owner decision (b). Each turn that ends is
		// kept for the pull request, and Clear session keeps none.
		tab->set_thread_observer([this](std::vector<qt::agent_thread_turn> aTurns) {
			if (Closing || !AgentConversation) {
				return;
			}
			// RFC-BPR-0125 (AC07). Each suggested prompt's turn keeps its name.
			// [[BPR-2237]]. One whose text the reviewer changed reads back as
			// written: the name alone would hide their words.
			if (PromptTurnNames) {
				for (const auto& turn : aTurns) {
					if (!turn.Name.isEmpty() && !turn.Edited) {
						PromptTurnNames->remember({ .Asked = turn.Question.toStdString(), .Name = turn.Name.toStdString() });
					}
				}
			}
			auto kept = std::vector<application::agent_conversation_turn>{ };
			kept.reserve(aTurns.size());
			for (const auto& turn : aTurns) {
				kept.push_back({ .Asked = turn.Question.toStdString(), .Answered = turn.Answer.toStdString(), .Ending = turn.Footer.toStdString(), .Outcome = turn.Outcome, .Steered = turn.Steered });
			}
			if (AgentNativeHistory.Scope.empty()) {
				AgentConversation->record(std::move(kept));
			}
		});
		// Owner decision 2026-10-23 ("Yep build it like that"): a card's
		// actions, and a plain reply's one keep action (RFC-BPR-0127). Each
		// makes a private draft or Local comment in the review, never part
		// of a session.
		// Owner decision 2026-10-23: switching agents shows that agent's own
		// conversation about the open pull request.
		QObject::connect(Window->agent_panel()->profile_selector(), &QComboBox::currentIndexChanged, Window.get(), [this] { observe_conversation_agent(); });
		tab->set_finding_card_dispatch([this](const qt::agent_finding_card_request& aRequest) { act_on_agent_card(aRequest); });
		// [[BPR-2061]]. Draft all and its Undo, and whether a suggestion's
		// lines are still in the pull request.
		tab->set_draft_all_dispatch([this](qt::agent_draft_all_request aRequest, qt::agent_draft_all_done aDone) { draft_all_agent_cards(std::move(aRequest), std::move(aDone)); },
									[this](const std::uint64_t aBatch, qt::agent_draft_all_undo_done aDone) { undo_draft_all(aBatch, std::move(aDone)); });
		tab->set_finding_placeability([this](const core::agent_finding& aFinding) { return agent_finding_placeable(aFinding); });
		tab->set_finding_card_observer([this](const qt::agent_finding_card_change& aChange) { remember_finding_card(aChange); });
		tab->set_answer_note_dispatch([this](const QString& aText, qt::agent_answer_note_done aDone) { save_agent_note(aText.toStdString(), std::nullopt, { }, std::move(aDone)); });
		// [[BPR-1773]]. Ask agent on a thread attaches it; the chip's
		// close removes it. Owner look 2026-10-27: every answer's Draft
		// comment writes it as a private draft where core places it when
		// it was asked, and Go to draft opens that draft.
		Window->set_conversation_ask_dispatch([this](const std::string& aIdentity) { ask_agent_about_thread(aIdentity); });
		// RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). The returning questions:
		// a thread's Was this addressed?, the Pull Request menu's What
		// changed since my review, and Ask more on either's answer.
		Window->set_conversation_addressed_dispatch([this](const std::string& aIdentity) { ask_was_addressed(aIdentity); });
		Window->set_what_changed_dispatch([this] { ask_what_changed(); });
		// [[BPR-2251]]. Its Agent tab suggestion reads at the press too.
		Window->agent_tab()->set_latest_upload_dispatch([this] { ask_what_changed(); });
		// RFC-BPR-0117 ([[BPR-2076]]). Explain this file and its card.
		bind_explain_file();
		// RFC-BPR-0128 ([[BPR-2164]]). The Ask controls and menus.
		bind_quick_ask();
		// [[BPR-2286]]. Ask follow-up and Ask about this.
		bind_follow_up();
		// [[BPR-2248]]. The Commits tab's agent prompts.
		bind_commit_asks();
		// RFC-BPR-0118 ([[BPR-2078]]). Suggest the change on a draft comment.
		bind_suggest_change();
		tab->set_ask_more_dispatch([this](const QString& aQuestion, const QString& aAnswer) {
			AskMoreSeed = std::pair{ aQuestion.toStdString(), aAnswer.toStdString() };
		});
		tab->set_conversation_dismiss([this] { dismiss_asked_conversation(); });
		tab->set_answer_draft_target([this] { return decide_answer_draft(); });
		tab->set_answer_draft_dispatch([this](const std::string& aKey, const QString& aText, qt::agent_answer_draft_done aDone) { draft_agent_answer(aKey, aText, std::move(aDone)); });
		tab->set_draft_navigation([this](const std::string& aKey) { go_to_agent_answer_draft(aKey); });
		// Owner look v004. Go to comment; the files and lines a question or an
		// answer names open in Review; the suggestions' toggle is kept.
		tab->set_note_navigation([this](const std::uint64_t aNote) {
			if (!Closing && Window != nullptr) {
				Window->go_to_private_note(aNote);
			}
		});
		// [[BPR-1998]]. The changed files the core's recogniser resolves to;
		// this application cannot show an unchanged file at a line, so such a
		// reference is not a link.
		tab->set_line_links(
			[this] { return core::agent_reference_files{ .Changed = Closing || Window == nullptr ? std::vector<std::string>{ } : Window->review_changed_files() }; },
			[this](const QString& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const bool aBefore) {
				if (!Closing && Window != nullptr) {
					static_cast<void>(Window->go_to_file_line(aPath, aFirst, aBefore, aLast));
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
		// RFC-BPR-0125; [[BPR-2181]] (AC13, Prompts review B1). The Agent
		// tab's and the tour chats' offers are decided here, once, from the
		// facts the window holds (the review's included); the chats are
		// handed them.
		tab->set_prompt_source([this](const core::prompt_chat_view& aView) { return chat_prompts(aView); });
		if (auto* const editor = Window->tour_editor(); editor != nullptr && editor->side_chat() != nullptr) {
			editor->side_chat()->set_prompt_source([this](const core::prompt_chat_view& aView) { return chat_prompts(aView); });
			// [[BPR-2235]]. A tour chat's prompt turns read back by their names.
			editor->side_chat()->set_turn_name_source([this](const std::string& aAsked) {
				const auto name = PromptTurnNames ? PromptTurnNames->names().recall(aAsked) : std::nullopt;
				return name ? QString::fromStdString(*name) : QString{ };
			});
		}
		tab->set_suggestion_context([this] {
			auto context = qt::agent_suggestion_context{ };
			// RFC-BPR-0125. The names a suggested prompt may fill in and the
			// facts its conditions read, from what the window already holds.
			if (!Closing && WindowRead != nullptr && WindowRead->accepted_snapshot() && AgentReview) {
				const auto& record      = WindowRead->accepted_snapshot()->PullRequest;
				auto        pullRequest = core::prompt_pull_request{ };
				pullRequest.Number      = AgentReview->Number.value();
				pullRequest.Title       = record.Title;
				pullRequest.Url         = "https://" + AgentReview->Hostname + "/" + AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name + "/pull/" + std::to_string(AgentReview->Number.value());
				pullRequest.Author      = record.Author ? record.Author->Login : std::string{ };
				pullRequest.Base        = record.BaseRef;
				pullRequest.Head        = record.HeadRef;
				context.PullRequest     = std::move(pullRequest);
				context.Repository      = AgentReview->Locator.Owner + "/" + AgentReview->Locator.Name;
				context.Draft           = record.Draft;
				// [[BPR-2181]] (AC13). Whether I wrote it, from the signed-in
				// viewer the read names; unknown when it names none.
				if (const auto& viewer = WindowRead->accepted_snapshot()->Viewer.Login; !viewer.empty() && record.Author) {
					context.AuthorIsMe = record.Author->Login == viewer;
				}
				// [[BPR-2221]]. Whether a check on the head failed is core's,
				// from each check's end as the read gives it.
				const auto& snapshot = *WindowRead->accepted_snapshot();
				auto        checks   = std::vector<core::quick_ask_check>{ };
				checks.reserve(snapshot.Checks.size());
				for (const auto& check : snapshot.Checks) {
					using conclusion = application::github_check_conclusion;
					using end        = core::quick_ask_check_end;
					const auto ended = check.StatusError                              ? end::errored
						: check.Status != application::github_check_status::completed ? end::unfinished
						: check.Conclusion == conclusion::success                     ? end::success
						: check.Conclusion == conclusion::failure                     ? end::failure
						: check.Conclusion == conclusion::neutral                     ? end::neutral
						: check.Conclusion == conclusion::cancelled                   ? end::cancelled
						: check.Conclusion == conclusion::skipped                     ? end::skipped
						: check.Conclusion == conclusion::timed_out                   ? end::timed_out
						: check.Conclusion == conclusion::action_required             ? end::action_required
						: check.Conclusion == conclusion::stale                       ? end::stale
																					  : end::unfinished;
					checks.push_back({ .Commit = std::string{ check.EvaluatedOid.value() }, .End = ended });
				}
				const auto complete = snapshot.ChecksAvailability == application::github_collection_availability::available
					&& snapshot.StatusesAvailability == application::github_collection_availability::available;
				context.ChecksFailing = core::decide_checks_failing(checks, record.HeadOid.value(), complete);
			}
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
			.ChooseEntry =
				[this](const std::uint32_t aId) {
					if (!Closing && AgentSetup) {
						AgentSetup->choose_entry(aId);
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
		// [[BPR-2198]] (owner, 2026-11-15). Import Profile Settings… and
		// Export Profile Settings… are on Settings › Profiles.
		Window->profiles_settings()->set_share_handlers([this] { import_agent_configuration(); }, [this] { export_agent_configuration(); });
		// [[BPR-2283]] (owner, 2026-11-19). Appearance's Import… is the same
		// import: a theme rides a .bprsettings file.
		if (auto* const appearance = Window->appearance_settings(); appearance != nullptr) {
			appearance->set_import_handler([this] { import_agent_configuration(); });
		}
		// [[BPR-2282]] (owner, 2026-11-19). A file dropped onto the window
		// opens the same import, over the window.
		new qt::window_file_drop{ *Window, { .Admit = dropped_file, .Dropped = [this](const QString& aPath) { import_agent_configuration(aPath); }, .Say = [this](const QString& aText) {
											if (Window != nullptr) {
												Window->statusBar()->showMessage(aText, 8000);
											} } } };
		Window->agent_programs()->set_dispatch(qt::agent_programs_panel::dispatch{
			// [[BPR-1859]]. Check agent is the setup's check, and its Stop
			// the setup's Stop.
			.Check     = [this] { check_agent_from_settings(); },
			.StopCheck = [this] { stop_agent_setup_check(); },
			// [[BPR-2051]]. The roster's Check all and a row's own check.
			.CheckAll =
				[this] {
					if (!Closing && AgentSetup) {
						AgentSetup->check_all();
					}
				},
			.CheckEntry =
				[this](const std::uint32_t aId) {
					if (!Closing && AgentSetup) {
						AgentSetup->check_entry(aId);
					}
				},
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
			.SetRunLimit        = [this](const std::uint32_t aMinutes) { return !Closing && AgentSetup && AgentSetup->set_run_limit(aMinutes); },
			.SetFollowUpSteer   = [this](const bool aSteer) {
                if (!Closing && AgentSetup) {
                    AgentSetup->set_follow_up_steer(aSteer);
                } },
			.SetScratchPolicy   = [this](const std::uint32_t aLimitMiB, const std::uint32_t aKeepDays) {
				const auto taken = !Closing && AgentSetup && AgentSetup->set_scratch_policy(aLimitMiB, aKeepDays);
				present_run_settings();
				return taken; },
			.ClearScratch       = [this] { clear_agent_scratch(std::nullopt, true); },
			.SetWorkspacePolicy = [this](const std::uint32_t aKeepDays, const std::uint32_t aCapGb) {
				const auto taken = !Closing && AgentSetup && AgentSetup->set_workspace_policy(aKeepDays, aCapGb);
				if (taken) {
					Profile->sweep_agent_workspaces(application::retained_workspace_sweep::policy);
				}
				return taken; },
			.RemoveWorkspaces   = [this] {
				if (!Closing) {
					Profile->sweep_agent_workspaces(application::retained_workspace_sweep::remove_all, true);
				} },
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
			// [[BPR-2254]]. The saved agents could not be read: the existing
			// recovery is Storage & privacy's Troubleshooting ([[BPR-1602]]).
			.OpenTroubleshooting =
				[this] {
					if (!Closing && Window != nullptr) {
						Window->open_settings(qt::settings_page::storage_and_privacy);
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
		// Owner look e97c6a3d: scratch folders unused past the kept days go,
		// once a session, before the first question can use one.
		if (!ScratchSwept && !AgentScratchRoot.empty()) {
			ScratchSwept = true;
			if (setup.Preferences.ScratchKeepDays != 0U) {
				const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				clear_agent_scratch(now - static_cast<std::int64_t>(setup.Preferences.ScratchKeepDays) * 86400, false);
			}
			present_scratch_usage();
			present_agent_workspaces();
		}
		// [[BPR-1753]], owner amendment 2026-10-23. One per added
		// agent, from its entry; its protocol says which settings apply.
		auto shown = std::vector<qt::agent_run_settings>{ };
		for (const auto& entry : setup.Preferences.Agents) {
			auto settings    = qt::agent_run_settings{ };
			settings.Profile = core::entry_profile_id(entry).value();
			settings.Agent   = QString::fromStdString(entry.Name);
			// Owner look e97c6a3d: the program setup found goes unsaid; a
			// program or arguments of the reviewer's own are named.
			auto program = QString::fromStdString(entry.Program);
			for (const auto& argument : entry.Arguments) {
				program += (program.isEmpty() ? QString{ } : QStringLiteral(" ")) + QString::fromStdString(argument);
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
			// [[BPR-2139]]. Said in the tooltip when the agent's MCP servers load.
			settings.McpOn = entry.McpServers == core::agent_mcp_servers::own && core::entry_settings_program(entry.Protocol).has_value();
			// [[BPR-2007]], [[BPR-2008]], RFC-BPR-0104 as amended 2026-11-11
			// (owner look e97c6a3d, review findings 4 to 6). The two toggles,
			// decided from the facts the launch is decided from.
			if (const auto grantProgram = core::entry_settings_program(entry.Protocol); grantProgram && AgentReview) {
				const auto* const own    = Window->agent_panel()->existing_checkout_action();
				const auto        grant  = agent_grant_for(entry, own != nullptr && own->isChecked());
				settings.GrantApplies    = grant.Applies;
				settings.ScratchText     = QString::fromStdString(core::agent_scratch_toggle_text(grant));
				settings.ScratchOn       = core::agent_scratch_toggle_checked(grant);
				settings.ScratchEnabled  = core::agent_scratch_toggle_enabled(grant);
				settings.ScratchTip      = QString::fromStdString(core::agent_scratch_tooltip(grant, grantProgram, setup.Preferences.ScratchLimitMiB));
				settings.BuildsOn        = core::agent_builds_toggle_checked(grant);
				settings.BuildsEnabled   = core::agent_builds_toggle_enabled(grant);
				settings.BuildsTip       = QString::fromStdString(core::agent_builds_tooltip(grant, grantProgram));
				const auto keyed         = grant_key(entry.Id);
				settings.ScratchOpenable = keyed && AgentScratchFolders.contains(*keyed);
			}
			// Owner look e97c6a3d ("I'm not even sure if type /MODEL or type
			// /EFFORT is true in general with custom agents"). The question
			// field's /model and /effort set what BetterPullReview passes; they
			// are said to work only for an agent that runs the claude or codex
			// it names (no program of its own, or one of those by name) and
			// whose own arguments do not set the model or effort already.
			if (const auto runtime = core::entry_settings_program(entry.Protocol); runtime) {
				const auto name       = std::filesystem::path{ entry.Program }.filename().string();
				const auto runsItself = entry.Program.empty() || name == (*runtime == core::agent_program::codex ? "codex" : "claude");
				const auto sets       = [&entry](const std::initializer_list<std::string_view> aKeys) {
					return std::ranges::any_of(entry.Arguments, [&aKeys](const std::string& aArgument) {
						return std::ranges::any_of(aKeys, [&aArgument](const std::string_view aKey) { return aArgument == aKey || aArgument.starts_with(std::string{ aKey } + "=") || aArgument.find(aKey) != std::string::npos; });
					});
				};
				if (runsItself && !sets({ "--model", "-m", "model=" }) && !std::ranges::contains(entry.Arguments, std::string{ "-m" })) {
					settings.ModelHint = QStringLiteral("Or type /model NAME in the Agent tab's question field.");
				}
				if (runsItself && !sets({ "--effort", "model_reasoning_effort" })) {
					settings.EffortHint = QStringLiteral("Or type /effort LEVEL in the Agent tab's question field.");
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
		// [[BPR-2177]], look ask-v003: the footer names a tests-on build's
		// fixture agent as the setup does.
		const auto named = [&setup](const core::setup_agent aAgent, const QString& aPlain) {
			const auto found = setup.Names.find(aAgent);
			return found == setup.Names.end() ? aPlain : QString::fromStdString(found->second);
		};
		auto claude            = qt::agent_run_settings{ };
		claude.Profile         = core::claude_stream_json_profile_id.value();
		claude.Agent           = named(core::setup_agent::claude_code, QStringLiteral("Claude Code"));
		claude.Program         = program(core::setup_agent::claude_code);
		claude.ModelChoosable  = true;
		claude.EffortChoosable = true;
		claude.Model           = ClaudeModel;
		claude.Effort          = setup.Preferences.ClaudeEffort;
		claude.Models          = claude_models();
		claude.Efforts         = core::known_agent_efforts(core::agent_program::claude);
		auto codex             = qt::agent_run_settings{ };
		codex.Agent            = named(core::setup_agent::codex, QStringLiteral("Codex"));
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
		// [[BPR-2254]]. Settings › Agents and the footer say the saved agents
		// could not be read, rather than show an empty list.
		const auto unreadable = aProjection.SettingsUnreadable ? QString::fromStdString(aProjection.SettingsFile.empty() ? std::string{ "agent-setup.sqlite3" } : aProjection.SettingsFile) : QString{ };
		Window->agent_programs()->present_settings_unreadable(unreadable);
		if (auto* const footer = Window->agent_footer_widget(); footer != nullptr) {
			footer->present_settings_unreadable(unreadable);
		}
		// [[BPR-1753]], owner amendment 2026-10-23. The launches use
		// the list as published, and the picker offers it.
		AgentEntries->set(aProjection.Preferences.Agents);
		if (aProjection.Preferences.AgentsMoved) {
			// Canonical entries name their program. A legacy fallback must
			// neither replace one nor resurrect a wrapper after Reset.
			ClaudeWrapperSource->set({ });
		}
		Window->agent_panel()->set_run_limit_minutes(aProjection.Preferences.RunLimitMinutes);
		Window->agent_tab()->set_follow_up_steer(aProjection.Preferences.FollowUpSteer);
		Window->tour_editor()->side_chat()->set_follow_up_steer(aProjection.Preferences.FollowUpSteer);
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
	#if defined(BPR_BUILD_TESTING)
		// [[BPR-2177]], look ask-v002 (owner, 2026-11-14: "Aren't we supposed
		// to have a mack agent for testing now?"). A person's session on a
		// tests-on build opens with the fixture agent set up: its Check runs
		// once, by itself, as soon as a review gives it a workspace to make.
		// [[BPR-2153]]. It is added as a person's setup adds Claude Code: a
		// saved entry, preferred, so its Agent tab turns keep a native
		// session, as a person's do, and can be queued to and steered. The
		// stored list is read off the lane and replaces what is shown when it
		// lands, so the entry is added whenever the list shows none. The Check
		// below waits for it: adding a preferred entry starts the setup again.
		if (FixtureAgentSession && ClaudeAgentExecutable && aProjection.Preferences.Agents.empty() && !aProjection.SettingsUnreadable && !FixtureEntryAdding) {
			FixtureEntryAdding = true;
			QTimer::singleShot(0, Window.get(), [this] {
				FixtureEntryAdding = false;
				if (!Closing && AgentSetup && ClaudeAgentExecutable && AgentSetup->projection().Preferences.Agents.empty()) {
					auto entry = core::preset_agent_entry(core::setup_agent::claude_code, core::codex_protocol_choice::app_server, ClaudeAgentExecutable->string());
					entry.Name = "Fixture agent";
					AgentSetup->add_agent(std::move(entry), true);
				}
			});
		}
		if (FixtureAgentSession && !FixtureSetupChecked && !aProjection.Preferences.Agents.empty() && AgentReview && aProjection.Step == core::setup_step::check && !aProjection.Checking
			&& aProjection.State.AgentCheck.State == core::setup_check_state::not_run && aProjection.State.WorkspaceCheck.State == core::setup_check_state::not_run) {
			FixtureSetupChecked = true;
			QTimer::singleShot(0, Window.get(), [this] { check_agent_setup(true); });
		}
	#endif
	}

	auto github_application::implementation::grant_key(const std::uint32_t aAgent) const -> std::optional<application::agent_conversation_key>
	{
		if (!AgentReview) {
			return std::nullopt;
		}
		return application::agent_conversation_key{ .Host = AgentReview->Hostname, .Owner = AgentReview->Locator.Owner, .Name = AgentReview->Locator.Name, .Number = AgentReview->Number.value(), .Agent = aAgent };
	}

	auto github_application::implementation::agent_grant_for(const core::agent_entry& aEntry, const bool aOwnCheckout) const -> core::agent_grant
	{
		auto facts          = core::agent_grant_facts{ };
		facts.Program       = core::entry_settings_program(aEntry.Protocol);
		facts.ScratchStarts = aEntry.ScratchStarts;
		facts.Builds        = aEntry.BuildsAndTests;
		facts.OwnCheckout   = aOwnCheckout;
		// Review finding 4: what the launch can carry. A folder is made
		// only under a root a rule can name (never on Windows); a folder
		// full at its last turn's end is full until it is measured again.
		facts.ScratchPlace = !AgentScratchRoot.empty();
		facts.Refusal      = agent_grant_refusal_for(aEntry);
		// Re-review N3: by pull request and agent, so another pull request's
		// full folder is not this one's, and a restart reads it from disk.
		if (const auto key = grant_key(aEntry.Id); key) {
			if (const auto full = AgentScratchFull.find(*key); full != AgentScratchFull.end()) {
				facts.ScratchFull = full->second;
			}
			facts.ScratchUnmade = AgentScratchUnmade.contains(*key);
			if (AgentConversationStore) {
				facts.ScratchChoice = AgentConversationStore->load_scratch_choice(*key);
				facts.BuildAnswer   = AgentConversationStore->load_build_answer(*key);
			}
		}
		return core::resolve_agent_grant(facts);
	}

	void github_application::implementation::toggle_agent_scratch(const std::uint64_t aProfile, const bool aOn)
	{
		if (Closing || !AgentSetup || !AgentConversationStore) {
			return;
		}
		const auto  agents = AgentSetup->projection().Preferences.Agents;
		const auto* entry  = core::entry_of_profile(agents, core::agent_profile_id{ aProfile });
		const auto  key    = entry != nullptr ? grant_key(entry->Id) : std::nullopt;
		// RFC-BPR-0104 AC09. Kept for this pull request and agent; the agent's
		// starting state is not touched. The next turn takes it.
		if (key && !AgentConversationStore->store_scratch_choice(*key, aOn)) {
			Window->agent_panel()->refuse("The scratch folder choice could not be saved.");
		}
		present_run_settings();
	}

	void github_application::implementation::toggle_agent_builds(const std::uint64_t aProfile, const bool aOn)
	{
		if (Closing || !AgentSetup || !AgentConversationStore) {
			return;
		}
		const auto  agents = AgentSetup->projection().Preferences.Agents;
		const auto* entry  = core::entry_of_profile(agents, core::agent_profile_id{ aProfile });
		const auto  key    = entry != nullptr ? grant_key(entry->Id) : std::nullopt;
		// Owner look e97c6a3d: the toggle is this pull request's answer to
		// the build question (one answer per pull request, as the question
		// keeps it); its tooltip says what building does. The next turn
		// takes it.
		if (key && !AgentConversationStore->store_build_answer(*key, aOn)) {
			Window->agent_panel()->refuse("The build and test choice could not be saved.");
		}
		present_run_settings();
	}

	auto github_application::implementation::agent_grant_refusal_for(const core::agent_entry& aEntry) const -> std::optional<core::agent_grant_refusal>
	{
		if (core::entry_settings_program(aEntry.Protocol) != core::agent_program::codex) {
			return std::nullopt;
		}
		// Review finding 1 (2026-11-11). BPR's Codex profile replaces the
		// configured one and asks for no approval, so it is passed only where
		// the reviewer's own configuration (the user's and a managed one) has
		// no permissions profile and does not ask before commands.
		// Re-review N1b, N1d: the files the launched Codex reads. BPR passes
		// it its home and no `CODEX_HOME`, so its configuration is the home's
		// `.codex` (on Unix `QDir::homePath()` is that home, falling back as
		// Codex does), whatever `CODEX_HOME` BPR itself was started with; and
		// the system's and the managed file.
		const auto paths      = core::codex_configuration_paths(QDir::homePath().toStdString());
		auto       texts      = std::vector<std::string>{ };
		auto       unreadable = !paths.has_value();
		for (const auto& path : paths.value_or(std::vector<std::string>{ })) {
			auto file = QFile{ QString::fromStdString(path) };
			if (!file.exists()) {
				continue;
			}
			if (file.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
				unreadable = true;
				continue;
			}
			texts.push_back(file.readAll().toStdString());
		}
		auto views = std::vector<std::string_view>{ texts.begin(), texts.end() };
		return core::codex_grant_refusal(views, unreadable, aEntry.Arguments);
	}

	void github_application::implementation::derive_agent_scratch()
	{
		if (Closing || !AgentSetup || !AgentConversationStore || AgentScratchRoot.empty()) {
			return;
		}
		const auto setup = AgentSetup->projection();
		const auto limit = core::agent_scratch_limit_bytes(setup.Preferences.ScratchLimitMiB);
		for (const auto& entry : setup.Preferences.Agents) {
			const auto key = grant_key(entry.Id);
			if (!key) {
				continue;
			}
			const auto name = AgentConversationStore->load_scratch_folder(*key);
			if (!name || !application::valid_agent_scratch_key(*name)) {
				continue;
			}
			const auto folder = AgentScratchRoot / *name;
			auto       error  = std::error_code{ };
			const auto status = std::filesystem::symlink_status(folder, error);
			if (error || !std::filesystem::is_directory(status)) {
				AgentScratchFolders.erase(*key);
				AgentScratchFull.erase(*key);
				continue;
			}
			AgentScratchFolders[*key] = folder.string();
			AgentScratchFull[*key]    = application::agent_scratch_full(application::list_agent_scratch_folder(folder), limit);
		}
	}

	void github_application::implementation::present_agent_workspaces()
	{
		if (Closing || Window == nullptr || Window->agent_programs() == nullptr) {
			return;
		}
		const auto& summary = Profile->RetainedWorkspaces;
		if (!summary) {
			Profile->sweep_agent_workspaces(application::retained_workspace_sweep::survey);
			Window->agent_programs()->present_workspace_usage(std::nullopt, Profile->RetainedWorkspacesSaid);
			return;
		}
		auto usage  = qt::agent_programs_panel::workspace_usage{ };
		usage.Count = summary->Count;
		usage.Bytes = summary->Bytes;
		usage.InUse = summary->InUse;
		Window->agent_programs()->present_workspace_usage(usage, Profile->RetainedWorkspacesSaid);
	}

	void github_application::implementation::present_scratch_usage(const QString& aSaid)
	{
		if (Closing || Window == nullptr || Window->agent_programs() == nullptr) {
			return;
		}
		if (AgentScratchRoot.empty()) {
			Window->agent_programs()->present_scratch_usage(std::pair<std::uint64_t, std::size_t>{ 0U, 0U }, aSaid);
			return;
		}
		auto       bytes = std::uint64_t{ 0U };
		const auto all   = application::survey_agent_scratch_root(AgentScratchRoot);
		for (const auto& entry : all) {
			bytes += entry.Bytes;
		}
		Window->agent_programs()->present_scratch_usage(std::pair{ bytes, all.size() }, aSaid);
	}

	void github_application::implementation::clear_agent_scratch(const std::optional<std::int64_t> aBeforeSeconds, const bool aSay)
	{
		if (Closing || AgentScratchRoot.empty()) {
			return;
		}
		// Never under a running question: its folder could be any of them.
		if (AgentHost && !AgentHost->is_quiescent()) {
			if (aSay) {
				present_scratch_usage(QStringLiteral("Stop the agent's question first."));
			}
			return;
		}
		// Re-review N4: the open pull request's folders stay, by its
		// identity (read from the store when it opened, so after a restart
		// or a switch too); the others go.
		const auto isOpen = [this](const application::agent_conversation_key& aKey) {
			const auto here = grant_key(aKey.Agent);
			return here && *here == aKey;
		};
		auto keep = std::vector<std::string>{ };
		for (const auto& [key, folder] : AgentScratchFolders) {
			if (isOpen(key)) {
				keep.push_back(std::filesystem::path{ folder }.filename().string());
			}
		}
		const auto cleanup = application::remove_agent_scratch_folders(AgentScratchRoot, aBeforeSeconds, keep);
		std::erase_if(AgentScratchFolders, [&](const auto& aEntry) { return !isOpen(aEntry.first) && !std::filesystem::exists(std::filesystem::path{ aEntry.second }); });
		if (!aBeforeSeconds) {
			std::erase_if(AgentScratchFull, [&](const auto& aEntry) { return !isOpen(aEntry.first); });
		}
		if (aSay) {
			auto said = cleanup.Removed == 0U ? QStringLiteral("Nothing to clear.")
											  : QStringLiteral("Cleared %1 from %2 %3.").arg(QLocale{ }.formattedDataSize(static_cast<qint64>(cleanup.Bytes))).arg(cleanup.Removed).arg(cleanup.Removed == 1U ? QStringLiteral("folder") : QStringLiteral("folders"));
			if (cleanup.Failed != 0U) {
				said += QStringLiteral(" %1 could not be removed.").arg(cleanup.Failed);
			}
			present_scratch_usage(said);
			present_run_settings();
		}
	}

	void github_application::implementation::open_agent_scratch(const std::uint64_t aProfile)
	{
		const auto* const entry = AgentSetup ? core::entry_of_profile(AgentSetup->projection().Preferences.Agents, core::agent_profile_id{ aProfile }) : nullptr;
		const auto        key   = entry != nullptr ? grant_key(entry->Id) : std::nullopt;
		const auto        found = key ? AgentScratchFolders.find(*key) : AgentScratchFolders.end();
		if (Closing || found == AgentScratchFolders.end()) {
			return;
		}
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(found->second)));
	}

	void github_application::implementation::note_agent_scratch(const core::agent_profile_id aProfile, const application::agent_run_result& aResult)
	{
		if (Closing) {
			return;
		}
		// RFC-BPR-0104 as amended 2026-11-11: a turn that filled its folder
		// was stopped; the reviewer is told why, and the toggle says full.
		if (aResult.ScratchOverflowed && Window != nullptr) {
			const auto limit = AgentSetup ? AgentSetup->projection().Preferences.ScratchLimitMiB : core::default_agent_scratch_limit_mib;
			Window->agent_panel()->refuse("Stopped: the agent's scratch folder reached its limit of " + std::to_string(limit) + " MiB. Open it to make room, clear it in Settings, or raise the limit there.");
		}
		// Re-review N3: the result belongs to the pull request the turn was
		// granted in, which may no longer be the open one.
		const auto* const entry = AgentSetup ? core::entry_of_profile(AgentSetup->projection().Preferences.Agents, aProfile) : nullptr;
		const auto        key   = entry != nullptr && AgentScratchTurn && AgentScratchTurn->Agent == entry->Id ? AgentScratchTurn : std::nullopt;
		if (!key || (!aResult.Grant && !aResult.ScratchUnmade)) {
			present_run_settings();
			return;
		}
		if (aResult.ScratchUnmade) {
			AgentScratchUnmade.insert(*key);
		} else if (aResult.Grant && !aResult.Grant->ScratchFolder.empty()) {
			AgentScratchUnmade.erase(*key);
			AgentScratchFull[*key] = aResult.ScratchFull;
			present_scratch_usage();
		}
		if (!aResult.ScratchFolder.empty()) {
			AgentScratchFolders[*key] = aResult.ScratchFolder;
			if (AgentConversationStore) {
				static_cast<void>(AgentConversationStore->store_scratch_folder(*key, std::filesystem::path{ aResult.ScratchFolder }.filename().string()));
			}
		}
		present_run_settings();
	}

	auto github_application::implementation::grant_agent_turn(application::agent_invocation_request& aInvocation) -> bool
	{
		if (!AgentSetup || aInvocation.WritingOnly || aInvocation.ReadOnlyTools) {
			return true;
		}
		const auto  agents = AgentSetup->projection().Preferences.Agents;
		const auto* entry  = core::entry_of_profile(agents, aInvocation.Profile);
		if (entry == nullptr) {
			return true;
		}
		auto grant = agent_grant_for(*entry, aInvocation.ExistingCheckoutChosen);
		// RFC-BPR-0104 AC04. Asked once per pull request, before the first
		// turn that would build; declining leaves reading and scripting.
		if (core::build_question_due(grant)) {
			const auto asked  = grant_key(entry->Id);
			const auto answer = AskBuildQuestion ? AskBuildQuestion() : qt::ask_build_question(Window.get());
			// Review finding 7: the question is modal, and the window may
			// close or move to another pull request while it is open. The
			// turn goes only to the pull request it was asked in.
			if (!answer || Closing || !AgentSetup || grant_key(entry->Id) != asked) {
				return false;
			}
			if (asked && AgentConversationStore) {
				static_cast<void>(AgentConversationStore->store_build_answer(*asked, *answer));
			}
			grant.Builds = *answer ? core::agent_builds_state::allowed : core::agent_builds_state::declined;
			present_run_settings();
		}
		aInvocation.ScratchFolder   = core::scratch_granted(grant);
		aInvocation.BuildsAndTests  = core::builds_granted(grant);
		AgentScratchTurn            = grant_key(entry->Id);
		aInvocation.ScratchLimitMiB = AgentSetup->projection().Preferences.ScratchLimitMiB;
		return true;
	}

	// [[BPR-2198]]. What Identity and Repositories read: the profile's name,
	// the account signed in, the servers it remembers and its saved and
	// followed repositories. Names only: no credential is reachable from here.
	auto github_application::implementation::profile_settings_facts() const -> core::profile_facts
	{
		auto facts = core::profile_facts{ .ProfileName = Profile->Admission.Label };
		if (QueueHost == nullptr) {
			return facts;
		}
		const auto& session = QueueHost->session();
		if (!session.Host.empty() && !session.Account.empty()) {
			facts.SignedIn.push_back({ .Host = session.Host, .Login = session.Account });
		}
		for (const auto& server : QueueHost->saved_servers()) {
			facts.Remembered.push_back({ .Host = server.Hostname, .Login = server.Account });
		}
		for (const auto& repository : QueueHost->saved_repositories()) {
			facts.Repositories.push_back({ .Host = repository.Host, .Repository = repository.Repository, .Saved = true });
		}
		for (const auto& rule : QueueHost->rules()) {
			if (rule.Kind == core::observer_rule_kind::repository && !rule.Excluding) {
				facts.Repositories.push_back({ .Repository = rule.Pattern, .Followed = true });
			}
		}
		return facts;
	}

	// [[BPR-2016]], [[BPR-2017]], RFC-BPR-0107; [[BPR-2198]]. Export Profile
	// Settings…: the stored snapshot and the profile's names, the dialog's
	// choice, then the bytes written where the reviewer chose. Nothing else
	// is read or run.
	void github_application::implementation::export_agent_configuration()
	{
		if (Closing || !AgentConfiguration || Window == nullptr) {
			return;
		}
		AgentConfiguration->snapshot([this](std::optional<application::agent_configuration_snapshot> aSnapshot) {
			if (Closing || Window == nullptr) {
				return;
			}
			if (!aSnapshot) {
				Window->profiles_settings()->present_exchange(QObject::tr("The agent settings could not be read, so nothing was exported."));
				return;
			}
			auto unsaved = std::array<bool, core::writing_hook_count + 1U>{ };
			for (std::size_t index = 0U; index < core::writing_hook_count; ++index) {
				unsaved[index] = Window->writing_hooks()->has_unsaved(static_cast<core::writing_hook>(index));
			}
			// [[BPR-2283]]. The theme on screen is the Theme section.
			auto theme = std::optional<core::portable_theme>{ };
			if (AppearanceProfile) {
				const auto variant = AppearanceProfile->variant();
				theme              = core::portable_theme{ .Base = variant, .Palette = AppearanceProfile->palette_for(variant) };
			}
			// [[BPR-2301]]. The profile's Settings › Review is the Review section.
			auto review = std::optional<core::portable_review>{ };
			if (Profile->WorkbenchPreferences && Profile->WorkbenchPreferences->accepted()) {
				review = application::portable_review_of(Profile->WorkbenchPreferences->accepted()->Behaviour);
			}
			auto* const dialog = new qt::agent_configuration_export_dialog{ std::move(*aSnapshot), unsaved,
																			("BetterPullReview " + QCoreApplication::applicationVersion()).trimmed().toStdString(), profile_settings_facts(),
																			Window->profiles_settings()->window(), std::move(theme), std::move(review) };
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			// RFC-BPR-0125 AC24. The host mints every UUID an export may need:
			// a new pack's, and one for each own prompt made before UUIDs.
			auto fresh = std::vector<std::string>{ };
			for (const auto& own : dialog->snapshot().PromptMacros.Differences.Own) {
				if (!core::valid_prompt_uuid(own.Id)) {
					fresh.push_back(application::mint_prompt_uuid());
				}
			}
			dialog->set_minted_identities(application::mint_prompt_uuid(), std::move(fresh));
			dialog->set_file_chooser([dialog] {
	#if defined(BPR_BUILD_TESTING)
				// The attended look's own check names the file; a person is asked.
				if (const auto forced = qEnvironmentVariable("BPR_ATTENDED_CONFIGURATION_FILE"); !forced.isEmpty()) {
					return forced;
				}
	#endif
				const auto folder = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
				// [[BPR-2283]]. Every export is a .bprsettings file.
				auto path = QFileDialog::getSaveFileName(dialog, QObject::tr("Export Profile Settings"), folder + QStringLiteral("/profile.bprsettings"), QObject::tr("BetterPullReview settings (*.bprsettings)"));
				if (!path.isEmpty() && !path.endsWith(QStringLiteral(".bprsettings"), Qt::CaseInsensitive)) {
					path += QStringLiteral(".bprsettings");
				}
				return path;
			});
			QObject::connect(dialog, &QDialog::accepted, Window.get(), [this, dialog] {
				if (Closing || !AgentConfiguration) {
					return;
				}
				const auto path  = dialog->chosen_path();
				const auto write = [this, path, bytes = dialog->bytes()] {
					AgentConfiguration->write(path.toStdString(), bytes, [this, path](const bool aWritten) {
						if (!Closing && Window != nullptr) {
							Window->profiles_settings()->present_exchange(aWritten ? QObject::tr("Exported this profile's settings to %1.").arg(QFileInfo{ path }.fileName())
																				   : QObject::tr("%1 could not be written. Nothing was exported.").arg(QFileInfo{ path }.fileName()));
						}
					});
				};
				// RFC-BPR-0125 AC24. A prompt pack is recorded on the exported
				// prompts first, so every file written has its pack here.
				if (const auto& pack = dialog->prompt_pack()) {
					const auto& before = dialog->snapshot();
					AgentConfiguration->record_export(before, application::snapshot_after_export(before, *pack), [this, write](const application::agent_configuration_outcome aOutcome) {
						if (Closing || Window == nullptr) {
							return;
						}
						if (aOutcome != application::agent_configuration_outcome::applied) {
							Window->profiles_settings()->present_exchange(aOutcome == application::agent_configuration_outcome::changed_since
																			  ? QObject::tr("Your settings changed while Export was open, so nothing was exported. Export again.")
																			  : QObject::tr("The prompt pack could not be recorded, so nothing was exported."));
							return;
						}
						if (PromptMacros) {
							PromptMacros->load();
						}
						write();
					});
					return;
				}
				write();
			});
			dialog->open();
		});
	}

	// [[BPR-2282]]. What a drag onto the window carries, without reading a
	// file: exactly one local file is the window's; what it is, by its
	// content, the host says once it is dropped.
	auto github_application::implementation::dropped_file(const QMimeData& aMime) -> qt::dropped_file_admission
	{
		if (!aMime.hasUrls()) {
			return { };
		}
		const auto urls  = aMime.urls();
		const auto local = std::ranges::count_if(urls, [](const QUrl& aUrl) { return aUrl.isLocalFile(); });
		if (local == 0 || local != urls.size()) {
			return { };
		}
		if (local > 1) {
			return { .Kind = qt::dropped_file_admission::kind::several_files, .Path = { } };
		}
		return { .Kind = qt::dropped_file_admission::kind::one_file, .Path = urls.front().toLocalFile() };
	}

	// Import Profile Settings…: the file is read and decoded against what the
	// profile holds now; the dialog decides; one all-or-nothing application;
	// then the hosts read back what was stored. No agent is checked or run.
	// [[BPR-2282]]: `aDropped`, a file dropped onto the window, is read the
	// same way; its dialog opens over the window and its refusals are said in
	// the window's status bar, where the person dropping it is looking.
	void github_application::implementation::import_agent_configuration(const QString& aDropped)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		const auto dropped = !aDropped.isEmpty();
		const auto say     = [this, dropped](const QString& aText) {
			Window->profiles_settings()->present_exchange(aText);
			if (dropped) {
				Window->statusBar()->showMessage(aText, 8000);
			}
		};
		if (!AgentConfiguration) {
			if (dropped) {
				say(QObject::tr("Open a profile before importing its settings. Nothing was imported."));
			}
			return;
		}
		if (dropped && DroppedImportReading) {
			return;
		}
		auto path = aDropped;
	#if defined(BPR_BUILD_TESTING)
		if (path.isEmpty()) {
			path = qEnvironmentVariable("BPR_ATTENDED_CONFIGURATION_FILE");
		}
	#endif
		if (path.isEmpty()) {
			path = QFileDialog::getOpenFileName(Window->profiles_settings()->window(), QObject::tr("Import Profile Settings"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
												QObject::tr("BetterPullReview settings (*.bprsettings)"));
		}
		if (path.isEmpty()) {
			return;
		}
		DroppedImportReading = DroppedImportReading || dropped;
		AgentConfiguration->read(path.toStdString(), [this, path, dropped, say](std::expected<application::agent_configuration_preview, application::agent_configuration_read_error> aPreview) {
			if (dropped) {
				DroppedImportReading = false;
			}
			if (Closing || Window == nullptr) {
				return;
			}
			if (!aPreview) {
				say(aPreview.error().Refusal ? QString::fromUtf8(core::describe(*aPreview.error().Refusal).data()) + QLatin1Char(' ') + QObject::tr("Nothing was imported.")
											 : QObject::tr("%1 could not be read. Nothing was imported.").arg(QFileInfo{ path }.fileName()));
				return;
			}
			auto facts = qt::agent_configuration_import_dialog::facts{ .Skills = SkillLibrary.Catalog, .Found = { }, .Profile = Profile->Admission.Label };
			for (const auto& row : AgentSetup ? AgentSetup->projection().State.Rows : std::vector<core::agent_setup_row>{ }) {
				if (!row.Path.empty()) {
					facts.Found.push_back(row.Agent);
				}
			}
			aPreview->Here = profile_settings_facts();
			// [[BPR-2283]]. The file's Theme as the appearance host would keep
			// it, against the theme on screen.
			if (aPreview->File.Theme && aPreview->File.Theme->Theme && AppearanceHost && AppearanceProfile) {
				const auto variant = AppearanceProfile->variant();
				aPreview->Theme    = application::agent_configuration_preview::theme_against_here{
					.OnScreen = variant, .Here = AppearanceProfile->palette_for(variant), .Imported = AppearanceHost->resolve_theme(*aPreview->File.Theme->Theme)
				};
			}
			// [[BPR-2301]]. The file's Review against the profile's Settings › Review.
			if (aPreview->File.Review && Profile->WorkbenchPreferences && Profile->WorkbenchPreferences->accepted()) {
				const auto& here     = Profile->WorkbenchPreferences->accepted()->Behaviour;
				auto        imported = application::with_review(here, aPreview->File.Review->Review);
				auto        changes  = application::review_differences(here, imported);
				aPreview->Review     = application::agent_configuration_preview::review_against_here{ .Here = here, .Imported = std::move(imported), .Changes = std::move(changes) };
			}
			auto* const dialog = new qt::agent_configuration_import_dialog{ std::move(*aPreview), std::move(facts), dropped ? static_cast<QWidget*>(Window.get()) : Window->profiles_settings()->window() };
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->set_program_chooser([dialog](const QString& aHint) {
	#if defined(BPR_BUILD_TESTING)
				if (const auto forced = qEnvironmentVariable("BPR_ATTENDED_CONFIGURATION_PROGRAM"); !forced.isEmpty()) {
					return forced;
				}
	#endif
				return QFileDialog::getOpenFileName(dialog, QObject::tr("Choose the program for %1").arg(aHint.isEmpty() ? QObject::tr("this agent") : aHint));
			});
			QObject::connect(dialog, &QDialog::accepted, Window.get(), [this, dialog, dropped] { apply_agent_configuration(*dialog, dropped); });
			dialog->open();
		});
	}

	void github_application::implementation::apply_profile_changes(const core::profile_settings_changes& aChanges)
	{
		if (QueueHost != nullptr) {
			for (const auto& server : aChanges.RememberServers) {
				QueueHost->apply(application::saved_server_intent{ .Action = application::saved_server_action::add, .Server = { .Hostname = server.Host, .Account = server.Login } });
			}
			for (const auto& saved : aChanges.Save) {
				QueueHost->apply(application::saved_repository_intent{ .Action = application::saved_repository_action::add, .Repository = { .Host = saved.Account.Host, .Account = saved.Account.Login, .Repository = saved.Repository } });
			}
			for (const auto& followed : aChanges.Follow) {
				QueueHost->apply(application::queue_rule_intent{ .Action = application::queue_rule_action::add, .Rule = { .Kind = core::observer_rule_kind::repository, .Pattern = followed } });
			}
		}
		if (aChanges.Rename && Window != nullptr) {
			Window->dispatch_profile_management(application::rename_bpr_profile_intent{ Profile->Admission.Authority.profile(), *aChanges.Rename });
		}
	}

	void github_application::implementation::apply_agent_configuration(const qt::agent_configuration_import_dialog& aDialog, const bool aDropped)
	{
		if (Closing || !AgentConfiguration || Window == nullptr) {
			return;
		}
		const auto say = [this, aDropped](const QString& aText) {
			Window->profiles_settings()->present_exchange(aText);
			if (aDropped) {
				Window->statusBar()->showMessage(aText, 8000);
			}
		};
		if (Profile->WorkbenchPreferences->is_busy()) {
			say(QObject::tr("Settings were being saved. Nothing was imported; import the file again."));
			return;
		}
		// [[BPR-2198]]. Identity and Repositories go through the profile's own
		// durable paths once the rest is stored: the core says what changes.
		auto profileChanges = core::profile_changes(aDialog.preview().File, aDialog.plan(), aDialog.identity_choice(), aDialog.repository_choices(), profile_settings_facts());
		// [[BPR-2283]]. The Theme, when chosen, is kept on the theme it was
		// planned against, through the appearance host's own commit, once
		// the rest of the file is stored.
		auto       chosenTheme  = aDialog.theme_choice();
		const auto themeVariant = aDialog.preview().Theme ? aDialog.preview().Theme->OnScreen : core::appearance_variant::light;
		// [[BPR-2301]]. Review, when chosen, through the workbench preferences'
		// own write, once the rest is stored.
		auto chosenReview = aDialog.review_choice();
		AgentConfiguration->apply(aDialog.preview(), aDialog.agent_choices(), aDialog.prompt_choices(), [this, say, agents = aDialog.agent_choices().size(), changes = std::move(profileChanges), theme = std::move(chosenTheme), themeVariant, review = std::move(chosenReview)](application::agent_configuration_host::applied aApplied) {
			if (Closing || Window == nullptr) {
				return;
			}
			if (aApplied && aApplied->first == application::agent_configuration_outcome::applied) {
				apply_profile_changes(changes);
				if (theme && AppearanceHost) {
					AppearanceHost->apply(*theme, themeVariant);
				}
			}
			if (!aApplied) {
				const auto why = aApplied.error().PromptPack ? core::stable_name(*aApplied.error().PromptPack) : core::stable_name(aApplied.error().Refusal);
				say(QObject::tr("The import was refused (%1). Nothing changed.").arg(QString::fromUtf8(why.data(), static_cast<qsizetype>(why.size()))));
				return;
			}
			switch (aApplied->first) {
			case application::agent_configuration_outcome::applied:
				// The stores hold the result; the hosts read it back.
				static_cast<void>(Profile->WorkbenchPreferences->adopt_writing_hooks(aApplied->second.Hooks));
				if (review) {
					static_cast<void>(Profile->WorkbenchPreferences->apply_behaviour(*review));
				}
				if (AgentSetup) {
					AgentSetup->load();
				}
				if (ReviewSkill) {
					ReviewSkill->load();
				}
				// RFC-BPR-0112. A draft's Workflow list follows the profile's.
				if (DraftDesktop) {
					DraftDesktop->reload_workflows();
				}
				// RFC-BPR-0125 AC24. The chips, menus and Settings › Prompts
				// follow the merged prompt pack.
				if (PromptMacros) {
					PromptMacros->load();
				}
				say(agents > 0U ? QObject::tr("Imported into this profile. Check a new agent before you ask it anything.") : QObject::tr("Imported into this profile."));
				break;
			case application::agent_configuration_outcome::changed_since:
				say(QObject::tr("Your agent or prompt settings changed after the file was read, so nothing was imported. Import it again to see the differences."));
				break;
			case application::agent_configuration_outcome::storage_failed:
				say(QObject::tr("The import could not be saved. Nothing changed."));
				break;
			} }, aDialog.workflow_choices(), aDialog.prompt_pack_choices());
	}
#endif
}
