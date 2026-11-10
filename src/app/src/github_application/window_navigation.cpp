#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/cached_review_overview_store.hpp"
#include "bpr/application/device_authorization_application_host.hpp"
#include "bpr/application/diagnostic_log_port.hpp"
#include "bpr/application/git_cache_port.hpp"
#include "bpr/application/git_smart_transport_port.hpp"
#include "bpr/application/github_provider_port.hpp"
#include "bpr/application/public_preview_projection.hpp"
#include "bpr/application/reference_page.hpp"
#include "bpr/application/window_review_restoration_host.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/github_window_read.hpp"
#include "bpr/core/identity_avatar_picture.hpp"
#include "bpr/core/markdown_image.hpp"
#include "bpr/core/pull_request_activity.hpp"
#include "bpr/core/reference_page.hpp"
#include "bpr/core/refresh_feedback.hpp"
#include "bpr/core/release_maintenance.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/event_loop_executor.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/workbench_strip.hpp"
#include "terminal_diagnostics.hpp"

#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QObject>
#include <QTextStream>

#include <algorithm>
#include <expected>
#include <stdexcept>
#include <string>
#include <variant>

#if defined(BPR_BUILD_TESTING)
	#include "bpr/qt/settings_window.hpp"
#endif

namespace bpr::app {
	void github_application::implementation::publish_recent()
	{
		if (Closing || !Window || !Profile->ReviewRestoration) {
			return;
		}
		// Signed out, the public entries only; an account's entries once
		// that account's session exists. Resume needs the exact session.
		const auto& session    = Host->accepted_session();
		const auto* profile    = session && Host->state().Account == session->Viewer.Account ? find_profile(session->Profile) : nullptr;
		const auto  hostname   = profile ? std::string{ profile->hostname() } : std::string{ };
		const auto  authority  = core::window_review_authority{ session ? session->Mode : core::github_access_mode::authenticated, hostname };
		auto        projection = application::recent_reviews_projection{ };
		auto        map        = std::vector<std::size_t>{ };
		const auto& recent     = Profile->ReviewRestoration->recent();
		for (std::size_t index = 0; index < recent.size(); ++index) {
			const auto& entry   = recent[index];
			const auto  matches = session && profile && core::matches_review_authority(entry.Destination, authority, session->Viewer.Account);
			if (entry.Destination.Mode != core::github_access_mode::public_preview && !matches) {
				continue;
			}
			const auto resumable = entry.Position.has_value() && matches;
			// [[BPR-1365]]. The entry this window is reading, so Home can tell
			// the open review from a selected row.
			const auto open = ReadingDestination.has_value() && entry.Destination == *ReadingDestination;
			projection.Entries.push_back({ entry.Destination, entry.Title, entry.ViewedAt, resumable, open, entry.Draft });
			map.push_back(index);
		}
		RecentIndexMap = std::move(map);
		if (RecentPublished == projection) {
			return;
		}
		RecentPublished = projection;
		Window->publish(projection);
	}

	/// [[BPR-1338]]. The page of a pull request the window names, built from
	/// the accepted session's profile hostname and never from row text. A
	/// Home queue row must be the accepted account's on that host; a
	/// Recently viewed row must be one this window published, on that host,
	/// and an account's row that account's.
	auto github_application::implementation::open_pull_request_page(const application::pull_request_page_intent& aIntent) -> core::markdown_link_result
	{
		if (const auto* const context = std::get_if<core::markdown_context>(&aIntent)) {
			return WindowRead->open_pull_request_page(*context, Profile->WindowBrowser);
		}
		const auto& session = Host->accepted_session();
		const auto* profile = session && Host->state().Account == session->Viewer.Account ? find_profile(session->Profile) : nullptr;
		if (profile == nullptr) {
			return core::markdown_link_result::unavailable;
		}
		const auto hostname = std::string{ profile->hostname() };
		const auto browser  = Profile->WindowBrowser;
		auto       address  = std::expected<core::pull_request_page, core::markdown_link_result>{ std::unexpected(core::markdown_link_result::refused) };
		if (const auto* const ref = std::get_if<core::pull_request_ref>(&aIntent)) {
			if (session->Mode != core::github_access_mode::authenticated) {
				return core::markdown_link_result::unavailable;
			}
			address = core::admit_queue_pull_request_page(*ref, hostname, session->Viewer.Login, browser != nullptr);
		} else {
			const auto row = std::get<application::recent_review_row>(aIntent).Index;
			if (!Profile->ReviewRestoration || !RecentPublished || row >= RecentIndexMap.size() || row >= RecentPublished->Entries.size()) {
				return core::markdown_link_result::stale;
			}
			const auto  index  = RecentIndexMap[row];
			const auto& recent = Profile->ReviewRestoration->recent();
			if (index >= recent.size() || recent[index].Destination != RecentPublished->Entries[row].Destination) {
				return core::markdown_link_result::stale;
			}
			address = core::admit_recent_pull_request_page(recent[index].Destination, core::window_review_authority{ session->Mode, hostname }, session->Viewer.Account, browser != nullptr);
		}
		if (!address) {
			return address.error();
		}
		try {
			return browser->open(application::pull_request_page_address(*address)) ? core::markdown_link_result::opened : core::markdown_link_result::failed;
		} catch (...) {
			return core::markdown_link_result::failed;
		}
	}

	/// [[BPR-1342]]. Open Review in New Window. The row is admitted here, by
	/// the rules its Open and its GitHub page already follow, before the
	/// profile is asked for a window: a Home queue row only as the accepted
	/// account's on the accepted host, a Recently viewed row only as one
	/// this window published, and an account's reading only under that
	/// account's exact authority. The new window is the profile's ordinary
	/// New Window; it always opens, even where this or another window shows
	/// the same pull request (RFC-BPR-0024).
	auto github_application::implementation::request_review_window(const application::review_window_intent& aIntent) -> bool
	{
		if (Closing || !Profile->ReviewWindowRequest || !Profile->current_window(DraftWindow)) {
			return false;
		}
		const auto& session     = Host->accepted_session();
		const auto* profile     = session && Host->state().Account == session->Viewer.Account ? find_profile(session->Profile) : nullptr;
		auto        destination = std::expected<core::window_review_destination, core::markdown_link_result>{ std::unexpected(core::markdown_link_result::refused) };
		if (const auto* const ref = std::get_if<core::pull_request_ref>(&aIntent)) {
			if (profile == nullptr || session->Mode != core::github_access_mode::authenticated) {
				return false;
			}
			destination = core::admit_queue_review_window(*ref, profile->hostname(), session->Viewer.Login, session->Viewer.Account);
		} else {
			const auto row = std::get<application::recent_review_row>(aIntent).Index;
			if (!Profile->ReviewRestoration || !RecentPublished || row >= RecentIndexMap.size() || row >= RecentPublished->Entries.size()) {
				return false;
			}
			const auto  index  = RecentIndexMap[row];
			const auto& recent = Profile->ReviewRestoration->recent();
			if (index >= recent.size() || recent[index].Destination != RecentPublished->Entries[row].Destination) {
				return false;
			}
			auto authority = std::optional<core::window_review_authority>{ };
			if (profile != nullptr) {
				authority = core::window_review_authority{ session->Mode, std::string{ profile->hostname() } };
			}
			destination = core::admit_recent_review_window(recent[index].Destination, authority, profile != nullptr ? session->Viewer.Account : core::github_account_id{ });
		}
		return destination.has_value() && Profile->ReviewWindowRequest(*destination);
	}

	/// [[BPR-1342]]. The fresh window opens what its New Window carried, by
	/// the bar's path, as Recently viewed's Open does: a signed-in account's
	/// pull request only under that account's exact session, a public
	/// reading on its own host. Its parts are admitted again here.
	void github_application::implementation::open_review_destination(const core::window_review_destination& aDestination)
	{
		if (Closing || !SetupDestination || !Profile->current_window(DraftWindow)) {
			return;
		}
		const auto page = core::admit_pull_request_page({ .BrowserAvailable = true,
														  .Hostname         = aDestination.Hostname,
														  .Owner            = aDestination.Repository.Owner,
														  .Repository       = aDestination.Repository.Name,
														  .Number           = aDestination.Number });
		if (!page) {
			return;
		}
		if (aDestination.Mode != core::github_access_mode::public_preview) {
			const auto& session = Host->accepted_session();
			const auto* profile = session && Host->state().Account == session->Viewer.Account ? find_profile(session->Profile) : nullptr;
			if (profile == nullptr
				|| !core::matches_review_authority(aDestination, core::window_review_authority{ session->Mode, std::string{ profile->hostname() } }, session->Viewer.Account)) {
				return;
			}
		}
		retire_setup_effects();
		cancel_restoration(true);
		SetupDestination->submit(application::pull_request_page_address(*page));
	}

