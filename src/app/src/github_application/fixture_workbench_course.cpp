#include "github_application/fixture_course_support.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application/workbench_composition.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include <QAbstractItemModel>
	#include <QAbstractItemView>
	#include <QAction>
	#include <QDesktopServices>
	#include <QList>
	#include <QListView>
	#include <QModelIndex>
	#include <QSplitter>
	#include <QSplitterHandle>
	#include <QString>
	#include <QTextStream>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <cstddef>
	#include <filesystem>
	#include <functional>
	#include <iterator>
	#include <memory>
	#include <optional>
	#include <span>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		[[nodiscard]] auto workbench_probe_invalidation(
			const application::accepted_pull_request_handoff& aHandoff,
			const core::pull_request_id                       aPullRequest)
			-> application::pull_request_authority_invalidation
		{
			return {
				.ApplicationSession = aHandoff.ApplicationSession,
				.Profile            = aHandoff.Profile,
				.ProfileRevision    = aHandoff.ProfileRevision,
				.Host               = aHandoff.Host,
				.HostAuthority      = aHandoff.HostAuthority,
				.Account            = aHandoff.Account,
				.Viewer             = aHandoff.Viewer,
				.Repository         = aHandoff.Repository,
				.PullRequest        = aPullRequest,
				.IdentityGeneration = aHandoff.IdentityGeneration,
			};
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::exercise_workbench_composition(
		github_application& aApplication)
		-> detail::workbench_composition_probe_result
	{
		using result               = detail::workbench_composition_probe_result;
		using state_type           = workbench_composition_probe_state;
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->Window == nullptr
			|| implementation->Host == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| !implementation->LastAcceptedHandoff.has_value()) {
			return result::failed;
		}

		if (!implementation->WorkbenchCompositionProbe.has_value()) {
			implementation->WorkbenchCompositionProbe.emplace();
		}
		auto& probe = *implementation->WorkbenchCompositionProbe;
		probe.Evidence.FixtureDiffHelperUnused
			= implementation->FixtureDiffHelperInvocations == 0U;
		auto* const host       = implementation->WorkbenchHost.get();
		auto* const window     = implementation->Window.get();
		auto* const comparison = window->comparison_surface();
		const auto  fail       = [&probe] {
			probe.Phase = state_type::phase::failed;
			return result::failed;
		};
		// [[BPR-0860]], [[BPR-0864]]. Every phase that waits does so through
		// the shared bounded wait, which names the condition and prints what it
		// depends on when it gives up ([[BPR-1037]]).
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
		// [[BPR-0860]]. What the drawer's room and its width store depend on.
		const auto widthFacts = [&](QTextStream& aStream) {
			aStream
				<< "dispatches=" << static_cast<qulonglong>(implementation->WorkbenchWidthDispatches)
				<< " persisted_layouts=" << static_cast<qulonglong>(host->persisted_layouts())
				<< " layout_updates=" << static_cast<qulonglong>(window->statistics().LayoutUpdates)
				<< " host_files_width=" << static_cast<qulonglong>(host->layout().FilesWidth)
				<< " splitter_widgets=" << static_cast<qulonglong>(window->central_splitter()->sizes().size())
				<< " persistence_available=" << truth(implementation->WorkbenchPersistenceAvailable)
				<< " profile_scoped="
				<< truth(implementation->WorkbenchSettings != nullptr
						 && implementation->WorkbenchSettings->profile_scoped());
			// The drag's baselines and target exist only once it is made.
			if (probe.Phase == state_type::phase::awaiting_width_store) {
				aStream
					<< " expected_dispatches=" << static_cast<qulonglong>(probe.WidthDispatches) + 1U
					<< " expected_persisted=" << static_cast<qulonglong>(probe.PersistedLayouts) + 1U
					<< " requested_files_width=" << probe.RequestedFilesWidth
					<< " initial_files_width=" << static_cast<qulonglong>(probe.InitialFilesWidth);
			}
		};

		if (probe.Phase == state_type::phase::failed) {
			return result::failed;
		}
		if (probe.Phase == state_type::phase::passed) {
			return result::passed;
		}

		if (probe.Phase == state_type::phase::awaiting_observation) {
			const auto& handoff  = *implementation->LastAcceptedHandoff;
			const auto& snapshot = implementation->WindowRead->accepted_snapshot();
			if (!snapshot.has_value() || host->outstanding_requests() != 0U
				|| !host->state().Navigation.has_value()) {
				// [[BPR-0864]]. The accepted pull request's files reach the
				// workbench host as an observation; its navigation is what the
				// file gesture below needs.
				return wait("WORKBENCH_OBSERVATION_STALLED", [&](QTextStream& aStream) {
					aStream << "snapshot=" << present(snapshot.has_value())
							<< " snapshot_files=" << static_cast<qulonglong>(snapshot.has_value() ? snapshot->Files.size() : 0U)
							<< " window_read_quiescent=" << truth(implementation->WindowRead->is_quiescent())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " review_active=" << truth(implementation->WorkbenchReview.active());
				});
			}

			probe.Authority = workbench_authority(handoff);
			probe.ExpectedFiles.clear();
			for (const auto& file : snapshot->Files) {
				const auto identity = core::file_identity_of(file.Path);
				if (!identity.valid()) {
					return fail();
				}
				if (std::ranges::find(probe.ExpectedFiles, identity)
					== probe.ExpectedFiles.end()) {
					probe.ExpectedFiles.push_back(identity);
				}
			}

			probe.Evidence.ExactAuthority
				= application::valid(probe.Authority)
				&& implementation->WorkbenchReview.exact(handoff)
				&& probe.Authority.ApplicationSession
					== handoff.ApplicationSession
				&& probe.Authority.Profile == handoff.Profile
				&& probe.Authority.ProfileRevision == handoff.ProfileRevision
				&& probe.Authority.Host == handoff.Host
				&& probe.Authority.HostAuthority == handoff.HostAuthority
				&& probe.Authority.Account == handoff.Account
				&& probe.Authority.Viewer == handoff.Viewer
				&& probe.Authority.Repository == handoff.Repository
				&& probe.Authority.PullRequest == handoff.PullRequest
				&& probe.Authority.Number == handoff.Number
				&& probe.Authority.Snapshot == handoff.Snapshot
				&& probe.Authority.IdentityGeneration
					== handoff.IdentityGeneration
				&& probe.Authority.PullRequestGeneration
					== handoff.PullRequestGeneration;
			probe.Evidence.BoundedObservation
				= !probe.ExpectedFiles.empty()
				&& probe.ExpectedFiles.size()
					<= application::workbench_host_limits{ }.ActionableFiles
				&& std::ranges::equal(host->actionable(), probe.ExpectedFiles)
				&& host->state().Navigation.has_value();
			// [[BPR-0984]]. The session store also holds this window's observed
			// native geometry (BPR-0977): presentation the window reported, not
			// a layout storage supplied. Every stored field is still the default.
			auto sessionLayout     = host->layout();
			sessionLayout.Geometry = { };
			probe.Evidence.SessionSettingsFallback
				= !implementation->WorkbenchPersistenceAvailable
				&& implementation->WorkbenchSettings != nullptr
				&& sessionLayout == application::workbench_layout_settings{ };
			if (!probe.Evidence.ExactAuthority
				|| !probe.Evidence.BoundedObservation
				|| !probe.Evidence.SessionSettingsFallback
				|| window->files_drawer() == nullptr
				|| window->files_drawer()->contents() == nullptr
				|| window->files_drawer()->contents_view() == nullptr
				|| window->files_drawer()->contents()->rowCount() != 1) {
				return fail();
			}

			probe.IntentDispatches   = implementation->WorkbenchIntentDispatches;
			probe.WindowStatistics   = window->statistics();
			probe.SelectedDiffBegins = implementation->SelectedDiffBegins;
			probe.SelectedDiffPublications
				= implementation->SelectedDiffPublications;
			probe.SelectedDiffFailures = implementation->SelectedDiffFailures;
			auto* const view           = window->files_drawer()->contents_view();
			view->setCurrentIndex(view->model()->index(0, 0));
			return advance(state_type::phase::awaiting_file_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_file_projection) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches) {
				if (statistics.FileSelections
					== probe.WindowStatistics.FileSelections + 1U) {
					probe.Evidence.FileGesture = true;
					return fail();
				}
				// [[BPR-0864]]. Neither the files drawer's selection nor the
				// intent it dispatches has arrived.
				return wait("WORKBENCH_FILE_SELECTION_STALLED", [&](QTextStream& aStream) {
					const auto* const view = window->files_drawer()->contents_view();
					aStream << "intent_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchIntentDispatches)
							<< " expected_intent_dispatches=" << static_cast<qulonglong>(probe.IntentDispatches) + 1U
							<< " file_selections=" << static_cast<qulonglong>(statistics.FileSelections)
							<< " expected_file_selections=" << static_cast<qulonglong>(probe.WindowStatistics.FileSelections) + 1U
							<< " current_row=" << view->currentIndex().row()
							<< " drawer_rows=" << window->files_drawer()->contents()->rowCount()
							<< " host_selected=" << present(host->selected().has_value());
				});
			}
			probe.Evidence.IntentDispatch
				= implementation->WorkbenchIntentDispatches
				== probe.IntentDispatches + 1U;
			probe.Evidence.FileGesture
				= statistics.FileSelections
				== probe.WindowStatistics.FileSelections + 1U;
			// [[BPR-1328]]. The fixture's only file is also its last, so the
			// delivered commands bound Next file there: the arrow stays enabled
			// and names the bound, where before BPR-1328 it was disabled.
			probe.Evidence.ProjectionDelivery
				= host->selected() == std::optional{ probe.ExpectedFiles.front() }
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& statistics.CommandUpdates
					> probe.WindowStatistics.CommandUpdates
				&& window->next_file_action()->isEnabled()
				&& window->next_file_action()->statusTip()
					== QStringLiteral("Last file in this pull request.");
			if (!probe.Evidence.IntentDispatch || !probe.Evidence.FileGesture
				|| !probe.Evidence.ProjectionDelivery) {
				return fail();
			}
			return advance(state_type::phase::awaiting_diff);
		}

		if (probe.Phase == state_type::phase::awaiting_diff) {
			if (implementation->SelectedDiffFailures
				!= probe.SelectedDiffFailures) {
				return fail();
			}
			if (comparison == nullptr || comparison->current_result() == nullptr
				|| !comparison->selection().has_value()
				|| implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications
				|| !implementation->LastSelectedDiff.has_value()) {
				// [[BPR-0864]]. The selected file's diff is computed through
				// the Git and cache actors and published to the comparison.
				return wait("WORKBENCH_SELECTED_DIFF_STALLED", [&](QTextStream& aStream) {
					const auto work = diff_work_evidence(aApplication);
					aStream << "selected_diff_begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins)
							<< " expected_selected_diff_begins=" << static_cast<qulonglong>(probe.SelectedDiffBegins) + 1U
							<< " selected_diff_publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications)
							<< " expected_selected_diff_publications=" << static_cast<qulonglong>(probe.SelectedDiffPublications) + 1U
							<< " selected_diff_failures=" << static_cast<qulonglong>(implementation->SelectedDiffFailures)
							<< " last_selected_diff=" << present(implementation->LastSelectedDiff.has_value())
							<< " comparison_result=" << present(comparison != nullptr && comparison->current_result() != nullptr)
							<< " comparison_selection=" << present(comparison != nullptr && comparison->selection().has_value())
							<< " host_selected=" << present(host->selected().has_value())
							<< " diff_jobs=" << static_cast<qulonglong>(work.OutstandingJobs)
							<< " diff_pending=" << truth(work.Pending)
							<< " diff_quiescent=" << truth(work.Quiescent)
							<< " epoch_jobs=" << static_cast<qulonglong>(work.EpochOutstandingJobs);
				});
			}
			const auto& publication = *implementation->LastSelectedDiff;
			const auto& request     = publication.Request;
			const auto& key         = publication.Key;
			const auto& epoch       = implementation->EpochHost->state();
			const auto  fixture     = implementation->Fixture->evidence();
			const auto  leftBlob    = core::parse_git_blob_oid(fixture.BaseBlobOid);
			const auto  rightBlob   = core::parse_git_blob_oid(fixture.HeadOneBlobOid);
			probe.Evidence.SelectedDiffRequest
				= implementation->SelectedDiffBegins
					== probe.SelectedDiffBegins + 1U
				&& implementation->SelectedDiffPublications
					== probe.SelectedDiffPublications + 1U
				&& request.WorkbenchAuthority == probe.Authority
				&& request.Context.Generation == host->generation()
				&& request.File.File == probe.ExpectedFiles.front()
				&& request.File.PresentationPath == "review.cpp"
				&& request.File.CurrentPath == std::filesystem::path{ "review.cpp" }
				&& !request.File.PreviousPath.has_value()
				&& request.File.Change
					== application::workbench_file_change_kind::modified;
			probe.Evidence.ExactComparison
				= epoch.Authority.has_value()
				&& epoch.AcceptedComparison.has_value()
				&& leftBlob.has_value() && rightBlob.has_value()
				&& key.Authority == *epoch.Authority
				&& key.Repository == epoch.Authority->Repository
				&& key.PullRequest == epoch.Authority->PullRequest
				&& key.LeftAnchor == epoch.AcceptedComparison->Left.Identity
				&& key.RightAnchor == epoch.AcceptedComparison->Right.Identity
				&& key.LeftBlob == leftBlob
				&& key.RightBlob == rightBlob
				&& key.File == probe.ExpectedFiles.front()
				&& key.Options == request.Diff.OptionsDigest
				&& key.Algorithm == core::supported_diff_algorithm_revision;
			if (!probe.Evidence.SelectedDiffRequest
				|| !probe.Evidence.ExactComparison) {
				QTextStream stream{ stdout };
				stream << "WORKBENCH_SELECTED_DIFF_FACTS:begins=" << static_cast<qulonglong>(implementation->SelectedDiffBegins - probe.SelectedDiffBegins)
					   << " publications=" << static_cast<qulonglong>(implementation->SelectedDiffPublications - probe.SelectedDiffPublications)
					   << " authority=" << truth(request.WorkbenchAuthority == probe.Authority)
					   << " generation=" << truth(request.Context.Generation == host->generation())
					   << " file=" << truth(request.File.File == probe.ExpectedFiles.front())
					   << " presentation_path=" << truth(request.File.PresentationPath == "review.cpp")
					   << " current_path=" << truth(request.File.CurrentPath == std::filesystem::path{ "review.cpp" })
					   << " previous_path=" << present(request.File.PreviousPath.has_value())
					   << " modified=" << truth(request.File.Change == application::workbench_file_change_kind::modified)
					   << " exact_comparison=" << truth(probe.Evidence.ExactComparison) << '\n';
				stream.flush();
				return fail();
			}
			const auto& groups   = comparison->current_result()->Alignment.Groups;
			const auto  selected = comparison->selection().has_value()
					&& comparison->selection()->Group.has_value()
				? std::ranges::find(
					  groups,
					  *comparison->selection()->Group,
					  &core::change_group::Id)
				: groups.end();
			auto*       action   = selected == groups.end()
					|| std::next(selected) != groups.end()
				? window->next_change_action()
				: window->previous_change_action();
			if (action == nullptr || !action->isEnabled()) {
				return fail();
			}

			probe.Selection        = comparison->selection();
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			action->trigger();
			return advance(state_type::phase::awaiting_change_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_change_projection) {
			if (implementation->WorkbenchChangeDispatches
				== probe.ChangeDispatches) {
				// [[BPR-0864]]. The change action's intent has not reached the
				// host.
				return wait("WORKBENCH_CHANGE_DISPATCH_STALLED", [&](QTextStream& aStream) {
					const auto computed = comparison == nullptr ? nullptr : comparison->current_result();
					aStream << "change_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
							<< " expected_change_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches) + 1U
							<< " next_change_enabled=" << truth(window->next_change_action()->isEnabled())
							<< " previous_change_enabled=" << truth(window->previous_change_action()->isEnabled())
							<< " selection_moved=" << truth(comparison != nullptr && comparison->selection() != probe.Selection)
							<< " groups=" << static_cast<qulonglong>(computed == nullptr ? 0U : computed->Alignment.Groups.size());
				});
			}
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches
					== probe.ChangeDispatches + 1U
				&& comparison != nullptr
				&& comparison->selection() != probe.Selection;
			if (!probe.Evidence.ChangeDispatch || window->files_drawer() == nullptr) {
				return fail();
			}

			const auto& groups = comparison->current_result()->Alignment.Groups;
			if (groups.size() < 2U) {
				return fail();
			}
			for (auto attempts = std::size_t{ 0U };
				 attempts < groups.size()
				 && (!comparison->selection().has_value()
					 || comparison->selection()->Group != groups.back().Id);
				 ++attempts) {
				if (!comparison->navigate_change(
						core::change_navigation_direction::next,
						core::change_navigation_policy::stop)) {
					return fail();
				}
			}
			if (!comparison->selection().has_value()
				|| comparison->selection()->Group != groups.back().Id) {
				return fail();
			}
			auto wrapCommands = core::workbench_command_delta{ };
			for (auto index = std::size_t{ 0U };
				 index < wrapCommands.Commands.size(); ++index) {
				wrapCommands.Commands[index] = core::workbench_command_projection{
					.Id             = static_cast<core::workbench_command_id>(index),
					.Enabled        = true,
					.DisabledReason = core::workbench_reason_code::none,
				};
			}
			wrapCommands.Policy = core::change_navigation_policy::wrap;
			window->publish(application::workbench_projection{
				.Context  = implementation->LastSelectedDiff->Request.Context,
				.Commands = wrapCommands,
			});
			probe.ChangeDispatches = implementation->WorkbenchChangeDispatches;
			window->next_change_action()->trigger();
			// [[BPR-1417]]. A press in Review is applied once the active tab's
			// exact view is on screen, and the selections made just above reach
			// the tab a lane turn later, so the press is not answered here.
			return advance(state_type::phase::awaiting_wrap_projection);
		}

		if (probe.Phase == state_type::phase::awaiting_wrap_projection) {
			if (implementation->WorkbenchChangeDispatches == probe.ChangeDispatches) {
				return wait("WORKBENCH_WRAP_DISPATCH_STALLED", [&](QTextStream& aStream) {
					aStream << "change_dispatches=" << static_cast<qulonglong>(implementation->WorkbenchChangeDispatches)
							<< " expected_change_dispatches=" << static_cast<qulonglong>(probe.ChangeDispatches) + 1U
							<< " navigation_retained=" << truth(implementation->PendingReviewNavigation.has_value())
							<< " next_change_enabled=" << truth(window->next_change_action()->isEnabled());
				});
			}
			const auto computed = comparison->current_result();
			probe.Evidence.ChangeDispatch
				= implementation->WorkbenchChangeDispatches == probe.ChangeDispatches + 1U
				&& computed != nullptr && !computed->Alignment.Groups.empty()
				&& comparison->selection().has_value()
				&& comparison->selection()->Group == computed->Alignment.Groups.front().Id;
			if (!probe.Evidence.ChangeDispatch) {
				return fail();
			}

			// [[BPR-0860]]. The drag needs a files drawer with an edge. Since
			// [[BPR-0771]] this course names its pull request in the Home bar,
			// so the window opens on Overview -- [[BPR-0661]]'s default opening
			// modality -- and since [[BPR-0727]] Overview collapses the three
			// drawers in the core's own decision. A collapsed drawer with the
			// controls hidden takes no width and the handle beside it has
			// nothing to move, so the drag reported nothing and the course sat
			// in `awaiting_width_store` until the run budget ended. A reviewer
			// who wants the files drawer wider leaves Overview first; the
			// course states Review, the one modality whose files drawer is
			// expanded, and enters it through the shared step ([[BPR-0863]]).
			return advance(state_type::phase::awaiting_files_drawer);
		}

		if (probe.Phase == state_type::phase::awaiting_files_drawer) {
			// The step presses the strip's Review and answers once the window
			// and the host agree on it, the files drawer included, and the
			// store Review causes has landed.
			const auto modality = enter_course_modality(aApplication, probe.Modality);
			if (modality == detail::course_modality_result::pending) {
				return result::pending;
			}
			if (modality == detail::course_modality_result::failed) {
				return fail();
			}
			// The drawer's room follows its state through the splitter. The
			// baselines below are taken only once it has some and nothing is
			// outstanding, so the width store the drag causes is the next one.
			// The step's own wait is bounded by the step; this one starts once
			// the step has answered.
			auto* const splitter = window->central_splitter();
			const auto  sizes    = splitter->sizes();
			if (sizes.size() != 4 || sizes[1] <= 0
				|| host->outstanding_requests() != 0U) {
				return wait("WORKBENCH_FILES_DRAWER_EXPAND_STALLED", widthFacts);
			}

			probe.WidthDispatches   = implementation->WorkbenchWidthDispatches;
			probe.PersistedLayouts  = host->persisted_layouts();
			probe.InitialFilesWidth = host->layout().FilesWidth;
			probe.WindowStatistics  = window->statistics();
			const auto target       = static_cast<int>(std::min<std::uint32_t>(
				probe.InitialFilesWidth + 41U,
				application::maximum_workbench_drawer_width));
			// [[BPR-0662]]. The reviewer's path: a drag of the handle
			// between the files drawer and the centre. A direct resize of
			// the panel is a squeeze the window answers with the persisted
			// widths and never reports.
			//
			// The handle is moved by the distance the reviewer would drag it
			// rather than to a position computed from the widths to its left:
			// a collapsed neighbour is hidden, and so is its handle, so those
			// widths are no longer what stands between the splitter's edge and
			// this one.
			splitter->move_handle(
				2, splitter->handle(2)->x() + (target - sizes[1]));
			probe.RequestedFilesWidth = target;
			return advance(state_type::phase::awaiting_width_store);
		}

		if (probe.Phase == state_type::phase::awaiting_width_store) {
			const auto statistics = window->statistics();
			if (implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches
				|| host->persisted_layouts() == probe.PersistedLayouts) {
				// [[BPR-0860]]. The dispatch and the store are both effects
				// this stage does not control; the term says which of the two
				// never arrived.
				return wait(
					implementation->WorkbenchWidthDispatches
							== probe.WidthDispatches
						? "WORKBENCH_DRAWER_WIDTH_DISPATCH_STALLED"
						: "WORKBENCH_DRAWER_WIDTH_STORE_STALLED",
					widthFacts);
			}
			probe.Evidence.DrawerWidthDispatch
				= implementation->WorkbenchWidthDispatches
					== probe.WidthDispatches + 1U
				&& host->layout().FilesWidth != probe.InitialFilesWidth
				&& host->persisted_layouts() == probe.PersistedLayouts + 1U
				&& statistics.LayoutUpdates
					> probe.WindowStatistics.LayoutUpdates;
			if (!probe.Evidence.DrawerWidthDispatch) {
				return fail();
			}

			const auto& handoff  = *implementation->LastAcceptedHandoff;
			auto        mismatch = workbench_probe_invalidation(
				handoff,
				core::pull_request_id{ handoff.PullRequest.value() + 1U });
			const auto selected      = host->selected();
			const auto generation    = host->generation();
			const auto invalidations = implementation->WorkbenchInvalidations;
			implementation->invalidate(std::move(mismatch));
			probe.Evidence.MismatchPreserved
				= implementation->WorkbenchInvalidations == invalidations
				&& implementation->WorkbenchReview.active()
				&& host->selected() == selected
				&& host->generation() == generation;
			if (!probe.Evidence.MismatchPreserved) {
				return fail();
			}

			probe.Generation       = generation;
			probe.Invalidations    = invalidations;
			probe.WindowStatistics = statistics;
			implementation->invalidate(
				workbench_probe_invalidation(handoff, handoff.PullRequest));
			return advance(state_type::phase::awaiting_invalidation);
		}

		if (probe.Phase == state_type::phase::awaiting_invalidation) {
			const auto statistics = window->statistics();
			if (host->generation() == probe.Generation
				|| host->outstanding_requests() != 0U) {
				// [[BPR-0864]]. The exact invalidation supersedes the host's
				// generation and clears what the review held.
				return wait("WORKBENCH_INVALIDATION_STALLED", [&](QTextStream& aStream) {
					aStream << "generation=" << static_cast<qulonglong>(host->generation().value())
							<< " invalidated_generation=" << static_cast<qulonglong>(probe.Generation.value())
							<< " invalidations=" << static_cast<qulonglong>(implementation->WorkbenchInvalidations)
							<< " expected_invalidations=" << static_cast<qulonglong>(probe.Invalidations) + 1U
							<< " review_active=" << truth(implementation->WorkbenchReview.active())
							<< " actionable=" << static_cast<qulonglong>(host->actionable().size())
							<< " host_selected=" << present(host->selected().has_value())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " publications=" << static_cast<qulonglong>(statistics.Publications)
							<< " invalidated_publications=" << static_cast<qulonglong>(probe.WindowStatistics.Publications);
				});
			}
			probe.Evidence.InvalidationSuperseded
				= implementation->WorkbenchInvalidations
					== probe.Invalidations + 1U
				&& !implementation->WorkbenchReview.active()
				&& host->actionable().empty()
				&& !host->selected().has_value()
				&& !host->state().Navigation.has_value()
				&& statistics.Publications > probe.WindowStatistics.Publications
				&& !window->previous_file_action()->isEnabled()
				&& !window->next_file_action()->isEnabled();
			if (!probe.Evidence.InvalidationSuperseded) {
				return fail();
			}

			probe.WindowStatistics = statistics;
			implementation->WorkbenchReview.make_unavailable(probe.Authority);
			host->observe(probe.Authority);
			return advance(state_type::phase::awaiting_failure);
		}

		if (probe.Phase == state_type::phase::awaiting_failure) {
			const auto statistics = window->statistics();
			if (statistics.Failures == probe.WindowStatistics.Failures
				|| host->outstanding_requests() != 0U) {
				// [[BPR-0864]]. An observation of an unavailable review fails,
				// and the failure reaches the window.
				return wait("WORKBENCH_FAILURE_DELIVERY_STALLED", [&](QTextStream& aStream) {
					aStream << "failures=" << static_cast<qulonglong>(statistics.Failures)
							<< " expected_failures=" << static_cast<qulonglong>(probe.WindowStatistics.Failures) + 1U
							<< " review_active=" << truth(implementation->WorkbenchReview.active())
							<< " navigation=" << present(host->state().Navigation.has_value())
							<< " publications=" << static_cast<qulonglong>(statistics.Publications)
							<< " observed_publications=" << static_cast<qulonglong>(probe.WindowStatistics.Publications);
				});
			}
			probe.Evidence.FailureDelivery
				= statistics.Failures == probe.WindowStatistics.Failures + 1U;
			probe.Evidence.FixtureDiffHelperUnused
				= implementation->FixtureDiffHelperInvocations == 0U;
			if (!probe.Evidence.FailureDelivery) {
				return fail();
			}

			const auto all = std::array{
				probe.Evidence.ExactAuthority,
				probe.Evidence.BoundedObservation,
				probe.Evidence.IntentDispatch,
				probe.Evidence.ProjectionDelivery,
				probe.Evidence.FileGesture,
				probe.Evidence.SelectedDiffRequest,
				probe.Evidence.ExactComparison,
				probe.Evidence.ChangeDispatch,
				probe.Evidence.DrawerWidthDispatch,
				probe.Evidence.SessionSettingsFallback,
				probe.Evidence.MismatchPreserved,
				probe.Evidence.InvalidationSuperseded,
				probe.Evidence.FailureDelivery,
				probe.Evidence.FixtureDiffHelperUnused,
			};
			if (!std::ranges::all_of(all, std::identity{ })) {
				return fail();
			}
			probe.Phase = state_type::phase::passed;
			return result::passed;
		}

		return fail();
	}

	auto detail::github_application_fixture_access::workbench_composition_evidence(
		const github_application& aApplication) noexcept
		-> detail::workbench_composition_probe_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
				&& implementation->WorkbenchCompositionProbe.has_value()
			? implementation->WorkbenchCompositionProbe->Evidence
			: detail::workbench_composition_probe_evidence{ };
	}
#endif
}
