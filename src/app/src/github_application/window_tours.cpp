#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "bpr/application/skill_guidance.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/tour_stop_acceptance.hpp"
#include "bpr/qt/agent_footer.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/local_draft_parts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#include "tour_observation.hpp"
#include "tour_owner_service.hpp"

#include <QAction>
#include <QByteArrayView>
#include <QComboBox>
#include <QDesktopServices>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <chrono>
#include <iterator>
#include <mutex>
#include <string_view>
#include <variant>
#include <vector>

#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/application/agent_scratch_folder.hpp"
	#include "bpr/application/tour_agent_conversation.hpp"
	#include "bpr/application/tour_agent_drafting.hpp"
	#include "bpr/core/agent_entry.hpp"
#endif
#if defined(BPR_BUILD_TESTING)
	#include "bpr/application/workbench_preferences_host.hpp"
#endif

namespace bpr::app {
	auto github_application::implementation::selected_tour_source() const -> std::optional<core::tour_source_range>
	{
		const auto* surface = Window->comparison_surface();
		if (!TourOwner || !SelectedDiffPublication || !surface->current_context() || surface->current_context()->Key != SelectedDiffPublication->Key) {
			return std::nullopt;
		}
		const auto range = surface->selected_source_range();
		if (!range) {
			return std::nullopt;
		}
		const auto& p     = *SelectedDiffPublication;
		const auto& owner = p.Request.WorkbenchAuthority;
		if (owner.Host != TourOwner->Host || owner.HostAuthority != TourOwner->HostAuthority || owner.Account != TourOwner->Account || owner.Repository != TourOwner->Repository || owner.PullRequest != TourOwner->PullRequest) {
			return std::nullopt;
		}
		const bool  left     = range->first.Side == core::diff_side::left;
		const auto& path     = left ? p.LeftPath : p.RightPath;
		const auto& blob     = left ? p.Key.LeftBlob : p.Key.RightBlob;
		const auto& endpoint = left ? p.LeftEndpoint : p.RightEndpoint;
		if (!path || !blob || !endpoint || !application::valid(*endpoint, p.Request, range->first.Side, *blob, *path)) {
			return std::nullopt;
		}
		const auto bytes  = path->generic_u8string();
		auto       source = core::tour_source_range{ range->first, range->second, left ? p.Request.Diff.Comparison.Left.Commit : p.Request.Diff.Comparison.Right.Commit, blob, std::string{ bytes.begin(), bytes.end() } };
		return core::valid(source) ? std::optional{ std::move(source) } : std::nullopt;
	}

	auto github_application::implementation::current_tour_attention() const -> core::tour_attention
	{
		core::tour_attention attention;
		attention.EditorVisible = Window->tour_editor()->writing_visible(core::writing_hook::comment);
		const auto* surface     = Window->comparison_surface();
		if (attention.EditorVisible && surface->isVisible()) {
			const auto source = selected_tour_source();
			if (source && surface->source_line_visible(source->First.Side, source->First.Line)) {
				attention.Source = source;
			}
		}
		return attention;
	}

	void github_application::implementation::observe_tour_visit()
	{
		const auto* stop  = Window->tour_editor()->selected_stop();
		const auto  draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!stop || !stop->Suggestion || !draft) {
			return;
		}
		auto attention = current_tour_attention();
		if (!attention.EditorVisible) {
			return;
		}
		if (!core::visible_tour_stop(*stop, attention, true)) {
			attention.Source.reset();
			attention.SourceUnavailable = true;
			Window->tour_editor()->show_notice(QStringLiteral("This stop's exact source is not visible. Inspect or retarget its source before marking it Ready."));
		}
		TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::visit, .Stop = stop->Id, .Attention = attention });
	}

	void github_application::implementation::observe_tour_comparison()
	{
		if (!TourHost || !TourOwner || !SelectedDiffPublication) {
			return;
		}
		const auto* surface = Window->comparison_surface();
		const auto& p       = *SelectedDiffPublication;
		const auto  index   = surface->current_semantic_index();
		if (!index || !surface->current_context() || surface->current_context()->Key != p.Key || !p.LeftPath || !p.RightPath || !p.Key.LeftBlob || !p.Key.RightBlob) {
			return;
		}
		const auto& owner = p.Request.WorkbenchAuthority;
		if (owner.Host != TourOwner->Host || owner.HostAuthority != TourOwner->HostAuthority || owner.Account != TourOwner->Account || owner.Repository != TourOwner->Repository || owner.PullRequest != TourOwner->PullRequest) {
			return;
		}
		const auto left = p.LeftPath->generic_u8string(), right = p.RightPath->generic_u8string();
		TourHost->observe_comparison(*index, { p.Request.Diff.Comparison.Left.Commit, p.Key.LeftBlob, std::string{ left.begin(), left.end() }, core::diff_side::left, 0, 0 }, { p.Request.Diff.Comparison.Right.Commit, p.Key.RightBlob, std::string{ right.begin(), right.end() }, core::diff_side::right, 0, 0 });
	}

	void github_application::implementation::finish_tour_navigation()
	{
		if (!PendingTourSource || !TourHost || !TourOwner || Closing || !WorkbenchHost) {
			return;
		}
		const auto source = *PendingTourSource;
		const auto file   = core::file_identity_of(source.Path);
		if (WorkbenchHost->selected() != file) {
			PendingTourSource.reset();
			return;
		}
		auto* surface = Window->comparison_surface();
		if (!SelectedDiffPublication || SelectedDiffPublication->Request.File.File != file || !surface->current_context() || surface->current_context()->Key != SelectedDiffPublication->Key || !surface->current_semantic_index()) {
			return;
		}
		const auto& p      = *SelectedDiffPublication;
		const bool  left   = source.Side == core::diff_side::left;
		const auto& commit = left ? p.Request.Diff.Comparison.Left.Commit : p.Request.Diff.Comparison.Right.Commit;
		const auto& blob   = left ? p.Key.LeftBlob : p.Key.RightBlob;
		const auto& path   = left ? p.LeftPath : p.RightPath;
		const auto  bytes  = path ? path->generic_u8string() : std::u8string{ };
		PendingTourSource.reset();
		if (commit != source.Commit || (source.Blob && blob != source.Blob) || std::string{ bytes.begin(), bytes.end() } != source.Path) {
			Window->tour_editor()->show_notice(QStringLiteral("The original source revision is not in this comparison. The stop and its pinned destination are retained; choose its revision to inspect it."));
			observe_tour_visit();
			return;
		}
		auto first = core::source_line_ref{ core::comparison_digest(p.Key), source.Side, source.FirstLine };
		auto last  = core::source_line_ref{ core::comparison_digest(p.Key), source.Side, source.LastLine };
		// Restart changes runtime scope. Reuse lines only after the immutable
		// commit, blob, path and side match the newly admitted material.
		first.Comparison = last.Comparison = core::comparison_digest(p.Key);
		const bool placed                  = surface->restore_source_range(first, last);
		if (placed) {
			auto observed = source;
			observed.Blob = blob;
			TourHost->observe_source(std::move(observed));
		}
		observe_tour_visit();
		Window->tour_editor()->show_notice(placed ? QStringLiteral("Tour source selected. Visiting a stop does not mark code reviewed.") : QStringLiteral("Source lines unavailable. The explanation and original destination are retained."));
	}

	void github_application::implementation::navigate_tour_source(const std::optional<core::tour_code_destination>& aSource)
	{
		// [[BPR-1914]] v004. From the Tour tab itself: the step stays open.
		Window->open_review_tour(true);
		Window->tour_editor()->show_stops();
		if (!aSource || !WorkbenchHost || !WorkbenchHost->contains_file(core::file_identity_of(aSource->Path))) {
			Window->tour_editor()->show_notice(QStringLiteral("Source is unavailable in this comparison. The stop stays editable and the normal file path remains available."));
			observe_tour_visit();
			return;
		}
		abandon_tab_load();
		TabModality       = core::workbench_modality::review;
		PendingTourSource = aSource;
		const auto file   = core::file_identity_of(aSource->Path);
		if (WorkbenchHost->selected() != file) {
			WorkbenchHost->apply(core::select_workbench_file_intent{ file });
		}
		finish_tour_navigation();
	}

	void github_application::implementation::observe_tour_head()
	{
		if (!TourHost) {
			return;
		}
		const auto draft = TourHost->presentation().Draft;
		if (draft && draft->ObservedHead.value_or(draft->Head) != TourHead && TourAttentionHead != TourHead) {
			TourAttentionHead = TourHead;
			TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::observe_head, .Head = TourHead });
		}
	}

	void github_application::implementation::suspend_review_tour()
	{
		PendingTourSource.reset();
		++TourRepairGeneration;
		++TourMaterialGeneration;
		TourMaterialRequests.clear();
		TourMaterialReading = false;
		// Re-review finding 3: what was asked and is now dropped is asked again.
		TourMaterial.dropped();
		TourRepairComparisons.clear();
		TourRepairFiles.clear();
		TourRepairCommand.reset();
		TourSourceRequest.reset();
		PendingTourStart = false;
		PendingTourStop.reset();
		if (TourHost && TourOwner) {
			RetiredTourHosts.push_back({ *TourOwner, std::move(TourHost) });
		}
		TourOwner.reset();
		// Clean, drained copies can be reconstructed. Keep failed or pending
		// copies reachable by their immutable owner when that PR is reopened.
		std::erase_if(RetiredTourHosts, [](const auto& aRetained) {
			const auto p         = aRetained.Host->presentation();
			const bool untouched = p.Draft && p.Draft->Revision == 1U && p.Draft->Stops.empty();
			return aRetained.Host->is_quiescent() && !p.Failure && p.Draft && (p.SavedRevision == p.Draft->Revision || untouched);
		});
		Window->prepare_tour_action()->setEnabled(false);
		if (!local_tour_shown()) {
			Window->tour_editor()->present({ });
		}
	}

	/// The draft this window holds in memory for an opaque selector, current or retired; decision lane.
	auto github_application::implementation::live_tour(std::string_view aSelector) -> live_tour_view
	{
		const auto matches = [&](const application::review_tour_host& aHost) {
			const auto shown = aHost.presentation();
			if (!shown.Draft) {
				return false;
			}
			application::tour_saved_target target;
			target.Owner = shown.Draft->Owner;
			target.Draft = shown.Draft->Id;
			return tour_draft_token(target) == aSelector;
		};
		if (TourHost && matches(*TourHost)) {
			return { TourHost.get(), true, Window && Window->tour_editor()->composing(), tour_changed_files() };
		}
		for (const auto& retained : RetiredTourHosts) {
			if (matches(*retained.Host)) {
				return { retained.Host.get(), false, false };
			}
		}
		return { };
	}

	/// [[BPR-1227]]. Reads one pinned range for the draft this window composes now, through its own
	/// epoch host and Git cache on the diff lane, and answers it there; decision lane. False when
	/// this window does not show that draft's pull request, so it neither answers nor keeps the reply.
	auto github_application::implementation::read_tour_source(std::string_view aSelector, const core::tour_code_destination& aSource, std::function<void(application::tour_channel_bytes)>& aRespond) -> bool
	{
		if (Closing || !EpochHost || !LastAcceptedHandoff || !TourOwner || !live_tour(aSelector).Current) {
			return false;
		}
		const auto authority = workbench_authority(*LastAcceptedHandoff);
		if (*TourOwner != core::tour_owner{ DraftWindow.Profile, authority.Host, authority.HostAuthority, authority.Account, authority.Repository, authority.PullRequest }) {
			return false;
		}
		const auto refuse  = [&](std::string aCode) { aRespond(adapters::sqlite::encode_tour_response({ .Error = std::move(aCode) })); };
		const auto context = application::review_tour_source_context(*EpochHost, authority, aSource);
		if (!context) {
			refuse("source_unavailable");
			return true;
		}
		const application::workbench_selected_diff_request request{ .Context = { core::request_id{ 1 }, authority.ApplicationSession, core::request_generation{ 1 } }, .WorkbenchAuthority = authority, .Diff = *context, .File = { .File = core::file_identity_of(aSource.Path), .PresentationPath = aSource.Path, .CurrentPath = native_filesystem_path(QString::fromUtf8(aSource.Path)), .Change = application::workbench_file_change_kind::unchanged, .ContentKind = application::workbench_file_content_kind::text } };
		auto* const                                        epoch = EpochHost.get();
		TourSourceReadsInFlight.fetch_add(1U, std::memory_order_acq_rel);
		try {
			DiffEffects.post([this, epoch, request, source = aSource, stop = TourSourceStop.get_token(), respond = aRespond] {
				auto answer = application::tour_owner_response{ };
				try {
					auto material = application::materialize_review_tour_source(*epoch, *epoch, request, source, stop);
					if (material) {
						answer.Source = std::move(*material);
					} else {
						answer.Error = material.error() == application::review_tour_source_failure::range_unavailable ? "source_range_unavailable"
							: material.error() == application::review_tour_source_failure::too_large                  ? "source_too_large"
																													  : "source_unavailable";
					}
				} catch (...) {
					answer = { .Error = "source_unavailable" };
				}
				try {
					respond(adapters::sqlite::encode_tour_response(std::move(answer)));
				} catch (...) { }
				// Last: once this reaches zero the closing window may be destroyed.
				TourSourceReadsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
			});
		} catch (...) {
			TourSourceReadsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
			refuse("source_unavailable");
		}
		return true;
	}

	/// [[BPR-1781]]. A reader's Start tour from Overview: a published tour
	/// already in this window opens at its first stop; one only on offer is
	/// imported first, as Review's Import does, and opens once it arrives.
	void github_application::implementation::start_published_tour(std::optional<std::pair<std::string, core::published_tour_stop>> aStop)
	{
		if (Closing || !TourHost || !current(DraftWindow)) {
			return;
		}
		const auto shown = TourHost->presentation();
		if (shown.Draft && shown.Draft->Remote && !shown.Draft->Stops.empty()) {
			start_tour_at(aStop);
			return;
		}
		if (!shown.Offered) {
			Window->open_review_tour();
			return;
		}
		PendingTourStart = true;
		PendingTourStop  = std::move(aStop);
		Window->open_review_tour();
		TourHost->import_selected(0U);
	}

	auto github_application::implementation::follow_in_app_link(const core::markdown_link_intent& aIntent) -> bool
	{
		if (Closing || !Window || !WindowRead || !current(DraftWindow)) {
			return false;
		}
		// The window's own published tour first, then each offered one in
		// import order: the index the core answers says which.
		auto roots = std::vector<core::tour_remote_binding>{ };
		auto own   = false;
		if (TourHost) {
			const auto shown = TourHost->presentation();
			if (shown.Draft && shown.Draft->Remote && !shown.Draft->Stops.empty()) {
				roots.push_back(*shown.Draft->Remote);
				own = true;
			}
			std::ranges::move(TourHost->import_roots(), std::back_inserter(roots));
		}
		const auto rightIsHead = EpochHost && comparison_ends_at_head();
		const auto link        = WindowRead->markdown_in_app_link_of(aIntent, roots, rightIsHead);
		if (!link) {
			return false;
		}
		switch (link->Decision.Route) {
		case core::pull_request_link_route::external: return false;
		case core::pull_request_link_route::tour:
			if (own && link->Decision.Tour == 0U) {
				start_tour_at(std::nullopt);
			} else {
				open_linked_tour(link->Decision.Tour - (own ? 1U : 0U), roots[link->Decision.Tour]);
			}
			return true;
		case core::pull_request_link_route::conversation_page:
			Window->open_overview_tab(qt::overview_drawer_tab::comments);
			return true;
		case core::pull_request_link_route::files_page:
			Window->choose_modality(qt::review_modality::review);
			return true;
		case core::pull_request_link_route::commits_page:
			Window->show_commits();
			return true;
		case core::pull_request_link_route::commit: {
			// RFC-BPR-0089. Show only that commit's changes, as choosing its
			// row in the Commits tab does, and show the tab.
			auto gesture = application::commit_filter_gesture{ };
			gesture.Kind = core::commit_filter_operation_kind::show_only;
			gesture.Commits.push_back(link->Decision.Commit);
			apply_commit_filter_gesture(gesture);
			Window->show_commits();
			return true;
		}
		case core::pull_request_link_route::location:
			// [[BPR-0987]]. Followed exactly as a pasted link's location is,
			// in the pull request this window already shows.
			PendingLinkLocation = link_location{ .Destination = link->Destination };
			schedule_link_location();
			return true;
		}
		return false;
	}

	/// An offered tour a link named: imported as Start tour imports, and
	/// opened at its first stop once it arrives. A private draft that the
	/// import cannot replace keeps its text; the Tour tab says so.
	void github_application::implementation::open_linked_tour(const std::size_t aImport, const core::tour_remote_binding& aRoot)
	{
		if (!TourHost) {
			return;
		}
		const auto  shown   = TourHost->presentation();
		const auto& draft   = shown.Draft;
		const auto  replace = draft && (draft->Remote ? draft->Remote->Entity == aRoot.Entity : draft->Stops.empty());
		PendingTourStart    = replace;
		PendingTourStop.reset();
		Window->open_review_tour();
		TourHost->import_selected(aImport);
	}

	void github_application::implementation::start_tour_at(const std::optional<std::pair<std::string, core::published_tour_stop>>& aStop)
	{
		if (aStop) {
			Window->start_tour_at_published_stop(aStop->first, aStop->second);
		} else {
			Window->start_tour_at_first_stop();
		}
	}

	void github_application::implementation::finish_pending_tour_start(const application::tour_presentation& aProjection)
	{
		if (!PendingTourStart || !aProjection.Draft || !aProjection.Draft->Remote || aProjection.Draft->Stops.empty()) {
			return;
		}
		PendingTourStart = false;
		const auto stop  = std::exchange(PendingTourStop, std::nullopt);
		start_tour_at(stop);
	}

	void github_application::implementation::compose_review_tour(const application::accepted_pull_request_handoff& aHandoff)
	{
		const auto owner = core::tour_owner{ DraftWindow.Profile, aHandoff.Host, aHandoff.HostAuthority, aHandoff.Account, aHandoff.Repository, aHandoff.PullRequest };
		if (!core::valid(owner)) {
			return;
		}
		TourHead = aHandoff.Head;
		if (TourOwner == owner && TourHost) {
			observe_tour_head();
			refresh_tour_imports();
			return;
		}
		// [[BPR-2051]]. Another pull request: the footer's total is this one's.
		TourFooterCosts = { };
		if (Window != nullptr && Window->agent_footer_widget() != nullptr) {
			for (const auto place : { qt::agent_footer_place::tour_chat, qt::agent_footer_place::enhance, qt::agent_footer_place::draft_tour, qt::agent_footer_place::tour_tab }) {
				Window->agent_footer_widget()->present_cost(place, std::nullopt);
			}
		}
		suspend_review_tour();
		if (const auto found = std::ranges::find(RetiredTourHosts, owner, &retained_tour_host::Owner); found != RetiredTourHosts.end()) {
			TourHost = std::move(found->Host);
			RetiredTourHosts.erase(found);
			TourOwner          = owner;
			TourAttentionHead  = { };
			auto shown         = TourHost->presentation();
			shown.ChangedFiles = tour_changed_files();
			if (!local_tour_shown()) {
				Window->tour_editor()->present(shown);
			}
			Window->prepare_tour_action()->setEnabled(static_cast<bool>(TourHost->presentation().Draft));
			observe_tour_head();
			return;
		}
		if (RetiredTourHosts.size() >= 8U) {
			Window->tour_editor()->show_notice(QStringLiteral("Eight earlier tours still have unsaved edits. Reopen one of those pull requests and retry its local save before starting another tour."));
			return;
		}
		TourOwner         = owner;
		TourAttentionHead = { };
		auto       seed   = core::review_tour{ .Id = core::tour_draft_id{ 1 }, .Owner = owner, .Base = aHandoff.Base, .Head = aHandoff.Head };
		const auto weak   = std::weak_ptr<const int>{ WindowLifetime };
		TourHost          = std::make_unique<application::review_tour_host>(std::move(seed), adapters::sqlite::make_review_tour_store(ApplicationDataRoot, DraftWindow.Profile), WorkbenchEffects, DecisionLane,
																			[this, weak, owner](const application::tour_presentation& aProjection) {
																	   if (weak.expired() || Closing || TourOwner != owner || !current(DraftWindow)) {
																		   return;
																	   }
																	   auto projected         = aProjection;
																	   projected.ChangedFiles = tour_changed_files();
																	   if (projected.Draft && WindowRead->accepted_snapshot()) {
																		   const auto& snapshot = *WindowRead->accepted_snapshot();
																		   if (const auto* profile = find_profile(snapshot.Profile)) {
																			   const auto scope = adapters::github::detail::tour_permalink_scope{ std::string{ profile->hostname() }, { snapshot.Repository.Owner, snapshot.Repository.Name } };
																			   for (auto& destination : projected.Destinations) {
																				   const auto stop = std::ranges::find(projected.Draft->Stops, destination.Stop, &core::tour_stop::Id);
																				   if (stop == projected.Draft->Stops.end()) {
																					   continue;
																				   }
																				   auto source = stop->Destination;
																				   if (stop->Original) {
																					   source = core::destination_of(*stop->Original);
																				   }
																				   if (source) {
																					   destination.SourceLink = adapters::github::detail::compile_tour_permalink(scope, *source).value_or(std::string{ });
																				   }
																			   }
																		   }
																	   }
																	   if (!local_tour_shown()) {
																		   Window->tour_editor()->present(projected);
																	   }
#if defined(BPR_AGENT_INTEGRATIONS)
																	   // [[BPR-1931]]. Undo reset goes once the tour changes.
																	   present_tour_reset();
#endif
																	   finish_pending_tour_start(projected);
																	   carry_local_tour(projected);
																	   if (TourPublication) {
																		   TourPublication->observe(projected);
																	   }
																	   Window->prepare_tour_action()->setEnabled(static_cast<bool>(aProjection.Draft));
																	   observe_tour_head();
																	   check_tour_material();
																   });
		TourHost->set_diagnostics(DiagnosticLog.get()); // [[BPR-1830]]
		TourHost->start();
		refresh_tour_imports();
	}

	/// Lists this window's draft in the profile's exact-target catalog, so the CLI and the owner's
	/// forwarding address it by the same opaque selector the desktop's own draft has.
	void github_application::implementation::remember_tour_target(const core::review_tour& aSeed, const application::github_pull_request_snapshot& aSnapshot, std::string_view aHostname)
	{
		application::tour_saved_target target;
		target.Owner       = aSeed.Owner;
		target.Draft       = aSeed.Id;
		target.HostProfile = aSnapshot.Profile;
		target.Hostname    = std::string{ aHostname };
		target.Repository  = { aSnapshot.Repository.Owner, aSnapshot.Repository.Name };
		target.Number      = aSnapshot.PullRequest.Number;
		target.Base        = aSeed.Base;
		target.Head        = aSeed.Head;
		if (RememberedTourTarget == target) {
			return;
		}
		RememberedTourTarget = target;
		try {
			WorkbenchEffects.post([saved = target, root = ApplicationDataRoot, profile = DraftWindow.Profile] {
				try {
					static_cast<void>(adapters::sqlite::make_review_tour_store(root, profile)->remember_target(saved, { }));
				} catch (...) { }
			});
		} catch (...) { }
	}

	void github_application::implementation::refresh_tour_imports()
	{
		if (!TourHost || !WindowRead->accepted_snapshot() || !TourOwner) {
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto* profile  = find_profile(snapshot.Profile);
		const auto  base     = core::parse_git_commit_oid(snapshot.PullRequest.BaseOid.value());
		const auto  head     = core::parse_git_commit_oid(snapshot.PullRequest.HeadOid.value());
		if (!profile || !base || !head) {
			return;
		}
		const auto seed = core::review_tour{ .Id = core::tour_draft_id{ 1 }, .Owner = *TourOwner, .Base = *base, .Head = *head };
		remember_tour_target(seed, snapshot, profile->hostname());
		TourHost->observe_imports(observe_published_tours(seed, snapshot, profile->hostname()));
	}

	auto github_application::implementation::tour_permalink(const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast, const bool aBefore) const -> std::optional<std::string>
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!draft || !WindowRead->accepted_snapshot() || aFirst == 0U || aLast < aFirst) {
			return std::nullopt;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto* profile  = find_profile(snapshot.Profile);
		if (profile == nullptr) {
			return std::nullopt;
		}
		const auto scope       = adapters::github::detail::tour_permalink_scope{ std::string{ profile->hostname() }, { snapshot.Repository.Owner, snapshot.Repository.Name } };
		auto       destination = core::tour_code_destination{ };
		destination.Commit     = aBefore ? draft->Base : draft->Head;
		destination.Path       = aPath;
		destination.Side       = aBefore ? core::diff_side::left : core::diff_side::right;
		destination.FirstLine  = aFirst - 1U;
		destination.LastLine   = aLast - 1U;
		return adapters::github::detail::compile_tour_permalink(scope, destination);
	}

	/// [[BPR-1900]]. Whether the tour session can take a request now: its
	/// own host is idle and no run of it is still running. The Agent tab's
	/// chat being busy does not matter.
	auto github_application::implementation::tour_session_idle() const -> bool
	{
#if defined(BPR_AGENT_INTEGRATIONS)
		return TourSession && TourSession->is_quiescent() && !core::running_tour_run(TourAgent);
#else
		return false;
#endif
	}

	/// [[BPR-2040]]. The answer can be final before the host retires its
	/// cleanup/probes. Present that existing capability without making the
	/// answer Running again. One editor-bound observer does no effect work.
	void github_application::implementation::refresh_tour_host_idle()
	{
#if defined(BPR_AGENT_INTEGRATIONS)
		if (Closing || !Window || !TourSession) {
			if (TourIdleObservation) {
				TourIdleObservation->stop();
			}
			return;
		}
		const auto host   = TourSession.get();
		const auto editor = QPointer{ Window->tour_editor() };
		if (!editor) {
			if (TourIdleObservation) {
				TourIdleObservation->stop();
			}
			return;
		}
		const auto projectedGeneration = TourIdleObservationGeneration;
		const auto idle                = tour_session_idle();
		editor->set_tour_host_idle(idle);
		if (idle) {
			// Feedback left from a draft no window has any more comes back
			// to the chat as not sent (deferred out of the presentation).
			QTimer::singleShot(0, editor.data(), [this] {
				drain_tour_queue();
				// [[BPR-2154]]. And each tour part's queue.
				drain_chat_queues();
			});
		}

		// A presentation callback can replace either identity or an observer.
		// Never let the older refresh overwrite that reentrant observation.
		if (Closing || !Window || !editor || Window->tour_editor() != editor.data()
			|| TourSession.get() != host || projectedGeneration != TourIdleObservationGeneration) {
			return;
		}
		if (idle || TourRun) {
			++TourIdleObservationGeneration;
			if (TourIdleObservation) {
				TourIdleObservation->stop();
			}
			return;
		}
		if (TourIdleObservation && TourIdleObservation->isActive() && TourIdleObservedHost == host && TourIdleObservedEditor == editor) {
			return;
		}
		const auto generation  = ++TourIdleObservationGeneration;
		TourIdleObservation    = std::make_unique<QTimer>();
		TourIdleObservedHost   = host;
		TourIdleObservedEditor = editor;
		const auto observer    = QPointer{ TourIdleObservation.get() };
		observer->setTimerType(Qt::PreciseTimer);
		observer->setInterval(100);
		QObject::connect(editor.data(), &QObject::destroyed, observer.data(), &QTimer::stop);
		QObject::connect(observer.data(), &QTimer::timeout, editor.data(), [this, observer, editor, host, generation] {
			if (Closing || !Window || !editor || Window->tour_editor() != editor.data()
				|| TourSession.get() != host || generation != TourIdleObservationGeneration || TourRun) {
				if (observer) {
					observer->stop();
				}
				return;
			}
			const auto ready = tour_session_idle();
			editor->set_tour_host_idle(ready);

			// Reentrant presentation may have deleted this exact observer.
			if (ready && observer) {
				observer->stop();
			}
		});
		observer->start();
#endif
	}

	void github_application::implementation::cancel_writing()
	{
		WritingState.cancel();
		if (!PendingWriting) {
			return;
		}
		// [[BPR-2051]]. An Enhance run is the Agent tab host's, and its field
		// is told it ended.
		if (const auto enhance = PendingWriting->Enhance) {
			cancel_enhancement(*enhance);
			answer_enhancement(*enhance, std::nullopt, QStringLiteral("Stopped. Your text is unchanged."));
			return;
		}
		PendingWriting.reset();
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1900]]. Only the tour session's own run is stopped; the
		// Agent tab's chat is never touched by a tour gesture.
		if (TourSession) {
			TourSession->cancel();
		}