	/// A Recently viewed row's Remove: only a row this window published,
	/// still naming the entry it was published for.
	void github_application::implementation::forget_recent(application::recent_review_row aRow)
	{
		if (Closing || !Profile->ReviewRestoration || !RecentPublished || aRow.Index >= RecentIndexMap.size()
			|| aRow.Index >= RecentPublished->Entries.size()) {
			return;
		}
		const auto  index  = RecentIndexMap[aRow.Index];
		const auto& recent = Profile->ReviewRestoration->recent();
		if (index >= recent.size() || recent[index].Destination != RecentPublished->Entries[aRow.Index].Destination) {
			return;
		}
		Profile->ReviewRestoration->forget_recent(RecentPublished->Entries[aRow.Index].Destination);
	}

	void github_application::implementation::open_recent(application::recent_review_intent aIntent)
	{
		if (Closing || !Profile->ReviewRestoration || aIntent.Index >= RecentIndexMap.size()) {
			return;
		}
		const auto  index  = RecentIndexMap[aIntent.Index];
		const auto& recent = Profile->ReviewRestoration->recent();
		if (index >= recent.size() || !RecentPublished || aIntent.Index >= RecentPublished->Entries.size()
			|| recent[index].Destination != RecentPublished->Entries[aIntent.Index].Destination) {
			return;
		}
		const auto& active        = Host->accepted_session();
		const auto* activeProfile = active && Host->state().Account == active->Viewer.Account ? find_profile(active->Profile) : nullptr;
		const auto  matches       = active && activeProfile && core::matches_review_authority(recent[index].Destination, core::window_review_authority{ active->Mode, std::string{ activeProfile->hostname() } }, active->Viewer.Account);
		if ((aIntent.Resume || recent[index].Destination.Mode != core::github_access_mode::public_preview) && !matches) {
			return;
		}
		if (aIntent.Resume && recent[index].Position) {
			// The same exact-position course a restart used to run by
			// itself; a gesture starts it now.
			cancel_restoration(false);
			Profile->ReviewRestoration->open_recent(DraftWindow, index);
			return;
		}
		// Open on Overview by the bar's path: it serves a signed-in
		// account and a signed-out public read alike (BPR-0732).
		if (!SetupDestination) {
			return;
		}
		const auto& destination = recent[index].Destination;
		retire_setup_effects();
		cancel_restoration(true);
		// [[BPR-1475]]. Even the pull request this window already shows is
		// read fresh: the owner chose a read per reopen over a stale view
		// (2026-10-13), replacing BPR-0987's reuse here.
		SetupDestination->submit("https://" + destination.Hostname + "/" + destination.Repository.Owner + "/" + destination.Repository.Name + "/pull/" + std::to_string(destination.Number.value()));
	}

	void github_application::implementation::schedule_reading_restoration()
	{
		if (Closing || !ReadingRestoration || ReadingAdvanceQueued) {
			return;
		}
		ReadingAdvanceQueued = true;
		DecisionLane.post([this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
			if (weak.expired()) {
				return;
			}
			ReadingAdvanceQueued = false;
			advance_reading_restoration();
		});
	}

	void github_application::implementation::finish_reading_restoration(bool aSuccess)
	{
		if (!PendingRestoration) {
			return;
		}
		const auto pending        = *PendingRestoration;
		ReadingRestoreUnavailable = !aSuccess;
		ReadingRestoration.reset();
		PendingRestoration.reset();
		ReadingComparisonRequest.reset();
		RestorationReadRequest.reset();
		if (aSuccess) {
			AcceptedReadingPosition = pending.Position;
			restoration_notice({ });
			// Resume returns the saved file in Review, in the arrangement
			// it last had ([[BPR-1295]]). Leaving it on Overview would
			// immediately replace an older tab's pair with the window's.
			if (pending.Position && pending.Position->File) {
				TabModality = core::workbench_modality::review;
				Window->choose_modality(qt::review_modality::review);
			}
			// [[BPR-1132]]. Resume brings the tabs back once the active
			// tab's reading is on screen; its load completes from there.
			if (pending.Tabs.has_value() && TabHost != nullptr) {
				abandon_tab_load();
				ObservedTabFile = WorkbenchHost->selected();
				TabHost->restore(*pending.Tabs);
				if (pending.Tabs->Tabs.empty()) {
					clear_diff_selection();
				}
			}
			schedule_tab_presentation();
		} else {
			clear_diff_selection();
			Window->choose_home(true);
		}
		Profile->ReviewRestoration->complete_restore(DraftWindow, pending.Request,
													 aSuccess ? std::expected<void, core::window_review_failure>{ } : std::unexpected(core::window_review_failure::unavailable));
		retain_filters();
	}

	void github_application::implementation::advance_reading_restoration()
	{
		if (Closing || !ReadingRestoration || !PendingRestoration || !RestorationReadAccepted
			|| !Profile->ReviewRestoration->current_restore(DraftWindow, PendingRestoration->Request)) {
			return;
		}
		const auto& position    = ReadingRestoration->position();
		const auto* surface     = Window->comparison_surface();
		const auto& publication = SelectedDiffPublication;
		const auto  diffReady   = publication && position.File == publication->Request.File.File
			&& publication->Request.Diff.Comparison == position.Comparison && surface->current_result()
			&& surface->current_context() && surface->current_context()->Key == publication->Key;
		const auto action = ReadingRestoration->advance(EpochHost->state(), { .FilesReady = WorkbenchHost->state().Navigation.has_value(), .FileAvailable = position.File && WorkbenchHost->contains_file(*position.File), .Failed = ReadingFailed, .DiffReady = diffReady, .SemanticIndex = diffReady ? surface->current_semantic_index().get() : nullptr });
		switch (action) {
		case core::window_reading_restore_action::select_comparison:
			clear_diff_selection();
			EpochHost->dispatch(core::select_comparison_anchors_intent{ position.Comparison.Left.Identity, position.Comparison.Right.Identity, EpochHost->state().Catalog->Revision });
			ReadingComparisonRequest = EpochHost->state().PendingComparison;
			schedule_reading_restoration();
			break;
		case core::window_reading_restore_action::select_file:
			if (WorkbenchHost->selected() == position.File) {
				WorkbenchHost->refresh_selected_diff();
			} else {
				WorkbenchHost->apply(core::select_workbench_file_intent{ *position.File });
			}
			schedule_reading_restoration();
			break;
		case core::window_reading_restore_action::select_location:
			finish_reading_restoration(Window->comparison_surface()->restore_selection(*position.Selection));
			break;
		case core::window_reading_restore_action::complete:    finish_reading_restoration(true); break;
		case core::window_reading_restore_action::unavailable: finish_reading_restoration(false); break;
		case core::window_reading_restore_action::wait:        break;
		}
	}

	/// \brief Whether the modality shows a tab (Review) or the window's
	/// comparison (Overview and Discuss, which have no tab).
	auto github_application::implementation::tab_modality(const core::workbench_modality aModality) noexcept -> bool
	{
		return aModality == core::workbench_modality::review;
	}

	auto github_application::implementation::tab_view_presented(const core::window_review_position& aView) const -> bool
	{
		const auto& epoch   = EpochHost->state();
		const auto* surface = Window->comparison_surface();
		return !epoch.PendingComparison && epoch.AcceptedComparison == std::optional{ aView.Comparison }
		&& aView.File.has_value() && WorkbenchHost->selected() == aView.File
			&& SelectedDiffPublication && SelectedDiffPublication->Request.File.File == aView.File
			&& SelectedDiffPublication->Request.Diff.Comparison == aView.Comparison
			&& surface->current_result() && surface->current_context()
			&& surface->current_context()->Key == SelectedDiffPublication->Key
			&& (!aView.Selection || surface->selection() == aView.Selection);
	}

	auto github_application::implementation::pr_search_scope() -> std::optional<core::source_search_scope>
	{
		if (Closing || !LastAcceptedHandoff || !EpochHost || !TabHost || !EpochHost->state().AcceptedComparison) {
			return { };
		}
		const auto& tabs   = TabHost->state();
		const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		const auto  pair   = active ? active->View.Comparison : tabs.WindowComparison.value_or(*EpochHost->state().AcceptedComparison);
		// A reviewer's comparison gesture retires hits before its asynchronous
		// exact pair is reflected into the tab. Ordinary file loads do not.
		if (SearchComparisonGesture) {
			if (EpochHost->state().PendingComparison || pair != *EpochHost->state().AcceptedComparison) {
				return { };
			}
			SearchComparisonGesture = false;
		}
		if (SearchCreatedTab && (tabs.Active != SearchCreatedTab || !SearchCreatedComparison || *SearchCreatedComparison != pair)) {
			SearchCreatedTab.reset();
			SearchCreatedComparison.reset();
		}
		// [[BPR-1712]]. While a result opens, and while the tab it
		// opened is active, the search keeps the tab it was bound to, so
		// the results and the result's reveal stay.
		auto bound = tabs.Active;
		if (SearchOpening) {
			bound = SearchOpeningBinding;
		} else if (SearchCreatedTab) {
			bound = SearchCreatedBinding;
		}
		return EpochHost->bind_source_search(workbench_authority(*LastAcceptedHandoff), pair, bound,
											 pair == *EpochHost->state().AcceptedComparison ? filtered_search_files() : std::nullopt);
	}

