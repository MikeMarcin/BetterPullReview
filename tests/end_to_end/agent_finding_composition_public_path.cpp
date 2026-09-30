/// \file
/// \brief RFC-BPR-0012 composition, driven through the shipped application.
///
/// \details Modelled on `agent_invocation_composition_public_path.cpp`, and
/// deliberately the same shape: one real fixture-served review, real gestures,
/// and a construction cut per composed surface.
///
/// What this course is for, in one sentence: a reviewer's gesture reaches one
/// delivered finding, converting it produces one local draft, and no GitHub
/// mutation call happens anywhere along the way.

#include "private_data_root.hpp"
#include "public_path_poll.hpp"
#include "right_side_change.hpp"

#include "../../src/app/src/github_application_fixture_access.hpp"

#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/workbench_drawer.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QListView>
#include <QObject>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	using e2e::default_agent_budget;
	using e2e::extended_agent_budget;
	using e2e::wait_terms;

	/// The reviewer's own comment, byte for byte, before any agent sees it.
	constexpr std::string_view reviewer_original
		= "BPR_REVIEWER_ORIGINAL_0504 retry\tbudget? ";
	/// Exactly what `agent_finding_canary_main.cpp` proposes instead.
	constexpr std::string_view agent_suggested
		= "BPR_FINDING_SUGGESTED_0504 name the exhausted retry in the message";
	constexpr std::string_view agent_claim
		= "BPR_FINDING_CLAIM_0504 the retry budget is not reported to the caller";
	constexpr auto contextual_cue_cooldown_ms = 2'100UL;

	/// \brief Saves the real composed workshop when a review capture directory is supplied.
	auto capture_workshop(qt::workbench_window& aWindow, const QString& aStage) -> bool
	{
		const auto directory = qEnvironmentVariable("BPR_DIF_WORKSHOP_RENDER_DIR");
		return directory.isEmpty()
			|| aWindow.grab().save(QDir{ directory }.filePath(aStage + QStringLiteral(".png")));
	}

	/// \brief Records the visible sprite's paint events inside event-loop delivery.
	/// \details A start and a terminal can both land before the next wait predicate.
	/// Observing paints retains a short cue without delaying the agent or the UI.
	class dif_paint_observation final : public QObject
	{
	public:
		explicit dif_paint_observation(qt::dif_widget& aDif)
			: Dif{ aDif }
		{
			Dif.installEventFilter(this);
		}

		auto saw(qt::dif_clip_id aClip) const -> bool
		{
			return Painted[qt::dif_index(aClip)];
		}

	protected:
		auto eventFilter(QObject* aObject, QEvent* aEvent) -> bool override
		{
			if (aObject == &Dif && aEvent->type() == QEvent::Paint && Dif.isVisible()) {
				const auto clip = Dif.current_frame().Clip;
				if (clip < Painted.size()) {
					Painted[clip] = true;
				}
			}
			return false;
		}

	private:
		qt::dif_widget&      Dif;
		std::array<bool, 10> Painted{ };
	};

	template <typename Predicate>
	[[nodiscard]] auto wait_until_reporting(
		wait_terms&                     aTerms,
		Predicate                       aPredicate,
		const std::chrono::milliseconds aBudget = default_agent_budget) -> bool
	{
		return e2e::wait_until(aTerms, aPredicate, aBudget);
	}

	/// \brief Every term of a contract read once, after the run, by name.
	///
	/// \details [[BPR-0835]]. `wait_terms` stops at the first false term, which
	/// suits a wait that is evaluated again until it holds. The evidence a
	/// course reads at its end is read once, so stopping there would hide every
	/// false term after the first; and a course that printed only its counters
	/// ended with none named, because the term that failed was not one of them.
	/// Every name is a literal of this file, so nothing from the run is printed
	/// through it.
	class contract_terms final
	{
	public:
		/// \brief Records one term and returns its value.
		auto require(const std::string_view aName, const bool aValue) -> bool
		{
			if (!aValue) {
				Failed += Failed.empty() ? "" : " ";
				Failed += aName;
			}
			return aValue;
		}

		/// \brief Whether every recorded term held.
		[[nodiscard]] auto held() const noexcept -> bool
		{
			return Failed.empty();
		}

		/// \brief The space-separated names of the terms that did not hold.
		[[nodiscard]] auto failed() const noexcept -> std::string_view
		{
			return Failed;
		}

	private:
		std::string Failed;
	};

	/// \brief Names the step a course stopped at, and why, on one line.
	void report_failure(const std::string_view aStep, const std::string_view aState = { })
	{
		std::cerr << "agent-finding-composition-failure:" << aStep;
		if (!aState.empty()) {
			std::cerr << ' ' << aState;
		}
		std::cerr << '\n';
	}

	/// \brief Names a contract's false terms under its step, if there are any.
	///
	/// \returns Whether the contract held.
	auto report_contract(const std::string_view aStep, const contract_terms& aTerms)
		-> bool
	{
		if (!aTerms.held()) {
			report_failure(aStep, "failed=" + std::string{ aTerms.failed() });
		}
		return aTerms.held();
	}

	[[nodiscard]] auto shutdown(app::github_application& aApplication) -> bool
	{
		aApplication.begin_shutdown();
		auto       terms    = wait_terms{ };
		const auto complete = wait_until_reporting(
			terms,
			[&aApplication](wait_terms& aTerms) {
				QApplication::processEvents();
				return aTerms.require(
					"shutdown-complete", aApplication.status().ShutdownComplete);
			},
			extended_agent_budget);
		if (!complete) {
			report_failure("shutdown", terms.render());
		}
		return complete;
	}

	[[nodiscard]] auto fail(
		app::github_application& aApplication,
		const std::string_view   aReason,
		const std::string_view   aState = { }) -> int
	{
		report_failure(aReason, aState);
		static_cast<void>(shutdown(aApplication));
		return EXIT_FAILURE;
	}

	/// \brief Whether the composed helper launch is exactly what was pinned.
	///
	/// \details Three properties, all of them absences: no argument vector, an
	/// environment of exactly the two allowlisted names in the adapter's own
	/// order, and a working directory that is the pinned snapshot root and
	/// names no host. A supervisor that could add a fourth thing to this list
	/// could add `--listen`.
	auto exact_helper_launch(
		const app::detail::agent_finding_composition_evidence& aEvidence,
		const std::filesystem::path&                           aSnapshotRoot,
		contract_terms&                                        aTerms) -> bool
	{
		if (!aTerms.require("helper-launch-described", aEvidence.McpLaunch.has_value())) {
			return false;
		}
		const auto& launch = *aEvidence.McpLaunch;
		const auto  names
			= application::mcp_context_helper_supervisor::environment_names();
		if (!aTerms.require(
				"helper-environment-is-allowlist-sized",
				launch.Environment.size() == adapters::mcp::mcp_environment_allowlist.size()
					&& names.size() == adapters::mcp::mcp_environment_allowlist.size())) {
			return false;
		}
		auto namesInOrder = true;
		for (auto index = std::size_t{ 0U }; index < names.size(); ++index) {
			namesInOrder = namesInOrder
				&& names[index] == adapters::mcp::mcp_environment_allowlist[index]
				&& launch.Environment[index].first == names[index];
		}
		const auto binding
			= adapters::mcp::decode_revision_binding(launch.Environment[1].second);
		const auto terms = std::array{
			aTerms.require("helper-environment-names-in-order", namesInOrder),
			aTerms.require(
				"helper-directory-is-snapshot-root",
				// [[BPR-0835]]. The same directory, compared as paths. The helper is
				// launched in the native spelling, while the course's root keeps Qt's
				// forward slashes on Windows; lexical normalisation makes the
				// separators agree without resolving anything.
				std::filesystem::path{ launch.WorkingDirectory }.lexically_normal()
					== aSnapshotRoot.lexically_normal()),
			aTerms.require("helper-executable-named", !launch.Executable.empty()),
			aTerms.require(
				"helper-directory-names-no-host",
				!adapters::mcp::names_uri_scheme(launch.WorkingDirectory)
					&& !adapters::mcp::names_network_authority(launch.WorkingDirectory)
					&& !application::mcp_context_helper_supervisor::
						   names_network_location(launch.WorkingDirectory)),
			aTerms.require(
				"helper-binding-is-canonical",
				binding.has_value()
					&& launch.Environment[1].second
						== adapters::mcp::encode_revision_binding(*binding)),
		};
		return std::ranges::all_of(terms, std::identity{ });
	}

	/// \brief The marker every construction cut must turn false.
	///
	/// \details The five composed surfaces, the one finding, the one proposal
	/// accepted into the draft, a presented side chat, and the exact helper
	/// launch. Recorded term by term so the positive courses can name which of
	/// them failed; a cut reads only whether all of them held.
	auto record_positive_marker(
		const app::detail::agent_finding_composition_evidence& aEvidence,
		const std::filesystem::path&                           aSnapshotRoot,
		contract_terms&                                        aTerms) -> bool
	{
		const auto terms = std::array{
			aTerms.require("finding-host-composed", aEvidence.FindingHostComposed),
			aTerms.require(
				"comment-workshop-composed", aEvidence.CommentWorkshopComposed),
			aTerms.require("mcp-helper-composed", aEvidence.McpHelperComposed),
			aTerms.require("side-chat-panel-composed", aEvidence.SideChatPanelComposed),
			aTerms.require("finding-queue-composed", aEvidence.FindingQueueComposed),
			aTerms.require("queued-findings-is-one", aEvidence.QueuedFindings == 1U),
			aTerms.require(
				"workshop-proposals-is-one", aEvidence.WorkshopProposals == 1U),
			aTerms.require(
				"draft-body-is-suggestion", aEvidence.LocalDraftBody == agent_suggested),
			aTerms.require(
				"side-chat-presented", aEvidence.SideChatPresentations != 0U),
			exact_helper_launch(aEvidence, aSnapshotRoot, aTerms),
		};
		return std::ranges::all_of(terms, std::identity{ });
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication qtApplication{ argc, argv };
	QApplication::setQuitOnLastWindowClosed(false);
	if (argc != 3) {
		report_failure("arguments", "expected=<course> <canary>");
		return EXIT_FAILURE;
	}
	const auto course = std::string_view{ argv[1] };
	if (
		course != "positive" && course != "full-agent"
		&& course != "finding-host-construction-cut"
		&& course != "workshop-construction-cut"
		&& course != "mcp-construction-cut"
		&& course != "chat-panel-construction-cut"
		&& course != "queue-construction-cut"
		&& course != "workshop-press" && course != "dif-context") {
		report_failure("unknown-course");
		return EXIT_FAILURE;
	}
	auto data = QTemporaryDir{ };
	if (!data.isValid()) {
		report_failure("temporary-root");
		return EXIT_FAILURE;
	}
	// [[BPR-0812]], [[BPR-1050]]. The stores refuse a root that is not
	// owner-private, and the agent workspace neighbourhood beside the root
	// refuses a parent another principal can write.
	const auto privateRoot = e2e::private_data_root(data);
	if (!privateRoot) {
		report_failure("private-test-root");
		return EXIT_FAILURE;
	}
	const auto& dataRoot = *privateRoot;

	std::unique_ptr<app::github_application> application;
	auto                                     startupTerminal = false;
	auto                                     startup         = app::detail::github_application_fixture_access::start(
		app::detail::github_public_path_fixture::github_com,
		{
			.ApplicationDataRoot = dataRoot,
			.EpochPhase          = app::detail::github_epoch_fixture_phase::single,
			.DisableAgentFindingHostComposition
			= course == "finding-host-construction-cut",
			.DisableCommentWorkshopComposition
			= course == "workshop-construction-cut",
			.DisableMcpHelperComposition = course == "mcp-construction-cut",
			.DisableSideChatPanelComposition
			= course == "chat-panel-construction-cut",
			.DisableFindingQueueComposition
			= course == "queue-construction-cut",
			.CodexAgentExecutable = std::filesystem::path{ argv[2] },
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
					"profiles-is-four", panel->profile_selector()->count() == 4)
				&& aTerms.require(
					"disclosure-present", !panel->disclosure_text().isEmpty());
		})) {
		return fail(*application, "agent-offerings", offerings.render());
	}
	// [[BPR-0771]]. Opening from the Home bar lands on the reviewer's opening
	// modality, Overview by default; the retired locator this course opened
	// through left the window on Review. The course is about the review
	// surface, so it chooses Review from the strip, as a reviewer would.
	window.choose_modality(qt::review_modality::review);
	QApplication::processEvents();

	auto* const files = window.files_drawer()->contents_view();
	files->selectionModel()->setCurrentIndex(
		files->model()->index(0, 0),
		QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	auto selectedDiff = wait_terms{ };
	if (!wait_until_reporting(
			selectedDiff,
			[&window, panel](wait_terms& aTerms) {
				return aTerms.require(
						   "comparison-shown",
						   window.comparison_surface()->current_result() != nullptr)
					&& aTerms.require(
						"start-enabled", panel->start_action()->isEnabled());
			},
			extended_agent_budget)) {
		return fail(*application, "selected-diff", selectedDiff.render());
	}
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
	// [[BPR-1245]]. The first change is a deletion, left-side only, and a new
	// draft needs a right-side line ([[BPR-0587]]).
	if (!e2e::select_right_side_of_change(*surface)) {
		return fail(*application, "right-side-selection");
	}
	if (!window.read_action()->isEnabled()) {
		return fail(*application, "receipt-action");
	}
	window.read_action()->trigger();
	auto receipt = wait_terms{ };
	if (!wait_until_reporting(receipt, [&window](wait_terms& aTerms) {
			return aTerms.require(
				"read-checked", window.read_action()->isChecked());
		})) {
		return fail(*application, "receipt-projection", receipt.render());
	}

	auto* const editor     = window.review_editor();
	auto        draftReady = wait_terms{ };
	if (
		editor == nullptr
		|| !wait_until_reporting(draftReady, [editor](wait_terms& aTerms) {
			   return aTerms.require(
				   "draft-shown", editor->shown_draft().has_value());
		   })) {
		return fail(*application, "local-draft", draftReady.render());
	}
	// The reviewer's own bytes, tab and trailing space included, so a proposal
	// that overwrote them has nowhere to hide.
	editor->body_editor()->setPlainText(
		QString::fromUtf8(
			reviewer_original.data(),
			static_cast<qsizetype>(reviewer_original.size())));
	QApplication::processEvents();

	qt::dif_widget* dif           = nullptr;
	auto            difComparison = false;
	auto            difPaints     = std::unique_ptr<dif_paint_observation>{ };
	if (course == "dif-context") {
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown,
			  .Motion     = core::motion_mode::full },
			core::motion_observation::full));
		if (window.conversations_drawer()->state()
			!= core::workbench_drawer_state::expanded) {
			window.conversations_drawer_toggle_action()->trigger();
			auto drawer = wait_terms{ };
			if (!wait_until_reporting(drawer, [&window](wait_terms& aTerms) {
					return aTerms.require(
						"conversations-expanded",
						window.conversations_drawer()->state()
							== core::workbench_drawer_state::expanded);
				})) {
				return fail(*application, "dif-drawer", drawer.render());
			}
		}
		if (!window.ai_tools_action()->isChecked()) {
			window.ai_tools_action()->trigger();
		}
		auto placement = wait_terms{ };
		// [[BPR-1245]]. The corner Dif is one status row, 28 pixels
		// (8790cdc7b); the 40-pixel corner was rejected.
		if (!wait_until_reporting(placement, [&window, &dif](wait_terms& aTerms) {
				dif = window.dif();
				return aTerms.require("dif-created", dif != nullptr)
					&& aTerms.require(
						"dif-corner", window.dif_corner_slot()->isVisible() && dif->parentWidget() == &window && dif->size() == QSize{ 28, 28 })
					&& aTerms.require(
						"dif-visible", dif != nullptr && dif->isVisible());
			})) {
			return fail(*application, "dif-placement", placement.render());
		}
		difComparison = dif->current_frame().Clip
			== qt::dif_index(qt::dif_clip_id::compare_glance);
		QThread::msleep(contextual_cue_cooldown_ms);
		QApplication::processEvents();
		difPaints = std::make_unique<dif_paint_observation>(*dif);
	}

	const auto codexIndex = panel->profile_selector()->findData(
		QVariant::fromValue<qulonglong>(core::codex_jsonl_profile_id.value()));
	if (codexIndex < 0) {
		return fail(*application, "codex-profile");
	}
	panel->profile_selector()->setCurrentIndex(codexIndex);
	if (dif != nullptr) {
		// [[BPR-1209]]. The corner stays painted even when the private
		// content is scrolled. The cues keep their original triggers.
		auto exposure = wait_terms{ };
		if (!wait_until_reporting(exposure, [dif](wait_terms& aTerms) {
				return aTerms.require("dif-in-viewport", !dif->visibleRegion().isEmpty());
			})) {
			return fail(*application, "dif-exposure", exposure.render());
		}
	}
	// BPR-0533. The workshop course starts its turn from the comment-workshop
	// control instead of the launch panel, and from nowhere else: the press is
	// the only gesture it makes, so every process, workspace, and delivery
	// counted below belongs to it.
	//
	// The refusal is read on the surface that asked rather than on the launch
	// panel: the panel deliberately ignores a refusal while an invocation is
	// running, so a second press would leave no trace there and "never queued
	// silently" would be unfalsifiable. On the control it is the control's own
	// state, which is also where the reviewer would read it.
	auto pressedRefusal         = std::string{ };
	auto reofferedAfterProposal = false;
	if (course == "workshop-press") {
		auto* const clarify = editor->clarify_action();
		if (clarify == nullptr || !clarify->isEnabled()) {
			return fail(*application, "workshop-control");
		}
		clarify->trigger();
		QApplication::processEvents();
		if (!clarify->isEnabled()) {
			pressedRefusal = clarify->toolTip().toStdString();
		}
	} else {
		panel->start_button()->click();
	}

	auto delivered = wait_terms{ };
	if (!wait_until_reporting(
			delivered,
			[&application](wait_terms& aTerms) {
				const auto invocation = app::detail::
					github_application_fixture_access::agent_invocation_evidence(
						*application);
				aTerms.note(
					"process",
					static_cast<std::uint64_t>(
						invocation.Statistics.ProcessStarts));
				// [[BPR-0840]]. A turn that ended without launching never
				// satisfies this wait, so the report names how it ended.
				aTerms.note_identifier(
					"panel",
					app::detail::github_application_fixture_access::
						agent_finding_evidence(*application)
							.AgentPanelStatusText);
				return aTerms.require(
						   "run-quiescent",
						   invocation.Quiescent
							   && invocation.Statistics.ProcessStarts == 1U)
					&& aTerms.require(
						"cleanups-is-one",
						invocation.Statistics.WorkspaceCleanups == 1U);
			},
			extended_agent_budget)) {
		return fail(*application, "agent-run", delivered.render());
	}
	QApplication::processEvents();
	const auto difSawConsider = difPaints != nullptr
		&& difPaints->saw(qt::dif_clip_id::consider);
	const auto difAttention = dif != nullptr
		&& dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::attention);

	// BPR-0538. The same terminal the first turn was waited on, for every turn
	// after it: quiescent, with exactly as many processes started and
	// workspaces cleaned up as turns asked for. A stage that read its evidence
	// before its turn ended would be reading the previous turn's.
	const auto awaitTurn = [&application](const std::size_t aTurns) {
		auto       terms   = wait_terms{ };
		const auto settled = wait_until_reporting(
			terms,
			[&application, aTurns](wait_terms& aTerms) {
				const auto invocation = app::detail::
					github_application_fixture_access::agent_invocation_evidence(
						*application);
				aTerms.note(
					"process",
					static_cast<std::uint64_t>(
						invocation.Statistics.ProcessStarts));
				aTerms.note_identifier(
					"panel",
					app::detail::github_application_fixture_access::
						agent_finding_evidence(*application)
							.AgentPanelStatusText);
				return aTerms.require(
						   "run-quiescent",
						   invocation.Quiescent
							   && invocation.Statistics.ProcessStarts == aTurns)
					&& aTerms.require(
						"cleanups-match",
						invocation.Statistics.WorkspaceCleanups == aTurns);
			},
			extended_agent_budget);
		if (!settled) {
			std::cerr << "agent-finding-workshop-press-turn:" << aTurns << ':'
					  << terms.render() << '\n';
		}
		QApplication::processEvents();
		return settled;
	};

	// [[BPR-0705]]. A snapshot belongs to the window session that froze it:
	// the helper's root is `window-<id>-generation-<generation>` under the
	// profile's `mcp-snapshot` directory, derived from the typed identity of
	// that window, and cleanup removes only it. The course derives the path
	// from the one live window it drove rather than reading it back from the
	// helper it is checking. [[BPR-0835]]: until it did, it expected the
	// directory itself and failed without naming a term.
	const auto snapshotParent = dataRoot / "mcp-snapshot";
	const auto liveWindows    = application->windows();
	const auto drivenWindow
		= liveWindows.size() == 1U && application->window(liveWindows.front()) == &window;
	const auto snapshotRoot = drivenWindow
		? snapshotParent
			/ ("window-" + std::to_string(liveWindows.front().Window.value())
			   + "-generation-" + std::to_string(liveWindows.front().Generation.value()))
		: std::filesystem::path{ };

	// Detailing the delivered finding is a Qt-local navigation and emits
	// nothing; it is what makes the seven disclosed facts readable.
	auto* const queue
		= app::detail::github_application_fixture_access::finding_queue(
			*application);
	if (queue != nullptr && queue->finding_count() != 0U) {
		queue->select(0U);
		QApplication::processEvents();
	}
	const auto afterDelivery
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);
	if (course == "dif-context") {
		QThread::msleep(contextual_cue_cooldown_ms);
		QApplication::processEvents();
	}

	// The conversion gesture, on the composed queue, against the finding the
	// run delivered. Nothing is published by it and nothing can be.
	const auto draftBeforeConversion = editor->shown_draft();
	if (queue != nullptr && queue->finding_count() != 0U) {
		auto* const convert
			= queue->action(qt::finding_queue_action::convert_to_draft_comment);
		if (convert == nullptr) {
			return fail(*application, "convert-action");
		}
		convert->trigger();
		QApplication::processEvents();
	}
	const auto afterConversion
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);
	const auto difCommentReady = dif != nullptr
		&& dif->current_frame().Clip
			== qt::dif_index(qt::dif_clip_id::comment_ready);
	const auto proposalMatchesDraft = draftBeforeConversion.has_value()
		&& editor->shown_proposal().has_value()
		&& editor->shown_proposal()->Draft == draftBeforeConversion->Id
		&& editor->shown_draft() == draftBeforeConversion;
	const auto proposalEditorVisible   = editor->isVisible();
	const auto proposalCornerVisible   = window.dif_corner_slot()->isVisible();
	const auto proposalDifVisible      = dif != nullptr && dif->isVisible();
	const auto proposalWorkshopVisible = window.dif_workshop_slot()->isVisible()
		&& dif != nullptr && dif->parentWidget() == window.dif_workshop_slot()
		&& !dif->visibleRegion().isEmpty();
	const auto draftIntentsBeforeAcceptance = editor->statistics().DraftIntents;
	if (course == "dif-context" && !capture_workshop(window, QStringLiteral("proposal"))) {
		return fail(*application, "proposal-capture");
	}

	if (course == "workshop-press" && editor->clarify_action() != nullptr) {
		// Answered, so the offer is open again. A control that stayed shut
		// after its own answer arrived would be a workshop good for one
		// question per comment.
		reofferedAfterProposal = editor->clarify_action()->isEnabled();
	}

	// The acceptance gesture, on the shipped editor. This is the one gesture
	// that emits a local draft command, and it emits exactly one.
	if (
		afterConversion.WorkshopProposals == 1U
		&& editor->accept_revision_action() != nullptr
		&& editor->accept_revision_action()->isEnabled()) {
		if (course == "dif-context") {
			QThread::msleep(contextual_cue_cooldown_ms);
			QApplication::processEvents();
			auto* page   = window.findChild<QScrollArea*>(QStringLiteral("composeInlineDraftPage"));
			auto* accept = window.findChild<QToolButton*>(QStringLiteral("reviewAcceptRevisionButton"));
			if (page == nullptr || accept == nullptr || !accept->isVisible() || !accept->isEnabled()) {
				return fail(*application, "workshop-accept-button");
			}
			page->verticalScrollBar()->setValue(page->verticalScrollBar()->maximum());
			QApplication::processEvents();
			if (dif->visibleRegion().isEmpty() || accept->visibleRegion().isEmpty()) {
				return fail(*application, "workshop-header-scroll");
			}
			accept->click();
		} else {
			editor->accept_revision_action()->trigger();
		}
		QApplication::processEvents();
	}
	// [[BPR-1245]]. Dif acknowledges when the draft is accepted, which with
	// private review is on the private review's answer ([[BPR-0587]]), so the
	// course waits for it rather than reading the frame at once.
	auto       acknowledged   = wait_terms{ };
	const auto difAcknowledge = dif != nullptr
		&& wait_until_reporting(acknowledged, [dif](wait_terms& aTerms) {
									return aTerms.require("acknowledge", dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::acknowledge));
								});
	const auto acceptedMatchesDraft = draftBeforeConversion.has_value()
		&& editor->shown_draft().has_value()
		&& editor->shown_draft()->Id == draftBeforeConversion->Id;
	const auto acceptedEditorVisible   = editor->isVisible();
	const auto acceptedCornerVisible   = window.dif_corner_slot()->isVisible();
	const auto acceptedDifVisible      = dif != nullptr && dif->isVisible();
	const auto acceptedWorkshopVisible = window.dif_workshop_slot()->isVisible()
		&& dif != nullptr && dif->parentWidget() == window.dif_workshop_slot()
		&& !dif->visibleRegion().isEmpty();
	const auto acceptanceEmitsOneDraft = editor->statistics().DraftIntents
		== draftIntentsBeforeAcceptance + 1U;
	if (course == "dif-context" && !capture_workshop(window, QStringLiteral("accepted"))) {
		return fail(*application, "accepted-capture");
	}
	// BPR-0538. Two more stages, on the press course only, and both driven
	// through the same composed graph: the turn that answers nothing, and the
	// turn that belongs to somebody else.
	auto silentReason       = std::string{ "unread" };
	auto silentPending      = std::string{ "unread" };
	auto silentReoffered    = false;
	auto boundPending       = std::string{ };
	auto boundSession       = false;
	auto unrelatedOperation = std::string{ "unread" };
	auto unrelatedPending   = std::string{ };
	auto unrelatedSession   = false;
	auto unrelatedProposals = std::size_t{ 0U };
	auto acceptedDraftBody  = std::string{ };
	if (course == "workshop-press") {
		acceptedDraftBody = app::detail::github_application_fixture_access::
								agent_finding_evidence(*application)
									.LocalDraftBody;
		// The reviewer's own words again, typed on the shipped editor. The
		// accepted revision above left the draft byte-identical to what this
		// fixture proposes, and a workshop asked to revise text into itself
		// refuses "unchanged" - which would make the stages below prove that
		// refusal instead of what they are about.
		editor->body_editor()->setPlainText(QString::fromUtf8(
			reviewer_original.data(),
			static_cast<qsizetype>(reviewer_original.size())));
		QApplication::processEvents();
		const auto reopened = [&editor]() {
			const auto six = std::array<QAction*, 6U>{
				editor->clarify_action(),
				editor->shorten_action(),
				editor->make_actionable_action(),
				editor->phrase_as_a_question_action(),
				editor->make_more_direct_action(),
				editor->check_the_premise_action(),
			};
			return std::ranges::all_of(six, [](const QAction* aControl) {
				return aControl != nullptr && aControl->isEnabled();
			});
		};
		// Stage two: the fixture answers `shorten` with a completed turn and
		// no findings, which is what an agent with nothing to say does. The
		// press must not survive it: nothing is coming, so the six reopen and
		// the surface says so where the before/after would have been.
		auto* const shorten = editor->shorten_action();
		if (shorten == nullptr || !shorten->isEnabled()) {
			return fail(*application, "workshop-shorten-control");
		}
		shorten->trigger();
		QApplication::processEvents();
		if (!awaitTurn(2U)) {
			return fail(*application, "workshop-silent-turn");
		}
		const auto afterSilent = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		silentReason    = afterSilent.WorkshopOperationText;
		silentPending   = afterSilent.WorkshopPendingOperation;
		silentReoffered = reopened();

		// Stage three: a press that is answered, left unconverted, and so
		// still bound to its own turn when the next one runs.
		auto* const clarify = editor->clarify_action();
		if (clarify == nullptr || !clarify->isEnabled()) {
			return fail(*application, "workshop-second-clarify-control");
		}
		clarify->trigger();
		QApplication::processEvents();
		if (!awaitTurn(3U)) {
			return fail(*application, "workshop-bound-turn");
		}
		const auto afterBound = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		boundPending = afterBound.WorkshopPendingOperation;
		boundSession = afterBound.WorkshopPendingSessionBound;

		// Stage four: a second, unrelated turn — started from the launch
		// panel, so it is nobody's press — runs, delivers, and is converted.
		// Neither its ending nor its outcome may consume the press stage three
		// left bound: a revision nobody workshopped is proposed with no
		// operation named, and the question stays outstanding.
		panel->start_button()->click();
		QApplication::processEvents();
		if (!awaitTurn(4U)) {
			return fail(*application, "workshop-unrelated-turn");
		}
		if (queue != nullptr && queue->finding_count() != 0U) {
			queue->select(0U);
			QApplication::processEvents();
			auto* const convert
				= queue->action(qt::finding_queue_action::convert_to_draft_comment);
			if (convert == nullptr) {
				return fail(*application, "workshop-unrelated-convert");
			}
			convert->trigger();
			QApplication::processEvents();
		}
		const auto afterUnrelated = app::detail::
			github_application_fixture_access::agent_finding_evidence(
				*application);
		unrelatedOperation = afterUnrelated.WorkshopOperationText;
		unrelatedPending   = afterUnrelated.WorkshopPendingOperation;
		unrelatedSession   = afterUnrelated.WorkshopPendingSessionBound;
		unrelatedProposals = afterUnrelated.WorkshopStatistics.Proposals;
	}

	const auto helperRefusal
		= app::detail::github_application_fixture_access::start_mcp_context_helper(
			*application,
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}\n");
	auto helperAnswered = wait_terms{ };
	static_cast<void>(
		helperRefusal != application::mcp_helper_refusal::none
		|| wait_until_reporting(
			helperAnswered,
			[&application](wait_terms& aTerms) {
				const auto current = app::detail::github_application_fixture_access::
					agent_finding_evidence(*application);
				return aTerms.require(
					"helper-completed", current.McpStatistics.Completions == 1U);
			},
			default_agent_budget));

	const auto evidence
		= app::detail::github_application_fixture_access::agent_finding_evidence(
			*application);
	const auto mutation
		= app::detail::github_application_fixture_access::review_mutation_evidence(
			*application);

	// Zero, everywhere, on every course. Converting a finding and accepting a
	// revision are local operations, and a backchannel from the agent host to
	// the RFC-BPR-0008 coordinator would show up here as a call nobody asked
	// for.
	const auto noGithubMutation = mutation.Submissions == 0U
		&& mutation.TargetObservations == 0U && mutation.StoredComments == 0U;

	auto       positiveTerms  = contract_terms{ };
	const auto positiveMarker = record_positive_marker(evidence, snapshotRoot, positiveTerms);

	if (course == "dif-context") {
		auto contract = contract_terms{ };
		contract.require("dif-showed-comparison", difComparison);
		contract.require("dif-saw-consider", difSawConsider);
		contract.require("dif-attention-after-delivery", difAttention);
		contract.require("dif-comment-ready-after-conversion", difCommentReady);
		contract.require("dif-acknowledge-after-acceptance", difAcknowledge);
		contract.require("proposal-in-compose-slot", proposalWorkshopVisible && !proposalCornerVisible);
		contract.require("accepted-in-compose-slot", acceptedWorkshopVisible && !acceptedCornerVisible);
		contract.require("ready-painted", difPaints->saw(qt::dif_clip_id::comment_ready));
		contract.require("acknowledge-painted", difPaints->saw(qt::dif_clip_id::acknowledge));
		contract.require("proposal-preserves-exact-draft", proposalMatchesDraft);
		contract.require("acceptance-preserves-draft-id", acceptedMatchesDraft);
		contract.require("acceptance-emits-one-draft", acceptanceEmitsOneDraft);
		contract.require("one-retained-dif-widget", window.dif() == dif && window.findChildren<QWidget*>(QStringLiteral("workbenchDif")).size() == 1);
		contract.require("delivery-queued-is-one", afterDelivery.QueuedFindings == 1U);
		contract.require(
			"queued-claim-is-agent-claim",
			afterDelivery.QueuedClaim.find(agent_claim) != std::string::npos);
		contract.require(
			"workshop-proposal-is-suggestion",
			afterConversion.WorkshopProposalText == agent_suggested);
		contract.require(
			"draft-body-is-suggestion", evidence.LocalDraftBody == agent_suggested);
		contract.require("no-github-mutation", noGithubMutation);
		// Leaving and returning exposes current content at rest, never a
		// queued success. Whole-review composition has no inline workshop.
		window.open_review_composition(false);
		QApplication::processEvents();
		contract.require("whole-review-hides-workshop", !window.dif_workshop_slot()->isVisible());
		window.open_review_composition(true);
		QApplication::processEvents();
		contract.require("return-keeps-one-neutral-sprite", window.dif() == dif && window.dif_workshop_slot()->isVisible() && dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::rest) && !dif->current_frame().NextMs);
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::hidden, .Motion = core::motion_mode::full },
			core::motion_observation::full));
		QApplication::processEvents();
		contract.require("hidden-preference-reclaims-workshop-slot", !window.dif_workshop_slot()->isVisible() && !dif->isVisible() && !dif->current_frame().NextMs);
		contract.require("hidden-art-keeps-acceptance-controls", window.findChild<QToolButton*>(QStringLiteral("reviewAcceptRevisionButton"))->isVisible() && editor->accept_revision_action()->isEnabled());
		window.adopt_dif(core::resolve_dif(
			{ .Visibility = core::dif_visibility::shown, .Motion = core::motion_mode::reduced },
			core::motion_observation::full));
		QApplication::processEvents();
		contract.require("reduced-workshop-rests", window.dif_workshop_slot()->isVisible() && dif->current_frame().Clip == qt::dif_index(qt::dif_clip_id::rest) && !dif->current_frame().NextMs);
		const auto acceptedDraft   = editor->shown_draft();
		const auto acceptedIntents = editor->statistics().DraftIntents;
		editor->reject_revision_action()->trigger();
		QApplication::processEvents();
		contract.require("dismissal-retires-only-workshop-slot", !window.dif_workshop_slot()->isVisible() && editor->shown_draft() == acceptedDraft && editor->statistics().DraftIntents == acceptedIntents);
		const auto finalMutation = app::detail::github_application_fixture_access::review_mutation_evidence(*application);
		contract.require("navigation-publishes-nothing", finalMutation.Submissions == 0U && finalMutation.TargetObservations == 0U && finalMutation.StoredComments == 0U);
		const auto passed = report_contract("dif-context-evidence", contract);
		if (!passed) {
			std::cerr << "agent-finding-dif-context-evidence:"
					  << difComparison << ':' << difSawConsider << ':'
					  << difAttention << ':' << difCommentReady << ':'
					  << difAcknowledge << ':' << afterDelivery.QueuedFindings
					  << ':' << mutation.Submissions << '\n';
			std::fprintf(stderr,
						 "agent-finding-dif-workshop-visibility:proposal-editor=%d proposal-ai-slot=%d proposal-dif=%d "
						 "accepted-editor=%d accepted-ai-slot=%d accepted-dif=%d proposal-workshop=%d accepted-workshop=%d "
						 "painted-consider=%d painted-ready=%d painted-acknowledge=%d\n",
						 proposalEditorVisible, proposalCornerVisible, proposalDifVisible,
						 acceptedEditorVisible, acceptedCornerVisible, acceptedDifVisible,
						 proposalWorkshopVisible, acceptedWorkshopVisible,
						 difPaints->saw(qt::dif_clip_id::consider),
						 difPaints->saw(qt::dif_clip_id::comment_ready),
						 difPaints->saw(qt::dif_clip_id::acknowledge));
		}
		if (!shutdown(*application) || !passed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-finding-dif-context:pass\n";
		return EXIT_SUCCESS;
	}

	if (course == "workshop-press") {
		// BPR-0533. Press → invocation → proposal, in the composed graph and
		// with no launch-panel gesture anywhere in it. The wait above already
		// required exactly one process start and one workspace cleanup, so the
		// run these findings came from is the one the press started.
		//
		// The three states of the asked-for operation are read in order, which
		// is what makes the pairing observable rather than assumed: recorded
		// and bound to its turn once the run is quiescent, still unspent while
		// the delivery sits in the queue, and spent — named on the proposal
		// and gone from the record — only by that turn's own outcome.
		auto contract = contract_terms{ };
		contract.require("finding-host-composed", evidence.FindingHostComposed);
		contract.require(
			"comment-workshop-composed", evidence.CommentWorkshopComposed);
		contract.require("no-github-mutation", noGithubMutation);
		contract.require(
			"deliveries-is-one", afterDelivery.FindingStatistics.Deliveries == 1U);
		contract.require("delivery-queued-is-one", afterDelivery.QueuedFindings == 1U);
		contract.require(
			"delivery-proposed-nothing", afterDelivery.WorkshopProposals == 0U);
		contract.require(
			"press-pending-after-delivery",
			afterDelivery.WorkshopPendingOperation == "clarify");
		contract.require(
			"press-bound-after-delivery", afterDelivery.WorkshopPendingSessionBound);
		// Never queued silently. While the turn was unanswered the six controls
		// were shut and said why, so there was no press to queue.
		contract.require(
			"second-press-refused-as-running",
			pressedRefusal.find("already running") != std::string::npos);
		contract.require("reoffered-after-proposal", reofferedAfterProposal);
		contract.require(
			"conversion-proposals-is-one", afterConversion.WorkshopProposals == 1U);
		contract.require(
			"workshop-statistics-proposals-is-one",
			afterConversion.WorkshopStatistics.Proposals == 1U);
		contract.require(
			"workshop-original-is-reviewer-text",
			afterConversion.WorkshopOriginalText == reviewer_original);
		contract.require(
			"workshop-proposal-is-suggestion",
			afterConversion.WorkshopProposalText == agent_suggested);
		// The before/after says which of the six produced it, and the question
		// it answered is no longer outstanding.
		contract.require(
			"proposal-names-clarify",
			afterConversion.WorkshopOperationText == "Proposed by: Clarify");
		contract.require(
			"press-spent-by-its-turn",
			afterConversion.WorkshopPendingOperation.empty());
		contract.require(
			"accepted-draft-is-suggestion", acceptedDraftBody == agent_suggested);
		contract.require(
			"final-draft-is-reviewer-text", evidence.LocalDraftBody == reviewer_original);
		// BPR-0538. The turn that ended without a revision took its press with
		// it: nothing is pending, the six are open, and the reason is stated
		// where the proposal would have been.
		contract.require("silent-turn-spent-press", silentPending.empty());
		contract.require(
			"silent-turn-says-why",
			silentReason == "No revision: the turn proposed no revision");
		contract.require("silent-turn-reoffered", silentReoffered);
		// And the press that is waiting on its own turn survives a second,
		// unrelated turn — its ending and its converted outcome both. The
		// revision nobody workshopped is proposed with no operation named.
		contract.require(
			"bound-press-pending", boundPending == "clarify" && boundSession);
		contract.require(
			"unrelated-turn-left-press-pending",
			unrelatedPending == "clarify" && unrelatedSession);
		contract.require("unrelated-proposal-names-nothing", unrelatedOperation.empty());
		contract.require("unrelated-proposals-is-two", unrelatedProposals == 2U);
		const auto pressed = report_contract("workshop-press-evidence", contract);
		if (!pressed) {
			std::cerr << "agent-finding-workshop-press-evidence:"
					  << afterDelivery.FindingStatistics.Deliveries << ':'
					  << afterDelivery.QueuedFindings << ':'
					  << afterConversion.WorkshopProposals << ':'
					  << mutation.Submissions
					  << "\npending-after-delivery:"
					  << afterDelivery.WorkshopPendingOperation
					  << "\npending-bound:"
					  << (afterDelivery.WorkshopPendingSessionBound ? "yes" : "no")
					  << "\npress-refusal:" << pressedRefusal
					  << "\nreoffered:"
					  << (reofferedAfterProposal ? "yes" : "no")
					  << "\noperation-text:"
					  << afterConversion.WorkshopOperationText
					  << "\npending-after-conversion:"
					  << afterConversion.WorkshopPendingOperation
					  << "\nworkshop-proposal:"
					  << afterConversion.WorkshopProposalText
					  << "\ndraft-body:" << evidence.LocalDraftBody
					  << "\naccepted-draft-body:" << acceptedDraftBody
					  << "\nsilent-reason:" << silentReason
					  << "\nsilent-pending:" << silentPending
					  << "\nsilent-reoffered:" << (silentReoffered ? "yes" : "no")
					  << "\nbound-pending:" << boundPending << ':'
					  << (boundSession ? "bound" : "unbound")
					  << "\nunrelated-pending:" << unrelatedPending << ':'
					  << (unrelatedSession ? "bound" : "unbound")
					  << "\nunrelated-operation:" << unrelatedOperation
					  << "\nunrelated-proposals:" << unrelatedProposals
					  << "\npanel-status:" << evidence.AgentPanelStatusText << '\n';
		}
		if (!shutdown(*application) || !pressed) {
			return EXIT_FAILURE;
		}
		std::cout << "agent-finding-workshop-press:pass\n";
		return EXIT_SUCCESS;
	}

	if (course == "positive" || course == "full-agent") {
		auto& contract = positiveTerms;
		contract.require("one-driven-window", drivenWindow);
		contract.require("no-github-submission", mutation.Submissions == 0U);
		contract.require("no-github-target-observation", mutation.TargetObservations == 0U);
		contract.require("no-github-stored-comment", mutation.StoredComments == 0U);
		contract.require("deliveries-is-one", afterDelivery.FindingStatistics.Deliveries == 1U);
		contract.require(
			"delivered-findings-is-one",
			afterDelivery.FindingStatistics.DeliveredFindings == 1U);
		contract.require("delivery-queued-is-one", afterDelivery.QueuedFindings == 1U);
		contract.require("frozen-files-is-one", afterDelivery.FrozenFileCount == 1U);
		contract.require(
			"frozen-snapshot-writes-is-one", afterDelivery.FrozenSnapshotWrites == 1U);
		contract.require(
			"queued-claim-is-agent-claim",
			afterDelivery.QueuedClaim.find(agent_claim) != std::string::npos);
		// A start gesture is not a chat turn, so the frame this run was handed
		// carried no question. The fixture counts what it received and says so:
		// zero here is the other half of the chat course's exactly-one.
		contract.require(
			"queued-claim-carried-no-question",
			afterDelivery.QueuedClaim.find("BPR_CHAT_ECHO_0517 count=0 text=")
				!= std::string::npos);
		contract.require(
			"queued-anchor-is-none",
			afterDelivery.QueuedAnchorState == "anchored (none)");
		contract.require(
			"delivery-proposed-nothing", afterDelivery.WorkshopProposals == 0U);
		contract.require(
			"conversion-actions-is-one",
			afterConversion.FindingStatistics.Actions == 1U);
		contract.require(
			"conversion-draft-commands-is-one",
			afterConversion.FindingStatistics.DraftCommands == 1U);
		contract.require(
			"conversion-stored-no-note",
			afterConversion.FindingStatistics.StoredNotes == 0U);
		contract.require(
			"conversion-proposals-is-one",
			afterConversion.WorkshopStatistics.Proposals == 1U);
		contract.require(
			"workshop-original-is-reviewer-text",
			afterConversion.WorkshopOriginalText == reviewer_original);
		contract.require(
			"workshop-proposal-is-suggestion",
			afterConversion.WorkshopProposalText == agent_suggested);
		// The conversion alone changes no draft. The reviewer's bytes are still
		// theirs until they accept.
		contract.require(
			"conversion-left-draft-alone",
			afterConversion.LocalDraftBody == reviewer_original);
		contract.require(
			"helper-not-refused",
			helperRefusal == application::mcp_helper_refusal::none);
		contract.require("helper-starts-is-one", evidence.McpStatistics.Starts == 1U);
		contract.require("helper-refusals-is-zero", evidence.McpStatistics.Refusals == 0U);
		contract.require(
			"helper-completions-is-one", evidence.McpStatistics.Completions == 1U);
		contract.require(
			"helper-answer-lists-tools",
			evidence.McpAnswer.find("snapshot_binding") != std::string::npos
				&& evidence.McpAnswer.find("read_snapshot_file") != std::string::npos);
		contract.require(
			"snapshot-binding-written",
			std::filesystem::exists(snapshotRoot / "binding"));
		contract.require(
			"snapshot-alignment-written",
			std::filesystem::exists(snapshotRoot / "alignment"));
		const auto exact = report_contract("positive-evidence", contract);
		if (!exact) {
			std::cerr << "agent-finding-positive-evidence:"
					  << afterDelivery.FindingStatistics.Deliveries << ':'
					  << afterDelivery.QueuedFindings << ':'
					  << afterDelivery.FrozenFileCount << ':'
					  << afterDelivery.FrozenSnapshotWrites << ':'
					  << afterConversion.WorkshopProposals << ':'
					  << afterConversion.FindingStatistics.DraftCommands << ':'
					  << static_cast<int>(helperRefusal) << ':'
					  << evidence.McpStatistics.Completions << ':'
					  << mutation.Submissions << ':' << mutation.TargetObservations
					  << "\nqueued-claim:" << afterDelivery.QueuedClaim
					  << "\nanchor-state:" << afterDelivery.QueuedAnchorState
					  << "\nworkshop-original:" << afterConversion.WorkshopOriginalText
					  << "\nworkshop-proposal:" << afterConversion.WorkshopProposalText
					  << "\ndraft-body:" << evidence.LocalDraftBody
					  << "\nmcp-answer:" << evidence.McpAnswer
					  << "\npanel-status:" << evidence.AgentPanelStatusText << '\n';
		}
		if (course == "positive") {
			if (!shutdown(*application) || !exact) {
				return EXIT_FAILURE;
			}
			std::cout << "agent-finding-composition:pass\n";
			return EXIT_SUCCESS;
		}
		// The full-agent half. The launch above is the full agent session: a
		// real child process, its own workspace, a returned structured
		// payload, and a promotion the reviewer accepted. What this course
		// adds is the *absence* the criterion is about.
		//
		// Two readings, because one alone would not be enough. The composed
		// run records zero GitHub mutation calls of any kind, so nothing left
		// the application while the agent was working or afterwards. And the
		// composition source itself names the RFC-BPR-0008 coordinator zero
		// times and carries exactly one publication call site - the reviewer's
		// own `begin_publish` branch - so there is no second edge for a
		// backchannel to travel along. The positive direction, that a
		// publication does round-trip through that one path, is
		// `application_review_round_trip_composition_contract`, which pins
		// this same file by digest.
		auto composition = std::ifstream{
			std::filesystem::path{ BPR_COMPOSITION_SOURCE },
			std::ios::binary,
		};
		const auto source = std::string{
			std::istreambuf_iterator<char>{ composition },
			std::istreambuf_iterator<char>{ },
		};
		const auto occurrences
			= [&source](const std::string_view aToken) -> std::size_t {
			auto count = std::size_t{ 0U };
			auto at    = source.find(aToken);
			while (at != std::string::npos) {
				++count;
				at = source.find(aToken, at + aToken.size());
			}
			return count;
		};
		auto edge = contract_terms{ };
		edge.require("composition-source-read", !source.empty());
		edge.require(
			"no-coordinator-named", occurrences("review_mutation_coordinator") == 0U);
		edge.require(
			"one-publish-call-site", occurrences("ReviewMutationHost->publish(") == 1U);
		// One ledger submission, and no submit call site on any other receiver
		// but the setup destination. [[BPR-0689]] composed that host, whose
		// `submit` takes a pull request locator and reaches only its own
		// setup port - configure, authenticate, preview, open, present - and
		// [[BPR-0733]] gave it a second call site, Home's recent list. Neither
		// is an edge to a mutation. Counting every `->submit(` as one read
		// them as backchannels; a submit on any receiver not named here still
		// fails this term.
		edge.require(
			"one-ledger-submit-call-site", occurrences("ReviewLedgerHost->submit(") == 1U);
		edge.require(
			"submit-call-sites-are-ledger-or-setup",
			occurrences("->submit(")
				== occurrences("ReviewLedgerHost->submit(")
					+ occurrences("SetupDestination->submit("));
		edge.require("no-value-submit-call-site", occurrences(".submit(") == 0U);
		edge.require(
			"no-observe-target-call-site",
			occurrences("->observe_target(") == 0U
				&& occurrences(".observe_target(") == 0U);
		// The finding host, the workshop host, and the helper supervisor each
		// reach exactly the ports they were given. None of them is handed the
		// mutation host, and no call site here hands it over.
		edge.require(
			"one-finding-host-construction",
			occurrences("FindingHost = std::make_unique<") == 1U);
		edge.require(
			"one-workshop-host-construction",
			occurrences("WorkshopHost = std::make_unique<") == 1U);
		edge.require(
			"one-helper-construction", occurrences("McpHelper = std::make_unique<") == 1U);
		edge.require(
			"finding-host-delivers",
			source.find("FindingHost->deliver(") != std::string::npos);
		edge.require(
			"mutation-host-named",
			source.find("ReviewMutationHost") != std::string::npos);
		const auto noSecondEdge = report_contract("full-agent-second-edge", edge);
		if (!shutdown(*application) || !exact || !noSecondEdge) {
			std::cerr << "agent-finding-full-agent:" << exact << ':'
					  << noSecondEdge << ':' << source.size() << ':'
					  << mutation.Submissions << ':' << mutation.TargetObservations
					  << ':' << mutation.StoredComments << '\n';
			return EXIT_FAILURE;
		}
		std::cout << "agent-finding-full-agent:pass\n";
		return EXIT_SUCCESS;
	}

	// Every construction cut asserts the same three things about a different
	// construction: the gesture still fired, its downstream effect is zero, and
	// the shared positive marker is false.
	const auto invocation
		= app::detail::github_application_fixture_access::agent_invocation_evidence(
			*application);
	const auto gestureObserved = panel->statistics().StartIntents == 1U
		&& invocation.AgentProcessLauncherInvocations == 1U
		&& invocation.Statistics.ProcessStarts == 1U;
	auto effectsZero = false;
	if (course == "finding-host-construction-cut") {
		effectsZero = !evidence.FindingHostComposed
			&& evidence.FindingStatistics
				== application::agent_finding_host_statistics{ }
			&& evidence.QueuedFindings == 0U && evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original;
	} else if (course == "queue-construction-cut") {
		effectsZero = !evidence.FindingQueueComposed
			&& evidence.QueuedFindings == 0U
			&& evidence.FindingStatistics.Actions == 0U
			&& evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original;
	} else if (course == "workshop-construction-cut") {
		effectsZero = !evidence.CommentWorkshopComposed
			&& evidence.WorkshopStatistics
				== application::agent_comment_workshop_statistics{ }
			&& evidence.WorkshopProposals == 0U
			&& evidence.LocalDraftBody == reviewer_original
			&& evidence.RefusalsPresented == 1U;
	} else if (course == "mcp-construction-cut") {
		effectsZero = !evidence.McpHelperComposed
			&& !evidence.McpLaunch.has_value()
			&& evidence.McpStatistics
				== application::mcp_helper_supervisor_statistics{ }
			&& evidence.FrozenSnapshotWrites == 0U
			&& helperRefusal == application::mcp_helper_refusal::no_helper
			&& !std::filesystem::exists(snapshotParent);
	} else {
		effectsZero = !evidence.SideChatPanelComposed
			&& evidence.SideChatPresentations == 0U
			&& evidence.SideChatIntents == 0U;
	}
	auto contract = contract_terms{ };
	contract.require("gesture-observed", gestureObserved);
	contract.require("cut-effects-zero", effectsZero);
	contract.require("positive-marker-false", !positiveMarker);
	contract.require("no-github-mutation", noGithubMutation);
	const auto passed = report_contract("cut-evidence", contract);
	if (!passed) {
		std::cerr << "agent-finding-cut-evidence:" << gestureObserved << ':'
				  << effectsZero << ':' << positiveMarker << ':'
				  << noGithubMutation << ':' << evidence.QueuedFindings << ':'
				  << evidence.WorkshopProposals << ':'
				  << evidence.RefusalsPresented << ':'
				  << evidence.SideChatPresentations << ':'
				  << evidence.FrozenSnapshotWrites << "\ndraft-body:"
				  << evidence.LocalDraftBody << '\n';
	}
	if (!shutdown(*application) || !passed) {
		return EXIT_FAILURE;
	}
	std::cout << "agent-finding-" << course << ":pass\n";
	return EXIT_SUCCESS;
}
