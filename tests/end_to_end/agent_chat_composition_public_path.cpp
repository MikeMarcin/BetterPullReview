/// \file
/// \brief RFC-BPR-0012's side-chat session, driven through the shipped window.
///
/// \details Modelled on `agent_finding_composition_public_path.cpp` and
/// deliberately the same shape: one real fixture-served review, real gestures,
/// and a construction cut that leaves the gesture and removes its effect.
///
/// What these courses are for, in one sentence each. **positive**: one send
/// typed into the shipped side-chat panel reaches exactly one agent turn on
/// the launch path that already existed, and exactly one delivered reply
/// arrives back through the finding-delivery bounds, with zero GitHub mutation
/// calls anywhere, and the message the reviewer typed appears exactly once in
/// the frame the agent was handed - counted by the agent itself and echoed
/// back. **stale**: after the head moves, a send against the session the chat
/// is still holding is refused by the core's own verdict and the refusal is
/// named on screen; choosing `start_new_session` then starts exactly one
/// session and starts no process. **continued**: from the same stale state,
/// choosing `continue_against_old_revision` starts no session at all, and the
/// next send produces one turn under the session the chat kept - still bound
/// to the revision it was asked about, which the revision under review has
/// moved away from, and composed from that revision: the binding the MCP
/// helper is pinned to, the binding record the frozen snapshot carries on
/// disk, and the kept session's own binding are one value, and it is not the
/// head the reviewer moved to. **anchored-continuation**: the same turn, sent
/// only once the accepted comparison has moved to the new pair, so the
/// re-alignment the continued turn asks for is a real retarget - and what it
/// freezes is byte-for-byte the alignment the shipped diff path independently
/// computed for that same revision before the review left it.
/// **construction-cut**: with the chat host's construction
/// removed, the same send gesture is still made and still observed, its turn
/// count is zero, and the positive marker is false.

#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "right_side_change.hpp"

