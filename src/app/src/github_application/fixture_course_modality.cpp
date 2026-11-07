#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/document_tab_host.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/epoch_catalog_port.hpp"
	#include "bpr/application/github_epoch_handoff.hpp"
	#include "bpr/application/github_read_model.hpp"
	#include "bpr/application/github_window_read_host.hpp"
	#include "bpr/application/notification_delivery.hpp"
	#include "bpr/application/personal_discovery_host.hpp"
	#include "bpr/application/review_mutation_application_host.hpp"
	#include "bpr/application/review_mutation_port.hpp"
	#include "bpr/application/review_queue_enrollment.hpp"
	#include "bpr/application/source_search_host.hpp"
	#include "bpr/application/workbench_application_host.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/anonymous_read_policy.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_transition.hpp"
	#include "bpr/core/document_tabs.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/core/epoch_transition.hpp"
	#include "bpr/core/github_model.hpp"
	#include "bpr/core/model.hpp"
	#include "bpr/core/profile_window.hpp"
	#include "bpr/core/review_round_trip.hpp"
	#include "bpr/core/window_review_restoration.hpp"
	#include "bpr/core/workbench_interaction.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/system_tray_notification_sink.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_strip.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/queue_notifications.hpp"
	#include "github_application/review_mutation_selection.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/workbench_composition.hpp"
	#include "github_application_fixture_access.hpp"
	#include "github_public_path_fixture.hpp"
	#include "pane_find_controller.hpp"
	#include <QAction>
	#include <QByteArray>
	#include <QChar>
	#include <QDesktopServices>
	#include <QLatin1Char>
	#include <QList>
	#include <QListSpecialMethods>
	#include <QSplitter>
	#include <QString>
	#include <QSystemTrayIcon>
	#include <QTextStream>
	#include <QWidget>
	#include <array>
	#include <chrono>
	#include <cstddef>
	#include <filesystem>
	#include <fstream>
	#include <map>
	#include <span>
	#include <stop_token>
	#include <string_view>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		/// \brief [[BPR-0863]]. How long a course's modality may take to settle.
		///
		/// \details The step is advanced by the automated run about every ten
		/// milliseconds and by the large-review benchmark about every
		/// millisecond, so its bound is time rather than a count of calls. Five
		/// seconds is far longer than a modality change and the drawer store
		/// behind it take on a loaded machine, and far short of any course's run
		/// budget. The floor of calls keeps one long tick from reading as a stall.
		inline constexpr auto course_modality_wait = std::chrono::milliseconds{ 5'000 };

		inline constexpr std::size_t course_modality_wait_floor = 16U;

		[[nodiscard]] auto course_core_modality(const qt::review_modality aModality) noexcept
			-> core::workbench_modality
		{
			switch (aModality) {
			case qt::review_modality::overview:
				return core::workbench_modality::overview;
			case qt::review_modality::review:
				return core::workbench_modality::review;
			case qt::review_modality::discuss:
				return core::workbench_modality::discuss;
			case qt::review_modality::threads:
				return core::workbench_modality::threads;
			}
			return core::workbench_modality::review;
		}

		[[nodiscard]] auto course_core_modality_name(const core::workbench_modality aModality)
			-> QString
		{
			switch (aModality) {
			case core::workbench_modality::overview:
				return QStringLiteral("Overview");
			case core::workbench_modality::review:
				return QStringLiteral("Review");
			case core::workbench_modality::discuss:
				return QStringLiteral("Discuss");
			case core::workbench_modality::threads:
				return QStringLiteral("Threads");
			}
			return QStringLiteral("unknown");
		}

		[[nodiscard]] auto course_drawer_name(const core::workbench_drawer_state aState) -> QString
		{
			return aState == core::workbench_drawer_state::expanded
				? QStringLiteral("expanded")
				: QStringLiteral("collapsed");
		}

		/// \brief [[BPR-0863]]. What the window shows a reviewer right now.
		///
		/// \details Read from the widgets, not from a table of what each
		/// modality should show, so the answer follows the product and cannot
		/// drift from it. A drawer is offered when it is shown and expanded.
		[[nodiscard]] auto course_offered_surfaces(const qt::workbench_window& aWindow)
			-> detail::course_surfaces
		{
			const auto offered = [&aWindow](const QWidget* const aSurface) {
				return !aWindow.showing_home() && aSurface != nullptr
					&& aSurface->isVisibleTo(&aWindow);
			};
			const auto expanded = [&offered](const qt::workbench_drawer_panel* const aPanel) {
				return offered(aPanel)
					&& aPanel->state() == core::workbench_drawer_state::expanded;
			};
			return {
				.Comparison          = offered(aWindow.comparison_surface()),
				.FilesDrawer         = expanded(aWindow.files_drawer()),
				.ConversationsDrawer = expanded(aWindow.conversations_drawer()),
				.RepositoriesDrawer  = expanded(aWindow.repositories_drawer()),
			};
		}

		/// \brief Names the surfaces set in `aSurfaces`, or `none`.
		[[nodiscard]] auto course_surface_names(const detail::course_surfaces& aSurfaces)
			-> QString
		{
			auto names = QStringList{ };
			if (aSurfaces.Comparison) {
				names.append(QStringLiteral("comparison"));
			}
			if (aSurfaces.FilesDrawer) {
				names.append(QStringLiteral("files_drawer"));
			}
			if (aSurfaces.ConversationsDrawer) {
				names.append(QStringLiteral("conversations_drawer"));
			}
			if (aSurfaces.RepositoriesDrawer) {
				names.append(QStringLiteral("repositories_drawer"));
			}
			return names.isEmpty() ? QStringLiteral("none") : names.join(QLatin1Char{ ',' });
		}

		/// \brief The surfaces in `aWanted` that `aOffered` does not show.
		[[nodiscard]] auto course_surfaces_missing(
			const detail::course_surfaces& aWanted,
			const detail::course_surfaces& aOffered) noexcept
			-> detail::course_surfaces
		{
			return {
				.Comparison          = aWanted.Comparison && !aOffered.Comparison,
				.FilesDrawer         = aWanted.FilesDrawer && !aOffered.FilesDrawer,
				.ConversationsDrawer = aWanted.ConversationsDrawer && !aOffered.ConversationsDrawer,
				.RepositoriesDrawer  = aWanted.RepositoriesDrawer && !aOffered.RepositoriesDrawer,
			};
		}

		[[nodiscard]] auto course_surfaces_any(const detail::course_surfaces& aSurfaces) noexcept
			-> bool
		{
			return aSurfaces.Comparison || aSurfaces.FilesDrawer
				|| aSurfaces.ConversationsDrawer || aSurfaces.RepositoriesDrawer;
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::course_modality_facts(
		const github_application& aApplication) -> QString
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr) {
			return QStringLiteral("modality=unavailable");
		}
		const auto& window  = *implementation->Window;
		const auto& state   = implementation->WorkbenchHost->state();
		const auto  offered = course_offered_surfaces(window);
		const auto  drawer  = [](const qt::workbench_drawer_panel* const aPanel) {
			return aPanel == nullptr ? QStringLiteral("absent") : course_drawer_name(aPanel->state());
		};
		const auto centre = window.showing_home() ? QStringLiteral("home")
			: window.showing_overview()           ? QStringLiteral("overview")
			: offered.Comparison                  ? QStringLiteral("comparison")
			: window.current_modality() == qt::review_modality::discuss
			? QStringLiteral("discussion")
			: window.current_modality() == qt::review_modality::threads
			? QStringLiteral("threads")
			: QStringLiteral("other");
		auto       sizes  = QStringList{ };
		if (window.central_splitter() != nullptr) {
			for (const auto size : window.central_splitter()->sizes()) {
				sizes.append(QString::number(size));
			}
		}
		const auto withheld = course_surfaces_missing(
			detail::course_surfaces{
				.Comparison          = true,
				.FilesDrawer         = true,
				.ConversationsDrawer = true,
				.RepositoriesDrawer  = true,
			},
			offered);
		return QStringList{
			QStringLiteral("modality=%1").arg(window.modality() != nullptr ? qt::review_modality_title(window.modality()->current()) : QStringLiteral("none")),
			QStringLiteral("host_modality=%1").arg(course_core_modality_name(state.Modality)),
			QStringLiteral("centre=%1").arg(centre),
			QStringLiteral("offered=%1").arg(course_surface_names(offered)),
			QStringLiteral("withheld=%1").arg(course_surface_names(withheld)),
			QStringLiteral("files_drawer=%1").arg(drawer(window.files_drawer())),
			QStringLiteral("host_files_drawer=%1").arg(course_drawer_name(state.FilesDrawer)),
			QStringLiteral("conversations_drawer=%1").arg(drawer(window.conversations_drawer())),
			QStringLiteral("host_conversations_drawer=%1").arg(course_drawer_name(state.ConversationsDrawer)),
			QStringLiteral("repositories_drawer=%1").arg(drawer(window.repositories_drawer())),
			QStringLiteral("host_repositories_drawer=%1").arg(course_drawer_name(state.RepositoriesDrawer)),
			QStringLiteral("drawer_controls=%1").arg(window.behaviour().DrawerControls == application::review_drawer_controls::shown ? QStringLiteral("shown") : QStringLiteral("hidden")),
			QStringLiteral("outstanding_requests=%1").arg(static_cast<qulonglong>(implementation->WorkbenchHost->outstanding_requests())),
			QStringLiteral("splitter_sizes=%1").arg(sizes.isEmpty() ? QStringLiteral("none") : sizes.join(QLatin1Char{ ',' })),
		}
			.join(QLatin1Char{ ' ' });
	}

	auto detail::github_application_fixture_access::enter_course_modality(
		github_application&   aApplication,
		course_modality_step& aStep) -> course_modality_result
	{
		if (aStep.Result != course_modality_result::pending) {
			return aStep.Result;
		}
		auto* const implementation = aApplication.Implementation.get();
		const auto  now            = std::chrono::steady_clock::now();
		const auto  report         = [&](const char* const aTerm, const QString& aDetail) {
			auto stream = QTextStream{ stdout };
			stream << aTerm << ":wanted=" << qt::review_modality_title(aStep.Modality)
				   << ' ' << aDetail << ' ' << course_modality_facts(aApplication) << '\n';
			stream.flush();
			aStep.Result = course_modality_result::failed;
			return aStep.Result;
		};
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->Window->modality() == nullptr) {
			return report("COURSE_MODALITY_STALLED", QStringLiteral("strip=absent"));
		}
		auto& window = *implementation->Window;
		// The reviewer's gesture: a press of the strip's segment. Triggered even
		// when the window already shows the modality, so the course starts from
		// the modality's own drawers rather than whatever an earlier gesture
		// left open.
		if (!aStep.ChosenAt.has_value()) {
			window.modality()->action(aStep.Modality)->trigger();
			aStep.ChosenAt = now;
			return course_modality_result::pending;
		}
		++aStep.Waits;
		const auto& host   = *implementation->WorkbenchHost;
		const auto& state  = host.state();
		const auto  agrees = [](const qt::workbench_drawer_panel* const aPanel,
								const core::workbench_drawer_state      aHost) {
			return aPanel != nullptr && aPanel->state() == aHost;
		};
		const auto settled = !window.showing_home()
			&& window.current_modality() == aStep.Modality
			&& state.Modality == course_core_modality(aStep.Modality)
			&& agrees(window.files_drawer(), state.FilesDrawer)
			&& agrees(window.conversations_drawer(), state.ConversationsDrawer)
			&& agrees(window.repositories_drawer(), state.RepositoriesDrawer)
			&& host.outstanding_requests() == 0U;
		// Once settled, the modality is the one the course stated and what it
		// withholds is the product's decision. A needed surface may still be a
		// layout pass behind the drawers, so it is waited for like the rest; at
		// the bound, a settled modality withholding it is named as such.
		const auto missing = settled
			? course_surfaces_missing(aStep.Needs, course_offered_surfaces(window))
			: detail::course_surfaces{ };
		if (settled && !course_surfaces_any(missing)) {
			aStep.Result = course_modality_result::entered;
			return aStep.Result;
		}
		const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - *aStep.ChosenAt);
		if (waited < course_modality_wait || aStep.Waits < course_modality_wait_floor) {
			return course_modality_result::pending;
		}
		const auto bound = QStringLiteral("waited_ms=%1 wait_cap_ms=%2 waits=%3")
							   .arg(static_cast<qlonglong>(waited.count()))
							   .arg(static_cast<qlonglong>(course_modality_wait.count()))
							   .arg(static_cast<qulonglong>(aStep.Waits));
		if (settled) {
			return report(
				"COURSE_MODALITY_WITHHOLDS",
				QStringLiteral("needs=%1 missing=%2 %3")
					.arg(course_surface_names(aStep.Needs), course_surface_names(missing), bound));
		}
		return report("COURSE_MODALITY_STALLED", bound);
	}

	auto detail::github_application_fixture_access::verify_pending_first_file_choice_invalidation(
		github_application& aApplication) -> bool
	{
		auto* const owner = aApplication.Implementation.get();
		if (!owner || !owner->TabHost || !owner->LastAcceptedHandoff || !owner->WorkbenchReview.exact(*owner->LastAcceptedHandoff)) {
			return false;
		}
		const auto& tabs   = owner->TabHost->state();
		const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		if (!active || !active->View.File) {
			return false;
		}
		const auto  file    = *active->View.File;
		const auto& handoff = *owner->LastAcceptedHandoff;
		const auto  exact   = application::pull_request_authority_invalidation{
			.ApplicationSession = handoff.ApplicationSession,
			.Profile            = handoff.Profile,
			.ProfileRevision    = handoff.ProfileRevision,
			.Host               = handoff.Host,
			.HostAuthority      = handoff.HostAuthority,
			.Account            = handoff.Account,
			.Viewer             = handoff.Viewer,
			.Repository         = handoff.Repository,
			.PullRequest        = handoff.PullRequest,
			.IdentityGeneration = handoff.IdentityGeneration,
			.Mode               = handoff.Mode,
		};
		// Arrange the core state already observed between the real first-file
		// gesture and its comparison: an explicit choice, no tab or pair yet.
		owner->TabHost->reset();
		owner->TabHost->observe_file_choice(file);
		auto foreign        = exact;
		foreign.PullRequest = core::pull_request_id{ exact.PullRequest.value() == 1U ? 2U : 1U };
		owner->invalidate(foreign);
		if (tabs.Active || tabs.Chosen != std::optional{ file }) {
			return false;
		}
		owner->invalidate(exact);
		return !tabs.Active && !tabs.Pending && !tabs.Chosen;
	}

	auto detail::github_application_fixture_access::epoch_refresh_identity(
		const github_application& aApplication, const std::optional<core::bpr_window_context> aContext) -> epoch_refresh_identity_evidence
	{
		auto        result         = epoch_refresh_identity_evidence{ };
		const auto* implementation = aApplication.Implementation.get();
		if (aContext) {
			const auto found = aApplication.Sessions.find(*aContext);
			implementation   = found == aApplication.Sessions.end() ? nullptr : found->second.get();
		}
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->WorkbenchHost == nullptr
			|| implementation->EpochHost == nullptr || implementation->TabHost == nullptr) {
			return result;
		}
		result.SelectedFile       = implementation->WorkbenchHost->selected();
		result.TabModality        = github_application::implementation::tab_modality(implementation->WorkbenchHost->state().Modality);
		const auto& epoch         = implementation->EpochHost->state();
		result.AcceptedComparison = epoch.AcceptedComparison;
		result.ComparisonPending  = epoch.PendingComparison.has_value();
		const auto& tabs          = implementation->TabHost->state();
		if (const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr) {
			result.ActiveTab = *active;
		}
		result.TabPending = tabs.Pending.has_value();
		result.TabLoad    = implementation->TabLoad.has_value();
		if (implementation->PaneFind) {
			const auto counters    = implementation->PaneFind->counters();
			result.SearchCounters  = { counters.Admitted, counters.Scheduled, counters.Terminals, counters.StaleTerminals, counters.ProjectionFailures, counters.IngressRefusals };
			result.SearchQuiescent = implementation->PaneFind->is_quiescent();
		}
		if (const auto& publication = implementation->SelectedDiffPublication; publication) {
			result.DiffRequestMatches   = implementation->SelectedDiffRequest == std::optional{ publication->Request };
			result.PublishedFile        = publication->Request.File.File;
			result.PublishedComparison  = publication->Request.Diff.Comparison;
			const auto* surface         = implementation->Window->comparison_surface();
			result.PublicationDisplayed = surface->current_result() && surface->current_context()
				&& surface->current_context()->Key == publication->Key;
		}
		return result;
	}

	auto detail::github_application_fixture_access::close_review(
		const github_application& aApplication) -> close_review_evidence
	{
		auto        result         = close_review_evidence{ };
		const auto* implementation = aApplication.Implementation.get();
		if (implementation == nullptr || !implementation->WindowRead) {
			return result;
		}
		const auto& state           = implementation->WindowRead->state();
		result.ReadHoldsPullRequest = state.ActiveLocator.has_value() || state.PendingPullRequest.has_value()
			|| state.PullRequest.has_value() || state.AcceptedSnapshot.has_value();
		result.SnapshotHeld       = implementation->WindowRead->accepted_snapshot().has_value();
		result.ReadingDestination = implementation->ReadingDestination.has_value();
		result.TabDestination     = implementation->TabDestination.has_value();
		result.LedgerComposed     = implementation->SharedDrafts != nullptr;
		if (implementation->TabHost != nullptr) {
			result.Tabs      = implementation->TabHost->state().Tabs.size();
			result.TabChosen = implementation->TabHost->state().Chosen.has_value();
		}
		for (const auto& draft : implementation->saved_draft_values()) {
			result.SavedDraftBodies.push_back(draft.Body);
		}
		return result;
	}

	auto detail::github_application_fixture_access::evidence(
		const github_application& aApplication) noexcept
		-> detail::github_public_path_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->evidence()
			: detail::github_public_path_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::review_mutation_evidence(
		const github_application& aApplication) noexcept
		-> detail::review_mutation_fixture_evidence
	{
		return aApplication.Implementation->Fixture != nullptr
			? aApplication.Implementation->Fixture->review_mutation_evidence()
			: detail::review_mutation_fixture_evidence{ };
	}

	auto detail::github_application_fixture_access::thread_resolution_evidence(
		const github_application& aApplication) noexcept
		-> detail::thread_resolution_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		auto              evidence       = detail::thread_resolution_fixture_evidence{ };
		if (implementation == nullptr) {
			return evidence;
		}
		evidence.Gestures    = implementation->ThreadResolutionGestures;
		evidence.Refusals    = implementation->ThreadResolutionRefusals;
		evidence.Completions = implementation->ThreadResolutionCompletions;
		evidence.Failures    = implementation->ThreadResolutionFailures;
		if (implementation->ReviewMutationHost == nullptr) {
			return evidence;
		}
		const auto capabilities    = implementation->ReviewMutationHost->capabilities();
		evidence.HostComposed      = true;
		evidence.BindingOpen       = implementation->InlineReviewBinding && implementation->InlineReviewBinding->Target.Open;
		evidence.ResolvesThreads   = capabilities.ResolvesThreads;
		evidence.Reacts            = capabilities.Reacts;
		evidence.EditsDescription  = capabilities.EditsDescription;
		evidence.EditsComments     = capabilities.EditsComments;
		evidence.RequestsReviewers = capabilities.RequestsReviewers;
		return evidence;
	}

	auto detail::github_application_fixture_access::notification_tray(
		github_application& aApplication) noexcept -> QSystemTrayIcon*
	{
		const auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr && implementation->SystemNotificationSink != nullptr
			? implementation->SystemNotificationSink->tray_icon()
			: nullptr;
	}

	auto detail::github_application_fixture_access::personal_discovery_evidence(
		const github_application& aApplication) noexcept
		-> detail::personal_discovery_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Discovery == nullptr) {
			return { };
		}
		const auto& discovery = *implementation->Discovery;
		return {
			.Composed   = true,
			.Statistics = discovery.statistics(),
			.Status     = discovery.status(),
			.Quiescent  = discovery.is_quiescent(),
		};
	}

	auto detail::github_application_fixture_access::queue_notification_evidence(
		const github_application& aApplication) noexcept
		-> detail::queue_notification_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr) {
			return { };
		}
		const auto statistics    = implementation->Notifications != nullptr
			? implementation->Notifications->statistics()
			: application::notification_delivery_statistics{ };
		auto       lastSubmitted = std::optional<application::delivered_notification>{ };
		if (implementation->FixtureNotificationSink != nullptr) {
			lastSubmitted
				= implementation->FixtureNotificationSink->submitted_payload();
		} else if (implementation->SystemNotificationSink != nullptr) {
			lastSubmitted
				= implementation->SystemNotificationSink->submitted_payload();
		}
		return {
			.RequestsAttempted  = implementation->NotificationRequestsAttempted,
			.Submitted          = statistics.Submitted,
			.DisplaySubmissions = implementation->CapturedNotificationDisplays,
			.SuppressedUnattributableActivation
			= statistics.SuppressedUnattributableActivation,
			.Activations       = statistics.Activations,
			.Selections        = implementation->NotificationSelections,
			.SelectionFailures = implementation->NotificationSelectionFailures,
			.QueueQuiescent    = implementation->QueueHost != nullptr
				&& implementation->QueueHost->is_quiescent(),
			.LastSubmitted = std::move(lastSubmitted),
		};
	}

	auto detail::github_application_fixture_access::activate_fixture_notification(
		github_application& aApplication) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		return implementation != nullptr
			&& implementation->FixtureNotificationSink != nullptr
			&& implementation->FixtureNotificationSink->activate();
	}

	auto detail::github_application_fixture_access::maintenance_evidence(
		const github_application&                       aApplication,
		core::review_epoch_id                           aTarget,
		const std::optional<core::file_comparison_key>& aRetainedComparison) noexcept
		-> detail::github_epoch_maintenance_fixture_evidence
	{
		const auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Fixture == nullptr
			|| implementation->EpochHost == nullptr) {
			return { };
		}

		const auto& state = implementation->EpochHost->state();
		if (aRetainedComparison && (!core::valid(*aRetainedComparison) || !aRetainedComparison->LeftBlob || !core::valid(aTarget) || aTarget.Authority != aRetainedComparison->Authority.Scope || aTarget.Incarnation != aRetainedComparison->LeftAnchor.Incarnation || (state.Authority && *state.Authority != aRetainedComparison->Authority))) {
			return { };
		}
		// Shutdown clears live authority. A captured fixture comparison lets
		// this read-only inspector reopen the same catalog and retained refs.
		const auto      selectedAuthority        = state.Authority ? state.Authority
			: aRetainedComparison                                  ? std::optional{ aRetainedComparison->Authority }
																   : std::nullopt;
		const auto      root                     = implementation->Fixture->application_data_root();
		auto            retiredRepositoryPresent = false;
		auto            retiredMarkerPresent     = false;
		std::error_code directoryError;
		auto            marker = std::filesystem::directory_iterator{
			root / "quarantine" / "v1", directoryError
		};
		while (!directoryError
			   && marker != std::filesystem::directory_iterator{ }) {
			if (marker->path().extension() == ".deleted") {
				retiredMarkerPresent = true;
				auto repository      = marker->path().stem();
				repository += ".git";
				std::error_code existsError;
				retiredRepositoryPresent = retiredRepositoryPresent
					|| std::filesystem::exists(
											   root / "repositories" / "v1" / repository,
											   existsError);
			}
			marker.increment(directoryError);
		}
		// Whole-cache deletion may leave a replacement authority projected while
		// shutdown is rejecting its unfinished rehydration.  An empty target asks
		// for durable evidence about the retired cache, so derive the stable
		// catalog binding from the captured smart authority instead of sampling
		// that transient projection.
		if (!selectedAuthority.has_value()
			|| (!core::valid(aTarget) && retiredMarkerPresent)) {
			auto       catalogRevision  = std::int64_t{ 0 };
			auto       baseCacheDeleted = false;
			auto       headCacheDeleted = false;
			auto       baseAvailability = -1;
			auto       headAvailability = -1;
			const auto smartAuthority   = implementation->Fixture->last_smart_authority();
			const auto fixtureEvidence  = implementation->Fixture->evidence();
			const auto base             = core::parse_git_commit_oid(fixtureEvidence.BaseOid);
			if (smartAuthority && base) {
				auto reopened = implementation->EpochCatalogs->open(
					application::epoch_catalog_open_request{
						.ApplicationSession = state.ApplicationSession,
						.Authority          = {
							.Profile         = smartAuthority->Profile,
							.ProfileRevision = smartAuthority->ProfileRevision,
							.Host            = smartAuthority->Host,
							.HostAuthority   = smartAuthority->HostAuthority,
							.Account         = smartAuthority->Account,
							.Viewer          = smartAuthority->Viewer,
							.Repository      = smartAuthority->Repository,
							.PullRequest     = smartAuthority->PullRequest,
							.Number          = smartAuthority->Number,
							.ObjectFormat    = base->algorithm(),
						},
					},
					{ });
				if (reopened && *reopened) {
					const auto& binding = (*reopened)->binding();
					catalogRevision     = binding.Catalog.Revision.value();
					aTarget             = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
					const auto anchors = (*reopened)->exact_anchors(
						application::exact_anchor_lookup_request{
							.Catalog = binding.Catalog,
							.Anchors = {
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_base,
									.Epoch = aTarget.Ordinal,
								},
								core::comparison_anchor_identity{
									.Authority   = aTarget.Authority,
									.Incarnation = aTarget.Incarnation,
									.Kind        = core::comparison_anchor_kind::
										epoch_head,
									.Epoch = aTarget.Ordinal,
								},
							},
						},
						{ });
					if (anchors && anchors->Anchors.size() == 2U) {
						baseAvailability = static_cast<int>(
							anchors->Anchors[0].Availability);
						headAvailability = static_cast<int>(
							anchors->Anchors[1].Availability);
						baseCacheDeleted = anchors->Anchors[0].Availability
							== core::anchor_availability::cache_deleted;
						headCacheDeleted = anchors->Anchors[1].Availability
							== core::anchor_availability::cache_deleted;
					}
				}
			}
			return {
				.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
				.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
				.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
				.CatalogRevision                   = catalogRevision,
				.RepositoryPresent                 = retiredRepositoryPresent,
				.CacheDeletionMarkerPresent        = retiredMarkerPresent,
				.TargetBaseCacheDeleted            = baseCacheDeleted,
				.TargetHeadCacheDeleted            = headCacheDeleted,
				.RetiredRepositoryPresent          = retiredRepositoryPresent,
				.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
				.TargetBaseAvailability            = baseAvailability,
				.TargetHeadAvailability            = headAvailability,
			};
		}

		const auto lowerHex = [](const std::span<const std::uint8_t> aBytes) {
			return QByteArray::fromRawData(reinterpret_cast<const char*>(aBytes.data()), static_cast<qsizetype>(aBytes.size())).toHex().toStdString();
		};

		const auto& authority = *selectedAuthority;
		if (!core::valid(aTarget) && state.Catalog) {
			aTarget = core::review_epoch_id{
				.Authority   = authority.Scope,
				.Incarnation = state.Catalog->Incarnation,
				.Ordinal     = core::epoch_ordinal{ 1 },
			};
		}
		const auto cache       = lowerHex(authority.Cache.bytes());
		const auto scope       = lowerHex(aTarget.Authority.bytes());
		const auto repository  = root / "repositories" / "v1" / (cache + ".git");
		const auto ordinalText = [&lowerHex](std::int64_t aOrdinal) {
			std::array<std::uint8_t, 8U> bytes{ };
			const auto                   ordinal = static_cast<std::uint64_t>(aOrdinal);
			for (std::size_t index = 0U; index < bytes.size(); ++index) {
				bytes[index] = static_cast<std::uint8_t>(ordinal >> ((bytes.size() - index - 1U) * 8U));
			}
			return lowerHex(bytes);
		};
		// Installed Git encodes retained ref ordinals as 16 lowercase hex digits.
		const auto ordinal   = ordinalText(aTarget.Ordinal.value());
		const auto epochRoot = repository / "refs" / "betterpullreview" / "v1"
			/ "scope" / scope / "epoch"
			/ ordinal;
		const auto nonTargetOrdinal   = ordinalText(2);
		const auto nonTargetEpochRoot = repository / "refs"
			/ "betterpullreview" / "v1" / "scope" / scope / "epoch"
			/ nonTargetOrdinal;
		const auto      deletionMarker = root / "quarantine" / "v1" / (cache + ".deleted");
		std::error_code error;
		const auto      present = [&error](const std::filesystem::path& aPath) {
			error.clear();
			const auto result = std::filesystem::exists(aPath, error);
			return !error && result;
		};

		std::string baseCatalogOid, headCatalogOid;
		auto        baseCacheDeleted = false;
		auto        headCacheDeleted = false;
		auto        baseAvailability = -1;
		auto        headAvailability = -1;
		const auto& acceptedSnapshot = implementation->WindowRead->accepted_snapshot();
		const auto  acceptedBase     = acceptedSnapshot
			? core::parse_git_commit_oid(
				  acceptedSnapshot->PullRequest.BaseOid.value())
			: std::nullopt;
		const auto  objectFormat     = acceptedBase ? std::optional{ acceptedBase->algorithm() }
			: aRetainedComparison                   ? std::optional{ aRetainedComparison->LeftBlob->algorithm() }
													: std::nullopt;
		if (objectFormat && !state.PendingMaintenance.has_value()) {
			auto reopened = implementation->EpochCatalogs->open(
				application::epoch_catalog_open_request{
					.ApplicationSession = state.ApplicationSession,
					.Authority          = {
						.Profile         = authority.Profile,
						.ProfileRevision = authority.ProfileRevision,
						.Host            = authority.Host,
						.HostAuthority   = authority.HostAuthority,
						.Account         = authority.Account,
						.Viewer          = authority.Viewer,
						.Repository      = authority.Repository,
						.PullRequest     = authority.PullRequest,
						.Number          = authority.Number,
						.ObjectFormat    = *objectFormat,
					},
				},
				{ });
			if (reopened && *reopened) {
				const auto& binding = (*reopened)->binding();
				if (!core::valid(aTarget)) {
					aTarget = core::review_epoch_id{
						.Authority   = binding.Authority.Scope,
						.Incarnation = binding.Catalog.Incarnation,
						.Ordinal     = core::epoch_ordinal{ 1 },
					};
				}
				const auto anchors = (*reopened)->exact_anchors(
					application::exact_anchor_lookup_request{
						.Catalog = binding.Catalog,
						.Anchors = {
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_base,
								.Epoch       = aTarget.Ordinal,
							},
							core::comparison_anchor_identity{
								.Authority   = aTarget.Authority,
								.Incarnation = aTarget.Incarnation,
								.Kind        = core::comparison_anchor_kind::epoch_head,
								.Epoch       = aTarget.Ordinal,
							},
						},
					},
					{ });
				if (anchors && anchors->Anchors.size() == 2U) {
					baseCatalogOid   = core::canonical_hex(anchors->Anchors[0].Commit).view();
					headCatalogOid   = core::canonical_hex(anchors->Anchors[1].Commit).view();
					baseAvailability = static_cast<int>(
						anchors->Anchors[0].Availability);
					headAvailability = static_cast<int>(
						anchors->Anchors[1].Availability);
					baseCacheDeleted = anchors->Anchors[0].Availability
						== core::anchor_availability::cache_deleted;
					headCacheDeleted = anchors->Anchors[1].Availability
						== core::anchor_availability::cache_deleted;
				}
			}
		}

		const auto refOid = [](const std::filesystem::path& aPath) {
			std::ifstream input{ aPath };
			std::string   oid;
			input >> oid;
			return oid;
		};
		return {
			.PruneIntentDispatchCount          = implementation->EpochPruneIntentDispatchCount,
			.DeleteIntentDispatchCount         = implementation->EpochDeleteIntentDispatchCount,
			.CancelIntentDispatchCount         = implementation->EpochCancelIntentDispatchCount,
			.CatalogRevision                   = state.Catalog
				? state.Catalog->Revision.value()
				: 0,
			.TargetBaseCatalogOid              = std::move(baseCatalogOid),
			.TargetHeadCatalogOid              = std::move(headCatalogOid),
			.TargetBaseRefOid                  = refOid(epochRoot / "base"),
			.TargetHeadRefOid                  = refOid(epochRoot / "head"),
			.TargetBaseRefPresent              = present(epochRoot / "base"),
			.TargetHeadRefPresent              = present(epochRoot / "head"),
			.NonTargetBaseRefPresent           = present(nonTargetEpochRoot / "base"),
			.NonTargetHeadRefPresent           = present(nonTargetEpochRoot / "head"),
			.RepositoryPresent                 = present(repository),
			.CacheDeletionMarkerPresent        = present(deletionMarker),
			.TargetBaseCacheDeleted            = baseCacheDeleted,
			.TargetHeadCacheDeleted            = headCacheDeleted,
			.RetiredRepositoryPresent          = retiredRepositoryPresent,
			.RetiredCacheDeletionMarkerPresent = retiredMarkerPresent,
			.TargetBaseAvailability            = baseAvailability,
			.TargetHeadAvailability            = headAvailability,
		};
	}
#endif
}