	/// \brief RFC-BPR-0089 ([[BPR-1688]]). While a filter leaves files out of
	/// the review, Find in files searches the files it lists, each at its
	/// filtered diff's commits (a split file from its first diff's left to
	/// its last diff's right); files set aside as generated are not searched.
	auto github_application::implementation::filtered_search_files() const -> std::optional<std::vector<core::source_search_filtered_file>>
	{
		const auto files = CommitFilter.files();
		if (!files) {
			return std::nullopt;
		}
		auto result = std::vector<core::source_search_filtered_file>{ };
		result.reserve(files->Files.size());
		for (const auto& file : files->Files) {
			auto entry = core::source_search_filtered_file{ .Path = file.Record.Path, .Endpoints = file.Filter };
			if (file.Split && !file.Segments.empty()) {
				entry.Endpoints = core::filtered_diff_endpoints{ .Left = file.Segments.front().Endpoints.Left, .Right = file.Segments.back().Endpoints.Right };
			}
			result.push_back(std::move(entry));
		}
		std::ranges::sort(result, { }, &core::source_search_filtered_file::Path);
		result.erase(std::ranges::unique(result, { }, &core::source_search_filtered_file::Path).begin(), result.end());
		return result;
	}

	/// \brief [[BPR-1712]]. Find in files' Open in New Tab: the
	/// result's file in a tab of its own after the active one, which it
	/// then makes active so the result's line can be shown. The core's
	/// Open in New Tab and tab activation decide both; at the tab limit
	/// nothing opens.
	void github_application::implementation::open_search_result_in_new_tab(const core::file_identity& aFile)
	{
		auto before = std::vector<core::document_tab_id>{ };
		for (const auto& tab : TabHost->state().Tabs) {
			before.push_back(tab.Id);
		}
		const auto active = TabHost->state().Active;
		TabHost->apply(core::open_in_new_tab_intent{ .File = aFile });
		if (!active) {
			return;
		}
		const auto& tabs  = TabHost->state().Tabs;
		const auto  added = std::ranges::find_if(tabs, [&](const core::document_tab& aTab) { return std::ranges::find(before, aTab.Id) == before.end(); });
		if (added != tabs.end()) {
			TabHost->apply(core::activate_tab_intent{ .Tab = added->Id });
		}
	}