#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTabBar>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	using e2e::default_agent_budget;
	using e2e::extended_agent_budget;
	using e2e::wait_terms;

	/// Exactly what the reviewer types into the composed panel's composer.
	constexpr std::string_view chat_message
		= "BPR_CHAT_SEND_0514 what does this retry budget do?";

	/// The second question, asked of the session the reviewer chose to keep.
	/// Distinct bytes from the first, so a turn that re-sent the earlier
	/// message would not satisfy the echo below.
	constexpr std::string_view continued_message
		= "BPR_CHAT_SEND_0517 which retry did the old revision exhaust?";

	/// \brief What the fixture agent must report having received, and once.
	///
	/// \details The count is the agent's own, taken over the whole frame it was
	/// handed rather than over the fields it parsed, so a message placed both
	/// in the supplement and beside it in the framing reads as two.
	[[nodiscard]] auto exactly_once_echo(const std::string_view aMessage)
		-> std::string
	{
		return "BPR_CHAT_ECHO_0517 count=1 text=" + std::string{ aMessage };
	}

	template <typename Predicate>
	[[nodiscard]] auto wait_until_reporting(
		wait_terms&                     aTerms,
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget = default_agent_budget) -> bool
	{
		return e2e::wait_until(aTerms, aPredicate, aBudget);
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		auto terms = wait_terms{ };
		return wait_until_reporting(
			terms,
			[&aApplication](wait_terms& aTerms) {
				QApplication::processEvents();
				return aTerms.require(
					"shutdown-complete", aApplication.status().ShutdownComplete);
			},
			extended_agent_budget);
	}

	[[nodiscard]] auto fail(
		app::github_application& aApplication,
		const std::string_view   aReason,
		const std::string_view   aState = { }) -> int
	{
		std::cerr << "agent-chat-composition-failure:" << aReason;
		if (!aState.empty()) {
			std::cerr << ' ' << aState;
		}
		std::cerr << '\n';
		static_cast<void>(shutdown(aApplication));
		return EXIT_FAILURE;
	}

	/// \brief Details the delivered finding so its claim is readable.
	///
	/// \details A Qt-local navigation on the composed queue, exactly what
	/// `agent_finding_composition_public_path` does and for the same reason:
	/// the queue shows a row's detail when a row is selected, and nothing is
	/// sent, published, or promoted by looking at one.
	void detail_queued_finding(app::github_application& aApplication)
	{
		auto* const queue
			= app::detail::github_application_fixture_access::finding_queue(
				aApplication);
		if (queue != nullptr && queue->finding_count() != 0U) {
			queue->select(0U);
			QApplication::processEvents();
		}
	}

	/// \brief Reads the binding record the frozen snapshot carries on disk.
	///
	/// \details The first line of the snapshot root's `binding` file - exactly
	/// what `adapters::mcp::mcp_context_helper` reads before it serves anything
	/// and compares its own pin against. Read from disk rather than asked of the
	/// composition, because the question is what the helper would find there.
	[[nodiscard]] auto snapshot_binding_record(
		const std::filesystem::path& aSnapshotRoot) -> std::string
	{
		auto file = std::ifstream{ aSnapshotRoot / "binding", std::ios::binary };
		auto line = std::string{ };
		std::getline(file, line);
		return line;
	}

	/// \brief The per-path identities the frozen snapshot carries on disk.
	///
	/// \details The `alignment` file beside the binding record, in the
	/// `name<TAB>value` shape `freeze_mcp_snapshot` wrote and the MCP helper
	/// serves. Read from disk for the same reason the binding record is: the
	/// question is what a reader of the snapshot would find there, not what
	/// the composition believes it wrote.
	struct frozen_alignment_record
	{
		std::size_t              Files = 0U;
		std::string              Path;
		std::string              Comparison;
		std::string              LeftLineCount;
		std::string              RightLineCount;
		std::vector<std::string> Segments;
		std::vector<std::string> Groups;
		std::vector<std::string> Moves;
	};

	[[nodiscard]] auto snapshot_alignment_record(
		const std::filesystem::path& aSnapshotRoot) -> frozen_alignment_record
	{
		auto record = frozen_alignment_record{ };
		auto file   = std::ifstream{ aSnapshotRoot / "alignment", std::ios::binary };
		auto line   = std::string{ };
		while (std::getline(file, line)) {
			const auto tab = line.find('\t');
			if (tab == std::string::npos) {
				continue;
			}
			const auto name  = line.substr(0U, tab);
			auto       value = line.substr(tab + 1U);
			if (name == "file") {
				++record.Files;
				record.Path = std::move(value);
			} else if (name == "comparison") {
				record.Comparison = std::move(value);
			} else if (name == "left_line_count") {
				record.LeftLineCount = std::move(value);
			} else if (name == "right_line_count") {
				record.RightLineCount = std::move(value);
			} else if (name == "segment") {
				record.Segments.push_back(std::move(value));
			} else if (name == "group") {
				record.Groups.push_back(std::move(value));
			} else if (name == "move") {
				record.Moves.push_back(std::move(value));
			}
		}
		return record;
	}

	/// \brief Counts one exact token in the composition source.
	[[nodiscard]] auto occurrences(
		const std::string& aSource, const std::string_view aToken) -> std::size_t
	{
		auto count = std::size_t{ 0U };
		auto at    = aSource.find(aToken);
		while (at != std::string::npos) {
			++count;
			at = aSource.find(aToken, at + aToken.size());
		}
		return count;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3) {
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (
		course != "positive" && course != "stale" && course != "continued"
		&& course != "anchored-continuation" && course != "construction-cut") {
		return EXIT_FAILURE;
	}
	// The two courses that take `continue_against_old_revision` to a further
	// turn. They differ in when that turn is sent, and in nothing else.
	const auto continuing
		= course == "continued" || course == "anchored-continuation";
	const auto anchored = course == "anchored-continuation";
	auto       data     = QTemporaryDir{ };
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	// [[BPR-0812]], [[BPR-1050]]. The stores refuse a root that is not
	// owner-private, and the agent workspace neighbourhood beside the root
	// refuses a parent another principal can write.
	const auto privateRoot = e2e::private_data_root(data);
	if (!privateRoot) {
		std::cerr << "agent-chat-composition-failure:private-test-root\n";
		return EXIT_FAILURE;
	}
	const auto& dataRoot = *privateRoot;

	std::unique_ptr<app::github_application> application;
	auto                                     startupTerminal = false;
	auto                                     startup         = app::detail::github_application_fixture_access::start(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot = dataRoot,
			// The continued course needs the head to move *and* the new
			// revision to be preparable, because it goes on to ask a further
			// question; the refresh recording is the only fixture path that
			// serves a second head's material.
			.EpochPhase                        = continuing
				? app::detail::github_epoch_fixture_phase::seed
				: app::detail::github_epoch_fixture_phase::single,
			.DisableChatSessionHostComposition = course == "construction-cut",
			.CodexAgentExecutable              = std::filesystem::path{ argv[2] },
		},
		[&](auto aResult) {
			startupTerminal = true;
			if (aResult) {
				application = std::move(*aResult);
			}
		});
	auto startupTerms = wait_terms{ };
	if (!wait_until_reporting(startupTerms, [&](wait_terms& aTerms) {
			return aTerms.require("startup-terminal", startupTerminal && startup && startup->is_quiescent());
		})
		|| !application) {
		std::cerr << "application-startup-failed:" << startupTerms.render() << '\n';
		return EXIT_FAILURE;
	}
	application->show();
	auto& window = application->window();
	window.github_connect_action()->trigger();
	auto connected = wait_terms{ };
	if (!wait_until_reporting(connected, [&window](wait_terms& aTerms) {
			aTerms.note_identifier("status", window.github_status_text());
			return aTerms.require(
					   "status-is-connected",
					   window.github_status_text() == "GitHub account connected.")
				&& aTerms.require(
					"open-enabled", window.github_open_action()->isEnabled());
		})) {
		return fail(*application, "connect", connected.render());
	}
	// [[BPR-0771]]. A pull request is named in the Home bar; the locator is retired.
	window.home_open_bar()->setText(QStringLiteral("octo-org/review-repository#17"));
	window.home_open_action()->trigger();

	auto* const panel     = window.agent_panel();
	auto        offerings = wait_terms{ };
	if (!wait_until_reporting(offerings, [&window, panel](wait_terms& aTerms) {
			return aTerms.require(
					   "files-listed",
					   window.files_drawer()->contents_view()->model()->rowCount()
						   != 0)
				&& aTerms.require(
					"profiles-is-four", panel->profile_selector()->count() == 4);
		})) {
		return fail(*application, "agent-offerings", offerings.render());
	}

	auto* const files = window.files_drawer()->contents_view();
	files->selectionModel()->setCurrentIndex(
		files->model()->index(0, 0),
		QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto selectedDiff = wait_terms{ };
	if (!wait_until_reporting(
			selectedDiff,
			[&window](wait_terms& aTerms) {
				return aTerms.require(
					"comparison-shown",
					window.comparison_surface()->current_result() != nullptr);
			},
			extended_agent_budget)) {
		return fail(*application, "selected-diff", selectedDiff.render());
	}
	// A real semantic selection, because `ask_about_selection` is one of the
	// three entry points the core refuses without one. Navigating produces it;
	// nothing about navigating sends anything.
	auto* const surface = window.comparison_surface();
	if (!surface->navigate_change(
			core::change_navigation_direction::next,
			core::change_navigation_policy::stop)) {
		return fail(*application, "semantic-selection-action");
	}
	auto semantic = wait_terms{ };
	if (!wait_until_reporting(semantic, [surface](wait_terms& aTerms) {
			return aTerms.require(
				"selection-present", surface->selection().has_value());
		})) {
		return fail(*application, "semantic-selection", semantic.render());
	}
	// [[BPR-1245]]. The course promotes the reply into Comment's draft, which
	// needs a right-side line ([[BPR-0587]]); the first change is a deletion.
	if (!e2e::select_right_side_of_change(*surface)) {
		return fail(*application, "right-side-selection");
	}

	auto* const chat
		= app::detail::github_application_fixture_access::side_chat_panel(
			*application);
	if (chat == nullptr) {
		return fail(*application, "side-chat-panel");
	}
	auto presented = wait_terms{ };
	if (!wait_until_reporting(presented, [chat](wait_terms& aTerms) {
			return aTerms.require(
					   "chat-presented", chat->statistics().Presentations != 0U)
				&& aTerms.require(
					"send-enabled", chat->send_action()->isEnabled());
		})) {
		return fail(*application, "chat-presented", presented.render());
	}
	const auto beforeSend
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);

	// The gesture. Typed into the shipped composer and sent with the shipped
	// action, on the panel the window itself placed.
	chat->composer()->setPlainText(QString::fromUtf8(
		chat_message.data(), static_cast<qsizetype>(chat_message.size())));
	QApplication::processEvents();
	chat->send_action()->trigger();
	QApplication::processEvents();

	const auto sendIntents = chat->statistics().SendIntents;

	if (course == "construction-cut") {
		const auto evidence
			= app::detail::github_application_fixture_access::agent_chat_evidence(
				*application);
		const auto invocation = app::detail::
			github_application_fixture_access::agent_invocation_evidence(
				*application);
		const auto mutation = app::detail::
			github_application_fixture_access::review_mutation_evidence(
				*application);
		// The gesture is still made and still observed; its effect is gone.
		const auto gestureObserved = sendIntents == 1U
			&& evidence.Intents == 1U && evidence.SideChatPanelComposed
			&& evidence.Presentations != 0U;
		const auto effectsZero = !evidence.ChatHostComposed
			&& evidence.Turns == 0U
			&& evidence.Statistics == application::agent_chat_session_statistics{ }
			&& !evidence.Session.has_value() && !evidence.Observed.has_value()
			&& invocation.Statistics.ProcessStarts == 0U
			&& invocation.AgentProcessLauncherInvocations == 0U;
		// The reviewer is told, rather than watching a chat do nothing.
		const auto refused = evidence.AgentPanelStatusText.find(
								 "side chat refused: no composed chat session host")
			!= std::string::npos;
		const auto positiveMarker = evidence.ChatHostComposed
			&& evidence.Turns == 1U && evidence.Statistics.Turns == 1U
			&& evidence.Statistics.Replies == 1U;
		const auto noGithubMutation = mutation.Submissions == 0U
			&& mutation.TargetObservations == 0U && mutation.StoredComments == 0U;
		const auto passed = gestureObserved && effectsZero && refused
			&& !positiveMarker && noGithubMutation;
		if (!passed) {
			std::cerr << "agent-chat-cut-evidence:" << gestureObserved << ':'
					  << effectsZero << ':' << refused << ':' << positiveMarker
					  << ':' << noGithubMutation << ':' << evidence.Intents << ':'
					  << evidence.Turns << ':'
					  << invocation.Statistics.ProcessStarts
					  << "\npanel-status:" << evidence.AgentPanelStatusText << '\n';
		}
		if (!shutdown(*application) || !passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-chat-construction-cut:pass\n";
		return EXIT_SUCCESS;
	}

	// One turn, on the launch path that already existed, answered once.
	auto answered = wait_terms{ };
	if (!wait_until_reporting(
			answered,
			[&application](wait_terms& aTerms) {
				const auto invocation = app::detail::
					github_application_fixture_access::agent_invocation_evidence(
						*application);
				const auto chatEvidence = app::detail::
					github_application_fixture_access::agent_chat_evidence(
						*application);
				aTerms.note(
					"process",
					static_cast<std::uint64_t>(
						invocation.Statistics.ProcessStarts));
				aTerms.note(
					"replies",
					static_cast<std::uint64_t>(chatEvidence.Statistics.Replies));
				aTerms.note_identifier(
					"panel", chatEvidence.AgentPanelStatusText);
				return aTerms.require(
						   "run-quiescent",
						   invocation.Quiescent
							   && invocation.Statistics.ProcessStarts == 1U)
					&& aTerms.require(
						"reply-delivered",
						chatEvidence.Statistics.Replies == 1U);
			},
			extended_agent_budget)) {
		return fail(*application, "chat-turn", answered.render());
	}
	QApplication::processEvents();
	detail_queued_finding(*application);

	const auto afterSend
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);
	const auto invocation
		= app::detail::github_application_fixture_access::agent_invocation_evidence(
			*application);
	const auto finding
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);
	const auto mutation
		= app::detail::github_application_fixture_access::review_mutation_evidence(
			*application);
	const auto noGithubMutation = mutation.Submissions == 0U
		&& mutation.TargetObservations == 0U && mutation.StoredComments == 0U;

	// The composition source itself: exactly one launch call site, reached from
	// both the launch panel and the chat, so a chat turn cannot have grown its
	// own workspace or its own frozen snapshot.
	auto composition = std::ifstream{
		std::filesystem::path{ BPR_COMPOSITION_SOURCE },
		std::ios::binary,
	};
	const auto source = std::string{
		std::istreambuf_iterator<char>{ composition },
		std::istreambuf_iterator<char>{ },
	};
	const auto oneLaunchPath = !source.empty()
		&& occurrences(source, "AgentHost->dispatch(") == 1U
		&& occurrences(source, "application::agent_chat_session_host>(") == 1U
		// One declaration and exactly six call sites: the launch panel's
		// start gesture, the chat host's turn dispatcher, BPR-0533's
		// comment-workshop press, RFC-BPR-0023's private run from selected
		// sources (f55e39e72), and the tour writing hook and its saved-hook
		// test (9b71342a0, 1d84e59c2; writing-only, no private context).
		// [[BPR-1245]] reviewed the last three. Six gestures, one launch path —
		// which is what `AgentHost->dispatch(` being pinned at one above
		// already says, and what this count guards from the other side: a
		// gesture that grew its own launch would not appear here at all.
		&& occurrences(source, "dispatch_agent_invocation(") == 7U
		&& source.find("ChatHost->deliver(") != std::string::npos;

	const auto positive = afterSend.ChatHostComposed
		&& sendIntents == 1U && afterSend.Intents == 1U && afterSend.Turns == 1U
		&& afterSend.Statistics.Intents == 1U && afterSend.Statistics.Turns == 1U
		&& afterSend.Statistics.Replies == 1U
		&& afterSend.Statistics.Refusals == 0U
		&& afterSend.Statistics.PresentedOutcomes
			== afterSend.Statistics.Intents
		&& invocation.Statistics.ProcessStarts == 1U
		&& invocation.Statistics.WorkspaceCleanups == 1U
		// The reply arrived through the finding-delivery bounds and nowhere
		// else: the chat's reply count and the finding host's delivery count
		// are the same single validated value.
		&& finding.FindingStatistics.Deliveries == 1U
		&& finding.FindingStatistics.DeliveredFindings == 1U
		&& finding.QueuedFindings == 1U
		// The chat adopted the session the launch path admitted, so the reply
		// is keyed to the session the chat is holding.
		&& afterSend.Session.has_value()
		&& beforeSend.Statistics.SessionsStarted == 1U
		&& afterSend.Statistics.SessionsStarted == 1U
		// The typed message reached the agent, in the frame, exactly once. The
		// fixture counted its own input and said so; the host's `chat_message`
		// field is the only thing that could have put it there.
		&& finding.QueuedClaim.find(exactly_once_echo(chat_message))
			!= std::string::npos
		&& afterSend.LastTurn.has_value()
		&& afterSend.LastTurn->Message == chat_message
		&& noGithubMutation && oneLaunchPath;

	if (course == "positive") {
		if (chat->replies_list()->count() != 1) {
			return fail(*application, "chat-reply-visible");
		}
		chat->replies_list()->setCurrentRow(0);
		const auto replyId = chat->replies_list()->item(0)->data(Qt::UserRole);
		if (!chat->promote_action()->isEnabled()) {
			return fail(*application, "chat-reply-promotion-target");
		}
		chat->promote_action()->trigger();
		const auto* editor = window.review_editor();
		// [[BPR-1245]]. The promoted draft is accepted on the private review's
		// answer, so the course waits for it to be shown.
		auto promoted = wait_terms{ };
		if (!wait_until_reporting(promoted, [editor, &window, &finding](wait_terms& aTerms) {
				return aTerms.require("draft-is-reply", editor->shown_draft().has_value() && editor->shown_draft()->Body == finding.QueuedClaim)
					&& aTerms.require("comments-shown", editor->body_editor()->isVisible() && window.tray_tabs()->currentIndex() == 0);
			})) {
			return fail(*application, "chat-reply-promotion", promoted.render());
		}
		const auto afterPromotion = app::detail::github_application_fixture_access::review_mutation_evidence(*application);
		if (chat->replies_list()->count() != 1 || chat->replies_list()->item(0)->data(Qt::UserRole) != replyId
			|| afterPromotion.Submissions != 0U || afterPromotion.StoredComments != 0U || afterPromotion.Replies != 0U) {
			return fail(*application, "chat-reply-promotion-preserves-source-and-privacy");
		}
		if (!positive) {
			std::cerr << "agent-chat-positive-evidence:" << afterSend.Turns << ':'
					  << afterSend.Statistics.Turns << ':'
					  << afterSend.Statistics.Replies << ':'
					  << afterSend.Statistics.Refusals << ':'
					  << afterSend.Statistics.SessionsAdopted << ':'
					  << afterSend.Statistics.SessionsStarted << ':'
					  << invocation.Statistics.ProcessStarts << ':'
					  << finding.FindingStatistics.Deliveries << ':'
					  << oneLaunchPath << ':' << noGithubMutation
					  << "\nqueued-claim:" << finding.QueuedClaim
					  << "\npanel-status:" << afterSend.AgentPanelStatusText
					  << '\n';
		}
		if (!shutdown(*application) || !positive) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-chat-composition:pass\n";
		return EXIT_SUCCESS;
	}

	// The stale half. The head moves under a session that was asked about the
	// old one, and the session is never retargeted to follow it. The `stale`
	// course moves it by replaying an accepted handoff, which is the cheapest
	// way to move a head and leaves the new revision unprepared; `continued`
	// moves it with the shipped refresh gesture against the seeded recording,
	// so the revision the reviewer is now looking at is a real one.
	// The old pair's own identities, as the shipped alignment path produced
	// them for the turn that just ran against it. Read off disk before the
	// head moves, because after the move nothing on screen belongs to this
	// revision any more. The anchored course compares what a re-alignment
	// recomputes against exactly these bytes.
	const auto keptFrozen = snapshot_alignment_record(std::filesystem::path{
		finding.McpLaunch.has_value() ? finding.McpLaunch->WorkingDirectory
									  : std::string{ } });
	if (continuing) {
		window.github_refresh_action()->trigger();
	} else if (!app::detail::github_application_fixture_access::
				   reaccept_fixture_revision(*application, true)) {
		return fail(*application, "stale-revision");
	}
	auto staleShown = wait_terms{ };
	if (!wait_until_reporting(
			staleShown,
			[&application, chat](wait_terms& aTerms) {
				QApplication::processEvents();
				const auto evidence = app::detail::
					github_application_fixture_access::agent_chat_evidence(
						*application);
				aTerms.note_identifier("state", evidence.SessionStateText);
				return aTerms.require(
						   "session-stale",
						   evidence.Projection.has_value()
							   && evidence.Projection->Continuations.Required
							   && evidence.Projection->Continuations.Status
									   .Staleness
								   == core::agent_session_staleness::head_moved)
					// The panel's own guard: a session that stopped matching is
					// answered before it is talked to again.
					&& aTerms.require(
						"send-disabled", !chat->send_action()->isEnabled())
					&& aTerms.require(
						"continuations-offered",
						chat->offered_continuations().size() == 2U);
			},
			extended_agent_budget)) {
		return fail(*application, "stale-projection", staleShown.render());
	}
	const auto beforeStaleSend
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);
	// The send the panel will not let a reviewer make, put to the host that
	// must refuse it anyway. Delivered on the panel's own composed dispatch
	// edge, carrying exactly the value the panel would have emitted.
	if (!app::detail::github_application_fixture_access::dispatch_side_chat_send(
			*application, std::string{ chat_message })) {
		return fail(*application, "stale-send");
	}
	QApplication::processEvents();
	const auto afterStaleSend
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);
	const auto staleRefused
		= afterStaleSend.Statistics.Turns == beforeStaleSend.Statistics.Turns
		&& afterStaleSend.Statistics.Refusals
			== beforeStaleSend.Statistics.Refusals + 1U
		&& afterStaleSend.RefusalsPresented
			== beforeStaleSend.RefusalsPresented + 1U
		&& afterStaleSend.AgentPanelStatusText.find(
			   "side chat refused: session_stale (head_moved)")
			!= std::string::npos;

	// The other continuation, driven to a turn. Choosing to keep talking to a
	// session about the revision it was asked about is the half BPR-0514
	// implemented and never drove, so this is what it does when it is used.
	if (continuing) {
		const auto beforeContinue = app::detail::
			github_application_fixture_access::agent_chat_evidence(*application);
		const auto beforeContinueInvocation = app::detail::
			github_application_fixture_access::agent_invocation_evidence(
				*application);
		if (
			chat->continue_old_revision_action() == nullptr
			|| !chat->continue_old_revision_action()->isEnabled()) {
			return fail(*application, "continue-old-revision-action");
		}
		chat->continue_old_revision_action()->trigger();
		QApplication::processEvents();
		const auto afterContinue = app::detail::
			github_application_fixture_access::agent_chat_evidence(*application);
		const auto afterContinueInvocation = app::detail::
			github_application_fixture_access::agent_invocation_evidence(
				*application);
		const auto keptSession = afterContinue.Statistics.Continuations
				== beforeContinue.Statistics.Continuations + 1U
			// Nothing was started and nothing was adopted: this continuation is
			// the absence of a new session, which is what distinguishes it from
			// the other one.
			&& afterContinue.Statistics.SessionsStarted
				== beforeContinue.Statistics.SessionsStarted
			&& afterContinue.Statistics.SessionsAdopted
				== beforeContinue.Statistics.SessionsAdopted
			&& afterContinue.Session.has_value()
			&& beforeContinue.Session.has_value()
			&& afterContinue.Session->Id == beforeContinue.Session->Id
			&& afterContinue.Session->Binding == beforeContinue.Session->Binding
			// Still bound to the revision it was asked about, which is not the
			// one under review.
			&& afterContinue.Observed.has_value()
			&& afterContinue.Session->Binding != *afterContinue.Observed
			// Only "you have to answer" was answered. The staleness verdict is
			// the core's and is untouched.
			&& afterContinue.Projection.has_value()
			&& !afterContinue.Projection->Continuations.Required
			&& afterContinue.Projection->Continuations.Status.Staleness
				== core::agent_session_staleness::head_moved
			&& afterContinueInvocation.Statistics.ProcessStarts
				== beforeContinueInvocation.Statistics.ProcessStarts
			&& chat->send_action()->isEnabled();
		const auto keptSessionId = afterContinue.Session->Id;
		const auto keptBinding   = afterContinue.Session->Binding;

		// The anchored course sends only once the accepted comparison is the
		// new pair. Until then the pair the kept session names is the pair on
		// screen — only the head has moved — so a re-alignment would ask for
		// what is already resolved, and a course that sent here would prove
		// nothing about re-alignment. The acceptance arrives on its own,
		// behind the refresh's own epoch work.
		if (anchored) {
			auto movedComparison = wait_terms{ };
			if (!wait_until_reporting(
					movedComparison,
					[&application, &keptBinding](wait_terms& aTerms) {
						QApplication::processEvents();
						const auto now = app::detail::
							github_application_fixture_access::agent_chat_evidence(
								*application);
						return aTerms.require(
							"comparison-moved",
							now.Observed.has_value()
								&& (now.Observed->LeftEpoch != keptBinding.LeftEpoch
									|| now.Observed->RightEpoch
										!= keptBinding.RightEpoch));
					},
					extended_agent_budget)) {
				return fail(
					*application, "anchored-comparison", movedComparison.render());
			}
			// A comparison the reviewer has not answered for disables Send
			// again, and the answer is the same one they already gave: keep
			// talking to the session about the revision it was asked about.
			auto anchoredReady = wait_terms{ };
			if (!wait_until_reporting(
					anchoredReady,
					[&application, chat](wait_terms& aTerms) {
						QApplication::processEvents();
						const auto now = app::detail::
							github_application_fixture_access::agent_chat_evidence(
								*application);
						if (now.Projection.has_value()
							&& now.Projection->Continuations.Required
							&& chat->continue_old_revision_action() != nullptr
							&& chat->continue_old_revision_action()->isEnabled()) {
							chat->continue_old_revision_action()->trigger();
						}
						return aTerms.require(
							"send-enabled", chat->send_action()->isEnabled());
					},
					extended_agent_budget)) {
				return fail(
					*application, "anchored-ready", anchoredReady.render());
			}
		}

		// The head moving cleared the workbench selection with the comparison
		// it belonged to, so the reviewer points at something again before
		// asking about it: `ask_about_selection` is one of the three entry
		// points the core refuses without a selection. Navigating is how one
		// is made, and navigating adds no context by itself - the chat's own
		// scope and session are untouched by it.
		auto continuedFiles = wait_terms{ };
		if (!wait_until_reporting(
				continuedFiles,
				[files](wait_terms& aTerms) {
					QApplication::processEvents();
					return aTerms.require(
						"files-listed", files->model()->rowCount() != 0);
				},
				extended_agent_budget)) {
			return fail(*application, "continued-files", continuedFiles.render());
		}
		files->selectionModel()->clearCurrentIndex();
		QApplication::processEvents();
		files->selectionModel()->setCurrentIndex(
			files->model()->index(0, 0),
			QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
		auto continuedSelection = wait_terms{ };
		if (!wait_until_reporting(
				continuedSelection,
				[surface](wait_terms& aTerms) {
					QApplication::processEvents();
					if (
						!surface->selection().has_value()
						&& surface->current_result() != nullptr) {
						static_cast<void>(surface->navigate_change(
							core::change_navigation_direction::next,
							core::change_navigation_policy::stop));
					}
					return aTerms.require(
							   "comparison-shown",
							   surface->current_result() != nullptr)
						&& aTerms.require(
							"selection-present",
							surface->selection().has_value());
				},
				extended_agent_budget)) {
			return fail(
				*application, "continued-selection", continuedSelection.render());
		}

		chat->composer()->setPlainText(QString::fromUtf8(
			continued_message.data(),
			static_cast<qsizetype>(continued_message.size())));
		QApplication::processEvents();
		const auto beforeContinuedSend = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		chat->send_action()->trigger();
		QApplication::processEvents();

		// The composition half, checked before the answer rather than after
		// it. One revision said three times - the binding the MCP helper is
		// pinned to, the binding record the frozen snapshot carries on disk,
		// and the session the reviewer kept - and it is not the head on
		// screen. BPR-0532 moved the freeze off the gesture: a continued turn
		// asks for its own revision's alignment first and is dispatched when
		// that answers or is refused, so this waits for the freeze the
		// dispatch writes instead of reading it out of the send.
		auto continuedFreeze = wait_terms{ };
		if (!wait_until_reporting(
				continuedFreeze,
				[&application, &beforeContinuedSend](wait_terms& aTerms) {
					QApplication::processEvents();
					const auto now = app::detail::
						github_application_fixture_access::agent_finding_evidence(
							*application);
					aTerms.note_identifier(
						"realignment", now.LastContinuationRealignment);
					return aTerms.require(
						"frozen-written",
						now.FrozenSnapshotWrites
							== beforeContinuedSend.FrozenSnapshotWrites + 1U);
				},
				extended_agent_budget)) {
			return fail(*application, "continued-freeze", continuedFreeze.render());
		}
		const auto composed = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		const auto pinnedBinding = composed.McpLaunch.has_value()
				&& composed.McpLaunch->Environment.size() == 2U
			? composed.McpLaunch->Environment[1].second
			: std::string{ };
		const auto recordedBinding
			= snapshot_binding_record(std::filesystem::path{
				composed.McpLaunch.has_value()
					? composed.McpLaunch->WorkingDirectory
					: std::string{ } });
		const auto keptEncoded
			= adapters::mcp::encode_revision_binding(keptBinding);
		const auto observedEncoded
			= adapters::mcp::encode_revision_binding(*afterContinue.Observed);
		const auto oldRevisionComposed = !keptEncoded.empty()
			&& keptEncoded != observedEncoded && pinnedBinding == keptEncoded
			&& recordedBinding == keptEncoded
			&& composed.FrozenSnapshotWrites
				== beforeContinuedSend.FrozenSnapshotWrites + 1U;
		// What the turn was actually dispatched with, and why. The two are one
		// claim: a re-alignment that answered froze this revision's own
		// identities, and any other reason froze nothing at all. The fixture
		// retains both revisions, so which of the two this course takes is the
		// composition's answer and not the fixture's.
		const auto continuedFrozen = snapshot_alignment_record(
			std::filesystem::path{ composed.McpLaunch.has_value()
									   ? composed.McpLaunch->WorkingDirectory
									   : std::string{ } });
		// What a re-alignment that answered must have produced: the old
		// revision's own identities, byte for byte, as the shipped diff path
		// independently computed them for that same revision before the review
		// left it. `keptFrozen` was read off disk then, from the turn that ran
		// against it, so this compares a recompute against an independent
		// compute rather than against itself.
		const auto realignedIdentities = continuedFrozen.Files == 1U
			&& keptFrozen.Files == 1U && !keptFrozen.Comparison.empty()
			&& continuedFrozen.Path == keptFrozen.Path
			&& continuedFrozen.Comparison == keptFrozen.Comparison
			&& continuedFrozen.LeftLineCount == keptFrozen.LeftLineCount
			&& continuedFrozen.RightLineCount == keptFrozen.RightLineCount
			&& continuedFrozen.Segments == keptFrozen.Segments
			&& continuedFrozen.Groups == keptFrozen.Groups
			&& continuedFrozen.Moves == keptFrozen.Moves;
		// The reason and the record are one claim. `none` means the old
		// revision was aligned again and its identities are what this turn
		// froze; any other reason means the documented fallback - nothing
		// frozen, every finding unanchored - and a turn that recorded one and
		// did the other would be lying about which revision it was answered
		// against.
		const auto realignmentStated
			= composed.LastContinuationRealignment == "none"
			? realignedIdentities
			: continuedFrozen.Files == 0U;
		// The anchored course requires all three, because the invariant above
		// is satisfied by a turn that froze nothing for a stated reason and
		// that is the behaviour this card exists to replace. The retarget must
		// be real - it sent only after the accepted comparison became the new
		// pair, so the pair the kept session names is not the pair on screen
		// and the request asks for something the current selection cannot
		// supply; the re-alignment must have answered rather than refused; and
		// what it produced must be the old revision's own identities.
		const auto anchoredFreeze = !anchored
			|| (composed.LastContinuationRealignmentRetargeted
				&& composed.LastContinuationRealignment == "none"
				&& realignedIdentities);
		if (!oldRevisionComposed || !realignmentStated || !anchoredFreeze) {
			std::cerr << "agent-chat-continued-composition:"
					  << "\npinned:" << pinnedBinding
					  << "\nrecorded:" << recordedBinding
					  << "\nkept:" << keptEncoded
					  << "\nobserved:" << observedEncoded
					  << "\nrealignment:" << composed.LastContinuationRealignment
					  << "\nretargeted:"
					  << composed.LastContinuationRealignmentRetargeted
					  << "\nfrozen-files:" << continuedFrozen.Files
					  << "\nfrozen-comparison:" << continuedFrozen.Comparison
					  << "\nkept-comparison:" << keptFrozen.Comparison
					  << "\nfrozen-segments:" << continuedFrozen.Segments.size()
					  << "\nkept-segments:" << keptFrozen.Segments.size()
					  << "\nfreezes:" << beforeContinuedSend.FrozenSnapshotWrites
					  << "->" << composed.FrozenSnapshotWrites << '\n';
			return fail(*application, "continued-old-revision-composition");
		}
		auto continuedAnswer = wait_terms{ };
		if (!wait_until_reporting(
				continuedAnswer,
				[&application](wait_terms& aTerms) {
					const auto invocationNow = app::detail::
						github_application_fixture_access::agent_invocation_evidence(
							*application);
					const auto chatNow = app::detail::
						github_application_fixture_access::agent_chat_evidence(
							*application);
					aTerms.note(
						"process",
						static_cast<std::uint64_t>(
							invocationNow.Statistics.ProcessStarts));
					aTerms.note(
						"replies",
						static_cast<std::uint64_t>(chatNow.Statistics.Replies));
					aTerms.note_identifier(
						"panel", chatNow.AgentPanelStatusText);
					return aTerms.require(
							   "second-run-quiescent",
							   invocationNow.Quiescent
								   && invocationNow.Statistics.ProcessStarts == 2U)
						&& aTerms.require(
							"second-reply-delivered",
							chatNow.Statistics.Replies == 2U);
				},
				extended_agent_budget)) {
			return fail(
				*application, "continued-turn", continuedAnswer.render());
		}
		QApplication::processEvents();
		detail_queued_finding(*application);
		const auto afterTurn = app::detail::
			github_application_fixture_access::agent_chat_evidence(*application);
		const auto continuedFinding = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		const auto continuedMutation = app::detail::
			github_application_fixture_access::review_mutation_evidence(
				*application);
		const auto continuedTurn = afterTurn.Statistics.Turns == 2U
			&& afterTurn.Statistics.SessionsStarted
				== beforeContinue.Statistics.SessionsStarted
			// The turn continued the session rather than starting a second
			// one, so nothing was adopted and the chat still holds the binding
			// it kept: an admission bound to the head on screen would have
			// retargeted it here, after the reviewer said not to.
			&& afterTurn.Statistics.SessionsAdopted
				== beforeContinue.Statistics.SessionsAdopted
			&& afterTurn.Session.has_value()
			&& afterTurn.Session->Id == keptSessionId
			&& afterTurn.Session->Binding == keptBinding
			&& afterTurn.LastTurn.has_value()
			// The turn ran under the session the reviewer kept, bound to the
			// revision that session was asked about and not to the one on
			// screen.
			&& afterTurn.LastTurn->Session.Id == keptSessionId
			&& afterTurn.LastTurn->Session.Binding == keptBinding
			&& afterTurn.Observed.has_value()
			&& afterTurn.LastTurn->Session.Binding != *afterTurn.Observed
			// Carrying the second question, and the agent received it once.
			&& afterTurn.LastTurn->Message == continued_message
			&& continuedFinding.QueuedClaim.find(
				   exactly_once_echo(continued_message))
				!= std::string::npos
			&& continuedFinding.FindingStatistics.Deliveries == 2U;
		// The turn completed. A binding change during a continued turn used to
		// supersede it, which arrived as `stale_result` and an answer the
		// reviewer asked for and never got; the panel names the outcome, so
		// the panel is where this is read.
		const auto continuedCompleted
			= afterTurn.AgentPanelStatusText.find("Finished: success")
			!= std::string::npos;
		// The finding the continued turn returned, validated against what that
		// turn froze. Frozen identities that no finding is checked against
		// would be a file on disk and nothing more; this is the queue's own
		// verdict on the answer the reviewer will read, and it can only say
		// `anchored` because the old revision's identities were there to
		// validate against.
		const auto anchoredFinding
			= !anchored || continuedFinding.QueuedAnchorState == "anchored (none)";
		const auto continuedNoMutation = continuedMutation.Submissions == 0U
			&& continuedMutation.TargetObservations == 0U
			&& continuedMutation.StoredComments == 0U;
		const auto continuedPassed = positive && staleRefused && keptSession
			&& oldRevisionComposed && anchoredFreeze && realignmentStated
			&& anchoredFinding && continuedTurn && continuedCompleted
			&& continuedNoMutation;
		if (!continuedPassed) {
			std::cerr << "agent-chat-continued-evidence:" << positive << ':'
					  << staleRefused << ':' << keptSession << ':'
					  << oldRevisionComposed << ':' << continuedTurn << ':'
					  << continuedCompleted << ':' << anchoredFinding << ':'
					  << continuedNoMutation << ':'
					  << afterTurn.Statistics.Turns << ':'
					  << afterTurn.Statistics.SessionsStarted << ':'
					  << afterTurn.Statistics.Continuations
					  << "\nqueued-claim:" << continuedFinding.QueuedClaim
					  << "\nanchor-state:" << continuedFinding.QueuedAnchorState
					  << "\nsession-state:" << afterTurn.SessionStateText
					  << "\npanel-status:" << afterTurn.AgentPanelStatusText
					  << '\n';
		}
		if (!shutdown(*application) || !continuedPassed) {
			return EXIT_FAILURE;
		}
		std::cout << (anchored ? "agent-chat-anchored-continuation:pass\n"
							   : "agent-chat-continued:pass\n");
		return EXIT_SUCCESS;
	}

	// The continuation that starts one. Exactly one session, and no process.
	const auto beforeChoice
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);
	const auto beforeChoiceInvocation
		= app::detail::github_application_fixture_access::agent_invocation_evidence(
			*application);
	if (
		chat->new_session_action() == nullptr
		|| !chat->new_session_action()->isEnabled()) {
		return fail(*application, "continuation-action");
	}
	chat->new_session_action()->trigger();
	QApplication::processEvents();
	const auto afterChoice
		= app::detail::github_application_fixture_access::agent_chat_evidence(
			*application);
	const auto afterChoiceInvocation
		= app::detail::github_application_fixture_access::agent_invocation_evidence(
			*application);
	const auto startedOne = afterChoice.Statistics.Continuations
			== beforeChoice.Statistics.Continuations + 1U
		&& afterChoice.Statistics.SessionsStarted
			== beforeChoice.Statistics.SessionsStarted + 1U
		&& afterChoice.Statistics.SessionsAdopted
			== beforeChoice.Statistics.SessionsAdopted
		&& afterChoice.Session.has_value() && beforeChoice.Session.has_value()
		&& afterChoice.Session->Id != beforeChoice.Session->Id
		// The new session is bound to what the reviewer is looking at now; the
		// old one kept the revision it was asked about.
		&& afterChoice.Observed.has_value()
		&& afterChoice.Session->Binding == *afterChoice.Observed
		&& beforeChoice.Session->Binding != *afterChoice.Observed
		// A continuation choice starts a session, and starts no process.
		&& afterChoiceInvocation.Statistics.ProcessStarts
			== beforeChoiceInvocation.Statistics.ProcessStarts
		&& afterChoiceInvocation.AgentProcessLauncherInvocations
			== beforeChoiceInvocation.AgentProcessLauncherInvocations
		&& chat->send_action()->isEnabled();

	const auto passed = positive && staleRefused && startedOne
		&& noGithubMutation;
	if (!passed) {
		std::cerr << "agent-chat-stale-evidence:" << positive << ':'
				  << staleRefused << ':' << startedOne << ':' << noGithubMutation
				  << ':' << afterStaleSend.Statistics.Refusals << ':'
				  << afterChoice.Statistics.SessionsStarted << ':'
				  << afterChoice.Statistics.Continuations
				  << "\npanel-status:" << afterStaleSend.AgentPanelStatusText
				  << "\nsession-state:" << afterChoice.SessionStateText << '\n';
	}
	if (!shutdown(*application) || !passed) {
		return EXIT_FAILURE;
	}
	std::cout << "agent-chat-stale:pass\n";
	return EXIT_SUCCESS;
}
