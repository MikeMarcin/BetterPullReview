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
/// calls anywhere. **stale**: after the head moves, a send against the session
/// the chat is still holding is refused by the core's own verdict and the
/// refusal is named on screen; choosing a continuation then starts exactly one
/// session and starts no process. **construction-cut**: with the chat host's
/// construction removed, the same send gesture is still made and still
/// observed, its turn count is zero, and the positive marker is false.

#include "public_path_poll.hpp"

#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QTemporaryDir>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	using e2e::default_agent_budget;
	using e2e::extended_agent_budget;
	using e2e::wait_terms;

	/// Exactly what the reviewer types into the composed panel's composer.
	constexpr std::string_view chat_message
		= "BPR_CHAT_SEND_0514 what does this retry budget do?";

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
		course != "positive" && course != "stale"
		&& course != "construction-cut") {
		return EXIT_FAILURE;
	}
	auto data = QTemporaryDir{ };
	if (!data.isValid()) {
		return EXIT_FAILURE;
	}
	const auto dataRoot    = std::filesystem::path{ data.path().toStdString() };
	auto       application = app::detail::github_application_fixture_access::make(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot               = dataRoot,
			.EpochPhase                        = app::detail::github_epoch_fixture_phase::single,
			.DisableChatSessionHostComposition = course == "construction-cut",
			.CodexAgentExecutable              = std::filesystem::path{ argv[2] },
		});
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
	window.session_panel()->owner_editor()->setText(QStringLiteral("octo-org"));
	window.session_panel()->repository_editor()->setText(
		QStringLiteral("review-repository"));
	window.session_panel()->pull_request_editor()->setValue(17);
	window.github_open_action()->trigger();

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
		// One declaration and exactly two call sites: the launch panel's start
		// gesture and the chat host's turn dispatcher.
		&& occurrences(source, "dispatch_agent_invocation(") == 3U
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
		&& noGithubMutation && oneLaunchPath;

	if (course == "positive") {
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
	// old one, and the session is never retargeted to follow it.
	if (!app::detail::github_application_fixture_access::reaccept_fixture_revision(
			*application, true)) {
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

	// The continuation. Exactly one session starts, and no process does.
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