#endif
		if (Window) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("Assistance cancelled. Your text is unchanged."));
		}
	}

	auto github_application::implementation::writing_context_key() const -> std::array<std::uint8_t, 32>
	{
		std::string key;
		if (const auto& snapshot = WindowRead->accepted_snapshot(); snapshot) {
			key = std::to_string(snapshot->Snapshot.value()) + ":" + std::string{ snapshot->PullRequest.HeadOid.value() };
		}
		if (const auto& draft = Window->tour_editor()->shown().Draft; draft) {
			key += ":" + std::to_string(draft->Revision);
		}
		if (SelectedDiffPublication) {
			const auto digest = core::comparison_digest(SelectedDiffPublication->Key);
			key.append(reinterpret_cast<const char*>(digest.bytes().data()), digest.bytes().size());
		}
		if (const auto range = Window->comparison_surface()->selected_source_range()) {
			key += ":" + std::to_string(static_cast<unsigned>(range->first.Side)) + ":" + std::to_string(range->first.Line) + ":" + std::to_string(range->second.Line);
		}
		return core::writing_digest(key);
	}

	/// \brief [[BPR-1753]], owner decision 2026-10-23. The agent tour
	/// writing asks: the one chosen in the pull request's agent picker. The
	/// callers refuse it when it does not speak Claude Code's stream JSON
	/// (tool-free writing needs it); no other agent is tried.
	auto github_application::implementation::writing_agent() const -> std::optional<core::agent_profile_id>
	{
		return Window->agent_panel()->selected_profile();
	}

	/// [[BPR-1878]]. An agent's name as the agent menus show it.
	auto github_application::implementation::agent_display_name(const core::agent_profile_id aProfile) const -> QString
	{
		const auto* const menu = Window->agent_panel()->profile_selector();
		const auto        row  = menu->findData(QVariant::fromValue<qulonglong>(aProfile.value()));
		return row >= 0 ? menu->itemText(row) : QStringLiteral("The agent");
	}

	/// \brief [[BPR-2259]]. What a writing run that ended on a failed sign-in
	/// says: the agent that ran, by its name in the agent menus, and never a
	/// stock CLI's sign-in command (a custom agent signs in its own way).
	auto github_application::implementation::sign_in_failure_words(const core::agent_profile_id aProfile, const QString& aThen) const -> QString
	{
		return QStringLiteral("%1 isn't signed in, or its wrapper couldn't get a sign-in token. Sign it in again in a terminal, then %2. Your text is unchanged.").arg(agent_display_name(aProfile), aThen);
	}

	/// Whether a profile may run tool-free writing: Claude's stream JSON.
	auto github_application::implementation::writes_tool_free(const core::agent_profile_id aProfile) const -> bool
	{
#if defined(BPR_AGENT_INTEGRATIONS)
		if (core::is_agent_entry_profile(aProfile)) {
			if (!AgentSetup) {
				return false;
			}
			const auto        agents = AgentSetup->projection().Preferences.Agents;
			const auto* const entry  = core::entry_of_profile(agents, aProfile);
			return entry != nullptr && entry->Protocol == core::agent_protocol::claude_stream_json;
		}
#endif
		return aProfile == core::claude_stream_json_profile_id;
	}

	auto github_application::implementation::writing_target(core::writing_hook aHook, const qt::prose_capture& aCapture) const -> std::optional<core::writing_target>
	{
		const auto& draft = Window->tour_editor()->shown().Draft;
		const auto& prefs = Profile->WorkbenchPreferences->accepted();
		const auto  agent = writing_agent();
		if (!draft || !prefs || !agent || !EpochHost || !TourOwner || draft->Owner != *TourOwner || !current(DraftWindow)) {
			return std::nullopt;
		}
		const auto* stop = Window->tour_editor()->selected_stop();
		if (aHook == core::writing_hook::comment && !stop) {
			return std::nullopt;
		}
		return core::writing_target{ .Owner = draft->Owner, .Draft = draft->Id, .Stop = aHook == core::writing_hook::comment ? stop->Id : core::tour_stop_id{ }, .Hook = aHook, .Window = DraftWindow, .Session = EpochHost->state().ApplicationSession, .Agent = *agent, .HookRevision = prefs->WritingHooks.Revision, .TextGeneration = aCapture.TextGeneration, .EditorGeneration = aCapture.EditorGeneration, .ContextDigest = writing_context_key() };
	}

	auto github_application::implementation::selected_writing_source(bool aOutline) const -> std::optional<application::writing_source>
	{
		const auto  selected = selected_tour_source();
		const auto& result   = Window->comparison_surface()->current_result();
		if (!selected || !result || !SelectedDiffPublication || result->Alignment.Key != SelectedDiffPublication->Key) {
			return std::nullopt;
		}
		const auto* stop = Window->tour_editor()->selected_stop();
		if (!aOutline && (!stop || !core::visible_tour_stop(*stop, { .EditorVisible = true, .Source = selected }, true))) {
			return std::nullopt;
		}
		const auto& document = selected->First.Side == core::diff_side::left ? result->LeftDocument : result->RightDocument;
		if (!document || selected->Last.Line >= document->Lines.size()) {
			return std::nullopt;
		}
		const auto begin = document->Lines[selected->First.Line].Content.Begin, end = document->Lines[selected->Last.Line].Content.End;
		if (end < begin || end > document->Bytes.size() || end - begin > (aOutline ? 3072U : 4096U)) {
			return std::nullopt;
		}
		auto text = std::string{ reinterpret_cast<const char*>(document->Bytes.data() + begin), static_cast<std::size_t>(end - begin) };
		if (aOutline) {
			text = selected->Path + " · lines " + std::to_string(selected->First.Line + 1) + "–" + std::to_string(selected->Last.Line + 1) + "\n" + text;
		}
		return application::writing_source{ aOutline ? "selected-source" : "stop-source", std::move(text), selected };
	}

	auto github_application::implementation::writing_sources(core::writing_hook aHook, const core::writing_request& aRequest) const -> std::vector<application::writing_source>
	{
		std::vector<application::writing_source> sources;
		std::size_t                              total = 0;
		const auto                               add   = [&](std::string aId, std::string_view aText, std::size_t aLimit = 2048) {
			if (aText.empty() || sources.size() == 8) {
				return;
			}
			auto size = std::min({ aText.size(), aLimit, 8192 - total });
			while (size && !core::writing_boundary(aText, size)) {
				--size;
			}
			if (!size || !core::valid_writing_utf8(aText.substr(0, size))) {
				return;
			}
			sources.push_back({ std::move(aId), std::string{ aText.substr(0, size) } });
			total += size;
		};
		if (aHook == core::writing_hook::outline) {
			if (const auto source = selected_writing_source(true)) {
				add(source->Id, source->Text, 4096);
				if (!sources.empty()) {
					sources.back().Source = source->Source;
				}
			}
		}
		const auto& snapshot = WindowRead->accepted_snapshot();
		if (snapshot) {
			add("pr-description", snapshot->PullRequest.Body, 4096);
			for (const auto& comment : snapshot->IssueComments) {
				if (snapshot->PullRequest.Author && comment.Author && comment.Author->Actor == snapshot->PullRequest.Author->Actor && !comment.Deleted) {
					add("author-comment-" + std::string{ comment.Comment.value() }, comment.Observation.RawBody.value_or(comment.Body), 1024);
				}
			}
		}
		if (aRequest.Scope == core::writing_scope::selection) {
			add("field-before", std::string_view{ aRequest.Raw }.substr(0, aRequest.Selection.Begin), 512);
			add("field-after", std::string_view{ aRequest.Raw }.substr(aRequest.Selection.End), 512);
		}
		if (aHook == core::writing_hook::comment) {
			if (const auto source = selected_writing_source(false)) {
				add(source->Id, source->Text, 4096);
			}
		}
		return sources;
	}

	void github_application::implementation::request_writing(core::writing_hook aHook, qt::prose_capture aCapture)
	{
		cancel_writing();
		if (Closing || !TourHost || !WindowRead->accepted_snapshot() || !Window->tour_editor()->writing_visible(aHook)) {
			return;
		}
		if (aHook == core::writing_hook::comment) {
			observe_tour_visit();
			const auto* stop  = Window->tour_editor()->selected_stop();
			const auto  draft = TourHost->presentation().Draft;
			if (draft && stop && stop->Suggestion && stop->Preparation == core::tour_preparation::ready) {
				TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = stop->Id, .Value = *stop });
			}
		}
		const auto target = writing_target(aHook, aCapture);
		if (!target || !tour_session_idle()) {
			// [[BPR-1878]]. Says which it is, naming only what the tab shows:
			// the agent picker at its bottom.
			const auto agent = writing_agent();
			Window->tour_editor()->show_writing_notice(Window->agent_panel()->offered_count() == 0 ? QStringLiteral("No agent is set up. Add one in Settings, Agents, or keep writing yourself.")
														   : !agent                                ? QStringLiteral("Choose an agent with the agent picker in the window's footer, or keep writing yourself.")
														   : !target                               ? QStringLiteral("Only your own tour draft, open in this window, can be drafted for. You can keep writing yourself.")
																								   : QStringLiteral("The tour session is drafting. Wait for it to finish or stop it, or keep writing yourself."));
			return;
		}
		const auto request = WritingState.begin(*target, std::move(aCapture.Raw), aCapture.Selection);
		if (!request) {
			return;
		}
		auto sources = writing_sources(aHook, *request);
		if (!writes_tool_free(target->Agent)) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("This agent has no qualified tool-free writing mode: writing needs one that speaks Claude Code's stream JSON. Add one in Settings › Agents, or keep writing manually."));
			return;
		}
		if (request->Scope == core::writing_scope::empty_field && (sources.empty() || (aHook == core::writing_hook::comment && std::ranges::find(sources, "stop-source", &application::writing_source::Id) == sources.end()))) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("Not enough approved context to draft an explanation: the stop's code could not be read."));
			return;
		}
		// [[BPR-2140]]. The built-in prompt's instructions apart from the material.
		const auto prompt = application::build_writing_prompt_parts(Profile->WorkbenchPreferences->accepted()->WritingHooks, *request, sources);
		if (!prompt) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("This request exceeds the writing context limit. Select a smaller passage or shorten the hook instructions."));
			return;
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1900]]. Draft from PR context and Suggest an outline read the
		// repository in the background; a small rewrite stays tool-free.
		const auto kind     = core::tour_request_of(aHook, request->Scope);
		auto       readOnly = core::tools_for(kind) == core::tour_agent_tools::read_only;
		// [[BPR-2141]], RFC-BPR-0101 AC13. Draft from PR context and Suggest
		// an outline are one tool-free call with the diff up front; past the
		// budget they read the repository, and the notice says so.
		auto files = std::vector<application::tour_draft_file>{ };
		auto bytes = std::size_t{ 0U };
		auto route = core::tour_context_route::up_front;
		if (readOnly) {
			files    = tour_request_files();
			bytes    = application::tour_diff_bytes(files);
			route    = core::choose_tour_context(bytes, true);
			readOnly = route != core::tour_context_route::up_front;
		}
		const auto upFront = core::tools_for(kind) == core::tour_agent_tools::read_only && !readOnly;
		const auto notice  = readOnly || upFront ? tour_context_notice(route, bytes) : QStringLiteral("Writing assistance running… Your text remains editable.");
		PendingWriting     = writing_operation{ *request, std::move(sources), { }, false, readOnly, std::nullopt };
		Window->tour_editor()->show_writing_notice(notice);
		const auto asked = Window->tour_editor()->request_name(aHook);
		// [[BPR-1916]]. The skills the hook names, inline. [[BPR-2051]] A
		// stop's Enhance sends the stop's Enhance prompt, and its skills.
		auto skills         = skill_run_for(core::writing_instructions_for(aHook, request->Scope));
		auto text           = prompt->Message + (upFront ? application::render_up_front_diff(files) : std::string{ }) + skills.Directive;
		TourRunInstructions = (readOnly      ? std::string{ application::read_only_writing_preamble() }
								   : upFront ? std::string{ application::up_front_writing_preamble() }
											 : std::string{ })
			+ prompt->Instructions;
		TourRunUpFront = upFront;
		if (skills.Note) {
			Window->tour_editor()->show_writing_notice(notice + QStringLiteral(" ") + QString::fromStdString(*skills.Note));
		}
		if (!dispatch_tour_run(target->Agent, kind, asked.toStdString(), text, readOnly || upFront ? std::optional{ aHook } : std::nullopt, std::nullopt, core::tour_thread{ }, false, std::move(skills))) {
			TourRunUpFront = false;
			cancel_writing();
		}
#else
		PendingWriting = writing_operation{ *request, std::move(sources), { }, false, false, std::nullopt };
		cancel_writing();