	auto github_application::implementation::register_search_open() -> bool
	{
		if (!SearchOpenFile || !LastAcceptedHandoff || !WorkbenchHost || !EpochHost || !TabHost) {
			return false;
		}
		const auto& [scope, entry] = *SearchOpenFile;
		const auto context         = EpochHost->context(workbench_authority(*LastAcceptedHandoff));
		if (!context || context->Authority != scope.Authority || context->Comparison != scope.Comparison) {
			return false;
		}
		auto file             = application::workbench_file_observation_entry{ };
		file.File             = entry.File;
		file.PresentationPath = entry.path();
		file.CurrentPath      = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(entry.path().data()), entry.path().size() } };
		file.ContentKind      = application::workbench_file_content_kind::text;
		switch (entry.Change) {
		case 'A': file.Change = application::workbench_file_change_kind::added; break;
		case 'D': file.Change = application::workbench_file_change_kind::removed; break;
		case 'R': file.Change = application::workbench_file_change_kind::renamed; break;
		case 'C': file.Change = application::workbench_file_change_kind::copied; break;
		case 'T': file.Change = application::workbench_file_change_kind::type_changed; break;
		default:  file.Change = application::workbench_file_change_kind::modified; break;
		}
		if (entry.Change == 'R' || entry.Change == 'C') {
			if (!entry.Left || !entry.Right) {
				return false;
			}
			file.PreviousPath = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(entry.Left->Path.data()), entry.Left->Path.size() } };
		}
		auto tabs = std::vector<core::window_review_position>{ };
		for (const auto& tab : TabHost->state().Tabs) {
			tabs.push_back(tab.View);
		}
		const auto retained = WorkbenchHost->retain_source_file(*context, std::move(file), tabs);
		if (retained) {
			SearchOpenFile.reset();
		}
		return retained;
	}

	/// \brief [[BPR-1693]]. A set-aside file's entry, retained for its
	/// diff outside the walk as a search result is; none while no exact
	/// comparison context holds it.
	auto github_application::implementation::register_set_aside_open() -> bool
	{
		if (!SetAsideOpenFile || !LastAcceptedHandoff || !WorkbenchHost || !EpochHost || !TabHost) {
			return false;
		}
		const auto context = EpochHost->context(workbench_authority(*LastAcceptedHandoff));
		if (!context) {
			return false;
		}
		auto tabs = std::vector<core::window_review_position>{ };
		for (const auto& tab : TabHost->state().Tabs) {
			tabs.push_back(tab.View);
		}
		const auto retained = WorkbenchHost->retain_source_file(*context, *SetAsideOpenFile, tabs);
		if (retained) {
			SetAsideOpenFile.reset();
		}
		return retained;
	}

	/// \brief [[BPR-1693]]. Shows a generated file's diff on request from
	/// the generated row. The file stays set aside: it is not added to the
	/// walk, the counts or preparation, and no mark is written.
	void github_application::implementation::open_set_aside_file(const std::string& aPath)
	{
		if (Closing || WorkbenchHost == nullptr || TabHost == nullptr || EpochHost == nullptr || !LastAcceptedHandoff) {
			return;
		}
		const auto files = CommitFilter.files();
		if (!files) {
			return;
		}
		const auto found = std::ranges::find(files->SetAside, aPath, [](const application::commit_filter_file& aFile) -> const std::string& { return aFile.Record.Path; });
		if (found == files->SetAside.end()) {
			return;
		}
		auto entry = application::workbench_file_entry(found->Record);
		if (!entry) {
			return;
		}
		entry->Filter        = found->Filter;
		entry->SplitByFilter = found->Split;
		entry->Segments      = application::workbench_segments(found->Segments);
		if (!application::valid(*entry)) {
			return;
		}
		cancel_review_navigation();
		cancel_restoration(false);
		abandon_tab_load();
		AutoSeenPresentedRequest.reset();
		const auto file  = entry->File;
		SetAsideOpenFile = std::move(*entry);
		static_cast<void>(register_set_aside_open());
		observe_tab_reading();
		TabHost->apply(core::open_in_active_tab_intent{ .File = file });
	}

	void github_application::implementation::present_tab(const core::document_tab_view_request& aRequest)
	{
		if (Closing || TabHost == nullptr || EpochHost == nullptr || WorkbenchHost == nullptr) {
			return;
		}
		TabLoad   = aRequest;
		TabFailed = false;
		// A load the reviewer's own gesture already put on screen, as a
		// choice of file or of comparison is, completes at once.
		if (!ReadingRestoration && tab_view_presented(aRequest.View)) {
			TabDriver.reset();
			finish_tab_load(core::document_tab_view_outcome::ready);
			return;
		}
		TabDriver.emplace(aRequest.View);
		schedule_tab_presentation();
	}

	/// \brief [[BPR-1132]]. Another pull request: its tabs start again.
	///
	/// \details [[BPR-1539]]. Kept tabs name the previous pull request's
	/// comparison, which the core never admits for this one, so every
	/// view they asked for was unavailable and the file and change arrows
	/// refused each press while the files tree still chose files.
	void github_application::implementation::adopt_tab_destination(const core::window_review_destination& aDestination)
	{
		if (TabDestination.has_value() && *TabDestination != aDestination && TabHost != nullptr) {
			cancel_review_navigation();
			abandon_tab_load();
			TabPresented.reset();
			ObservedTabFile.reset();
			TabHost->reset();
		}
		TabDestination = aDestination;
	}

	/// \brief [[BPR-1573]]. Close review on Home.
	///
	/// \details The core forgets this window's pull request; the read host
	/// stops its read and invalidates the epoch authority, which releases
	/// the review through `invalidate`, as opening another pull request
	/// does, so its drafts are kept where that keeps them. The tabs were
	/// the closed review's and start again, as for another pull request
	/// ([[BPR-1539]]). Other windows keep their own reads.
	void github_application::implementation::close_review()
	{
		if (Closing || !WindowRead || !Profile->current_window(DraftWindow)) {
			return;
		}
		cancel_restoration(false, true);
		WindowRead->dispatch(core::github_window_read_event{ core::close_pull_request_intent{ } });
		const auto& state = WindowRead->state();
		if (state.ActiveLocator || state.PendingPullRequest || state.PullRequest) {
			return;
		}
		if (TabHost != nullptr) {
			cancel_review_navigation();
			abandon_tab_load();
			TabPresented.reset();
			ObservedTabFile.reset();
			TabHost->reset();
		}
		TabDestination.reset();
	}

	void github_application::implementation::cancel_review_navigation()
	{
		PendingReviewNavigation.reset();
		ReviewNavigationFocus.reset();
	}

	void github_application::implementation::finish_review_navigation_focus()
	{
		const auto focus = std::exchange(ReviewNavigationFocus, std::nullopt);
		if (focus && !Closing && LastAcceptedHandoff && WorkbenchHost
			&& workbench_authority(*LastAcceptedHandoff) == focus->Authority
			&& WorkbenchHost->generation() == focus->Generation) {
			Window->focus_review_navigation(focus->Command);
		}
	}

	void github_application::implementation::continue_review_navigation()
	{
		if (!PendingReviewNavigation) {
			return;
		}
		const auto& pending = *PendingReviewNavigation;
		const auto* active  = TabHost && TabHost->state().Active ? core::find_tab(TabHost->state(), *TabHost->state().Active) : nullptr;
		if (Closing || !LastAcceptedHandoff || !TabHost || !WorkbenchHost
			|| Window->showing_home() || Window->showing_local_cycle()
			|| WorkbenchHost->state().Modality != core::workbench_modality::review
			|| workbench_authority(*LastAcceptedHandoff) != pending.Authority
			|| WorkbenchHost->generation() != pending.Generation || TabHost->state().Active != pending.Tab
			|| (active != nullptr) != pending.View.has_value()
			|| (active && (active->View.File != pending.View->File || active->View.Comparison != pending.View->Comparison))) {
			cancel_review_navigation();
			return;
		}
		if (TabLoad || TabDriver || TabModality != core::workbench_modality::review || EpochHost->state().PendingComparison) {
			return;
		}
		// The press is bound to its tab's file and exact pair, not to the
		// reading position it saw: a selection made just before the press
		// reaches the tab a lane turn later, and the stale position would
		// never be on screen again, holding the press forever.
		// Ordinary file navigation needs the accepted file order, not source
		// material or a matching line selection. A terminal source failure
		// must not trap its arrows while the tree can still choose a file.
		// Entry from another modality still restores the exact reading first.
		const auto fileOrderReady = !pending.RestoreReading && active
			&& WorkbenchHost->selected() == active->View.File
			&& EpochHost->state().AcceptedComparison == std::optional{ active->View.Comparison };
		if (active && !fileOrderReady && !tab_view_presented(active->View)) {
			// An unavailable exact view keeps its normal explanation and never
			// lends another comparison to the retained navigation command.
			if (active->State == core::document_tab_view_state::unavailable) {
				const auto command = pending.Command;
				PendingReviewNavigation.reset();
				Window->refuse_review_navigation(command);
			}
			return;
		}
		const auto focus = pending;
		PendingReviewNavigation.reset();
		Window->perform_review_navigation(focus.Command);
		if (focus.Command == core::workbench_command_id::previous_file || focus.Command == core::workbench_command_id::next_file) {
			ReviewNavigationFocus = focus;
		}
	}

	void github_application::implementation::request_review_navigation(const std::optional<core::workbench_command_id> aCommand)
	{
		if (PrFind) {
			PrFind->cancel_reveal();
		}
		cancel_review_navigation();
		if (!aCommand || Closing || !TabHost || !WorkbenchHost || !EpochHost || !LastAcceptedHandoff) {
			return;
		}
		const auto restoreReading = TabModality != core::workbench_modality::review
			|| (*aCommand != core::workbench_command_id::previous_file && *aCommand != core::workbench_command_id::next_file);
		// Start the ordinary modality/tab driver before interpreting the
		// command, and reflect a preceding tree choice before binding it.
		// Otherwise that queued reflection cancels this later press as stale.
		advance_tab_presentation();
		const auto* active      = TabHost->state().Active ? core::find_tab(TabHost->state(), *TabHost->state().Active) : nullptr;
		PendingReviewNavigation = pending_review_navigation{ *aCommand, workbench_authority(*LastAcceptedHandoff), WorkbenchHost->generation(), TabHost->state().Active, active ? std::optional{ active->View } : std::nullopt, restoreReading };
		continue_review_navigation();
	}

	void github_application::implementation::finish_tab_load(const core::document_tab_view_outcome aOutcome, const bool aAcceptReading)
	{
		const auto request = std::exchange(TabLoad, std::nullopt);
		TabDriver.reset();
		RevalidatingKeptTabView = false;
		if (!request || TabHost == nullptr) {
			return;
		}
		TabPresented = aOutcome == core::document_tab_view_outcome::ready ? request : std::nullopt;
		if (aAcceptReading) {
			TabHost->complete_with_reading_position_on_lane(request->Request, Window->comparison_surface()->selection());
		} else {
			TabHost->complete_on_lane(request->Request, aOutcome);
		}
		// A failed reading restoration refuses entry navigation. An ordinary
		// file press can still use the now-settled exact file order below.
		if (aOutcome == core::document_tab_view_outcome::unavailable
			&& (!PendingReviewNavigation || PendingReviewNavigation->RestoreReading)) {
			const auto command = PendingReviewNavigation ? std::optional{ PendingReviewNavigation->Command } : std::nullopt;
			cancel_review_navigation();
			if (command) {
				Window->refuse_review_navigation(*command);
			}
		}
		if (aOutcome == core::document_tab_view_outcome::ready) {
			finish_review_navigation_focus();
			resume_reply();
			if (SelectedDiffPublication && ReviewLedgerHost) {
				mark_presented_file_seen(*SelectedDiffPublication, ReviewLedgerHost->retained());
			}
		}
		continue_review_navigation();
	}

	/// \brief A reviewer's gesture during a load: the load in force is
	/// left, and the gesture is what the active tab takes next.
	void github_application::implementation::abandon_tab_load()
	{
		TabDriver.reset();
		TabLoad.reset();
		RevalidatingKeptTabView = false;
	}

	void github_application::implementation::schedule_tab_presentation()
	{
		if (Closing || TabHost == nullptr || TabAdvanceQueued) {
			return;
		}
		TabAdvanceQueued = true;
		DecisionLane.post([this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
			if (weak.expired()) {
				return;
			}
			TabAdvanceQueued = false;
			advance_tab_presentation();
		});
	}

	void github_application::implementation::advance_tab_presentation()
	{
		if (Closing || TabHost == nullptr || EpochHost == nullptr || WorkbenchHost == nullptr || Window == nullptr) {
			return;
		}
		// A Resume in progress owns the comparison and the file until it
		// finishes; the tabs it brings are adopted then.
		if (ReadingRestoration || PendingRestoration) {
			return;
		}
		const auto modality = WorkbenchHost->state().Modality;
		if (const auto previous = std::exchange(TabModality, modality);
			previous.has_value() && tab_modality(*previous) != tab_modality(modality)) {
			// Overview and Discuss have no tab: they show the window's
			// comparison. Back in Review the active tab's exact
			// file, pair and reading position return.
			abandon_tab_load();
			const auto& tabs = TabHost->state();
			if (tab_modality(modality) && tabs.Active) {
				// Returning from a page with no tab restores the complete
				// active view, including its own semantic reading position.
				TabHost->refresh();
				return;
			}
			const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
			const auto  target = tab_modality(modality)
				? (active != nullptr ? std::optional{ active->View.Comparison } : std::nullopt)
				: tabs.WindowComparison;
			if (target.has_value() && EpochHost->state().AcceptedComparison != target && EpochHost->state().Catalog) {
				EpochHost->dispatch(core::select_comparison_anchors_intent{ target->Left.Identity, target->Right.Identity, EpochHost->state().Catalog->Revision });
				return;
			}
		}
		if (TabDriver && TabLoad) {
			drive_tab();
			return;
		}
		observe_tab_changes();
		continue_review_navigation();
		if (SelectedDiffPublication && ReviewLedgerHost) {
			mark_presented_file_seen(*SelectedDiffPublication, ReviewLedgerHost->retained());
		}
	}

	void github_application::implementation::drive_tab()
	{
		static_cast<void>(register_search_open());
		static_cast<void>(register_set_aside_open());
		const auto& position    = TabDriver->position();
		const auto* surface     = Window->comparison_surface();
		const auto& publication = SelectedDiffPublication;
		const auto  diffReady   = publication
			&& publication->Request.Context.Generation == WorkbenchHost->generation()
			&& SelectedDiffRequest == std::optional{ publication->Request }
			&& position.File == publication->Request.File.File
			&& publication->Request.Diff.Comparison == position.Comparison && surface->current_result()
			&& surface->current_context() && surface->current_context()->Key == publication->Key;
		const auto action = TabDriver->advance(EpochHost->state(), { .FilesReady = WorkbenchHost->state().Navigation.has_value(), .FileAvailable = position.File && WorkbenchHost->contains_file(*position.File), .Failed = TabFailed, .DiffReady = diffReady, .SemanticIndex = diffReady ? surface->current_semantic_index().get() : nullptr });
		switch (action) {
		case core::window_reading_restore_action::select_comparison:
			if (EpochHost->state().AcceptedComparison != std::optional{ position.Comparison }) {
				EpochHost->dispatch(core::select_comparison_anchors_intent{ position.Comparison.Left.Identity, position.Comparison.Right.Identity, EpochHost->state().Catalog->Revision });
			}
			schedule_tab_presentation();
			break;
		case core::window_reading_restore_action::select_file:
			if (WorkbenchHost->selected() == position.File) {
				// A file gesture or the epoch's accepted comparison already
				// starts this exact request. Adopt its pending or published
				// work instead of superseding it with a second request.
				if (!SelectedDiffRequest || SelectedDiffRequest->File.File != position.File
					|| SelectedDiffRequest->Diff.Comparison != position.Comparison
					|| SelectedDiffRequest->Context.Generation != WorkbenchHost->generation()
					|| EpochHost->context(SelectedDiffRequest->WorkbenchAuthority) != std::optional{ SelectedDiffRequest->Diff }) {
					WorkbenchHost->refresh_selected_diff();
				}
			} else {
				WorkbenchHost->apply(core::select_workbench_file_intent{ *position.File });
			}
			schedule_tab_presentation();
			break;
		case core::window_reading_restore_action::select_location:
			// [[BPR-1305]]. A same-head refresh can retain this exact
			// selection. Revealing it again would move a viewport the
			// reviewer scrolled independently of the selected line.
			finish_tab_load(Window->comparison_surface()->selection() == position.Selection
									|| Window->comparison_surface()->restore_selection(*position.Selection)
								? core::document_tab_view_outcome::ready
								: core::document_tab_view_outcome::unavailable);
			break;
		case core::window_reading_restore_action::complete:
			finish_tab_load(core::document_tab_view_outcome::ready);
			break;
		case core::window_reading_restore_action::unavailable:

#if defined(BPR_BUILD_TESTING)
			bpr::app::detail::terminal_text_stream{ } << "TAB_RESTORE_UNAVAILABLE:failed=" << TabFailed
													  << " epoch_status=" << static_cast<int>(EpochHost->state().Status)
													  << " pair=" << (EpochHost->state().AcceptedComparison == std::optional{ position.Comparison })
													  << " diff=" << diffReady << " semantic=" << bool(surface->current_semantic_index()) << '\n';
#endif
			finish_tab_load(core::document_tab_view_outcome::unavailable);
			break;
		case core::window_reading_restore_action::wait:
			break;
		}
	}

	/// \brief Reflects the reviewer's own choices into the tabs.
	void github_application::implementation::observe_tab_changes()
	{
		const auto& epoch = EpochHost->state();
		if (epoch.PendingComparison || !epoch.AcceptedComparison || !core::available_for_diff(*epoch.AcceptedComparison)) {
			return;
		}
		const auto& accepted = *epoch.AcceptedComparison;
		const auto  showsTab = tab_modality(WorkbenchHost->state().Modality);
		// With no active tab, an epoch gesture still chooses the window's
		// pair, including in Review. The first file must inherit
		// that accepted pair rather than restore a remembered earlier one.
		if (!TabHost->state().WindowComparison || !TabHost->state().Active || !showsTab) {
			if (TabHost->state().WindowComparison != std::optional{ accepted }) {
				TabHost->apply(core::set_window_comparison_intent{ .Comparison = accepted });
			}
		}
		if (!showsTab) {
			return;
		}
		// [[BPR-1527]]. A first file gesture can precede its accepted
		// comparison. A reread may retire the workbench selection before
		// that pair arrives, so open the core's retained explicit choice
		// once the current catalogue admits it. Review entry alone has none.
		if (const auto& tabs = TabHost->state(); !tabs.Active && tabs.Chosen
			&& WorkbenchHost->state().Navigation && WorkbenchHost->contains_file(*tabs.Chosen)) {
			TabHost->apply(core::open_in_active_tab_intent{ .File = *tabs.Chosen });
			return;
		}
		const auto activeTab = [this]() -> const core::document_tab* {
			const auto& tabs = TabHost->state();
			return tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		};
		if (const auto* active = activeTab(); active != nullptr && active->View.Comparison != accepted) {
			TabHost->apply(core::select_tab_comparison_intent{ .Comparison = accepted });
		}
		if (represent_refreshed_tab()) {
			return;
		}
		const auto selected     = WorkbenchHost->selected();
		const auto previousFile = std::exchange(ObservedTabFile, selected);
		if (const auto* active = activeTab(); selected.has_value()
			&& (active != nullptr ? active->View.File != selected : previousFile != selected)) {
			TabHost->apply(core::open_in_active_tab_intent{ .File = *selected });
		}
		observe_tab_reading();
	}

	/// \brief [[BPR-1303]]. A pull request's refresh observes its files
	/// under the new snapshot's authority, and that observation leaves the
	/// workbench with no selected file. When the refresh kept the head,
	/// Review kept the earlier diff on screen, so a line chosen there drew
	/// no draft and Comment stayed unavailable until the reviewer chose a
	/// file or left Review. Since BPR-1293 keeps Review after Publish,
	/// every inline publish led there. The active tab shows its exact view
	/// again, as it does on the way back from a page with no tab, once for
	/// that refresh.
	auto github_application::implementation::represent_refreshed_tab() -> bool
	{
		const auto& tabs   = TabHost->state();
		const auto* active = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		if (!RefreshKeptTabView || active == nullptr || !active->View.File.has_value() || TabLoad || ReadingRestoration
			|| PendingRestoration || WorkbenchHost->selected().has_value() || !WorkbenchHost->state().Navigation.has_value()
			|| !WorkbenchHost->contains_file(*active->View.File)) {
			return false;
		}
		RefreshKeptTabView      = false;
		RevalidatingKeptTabView = true;
		TabHost->refresh();
		return true;
	}

	// The reading position of the view on screen, for the load that
	// presented it. A tab gesture queues behind this observation.
	void github_application::implementation::observe_tab_reading()
	{
		if (Closing || TabHost == nullptr) {
			return;
		}
		const auto& tabs    = TabHost->state();
		const auto* active  = tabs.Active ? core::find_tab(tabs, *tabs.Active) : nullptr;
		const auto* surface = Window->comparison_surface();
		if (active != nullptr && TabPresented && TabPresented->Tab == active->Id && SelectedDiffPublication
			&& SelectedDiffPublication->Request.File.File == active->View.File
			&& SelectedDiffPublication->Request.Diff.Comparison == active->View.Comparison
			&& surface->current_context() && surface->current_context()->Key == SelectedDiffPublication->Key
			&& surface->selection() != active->View.Selection) {
			TabHost->observe_reading_position_on_lane(TabPresented->Request, surface->selection());
		}
	}

	/// \brief Keeps the tabs with the window's accepted destination.
	void github_application::implementation::retain_tabs(const core::window_review_tabs& aTabs)
	{
		if (Closing || PendingRestoration || ReadingRestoreUnavailable || !ReadingDestination || !Profile->ReviewRestoration) {
			return;
		}
		static_cast<void>(Profile->ReviewRestoration->accepted_tabs(DraftWindow, *ReadingDestination, aTabs));
	}

	void github_application::implementation::retain_reading_position()
	{
		if (Closing || PendingRestoration || ReadingRestoreUnavailable || !ReadingDestination || !EpochHost
			|| !Profile->ReviewRestoration || !EpochHost->state().AcceptedComparison) {
			return;
		}
		const auto& comparison = *EpochHost->state().AcceptedComparison;
		if (!core::available_for_diff(comparison)) {
			return;
		}
		auto position = AcceptedReadingPosition.value_or(core::window_review_position{ comparison, { }, { } });
		if (position.Comparison != comparison) {
			position = { comparison, { }, { } };
		}
		const auto* surface = Window->comparison_surface();
		if (SelectedDiffPublication && SelectedDiffPublication->Request.Diff.Comparison == comparison
			&& surface->current_result() && surface->current_context() && surface->current_context()->Key == SelectedDiffPublication->Key) {
			position.File      = SelectedDiffPublication->Request.File.File;
			position.Selection = surface->selection();
		}
		AcceptedReadingPosition = position;
		Profile->ReviewRestoration->accepted_position(DraftWindow, *ReadingDestination, position);
	}

	void github_application::implementation::publish(const application::workbench_projection& aProjection)
	{
		Window->publish(aProjection);
		if (aProjection.Selection) {
			present_opened_file();
		}
		if (aProjection.Files || aProjection.FilesDelta || aProjection.Selection) {
			schedule_preparation();
		}
		schedule_reading_restoration();
		schedule_tab_presentation();
		schedule_link_location();
	}

	void github_application::implementation::fail(const application::workbench_failure_publication& aFailure)
	{
		// [[BPR-1871]]. "No exact comparison is ready" while the pull
		// request is still loading only means "not yet": the comparison
		// area says the file is loading, and the status line says nothing
		// until the load stops without one.
		const auto refusal = aFailure.Failure == application::workbench_host_failure::selected_file_comparison_unavailable;
		if (refusal && WorkbenchHost != nullptr && WorkbenchHost->selected().has_value()) {
			OpenedFileFailure         = aFailure;
			OpenedFileFailed          = WorkbenchHost->selected();
			OpenedFileFailureWithheld = opened_file_wait() == core::opened_file_wait::loading;
		}
		if (!refusal || !OpenedFileFailureWithheld) {
			Window->fail(aFailure);
		}
		if (ReadingRestoration && aFailure.Failure != application::workbench_host_failure::settings_unavailable) {
			ReadingFailed = true;
			schedule_reading_restoration();
		}
		present_opened_file();
	}

	/// \brief [[BPR-1871]]. The facts the core decides the opened file's
	/// wait from: whether a file is open and on screen, what its last
	/// refusal or failure was, and where the review history stands.
	auto github_application::implementation::opened_file_facts() const -> core::opened_file_wait_facts
	{
		auto facts = core::opened_file_wait_facts{ };
		if (Closing || Window == nullptr || WorkbenchHost == nullptr || EpochHost == nullptr || !WorkbenchHost->selected().has_value()
			|| !WorkbenchHost->file_observation(*WorkbenchHost->selected()).has_value()) {
			return facts;
		}
		const auto  selected = *WorkbenchHost->selected();
		const auto& epoch    = EpochHost->state();
		const auto* surface  = Window->comparison_surface();
		facts.Opened         = true;
		// Its own comparison is loading or shown under its request; the
		// surface says that wait itself.
		facts.Shown = surface != nullptr && surface->current_context().has_value() && SelectedDiffRequest.has_value() && SelectedDiffRequest->File.File == selected;
		// Only this pull request's: a generation is one accepted authority.
		const auto failed = OpenedFileFailure.has_value() && OpenedFileFailed == std::optional{ selected }
			&& OpenedFileFailure->Context.Generation == WorkbenchHost->generation();
		const auto refusal        = failed && OpenedFileFailure->Failure == application::workbench_host_failure::selected_file_comparison_unavailable;
		facts.FileFailed          = failed && !refusal;
		facts.ComparisonRefused   = refusal;
		facts.ComparisonAvailable = epoch.AcceptedComparison.has_value() && core::available_for_diff(*epoch.AcceptedComparison);
		facts.ComparisonPending   = epoch.PendingComparison.has_value();
		facts.Epoch               = epoch.Status;
		return facts;
	}

	auto github_application::implementation::opened_file_wait() const -> core::opened_file_wait
	{
		return core::decide_opened_file_wait(opened_file_facts());
	}

	/// \brief [[BPR-1871]]. The owner, 2026-11-05: "if I try to open a
	/// file while it's still loading the pr the tab shows the "no file
	/// opened" screen instead it should indicate were loading that
	/// file." The comparison area presents the core's decision for the
	/// file the reviewer opened: loading it by name until it shows, or
	/// the sentence that says why it cannot.
	void github_application::implementation::present_opened_file()
	{
		if (Closing || Window == nullptr || WorkbenchHost == nullptr) {
			return;
		}
		const auto facts = opened_file_facts();
		const auto wait  = core::decide_opened_file_wait(facts);
		auto       path  = QString{ };
		if (facts.Opened) {
			if (const auto entry = WorkbenchHost->file_observation(*WorkbenchHost->selected()); entry.has_value()) {
				path = QString::fromStdString(entry->PresentationPath);
			}
		}
		if (wait == core::opened_file_wait::failed && OpenedFileFailureWithheld && OpenedFileFailure.has_value()) {
			// The load stopped without a comparison: now the status line
			// says so too.
			OpenedFileFailureWithheld = false;
			Window->fail(*OpenedFileFailure);
		}
		Window->present_opened_file(wait, path, !facts.ComparisonAvailable,
									facts.FileFailed || facts.ComparisonRefused ? std::optional{ OpenedFileFailure->Failure } : std::nullopt);
	}

	void github_application::implementation::restoration_notice(const QString& aText)
	{
		if (Closing || !Window) {
			return;
		}
		// [[BPR-0771]]. Through the window, the notice's one writer, so a
		// load refusal and a setup stage cannot overwrite it or be
		// overwritten by it.
		Window->present_home_restoration_notice(aText);
	}

	void github_application::implementation::present_agent_workspace_repair()
	{
		if (Closing || !Window) {
			return;
		}
		// [[BPR-0817]]. The status bar says it on Home, which does not show the
		// agent panel. The panel still says why nothing started.
		Window->set_agent_workspaces_unavailable(true, Profile ? Profile->AgentWorkspaceRecoveryReason : QString{ });
		if (Window->agent_panel() != nullptr) {
			// [[BPR-1857]]. The panel says the same reason as the status line.
			Window->agent_panel()->refuse(Window->agent_workspaces_problem_text().toStdString());
		}
	}

	void github_application::implementation::cancel_restoration(bool aStartup, bool aReadingGesture)
	{
		if (aReadingGesture && PrFind) {
			PrFind->cancel_reveal();
		}
		if (!Profile->ReviewRestoration) {
			return;
		}
		Profile->ReviewRestoration->cancel_restore(DraftWindow);
		if (aStartup) {
			Profile->ReviewRestoration->cancel_startup();
		}
		if (PendingRestoration && WindowRead) {
			WindowRead->cancel(application::pull_request_cancel_cause::restoration);
		}
		const auto suppressLateReading = ReadingRestoration.has_value() && !aStartup && !aReadingGesture;
		PendingRestoration.reset();
		ReadingRestoration.reset();
		ReadingRestoreUnavailable = suppressLateReading;
		RestorationReadRequest.reset();
		if (ReadingComparisonRequest && EpochHost) {
			const auto request = *ReadingComparisonRequest;
			ReadingComparisonRequest.reset();
			EpochHost->dispatch(core::cancel_comparison_resolution_intent{ request.Request, request.Generation });
		}
		restoration_notice({ });
	}

	void github_application::implementation::restore_review(const core::window_review_open_request& aRequest)
	{
		if (Closing || aRequest.Context != DraftWindow || !Profile->current_window(DraftWindow)
			|| !Profile->ReviewRestoration || !Profile->ReviewRestoration->current_restore(DraftWindow, aRequest.Request)) {
			return;
		}
		const auto& session = Host->accepted_session();
		const auto* profile = session ? find_profile(session->Profile) : nullptr;
		if (!session || !profile || Host->state().Account != session->Viewer.Account || !core::matches_review_authority(aRequest.Destination, { session->Mode, std::string{ profile->hostname() } }, session->Viewer.Account)
			|| WindowRead->state().ActiveProfile != session->Profile || WindowRead->state().AccessMode != session->Mode
			|| WindowRead->state().IdentityGeneration != Host->accepted_identity_generation()) {
			Profile->ReviewRestoration->complete_restore(DraftWindow, aRequest.Request, std::unexpected(core::window_review_failure::conflicting_authority));
			return;
		}
		PendingRestoration        = aRequest;
		RestoringFilters          = aRequest.Filters ? std::optional{ std::pair{ aRequest.Destination, *aRequest.Filters } } : std::nullopt;
		ReadingRestoration        = aRequest.Position ? std::optional{ core::window_reading_restoration{ *aRequest.Position } } : std::nullopt;
		ReadingFailed             = false;
		ReadingRestoreUnavailable = false;
		RestorationReadAccepted   = false;
		RestorationReadRequest    = WindowRead->state().NextRequest;
		restoration_notice(QStringLiteral("Resuming the saved reading…"));
		WindowRead->dispatch(core::open_pull_request_intent{ .Repository = aRequest.Destination.Repository, .Number = aRequest.Destination.Number });
		if (PendingRestoration && !WindowRead->state().PendingPullRequest && WindowRead->state().NextRequest == *RestorationReadRequest) {
			Profile->ReviewRestoration->complete_restore(DraftWindow, aRequest.Request, std::unexpected(core::window_review_failure::unavailable));
			PendingRestoration.reset();
			RestorationReadRequest.reset();
		}
	}

	void github_application::implementation::observe_restoration_read(const application::github_window_read_update& aUpdate)
	{
		if (!Profile->ReviewRestoration) {
			return;
		}
		const auto& accepted = aUpdate.Transition.AcceptedSnapshot;
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (accepted && snapshot && accepted->Context.ApplicationSession == WindowSession
			&& accepted->Snapshot == snapshot->Snapshot && accepted->Repository == snapshot->Repository.Repository
			&& accepted->PullRequest == snapshot->PullRequest.PullRequest && accepted->Context.Profile == snapshot->Profile
			&& accepted->Context.Mode == snapshot->Mode && accepted->Context.IdentityGeneration == WindowRead->state().IdentityGeneration) {
			const auto* profile = find_profile(snapshot->Profile);
			if (profile && profile->host() == snapshot->Host) {
				const auto destination = core::window_review_destination{ snapshot->Mode, std::string{ profile->hostname() }, { snapshot->Repository.Owner, snapshot->Repository.Name }, snapshot->PullRequest.Number, snapshot->Account };
				if (ReadingDestination != destination) {
					AcceptedReadingPosition.reset();
				}
				adopt_tab_destination(destination);
				const auto opened  = ReadingDestination != destination;
				ReadingDestination = destination;
				Profile->ReviewRestoration->accepted_read(DraftWindow, destination, std::max<qint64>(0, QDateTime::currentSecsSinceEpoch()));
				Profile->ReviewRestoration->accepted_title(destination, std::string_view{ snapshot->PullRequest.Title }.substr(0, core::maximum_recent_review_title));
				Profile->ReviewRestoration->accepted_draft(destination, snapshot->PullRequest.Draft);
				// [[BPR-1688]]. The destination may be known after its review.
				retain_filters();
				// [[BPR-1365]]. Which recent entry is open moved with it.
				if (opened) {
					publish_recent();
				}
				if (PendingRestoration && RestorationReadRequest == accepted->Context.Request
					&& destination == PendingRestoration->Destination && Profile->ReviewRestoration->current_restore(DraftWindow, PendingRestoration->Request)) {
					RestorationReadAccepted = true;
					if (ReadingRestoration) {
						schedule_reading_restoration();
					} else {
						finish_reading_restoration(true);
					}
				}
			}
		} else if (PendingRestoration && RestorationReadRequest && aUpdate.Transition.Status
				   && !WindowRead->state().PendingPullRequest && WindowRead->state().NextRequest.value() == RestorationReadRequest->value() + 1U
				   && (aUpdate.Transition.Status->Status == core::github_status::pull_request_failed || aUpdate.Transition.Status->Status == core::github_status::pull_request_cancelled)) {
			Profile->ReviewRestoration->complete_restore(DraftWindow, PendingRestoration->Request, std::unexpected(core::window_review_failure::unavailable));
			PendingRestoration.reset();
			ReadingRestoration.reset();
			RestorationReadRequest.reset();
		}
	}

	void github_application::implementation::start()
	{
		ReadDecision = std::make_unique<window_read_decision_lane>(DecisionLane);
		if (!Profile->CachedOverviews) {
			Profile->CachedOverviews = adapters::sqlite::make_cached_review_overview_store(Profile->ApplicationDataRoot);
		}
		auto weak = std::weak_ptr<const int>{ WindowLifetime };
		// [[BPR-1357]]. The application's clock for Home's wait: one bounded
		// single shot on the window's own thread, which is the decision
		// lane's. The host drops an elapse for a read the core no longer shows.
		HomeOpen = std::make_unique<application::home_open_host>(
			[this, weak](const std::uint32_t aMilliseconds, std::function<void()> aElapsed) {
				if (weak.expired() || Closing || !Window) {
					return;
				}
				QTimer::singleShot(static_cast<int>(aMilliseconds), Window.get(), [weak, elapsed = std::move(aElapsed)] {
					if (!weak.expired()) {
						elapsed();
					}
				});
			},
			[this, weak](const application::home_open_projection& aProjection) {
				if (!weak.expired() && !Closing && Window) {
					Window->present(aProjection);
				}
			});
		Window->set_home_open_dispatch([this, weak](const application::home_open_intent& aIntent) {
			if (weak.expired() || Closing || !WindowRead || !HomeOpen) {
				return;
			}
			if (const auto* const cancel = std::get_if<application::cancel_home_open>(&aIntent)) {
				// Admitted against the core's state: the read Home shows, still
				// the lane's pending one.
				if (HomeOpen->admit_cancel(cancel->Request, WindowRead->state())) {
					WindowRead->cancel(application::pull_request_cancel_cause::reviewer);
				}
				return;
			}
			if (const auto retry = HomeOpen->admit_retry(std::get<application::retry_home_open>(aIntent).Request)) {
				// The same path a row takes, so a Retry is an ordinary open.
				dispatch_intent(application::github_intent{ *retry });
			}
		});
		WindowRead = std::make_shared<application::github_window_read_host>(DraftWindow, WindowSession, *Provider, *ReadDecision, [this, weak](const application::github_window_read_update& aUpdate) {
				if (weak.expired() || Closing || aUpdate.Window != DraftWindow || !Profile->current_window(DraftWindow)){ return;
}
// [[BPR-1475]]. Whether a note was open in the drawer as this read began:
// the read's conversations close the drawer before its composition is retired.
if (DraftReadRequest != WindowRead->state().PendingPullRequest) {
DraftReadRequest = WindowRead->state().PendingPullRequest;
if (DraftReadRequest) {
	DraftOpenAtRead = Window->conversation_panel() != nullptr && Window->conversation_panel()->cards() != nullptr && Window->conversation_panel()->cards()->writing_draft();
}
}
if (QueueHost && WindowRead->state().PendingPullRequest && QueueFactRequest != WindowRead->state().PendingPullRequest) {
    QueueFactRequest = WindowRead->state().PendingPullRequest; QueueFactRevision = QueueHost->begin_observation();
}
// [[BPR-1851]]. A refresh's conversation laid over the accepted snapshot
// changes the bodies and threads these were taken from, under the same snapshot.
if (aUpdate.Transition.AcceptedSnapshot.has_value() || aUpdate.Presentation.ConversationRefreshed) {
ReadStateCache.reset();
NotePaths.reset();
DefectThreadNames.reset();
}
				publish_review_conversations(aUpdate.Presentation);
				// [[BPR-1357]]. In the same turn as the transition, so the row or
				// bar that asked shows it before control returns to the event loop.
				if (HomeOpen) {
					HomeOpen->observe(aUpdate.Transition, WindowRead->state());
				}
				// In the same turn too: a read this window now waits on goes
				// ahead of Home's loading (`core::home_loading_yields`).
				Profile->yield_discovery();
				Host->observe_attached_read(WindowRead, aUpdate.Transition.AcceptedSnapshot.has_value());
				observe_restoration_read(aUpdate);
				if (aUpdate.Presentation.PullRequest) { publish_overview(); publish_read_state(); }
				schedule_link_location();
				schedule_publication_reread(); }, this, PublicPreviewComposed ? static_cast<application::github_pull_request_port*>(CountedAnonymousPullRequests.get()) : nullptr, PublicPreviewComposed ? static_cast<application::git_smart_transport_factory_port*>(AnonymousSmartGit.get()) : nullptr, Profile->DiagnosticLog.get(), Profile->CachedOverviews.get(), &Profile->WorkbenchEffects, this, Profile->ReadAdmission, &Profile->BodyDigests, Profile->Images.get(), Profile->Provider.get());
		Window->overview_page()->set_cached_source_dispatch([this, weak](const QString& aPath) {
			if (!weak.expired() && !Closing && Profile->current_window(DraftWindow)) {
				WindowRead->read_cached_source(aPath.toStdString());
			}
		});
		Window->set_markdown_link_dispatch([this, weak](const core::markdown_link_intent& aIntent) {
			return !weak.expired() && !Closing && Profile->current_window(DraftWindow)
				? WindowRead->open_markdown_link(aIntent, Profile->WindowBrowser)
				: core::markdown_link_result::stale;
		});
		// The address the link question shows, from the same admission the
		// open above repeats once the reviewer chooses Open.
		Window->set_markdown_link_address_dispatch([this, weak](const core::markdown_link_intent& aIntent) -> std::expected<std::string, core::markdown_link_result> {
			if (weak.expired() || Closing || !Profile->current_window(DraftWindow)) {
				return std::unexpected(core::markdown_link_result::stale);
			}
			return WindowRead->markdown_link_address(aIntent, Profile->WindowBrowser);
		});
		Window->set_markdown_image_dispatch([this, weak](const core::markdown_image_intent& aIntent) {
			if (!weak.expired() && !Closing && Profile->current_window(DraftWindow)) {
				WindowRead->request_markdown_images(aIntent);
			}
		});
		// [[BPR-1807]]. People's pictures, as logins and sizes the faces drew.
		Window->set_identity_avatar_dispatch([this, weak](const std::vector<core::identity_avatar_intent>& aIntents) {
			if (!weak.expired() && !Closing && Profile->current_window(DraftWindow)) {
				WindowRead->request_identity_avatars(aIntents);
			}
		});
		Window->set_pull_request_page_dispatch([this, weak](const application::pull_request_page_intent& aIntent) {
			return !weak.expired() && !Closing && Profile->current_window(DraftWindow)
				? open_pull_request_page(aIntent)
				: core::markdown_link_result::stale;
		});
		// [[BPR-1566]]. About Conventional Comments, from Settings or a label
		// menu: the window names the page, the application admits it and
		// builds the address, and this window's browser opens it.
		Window->set_reference_page_dispatch([this, weak](const core::reference_page aPage) {
			return !weak.expired() && !Closing
				? application::open_reference_page(aPage, Profile->WindowBrowser)
				: core::markdown_link_result::stale;
		});
		Profile->Observers.insert_or_assign(DraftWindow, github_profile_runtime::window_observer{ .Lifetime = weak, .Github = [this, weak](const application::github_presentation_update& aUpdate) {
				if (weak.expired() || Closing){ return;
}
				auto presentation = aUpdate;
				if (WindowRead->accepted_snapshot()){ presentation.Transition.reset();
}
				Window->publish(presentation);
				// [[BPR-1357]]. A session change can drop the lane's read silently.
				if (HomeOpen) {
					HomeOpen->reconcile(WindowRead->state());
				}
				observe_setup_session(aUpdate);
				observe_cli_session(aUpdate); }, .Device = [this, weak](const application::device_authorization_presentation& aPresentation) {
				if (weak.expired() || Closing){ return;
}
				observe_device_stage(aPresentation);
				if (DevicePanel){ DevicePanel->apply(aPresentation);
} }, .Refresh = [this, weak](const application::device_authorization_refresh_observation& aObservation) {
				if (!weak.expired() && !Closing && DevicePanel){ DevicePanel->apply_refresh(aObservation);
} }, .Preview = [this, weak](const application::public_preview_presentation& aPresentation) {
				if (!weak.expired() && !Closing){ Window->publish(aPresentation);
} }, .SignIn = [this, weak](core::host_profile_id aProfile) { if (!weak.expired() && !Closing){ begin_public_preview_sign_in(aProfile);
} }, .SelectQueue = [this, weak](const core::pull_request_ref& aRef) { return !weak.expired() && !Closing && Window->select_queue_item(aRef); }, .Settings = [this, weak](qt::settings_window& aShell, qt::settings_page aPage) {
				if (!weak.expired() && !Closing) { Window->mount_settings(aShell); aShell.present(aPage); } }, .UnmountSettings = [this, weak] { if (!weak.expired() && Window){ Window->unmount_settings();
} }, .RestoreReview = [this, weak](const core::window_review_open_request& aRequest) { if (!weak.expired() && !Closing) { restore_review(aRequest); } }, .RestoreNotice = [this, weak](const QString& aText) { if (!weak.expired() && !Closing) { restoration_notice(aText); } }, .RecentChanged = [this, weak] { if (!weak.expired() && !Closing) { publish_recent(); } }, .Ledger = [this, weak](const application::workbench_pull_request_authority& aAuthority, ledger_changes aChanges) { if (!weak.expired() && !Closing) { refresh_shared_ledger(aAuthority, aChanges); } }, .SetupProfile = [this, weak](const application::github_profile_option& aOption) { if (!weak.expired() && !Closing) { Window->add_setup_profile(aOption); } }, .RetireServerSetup = [this, weak](std::string_view aHostname) { if (!weak.expired() && !Closing && SetupDestination && SetupDestination->pending() && SetupDestination->pending()->Destination.Hostname == aHostname) { retire_setup_effects(); SetupOwner = next_setup_owner(); SetupDestination->cancel(); } }, .AgentWorkspaceRepair = [this, weak] { if (!weak.expired()) { present_agent_workspace_repair(); } } });
		// What this window's read lane waits on, for Home's discovery to give
		// way to (`core::home_loading_yields`).
		Profile->Observers.at(DraftWindow).ReadLane = [this, weak]() -> std::optional<core::github_state> {
			return weak.expired() || Closing || !WindowRead ? std::nullopt : std::optional{ WindowRead->state() };
		};
		// [[BPR-1850]]. Activity notifications: what this window shows while
		// it is in use, opening a notified pull request, and the setting.
		Profile->Observers.at(DraftWindow).FocusedPullRequest = [this, weak]() -> std::optional<core::pull_request_ref> {
			return weak.expired() || Closing || !Window ? std::nullopt : Window->focused_pull_request();
		};
		Profile->Observers.at(DraftWindow).OpenNotified = [this, weak](const std::optional<core::pull_request_ref>& aRef) {
			return !weak.expired() && !Closing && Window && Window->open_notified_pull_request(aRef);
		};
		Profile->Observers.at(DraftWindow).PresentActivity = [this, weak](const bool aEnabled, const QString& aStatus) {
			if (!weak.expired() && !Closing && Window) {
				Window->present_activity_notifications(aEnabled, aStatus);
			}
		};
		// [[BPR-1867]]. New activity on the pull request this window shows,
		// whether or not it is in front, and coming back to it once its
		// update has aged: the window reads it again by itself.
		Profile->Observers.at(DraftWindow).ActivityArrived = [this, weak](const std::vector<core::pull_request_ref>& aActive) {
			if (weak.expired() || Closing || !Window) {
				return;
			}
			if (const auto shown = Window->shown_pull_request(); shown && core::activity_concerns(aActive, *shown)) {
				reread_for_news();
			}
		};
		Window->set_pull_request_return_dispatch([this, weak](const std::int64_t aNow) {
			if (!weak.expired() && !Closing && Window && core::refresh_due_on_return(Window->pull_request_refresh(), aNow)) {
				reread_for_news();
			}
		});
		Window->set_activity_notifications_dispatch([this, weak](const bool aEnabled) {
			if (!weak.expired() && !Closing) {
				Profile->set_activity_enabled(aEnabled);
			}
		});
		Profile->present_activity();
		// Home may already be checked while a restored read is pending. Its
		// toggle then emits false, but the gesture still supersedes restoration.
		QObject::connect(Window->home_action(), &QAction::triggered, Window.get(), [this, weak](bool) {
			if (weak.expired() || Closing) {
				return;
			}
			cancel_restoration(false);
			if (!WindowRead->accepted_snapshot()) {
				Window->choose_home(true);
			}
		});
		Window->set_diagnostic_exporter([this, weak](core::diagnostic_export_policy aPolicy) {
			if (!weak.expired() && !Closing) {
				Profile->present_diagnostic_export(DraftWindow, aPolicy);
			}
		});
		Window->set_insights_presenter([this, weak] { if (!weak.expired() && !Closing) { Profile->present_insights(DraftWindow, Window.get(), AppearanceProfile.get()); } });
		Window->set_settings_presenter([this, weak](qt::settings_page aPage) { if (!weak.expired() && !Closing){ Profile->present_settings(DraftWindow, aPage);
} });
		auto queue = QueueHost->attach(DraftWindow, std::make_shared<window_queue_projection>([this, weak](const core::bpr_window_context& aContext, const application::queue_landing_projection& aProjection) {
										   if (!weak.expired() && !Closing && aContext == DraftWindow) {
											   publish(aProjection);
										   }
									   }));
		if (!queue) {
			throw std::runtime_error{ "queue window admission unavailable" };
		}
		QueueConsumer = *queue;
		if (!Host->attach_window(WindowRead)) {
			throw std::runtime_error{ "window read admission unavailable" };
		}
		// [[BPR-1017]]. A window attached after the profile accepted its account
		// session presents it now, as its siblings did when it was accepted.
		if (const auto adopted = WindowRead->adopted_presentation()) {
			Window->publish(*adopted);
		} else if (const auto pending = Host->pending_session_presentation()) {
			Window->publish(*pending);
		}
		WorkbenchHost->restore();
		AppearanceHost->restore();
		if (Window) {
			Window->set_public_preview_dispatch([this, weak](application::public_preview_gesture) {
				if (!weak.expired() && !Closing) {
					begin_public_preview_sign_in(Host->state().ActiveProfile.value_or(core::host_profile_id{ }));
				}
			});
		}
	}
}
