#include "github_application/fixture_course_support.hpp"
#include "github_application/fixture_uiforetw_observation.hpp"
#include "review_submission_workflow.hpp"
#include "terminal_diagnostics.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/document_tab_host.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/document_tabs.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/appearance_profile.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/epoch_comparison_control.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application/workbench_composition.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAbstractScrollArea>
	#include <QAction>
	#include <QApplication>
	#include <QBrush>
	#include <QByteArray>
	#include <QColor>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QList>
	#include <QListView>
	#include <QModelIndex>
	#include <QPalette>
	#include <QPlainTextEdit>
	#include <QPoint>
	#include <QString>
	#include <QTextCharFormat>
	#include <QTextCursor>
	#include <QTextDocument>
	#include <QTextEdit>
	#include <QTextFormat>
	#include <QTextStream>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <filesystem>
	#include <functional>
	#include <span>
	#include <string_view>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		inline constexpr std::string_view uiforetw_resource_path
			= "UIforETW/Resource.h";

		inline constexpr std::string_view uiforetw_resource_base_blob
			= "7da9aac464237ee5f1e2c9bc821598ff1f02b352";

		inline constexpr std::string_view uiforetw_resource_head_blob
			= "79a81a859f1b281c99e3c624b3263e618d7e1652";

		inline constexpr std::string_view uiforetw_support_path
			= "UIforETW/Support.cpp";

		[[nodiscard]] auto reveal_intraline_pair(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment,
			const bool                      aReport = true) -> bool
		{
			const auto location = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			// [[BPR-0859]]. Each refusal names itself. A reveal that fails
			// leaves the role-paint evidence false without the role check ever
			// running, so an unnamed refusal here reads as a repaint defect.
			if (!location.has_value() || !location->Group.has_value()) {
				if (aReport) {
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_UNLOCATED:found="
						<< (location.has_value() ? 1 : 0) << '\n';
				}
				return false;
			}
			if (!navigate_to_group(aWorkbench, aResult, *location->Group)) {
				if (aReport) {
					bpr::app::detail::terminal_text_stream{ }
						<< "UIFORETW_REVEAL_PAIR_UNSELECTED:groups="
						<< static_cast<qulonglong>(aResult.Alignment.Groups.size())
						<< '\n';
				}
				return false;
			}
			return reveal_intraline_pairs(
				aWorkbench, aResult, *location, *location, aReport);
		}

		/// \brief [[BPR-0859]]. How many intraline marks a stalled check names.
		///
		/// \details Enough to show the revealed pair and its neighbours on both
		/// sides, and short of one line per changed span on an all-changed
		/// screenful.
		inline constexpr std::size_t role_paint_report_marks = 12U;

		/// \brief [[BPR-0859]]. Ticks an intraline reveal may take to settle.
		///
		/// \details The reveal is the product's own scroll: selecting the change
		/// group asks the linked controller to bring the segment on screen, and
		/// the geometry that positions it is measured on the completion lane
		/// rather than inside the gesture. Asserting on the first tick read a
		/// scroll still in flight as a change that never came on screen, which
		/// is how this course failed. The automated run advances the probe about
		/// once every ten milliseconds, so this is roughly two and a half
		/// seconds: far longer than a measured reveal needs, far short of the
		/// forty-second run budget, and bounded so the stall names itself.
		inline constexpr std::size_t uiforetw_intraline_reveal_waits = 256U;

		/// \brief [[BPR-0859]]. Ticks both panes may take to show once Review
		/// is entered.
		///
		/// \details Entering Review is the shared step's, bounded and named
		/// there ([[BPR-0863]]). The panes are this course's own need beyond
		/// the comparison the step waits for, so they keep this bound and name
		/// their own stall rather than expiring the run budget.
		inline constexpr std::size_t uiforetw_review_panes_waits = 512U;

		/// \brief [[BPR-0859]]. Reports what one pane's role-paint check saw.
		///
		/// \details A capped wait that gives up must say what it saw, or its
		/// only evidence is that it did not see what it wanted: BPR-0859 spent
		/// two scouting passes on a stall that printed one word. The accepted
		/// renderings are named with their colour specs, because `QColor ==`
		/// compares the spec as well as the value, and every mark the pane
		/// actually holds is named beside them. A pane holding no marks at all
		/// is a pane this modality never composed, or a target outside the
		/// painted bounds; marks in an unaccepted colour are a repaint or an
		/// expectation defect. The two are indistinguishable without this.
		void report_role_paint(
			QTextStream&                   aStream,
			const QPlainTextEdit&          aPane,
			const char* const              aName,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant)
		{
			const auto describe = [](const QColor& aColor) {
				return aColor.isValid()
					? aColor.name(QColor::HexArgb) + QStringLiteral("/spec")
						+ QString::number(static_cast<int>(aColor.spec()))
					: QStringLiteral("invalid");
			};
			const auto accepted
				= role_renderings(aPane, aExpected, aRowBand, aVariant);
			const auto selections = aPane.extraSelections();
			const auto mark       = [](const QTextEdit::ExtraSelection& aSelection) {
				return !aSelection.format.boolProperty(
						   QTextFormat::FullWidthSelection)
					&& aSelection.cursor.hasSelection();
			};
			const auto marks = static_cast<std::size_t>(
				std::ranges::count_if(selections, mark));
			const auto height = aPane.viewport()->height();
			aStream << "UIFORETW_ROLE_PAINT_PANE:" << aName
					<< " marks=" << static_cast<qulonglong>(marks) << " matched="
					<< static_cast<qulonglong>(role_paint_count(
						   aPane, aExpected, aRowBand, aVariant))
					<< " selections=" << static_cast<qulonglong>(selections.size())
					<< " blocks=" << aPane.document()->blockCount() << " top="
					<< aPane.cursorForPosition(QPoint{ 0, 0 }).blockNumber()
					<< " bottom="
					<< aPane.cursorForPosition(QPoint{ 0, height }).blockNumber()
					<< " viewport=" << aPane.viewport()->width() << 'x' << height
					<< " shown=" << (aPane.isVisible() ? 1 : 0)
					<< " role=" << describe(accepted[0])
					<< " washed=" << describe(accepted[1])
					<< " derived=" << describe(accepted[2])
					<< " band=" << describe(aRowBand) << " highlight="
					<< describe(aPane.palette().color(QPalette::Highlight))
					<< '\n';
			auto reported = std::size_t{ 0U };
			for (const auto& selection : selections) {
				if (!mark(selection) || ++reported > role_paint_report_marks) {
					continue;
				}
				const auto background = selection.format.background().color();
				aStream << "UIFORETW_ROLE_PAINT_MARK:" << aName
						<< " block=" << selection.cursor.blockNumber()
						<< " from=" << selection.cursor.selectionStart()
						<< " to=" << selection.cursor.selectionEnd()
						<< " background=" << describe(background) << " brush="
						<< static_cast<int>(selection.format.background().style())
						<< " underline="
						<< (selection.format.fontUnderline() ? 1 : 0)
						<< " accepted="
						<< (std::ranges::contains(accepted, background) ? 1 : 0)
						<< '\n';
			}
		}

		/// \brief [[BPR-0859]]. Reports both panes' role paint at a stall.
		///
		/// \details The modality is named first. A course that samples panes the
		/// review modality on screen never composed reads a correctly painted
		/// comparison as an unpainted one, which is how BPR-0860, BPR-0823 and
		/// BPR-0858 failed in this same run.
		void report_intraline_roles(
			const qt::diff_workbench&     aWorkbench,
			const qt::appearance_profile& aProfile,
			const std::size_t             aMinimum,
			const char* const             aTerm)
		{
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << aTerm << ":minimum=" << static_cast<qulonglong>(aMinimum)
				   << " one_pane=" << (aWorkbench.one_pane_enabled() ? 1 : 0)
				   << " variant=" << static_cast<int>(aProfile.variant()) << '\n';
			report_role_paint(
				stream,
				*aWorkbench.left_pane(),
				"left",
				aProfile.color(
					core::appearance_role::diff_intraline_removed_background),
				aProfile.color(core::appearance_role::diff_removed_background),
				aProfile.variant());
			report_role_paint(
				stream,
				*aWorkbench.right_pane(),
				"right",
				aProfile.color(
					core::appearance_role::diff_intraline_added_background),
				aProfile.color(core::appearance_role::diff_added_background),
				aProfile.variant());
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::
		exercise_native_appearance_selected_diff(
			github_application& aApplication)
			-> detail::native_appearance_selected_diff_probe_result
	{
		using result               = detail::native_appearance_selected_diff_probe_result;
		using reason               = detail::native_appearance_selected_diff_reason;
		using state_type           = native_appearance_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return result::refused;
		}
		if (!implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			implementation->NativeAppearanceSelectedDiffProbe.emplace();
		}
		auto&      probe   = *implementation->NativeAppearanceSelectedDiffProbe;
		const auto pending = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			return result::pending;
		};
		const auto refuse = [&probe](const reason aReason) {
			probe.Evidence.Reason = aReason;
			probe.Phase           = state_type::phase::refused;
			return result::refused;
		};

		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		if (!probe.Evidence.FixtureDiffHelperUnused) {
			return refuse(reason::fixture_diff_helper_invoked);
		}
		if (probe.Phase == state_type::phase::refused) {
			return result::refused;
		}
		if (probe.Phase == state_type::phase::ready) {
			return result::ready;
		}
		if (implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return refuse(reason::composition_unavailable);
		}

		auto* const host      = implementation->WorkbenchHost.get();
		auto* const window    = implementation->Window.get();
		auto* const workbench = window->comparison_surface();
		auto* const filesView = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto& fixture   = implementation->Fixture->evidence();

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				return pending(reason::awaiting_workbench_observation);
			}
			if (snapshot->Files.size() != 1U
				|| snapshot->Files.front().Path != "review.cpp"
				|| filesView == nullptr || filesView->model() == nullptr
				|| filesView->model()->rowCount() != 1) {
				return refuse(reason::file_projection_mismatch);
			}

			const auto& handoff     = *implementation->LastAcceptedHandoff;
			const auto& epoch       = implementation->EpochHost->state();
			probe.Authority         = workbench_authority(handoff);
			probe.File              = core::file_identity_of("review.cpp");
			const auto context      = implementation->EpochHost->context(probe.Authority);
			const auto expectedBase = core::parse_git_commit_oid(fixture.BaseOid);
			const auto expectedHead = core::parse_git_commit_oid(fixture.HeadOneOid);
			const auto expectedLeftBlob
				= core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto expectedRightBlob
				= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			const auto expectedOptions = core::diff_options{ };
			const auto expectedDigest  = core::options_digest(expectedOptions);

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& context.has_value() && epoch.Authority.has_value()
				&& context->ApplicationSession == handoff.ApplicationSession
				&& context->Authority == *epoch.Authority
				&& probe.Authority == workbench_authority(handoff);
			probe.Evidence.ExactRevisions
				= expectedBase.has_value() && expectedHead.has_value()
				&& epoch.AcceptedRevision.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& epoch.AcceptedRevision->Base == *expectedBase
				&& epoch.AcceptedRevision->Head == *expectedHead
				&& epoch.AcceptedComparison->Left.Commit == *expectedBase
				&& epoch.AcceptedComparison->Right.Commit == *expectedHead
				&& context.has_value()
				&& context->Comparison == *epoch.AcceptedComparison;
			const auto actionable    = host->actionable();
			probe.Evidence.ExactFile = probe.File.valid()
				&& actionable.size() == 1U && actionable.front() == probe.File;
			probe.Evidence.ExactBlobs
				= expectedLeftBlob.has_value() && expectedRightBlob.has_value();
			probe.Evidence.ExactOptions
				= context.has_value() && expectedDigest.has_value()
				&& context->Options == expectedOptions
				&& context->OptionsDigest == expectedDigest
				&& context->Algorithm
					== core::supported_diff_algorithm_revision;
			if (!probe.Evidence.ExactAuthority) {
				return refuse(reason::fixture_authority_mismatch);
			}
			if (!probe.Evidence.ExactRevisions || !probe.Evidence.ExactBlobs
				|| !probe.Evidence.ExactOptions) {
				return refuse(reason::fixture_material_mismatch);
			}
			if (!probe.Evidence.ExactFile) {
				return refuse(reason::file_projection_mismatch);
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			filesView->setCurrentIndex(filesView->model()->index(0, 0));
			probe.Phase = state_type::phase::awaiting_selection_projection;
			return pending(reason::awaiting_selection_projection);
		}

		if (probe.Phase
			== state_type::phase::awaiting_selection_projection) {
			const auto statistics = window->statistics();
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return refuse(reason::selected_diff_failed);
			}
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.SelectionGesture = true;
					return refuse(reason::selection_wire_unavailable);
				}
				return pending(reason::awaiting_selection_projection);
			}
			probe.Evidence.SelectionGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			probe.Evidence.SelectionIntent
				= implementation->WorkbenchIntentDispatches
					== probe.IntentDispatches + 1U
				&& host->selected() == std::optional{ probe.File };
			if (!probe.Evidence.SelectionGesture
				|| !probe.Evidence.SelectionIntent) {
				return refuse(reason::file_projection_mismatch);
			}
			probe.Phase = state_type::phase::awaiting_diff;
			return pending(reason::awaiting_selected_diff_publication);
		}

		if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
			return refuse(reason::selected_diff_failed);
		}
		if (implementation->SelectedDiffPublications
			== probe.SelectedDiffPublications) {
			return pending(reason::awaiting_selected_diff_publication);
		}
		if (implementation->SelectedDiffBegins != probe.SelectedDiffBegins + 1U
			|| implementation->SelectedDiffPublications
				!= probe.SelectedDiffPublications + 1U
			|| !implementation->LastSelectedDiff.has_value()) {
			return refuse(reason::selected_diff_mismatch);
		}

		const auto& publication = *implementation->LastSelectedDiff;
		const auto& request     = publication.Request;
		const auto& key         = publication.Key;
		const auto& epoch       = implementation->EpochHost->state();
		const auto  expectedLeftBlob
			= core::parse_git_blob_oid(fixture.BaseBlobOid);
		const auto expectedRightBlob
			= core::parse_git_blob_oid(fixture.HeadOneBlobOid);
		const auto expectedDigest = core::options_digest(core::diff_options{ });
		probe.Evidence.ExactAuthority
			= probe.Evidence.ExactAuthority
			&& request.WorkbenchAuthority == probe.Authority
			&& epoch.Authority.has_value() && key.Authority == *epoch.Authority
			&& key.Repository == epoch.Authority->Repository
			&& key.PullRequest == epoch.Authority->PullRequest;
		probe.Evidence.ExactRevisions
			= probe.Evidence.ExactRevisions
			&& epoch.AcceptedComparison.has_value()
			&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
			&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity;
		probe.Evidence.ExactFile = probe.Evidence.ExactFile
			&& request.Context.Generation == host->generation()
			&& request.File.File == probe.File
			&& request.File.PresentationPath == "review.cpp"
			&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
			&& !request.File.PreviousPath.has_value()
			&& request.File.Change
				== application::workbench_file_change_kind::modified
			&& key.File == probe.File;
		probe.Evidence.ExactBlobs = probe.Evidence.ExactBlobs
			&& expectedLeftBlob.has_value() && expectedRightBlob.has_value()
			&& key.LeftBlob == *expectedLeftBlob
			&& key.RightBlob == *expectedRightBlob;
		probe.Evidence.ExactOptions = probe.Evidence.ExactOptions
			&& expectedDigest.has_value()
			&& request.Diff.Options == core::diff_options{ }
			&& request.Diff.OptionsDigest == *expectedDigest
			&& key.Options == *expectedDigest
			&& key.Algorithm == core::supported_diff_algorithm_revision;
		if (!probe.Evidence.ExactAuthority || !probe.Evidence.ExactRevisions
			|| !probe.Evidence.ExactFile || !probe.Evidence.ExactBlobs
			|| !probe.Evidence.ExactOptions) {
			return refuse(reason::selected_diff_mismatch);
		}
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !workbench->selection().has_value()) {
			return pending(reason::awaiting_diff_presentation);
		}
		probe.Evidence.PresentationReady
			= workbench->current_result()->Alignment.Key == key
			&& workbench->selection()->Comparison
				== workbench->current_result()->Alignment.Digest
			&& workbench->status_text().startsWith(QStringLiteral("Diff ready"));
		if (!probe.Evidence.PresentationReady) {
			return refuse(reason::selected_diff_mismatch);
		}

		probe.Evidence.Reason = reason::none;
		probe.Phase           = state_type::phase::ready;
		return result::ready;
	}

	auto detail::github_application_fixture_access::
		native_appearance_selected_diff_evidence(
			const github_application& aApplication) noexcept
		-> detail::native_appearance_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return detail::native_appearance_selected_diff_probe_evidence{
				.Reason = detail::native_appearance_selected_diff_reason::
					composition_unavailable,
			};
		}
		if (implementation->NativeAppearanceSelectedDiffProbe.has_value()) {
			return implementation->NativeAppearanceSelectedDiffProbe->Evidence;
		}
		return detail::native_appearance_selected_diff_probe_evidence{
			.Reason = detail::native_appearance_selected_diff_reason::
				awaiting_workbench_observation,
			.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U,
		};
	}

	auto detail::github_application_fixture_access::exercise_uiforetw_selected_diff(
		github_application& aApplication)
		-> detail::uiforetw_selected_diff_probe_result
	{
		using result               = detail::uiforetw_selected_diff_probe_result;
		using state_type           = uiforetw_selected_diff_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Host == nullptr
			|| implementation->EpochHost == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->Window == nullptr
			|| implementation->AppearanceProfile == nullptr) {
			return result::failed;
		}
		if (!implementation->UIforEtwSelectedDiffProbe.has_value()) {
			implementation->UIforEtwSelectedDiffProbe.emplace();
		}
		auto&      probe = *implementation->UIforEtwSelectedDiffProbe;
		const auto fail  = [&probe, implementation] {
			const auto* const surface = implementation->Window->comparison_surface();
			bpr::app::detail::terminal_text_stream{ }
				<< "UIFORETW_PROBE_FAILURE:phase=" << static_cast<int>(probe.Phase)
				<< " waits=" << static_cast<qulonglong>(probe.Waits)
				<< " reveal_waits=" << static_cast<qulonglong>(probe.RevealWaits)
				<< " next=" << probe.Evidence.NextChangeNavigation
				<< " previous=" << probe.Evidence.PreviousChangeNavigation
				<< " rc_revealed=" << probe.RcRevealed
				<< " dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
				<< " baseline_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches)
				<< " selection=" << (surface && surface->selection().has_value())
				<< " geometry_pending=" << (surface && surface->geometry_pending())
				<< " pending_navigation=" << implementation->PendingReviewNavigation.has_value()
				<< " tab_pending=" << (implementation->TabHost && implementation->TabHost->state().Pending.has_value())
				<< " tab_queued=" << implementation->TabAdvanceQueued << '\n';
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		auto* const window             = implementation->Window.get();
		auto* const workbench          = window->comparison_surface();
		auto* const view               = window->files_drawer() != nullptr
			? window->files_drawer()->contents_view()
			: nullptr;
		const auto  fixture            = implementation->Fixture->evidence();
		probe.Evidence.ExactCommitPair = fixture.BaseOid == uiforetw_base_oid
			&& fixture.HeadOneOid == uiforetw_head_oid
			&& fixture.HeadTwoOid.empty();

		// [[BPR-1036]]. Every phase that waits for an effect it does not
		// control waits through the shared bounded wait ([[BPR-0864]],
		// [[BPR-1037]]): five seconds with a floor of sixteen calls, then a
		// term naming the condition, the inputs it depends on and the
		// modality facts. `advance` is the only thing that restarts it. The
		// waits this probe already bounded by ticks and named -- the panes,
		// the reveals and the role paint -- keep their bounds. The focused
		// layout awaits asynchronous preference completion through this helper.
		const auto wait = [&](const char* const aTerm, const auto& aFacts) {
			return keep_waiting(aApplication, probe.Wait, aTerm, aFacts) ? result::pending : fail();
		};
		const auto advance = [&probe](const state_type::phase aPhase) {
			enter_phase(probe.Phase, probe.Wait, aPhase);
			return result::pending;
		};
		const auto present = [](const bool aPresent) {
			return aPresent ? "present" : "absent";
		};
		const auto truth = [](const bool aValue) {
			return aValue ? "true" : "false";
		};
		const auto text = [](const std::string_view aValue) {
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		};
		const auto blobText = [](const std::optional<core::git_blob_oid>& aOid) {
			if (!aOid.has_value()) {
				return QStringLiteral("absent");
			}
			const auto hex = core::canonical_hex(*aOid);
			return QString::fromLatin1(hex.view().data(), static_cast<qsizetype>(hex.view().size()));
		};
		// What a file's selection, its publication and the comparison showing
		// it depend on. The baselines the gesture is counted against print only
		// in the phase that made that gesture.
		const auto diffFacts = [&](const std::string_view aPath, const std::string_view aBaseBlob, const std::string_view aHeadBlob) {
			return [&, aPath, aBaseBlob, aHeadBlob](QTextStream& aStream) {
				const auto        work    = diff_work_evidence(aApplication);
				const auto&       last    = implementation->LastSelectedDiff;
				const auto* const current = workbench != nullptr ? workbench->current_result().get() : nullptr;
				aStream << "path=" << text(aPath)
						<< " file_selections=" << static_cast<qulonglong>(window->statistics().FileSelections);
				if (probe.Phase == state_type::phase::awaiting_resource_diff
					|| probe.Phase == state_type::phase::awaiting_rc_diff) {
					aStream << " expected_file_selections=" << static_cast<qulonglong>(probe.WindowStatistics.FileSelections) + 1U;
				} else {
					aStream << " intent_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchIntentDispatches)
							<< " expected_intent_dispatches=" << static_cast<qulonglong>(probe.IntentDispatches) + 1U;
				}
				aStream << " current_row=" << (view != nullptr ? view->currentIndex().row() : -1)
						<< " selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
						<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
						<< " expected_selected_diff_publications=" << static_cast<qulonglong>(probe.SelectedDiffPublications) + 1U
						<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
						<< " last_selected_diff="
						<< (last.has_value() ? QString::fromStdString(last->Request.File.PresentationPath) : QStringLiteral("absent"))
						<< " last_left_blob=" << (last.has_value() ? blobText(last->Key.LeftBlob) : QStringLiteral("absent"))
						<< " expected_left_blob=" << text(aBaseBlob)
						<< " last_right_blob=" << (last.has_value() ? blobText(last->Key.RightBlob) : QStringLiteral("absent"))
						<< " expected_right_blob=" << text(aHeadBlob)
						<< " comparison_result=" << present(current != nullptr)
						<< " comparison_current=" << truth(current != nullptr && last.has_value() && current->Alignment.Key == last->Key)
						<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
						<< " diff_pending=" << truth(work.Pending)
						<< " diff_quiescent=" << truth(work.Quiescent)
						<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
			};
		};
		const auto resourceFacts = diffFacts(uiforetw_resource_path, uiforetw_resource_base_blob, uiforetw_resource_head_blob);
		const auto rcFacts       = diffFacts(uiforetw_rc_path, uiforetw_rc_base_blob, uiforetw_rc_head_blob);
		// The support file's blobs are not this course's to pin: it is the
		// file next-file lands on, and only its path is asserted.
		const auto supportFacts = diffFacts(uiforetw_support_path, "unpinned", "unpinned");

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			const auto  resource = drawer_row(*window, uiforetw_resource_path);
			const auto  rc       = drawer_row(*window, uiforetw_rc_path);
			if (!snapshot.has_value()
				|| implementation->WorkbenchHost->outstanding_requests() != 0U
				|| !implementation->WorkbenchHost->state().Navigation.has_value()
				|| !resource.isValid() || !rc.isValid() || view == nullptr) {
				// [[BPR-1036]]. The accepted pull request reaches the workbench
				// host as an observation, and its files reach the drawer rows
				// both gestures below select.
				return wait("UIFORETW_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " navigation=" << present(implementation->WorkbenchHost->state().Navigation.has_value())
							<< " actionable=" << static_cast<qulonglong>(implementation->WorkbenchHost->actionable().size())
							<< " files_view=" << present(view != nullptr)
							<< " drawer_rows=" << (view != nullptr && view->model() != nullptr ? view->model()->rowCount() : 0)
							<< " resource_row=" << present(resource.isValid())
							<< " rc_row=" << present(rc.isValid());
				});
			}
			if (!probe.Evidence.ExactCommitPair) {
				return fail();
			}
			window->resize(1'280, 1'000);
			QApplication::processEvents();
			// [[BPR-0859]]. Leave Overview before reading the comparison.
			// Since [[BPR-0771]] this course names its pull request in the Home
			// bar, so the window opens on [[BPR-0661]]'s default opening
			// modality, Overview, which shows no comparison at all. The panes
			// still hold the diff's text, so every marker up to the reveal
			// passed while the surface carrying them was hidden: unmeasured,
			// never scrolled to the selected change, and therefore never
			// painting an intraline mark where this course looked for one. A
			// reviewer who wants to read a file's changes leaves Overview
			// first, through the strip; the course states Review and enters it
			// through the step every course shares ([[BPR-0863]]).
			probe.Waits = 0U;
			return advance(state_type::phase::awaiting_review_modality);
		}

		if (probe.Phase == state_type::phase::awaiting_review_modality) {
			const auto modality = enter_course_modality(aApplication, probe.Modality);
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			// The comparison is shown; this course reads both of its panes.
			// [[BPR-1287]]. With no file chosen yet the comparison shows its
			// direction to open one in the panes' place; the panes return
			// with the file this course selects next.
			auto* const left  = workbench == nullptr ? nullptr
													 : workbench->left_pane();
			auto* const right = workbench == nullptr ? nullptr
													 : workbench->right_pane();
			const auto  idle  = workbench != nullptr && workbench->idle() && workbench->idle_page()->isVisible();
			if (left == nullptr || right == nullptr || (!idle && (!left->isVisible() || !right->isVisible()))) {
				if (++probe.Waits <= uiforetw_review_panes_waits) {
					return result::pending;
				}
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_REVIEW_PANES_HIDDEN:left_shown="
					<< (left != nullptr && left->isVisible())
					<< " right_shown=" << (right != nullptr && right->isVisible())
					<< ' ' << course_modality_facts(aApplication) << '\n';
				return fail();
			}
			// Taken here, after the modality has settled, so the file-selection
			// gesture below is measured against a window that is not still
			// changing what it shows.
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			const auto resourceRow     = drawer_row(*window, uiforetw_resource_path);
			if (!resourceRow.isValid() || view == nullptr) {
				return fail();
			}
			view->setCurrentIndex(resourceRow);
			return advance(state_type::phase::awaiting_resource_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_resource_diff) {
			probe.Evidence.ResourceGesture
				= window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				// [[BPR-1036]]. Bounded whether or not the drawer's selection
				// registered: a gesture that never lands is named rather than
				// waited on until the run budget expires.
				return wait(
					probe.Evidence.ResourceGesture ? "UIFORETW_RESOURCE_PUBLICATION_STALLED"
												   : "UIFORETW_RESOURCE_SELECTION_STALLED",
					resourceFacts);
			}
			const auto resourcePublication
				= implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!resourcePublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return fail();
			}
			probe.Evidence.ResourcePublication = true;
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return wait("UIFORETW_RESOURCE_BLOBS_STALLED", resourceFacts);
			}
			// [[BPR-0859]]. Reset once, on the tick the blobs first match.
			// Resetting on every tick made the role-paint cap below
			// unreachable: the count could never pass and never give up.
			if (!probe.Evidence.ResourceExactBlobs) {
				probe.Evidence.ResourceExactBlobs = true;
				probe.Waits                       = 0U;
			}
			const auto& computed                 = *workbench->current_result();
			probe.Evidence.ResourceIntralinePair = find_intraline_pair(
													   computed,
													   "_APS_NEXT_RESOURCE_VALUE        136",
													   "_APS_NEXT_RESOURCE_VALUE        137")
													   .has_value();
			if (!probe.Evidence.ResourceGesture
				|| !probe.Evidence.ResourcePublication
				|| !probe.Evidence.ResourceExactBlobs
				|| !probe.Evidence.ResourceIntralinePair) {
				return fail();
			}
			// [[BPR-0859]]. The reveal is a condition this stage waits for, not
			// one it controls: the scroll that brings the selected change on
			// screen is positioned from geometry measured on the completion
			// lane. One tick cannot tell "not yet" from "wrong".
			if (!probe.ResourceRevealed) {
				if (!reveal_intraline_pair(
						*workbench,
						computed,
						"_APS_NEXT_RESOURCE_VALUE        136",
						"_APS_NEXT_RESOURCE_VALUE        137",
						probe.RevealWaits >= uiforetw_intraline_reveal_waits)) {
					if (++probe.RevealWaits > uiforetw_intraline_reveal_waits) {
						return fail();
					}
					return result::pending;
				}
				probe.ResourceRevealed = true;
				probe.RevealWaits      = 0U;
				probe.Waits            = 0U;
			}
			probe.Evidence.ResourceRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 1U);
			if (!probe.Evidence.ResourceRolePaint) {
				if (++probe.Waits > 64U) {
					report_intraline_roles(
						*workbench,
						*implementation->AppearanceProfile,
						1U,
						"UIFORETW_RESOURCE_ROLE_PAINT_STALLED");
					return fail();
				}
				return result::pending;
			}
			auto* const nextFile = window->next_file_action();
			if (nextFile == nullptr || !nextFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			nextFile->trigger();
			return advance(state_type::phase::awaiting_support_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_support_diff) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait("UIFORETW_SUPPORT_PUBLICATION_STALLED", supportFacts);
			}
			const auto exactPublication
				= implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& implementation->LastSelectedDiff.has_value()
				&& implementation->LastSelectedDiff->Request.File.PresentationPath
					== uiforetw_support_path;
			if (!exactPublication) {
				return fail();
			}
			const auto exactSupport = workbench->current_result() != nullptr
				&& workbench->current_result()->Alignment.Key
					== implementation->LastSelectedDiff->Key;
			if (!exactSupport) {
				return wait("UIFORETW_SUPPORT_RESULT_STALLED", supportFacts);
			}
			probe.Evidence.NextFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			auto* const previousFile = window->previous_file_action();
			if (!probe.Evidence.NextFileNavigation || previousFile == nullptr
				|| !previousFile->isEnabled()) {
				return fail();
			}
			probe.IntentDispatches = implementation->WorkbenchIntentDispatches;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			previousFile->trigger();
			return advance(state_type::phase::awaiting_resource_return);
		}

		if (probe.Phase == state_type::phase::awaiting_resource_return) {
			if (implementation->SelectedDiffFailures != probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait("UIFORETW_RESOURCE_RETURN_PUBLICATION_STALLED", resourceFacts);
			}
			if (implementation->SelectedDiffPublications
					!= probe.SelectedDiffPublications + 1U
				|| !implementation->LastSelectedDiff.has_value()
				|| implementation->LastSelectedDiff->Request.File.PresentationPath
					!= uiforetw_resource_path) {
				return fail();
			}
			if (!selected_diff_matches(
					*implementation,
					uiforetw_resource_path,
					uiforetw_resource_base_blob,
					uiforetw_resource_head_blob)) {
				return wait("UIFORETW_RESOURCE_RETURN_BLOBS_STALLED", resourceFacts);
			}
			probe.Evidence.PreviousFileNavigation
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			const auto rc = drawer_row(*window, uiforetw_rc_path);
			if (!probe.Evidence.PreviousFileNavigation || !rc.isValid()
				|| view == nullptr) {
				return fail();
			}
			probe.WindowStatistics = window->statistics();
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			probe.Waits                = 0U;
			view->setCurrentIndex(rc);
			return advance(state_type::phase::awaiting_rc_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_rc_diff) {
			probe.Evidence.RcGesture = window->statistics().FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications) {
				return wait(
					probe.Evidence.RcGesture ? "UIFORETW_RC_PUBLICATION_STALLED"
											 : "UIFORETW_RC_SELECTION_STALLED",
					rcFacts);
			}
			const auto rcPublication = implementation->SelectedDiffPublications
				== probe.SelectedDiffPublications + 1U;
			if (!rcPublication
				|| !selected_diff_publication_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				return fail();
			}
			probe.Evidence.RcPublication = true;
			if (!selected_diff_matches(
					*implementation,
					uiforetw_rc_path,
					uiforetw_rc_base_blob,
					uiforetw_rc_head_blob)) {
				return wait("UIFORETW_RC_BLOBS_STALLED", rcFacts);
			}
			if (!probe.Evidence.RcExactBlobs) {
				probe.Evidence.RcExactBlobs = true;
				probe.Waits                 = 0U;
			}
			const auto& computed = *workbench->current_result();
			const auto  pairOne  = find_intraline_pair(
				computed, "WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
			const auto pairTwo = find_intraline_pair(
				computed, "WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
			probe.Evidence.RcIntralinePairs
				= pairOne.has_value() && pairTwo.has_value();
			if (!probe.Evidence.RcIntralinePairs || !pairOne->Group.has_value()) {
				return fail();
			}
			if (!probe.ChangeOrigin) {
				auto* const nextChange = window->next_change_action();
				if (!nextChange || !window->previous_change_action()
					|| !nextChange->isEnabled() || !workbench->selection()) {
					return fail();
				}
				probe.ChangeOrigin     = workbench->selection();
				probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
				nextChange->trigger();
				return result::pending;
			}
			// [[BPR-1527]]. A navigation press may be retained until its tab
			// restoration completes. Issue each gesture once, then observe its
			// exact dispatch and semantic result on the normal completion lane.
			if (!probe.Evidence.NextChangeNavigation) {
				if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches) {
					return wait("UIFORETW_RC_NEXT_CHANGE_STALLED", rcFacts);
				}
				auto* const previousChange = window->previous_change_action();
				probe.Evidence.NextChangeNavigation
					= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U
					&& workbench->selection() && workbench->selection()->Group != probe.ChangeOrigin->Group
					&& previousChange && previousChange->isEnabled();
				if (!probe.Evidence.NextChangeNavigation) {
					return fail();
				}
				previousChange->trigger();
				return result::pending;
			}
			if (!probe.Evidence.PreviousChangeNavigation) {
				if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U) {
					return wait("UIFORETW_RC_PREVIOUS_CHANGE_STALLED", rcFacts);
				}
				probe.Evidence.PreviousChangeNavigation
					= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 2U
					&& workbench->selection() == probe.ChangeOrigin;
				if (!probe.Evidence.PreviousChangeNavigation) {
					return fail();
				}
			}
			if (!workbench->selection().has_value()
				|| workbench->selection()->Group != pairOne->Group) {
				// [[BPR-1036]]. The walk to the first pair's group is bounded by
				// the groups there are, and either end of it names itself with
				// where it stands: how far it walked, and which group is selected
				// against the one it is walking to.
				const auto walkFailed = [&](const char* const aTerm) {
					const auto groupIndex = [&computed](const auto& aGroup) {
						const auto found = std::ranges::find(computed.Alignment.Groups, aGroup, &core::change_group::Id);
						return found == computed.Alignment.Groups.end()
							? qlonglong{ -1 }
							: static_cast<qlonglong>(std::distance(computed.Alignment.Groups.begin(), found));
					};
					const auto selection = workbench->selection();
					bpr::app::detail::terminal_text_stream{ }
						<< aTerm << ":walked=" << static_cast<qulonglong>(probe.Waits)
						<< " groups=" << static_cast<qulonglong>(computed.Alignment.Groups.size())
						<< " target_group=" << groupIndex(*pairOne->Group)
						<< " selection=" << present(selection.has_value())
						<< " selected_group="
						<< (selection.has_value() && selection->Group.has_value() ? groupIndex(*selection->Group) : qlonglong{ -1 })
						<< " next_change_enabled=" << truth(window->next_change_action() != nullptr && window->next_change_action()->isEnabled())
						<< ' ' << course_modality_facts(aApplication) << '\n';
					return fail();
				};
				if (probe.Waits++ > computed.Alignment.Groups.size()) {
					return walkFailed("UIFORETW_RC_CHANGE_WALK_EXHAUSTED");
				}
				if (!workbench->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return walkFailed("UIFORETW_RC_CHANGE_WALK_REFUSED");
				}
				return result::pending;
			}
			if (!probe.Evidence.RcGesture || !probe.Evidence.RcPublication
				|| !probe.Evidence.RcExactBlobs
				|| !probe.Evidence.RcIntralinePairs) {
				return fail();
			}
			// [[BPR-0859]]. As on the resource file: the reveal is waited for
			// and bounded, and its wait is not the paint's wait.
			if (!probe.RcRevealed) {
				if (!reveal_intraline_pair(
						*workbench,
						computed,
						"WS_TABSTOP,464,28,67,10",
						"WS_TABSTOP,464,24,67,10",
						probe.RevealWaits >= uiforetw_intraline_reveal_waits)) {
					if (++probe.RevealWaits > uiforetw_intraline_reveal_waits) {
						return fail();
					}
					return result::pending;
				}
				probe.RcRevealed  = true;
				probe.RevealWaits = 0U;
				probe.Waits       = 0U;
			}
			probe.Evidence.RcRolePaint = intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U);
			if (!probe.Evidence.RcRolePaint) {
				if (++probe.Waits > 64U) {
					report_intraline_roles(
						*workbench,
						*implementation->AppearanceProfile,
						2U,
						"UIFORETW_RC_ROLE_PAINT_STALLED");
					return fail();
				}
				return result::pending;
			}
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			const auto semantic = std::array{
				probe.Evidence.ExactCommitPair,
				probe.Evidence.ResourceGesture,
				probe.Evidence.ResourceExactBlobs,
				probe.Evidence.ResourceIntralinePair,
				probe.Evidence.ResourceRolePaint,
				probe.Evidence.ResourcePublication,
				probe.Evidence.RcGesture,
				probe.Evidence.RcExactBlobs,
				probe.Evidence.RcIntralinePairs,
				probe.Evidence.RcRolePaint,
				probe.Evidence.RcPublication,
				probe.Evidence.PreviousFileNavigation,
				probe.Evidence.NextFileNavigation,
				probe.Evidence.PreviousChangeNavigation,
				probe.Evidence.NextChangeNavigation,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(semantic, std::identity{ })) {
				return fail();
			}
			auto* const files = window->files_drawer_toggle_action();
			auto* const conversations
				= window->conversations_drawer_toggle_action();
			auto* const repositories = window->repositories_drawer_toggle_action();
			auto* const details      = window->review_details_toggle_action();
			if (files == nullptr || conversations == nullptr || details == nullptr
				|| repositories == nullptr || !files->isEnabled()
				|| !conversations->isEnabled() || !repositories->isEnabled()) {
				return fail();
			}
			// [[BPR-0651]]. The third drawer collapses to its rail like the
			// other two, so the focused layout is the comparison alone.
			//
			// [[BPR-0859]]. Each toggle proves its wire whatever the drawer
			// starts as. Review expands the files drawer and leaves the other
			// two on their rails, so an evidence bit set only inside
			// `if (isChecked())` asserted the opening modality rather than the
			// toggle, and stayed false for a drawer that was already down. A
			// drawer on its rail is now expanded and collapsed again -- the two
			// presses a reviewer would make -- so every press still travels the
			// intent wire and the layout still ends focused.
			const auto press = [implementation](QAction* const aAction) {
				const auto dispatches = implementation->WorkbenchIntentDispatches;
				const auto checked    = aAction->isChecked();
				aAction->trigger();
				return implementation->WorkbenchIntentDispatches == dispatches + 1U
					&& aAction->isChecked() != checked;
			};
			const auto collapse = [&press](QAction* const aAction) {
				if (!aAction->isChecked() && !press(aAction)) {
					return false;
				}
				return press(aAction) && !aAction->isChecked();
			};
			probe.Evidence.RepositoriesDrawerToggle  = collapse(repositories);
			probe.Evidence.FilesDrawerToggle         = collapse(files);
			probe.Evidence.ConversationsDrawerToggle = collapse(conversations);
			if (details->isChecked()) {
				details->trigger();
			}
			// [[BPR-1334]]. Line details is on by default and is not a review
			// detail; the focused review hides it through View > Line Details,
			// as a reader giving the panes all the room would. The comparison's
			// height floors measure that focused layout.
			if (auto* const lineDetails = window->line_details_action(); lineDetails == nullptr) {
				return fail();
			} else if (lineDetails->isChecked()) {
				lineDetails->trigger();
			}
			if (!probe.Evidence.FilesDrawerToggle
				|| !probe.Evidence.ConversationsDrawerToggle
				|| !probe.Evidence.RepositoriesDrawerToggle) {
				bpr::app::detail::terminal_text_stream{ }
					<< "UIFORETW_DRAWER_TOGGLE_REFUSED:files="
					<< probe.Evidence.FilesDrawerToggle << " conversations="
					<< probe.Evidence.ConversationsDrawerToggle
					<< " repositories="
					<< probe.Evidence.RepositoriesDrawerToggle
					<< " files_checked=" << files->isChecked()
					<< " conversations_checked=" << conversations->isChecked()
					<< " repositories_checked=" << repositories->isChecked()
					<< '\n';
				return fail();
			}
			probe.Waits = 0U;
			return advance(state_type::phase::awaiting_focused_layout);
		}

		if (probe.Phase == state_type::phase::awaiting_focused_layout) {
			auto* const detail = workbench->detail_pane();
			// [[BPR-0646]]. A collapsed drawer is its rail, so focused means
			// both drawers are down to their rails; there is no review header
			// above the comparison any more.
			const auto focused = window->files_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& window->conversations_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& window->repositories_drawer()->width()
					<= qt::workbench_drawer_rail::width_pixels
				&& !window->files_drawer_toggle_action()->isChecked()
				&& !window->repositories_drawer_toggle_action()->isChecked()
				&& !window->conversations_drawer_toggle_action()->isChecked()
				&& !window->review_details_toggle_action()->isChecked()
				&& !window->epoch_selectors()->details_visible()
				// [[BPR-1334]]. Hidden through View > Line Details above.
				&& !window->line_details_action()->isChecked()
				&& detail != nullptr && !detail->isVisible();
			if (!focused) {
				++probe.Waits;
				// [[BPR-2087]]. The accepted layout arrives after its preference save.
				return wait("UIFORETW_FOCUSED_LAYOUT_STALLED", [&](QTextStream& aStream) {
					aStream << "rail="
							<< qt::workbench_drawer_rail::width_pixels
							<< " files=" << window->files_drawer()->width()
							<< " conversations=" << window->conversations_drawer()->width()
							<< " repositories=" << window->repositories_drawer()->width()
							<< " files_checked="
							<< window->files_drawer_toggle_action()->isChecked()
							<< " repositories_checked="
							<< window->repositories_drawer_toggle_action()->isChecked()
							<< " conversations_checked="
							<< window->conversations_drawer_toggle_action()->isChecked()
							<< " details_checked="
							<< window->review_details_toggle_action()->isChecked()
							<< " selector_details="
							<< window->epoch_selectors()->details_visible()
							<< " line_details_checked="
							<< window->line_details_action()->isChecked()
							<< " detail_shown="
							<< (detail != nullptr && detail->isVisible());
				});
			}
			probe.Evidence.FocusedReviewLayout = true;
			probe.Phase                        = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::uiforetw_selected_diff_evidence(
		const github_application& aApplication) noexcept
		-> detail::uiforetw_selected_diff_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->UIforEtwSelectedDiffProbe.has_value()
			? implementation->UIforEtwSelectedDiffProbe->Evidence
			: detail::uiforetw_selected_diff_probe_evidence{ };
	}
#endif
}