#endif
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// [[BPR-1900]]. The tour session's process chain and host: its own
	/// launcher and lane, the Claude wrapper, program and added-agent
	/// ports the Agent tab's launches use, and its own projection.
	void github_application::implementation::compose_tour_session(const std::vector<core::agent_profile>& aProfiles, const std::vector<core::agent_host_policy>& aPolicies)
	{
		// Invalidate even a same-address replacement before any owned port dies.
		++TourIdleObservationGeneration;
		if (TourIdleObservation) {
			TourIdleObservation->stop();
		}
		TourIdleObservedHost = nullptr;
		TourIdleObservedEditor.clear();
		TourProcess = std::make_unique<adapters::agent::agent_process_launcher>();
	#if defined(BPR_BUILD_TESTING)
		// Counted as the Agent tab's launcher is, so a course sees a tour
		// run start and finish.
		TourProcessProbe = std::make_unique<composition_agent_process_launcher_probe>(*TourProcess);
		// [[BPR-2177]]. The tour's runs keep to the fixture agent too.
		if (Fixture) {
			TourProcessProbe->set_admission(fixture_agent_admission());
		}
		auto& launcher = static_cast<application::agent_process_port&>(*TourProcessProbe);
	#else
		auto& launcher = static_cast<application::agent_process_port&>(*TourProcess);
	#endif
		TourClaudeProcess    = std::make_unique<application::wrapped_claude_process>(launcher, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		TourLocatedProcess   = std::make_unique<application::located_agent_process>(*TourClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper, locate_agent_program());
		TourEntryProcess     = std::make_unique<application::agent_entry_process>(launcher, *TourLocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper, locate_claude(), locate_agent_program());
		TourDiagnosedProcess = std::make_unique<application::diagnosed_agent_process>(*TourEntryProcess, DiagnosticLog);
		TourProjection       = std::make_unique<tour_session_projection>(
			[this](const application::agent_invocation_projection_identity& aIdentity) { tour_run_admitted(aIdentity); },
			[this](const application::agent_invocation_projection_identity& aIdentity, const application::prepared_workspace& aWorkspace) {
				TourWorkspacePath = aWorkspace.Path;
				TourWorkspaceHead = aWorkspace.PinnedHead;
				tour_run_started(aIdentity, aWorkspace);
			},
			[this](const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult) { tour_run_finished(aIdentity, aResult); },
			[this](const std::string_view aReason) { tour_run_refused(aReason); },
			[this](const application::agent_invocation_projection_identity& aIdentity, const application::agent_stream_event& aEvent) { tour_run_streamed(aIdentity, aEvent); });
		TourSession = std::make_unique<application::agent_invocation_host>(
			aProfiles, aPolicies, AgentWorkspaceNeighbourhood, *this, *EpochHost, *TourDiagnosedProcess,
			[](const std::string& aName) -> std::optional<std::string> {
				if (!qEnvironmentVariableIsSet(aName.c_str())) {
					return std::nullopt;
				}
				return qEnvironmentVariable(aName.c_str()).toStdString();
			},
			TourAgentEffects, DecisionLane, *TourProjection, nullptr, NativeAgentSessions.get(), DiagnosticLog.get());
		// [[BPR-2181]]. The tour session's retirement event releases the
		// feedback queued for its draft.
		TourSession->set_retired_observer([this] { tour_runtime_retired(); });
		// [[BPR-2377]]. Tour chats are read-only and share the pull request's
		// workspace; a deleted one removes it only when nothing else uses it.
		TourSession->set_retained_workspace_files(RetainedWorkspaceFiles.get());
		TourProjection->NativeHistory = [this](const application::native_agent_history_projection& aView) { present_tour_native_history(aView); };
		TourProjection->NativeSession = [this](const core::agent_session& aSession) {
			if (Closing || !Window) {
				return;
			}
			if (const auto thread = Window->tour_editor()->side_chat()->current(); thread && TourHistoryKey == tour_native_agent() + ":" + tour_native_scope(*thread)) {
				TourNativeSessions[TourHistoryKey] = aSession;
				present_tour_continuation();
			}
		};
	}

	/// RFC-BPR-0110. A local draft's conversations run tool-free (no tools,
	/// no session saved, an empty folder of their own): the request the
	/// draft's host composed from the captured candidate is the only input,
	/// so nothing in the worktree can reach the agent. The agent is the one
	/// the window's agent picker names; it must speak Claude Code's stream JSON.
	void github_application::implementation::compose_local_draft_agent(const std::vector<core::agent_profile>& aProfiles)
	{
		if (!DraftDesktop) {
			return;
		}
		LocalDraftProcess        = std::make_unique<adapters::agent::agent_process_launcher>();
		LocalDraftClaudeProcess  = std::make_unique<application::wrapped_claude_process>(*LocalDraftProcess, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		LocalDraftLocatedProcess = std::make_unique<application::located_agent_process>(*LocalDraftClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper,
																						locate_agent_program());
		LocalDraftEntryProcess   = std::make_unique<application::agent_entry_process>(*LocalDraftProcess, *LocalDraftLocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper,
																					  locate_claude(), locate_agent_program());
		// [[BPR-2264]]. A person reads every answer a local draft's agent
		// gives (description, discussion, commit message, a check's plan and
		// report), so every run carries the Writing style. The chooser, on
		// this thread, reads it from the saved prompts before each request;
		// the runner, on the agent lane, appends it to the launch.
		struct local_writing_style
		{
			std::mutex  Lock;
			std::string Text;
		};
		auto style  = std::make_shared<local_writing_style>();
		auto runner = [profiles = std::make_shared<const std::vector<core::agent_profile>>(aProfiles), process = LocalDraftEntryProcess.get(), root = ApplicationDataRoot, style](
						  const application::local_draft_agent_run& aRun, const std::stop_token aStop, const std::vector<core::supplement_skill>& aSkills = { }) -> application::local_draft_agent_answer {
			const auto profile = std::ranges::find_if(*profiles, [&aRun](const core::agent_profile& aProfile) { return aProfile.Id.value() == aRun.Profile; });
			if (profile == profiles->end()) {
				return { .Failure = "That agent is no longer set up. Choose one with the agent picker." };
			}
			auto supplement    = core::agent_context_supplement{ };
			supplement.Session = core::agent_session{ .Id = core::agent_session_id{ 1U }, .Profile = profile->Id };
			supplement.Message = aRun.Prompt;
			if (!aSkills.empty()) {
				supplement.Skills = aSkills;
				supplement.Message += application::inline_skill_directive();
			}
			auto options = application::agent_launch_options{ .WritingOnly = true, .Path = aRun.Path, .SystemInstructions = aRun.Instructions, .Instructions = aRun.Delivery };
			{
				const auto lock      = std::scoped_lock{ style->Lock };
				options.WritingStyle = style->Text;
			}
			// RFC-BPR-0120. A check runs in the candidate's check copy, never
			// the worktree: its plan with read-only tools, its run with the
			// draft's grants for this agent (RFC-BPR-0104). The copy's own
			// agent settings are the change's, so they are never loaded.
			const auto checking = aRun.Check != application::local_check_mode::none;
			if (checking) {
				auto spellings = application::agent_checkout_spellings(aRun.Folder);
				if (!spellings) {
					return { .Failure = "The check copy could not be resolved, so its own agent settings could not be shut out. Nothing ran." };
				}
				options.WritingOnly         = false;
				options.PullRequestCheckout = std::move(*spellings);
				options.ReadOnlyTools       = aRun.Check == application::local_check_mode::plan;
				if (aRun.Check == application::local_check_mode::run) {
					auto grant = application::agent_launch_grant{ .ScratchLimitMiB = aRun.ScratchLimitMiB };
					if (!aRun.Scratch.empty() && application::valid_agent_grant_path(aRun.Scratch)) {
						grant.ScratchFolder = aRun.Scratch;
					}
					if (aRun.Builds && application::valid_agent_build_path(aRun.Folder)) {
						grant.BuildWorkspace = aRun.Folder;
					}
					if (!grant.ScratchFolder.empty() || !grant.BuildWorkspace.empty()) {
						options.Grant = std::move(grant);
					}
				}
			}
			const auto launch = application::build_agent_launch(
				*profile, supplement,
				[](const std::string& aName) -> std::optional<std::string> {
					if (!qEnvironmentVariableIsSet(aName.c_str())) {
						return std::nullopt;
					}
					return qEnvironmentVariable(aName.c_str()).toStdString();
				},
				options);
			if (!launch) {
				return { .Failure = launch.error() == application::agent_launch_failure::input_too_large
							 ? "The question is longer than one request can carry."
							 : "This agent cannot answer without tools here. Choose one that speaks Claude Code's stream JSON." };
			}
			auto request = *launch;
			// [[BPR-2138]]. The draft's discussion is a native session: it
			// runs in an empty folder kept for the draft, where the agent
			// saves it, and a later turn resumes it there by its handle.
			if (core::session_mode_of(aRun.Path) == core::agent_session_mode::resumed) {
				const auto folder = root / "local-draft-sessions-v1" / aRun.Draft;
				if (!core::valid(core::local_draft_id{ aRun.Draft }) || !adapters::sqlite::create_storage_root_owner_private(folder)) {
					return { .Failure = "The folder this draft's conversation runs in could not be made." };
				}
				request.NativeHandle     = aRun.Handle;
				request.WorkingDirectory = folder.string();
				auto resumed             = application::prepare_native_agent_turn(request, false);
				if (!resumed) {
					return { .Failure = "This agent cannot resume the conversation's session." };
				}
				if (aRun.Run != 0U) {
					// [[BPR-2154]]. A discussion question runs on a kept
					// connection of its own number, which resumes the session
					// as it starts: a message steered while it works reaches
					// it, as the pull request's tour chat's does. The
					// connection ends with the turn.
					request.NativeConversation = application::local_draft_native_conversation(aRun.Draft, aRun.Run);
					request.Run                = core::agent_run_id{ aRun.Run };
				} else {
					request = std::move(*resumed);
				}
			}
			// RFC-BPR-0103. A check's activity is heard as it happens: the
			// commands it runs are what its report lists.
			auto activity = core::agent_turn_activity{ };
			auto sink     = application::agent_output_sink{ };
			if (checking) {
				request.WorkingDirectory = aRun.Folder;
				sink                     = [&activity, &aRun, started = std::chrono::steady_clock::now()](const application::agent_stream_event& aEvent) {
					const auto at = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
					activity      = aEvent.Kind == application::agent_stream_kind::activity && aEvent.Activity ? core::apply_agent_activity(std::move(activity), *aEvent.Activity, at)
																											   : core::note_agent_frame(std::move(activity), at);
					if (aRun.Progress) {
						aRun.Progress(activity);
					}
				};
			} else if (aRun.Steered) {
				// [[BPR-2154]] (look queue-tour-v002). A message steered into
				// the question is told as it is written, so its turn shows it.
				sink = [&aRun](const application::agent_stream_event& aEvent) {
					if (aEvent.Kind == application::agent_stream_kind::steered && aEvent.Steered) {
						aRun.Steered({ .Text = aEvent.Steered->Text, .AfterSteps = aEvent.Steered->AfterSteps });
					}
				};
			}
			const auto result = process->run(request, (aRun.Check == application::local_check_mode::run ? 60 : 10) * 60 * 1000, sink, aStop);
			// [[BPR-2154]]. What was steered into the run, settled by its end.
			auto steered = std::vector<application::local_draft_steered>{ };
			for (const auto& input : result.SteeredInputs) {
				steered.push_back({ .Identity = input.Identity, .Text = input.Text, .AfterSteps = input.AfterSteps, .Disposition = input.Disposition });
			}
			if (aStop.stop_requested()) {
				return { .Failure = "Stopped.", .Commands = activity.CommandsRun, .Stopped = true, .Steered = std::move(steered) };
			}
			if (result.Outcome != application::agent_run_outcome::success) {
				return { .Failure = "The agent did not answer (" + std::string{ application::name(result.Outcome) } + ").", .Commands = activity.CommandsRun, .Steered = std::move(steered) };
			}
			auto answer = application::local_draft_agent_answer{ .Answered = true, .Handle = result.NativeHandle, .Commands = activity.CommandsRun, .Steered = std::move(steered) };
			for (const auto& item : result.AssistantItems) {
				answer.Text += answer.Text.empty() ? item : "\n\n" + item;
			}
			if (result.TurnCost) {
				answer.CostMicros = *result.TurnCost / 1000U;
			} else if (result.ReportedCostUsd) {
				if (const auto cost = core::parse_agent_cost(*result.ReportedCostUsd)) {
					answer.CostMicros = *cost / 1000U;
				}
			}
			return answer;
		};
		// [[BPR-2281]]. Draft Description runs on the same agent runner, one
		// fresh, tool-free run, on a lane of its own.
		if (!DescriptionDraftLane) {
			DescriptionDraftLane = std::make_unique<worker_executor>();
		}
		if (!DescriptionDraft) {
			DescriptionDraft = std::make_unique<application::description_draft_host>(*DescriptionDraftLane, DecisionLane,
																					 [this](const application::description_draft_projection& aProjection) { present_description_draft(aProjection); });
		}
		DescriptionDraft->set_runner([runner, style](const application::description_draft_agent& aAgent, const std::string& aMessage, const std::stop_token aStop) -> std::expected<std::string, std::string> {
			{
				const auto lock = std::scoped_lock{ style->Lock };
				style->Text     = aAgent.WritingStyle;
			}
			auto answer = runner({ .Agent = aAgent.Agent, .AgentName = aAgent.AgentName, .Profile = aAgent.Profile, .Prompt = aMessage, .Instructions = aAgent.Instructions, .Path = core::agent_run_path::local_description_first_draft, .Delivery = aAgent.Delivery },
								 aStop);
			if (!answer.Answered) {
				return std::unexpected{ answer.Failure.empty() ? std::string{ "The agent did not answer." } : answer.Failure };
			}
			return answer.Text;
		});
		// [[BPR-2286]]. Each follow-up round is one fresh run. [[BPR-2370]]:
		// it reads the pull request's workspace with read-only tools, on the
		// side chat's own session (`launch_follow_up_round`).
		if (!FollowUpLane) {
			FollowUpLane = std::make_unique<worker_executor>();
		}
		if (!FollowUp) {
			FollowUp = std::make_unique<application::follow_up_host>(*FollowUpLane, DecisionLane, [this](const application::follow_up_projection& aProjection) { present_follow_up(aProjection); });
			// [[BPR-2322]]. Each side chat question, proposal and Use is logged,
			// lengths and outcomes only.
			FollowUp->set_log(Profile->DiagnosticLog);
		}
		FollowUp->set_launcher([this](const application::follow_up_agent& aAgent, const std::string& aMessage, std::stop_token aStop, application::follow_up_answer aAnswer) {
			launch_follow_up_round(aAgent, aMessage, std::move(aStop), std::move(aAnswer));
		});
		DraftDesktop->set_agent(std::move(runner), &TourAgentEffects, [this, style]() -> std::optional<local_draft_desktop::agent_choice> {
			const auto  profile = writing_agent();
			const auto& prefs   = Profile->WorkbenchPreferences->accepted();
			if (Closing || !profile || !writes_tool_free(*profile) || !prefs) {
				Window->tour_editor()->show_notice(QStringLiteral("Choose an agent that speaks Claude Code's stream JSON with the agent picker, or keep writing yourself."));
				return std::nullopt;
			}
			{
				const auto lock = std::scoped_lock{ style->Lock };
				style->Text     = application::writing_style_for(prefs->WritingHooks, true);
			}
			auto identity = tour_native_agent();
			if (identity.empty()) {
				identity = "profile-" + std::to_string(profile->value());
			}
			// [[BPR-2140]]. The agent's own switch says where instructions go.
			auto delivery = core::agent_instruction_delivery::system_prompt;
			if (AgentSetup) {
				if (const auto* const entry = core::entry_of_profile(AgentSetup->projection().Preferences.Agents, *profile); entry != nullptr) {
					delivery = entry->Instructions;
				}
			}
			return local_draft_desktop::agent_choice{
				.Identity           = std::move(identity),
				.Name               = agent_display_name(*profile).toStdString(),
				.Profile            = profile->value(),
				.Description        = std::string{ application::effective_writing_hook(prefs->WritingHooks, core::writing_hook::local_description) },
				.DescriptionVersion = std::string{ application::writing_hook_version_of(core::writing_hook::local_description) },
				.Discussion         = std::string{ application::effective_writing_hook(prefs->WritingHooks, core::writing_hook::local_discussion) },
				.DiscussionVersion  = std::string{ application::writing_hook_version_of(core::writing_hook::local_discussion) },
				.CommitMessage      = std::string{ application::effective_writing_hook(prefs->WritingHooks, core::writing_hook::local_commit_message) },
				.Delivery           = delivery,
				.Check              = std::string{ application::effective_writing_hook(prefs->WritingHooks, core::writing_hook::local_run_check) },
				.CheckVersion       = std::string{ application::writing_hook_version_of(core::writing_hook::local_run_check) },
			};
		});
		// RFC-BPR-0120. A check's copies are scratch folders: counted,
		// cleared and aged out with them (RFC-BPR-0104). No root, no check.
		if (!AgentScratchRoot.empty()) {
			DraftDesktop->set_check_copies(std::make_unique<application::local_check_copies>(AgentScratchRoot), [this] {
				return core::agent_scratch_limit_bytes(AgentSetup ? AgentSetup->projection().Preferences.ScratchLimitMiB : 0U);
			});
		}
	}

	/// [[BPR-1900]]. What a drafting run says while it reads, and that a
	/// review without a clone reads only the pull request's files.
	auto github_application::implementation::tour_reading_notice() const -> QString
	{
		const auto sparse = AgentReview && AgentReview->Sparse && !linked_history_clone(false);
		return sparse ? QStringLiteral("Drafting in the background. No clone is linked, so it reads only this pull request's files. Keep writing.")
					  : QStringLiteral("Drafting in the background from the repository. Keep writing.");
	}

	/// [[BPR-1900]]. Whether an agent has a recorded read-only allowlist.
	auto github_application::implementation::reads_read_only(const core::agent_profile_id aProfile) const -> bool
	{
		if (writes_tool_free(aProfile) || (aProfile == core::codex_jsonl_profile_id || aProfile == core::codex_app_server_profile_id)) {
			return true;
		}
		if (core::is_agent_entry_profile(aProfile) && AgentSetup) {
			const auto        agents = AgentSetup->projection().Preferences.Agents;
			const auto* const entry  = core::entry_of_profile(agents, aProfile);
			return entry != nullptr && (entry->Protocol == core::agent_protocol::codex_jsonl_exec || entry->Protocol == core::agent_protocol::codex_app_server);
		}
		return false;
	}

	/// [[BPR-1900]]. The one way a tour request reaches the tour session:
	/// the core starts the run in the session, then the launch path sends it
	/// (`aRunFor` names the hook whose control shows it running).
	auto github_application::implementation::dispatch_tour_run(const core::agent_profile_id aAgent, const core::tour_agent_request aKind, const std::string& aAsked, const std::string& aPrompt, const std::optional<core::writing_hook> aRunFor,
															   const std::optional<core::tour_passage>& aPassage, const core::tour_thread& aThread, const bool aUpdate, skill_run aSkills) -> bool
	{
		adopt_tour_conversation();
		// [[BPR-2140]]. This run's instructions, never the next one's.
		auto instructions = std::move(TourRunInstructions);
		TourRunInstructions.clear();
		const auto fresh = std::exchange(TourAskFresh, false) && aKind == core::tour_agent_request::ask;
		// [[BPR-2138]], RFC-BPR-0101 (owner, 2026-11-12). Only a turn of a
		// side chat or of the whole-tour conversation resumes; Draft Tour's
		// first draft, a field's draft and an Enhance run fresh.
		const auto conversational = aKind == core::tour_agent_request::ask && !fresh;
		const auto continued      = conversational ? continued_tour_session(aThread) : std::nullopt;
		const auto key            = tour_native_agent() + ":" + tour_native_scope(aThread);
		const auto session        = TourNativeSessions.find(key);
		const auto observed       = tour_observed_binding();
		if (conversational && !continued && session != TourNativeSessions.end() && observed && core::offer_agent_continuations(session->second, *observed).Required) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("This chat reads an earlier revision. Choose Continue or New session on its card before asking."));
			present_tour_continuation();
			return false;
		}
		// [[BPR-2141]]. A drafting run with its context up front has no tools.
		const auto upFront = std::exchange(TourRunUpFront, false) && aKind != core::tour_agent_request::ask;
		const auto tools   = upFront ? core::tour_agent_tools::none : core::tools_for(aKind);
		const auto run     = aKind == core::tour_agent_request::ask ? core::begin_tour_ask(TourAgent, aThread, aPassage, aAsked, aUpdate) : core::begin_tour_run(TourAgent, aKind, tools, aAsked);
		if (!run) {
			return false;
		}
		TourRunScope.clear();
		if (fresh) {
			if (auto found = std::ranges::find(TourAgent.Runs, *run, &core::tour_agent_run::Id); found != TourAgent.Runs.end()) {
				found->Fresh = true;
			}
		}
		if ((conversational || aKind == core::tour_agent_request::draft_tour) && !tour_native_agent().empty()) {
			TourRunScope = tour_native_agent() + ":" + tour_native_scope(aThread);
			if (auto found = std::ranges::find(TourAgent.Runs, *run, &core::tour_agent_run::Id); found != TourAgent.Runs.end()) {
				found->ConversationScope = TourRunScope;
				found->Conversation      = TourSelectedConversations[TourRunScope];
			}
		}
		TourSubmittedDraft = TourHost && TourHost->presentation().Draft ? std::optional{ *TourHost->presentation().Draft } : std::nullopt;
		if (TourSubmittedDraft) {
			if (auto found = std::ranges::find(TourAgent.Runs, *run, &core::tour_agent_run::Id); found != TourAgent.Runs.end()) {
				for (const auto& stop : TourSubmittedDraft->Stops) {
					found->SubmittedStops.push_back(stop.Id);
				}
			}
		}
		TourRun        = *run;
		TourRunStarted = std::chrono::steady_clock::now();
		TourInvocation.reset();
		TourWorkspacePath.clear();
		TourWorkspaceHead.reset();
		auto launch               = tour_launch{ .NativeScope = TourRunScope.empty() || !conversational ? std::string{ } : tour_native_scope(aThread), .NativeQuestion = aAsked, .ReadOnly = tools == core::tour_agent_tools::read_only, .Skills = std::move(aSkills) };
		launch.Path               = aKind == core::tour_agent_request::draft_tour ? core::agent_run_path::draft_tour_first_draft
			: aKind == core::tour_agent_request::rewrite || fresh                 ? core::agent_run_path::tour_enhance
			: aKind == core::tour_agent_request::ask                              ? (aThread.Kind == core::tour_thread_kind::tour ? core::agent_run_path::tour_conversation : core::agent_run_path::tour_side_chat)
																				  : core::agent_run_path::tour_field_draft;
		launch.SystemInstructions = std::move(instructions);
		launch.UpFront            = upFront;
		if ((aKind == core::tour_agent_request::draft_tour || aKind == core::tour_agent_request::ask) && WindowRead->accepted_snapshot()) {
			for (const auto& file : WindowRead->accepted_snapshot()->Files) {
				if (file.Change != application::github_file_change::removed) {
					launch.Measure.push_back(file.Path);
				}
			}
		}
		refresh_tour_host_idle();
		if (!local_tour_shown()) {
			Window->tour_editor()->set_tour_running(aRunFor, aKind == core::tour_agent_request::draft_tour, aKind == core::tour_agent_request::ask);
		}
		present_tour_session();
		if (!dispatch_agent_invocation(aAgent, { }, { }, false, aPrompt, continued, core::agent_entry_point::ask_about_selection, { }, true, launch)) {
			static_cast<void>(core::finish_tour_run(TourAgent, *run, core::tour_agent_run_state::failed, "The tour session could not start this request."));
			TourRun.reset();
			if (!local_tour_shown()) {
				Window->tour_editor()->set_tour_running(std::nullopt, false);
			}
			present_tour_session();
			return false;
		}
		return true;
	}

	void github_application::implementation::tour_run_admitted(const application::agent_invocation_projection_identity& aIdentity)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		TourInvocation = aIdentity;
		if (PendingWriting && !PendingWriting->Invocation) {
			PendingWriting->Invocation = aIdentity;
		}
		// [[BPR-1941]]. It makes its workspace first, as the Agent tab says.
		auto live  = qt::agent_run_status{ };
		live.Agent = agent_display_name(aIdentity.Session.Profile);
		live.Stage = qt::agent_run_stage::preparing;
		present_tour_live(std::move(live));
	}

	void github_application::implementation::tour_run_started(const application::agent_invocation_projection_identity& aIdentity, const application::prepared_workspace& aWorkspace)
	{
		if (Closing || Window == nullptr || TourInvocation != std::optional{ aIdentity }) {
			return;
		}
		auto live              = TourLive.value_or(qt::agent_run_status{ });
		live.Agent             = agent_display_name(aIdentity.Session.Profile);
		live.Stage             = qt::agent_run_stage::thinking;
		live.FilesOnly         = aWorkspace.Shallow;
		live.Activity          = std::nullopt;
		live.ActivityStartedMs = qt::agent_activity_clock_ms();
		present_tour_live(std::move(live));
	}

	void github_application::implementation::tour_run_streamed(const application::agent_invocation_projection_identity& aIdentity, const application::agent_stream_event& aEvent)
	{
		if (Closing || Window == nullptr || TourInvocation != std::optional{ aIdentity } || !TourLive) {
			return;
		}
		auto live = *TourLive;
		// [[BPR-2261]]. Its kept workspace is checked, not made.
		if (aEvent.Kind == application::agent_stream_kind::workspace_check) {
			if (qt::agent_run_before_start(live.Stage)) {
				live.Stage = qt::agent_run_stage::checking;
				present_tour_live(std::move(live));
			}
			return;
		}
		// [[BPR-2005]], RFC-BPR-0103. The side chat's running block hears
		// what the agent and its helpers do, and every frame.
		const auto at = static_cast<std::uint64_t>(std::max<qint64>(0, qt::agent_activity_clock_ms() - live.ActivityStartedMs));
		if (aEvent.Kind == application::agent_stream_kind::activity && aEvent.Activity) {
			live.Activity = core::apply_agent_activity(live.Activity.value_or(core::agent_turn_activity{ }), *aEvent.Activity, at);
		} else if (aEvent.Kind == application::agent_stream_kind::heartbeat || live.Activity) {
			live.Activity = core::note_agent_frame(live.Activity.value_or(core::agent_turn_activity{ }), at);
		}
		if (aEvent.Kind == application::agent_stream_kind::text) {
			live.Stage = qt::agent_run_stage::writing;
		}
		// [[BPR-2252]]. The turn's cost so far, on its running line.
		if (aEvent.Kind == application::agent_stream_kind::cost) {
			live.Cost = aEvent.Cost;
		}
		present_tour_live(std::move(live));
		// [[BPR-2154]] (look queue-tour-v002). A message steered into the
		// part's running turn is the author's in its chat at once, where it
		// went in, as the Agent tab shows it.
		if (aEvent.Kind == application::agent_stream_kind::steered && aEvent.Steered && TourRun
			&& core::note_tour_steered(TourAgent, *TourRun, { .Text = aEvent.Steered->Text, .AfterSteps = aEvent.Steered->AfterSteps })) {
			present_tour_session();
		}
	}

	void github_application::implementation::present_tour_live(std::optional<qt::agent_run_status> aStatus)
	{
		TourLive = std::move(aStatus);
		if (Window != nullptr && Window->tour_editor() != nullptr) {
			Window->tour_editor()->present_run_status(TourLive);
		}
		// [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"). The
		// window's footer says the run and where it started, with Stop.
		if (Window != nullptr && Window->agent_footer_widget() != nullptr) {
			if (TourLive) {
				if (!TourFooterPlace) {
					TourFooterPlace = tour_footer_place();
					TourFooterStarted.start();
				}
				Window->agent_footer_widget()->present_run(*TourFooterPlace, qt::agent_footer_run{ .Status = *TourLive, .Started = TourFooterStarted, .Stop = [this] { cancel_tour_run(); } });
			} else if (TourFooterPlace) {
				Window->agent_footer_widget()->present_run(*TourFooterPlace, std::nullopt);
			}
		}
	}

	auto github_application::implementation::tour_footer_place() const -> qt::agent_footer_place
	{
		if (TourDraftPending) {
			return qt::agent_footer_place::draft_tour;
		}
		if (TourAskPending) {
			return qt::agent_footer_place::tour_chat;
		}
		if (PendingWriting && PendingWriting->Enhance) {
			return qt::agent_footer_place::enhance;
		}
		// A tour field's Enhance is Enhance; Draft from PR context and
		// Generate comment are the Tour tab's.
		if (PendingWriting && Window != nullptr && Window->tour_editor()->request_name(PendingWriting->Request.Target.Hook).startsWith(QStringLiteral("Enhance"))) {
			return qt::agent_footer_place::enhance;
		}
		return qt::agent_footer_place::tour_tab;
	}

	void github_application::implementation::finish_tour_footer(const application::agent_run_result& aResult)
	{
		if (!TourFooterPlace) {
			return;
		}
		const auto place   = *TourFooterPlace;
		const auto seconds = TourFooterStarted.isValid() ? TourFooterStarted.elapsed() / 1000 : 0;
		TourFooterPlace.reset();
		auto& cost = TourFooterCosts[static_cast<std::size_t>(place)];
		if (aResult.TurnCost) {
			cost = cost.value_or(0U) + *aResult.TurnCost;
		}
		if (Window == nullptr || Window->agent_footer_widget() == nullptr) {
			return;
		}
		auto* const footer = Window->agent_footer_widget();
		footer->present_cost(place, cost);
		auto ending = aResult.Outcome == application::agent_run_outcome::success ? QStringLiteral("answered in %1").arg(qt::agent_elapsed_words(seconds))
			: aResult.Outcome == application::agent_run_outcome::cancelled       ? QStringLiteral("stopped after %1").arg(qt::agent_elapsed_words(seconds))
																				 : QStringLiteral("didn't finish");
		if (aResult.TurnCost) {
			ending += QStringLiteral(" · ") + QString::fromStdString(core::agent_cost_words(*aResult.TurnCost));
		}
		footer->present_finished(place, ending);
	}

	void github_application::implementation::tour_run_refused(const std::string_view aReason)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		if (TourRun) {
			// [[BPR-1935]]. A refusal for size says so in words.
			static_cast<void>(core::finish_tour_run(TourAgent, *TourRun, core::tour_agent_run_state::failed, application::tour_refusal_words(aReason)));
			TourRun.reset();
		}
		present_tour_live(std::nullopt);
		TourFooterPlace.reset();
		if (!local_tour_shown()) {
			Window->tour_editor()->set_tour_running(std::nullopt, false);
		}
		TourAskPending = false;
		// [[BPR-2154]]. A part's released message no run took goes back
		// first, and its queue pauses.
		if (const auto sending = core::parse_session_key(ChatQueueSending); sending && sending->Kind == core::queued_session_kind::tour_part) {
			chat_queue_retired(core::queued_finish::refused);
		}
		if (TourDraftPending) {
			TourDraftPending = false;
			Window->tour_editor()->show_tour_draft_failure(aReason == "agent_context_too_large"
															   ? QStringLiteral("This pull request is too large to describe to the agent in one request.")
															   : QStringLiteral("The selected agent can't draft with read-only tools. Choose Claude or Codex with the agent picker in the window's footer."));
		}
		present_tour_session();
		if (!PendingWriting) {
			return;
		}
		if (const auto enhance = PendingWriting->Enhance) {
			PendingWriting.reset();
			WritingState.cancel();
			answer_enhancement(*enhance, std::nullopt, QStringLiteral("The agent refused this request. Your text is unchanged."));
			return;
		}
		const bool test = PendingWriting->Test;
		PendingWriting.reset();
		WritingState.cancel();
		if (test) {
			Window->writing_hooks()->show_notice("Writing request was refused by the selected agent host.");
		} else {
			Window->tour_editor()->show_writing_notice(QStringLiteral("Writing request was refused by the selected agent host. Manual editing is available."));
		}
	}

	void github_application::implementation::tour_run_finished(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		auto words = aResult.RefusalReason.empty() ? qt::agent_tab_view::run_failure_words(aResult.Outcome, agent_display_name(aIdentity.Session.Profile)) : QString::fromStdString(aResult.RefusalReason);
		// [[BPR-2004]]. A runtime that ended under its helpers says so.
		if (aResult.ProtocolFailure == application::agent_protocol_failure::helpers_unfinished) {
			words = QStringLiteral("%1 stopped before its %2 remaining helpers finished. What arrived is kept.").arg(qt::agent_short_name(agent_display_name(aIdentity.Session.Profile))).arg(aResult.UnfinishedHelpers.size());
		}
		if (TourRun) {
			if (!aResult.Conversation.empty()) {
				TourSelectedConversations[TourRunScope] = aResult.Conversation;
				TourConversationBindings[TourRunScope]  = aIdentity.Session.Binding;
				if (auto found = std::ranges::find(TourAgent.Runs, *TourRun, &core::tour_agent_run::Id); found != TourAgent.Runs.end()) {
					found->Conversation = aResult.Conversation;
					if (TourHistory.Scope == tour_native_scope(found->Thread) && !TourHistory.Replay) {
						TourNativeSessions[TourRunScope] = aIdentity.Session;
					}
				}
			}
			static_cast<void>(core::record_tour_activity(TourAgent, *TourRun, application::tour_transcript_of(aResult.ToolActivity, TourWorkspacePath)));
			// [[BPR-2154]]. Each steered message as the run ended.
			auto endings = std::vector<core::agent_steer_disposition>{ };
			for (const auto& input : aResult.SteeredInputs) {
				endings.push_back(input.Disposition);
			}
			static_cast<void>(core::settle_tour_steered(TourAgent, *TourRun, endings));
			const auto state = aResult.Outcome == application::agent_run_outcome::success ? core::tour_agent_run_state::answered
				: aResult.Outcome == application::agent_run_outcome::cancelled            ? core::tour_agent_run_state::cancelled
																						  : core::tour_agent_run_state::failed;
			// [[BPR-1913]]. A failed run's transcript keeps why, as the
			// failure's own log says it, so Copy run carries the reason.
			if (TourAskPending && state == core::tour_agent_run_state::answered) {
				// [[BPR-1914]]. The reply, and any change as a proposal.
				finish_tour_ask(*TourRun, aResult);
			} else {
				auto last = state == core::tour_agent_run_state::answered ? tour_answer_words(aResult)
					: state == core::tour_agent_run_state::cancelled      ? std::string{ "Stopped. Nothing in the tour changed." }
																		  : (words + QStringLiteral("\n\n") + qt::agent_launch_panel::failure_log(aResult)).toStdString();
				static_cast<void>(core::finish_tour_run(TourAgent, *TourRun, state, last));
			}
		}
		const auto run = TourRun;
		// [[BPR-1914]] v003. How long it worked, as "Worked for Ns" says.
		if (run) {
			static_cast<void>(core::time_tour_run(TourAgent, *run, static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - TourRunStarted).count())));
			// [[BPR-1999]]. What it cost, and its conversation's total.
			static_cast<void>(core::cost_tour_run(TourAgent, *run, aResult.TurnCost, aResult.SessionCost));
			// [[BPR-2005]]. The block folds under the turn's ending.
			if (TourLive && TourLive->Activity) {
				const auto at = static_cast<std::uint64_t>(std::max<qint64>(0, qt::agent_activity_clock_ms() - TourLive->ActivityStartedMs));
				Window->tour_editor()->side_chat()->record_run_activity(*run, core::end_agent_activity(*TourLive->Activity, at), static_cast<qint64>(at / 1000U));
			}
		}
		// [[BPR-2154]]. A tour part's corrections and released message
		// settle by this run, before its retirement releases the next.
		settle_tour_part_steers(aIdentity, aResult);
		TourRun.reset();
		TourInvocation.reset();
		finish_tour_footer(aResult);
		present_tour_live(std::nullopt);
		refresh_tour_host_idle();
		if (!local_tour_shown()) {
			Window->tour_editor()->set_tour_running(std::nullopt, false);
		}
		if (TourDraftPending) {
			TourDraftPending = false;
			finish_tour_draft(run, aResult, words);
			// [[BPR-1930]] AC3, RFC-BPR-0126 Q1. Feedback given while it
			// drafted is the whole-tour conversation's first queued message:
			// it goes once the run has really ended (the host retired), to
			// revise what it proposed. A draft that didn't propose a tour says
			// so in the tour chat, with the feedback as written.
			if (!(aResult.Outcome == application::agent_run_outcome::success && TourProposal)) {
				return_tour_feedback("The draft didn't propose a tour, so this feedback wasn't sent. Send it again when you want.");
			} else {
				// Released once the session retires (its retirement event).
				drain_tour_queue();
			}
		} else if (TourAskPending) {
			TourAskPending = false;
			// Its run's retirement event settles it (`tour_runtime_retired`).
			TourQueueAnswered = TourQueueSending;
		} else {
			finish_writing(aIdentity, aResult);
		}
		present_tour_session();
	#if defined(BPR_BUILD_TESTING)
		if (auto before = std::exchange(BeforeTourRetirementFixture, { }); before) {
			before();
		}
	#endif
	}

	/// [[BPR-1914]], owner 2026-11-06: "I want to ask questions, get back
	/// and forth and have it update it." The session is one conversation
	/// per tour: a run started on another tour lets the earlier tour's
	/// turns and proposal go (run numbers keep counting).
	auto github_application::implementation::tour_native_scope(const core::tour_thread& aThread) const -> std::string
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		return draft ? "tour:" + std::to_string(draft->Id.value()) + ":" + std::to_string(draft->ChatGeneration) + ":" + std::to_string(static_cast<unsigned>(aThread.Kind)) + ":" + std::to_string(aThread.Stop.value()) : std::string{ };
	}
	auto github_application::implementation::tour_native_agent() const -> std::string
	{
		const auto profile = writing_agent();
		if (!profile || !AgentSetup) {
			return { };
		}
		const auto  preferences = AgentSetup->projection().Preferences;
		const auto* entry       = core::entry_of_profile(preferences.Agents, *profile);
		return entry ? entry->Identity : std::string{ };
	}
	void github_application::implementation::observe_tour_native_history(const core::tour_thread& aThread, std::optional<std::string> aChoice)
	{
		if (Closing || !Window || !TourSession || !writing_agent() || tour_native_agent().empty() || tour_native_scope(aThread).empty()) {
			return;
		}
		const auto key = tour_native_agent() + ":" + tour_native_scope(aThread);
		if (aChoice || TourHistoryKey != key) {
			TourContinuation.reset();
		}
		TourHistoryKey = key;
		TourSession->restore_native(*writing_agent(), tour_native_agent(), tour_native_scope(aThread), std::move(aChoice));
	}
	/// [[BPR-2377]]. Renames (`aName` set) or deletes one of the current tour
	/// thread's sessions, through the same host calls as the Agent tab's.
	void github_application::implementation::edit_tour_session(const std::string& aId, std::optional<std::string> aName)
	{
		const auto thread = Window != nullptr ? Window->tour_editor()->side_chat()->current() : std::nullopt;
		if (Closing || !Window || !TourSession || !thread || !writing_agent() || tour_native_agent().empty() || tour_native_scope(*thread).empty()) {
			return;
		}
		const auto renaming = aName.has_value();
		auto       answer   = [this, renaming](const application::agent_session_edit_receipt& aReceipt) {
			if (!Closing && Window) {
				Window->tour_editor()->side_chat()->present_session_edit(qt::agent_session_edit_words(aReceipt, renaming));
			}
		};
		if (renaming) {
			TourSession->rename_native_session(*writing_agent(), tour_native_agent(), tour_native_scope(*thread), aId, std::move(*aName), std::move(answer));
		} else if (aId.empty()) {
			TourSession->delete_all_native_sessions(*writing_agent(), tour_native_agent(), tour_native_scope(*thread), std::move(answer));
		} else {
			TourSession->delete_native_session(*writing_agent(), tour_native_agent(), tour_native_scope(*thread), aId, std::move(answer));
		}
	}

	void github_application::implementation::present_tour_native_history(const application::native_agent_history_projection& aView)
	{
		if (Closing || !Window) {
			return;
		}
		const auto thread = Window->tour_editor()->side_chat()->current();
		if (!thread || tour_native_scope(*thread) != aView.Scope) {
			return;
		}
		// The chat's notice and its carry-over item name the agent it asks.
		const auto agent = writing_agent();
		Window->tour_editor()->side_chat()->set_agent_name(agent ? agent_display_name(*agent) : QString{ });
		if (aView.ReadOnly) {
			auto readable = aView;
			for (auto& turn : readable.Turns) {
				if (auto answer = application::decode_tour_ask_answer(turn.Answered)) {
					turn.Answered = answer->Reply;
				}
			}
			Window->tour_editor()->side_chat()->present_native_history(readable);
			return;
		}
		TourHistory = aView;
		Window->tour_editor()->side_chat()->present_native_history(aView);
		if (aView.Loading) {
			return;
		}
		if (!aView.Failure.empty()) {
			Window->tour_editor()->show_writing_notice(QString::fromStdString(aView.Failure));
			return;
		}
		const auto key = tour_native_agent() + ":" + aView.Scope;
		if (TourSelectedConversations[key] != aView.Selected || aView.Replay || aView.Selected.empty()) {
			TourNativeSessions.erase(key);
			TourContinuation.reset();
		}
		TourSelectedConversations[key] = aView.Selected;
		if (const auto found = std::ranges::find(aView.Entries, aView.Selected, &application::native_agent_history_entry::Id); found != aView.Entries.end()) {
			TourConversationBindings[key] = found->Binding;
		} else {
			TourConversationBindings.erase(key);
		}

		if (!aView.Selected.empty() && std::ranges::none_of(TourAgent.Runs, [&](const auto& aRun) { return aRun.ConversationScope == key && aRun.Conversation == aView.Selected; })) {
			for (const auto& saved : aView.Turns) {
				auto turn              = core::tour_agent_run{ };
				turn.Id                = core::tour_agent_run_id{ ++TourAgent.LastRun };
				turn.Request           = core::tour_agent_request::ask;
				turn.Tools             = core::tour_agent_tools::read_only;
				turn.Thread            = *thread;
				turn.Question          = saved.Asked;
				turn.Steered           = saved.Steered;
				turn.ConversationScope = key;
				turn.Conversation      = aView.Selected;
				if (const auto answer = application::decode_tour_ask_answer(saved.Answered); answer) {
					turn.Reply = answer->Reply;
				} else if (const auto drafted = application::decode_tour_draft_answer(saved.Answered); drafted) {
					turn.Request = core::tour_agent_request::draft_tour;
					turn.Reply   = "Draft Tour";
					turn.Reply += "\n\nProposed stops: " + std::to_string(drafted->Stops.size()) + ". Retained history does not restore or accept a proposal.";
				} else {
					turn.Reply = saved.Answered;
				}
				turn.State = saved.Outcome == core::agent_turn_outcome::answered ? core::tour_agent_run_state::answered : core::tour_agent_run_state::failed;
				turn.Entries.push_back({ core::tour_transcript_kind::asked, turn.Question });
				turn.Entries.push_back({ core::tour_transcript_kind::answered, turn.Reply });
				TourAgent.Runs.push_back(std::move(turn));
				while (TourAgent.Runs.size() > application::maximum_conversation_turns) {
					const auto removable = std::ranges::find_if(TourAgent.Runs, [&](const auto& aRun) { return aRun.State != core::tour_agent_run_state::running && (!TourProposal || TourProposal->Run != aRun.Id); });
					if (removable == TourAgent.Runs.end()) {
						break;
					}
					TourAgent.Runs.erase(removable);
				}
			}
		}
		present_tour_session();
	}

	void github_application::implementation::adopt_tour_conversation()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!draft || core::running_tour_run(TourAgent) || TourConversationDraft == draft->Id) {
			return;
		}
		if (TourConversationDraft) {
			TourAgent = core::tour_agent_session{ .LastRun = TourAgent.LastRun };
			TourProposal.reset();
			TourAnchorFiles.clear();
		}
		TourConversationDraft = draft->Id;
	}

	auto github_application::implementation::capture_tour_agent_context(const core::tour_thread& aThread) const -> application::tour_agent_context
	{
		auto context      = application::tour_agent_context{ };
		context.Session   = &TourAgent;
		context.Attention = current_tour_attention();
		// Selection is captured independently of visibility; visibility only gates Ready.
		context.SelectedSource = selected_tour_source();
		if (EpochHost && EpochHost->state().AcceptedRevision) {
			context.ObservedBase = EpochHost->state().AcceptedRevision->Base;
			context.ObservedHead = EpochHost->state().AcceptedRevision->Head;
		}
		const auto binding = TourConversationBindings.find(tour_native_agent() + ":" + tour_native_scope(aThread));
		if (binding != TourConversationBindings.end()) {
			context.PinnedBase = binding->second.BaseOid;
			context.PinnedHead = binding->second.HeadOid;
		} else if (TourHistory.Scope == tour_native_scope(aThread) && !TourHistory.Loading && TourHistory.Failure.empty() && TourHistory.Selected.empty()) {
			context.PinnedBase = context.ObservedBase;
			context.PinnedHead = context.ObservedHead;
		}

		if (TourHost) {
			const auto shown      = TourHost->presentation();
			context.Saving        = shown.Saving;
			context.SaveFailed    = shown.Failure.has_value();
			context.StoreConflict = static_cast<bool>(shown.Conflict);
			for (const auto& destination : shown.Destinations) {
				context.Destinations.push_back(destination.Plan);
			}
		}
		if (TourPublication) {
			const auto shown           = TourPublication->presentation();
			context.PublicationAllowed = shown.Publish;
			context.PublicationStatus  = shown.Status;
			context.BodyEdits          = TourPublication->operation_support();
			context.Compatibility      = TourPublication->compatibility_chosen();
			context.Destinations.clear();
			if (TourHost && TourHost->presentation().Draft) {
				for (const auto& stop : TourHost->presentation().Draft->Stops) {
					const auto source = core::publication_source(stop);
					context.Destinations.push_back(source ? core::plan_tour_destination(core::publication_observation(*source, context.ObservedHead.value_or(TourHost->presentation().Draft->Head), context.ObservedBase.value_or(TourHost->presentation().Draft->Base), stop.Body, context.Compatibility, context.BodyEdits == core::tour_operation_support::supported)) : core::tour_destination_plan{ });
				}
			}
		}
		return context;
	}
	/// [[BPR-1914]]. A turn of the tour conversation: a question about the
	/// tour or a selected passage, or a change asked for in words. It runs
	/// read-only in the workspace, as drafting does, and is given the
	/// earlier turns.
	void github_application::implementation::request_tour_ask(const core::tour_thread& aThread, const std::optional<core::tour_passage>& aPassage, const QString& aQuestion, const bool aUpdate)
	{
		const auto* const snapshot = WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		const auto        draft    = TourHost ? TourHost->presentation().Draft : nullptr;
		const auto        question = aQuestion.trimmed().toStdString();
		if (Closing || snapshot == nullptr || !draft || question.empty()) {
			return;
		}
		adopt_tour_conversation();
		if (aThread.Kind == core::tour_thread_kind::stop && std::ranges::find(draft->Stops, aThread.Stop, &core::tour_stop::Id) == draft->Stops.end()) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("This stop was removed. Its history stays readable; choose an active tour part to send a question."));
			return;
		}
		// [[BPR-1930]] AC3, owner: "particularly during the initial agent
		// draft". Feedback on the whole tour while Draft the tour runs waits
		// for its proposal, then revises it.
		// RFC-BPR-0126 Q1: it is the whole-tour conversation's first queued
		// message; more feedback before the draft is ready joins it.
		if (aThread.Kind == core::tour_thread_kind::tour && !aUpdate && TourDraftPending && AgentQueue && !tour_queue_key().empty()) {
			const auto  key     = tour_queue_key();
			const auto* queue   = AgentQueue->queue(key);
			auto        refused = core::queue_refusal::none;
			if (queue != nullptr && !queue->Entries.empty() && queue->Entries.front().State == core::queued_state::queued && !queue->Entries.front().Held) {
				const auto id     = queue->Entries.front().Id;
				const auto joined = queue->Entries.front().Input.Text + "\n\n" + question;
				refused           = joined.size() > core::maximum_queued_text_bytes ? core::queue_refusal::too_long : AgentQueue->hold(key, id);
				if (refused == core::queue_refusal::none) {
					refused = AgentQueue->release_hold(key, id, joined);
				}
			} else {
				auto label = question.substr(0U, question.find('\n'));
				if (label.size() > 80U) {
					label = label.substr(0U, 79U) + "\u2026";
				}
				refused = AgentQueue->enqueue(key, core::queued_input{ .Scope = "tour", .Label = std::move(label), .Text = question });
			}
			if (refused == core::queue_refusal::none) {
				TourFeedbackQueued = true;
				present_tour_feedback();
				// [[BPR-2181]] (AC11). Only what will happen: it goes if the
				// draft proposes a tour, and comes back here if not.
				Window->tour_editor()->show_writing_notice(QStringLiteral("Your feedback waits for the draft. If the draft proposes a tour, the feedback goes next to revise it; if not, it comes back here unsent."));
				return;
			}
			Window->tour_editor()->show_writing_notice(QStringLiteral("This feedback is too long to wait for the draft. Nothing was added; send it once the draft is ready."));
			return;
		}
		// A turn that cannot run still lands in its side chat, saying why.
		const auto refuse = [this, &aThread, &aPassage, &question, aUpdate](const std::string& aWhy) {
			if (const auto run = core::begin_tour_ask(TourAgent, aThread, aPassage, question, aUpdate)) {
				if (auto found = std::ranges::find(TourAgent.Runs, *run, &core::tour_agent_run::Id); found != TourAgent.Runs.end()) {
					found->ConversationScope = tour_native_agent() + ":" + tour_native_scope(aThread);
					found->Conversation      = TourSelectedConversations[found->ConversationScope];
				}
				static_cast<void>(core::finish_tour_run(TourAgent, *run, core::tour_agent_run_state::failed, aWhy));
			}
			present_tour_session();
		};
		const auto agent = writing_agent();
		if (!agent) {
			refuse("No agent is chosen. Choose one with the agent picker in the window's footer.");
			return;
		}
		if (!tour_session_idle()) {
			refuse("The tour agent was still finishing its last run. Send this again in a moment.");
			return;
		}
		if (!reads_read_only(*agent)) {
			refuse(QStringLiteral("%1 has no read-only mode to read the code with. Choose Claude or Codex with the agent picker in the window's footer.").arg(agent_display_name(*agent)).toStdString());
			return;
		}
		auto files = std::vector<application::tour_draft_file>{ };
		for (const auto& file : snapshot->Files) {
			const auto change = file.Change == application::github_file_change::added ? "added"
				: file.Change == application::github_file_change::removed             ? "removed"
				: file.Change == application::github_file_change::renamed             ? "renamed"
																					  : "modified";
			files.push_back({ file.Path, change, file.HeadHunks });
		}
		// [[BPR-1916]]. The skills the question names, inline; an unknown
		// name stays text, said once in the notice. [[BPR-1935]]: the
		// directive is part of the prompt the bound below is checked on.
		// RFC-BPR-0125 (AC09): a suggested prompt sent as offered names
		// skills only in its template's own text.
		const auto texts  = std::array{ TourAskSkillText ? std::string_view{ *TourAskSkillText } : std::string_view{ question } };
		auto       skills = prepare_skill_run(SkillLibrary, texts);
		// [[BPR-2138]]. An Enhance is fresh: no conversation goes with it.
		const auto fresh  = TourAskFresh;
		const auto prompt = application::build_tour_ask_prompt(snapshot->PullRequest.Title, snapshot->PullRequest.Body, files, *draft, (tour_native_agent().empty() && !fresh ? core::tour_conversation_of(TourAgent, aThread) : decltype(core::tour_conversation_of(TourAgent, aThread)){ }), aThread, aPassage, question, aUpdate, TourProposal, TourAgent.Focus, skills.Directive, capture_tour_agent_context(aThread));
		if (!prompt) {
			refuse("This question, with the pull request and the conversation so far, is too large to send in one request.");
			return;
		}
		if (skills.Note) {
			Window->tour_editor()->show_writing_notice(QString::fromStdString(*skills.Note));
		}
		// [[BPR-2140]]. The turn's instructions are the system prompt; the
		// message is the pull request, the tour and the conversation.
		auto parts = application::split_tour_prompt(*prompt);
		// [[BPR-2138]], RFC-BPR-0101 AC12. A fresh answer (Draft Tour's first
		// draft, a part's Enhance) goes in this thread's next turn: the
		// session that resumes from here holds it. Ask more asked for that by
		// a press; core now decides it for every such turn, so the tour's own
		// chat toggle, History and feedback queued behind the draft all start
		// from the answer. Once this turn runs or is answered core carries it
		// no more; a turn that fails leaves it for the next.
		if (const auto carried = fresh ? std::nullopt : tour_fresh_answer(aThread)) {
			if (const auto* const made = core::find_tour_run(TourAgent, *carried); made != nullptr) {
				parts.Message = "Earlier, in a separate run, the agent answered this. It is context for this conversation:\n" + tour_fresh_seed(*made) + "\n\n" + parts.Message;
			}
		}
		TourRunInstructions = std::move(parts.Instructions);
		TourAskPending      = true;
		if (!dispatch_tour_run(*agent, core::tour_agent_request::ask, question, parts.Message, std::nullopt, aPassage, aThread, aUpdate, std::move(skills))) {
			TourAskPending = false;
		}
	}

	auto github_application::implementation::tour_fresh_answer(const core::tour_thread& aThread) const -> std::optional<core::tour_agent_run_id>
	{
		const auto agent        = tour_native_agent();
		const auto key          = agent + ":" + tour_native_scope(aThread);
		const auto selected     = TourSelectedConversations.find(key);
		auto       conversation = core::tour_agent_session{ };
		// A fresh run is no conversation's. A turn counts when the next
		// question goes on from it: every turn of the thread where the
		// conversation is replayed from this session, and the selected
		// conversation's where the agent keeps its own.
		std::ranges::copy_if(TourAgent.Runs, std::back_inserter(conversation.Runs), [&](const core::tour_agent_run& aRun) {
			return aRun.Request == core::tour_agent_request::draft_tour || aRun.Fresh || agent.empty()
				|| (aRun.ConversationScope == key && (selected == TourSelectedConversations.end() || aRun.Conversation == selected->second));
		});
		const auto        carried = core::fresh_answer_to_carry(conversation, aThread, TourProposal);
		const auto* const made    = carried ? core::find_tour_run(TourAgent, *carried) : nullptr;
		return made != nullptr && (made->Request != core::tour_agent_request::draft_tour || TourDraftSeedRun == std::optional{ made->Id }) ? carried : std::nullopt;
	}

	auto github_application::implementation::tour_fresh_seed(const core::tour_agent_run& aRun) const -> std::string
	{
		auto seed = std::string{ };
		if (aRun.Request == core::tour_agent_request::draft_tour) {
			seed = TourDraftSeed;
		} else {
			seed = "Enhance, asked: " + aRun.Question + "\n\nIt answered: " + aRun.Reply;
			if (TourProposal) {
				for (const auto& item : TourProposal->Items) {
					if (item.Run == aRun.Id && !item.Text.empty()) {
						seed += "\n\nIts suggestion:\n" + item.Text;
					}
				}
			}
		}
		if (seed.size() > 8192U) {
			seed.resize(8192U);
		}
		return seed;
	}

	void github_application::implementation::present_tour_carried_answer()
	{
		if (Window == nullptr) {
			return;
		}
		auto* const chat    = Window->tour_editor()->side_chat();
		auto        carried = qt::tour_carried_answer::none;
		// A local draft's turns replay or resume their conversation; none is fresh.
		if (const auto thread = chat->current(); thread && !local_tour_shown()) {
			const auto        fresh = tour_fresh_answer(*thread);
			const auto* const made  = fresh ? core::find_tour_run(TourAgent, *fresh) : nullptr;
			if (made != nullptr) {
				carried = made->Request == core::tour_agent_request::draft_tour ? qt::tour_carried_answer::draft : qt::tour_carried_answer::enhance;
			}
		}
		chat->present_carried_answer(carried);
	}

	/// [[BPR-1914]]. An answered turn: its reply in the conversation, and
	/// any change it proposes added to the proposal, anchored as a draft's.
	void github_application::implementation::finish_tour_ask(const core::tour_agent_run_id aRun, const application::agent_run_result& aResult)
	{
		const auto said   = aResult.AssistantItems.empty() ? std::string{ } : aResult.AssistantItems.back();
		auto       answer = application::decode_tour_ask_answer(said);
		if (!answer) {
			static_cast<void>(core::finish_tour_run(TourAgent, aRun, core::tour_agent_run_state::failed, "The agent's answer wasn't one this app can read. Nothing in the tour changed.\n\n" + said.substr(0U, std::min<std::size_t>(said.size(), 1500U))));
			return;
		}
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		// [[BPR-1995]], RFC-BPR-0100. Enhance on a selection replaces only the
		// selection: the core puts the passage's new text back in its place.
		if (const auto* const asked = core::find_tour_run(TourAgent, aRun); draft && asked != nullptr && asked->Update && asked->Passage && asked->Passage->Field != core::tour_passage_field::stop_code) {
			answer = core::confine_to_passage(std::move(*answer), TourSubmittedDraft.value_or(*draft), asked->Thread, *asked->Passage);
		}
		auto proposal = std::optional<core::tour_proposal>{ };
		if (draft) {
			auto files = tour_anchor_files(aResult);
			proposal   = core::propose_tour_changes(aRun, *answer, files, TourSubmittedDraft.value_or(*draft), *draft, TourWorkspaceHead);
			// [[BPR-1914]] v003. An Update proposes only its chat's part.
			if (const auto* const asked = core::find_tour_run(TourAgent, aRun); asked != nullptr && asked->Update) {
				proposal = core::keep_thread_part(std::move(*proposal), asked->Thread);
			}
			if (!proposal->Items.empty()) {
				TourAnchorFiles = std::move(files);
			}
		}
		static_cast<void>(core::answer_tour_ask(TourAgent, aRun, answer->Reply, proposal ? core::tally_of(*proposal) : core::tour_proposal_tally{ }));
		if (!proposal) {
			return;
		}
		static_cast<void>(core::note_finished_tour_run(TourAgent, aRun, core::left_out_entries(*proposal)));
		const auto* const asked = core::find_tour_run(TourAgent, aRun);
		const auto        whole = asked != nullptr && asked->Thread.Kind == core::tour_thread_kind::tour;
		// [[BPR-1930]] follow-up, owner: "I dont really understand the focus
		// changes button". A focus is the author's own instruction and changes
		// no text of the tour: it applies at once, with Undo on the turn.
		if (whole && !proposal->Focus.empty()) {
			if (core::apply_tour_focus(TourAgent, aRun, proposal->Focus) && proposal->Items.empty() && !proposal->RevisesWaitingStops) {
				Window->tour_editor()->show_writing_notice(QStringLiteral("Focus set. Later drafts and chat answers favour %1.").arg(QString::fromStdString(TourAgent.Focus)));
			}
		}
		// [[BPR-1937]]. The core decides what waits: a newer plan for the
		// tour's shape replaces the earlier one whole, even refused whole.
		const auto refused = proposal->Refused.size();
		const auto items   = !proposal->Items.empty();
		if (items || proposal->RevisesWaitingStops || proposal->RevisesStructure) {
			const auto merged = core::merge_tour_proposal(TourProposal, std::move(*proposal));
			core::remember_tour_proposal(TourAgent, TourProposal, merged);
			TourProposal = merged;
		}
		if (refused > 0U) {
			// [[BPR-1937]] AC2. A refused part is said, with why, on the tour
			// as in the chat.
			const auto refusal = refused == 1U ? QStringLiteral("1 part of the tour agent's answer couldn't be proposed. The chat says why.")
											   : QStringLiteral("%1 parts of the tour agent's answer couldn't be proposed. The chat says why.").arg(refused);
			Window->tour_editor()->show_writing_notice(items ? QStringLiteral("The tour agent's other changes wait in the tour, marked grey. ") + refusal : refusal);
		} else if (items) {
			// [[BPR-1930]]. The tour chat's proposal waits in the tour itself.
			Window->tour_editor()->show_writing_notice(whole ? QStringLiteral("The tour agent's changes wait in the tour, marked grey. Nothing changes until you apply them.")
															 : QStringLiteral("The tour agent proposed a change in the side chat. Nothing changes until you apply it."));
		}
	}

	/// RFC-BPR-0126 Q1 ([[BPR-2151]]). The whole-tour conversation's queue
	/// on this pull request.
	auto github_application::implementation::tour_queue_key() const -> std::string
	{
		// AC06: the same pull request's session, of the tour-feedback kind.
		const auto session = core::parse_session_key(ask_session_key());
		return session ? core::session_key(core::with_kind(*session, core::queued_session_kind::tour_feedback)) : std::string{ };
	}

	/// The tour conversation takes its next turn only after Draft the tour
	/// has really ended: the answer handled, its proposal made and the run
	/// retired (RFC-BPR-0126: real completion, no timer retry).
	auto github_application::implementation::tour_queue_agent() const -> core::queue_agent
	{
		if (Closing || Window == nullptr || TourDraftPending || TourAskPending || TourQueueSending) {
			return core::queue_agent::busy;
		}
		return tour_session_idle() && TourProposal ? core::queue_agent::idle : core::queue_agent::busy;
	}

	/// The feedback goes as the whole-tour conversation's next turn, which
	/// starts from the draft's answer as every first question after a draft
	/// does (`tour_fresh_answer`), with its proposal in the request, so it
	/// revises that draft (Q1).
	auto github_application::implementation::release_tour_feedback(const core::queued_entry& aEntry) -> application::queued_admission
	{
		if (Closing || Window == nullptr) {
			return { };
		}
		TourFeedbackQueued = false;
		TourAgent.Waiting.reset();
		TourQueueSending = true;
		request_tour_ask(core::tour_thread{ }, std::nullopt, QString::fromStdString(aEntry.Input.Text), false);
		if (!TourAskPending) {
			// It could not run: the tour chat holds it as a turn that says
			// why, with the feedback as written, so it was answered.
			TourQueueSending = false;
			return { .Kind = application::queued_admission_kind::answered };
		}
		// AC03: the turn's own retirement settles it, as this run.
		const auto run = ++TourQueueRun;
		// [[BPR-2368]]. The tour chat shows the feedback as its running turn
		// now: it leaves the pending list (`core::begin_turn`), and still
		// holds the session until that run retires.
		if (AgentRuntime) {
			AgentRunner.began(*AgentRuntime, tour_queue_key(), run);
		}
		return { .Kind = application::queued_admission_kind::started, .Run = run };
	}

	/// Releases the feedback once this window's draft has really ended. A
	/// queue no open window drafted (the app closed, or the window that
	/// drafted did) comes back to the tour chat as not sent.
	void github_application::implementation::drain_tour_queue()
	{
		const auto key = tour_queue_key();
		if (Closing || Window == nullptr || !AgentQueue || key.empty() || TourQueueSending) {
			return;
		}
		const auto* queue = AgentQueue->queue(key);
		if (queue == nullptr || queue->Entries.empty()) {
			return;
		}
		if (!TourFeedbackQueued) {
			if (!AgentQueue->sending(key) && AgentRunner.state(key) == core::queue_agent::not_set_up) {
				return_tour_feedback("The draft this feedback waited for ended before it was ready, so it wasn't sent. Send it again when you want.");
			}
			return;
		}
		AgentQueue->agent_changed(key);
	}

	/// [[BPR-2181]]. The tour session's own retirement event: a feedback
	/// turn released here that has answered is settled; otherwise the host
	/// is told the tour conversation may take its turn.
	void github_application::implementation::tour_runtime_retired()
	{
		// [[BPR-2154]]. A tour part's released message settles on its run's
		// real end; otherwise every part's queue may go on. Deferred out of
		// the host's own retirement, so the next run starts on a host that
		// has finished retiring this one.
		if (Window != nullptr) {
			QTimer::singleShot(0, Window.get(), [this, weak = std::weak_ptr<const int>{ WindowLifetime }] {
				if (weak.expired() || Closing) {
					return;
				}
				if (const auto sending = core::parse_session_key(ChatQueueSending); sending && sending->Kind == core::queued_session_kind::tour_part) {
					if (ChatQueueFinish) {
						chat_queue_retired(ChatQueueFinish);
					}
				} else {
					drain_chat_queues();
				}
			});
		}
		const auto key = tour_queue_key();
		if (Closing || !AgentQueue || !AgentRuntime || key.empty()) {
			return;
		}
		if (std::exchange(TourQueueAnswered, false) && std::exchange(TourQueueSending, false)) {
			AgentRunner.retired(*AgentRuntime, key, TourQueueRun, core::queued_finish::went);
			return;
		}
		drain_tour_queue();
	}

	/// [[BPR-2181]] (AC11). Feedback the store kept for a draft no open
	/// window runs (the app quit or crashed while it waited, or while it was
	/// being sent) never waits forever: it comes back to the tour chat as
	/// not sent, and the queue takes the next draft's.
	void github_application::implementation::recover_tour_feedback()
	{
		const auto key = tour_queue_key();
		if (Closing || Window == nullptr || !AgentQueue || key.empty() || TourFeedbackQueued || TourQueueSending || TourDraftPending || ReturningTourFeedback) {
			return;
		}
		const auto* const queue = AgentQueue->queue(key);
		if (queue == nullptr || queue->Entries.empty()) {
			return;
		}
		// Another window's draft it waits for keeps it.
		// One whose turn began is in flight too, though no longer listed.
		const auto inFlight = std::ranges::any_of(queue->Entries, [](const core::queued_entry& aEntry) { return aEntry.State == core::queued_state::sending || !core::listed(aEntry); });
		if (inFlight || AgentRunner.state(key) != core::queue_agent::not_set_up) {
			return;
		}
		return_tour_feedback("The app closed before this feedback was answered, so it wasn't sent. Send it again when you want.");
	}

	void github_application::implementation::return_tour_feedback(const std::string& aWhy)
	{
		const auto key     = tour_queue_key();
		TourFeedbackQueued = false;
		TourAgent.Waiting.reset();
		if (!AgentQueue || key.empty() || AgentQueue->queue(key) == nullptr) {
			return;
		}
		auto texts = std::string{ };
		// A copy: withdrawing changes the list. One whose delivery a crash
		// left unknown is discarded by the core's transition: its turn never
		// answered here, and nothing sends it again by itself.
		const auto entries    = AgentQueue->queue(key)->Entries;
		ReturningTourFeedback = true;
		for (const auto& entry : entries) {
			const auto taken = entry.State == core::queued_state::queued ? AgentQueue->withdraw(key, entry.Id)
				: entry.State == core::queued_state::delivery_unknown    ? AgentQueue->discard(key, entry.Id)
																		 : core::queue_refusal::not_editable;
			if (taken == core::queue_refusal::none) {
				texts += (texts.empty() ? "" : "\n\n") + entry.Input.Text;
			}
		}
		// A tour's feedback queue holds only what waits for a draft: emptied,
		// it takes the next draft's.
		static_cast<void>(AgentQueue->resume(key));
		ReturningTourFeedback = false;
		if (!texts.empty()) {
			if (const auto turn = core::begin_tour_ask(TourAgent, core::tour_thread{ }, std::nullopt, texts)) {
				static_cast<void>(core::finish_tour_run(TourAgent, *turn, core::tour_agent_run_state::failed, aWhy));
			}
		}
		present_tour_session();
	}

	/// The tour chat shows the queued feedback where it showed the buffer.
	void github_application::implementation::present_tour_feedback()
	{
		if (Closing || Window == nullptr || !AgentQueue) {
			return;
		}
		const auto* queue  = AgentQueue->queue(tour_queue_key());
		auto        joined = std::optional<std::string>{ };
		if (queue != nullptr && TourFeedbackQueued) {
			for (const auto& entry : queue->Entries) {
				if (entry.State == core::queued_state::queued) {
					joined = joined ? *joined + "\n\n" + entry.Input.Text : entry.Input.Text;
				}
			}
		}
		if (joined != TourAgent.Waiting) {
			TourAgent.Waiting = std::move(joined);
			present_tour_session();
		}
	}

	/// [[BPR-1930]] follow-up. Undo on a turn's "Focus set to" line.
	void github_application::implementation::undo_tour_focus(const core::tour_agent_run_id aRun)
	{
		if (core::undo_tour_focus(TourAgent, aRun)) {
			Window->tour_editor()->show_writing_notice(TourAgent.Focus.empty() ? QStringLiteral("Focus undone. Later drafts and chat answers cover the whole pull request.")
																			   : QStringLiteral("Focus undone. The earlier focus is back."));
			present_tour_session();
		}
	}

	void github_application::implementation::clear_tour_focus()
	{
		if (core::clear_tour_focus(TourAgent)) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("Focus cleared. Later drafts and chat turns cover the whole pull request."));
			present_tour_session();
		}
	}

	/// [[BPR-1931]], owner: "I want the ability to reset/clear the tour that
	/// I'm authoring". The core makes the empty draft; the tour host saves
	/// it as any edit (one Undo restores it too); the chats, the focus and
	/// any proposal go with it, kept here for Undo reset. A published tour is
	/// not touched: no GitHub request is made.
	void github_application::implementation::reset_tour()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Closing || !draft || draft->ReadOnly || core::running_tour_run(TourAgent) || TourDraftPending || TourAskPending || (TourSession && !TourSession->is_quiescent()) || !core::resettable(core::reset_tally_of(*draft, TourAgent, TourProposal))) {
			return;
		}
		auto snapshot = tour_reset_undo{ .Draft = *draft, .Session = TourAgent, .Proposal = TourProposal, .Anchors = TourAnchorFiles, .ResetRevision = draft->Revision, .ResetRun = TourAgent.LastRun };
		// [[BPR-1931]] follow-up, owner 2026-11-07: "Reset tour doesn't seem
		// to reset unaccepted draft tour suggestions." A tour that is empty
		// but for the agent's proposals is already the empty draft: the tour
		// host refuses a restore that changes nothing, so only the proposals,
		// chats and focus go.
		if (const auto empty = core::reset_tour(*draft); empty != *draft) {
			const auto revision = draft->Revision;
			TourHost->dispatch(core::tour_command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = empty });
			const auto after = TourHost->presentation().Draft;
			if (!after || after->Revision == revision) {
				Window->tour_editor()->show_writing_notice(QStringLiteral("The tour couldn't be reset. Nothing changed."));
				return;
			}
			snapshot.ResetRevision = after->Revision;
		}
		TourHistoryKey.clear();
		TourResetUndo = std::move(snapshot);
		TourAgent     = core::tour_agent_session{ .LastRun = TourAgent.LastRun };
		TourProposal.reset();
		TourAnchorFiles.clear();
		Window->tour_editor()->show_writing_notice({ });
		if (const auto thread = Window->tour_editor()->side_chat()->current()) {
			observe_tour_native_history(*thread);
		}
		present_tour_session();
		present_tour_reset();
	}

	void github_application::implementation::undo_tour_reset()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Closing || !draft || !TourResetUndo || draft->Revision != TourResetUndo->ResetRevision || TourAgent.LastRun != TourResetUndo->ResetRun || core::running_tour_run(TourAgent)) {
			TourResetUndo.reset();
			present_tour_reset();
			return;
		}
		// Taken first: the restore's own presentation lets a stale undo go.
		auto undo = std::move(*TourResetUndo);
		TourResetUndo.reset();
		// A reset that left the draft as it was (only proposals, chats or
		// focus went) restores no draft either.
		if (undo.Draft != *draft) {
			TourHost->dispatch(core::tour_command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = undo.Draft });
			const auto after = TourHost->presentation().Draft;
			if (!after || after->Revision == draft->Revision) {
				TourResetUndo = std::move(undo);
				Window->tour_editor()->show_writing_notice(QStringLiteral("The reset couldn't be undone. The tour is as the reset left it."));
				return;
			}
		}
		const auto last   = TourAgent.LastRun;
		TourAgent         = std::move(undo.Session);
		TourAgent.LastRun = std::max(TourAgent.LastRun, last);
		TourProposal      = std::move(undo.Proposal);
		TourAnchorFiles   = std::move(undo.Anchors);
		TourHistoryKey.clear();
		if (const auto thread = Window->tour_editor()->side_chat()->current()) {
			observe_tour_native_history(*thread);
		}
		Window->tour_editor()->show_writing_notice(QStringLiteral("Reset undone. The tour, its chats, its focus and its proposals are back."));
		present_tour_session();
		present_tour_reset();
	}

	/// [[BPR-1931]]. What Reset tour would clear, and whether its Undo is
	/// still offered, for the Tour tab.
	void github_application::implementation::present_tour_reset()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Window == nullptr) {
			return;
		}
		// Undo reset goes once the tour or its conversation changes: a later
		// run's turns are never overwritten by an Undo.
		if (TourResetUndo && (!draft || draft->Revision != TourResetUndo->ResetRevision || TourAgent.LastRun != TourResetUndo->ResetRun)) {
			TourResetUndo.reset();
		}
		const auto tally = draft ? core::reset_tally_of(*draft, TourAgent, TourProposal) : core::tour_reset_tally{ };
		Window->tour_editor()->present_tour_reset(tally, draft && !draft->ReadOnly, TourResetUndo.has_value());
	}

	/// [[BPR-1900]]. The host's facts the core anchors against: the pull
	/// request's files, their hunks, and the head lines the host counted
	/// before the agent ran.
	auto github_application::implementation::tour_anchor_files(const application::agent_run_result& aResult) const -> std::vector<core::tour_anchor_file>
	{
		auto        files = std::vector<core::tour_anchor_file>{ };
		const auto* snap  = WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		if (snap == nullptr) {
			return files;
		}
		for (const auto& file : snap->Files) {
			auto anchor = core::tour_anchor_file{ .Path = file.Path, .OnHead = file.Change != application::github_file_change::removed };
			if (file.HeadHunks) {
				anchor.Hunks.emplace();
				for (const auto& [first, last] : *file.HeadHunks) {
					anchor.Hunks->push_back({ first, last });
				}
			}
			if (const auto counted = std::ranges::find(aResult.MeasuredLines, file.Path, &std::pair<std::string, std::uint32_t>::first); counted != aResult.MeasuredLines.end()) {
				anchor.HeadLines = counted->second;
			}
			files.push_back(std::move(anchor));
		}
		return files;
	}

	/// [[BPR-1900]]. What a run answered, as its transcript says it: a
	/// whole-tour draft by what it proposes, anything else as written.
	auto github_application::implementation::tour_answer_words(const application::agent_run_result& aResult) const -> std::string
	{
		auto said = aResult.AssistantItems.empty() ? std::string{ } : aResult.AssistantItems.back();
		if (!TourDraftPending) {
			return said;
		}
		const auto answer = application::decode_tour_draft_answer(said);
		if (!answer) {
			return "An answer that isn't a tour this app can read.";
		}
		auto parts = std::vector<std::string>{ };
		parts.push_back(std::to_string(answer->Stops.size()) + (answer->Stops.size() == 1U ? " stop" : " stops"));
		auto words = std::string{ "Proposed " };
		for (std::size_t index = 0U; index < parts.size(); ++index) {
			words += (index == 0U ? "" : index + 1U == parts.size() ? " and "
																	: ", ")
				+ parts[index];
		}
		return words + ".";
	}

	auto github_application::implementation::tour_request_files() const -> std::vector<application::tour_draft_file>
	{
		auto        files    = std::vector<application::tour_draft_file>{ };
		const auto* snapshot = WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		if (snapshot == nullptr) {
			return files;
		}
		for (const auto& file : snapshot->Files) {
			const auto change = file.Change == application::github_file_change::added ? "added"
				: file.Change == application::github_file_change::removed             ? "removed"
				: file.Change == application::github_file_change::renamed             ? "renamed"
																					  : "modified";
			files.push_back({ .Path = file.Path, .Change = change, .Hunks = file.HeadHunks, .Patch = file.Patch });
		}
		return files;
	}

	auto github_application::implementation::tour_context_notice(const core::tour_context_route aRoute, const std::size_t aDiffBytes) const -> QString
	{
		switch (aRoute) {
		case core::tour_context_route::up_front: return QStringLiteral("Drafting in the background from the pull request's diff, in one request. Keep writing.");
		case core::tour_context_route::reads_repository_diff_too_large:
			return QStringLiteral("This pull request's diff (%1 KiB) is more than one request carries (%2 KiB), so the agent reads the repository instead, in several requests. Keep writing.")
				.arg(aDiffBytes / 1024U)
				.arg(core::maximum_up_front_diff_bytes / 1024U);
		case core::tour_context_route::reads_repository_no_tool_free_mode:
			return QStringLiteral("This agent has no tool-free mode, so it reads the repository, in several requests. Keep writing.");
		}
		return tour_reading_notice();
	}

	/// [[BPR-1900]], owner addition. Draft the tour: the whole tour,
	/// drafted in the background from the pull request, as a proposal.
	void github_application::implementation::request_tour_draft()
	{
		const auto* const snapshot = WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		const auto        draft    = TourHost ? TourHost->presentation().Draft : nullptr;
		Window->tour_editor()->clear_tour_draft_failure();
		if (Closing || snapshot == nullptr || !draft || draft->ReadOnly) {
			return;
		}
		const auto agent = writing_agent();
		if (!agent || !tour_session_idle()) {
			Window->tour_editor()->show_writing_notice(!agent ? QStringLiteral("Choose an agent with the agent picker in the window's footer, or keep writing yourself.")
															  : QStringLiteral("The tour session is drafting. Wait for it to finish or stop it, or keep writing yourself."));
			return;
		}
		// [[BPR-2141]], RFC-BPR-0101 AC13. One tool-free call with the diff
		// up front; a diff past the budget, or an agent with no tool-free
		// mode, reads the repository instead, and the notice says so.
		const auto files = tour_request_files();
		const auto bytes = application::tour_diff_bytes(files);
		auto       route = core::choose_tour_context(bytes, writes_tool_free(*agent));
		if (route != core::tour_context_route::up_front && !reads_read_only(*agent)) {
			Window->tour_editor()->show_tour_draft_failure(route == core::tour_context_route::reads_repository_diff_too_large
															   ? QStringLiteral("This pull request's diff (%1 KiB) is more than one request carries (%2 KiB), and %3 cannot read the repository instead. Choose Claude or Codex with the agent picker in the window's footer.")
																	 .arg(bytes / 1024U)
																	 .arg(core::maximum_up_front_diff_bytes / 1024U)
																	 .arg(agent_display_name(*agent))
															   : QStringLiteral("%1 has no drafting mode. Choose Claude or Codex with the agent picker in the window's footer.").arg(agent_display_name(*agent)));
			return;
		}
		adopt_tour_conversation();
		auto prompt = application::build_tour_draft_prompt(snapshot->PullRequest.Title, snapshot->PullRequest.Body, files, *draft, TourAgent.Focus, { }, capture_tour_agent_context(), route == core::tour_context_route::up_front);
		if (!prompt && route == core::tour_context_route::up_front && reads_read_only(*agent)) {
			route  = core::tour_context_route::reads_repository_diff_too_large;
			prompt = application::build_tour_draft_prompt(snapshot->PullRequest.Title, snapshot->PullRequest.Body, files, *draft, TourAgent.Focus, { }, capture_tour_agent_context());
		}
		if (!prompt) {
			Window->tour_editor()->show_tour_draft_failure(QStringLiteral("This pull request is too large to describe to the agent in one request."));
			return;
		}
		core::remember_tour_proposal(TourAgent, TourProposal, std::nullopt);
		TourProposal.reset();
		TourDraftPending = true;
		Window->tour_editor()->show_writing_notice(tour_context_notice(route, bytes));
		// [[BPR-2140]]. Its instructions are the system prompt; the message
		// is the pull request and the tour so far.
		auto parts          = application::split_tour_prompt(*prompt);
		TourRunInstructions = std::move(parts.Instructions);
		TourRunUpFront      = route == core::tour_context_route::up_front;
		if (!dispatch_tour_run(*agent, core::tour_agent_request::draft_tour, "Draft Tour", parts.Message, std::nullopt)) {
			TourDraftPending = false;
			Window->tour_editor()->show_tour_draft_failure(QStringLiteral("The tour session could not start this request."));
		}
	}

	void github_application::implementation::finish_tour_draft(const std::optional<core::tour_agent_run_id> aRun, const application::agent_run_result& aResult, const QString& aWords)
	{
		if (aResult.Outcome == application::agent_run_outcome::cancelled) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("Drafting stopped. Nothing in the tour changed."));
			return;
		}
		if (aResult.Outcome != application::agent_run_outcome::success) {
			Window->tour_editor()->show_writing_notice({ });
			Window->tour_editor()->show_tour_draft_failure(aWords);
			return;
		}
		const auto  answer = aResult.AssistantItems.empty() ? std::nullopt : application::decode_tour_draft_answer(aResult.AssistantItems.back());
		const auto  draft  = TourHost ? TourHost->presentation().Draft : nullptr;
		const auto* snap   = WindowRead->accepted_snapshot() ? &*WindowRead->accepted_snapshot() : nullptr;
		if (!answer || !draft || snap == nullptr || !aRun) {
			Window->tour_editor()->show_writing_notice({ });
			Window->tour_editor()->show_tour_draft_failure(QStringLiteral("The agent's answer wasn't a tour this app can read. Nothing in the tour changed."));
			return;
		}
		// [[BPR-1900]], owner 2026-11-06: stops strongly prefer the change;
		// where it is comes from each file's patch.
		auto       files    = tour_anchor_files(aResult);
		const auto proposed = core::propose_tour(*aRun, *answer, files, *draft, TourWorkspaceHead);
		core::remember_tour_proposal(TourAgent, TourProposal, proposed);
		// [[BPR-2138]]. What the fresh first draft said, for the whole
		// tour's next question.
		TourDraftSeedRun = *aRun;
		TourDraftSeed    = "Draft Tour";
		TourDraftSeed += "\n\nProposed stops: " + std::to_string(answer->Stops.size()) + ".";
		if (TourDraftSeed.size() > 8192U) {
			TourDraftSeed.resize(8192U);
		}
		TourProposal    = proposed;
		TourAnchorFiles = std::move(files);
		// [[BPR-1900]] owner look v002. Stops left out of the proposal are
		// kept in the run's transcript, so nothing the agent said is lost.
		static_cast<void>(core::note_finished_tour_run(TourAgent, *aRun, core::left_out_entries(*TourProposal)));
		Window->tour_editor()->show_writing_notice(TourProposal->Items.empty() ? QStringLiteral("The agent proposed nothing for this tour.")
																			   : QStringLiteral("A proposed tour is ready to review. Nothing changes until you apply it."));
	}

	/// [[BPR-1900]]. The author accepts (with their edit) or discards one
	/// proposed field or stop; the core decides, the tour host applies.
	void github_application::implementation::decide_tour_proposal(const std::size_t aIndex, const core::tour_proposal_decision aDecision, const QString& aText)
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!TourProposal || !draft || draft->ReadOnly || aIndex >= TourProposal->Items.size()) {
			return;
		}
		auto decided = *TourProposal;
		if (!core::decide_tour_proposal(decided, aIndex, aDecision, aText.toStdString())) {
			return;
		}
		const auto& item = decided.Items[aIndex];
		if (aDecision == core::tour_proposal_decision::accepted && core::structural(item.Part)) {
			// [[BPR-1930]] AC1. The core makes the revised tour; the host
			// restores it as one edit, so one Undo takes it back.
			const auto revised = core::apply_tour_revision(*draft, item);
			if (!revised) {
				Window->tour_editor()->show_writing_notice(QStringLiteral("The tour changed since this was proposed, so it can't be applied. Nothing changed."));
				return;
			}
			const auto revision = draft->Revision;
			TourHost->dispatch(core::tour_command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = revised });
			const auto after = TourHost->presentation().Draft;
			if (!after || after->Revision == revision) {
				Window->tour_editor()->show_writing_notice(QStringLiteral("This change couldn't be applied. Nothing changed."));
				return;
			}
			// A pending part about a stop this took away goes with it.
			static_cast<void>(core::prune_tour_proposal(decided, *after));
		} else if (aDecision == core::tour_proposal_decision::accepted) {
			auto command = core::tour_command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision };
			if (item.Part == core::tour_proposal_part::stop_text) {
				// [[BPR-1914]]. A new explanation for a stop still in the tour.
				const auto stop = std::ranges::find(draft->Stops, item.StopId, &core::tour_stop::Id);
				if (stop == draft->Stops.end()) {
					Window->tour_editor()->show_writing_notice(QStringLiteral("That stop is no longer in the tour, so its new explanation can't be added."));
					return;
				}
				auto value    = *stop;
				value.Body    = item.Text;
				command.Kind  = core::tour_command_kind::edit_stop;
				command.Stop  = value.Id;
				command.Value = std::move(value);
			} else if (item.Part == core::tour_proposal_part::stop) {
				// After the stops already accepted from this proposal, in its order.
				auto position    = draft->Stops.size();
				command.Kind     = core::tour_command_kind::insert;
				command.Value    = core::tour_stop_of(item.Stop, core::proposal_head(item, *draft));
				command.Position = position;
			} else {
				return;
			}
			TourHost->dispatch(command);
			if (!TourHost->presentation().Draft || TourHost->presentation().Refusal != core::tour_refusal::none) {
				Window->tour_editor()->show_writing_notice(QStringLiteral("This proposal could not be applied. Nothing changed."));
				return;
			}
		}
		core::remember_tour_proposal(TourAgent, TourProposal, decided);
		TourProposal = std::move(decided);
		if (std::ranges::none_of(TourProposal->Items, [](const core::tour_proposal_item& aItem) { return aItem.Decision == core::tour_proposal_decision::pending; })) {
			TourProposal.reset();
			Window->tour_editor()->show_writing_notice(QStringLiteral("Proposal applied. Stop readiness is shown in the tour list."));
		}
		present_tour_session();
	}

	/// [[BPR-1900]]. Move to nearest change: the core moves the stop onto
	/// the closest hunk of its file.
	void github_application::implementation::relocate_tour_proposal(const std::size_t aIndex)
	{
		if (TourProposal && core::relocate_tour_stop(*TourProposal, aIndex, TourAnchorFiles)) {
			present_tour_session();
		}
	}

	auto github_application::implementation::tour_observed_binding() const -> std::optional<core::agent_revision_binding>
	{
		if (!EpochHost) {
			return std::nullopt;
		}
		const auto& state = EpochHost->state();
		if (!state.Authority || !state.AcceptedRevision || !state.AcceptedComparison) {
			return std::nullopt;
		}
		return core::agent_revision_binding{ .Host = state.Authority->Host, .Repository = state.Authority->Repository, .PullRequest = state.Authority->PullRequest, .BaseOid = state.AcceptedRevision->Base, .HeadOid = state.AcceptedRevision->Head, .LeftEpoch = state.AcceptedComparison->Left.Identity.Epoch, .RightEpoch = state.AcceptedComparison->Right.Identity.Epoch };
	}

	auto github_application::implementation::continued_tour_session(const core::tour_thread& aThread) const -> std::optional<core::agent_session>
	{
		const auto key      = tour_native_agent() + ":" + tour_native_scope(aThread);
		const auto selected = TourSelectedConversations.find(key);
		const auto session  = TourNativeSessions.find(key);
		if (TourContinuation && TourContinuation->Scope == key && selected != TourSelectedConversations.end()
			&& TourContinuation->Conversation == selected->second && session != TourNativeSessions.end() && TourContinuation->Session == session->second
			&& tour_observed_binding() == std::optional{ TourContinuation->Observed }) {
			return session->second;
		}
		return std::nullopt;
	}

	void github_application::implementation::choose_tour_continuation(const core::tour_thread& aThread)
	{
		const auto key      = tour_native_agent() + ":" + tour_native_scope(aThread);
		const auto selected = TourSelectedConversations.find(key);
		const auto session  = TourNativeSessions.find(key);
		const auto observed = tour_observed_binding();
		if (!Closing && tour_session_idle() && selected != TourSelectedConversations.end() && !selected->second.empty()
			&& session != TourNativeSessions.end() && observed) {
			const auto offer = core::offer_agent_continuations(session->second, *observed);
			if (offer.Required && offer.Status.Staleness != core::agent_session_staleness::different_review) {
				TourContinuation = tour_continuation{ key, selected->second, session->second, *observed };
			}
		}
		present_tour_continuation();
	}

	void github_application::implementation::present_tour_continuation()
	{
		if (!Window) {
			return;
		}
		auto*      chat     = Window->tour_editor()->side_chat();
		const auto thread   = chat->current();
		const auto observed = tour_observed_binding();
		if (TourContinuation && observed != std::optional{ TourContinuation->Observed }) {
			TourContinuation.reset();
		}
		const auto key     = thread ? tour_native_agent() + ":" + tour_native_scope(*thread) : std::string{ };
		const auto session = TourNativeSessions.find(key);
		const auto offer   = session != TourNativeSessions.end() && observed ? std::optional{ core::offer_agent_continuations(session->second, *observed) } : std::nullopt;
		chat->present_revision_continuation(offer, observed ? observed->HeadOid : core::git_commit_oid{ }, thread && continued_tour_session(*thread).has_value());
		present_tour_carried_answer();
	}

	void github_application::implementation::present_tour_session()
	{
		if (Window != nullptr) {
			auto       shown = TourAgent;
			const auto agent = tour_native_agent();
			if (!agent.empty()) {
				std::erase_if(shown.Runs, [&](const auto& aRun) {
					if (aRun.ConversationScope.empty()) {
						return aRun.Request == core::tour_agent_request::ask || aRun.Request == core::tour_agent_request::draft_tour;
					}
					const auto key      = agent + ":" + tour_native_scope(aRun.Thread);
					const auto selected = TourSelectedConversations.find(key);
					return aRun.ConversationScope != key || (selected != TourSelectedConversations.end() && aRun.Conversation != selected->second);
				});
			}
			if (!local_tour_shown()) {
				Window->tour_editor()->present_tour_session(shown, TourProposal);
			}
			refresh_tour_host_idle();
			present_tour_continuation();
			present_tour_reset();
			// [[BPR-2154]]. Each open part's pending list.
			present_chat_queues();
		}
	}

	void github_application::implementation::cancel_tour_run()
	{
		if (PendingWriting) {
			cancel_writing();
			return;
		}
		if (TourSession && TourRun) {
			TourSession->cancel();
		}
	}
#endif

	auto github_application::implementation::apply_writing_result(const writing_operation& aOperation, const application::writing_result& aResult) -> std::string
	{
		if (aResult.Insufficient) {
			return "More context needed: " + aResult.Reason;
		}
		const auto hook = aOperation.Request.Target.Hook;
		if (aOperation.Test) {
			if (hook == core::writing_hook::outline) {
				return "Hook test completed: valid outline skeleton. Nothing inserted.";
			}
			return "Hook test completed. Private result:\n" + aResult.Replacement;
		}
		if (!Window->tour_editor()->writing_visible(hook)) {
			return "The captured editor is no longer active. Your text is unchanged.";
		}
		auto*      editor  = Window->tour_editor()->writing_editor(hook);
		const auto capture = editor->capture();
		const auto target  = capture ? writing_target(hook, *capture) : std::nullopt;
		if (!target) {
			return "The captured owner or editor changed. Your text is unchanged.";
		}
		auto edit = WritingState.complete(aOperation.Request.Id, *target, editor->raw(), aResult.Replacement);
		if (!edit || !editor->apply(*edit)) {
			return "Result made no change or the captured text is no longer current.";
		}
		std::string message = "Updated captured text. Undo restores it. Nothing published.";
		for (const auto& note : aResult.Notes) {
			auto label = std::string{ "Unclear" };
			if (note.Kind == "recovered") {
				label = "Recovered";
			} else if (note.Kind == "inferred") {
				label = "Inferred";
			}
			message += "\n" + label + ": " + note.Text;
			for (const auto& ref : note.SourceRefs) {
				message += " [" + ref + "]";
			}
		}
		return message;
	}

	void github_application::implementation::finish_writing(const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult)
	{
		if (!PendingWriting || PendingWriting->Invocation != aIdentity) {
			return;
		}
		auto operation = std::move(*PendingWriting);
		PendingWriting.reset();
		std::string message = "Assistance did not complete; your text is unchanged.";
		std::string json;
		bool        bounded = true;
		for (const auto& event : aResult.Events) {
			if (json.size() + event.Text.size() > 32768) {
				bounded = false;
				break;
			}
			json += event.Text;
		}
		// [[BPR-1900]]. A run that read the repository spoke between its
		// tool uses; its answer is its last message alone.
		if (operation.ReadOnly) {
			json    = aResult.AssistantItems.empty() ? std::string{ } : aResult.AssistantItems.back();
			bounded = json.size() <= 32768;
		}
		const auto  result = aResult.Outcome == application::agent_run_outcome::success && bounded ? application::decode_writing_result(json, operation.Request.Target.Hook, operation.Sources, operation.Request.Scope == core::writing_scope::empty_field) : std::nullopt;
		const auto& prefs  = Profile->WorkbenchPreferences->accepted();
		// [[BPR-2051]]. Enhance on a comment or a note: a suggestion for the
		// field that asked, whatever the prompt says now (it ran with the one
		// saved when pressed), while the pull request is the one it read.
		if (operation.Enhance) {
			WritingState.cancel();
			const auto id = *operation.Enhance;
			if (result && enhancement_context_key() == operation.Request.Target.ContextDigest) {
				if (result->Insufficient) {
					answer_enhancement(id, std::nullopt, QStringLiteral("The agent needs more to go on: %1 Your text is unchanged.").arg(QString::fromStdString(result->Reason)));
				} else if (result->Replacement == std::string_view{ operation.Request.Raw }.substr(operation.Request.Selection.Begin, operation.Request.Selection.End - operation.Request.Selection.Begin)) {
					answer_enhancement(id, std::nullopt, QStringLiteral("The agent suggested no change. Your text is unchanged."));
				} else {
					answer_enhancement(id, result->Replacement, { });
				}
			} else if (aResult.Outcome == application::agent_run_outcome::cancelled) {
				answer_enhancement(id, std::nullopt, QStringLiteral("Stopped. Your text is unchanged."));
			} else if (aResult.Outcome == application::agent_run_outcome::success) {
				answer_enhancement(id, std::nullopt,
								   result ? QStringLiteral("The pull request changed while Enhance ran. Your text is unchanged; Enhance again.")
										  : QStringLiteral("The agent's answer could not be used. Your text is unchanged."));
			} else if (aResult.ProtocolFailure == application::agent_protocol_failure::not_signed_in) {
				// [[BPR-2259]]. The agent that ran, never a CLI it didn't run.
				answer_enhancement(id, std::nullopt, sign_in_failure_words(operation.Request.Target.Agent, QStringLiteral("Enhance again")));
			} else {
#if defined(BPR_AGENT_INTEGRATIONS)
				answer_enhancement(id, std::nullopt, qt::agent_tab_view::run_failure_words(aResult.Outcome, agent_display_name(operation.Request.Target.Agent)) + QStringLiteral(" Your text is unchanged."));
#else
				answer_enhancement(id, std::nullopt, QStringLiteral("Enhance did not finish. Your text is unchanged."));
#endif
			}
			return;
		}
		if (result && prefs && prefs->WritingHooks.Revision == operation.Request.Target.HookRevision && writing_context_key() == operation.Request.Target.ContextDigest) {
			message = apply_writing_result(operation, *result);
		} else if (aResult.Outcome == application::agent_run_outcome::success) {
			message = "Assistance returned an invalid, unsupported or stale result. Your text is unchanged.";
		} else if (aResult.ProtocolFailure == application::agent_protocol_failure::not_signed_in) {
			// [[BPR-1375]], [[BPR-2259]]. Writing runs the chosen agent, on its
			// own sign-in, and says so by that agent's name.
			message = sign_in_failure_words(operation.Request.Target.Agent, QStringLiteral("try again")).toStdString();
		}
		WritingState.cancel();
		if (operation.Test) {
			Window->writing_hooks()->show_notice(message);
		} else {
			Window->tour_editor()->show_writing_notice(QString::fromStdString(message));
			// [[BPR-1892]]. A run that failed says so beside its control,
			// in the Agent tab's own words, and offers the way to that
			// tab's card for it. A success, or a stop the person asked
			// for, leaves nothing there.
			const auto hook = operation.Request.Target.Hook;
			if (aResult.Outcome == application::agent_run_outcome::success || aResult.Outcome == application::agent_run_outcome::cancelled) {
				Window->tour_editor()->clear_agent_failure(hook);
			} else {
#if defined(BPR_AGENT_INTEGRATIONS)
				Window->tour_editor()->show_agent_failure(hook, qt::agent_tab_view::run_failure_words(aResult.Outcome, agent_display_name(operation.Request.Target.Agent)));
#else
				Window->tour_editor()->show_agent_failure(hook, { });
#endif
			}
		}
	}

	auto github_application::implementation::enhancement_context_key() const -> std::array<std::uint8_t, 32>
	{
		// [[BPR-2051]]. The pull request at the head Enhance read; the tour,
		// the comparison and the selected lines may move while it runs.
		auto key = std::string{ "enhance" };
		if (const auto& snapshot = WindowRead->accepted_snapshot(); snapshot) {
			key += ":" + std::to_string(snapshot->Snapshot.value()) + ":" + std::string{ snapshot->PullRequest.HeadOid.value() };
		}
		return core::writing_digest(key);
	}

	void github_application::implementation::answer_enhancement(const std::uint64_t aRequest, std::optional<std::string> aReplacement, const QString& aNotice)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		static_cast<void>(Window->present_enhancement(qt::enhance_outcome{ .Id = aRequest, .Replacement = std::move(aReplacement), .Notice = aNotice }));
	}

	void github_application::implementation::cancel_enhancement(const std::uint64_t aRequest)
	{
		if (!PendingWriting || PendingWriting->Enhance != std::optional{ aRequest }) {
			return;
		}
		PendingWriting.reset();
		WritingState.cancel();
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-2051]]. Enhance runs on the tour session's lane (every writing
		// run does), so Stop stops that lane, never the Agent tab's run.
		if (TourSession != nullptr) {
			TourSession->cancel();
		}
#endif
	}

	void github_application::implementation::request_enhancement(qt::enhance_request aRequest)
	{
		const auto id     = aRequest.Id;
		const auto refuse = [this, id](const QString& aWhy) { answer_enhancement(id, std::nullopt, aWhy); };
		// One Enhance writing prompt for every comment, reply and note; the
		// `writing-kind` source tells the agent which this is.
		const auto  hook  = core::writing_hook::enhance_writing;
		const auto& prefs = Profile->WorkbenchPreferences->accepted();
		const auto  agent = writing_agent();
		if (Closing || !prefs || !WindowRead->accepted_snapshot()) {
			refuse(QStringLiteral("Open a pull request to use Enhance. Your text is unchanged."));
			return;
		}
		if (Window->agent_panel()->offered_count() == 0 || !agent) {
			refuse(QStringLiteral("No agent is set up. Add one in Settings, Agents, or choose one with the agent picker in the window's footer. Your text is unchanged."));
			return;
		}
		if (!writes_tool_free(*agent)) {
			refuse(QStringLiteral("Enhance needs an agent that speaks Claude Code's stream JSON. Choose one with the agent picker in the window's footer. Your text is unchanged."));
			return;
		}
		if (PendingWriting || !tour_session_idle()) {
			refuse(QStringLiteral("The agent is already writing. Wait for it to finish, then Enhance again. Your text is unchanged."));
			return;
		}
		if (aRequest.Begin > aRequest.End || aRequest.End > aRequest.Raw.size() || !core::valid_writing_utf8(aRequest.Raw) || !core::writing_boundary(aRequest.Raw, aRequest.Begin)
			|| !core::writing_boundary(aRequest.Raw, aRequest.End) || core::writing_blank(std::string_view{ aRequest.Raw }.substr(aRequest.Begin, aRequest.End - aRequest.Begin))) {
			refuse(QStringLiteral("Write or select some text first."));
			return;
		}
		auto request                 = core::writing_request{ };
		request.Id                   = id;
		request.Target.Hook          = hook;
		request.Target.Window        = DraftWindow;
		request.Target.Agent         = *agent;
		request.Target.HookRevision  = prefs->WritingHooks.Revision;
		request.Target.ContextDigest = enhancement_context_key();
		request.Scope                = aRequest.Selection ? core::writing_scope::selection : core::writing_scope::field;
		request.Selection.Begin      = aRequest.Begin;
		request.Selection.End        = aRequest.End;
		request.Raw                  = std::move(aRequest.Raw);
		request.RawDigest            = core::writing_digest(request.Raw);
		auto        sources          = std::vector<application::writing_source>{ };
		std::size_t total            = 0;
		const auto  add              = [&](std::string aId, std::string_view aText, const std::size_t aLimit) {
			auto size = std::min({ aText.size(), aLimit, std::size_t{ 8192 } - total });
			while (size > 0 && !core::writing_boundary(aText, size)) {
				--size;
			}
			if (size == 0 || sources.size() == 8 || !core::valid_writing_utf8(aText.substr(0, size))) {
				return;
			}
			sources.push_back({ std::move(aId), std::string{ aText.substr(0, size) } });
			total += size;
		};
		// What the field is about: the pull request's description and, for a
		// comment, the lines it is on.
		add("writing-kind", aRequest.Surface == qt::enhance_surface::note ? std::string_view{ "A local comment the reviewer keeps for themselves; it stays on this computer and nobody else reads it." } : std::string_view{ "A draft comment or reply on the pull request, which its author will read." },
			256);
		add("pr-description", WindowRead->accepted_snapshot()->PullRequest.Body, 2048);
		if (aRequest.Surface == qt::enhance_surface::comment) {
			if (const auto source = selected_writing_source(true)) {
				add("comment-lines", source->Text, 3072);
			}
		}
		if (request.Scope == core::writing_scope::selection) {
			add("field-before", std::string_view{ request.Raw }.substr(0, request.Selection.Begin), 512);
			add("field-after", std::string_view{ request.Raw }.substr(request.Selection.End), 512);
		}
		// The prompt saved now is the one this run uses.
		const auto prompt = application::build_writing_prompt_parts(prefs->WritingHooks, request, sources);
		if (!prompt) {
			refuse(QStringLiteral("This text is too long for Enhance. Select a shorter passage, or shorten the Enhance prompt in Settings. Your text is unchanged."));
			return;
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		auto skills     = skill_run_for(hook);
		PendingWriting  = writing_operation{ .Request = request, .Sources = std::move(sources), .Invocation = { }, .Test = false, .ReadOnly = false, .Enhance = id };
		const auto text = prompt->Message + skills.Directive;
		// [[BPR-2138]], [[BPR-2140]]. A fresh run; the Enhance prompt in the
		// system prompt.
		if (!dispatch_agent_invocation(*agent, { }, { }, false, text, { }, core::agent_entry_point::ask_about_selection, { }, true,
									   tour_launch{ .Skills = std::move(skills), .Path = core::agent_run_path::enhance_writing, .SystemInstructions = prompt->Instructions })) {
			PendingWriting.reset();
			refuse(QStringLiteral("The agent could not start. Check it in the Agent tab, then Enhance again. Your text is unchanged."));
		}
#else
		refuse(QStringLiteral("This build has no agent. Your text is unchanged."));
#endif
	}

	void github_application::implementation::test_writing_hook(core::writing_hook aHook)
	{
		if (aHook >= core::writing_hook::feedback) {
			// [[BPR-1090]] The sample is a tour field; the feedback guidance is tried in the comment workshop.
			Window->writing_hooks()->show_notice("Test uses a tour field. Ask the comment workshop to try the review-feedback guidance.");
			return;
		}
		cancel_writing();
		const auto& prefs    = Profile->WorkbenchPreferences->accepted();
		const auto  selected = writing_agent();
		if (!prefs || !selected || !writes_tool_free(*selected) || !tour_session_idle()) {
			Window->writing_hooks()->show_notice("Add an agent that speaks Claude Code's stream JSON in Settings › Agents for tool-free writing tests; wait for any running request.");
			return;
		}
		core::writing_request request;
		// [[BPR-2051]] A stop's Enhance prompt is tried as a stop's Enhance:
		// the stop with text, whose instructions it is.
		request.Target.Hook                              = aHook == core::writing_hook::enhance_stop ? core::writing_hook::comment : aHook;
		request.Target.HookRevision                      = prefs->WritingHooks.Revision;
		request.Target.ContextDigest                     = writing_context_key();
		request.Raw                                      = "The cache reduces repeated work.";
		request.Selection.End                            = request.Raw.size();
		std::vector<application::writing_source> sources = { { "test-source", "The cache reuses previously computed results. No performance or test-success claim is supplied." } };
		const auto                               prompt  = application::build_writing_prompt_parts(prefs->WritingHooks, request, sources);
		if (!prompt) {
			Window->writing_hooks()->show_notice("Hook instructions exceed the test context limit.");
			return;
		}
		PendingWriting = writing_operation{ request, std::move(sources), { }, true, false, std::nullopt };
		auto skills    = skill_run_for(aHook);
		Window->writing_hooks()->show_notice(skills.Note ? "Testing saved prompt… Nothing will be inserted or published. " + *skills.Note : std::string{ "Testing saved prompt… Nothing will be inserted or published." });
#if defined(BPR_AGENT_INTEGRATIONS)
		const auto text = prompt->Message + skills.Directive;
		if (!dispatch_agent_invocation(*selected, { }, { }, false, text, { }, core::agent_entry_point::ask_about_selection, { }, true,
									   tour_launch{ .Skills = std::move(skills), .Path = core::agent_run_path::tour_enhance, .SystemInstructions = prompt->Instructions })) {
			PendingWriting.reset();
			Window->writing_hooks()->show_notice("An available agent and exact approved review context are required.");
		}
#else
		PendingWriting.reset();
#endif
	}

	void github_application::implementation::prepare_tour_repairs()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!draft || !TourPublication) {
			return;
		}
		const auto anchorFiles = [this] {
			auto result = std::vector<core::tour_anchor_file>{ };
			for (const auto& file : WindowRead->accepted_snapshot()->Files) {
				auto anchor = core::tour_anchor_file{ .Path = file.Path, .OnHead = file.Change != application::github_file_change::removed };
				if (file.HeadHunks) {
					anchor.Hunks.emplace();
					for (const auto& [first, last] : *file.HeadHunks) {
						anchor.Hunks->push_back({ first, last });
					}
				}
				result.push_back(std::move(anchor));
			}
			return result;
		};
		if (draft->Remote || !EpochHost || !LastAcceptedHandoff || !WindowRead->accepted_snapshot()) {
			// [[BPR-2116]], RFC-BPR-0100 AC06. An update publishes an edited
			// stop's references as permalinks, so it needs the pull request's
			// files as a first publication does; a published tour's stops
			// keep their destinations, so nothing else is checked here.
			if (draft->Remote && WindowRead && WindowRead->accepted_snapshot() && TourHead == draft->ObservedHead.value_or(draft->Head)) {
				TourPublication->observe_anchors(TourHead, anchorFiles());
			}
			TourPublication->dispatch(qt::tour_editor_action::preview_publication);
			return;
		}
		const auto authority = workbench_authority(*LastAcceptedHandoff);
		const auto current   = EpochHost->context(authority);
		if (!current || TourHead != draft->ObservedHead.value_or(draft->Head)) {
			Window->tour_editor()->show_notice(QStringLiteral("The current source comparison is not ready. Wait for the pull request to load, then Preview again."));
			TourPublication->dispatch(qt::tour_editor_action::preview_publication);
			return;
		}
		auto files    = anchorFiles();
		auto requests = std::vector<application::workbench_selected_diff_request>{ };
		for (const auto& stop : draft->Stops) {
			const auto source = core::publication_source(stop);
			if (!source || std::ranges::any_of(requests, [&](const auto& aRequest) { return aRequest.File.PresentationPath == source->Path && aRequest.Diff.Comparison.Left.Commit == source->Commit; })) {
				continue;
			}
			const auto observed = core::publication_observation(*source, draft->ObservedHead.value_or(draft->Head), draft->Base, stop.Body, false,
																TourPublication->operation_support() == core::tour_operation_support::supported);
			const auto repair   = core::plan_tour_stop_repair(*draft, stop, observed, files);
			if (observed.Subject != core::tour_subject::historical && repair.Problem != core::tour_destination_problem::off_hunk) {
				continue;
			}
			const auto binding = core::agent_revision_binding{ .Host = authority.Host, .Repository = authority.Repository, .PullRequest = authority.PullRequest, .BaseOid = source->Commit, .HeadOid = draft->ObservedHead.value_or(draft->Head), .LeftEpoch = current->Comparison.Left.Identity.Epoch, .RightEpoch = current->Comparison.Right.Identity.Epoch };
			const auto context = EpochHost->realignment_context(authority, binding);
			if (!context) {
				continue;
			}
			application::workbench_selected_diff_request request;
			request.Context               = { core::request_id{ requests.size() + 1U }, authority.ApplicationSession, core::request_generation{ draft->Revision } };
			request.WorkbenchAuthority    = authority;
			request.Diff                  = *context;
			request.File.File             = core::file_identity_of(source->Path);
			request.File.PresentationPath = source->Path;
			request.File.CurrentPath      = native_filesystem_path(QString::fromUtf8(source->Path));
			request.File.Change           = application::workbench_file_change_kind::modified;
			request.File.ContentKind      = application::workbench_file_content_kind::text;
			requests.push_back(std::move(request));
		}
		++TourRepairGeneration;
		TourRepairDraft    = draft;
		TourRepairRequests = { requests.begin(), requests.end() };
		TourRepairFiles    = std::move(files);
		TourRepairComparisons.clear();
		TourRepairCommand.reset();
		Window->tour_editor()->show_notice(QStringLiteral("Checking pinned sources and current diff hunks. No comments are sent."));
		read_next_tour_repair();
	}

	void github_application::implementation::read_next_tour_repair()
	{
		const auto now = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Closing || !TourRepairDraft || !now || now->Owner != TourRepairDraft->Owner || now->Id != TourRepairDraft->Id || now->Revision != TourRepairDraft->Revision) {
			return;
		}
		if (TourRepairRequests.empty()) {
			for (const auto& comparison : TourRepairComparisons) {
				if (comparison.Left.Commit != comparison.Right.Commit) {
					TourHost->observe_comparison(comparison.Index, comparison.Left, comparison.Right);
				}
			}
			TourPublication->observe_anchors(TourRepairDraft->ObservedHead.value_or(TourRepairDraft->Head), TourRepairFiles);
			TourPublication->dispatch(qt::tour_editor_action::preview_publication);
			return;
		}
		auto request = std::move(TourRepairRequests.front());
		TourRepairRequests.pop_front();
		// The epoch actor grants one realignment at a time. Authorize each
		// exact pair immediately before its read, on the decision lane.
		const auto context = EpochHost->realignment_context(request.WorkbenchAuthority,
															{ .Host = request.WorkbenchAuthority.Host, .Repository = request.WorkbenchAuthority.Repository, .PullRequest = request.WorkbenchAuthority.PullRequest, .BaseOid = request.Diff.Comparison.Left.Commit, .HeadOid = request.Diff.Comparison.Right.Commit, .LeftEpoch = request.Diff.Comparison.Left.Identity.Epoch, .RightEpoch = request.Diff.Comparison.Right.Identity.Epoch });
		if (!context) {
			read_next_tour_repair();
			return;
		}
		request.Diff = *context;
		read_tour_comparison(std::move(request), [this, generation = TourRepairGeneration](std::optional<application::tour_source_comparison> aProduced) {
			if (Closing || generation != TourRepairGeneration) {
				return;
			}
			if (aProduced) {
				const auto found = std::ranges::find(TourRepairFiles, aProduced->Right.Path, &core::tour_anchor_file::Path);
				if (found != TourRepairFiles.end() && aProduced->Index.diff().RightDocument) {
					found->HeadLines = static_cast<std::uint32_t>(aProduced->Index.diff().RightDocument->Lines.size());
				}
				TourRepairComparisons.push_back(std::move(*aProduced));
			}
			read_next_tour_repair();
		});
	}

	void github_application::implementation::read_tour_comparison(application::workbench_selected_diff_request aComparison, std::function<void(std::optional<application::tour_source_comparison>)> aDone)
	{
		struct comparison_read
		{
			application::workbench_selected_diff_request                            Comparison;
			std::function<void(std::optional<application::tour_source_comparison>)> Done;
			std::optional<application::tour_pinned_file>                            Left;
		};
		auto read = std::make_shared<comparison_read>(comparison_read{ std::move(aComparison), std::move(aDone), std::nullopt });
		// One side: grant its identical-commit pair here, read it on the diff
		// lane, and come back here for the other side or the answer.
		auto side = std::make_shared<std::function<void(core::diff_side)>>();
		*side     = [this, read, weak = std::weak_ptr{ side }](const core::diff_side aSide) {
			const auto& comparison = read->Comparison;
			const auto  commit     = aSide == core::diff_side::left ? comparison.Diff.Comparison.Left.Commit : comparison.Diff.Comparison.Right.Commit;
			const auto  current    = Closing || !EpochHost ? std::optional<application::workbench_selected_diff_context>{ } : EpochHost->context(comparison.WorkbenchAuthority);
			const auto  context    = current ? EpochHost->realignment_context(comparison.WorkbenchAuthority, { comparison.WorkbenchAuthority.Host, comparison.WorkbenchAuthority.Repository, comparison.WorkbenchAuthority.PullRequest, commit, commit, current->Comparison.Left.Identity.Epoch, current->Comparison.Right.Identity.Epoch })
											 : std::optional<application::workbench_selected_diff_context>{ };
			if (!context) {
				read->Done(std::nullopt);
				return;
			}
			auto request        = comparison;
			request.Diff        = *context;
			request.File.Change = application::workbench_file_change_kind::unchanged;
			auto* const epoch   = EpochHost.get();
			auto* const lane    = &DecisionLane;
			TourSourceReadsInFlight.fetch_add(1U, std::memory_order_acq_rel);
			try {
				DiffEffects.post([this, epoch, lane, matcher = LineMatcher, read, request, aSide, next = weak.lock(), token = TourSourceStop.get_token()] {
					auto produced = std::optional<application::tour_source_comparison>{ };
					auto file     = std::optional<application::tour_pinned_file>{ };
					try {
						file = application::materialize_pinned_tour_file(*epoch, request, token);
						if (file && aSide == core::diff_side::right && read->Left) {
							produced = application::compare_tour_source_files(*matcher, read->Comparison, *read->Left, *file, token);
						}
					} catch (...) {
						file.reset();
						produced.reset();
					}
					try {
						lane->post([this, read, aSide, next, pinned = std::move(file), answer = std::move(produced)]() mutable {
							TourSourceReadsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
							if (Closing) {
								return;
							}
							if (aSide == core::diff_side::left && pinned && next) {
								read->Left = std::move(pinned);
								(*next)(core::diff_side::right);
								return;
							}
							read->Done(std::move(answer));
						});
					} catch (...) {
						TourSourceReadsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
					}
				});
			} catch (...) {
				TourSourceReadsInFlight.fetch_sub(1U, std::memory_order_acq_rel);
				read->Done(std::nullopt);
			}
		};
		(*side)(core::diff_side::left);
	}

	void github_application::implementation::check_tour_material()
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Closing || !draft || draft->ReadOnly || TourMaterialReading || !EpochHost || !LastAcceptedHandoff || !WindowRead->accepted_snapshot() || !TourRepairRequests.empty()) {
			return;
		}
		const auto head = core::tour_current_head(*draft);
		if (TourHead != head) {
			return;
		}
		const auto authority = workbench_authority(*LastAcceptedHandoff);
		const auto current   = EpochHost->context(authority);
		if (!current || current->Comparison.Right.Commit != head) {
			return;
		}
		auto requests = std::vector<application::workbench_selected_diff_request>{ };
		for (const auto& stop : draft->Stops) {
			if (!stop.Acceptance) {
				continue;
			}
			for (const auto& material : stop.Acceptance->Material) {
				const auto& range = material.Range;
				if (range.Commit == head || (material.CheckedAt == head && material.Verdict != core::tour_material_verdict::unchecked)) {
					continue;
				}
				// One read per file, commit and head; a file read once at a
				// head is not asked again there. A read that failed is asked
				// again on a later presentation, a bounded number of times.
				auto key = application::tour_material_key{ range.Commit, head, range.Path };
				if (!TourMaterial.may_ask(key)) {
					continue;
				}
				const auto binding = core::agent_revision_binding{ .Host = authority.Host, .Repository = authority.Repository, .PullRequest = authority.PullRequest, .BaseOid = range.Commit, .HeadOid = head, .LeftEpoch = current->Comparison.Left.Identity.Epoch, .RightEpoch = current->Comparison.Right.Identity.Epoch };
				const auto context = EpochHost->realignment_context(authority, binding);
				if (!context) {
					// Not readable yet: nothing was asked, so it is asked again.
					continue;
				}
				TourMaterial.asked(std::move(key));
				application::workbench_selected_diff_request request;
				request.Context               = { core::request_id{ requests.size() + 1U }, authority.ApplicationSession, core::request_generation{ draft->Revision } };
				request.WorkbenchAuthority    = authority;
				request.Diff                  = *context;
				request.File.File             = core::file_identity_of(range.Path);
				request.File.PresentationPath = range.Path;
				request.File.CurrentPath      = native_filesystem_path(QString::fromUtf8(range.Path));
				request.File.Change           = application::workbench_file_change_kind::modified;
				request.File.ContentKind      = application::workbench_file_content_kind::text;
				requests.push_back(std::move(request));
			}
		}
		if (requests.empty()) {
			return;
		}
		++TourMaterialGeneration;
		TourMaterialRequests = { requests.begin(), requests.end() };
		TourMaterialReading  = true;
		read_next_tour_material();
	}

	void github_application::implementation::read_next_tour_material()
	{
		if (Closing || !TourHost || TourMaterialRequests.empty()) {
			TourMaterialReading = false;
			TourMaterialRequests.clear();
			// [[BPR-2343]]. The new head can be published while a material
			// read is active. Recheck here so a comparison that became stale
			// during that publication can use its remaining ledger attempts.
			check_tour_material();
			return;
		}
		auto request = std::move(TourMaterialRequests.front());
		TourMaterialRequests.pop_front();
		// As a repair read: the epoch actor grants one realignment at a time,
		// so each exact pair is authorized immediately before its read.
		const auto context = EpochHost->realignment_context(request.WorkbenchAuthority,
															{ .Host = request.WorkbenchAuthority.Host, .Repository = request.WorkbenchAuthority.Repository, .PullRequest = request.WorkbenchAuthority.PullRequest, .BaseOid = request.Diff.Comparison.Left.Commit, .HeadOid = request.Diff.Comparison.Right.Commit, .LeftEpoch = request.Diff.Comparison.Left.Identity.Epoch, .RightEpoch = request.Diff.Comparison.Right.Identity.Epoch });
		if (!context) {
			TourMaterial.answered(tour_material_key_of(request), false);
			read_next_tour_material();
			return;
		}
		request.Diff = *context;
		read_tour_comparison(request, [this, request, generation = TourMaterialGeneration](std::optional<application::tour_source_comparison> aProduced) {
			if (Closing || generation != TourMaterialGeneration) {
				return;
			}
			// Re-review finding 3: a read that left the code uncompared
			// (no verdict) is asked again like one that failed.
			const auto key = tour_material_key_of(request);
			if (aProduced && TourHost && aProduced->Left.Commit != aProduced->Right.Commit) {
				TourHost->observe_comparison(aProduced->Index, aProduced->Left, aProduced->Right);
				const auto draft = TourHost->presentation().Draft;
				TourMaterial.answered(key, draft && application::tour_material_compared(*draft, key));
			} else {
				TourMaterial.answered(key, false);
			}
			read_next_tour_material();
		});
	}

	auto github_application::implementation::tour_material_key_of(const application::workbench_selected_diff_request& aRequest) -> application::tour_material_key
	{
		return { aRequest.Diff.Comparison.Left.Commit, aRequest.Diff.Comparison.Right.Commit, aRequest.File.PresentationPath };
	}

	auto github_application::implementation::tour_changed_files() const -> std::vector<std::string>
	{
		const auto& snapshot = WindowRead->accepted_snapshot();
		return TourOwner ? application::tour_changed_paths(*TourOwner, snapshot ? &*snapshot : nullptr) : std::vector<std::string>{ };
	}

	void github_application::implementation::dispatch_tour_repair(const core::tour_stop_id aStop, const qt::tour_repair_action aAction)
	{
		if (!Closing && TourHost && current(DraftWindow) && aAction == qt::tour_repair_action::preview_relocation) {
			Window->tour_editor()->go_to_stop(aStop);
			Window->open_tour_page(true);
		}
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (Closing || !draft || !TourPublication || !current(DraftWindow) || !TourRepairDraft || TourRepairDraft->Owner != draft->Owner || TourRepairDraft->ObservedHead.value_or(TourRepairDraft->Head) != draft->ObservedHead.value_or(draft->Head)) {
			return;
		}
		if (aAction == qt::tour_repair_action::file_comment) {
			if (const auto command = TourPublication->file_comment_edit(*draft, aStop)) {
				TourHost->dispatch(*command);
			}
			return;
		}
		if (aAction == qt::tour_repair_action::apply_relocation) {
			if (TourRepairCommand && TourRepairCommand->Stop == aStop) {
				const auto command = std::exchange(TourRepairCommand, std::nullopt);
				TourHost->dispatch(*command);
			}
			return;
		}
		const auto stop = std::ranges::find(draft->Stops, aStop, &core::tour_stop::Id);
		if (stop == draft->Stops.end()) {
			return;
		}
		const auto source = core::publication_source(*stop);
		if (!source) {
			return;
		}
		const auto observation = core::publication_observation(*source, draft->ObservedHead.value_or(draft->Head), draft->Base, stop->Body, false,
															   TourPublication->operation_support() == core::tour_operation_support::supported);
		const auto kind        = aAction == qt::tour_repair_action::repin ? core::tour_source_repair_kind::repin_head : core::tour_source_repair_kind::nearest_hunk;
		const auto command     = core::prepare_tour_source_repair(*draft, aStop, kind, observation, TourRepairFiles);
		if (!command) {
			Window->tour_editor()->show_notice(QStringLiteral("This destination changed. Preview again to check its current repairs."));
			return;
		}
		if (aAction == qt::tour_repair_action::repin) {
			TourHost->dispatch(*command);
			return;
		}
		const auto repair = core::plan_tour_stop_repair(*draft, *stop, observation, TourRepairFiles);
		if (!repair.Relocation) {
			return;
		}
		for (const auto& comparison : TourRepairComparisons) {
			const auto& document = comparison.Index.diff().RightDocument;
			const auto& range    = *repair.Relocation;
			if (!document || comparison.Right.Commit != range.Commit || comparison.Right.Blob != range.Blob || comparison.Right.Path != range.Path || range.Last.Line >= document->Lines.size()) {
				continue;
			}
			const auto  first = document->Lines[range.First.Line].Content.Begin;
			const auto& last  = document->Lines[range.Last.Line];
			const auto  end   = std::max(last.Content.End, last.Terminator.End);
			const auto  text  = std::string{ document->Bytes.begin() + static_cast<std::ptrdiff_t>(first), document->Bytes.begin() + static_cast<std::ptrdiff_t>(end) };
			TourRepairCommand = command;
			Window->tour_editor()->present_repair_preview(aStop, range, text, draft->Revision);
			return;
		}
		Window->tour_editor()->show_notice(QStringLiteral("The proposed source text is unavailable. Preview again before moving this destination."));
	}

	void github_application::implementation::carry_local_tour(const application::tour_presentation& aProjection)
	{
		// A pull request made from a local draft starts its private tour
		// draft with the draft's tour, once, while that draft is still empty.
		if (!DraftDesktop || !TourOwner || TourCarryAsked == TourOwner || aProjection.Loading || !aProjection.Draft || !WindowRead->accepted_snapshot()) {
			return;
		}
		const auto& draft = *aProjection.Draft;
		if (!draft.Stops.empty() || draft.Remote || draft.ReadOnly) {
			TourCarryAsked = TourOwner;
			return;
		}
		const auto& snapshot = *WindowRead->accepted_snapshot();
		const auto& marks    = DraftDesktop->host().home().Marks;
		const auto  mark     = std::ranges::find_if(marks, [&snapshot](const application::local_draft_mark& aMark) {
			return aMark.Owner == snapshot.Repository.Owner && aMark.Name == snapshot.Repository.Name && aMark.Number == snapshot.PullRequest.Number.value();
		});
		if (mark == marks.end()) {
			return;
		}
		TourCarryAsked   = TourOwner;
		const auto weak  = std::weak_ptr<const int>{ WindowLifetime };
		const auto owner = *TourOwner;
		DraftDesktop->host().carry_tour(mark->Id, draft, [this, weak, owner](core::local_tour_carry aCarry) {
			if (weak.expired() || Closing || TourOwner != owner || !TourHost) {
				return;
			}
			const auto shown = TourHost->presentation();
			if (!shown.Draft || !shown.Draft->Stops.empty()) {
				// The author began this tour meanwhile: theirs stands.
				return;
			}
			// A carry the tour draft refuses is said, never dropped in silence
			// (the refusal that opened #182 with an empty tour was discarded here).
			auto refusal = core::tour_refusal::conflict;
			if (aCarry.Tour.Head == shown.Draft->Head) {
				const auto applied = TourHost->execute({ .Command = { .Draft = shown.Draft->Id, .Owner = shown.Draft->Owner, .ExpectedRevision = shown.Draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = std::move(aCarry.Tour) } });
				const auto taken   = applied.Status == application::tour_service_status::accepted || applied.Status == application::tour_service_status::unchanged;
				refusal            = taken ? core::tour_refusal::none : applied.Refusal != core::tour_refusal::none ? applied.Refusal
																													: core::tour_refusal::conflict;
			}
			if (const auto words = core::local_tour_carry_refusal_words(refusal); !words.empty() && Window != nullptr) {
				Window->tour_editor()->show_notice(QString::fromStdString(words));
			}
		});
	}

	auto github_application::implementation::local_tour_shown() const -> bool
	{
		// RFC-BPR-0110 as amended: while the window shows a local draft, its
		// Tour panel is the draft's, and the pull request's tour waits.
		return DraftDesktop && DraftDesktop->tour_shown();
	}

	void github_application::implementation::restore_tour_panel()
	{
		if (Window == nullptr || Closing) {
			return;
		}
		Window->tour_editor()->set_local_words(std::nullopt);
		// RFC-BPR-0120. A pull request's tour plans no check.
		Window->tour_editor()->side_chat()->set_check_dispatch({ });
		// Its chats keep sessions, in History.
		Window->tour_editor()->side_chat()->set_sessions_offered(true);
		Window->tour_editor()->set_tour_running(std::nullopt, false);
		if (TourHost) {
			auto shown         = TourHost->presentation();
			shown.ChangedFiles = tour_changed_files();
			Window->tour_editor()->present(shown);
		} else {
			Window->tour_editor()->present({ });
		}
		present_tour_session();
	}

	void github_application::implementation::dispatch_tour_action(qt::tour_editor_action aAction)
	{
		if (local_tour_shown()) {
			DraftDesktop->tour_action(aAction);
			return;
		}
		if (Closing || !TourHost || !current(DraftWindow)) {
			return;
		}
		switch (aAction) {
		case qt::tour_editor_action::preview_publication:
			prepare_tour_repairs();
			return;
		case qt::tour_editor_action::open_published:
			// [[BPR-1991]]. The head's On GitHub: the published tour's address,
			// through the window's browser port.
			if (const auto draft = TourHost->presentation().Draft; draft && draft->Remote && !draft->Remote->WebUrl.empty() && Profile->WindowBrowser != nullptr) {
				static_cast<void>(Profile->WindowBrowser->open(draft->Remote->WebUrl));
			}
			return;
		case qt::tour_editor_action::choose_connection:
			// [[BPR-1992]]. A refusal's Use another connection.
			Window->open_sign_in();
			return;
		case qt::tour_editor_action::publish_publication:
		case qt::tour_editor_action::resume_publication:
		case qt::tour_editor_action::reconcile_publication:
		case qt::tour_editor_action::stop_publication:
			if (TourPublication) {
				TourPublication->dispatch(aAction);
			}
			return;
		// Each on its own lines: clang-format 22 and 23 align short case
		// statements across label-only groups differently ([[BPR-1243]]).
		case qt::tour_editor_action::import_selected:
			TourHost->import_selected(Window->tour_editor()->import_choice());
			return;
		case qt::tour_editor_action::retry_save:
			TourHost->retry_save();
			return;
		case qt::tour_editor_action::use_saved_copy:
			TourHost->use_saved_copy();
			return;
		case qt::tour_editor_action::keep_local_conflict:
		case qt::tour_editor_action::use_remote_conflict:
			if (const auto draft = Window->tour_editor()->shown().Draft) {
				TourHost->resolve_remote_conflict(draft->Revision, aAction == qt::tour_editor_action::use_remote_conflict);
			}
			return;
		case qt::tour_editor_action::undo_structure: TourHost->undo(); return;
		case qt::tour_editor_action::redo_structure: TourHost->redo(); return;
		case qt::tour_editor_action::navigate_stop:  {
			const auto*                                stop = Window->tour_editor()->selected_stop();
			std::optional<core::tour_code_destination> source;
			if (stop) {
				source = stop->Destination;
				if (stop->Mapped || stop->Original) {
					source = core::destination_of(stop->Mapped ? *stop->Mapped : *stop->Original);
				}
			}
			navigate_tour_source(source);
			return;
		}
		case qt::tour_editor_action::ask_code:
			// [[BPR-1914]] v002. The lines selected in the code, or the stop's own.
			Window->tour_editor()->ask_about_code(selected_tour_source());
			return;
		case qt::tour_editor_action::navigate_related: {
			const auto related = Window->tour_editor()->selected_related();
			navigate_tour_source(related ? std::optional{ core::destination_of(*related) } : std::nullopt);
			return;
		}
		case qt::tour_editor_action::add_stop:
		case qt::tour_editor_action::insert_after:
		case qt::tour_editor_action::retarget_stop:
		case qt::tour_editor_action::add_related:   {
			const auto source = selected_tour_source();
			const auto draft  = TourHost->presentation().Draft;
			if (!draft) {
				return;
			}
			if (!source) {
				Window->open_review_tour();
				Window->tour_editor()->show_stops();
				Window->tour_editor()->show_notice(QStringLiteral("Select a line or contiguous range in an available source pane, then add it to the tour. Your draft remains saved."));
				return;
			}
			if (aAction == qt::tour_editor_action::add_related || aAction == qt::tour_editor_action::retarget_stop) {
				const auto* selected = Window->tour_editor()->selected_stop();
				if (!selected) {
					return;
				}
				auto value = *selected;
				if (aAction == qt::tour_editor_action::retarget_stop) {
					value.Mapped         = *source;
					value.Mapping        = core::tour_mapping::author_selected;
					value.ReaffirmedHead = draft->ObservedHead.value_or(draft->Head);
				} else {
					value.Related.push_back(*source);
				}
				TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::edit_stop, .Stop = value.Id, .Value = std::move(value) });
				return;
			}
			auto        position = draft->Stops.size();
			const auto* selected = Window->tour_editor()->selected_stop();
			if (selected) {
				const auto found = std::ranges::find(draft->Stops, selected->Id, &core::tour_stop::Id);
				position         = static_cast<std::size_t>(found - draft->Stops.begin()) + 1U;
			}
			TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::insert, .Stop = selected ? selected->Id : core::tour_stop_id{ }, .Value = { .Original = source, .Mapping = core::tour_mapping::exact_original, .ReaffirmedHead = draft->ObservedHead.value_or(draft->Head) }, .Position = position });
			Window->open_review_tour();
			Window->tour_editor()->show_stops();
			return;
		}
		}
	}
}
