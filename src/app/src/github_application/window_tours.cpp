#include "github_application/window_composition.hpp"

#include "bpr/adapters/sqlite/review_tour_store.hpp"
#include "bpr/adapters/sqlite/tour_owner_channel.hpp"
#include "bpr/app/native_filesystem_path.hpp"
#include "bpr/application/review_tour_source.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/markdown_body_view.hpp"
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
#include <string_view>
#include <variant>

#if defined(BPR_AGENT_INTEGRATIONS)
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
			const bool untouched = p.Draft && p.Draft->Revision == 1U && p.Draft->Problem.empty() && p.Draft->Solution.empty() && p.Draft->Stops.empty() && !p.Draft->ProblemBaseline && !p.Draft->SolutionBaseline;
			return aRetained.Host->is_quiescent() && !p.Failure && p.Draft && (p.SavedRevision == p.Draft->Revision || untouched);
		});
		Window->prepare_tour_action()->setEnabled(false);
		Window->tour_editor()->present({ });
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
			return { TourHost.get(), true, Window && Window->tour_editor()->composing() };
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
		suspend_review_tour();
		if (const auto found = std::ranges::find(RetiredTourHosts, owner, &retained_tour_host::Owner); found != RetiredTourHosts.end()) {
			TourHost = std::move(found->Host);
			RetiredTourHosts.erase(found);
			TourOwner         = owner;
			TourAttentionHead = { };
			Window->tour_editor()->present(TourHost->presentation());
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
																	   auto projected = aProjection;
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
																	   Window->tour_editor()->present(projected);
#if defined(BPR_AGENT_INTEGRATIONS)
																	   // [[BPR-1931]]. Undo reset goes once the tour changes.
																	   present_tour_reset();
#endif
																	   finish_pending_tour_start(projected);
																	   if (TourPublication) {
																		   TourPublication->observe(projected);
																	   }
																	   Window->prepare_tour_action()->setEnabled(static_cast<bool>(aProjection.Draft));
																	   observe_tour_head();
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

	void github_application::implementation::cancel_writing()
	{
		WritingState.cancel();
		if (OutlineProposal) {
			OutlineProposal.reset();
			Window->tour_editor()->present_outline({ });
		}
		if (!PendingWriting) {
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

	/// \brief [[BPR-1753]], owner decision 2026-10-23. The agent
	/// tour writing asks: the one chosen in the Agent tab when it speaks
	/// Claude Code's stream JSON (tool-free writing needs it), otherwise the
	/// first added agent that does.
	auto github_application::implementation::writing_agent() const -> std::optional<core::agent_profile_id>
	{
		const auto selected = Window->agent_panel()->selected_profile();
#if defined(BPR_AGENT_INTEGRATIONS)
		// A built-in profile chosen (a course's own program) is asked as is.
		if (selected && (writes_tool_free(*selected) || !core::is_agent_entry_profile(*selected))) {
			return selected;
		}
		if (AgentSetup) {
			const auto listed = AgentSetup->projection().Preferences.Agents;
			for (const auto& entry : listed) {
				if (entry.Protocol == core::agent_protocol::claude_stream_json) {
					return core::entry_profile_id(entry);
				}
			}
		}
		// None speaks it: the chosen one, which is then refused with why.
		return selected;
#else
		return selected;
#endif
	}

	/// [[BPR-1878]]. An agent's name as the agent menus show it.
	auto github_application::implementation::agent_display_name(const core::agent_profile_id aProfile) const -> QString
	{
		const auto* const menu = Window->agent_panel()->profile_selector();
		const auto        row  = menu->findData(QVariant::fromValue<qulonglong>(aProfile.value()));
		return row >= 0 ? menu->itemText(row) : QStringLiteral("The agent");
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
		const auto& draft = Window->tour_editor()->shown().Draft;
		if (draft) {
			if (aHook != core::writing_hook::problem) {
				add("private-problem", draft->Problem, 1024);
			}
			if (aHook != core::writing_hook::solution) {
				add("private-solution", draft->Solution, 1024);
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
			Window->tour_editor()->show_writing_notice(Window->agent_panel()->offered_count() == 0 ? QStringLiteral("No agent is set up. Add one in Settings, Agents & context, or keep writing yourself.")
														   : !agent                                ? QStringLiteral("Choose an agent with the agent picker at the bottom of this tab, or keep writing yourself.")
														   : !target                               ? QStringLiteral("Only your own tour draft, open in this window, can be drafted for. You can keep writing yourself.")
																								   : QStringLiteral("The tour session is drafting. Wait for it to finish or stop it, or keep writing yourself."));
			return;
		}
		const auto request = WritingState.begin(*target, std::move(aCapture.Raw), aCapture.Selection);
		if (!request) {
			return;
		}
		auto sources = writing_sources(aHook, *request);
		// Recover an unambiguous existing author section before asking a model.
		if (request->Scope == core::writing_scope::empty_field && (aHook == core::writing_hook::problem || aHook == core::writing_hook::solution)) {
			const auto&                         description = WindowRead->accepted_snapshot()->PullRequest.Body;
			const auto                          map         = core::map_tour_markdown(description);
			std::optional<core::tour_text_span> recovered;
			bool                                ambiguous = false;
			for (const auto& section : map.Sections) {
				if (section.Name == (aHook == core::writing_hook::problem ? "Problem" : "Solution")) {
					if (recovered) {
						ambiguous = true;
					}
					recovered = section.Content;
				}
			}
			if (recovered && !ambiguous && map.Bounded) {
				auto edit = WritingState.complete(request->Id, *target, request->Raw, description.substr(recovered->Begin, recovered->End - recovered->Begin));
				if (edit && Window->tour_editor()->writing_editor(aHook)->apply(*edit)) {
					Window->tour_editor()->show_writing_notice(QStringLiteral("Recovered — pr-description. Existing author text; no inference."));
				}
				return;
			}
		}
		if (!writes_tool_free(target->Agent)) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("This agent has no qualified tool-free writing mode: writing needs one that speaks Claude Code's stream JSON. Add one in Settings › Agents & context, or keep writing manually."));
			return;
		}
		if (request->Scope == core::writing_scope::empty_field && (sources.empty() || (aHook == core::writing_hook::comment && std::ranges::find(sources, "stop-source", &application::writing_source::Id) == sources.end()))) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("Not enough approved context to draft an explanation. Add the author's problem or solution first."));
			return;
		}
		const auto prompt = application::build_writing_prompt(Profile->WorkbenchPreferences->accepted()->WritingHooks, *request, sources);
		if (!prompt) {
			WritingState.cancel();
			Window->tour_editor()->show_writing_notice(QStringLiteral("This request exceeds the writing context limit. Select a smaller passage or shorten the hook instructions."));
			return;
		}
#if defined(BPR_AGENT_INTEGRATIONS)
		// [[BPR-1900]]. Draft from PR context and Suggest an outline read the
		// repository in the background; a small rewrite stays tool-free.
		const auto kind     = core::tour_request_of(aHook, request->Scope);
		const auto readOnly = core::tools_for(kind) == core::tour_agent_tools::read_only;
		PendingWriting      = writing_operation{ *request, std::move(sources), { }, false, readOnly };
		Window->tour_editor()->show_writing_notice(readOnly ? tour_reading_notice() : QStringLiteral("Writing assistance running… Your text remains editable."));
		const auto asked = Window->tour_editor()->request_name(aHook);
		// [[BPR-1916]]. The skills the hook names, inline.
		auto skills = skill_run_for(aHook);
		auto text   = (readOnly ? std::string{ application::read_only_writing_preamble() } + *prompt : *prompt) + skills.Directive;
		if (skills.Note) {
			Window->tour_editor()->show_writing_notice((readOnly ? tour_reading_notice() : QStringLiteral("Writing assistance running… Your text remains editable.")) + QStringLiteral(" ")
													   + QString::fromStdString(*skills.Note));
		}
		if (!dispatch_tour_run(target->Agent, kind, asked.toStdString(), text, readOnly ? std::optional{ aHook } : std::nullopt, std::nullopt, core::tour_thread{ }, false, std::move(skills))) {
			cancel_writing();
		}
#else
		PendingWriting = writing_operation{ *request, std::move(sources), { }, false };
		cancel_writing();
#endif
	}
#if defined(BPR_AGENT_INTEGRATIONS)
	/// [[BPR-1900]]. The tour session's process chain and host: its own
	/// launcher and lane, the Claude wrapper, program and added-agent
	/// ports the Agent tab's launches use, and its own projection.
	void github_application::implementation::compose_tour_session(const std::vector<core::agent_profile>& aProfiles, const std::vector<core::agent_host_policy>& aPolicies)
	{
		TourProcess = std::make_unique<adapters::agent::agent_process_launcher>();
	#if defined(BPR_BUILD_TESTING)
		// Counted as the Agent tab's launcher is, so a course sees a tour
		// run start and finish.
		TourProcessProbe = std::make_unique<composition_agent_process_launcher_probe>(*TourProcess);
		auto& launcher   = static_cast<application::agent_process_port&>(*TourProcessProbe);
	#else
		auto& launcher = static_cast<application::agent_process_port&>(*TourProcess);
	#endif
		TourClaudeProcess    = std::make_unique<application::wrapped_claude_process>(launcher, ClaudeWrapperSource, &adapters::agent::inspect_claude_process_wrapper, locate_claude());
		TourLocatedProcess   = std::make_unique<application::located_agent_process>(*TourClaudeProcess, AgentProgramSource, &adapters::agent::inspect_claude_process_wrapper, locate_agent_program());
		TourEntryProcess     = std::make_unique<application::agent_entry_process>(launcher, *TourLocatedProcess, AgentEntries, &adapters::agent::inspect_claude_process_wrapper, locate_claude(), locate_agent_program());
		TourDiagnosedProcess = std::make_unique<application::diagnosed_agent_process>(*TourEntryProcess, DiagnosticLog);
		TourProjection       = std::make_unique<tour_session_projection>(
			[this](const application::agent_invocation_projection_identity& aIdentity) { tour_run_admitted(aIdentity); },
			[this](const application::agent_invocation_projection_identity&, const application::prepared_workspace& aWorkspace) {
				TourWorkspacePath = aWorkspace.Path;
				TourWorkspaceHead = aWorkspace.PinnedHead;
			},
			[this](const application::agent_invocation_projection_identity& aIdentity, const application::agent_run_result& aResult) { tour_run_finished(aIdentity, aResult); },
			[this](const std::string_view aReason) { tour_run_refused(aReason); });
		TourSession = std::make_unique<application::agent_invocation_host>(
			aProfiles, aPolicies, AgentWorkspaceNeighbourhood, *this, *EpochHost, *TourDiagnosedProcess,
			[](const std::string& aName) -> std::optional<std::string> {
				if (!qEnvironmentVariableIsSet(aName.c_str())) {
					return std::nullopt;
				}
				return qEnvironmentVariable(aName.c_str()).toStdString();
			},
			TourAgentEffects, DecisionLane, *TourProjection, nullptr);
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
		if (writes_tool_free(aProfile) || aProfile == core::codex_jsonl_profile_id) {
			return true;
		}
		if (core::is_agent_entry_profile(aProfile) && AgentSetup) {
			const auto        agents = AgentSetup->projection().Preferences.Agents;
			const auto* const entry  = core::entry_of_profile(agents, aProfile);
			return entry != nullptr && entry->Protocol == core::agent_protocol::codex_jsonl_exec;
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
		const auto tools = core::tools_for(aKind);
		const auto run   = aKind == core::tour_agent_request::ask ? core::begin_tour_ask(TourAgent, aThread, aPassage, aAsked, aUpdate) : core::begin_tour_run(TourAgent, aKind, tools, aAsked);
		if (!run) {
			return false;
		}
		TourRun        = *run;
		TourRunStarted = std::chrono::steady_clock::now();
		TourInvocation.reset();
		TourWorkspacePath.clear();
		TourWorkspaceHead.reset();
		auto launch = tour_launch{ .ReadOnly = tools == core::tour_agent_tools::read_only, .Skills = std::move(aSkills) };
		if ((aKind == core::tour_agent_request::draft_tour || aKind == core::tour_agent_request::ask) && WindowRead->accepted_snapshot()) {
			for (const auto& file : WindowRead->accepted_snapshot()->Files) {
				if (file.Change != application::github_file_change::removed) {
					launch.Measure.push_back(file.Path);
				}
			}
		}
		Window->tour_editor()->set_tour_running(aRunFor, aKind == core::tour_agent_request::draft_tour, aKind == core::tour_agent_request::ask);
		present_tour_session();
		if (!dispatch_agent_invocation(aAgent, { }, { }, false, aPrompt, { }, core::agent_entry_point::ask_about_selection, { }, true, launch)) {
			static_cast<void>(core::finish_tour_run(TourAgent, *run, core::tour_agent_run_state::failed, "The tour session could not start this request."));
			TourRun.reset();
			Window->tour_editor()->set_tour_running(std::nullopt, false);
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
		Window->tour_editor()->set_tour_running(std::nullopt, false);
		TourAskPending = false;
		if (TourDraftPending) {
			TourDraftPending = false;
			Window->tour_editor()->show_tour_draft_failure(aReason == "agent_context_too_large"
															   ? QStringLiteral("This pull request is too large to describe to the agent in one request.")
															   : QStringLiteral("The selected agent can't draft with read-only tools. Choose Claude or Codex with the agent picker at the bottom of this tab."));
		}
		present_tour_session();
		if (!PendingWriting) {
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
		const auto words = qt::agent_tab_view::run_failure_words(aResult.Outcome, agent_display_name(aIdentity.Session.Profile));
		if (TourRun) {
			static_cast<void>(core::record_tour_activity(TourAgent, *TourRun, application::tour_transcript_of(aResult.ToolActivity, TourWorkspacePath)));
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
		}
		TourRun.reset();
		TourInvocation.reset();
		Window->tour_editor()->set_tour_running(std::nullopt, false);
		if (TourDraftPending) {
			TourDraftPending = false;
			finish_tour_draft(run, aResult, words);
			// [[BPR-1930]] AC3. Feedback given while it drafted goes next, to
			// revise what it proposed; a draft that didn't finish says so in
			// the tour chat, with the feedback as written.
			if (TourAgent.Waiting && aResult.Outcome == application::agent_run_outcome::success && TourProposal) {
				send_waiting_tour_feedback();
			} else if (const auto waiting = core::take_tour_feedback(TourAgent)) {
				if (const auto turn = core::begin_tour_ask(TourAgent, core::tour_thread{ }, std::nullopt, *waiting)) {
					static_cast<void>(core::finish_tour_run(TourAgent, *turn, core::tour_agent_run_state::failed, "The draft didn't propose a tour, so this feedback wasn't sent. Send it again when you want."));
				}
			}
		} else if (TourAskPending) {
			TourAskPending = false;
		} else {
			finish_writing(aIdentity, aResult);
		}
		present_tour_session();
	}

	/// [[BPR-1914]], owner 2026-11-06: "I want to ask questions, get back
	/// and forth and have it update it." The session is one conversation
	/// per tour: a run started on another tour lets the earlier tour's
	/// turns and proposal go (run numbers keep counting).
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
		// [[BPR-1930]] AC3, owner: "particularly during the initial agent
		// draft". Feedback on the whole tour while Draft the tour runs waits
		// for its proposal, then revises it.
		if (aThread.Kind == core::tour_thread_kind::tour && !aUpdate && TourDraftPending && core::queue_tour_feedback(TourAgent, question)) {
			Window->tour_editor()->show_writing_notice(QStringLiteral("Your feedback is sent to the tour agent when the draft is ready, so it revises that draft."));
			present_tour_session();
			return;
		}
		// A turn that cannot run still lands in its side chat, saying why.
		const auto refuse = [this, &aThread, &aPassage, &question, aUpdate](const std::string& aWhy) {
			if (const auto run = core::begin_tour_ask(TourAgent, aThread, aPassage, question, aUpdate)) {
				static_cast<void>(core::finish_tour_run(TourAgent, *run, core::tour_agent_run_state::failed, aWhy));
			}
			present_tour_session();
		};
		const auto agent = writing_agent();
		if (!agent) {
			refuse("No agent is chosen. Choose one with the agent picker at the bottom of the Tour tab.");
			return;
		}
		if (!tour_session_idle()) {
			refuse("The tour agent was still finishing its last run. Send this again in a moment.");
			return;
		}
		if (!reads_read_only(*agent)) {
			refuse(QStringLiteral("%1 has no read-only mode to read the code with. Choose Claude or Codex with the agent picker at the bottom of the Tour tab.").arg(agent_display_name(*agent)).toStdString());
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
		const auto texts  = std::array{ std::string_view{ question } };
		auto       skills = prepare_skill_run(SkillLibrary, texts);
		const auto prompt = application::build_tour_ask_prompt(snapshot->PullRequest.Title, snapshot->PullRequest.Body, files, *draft, core::tour_conversation_of(TourAgent, aThread), aThread, aPassage, question, aUpdate, TourProposal, TourAgent.Focus, skills.Directive);
		if (!prompt) {
			refuse("This question, with the pull request and the conversation so far, is too large to send in one request.");
			return;
		}
		if (skills.Note) {
			Window->tour_editor()->show_writing_notice(QString::fromStdString(*skills.Note));
		}
		TourAskPending = true;
		if (!dispatch_tour_run(*agent, core::tour_agent_request::ask, question, *prompt, std::nullopt, aPassage, aThread, aUpdate, std::move(skills))) {
			TourAskPending = false;
		}
	}

	/// [[BPR-1914]]. An answered turn: its reply in the conversation, and
	/// any change it proposes added to the proposal, anchored as a draft's.
	void github_application::implementation::finish_tour_ask(const core::tour_agent_run_id aRun, const application::agent_run_result& aResult)
	{
		const auto said   = aResult.AssistantItems.empty() ? std::string{ } : aResult.AssistantItems.back();
		const auto answer = application::decode_tour_ask_answer(said);
		if (!answer) {
			static_cast<void>(core::finish_tour_run(TourAgent, aRun, core::tour_agent_run_state::failed, "The agent's answer wasn't one this app can read. Nothing in the tour changed.\n\n" + said.substr(0U, std::min<std::size_t>(said.size(), 1500U))));
			return;
		}
		const auto draft    = TourHost ? TourHost->presentation().Draft : nullptr;
		auto       proposal = std::optional<core::tour_proposal>{ };
		if (draft) {
			auto files = tour_anchor_files(aResult);
			proposal   = core::propose_tour_changes(aRun, *answer, files, *draft, TourWorkspaceHead);
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
			TourProposal = core::merge_tour_proposal(TourProposal, std::move(*proposal));
		}
		if (refused > 0U) {
			// [[BPR-1937]] AC2. A refused part is said, with why, on the tour
			// as in the chat.
			const auto refusal = refused == 1U ? QStringLiteral("1 part of the tour agent's answer couldn't be proposed. The chat says why.")
											   : QStringLiteral("%1 parts of the tour agent's answer couldn't be proposed. The chat says why.").arg(refused);
			Window->tour_editor()->show_writing_notice(items ? QStringLiteral("The tour agent's other changes wait in the tour, marked grey. ") + refusal : refusal);
		} else if (items) {
			// [[BPR-1930]]. The tour chat's proposal waits in the tour itself.
			Window->tour_editor()->show_writing_notice(whole ? QStringLiteral("The tour agent's changes wait in the tour, marked grey. Nothing changes until you accept them.")
															 : QStringLiteral("The tour agent proposed a change in the side chat. Nothing changes until you accept it."));
		}
	}

	/// [[BPR-1930]] AC3. The feedback given while Draft the tour ran, sent as
	/// the tour chat's next turn once the run has fully ended, so it is given
	/// the draft's proposal and revises it.
	void github_application::implementation::send_waiting_tour_feedback(const int aTries)
	{
		if (Closing || Window == nullptr || !TourAgent.Waiting) {
			return;
		}
		if (!tour_session_idle() && aTries < 50) {
			QTimer::singleShot(100, Window->tour_editor(), [this, aTries] { send_waiting_tour_feedback(aTries + 1); });
			return;
		}
		const auto feedback = core::take_tour_feedback(TourAgent);
		if (feedback) {
			request_tour_ask(core::tour_thread{ }, std::nullopt, QString::fromStdString(*feedback), false);
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
		TourResetUndo = std::move(snapshot);
		TourAgent     = core::tour_agent_session{ .LastRun = TourAgent.LastRun };
		TourProposal.reset();
		TourAnchorFiles.clear();
		Window->tour_editor()->show_writing_notice({ });
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
		const auto said = aResult.AssistantItems.empty() ? std::string{ } : aResult.AssistantItems.back();
		if (!TourDraftPending) {
			return said;
		}
		const auto answer = application::decode_tour_draft_answer(said);
		if (!answer) {
			return "An answer that isn't a tour this app can read.";
		}
		auto parts = std::vector<std::string>{ };
		if (answer->Problem && !answer->Problem->empty()) {
			parts.emplace_back("a Problem");
		}
		if (answer->Solution && !answer->Solution->empty()) {
			parts.emplace_back("a Solution");
		}
		parts.push_back(std::to_string(answer->Stops.size()) + (answer->Stops.size() == 1U ? " stop" : " stops"));
		auto words = std::string{ "Proposed " };
		for (std::size_t index = 0U; index < parts.size(); ++index) {
			words += (index == 0U ? "" : index + 1U == parts.size() ? " and "
																	: ", ")
				+ parts[index];
		}
		return words + ".";
	}

	/// [[BPR-1900]], owner addition. Draft the tour: the whole tour,
	/// drafted in the background from the workspace, as a proposal.
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
			Window->tour_editor()->show_writing_notice(!agent ? QStringLiteral("Choose an agent with the agent picker at the bottom of this tab, or keep writing yourself.")
															  : QStringLiteral("The tour session is drafting. Wait for it to finish or stop it, or keep writing yourself."));
			return;
		}
		if (!reads_read_only(*agent)) {
			Window->tour_editor()->show_tour_draft_failure(QStringLiteral("%1 has no read-only drafting mode. Choose Claude or Codex with the agent picker at the bottom of this tab.").arg(agent_display_name(*agent)));
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
		adopt_tour_conversation();
		const auto prompt = application::build_tour_draft_prompt(snapshot->PullRequest.Title, snapshot->PullRequest.Body, files, *draft, TourAgent.Focus);
		if (!prompt) {
			Window->tour_editor()->show_tour_draft_failure(QStringLiteral("This pull request is too large to describe to the agent in one request."));
			return;
		}
		TourProposal.reset();
		TourDraftPending = true;
		Window->tour_editor()->show_writing_notice(tour_reading_notice());
		if (!dispatch_tour_run(*agent, core::tour_agent_request::draft_tour, "Draft Tour", *prompt, std::nullopt)) {
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
		auto files      = tour_anchor_files(aResult);
		TourProposal    = core::propose_tour(*aRun, *answer, files, *draft, TourWorkspaceHead);
		TourAnchorFiles = std::move(files);
		// [[BPR-1900]] owner look v002. Stops left out of the proposal are
		// kept in the run's transcript, so nothing the agent said is lost.
		static_cast<void>(core::note_finished_tour_run(TourAgent, *aRun, core::left_out_entries(*TourProposal)));
		Window->tour_editor()->show_writing_notice(TourProposal->Items.empty() ? QStringLiteral("The agent proposed nothing for this tour.")
																			   : QStringLiteral("A proposed tour is ready to review. Nothing changes until you accept it."));
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
			TourHost->dispatch(core::tour_command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::restore, .Restore = *revised });
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
				command.Kind = item.Part == core::tour_proposal_part::problem ? core::tour_command_kind::problem : core::tour_command_kind::solution;
				command.Text = item.Text;
			}
			TourHost->dispatch(command);
		}
		TourProposal = std::move(decided);
		if (std::ranges::none_of(TourProposal->Items, [](const core::tour_proposal_item& aItem) { return aItem.Decision == core::tour_proposal_decision::pending; })) {
			TourProposal.reset();
			Window->tour_editor()->show_writing_notice(QStringLiteral("Proposal done. Visit each accepted stop and confirm its lines in the code, then mark it Ready."));
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

	void github_application::implementation::present_tour_session()
	{
		if (Window != nullptr) {
			Window->tour_editor()->present_tour_session(TourAgent, TourProposal);
			present_tour_reset();
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
	auto github_application::implementation::outline_entries(const writing_operation& aOperation, const application::writing_result& aResult) const -> std::optional<std::vector<core::tour_outline_entry>>
	{
		std::vector<core::tour_outline_entry> entries;
		for (const auto& stop : aResult.Stops) {
			const auto source = std::ranges::find(aOperation.Sources, stop.Anchor, &application::writing_source::Id);
			if (source == aOperation.Sources.end()) {
				return std::nullopt;
			}
			core::tour_outline_entry entry{ .Title = stop.Title, .Source = source->Source, .Suggestion = { stop.Anchor, stop.Reason, stop.Question } };
			for (const auto& ref : stop.Related) {
				const auto related = std::ranges::find(aOperation.Sources, ref, &application::writing_source::Id);
				if (related == aOperation.Sources.end() || !related->Source) {
					return std::nullopt;
				}
				entry.Related.push_back(*related->Source);
			}
			entries.push_back(std::move(entry));
		}
		return entries;
	}

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
		if (hook == core::writing_hook::outline) {
			const auto target = current_outline_target();
			if (!target || !WritingState.complete_outline(aOperation.Request.Id, *target)) {
				return "The captured tour changed. No skeletons were inserted.";
			}
			auto entries = outline_entries(aOperation, aResult);
			if (!entries) {
				return "An outline source or related destination is unavailable. No skeletons were inserted.";
			}
			Window->tour_editor()->present_outline(*entries);
			OutlineProposal = tour_outline_proposal{ *target, std::move(*entries) };
			return "Choose the skeletons to insert. Each starts empty and needs your individual completion.";
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
		if (result && prefs && prefs->WritingHooks.Revision == operation.Request.Target.HookRevision && writing_context_key() == operation.Request.Target.ContextDigest) {
			message = apply_writing_result(operation, *result);
		} else if (aResult.Outcome == application::agent_run_outcome::success) {
			message = "Assistance returned an invalid, unsupported or stale result. Your text is unchanged.";
		} else if (aResult.ProtocolFailure == application::agent_protocol_failure::not_signed_in) {
			// [[BPR-1375]]. Writing runs only Claude, on its own sign-in.
			message = "Claude isn't signed in. Run claude auth login in a terminal, then try again. Your text is unchanged.";
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

	auto github_application::implementation::current_outline_target() const -> std::optional<core::writing_target>
	{
		const auto draft = TourHost ? TourHost->presentation().Draft : nullptr;
		if (!draft || draft->ReadOnly) {
			return std::nullopt;
		}
		return writing_target(core::writing_hook::outline, { .TextGeneration = draft->Revision, .EditorGeneration = draft->Revision });
	}

	void github_application::implementation::insert_tour_outline()
	{
		const auto target = current_outline_target();
		if (!OutlineProposal || !target || *target != OutlineProposal->Target || !Window->tour_editor()->writing_visible(core::writing_hook::outline)) {
			cancel_writing();
			Window->tour_editor()->show_writing_notice(QStringLiteral("The captured tour changed. Request a new outline before inserting stops."));
			return;
		}
		const auto         draft = TourHost->presentation().Draft;
		core::tour_command command{ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = core::tour_command_kind::insert_outline, .Position = draft->Stops.size() };
		for (const auto index : Window->tour_editor()->retained_outline_entries()) {
			if (index >= OutlineProposal->Entries.size()) {
				return;
			}
			command.Outline.push_back(OutlineProposal->Entries[index]);
		}
		if (command.Outline.empty()) {
			return;
		}
		OutlineProposal.reset();
		Window->tour_editor()->present_outline({ });
		TourHost->dispatch(command);
		Window->tour_editor()->show_writing_notice(QStringLiteral("Skeleton stops inserted. Undo arrangement removes this insertion. Visit each stop, write or explicitly generate its comment, then choose Ready and next."));
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
			Window->writing_hooks()->show_notice("Add an agent that speaks Claude Code's stream JSON in Settings › Agents & context for tool-free writing tests; wait for any running request.");
			return;
		}
		core::writing_request request;
		request.Target.Hook                              = aHook;
		request.Target.HookRevision                      = prefs->WritingHooks.Revision;
		request.Target.ContextDigest                     = writing_context_key();
		request.Raw                                      = "The cache reduces repeated work.";
		request.Selection.End                            = request.Raw.size();
		std::vector<application::writing_source> sources = { { "test-source", "The cache reuses previously computed results. No performance or test-success claim is supplied." } };
		const auto                               prompt  = application::build_writing_prompt(prefs->WritingHooks, request, sources);
		if (!prompt) {
			Window->writing_hooks()->show_notice("Hook instructions exceed the test context limit.");
			return;
		}
		PendingWriting = writing_operation{ request, std::move(sources), { }, true };
		auto skills    = skill_run_for(aHook);
		Window->writing_hooks()->show_notice(skills.Note ? "Testing saved prompt… Nothing will be inserted or published. " + *skills.Note : std::string{ "Testing saved prompt… Nothing will be inserted or published." });
#if defined(BPR_AGENT_INTEGRATIONS)
		const auto text = *prompt + skills.Directive;
		if (!dispatch_agent_invocation(*selected, { }, { }, false, text, { }, core::agent_entry_point::ask_about_selection, { }, true, tour_launch{ .Skills = std::move(skills) })) {
			PendingWriting.reset();
			Window->writing_hooks()->show_notice("An available agent and exact approved review context are required.");
		}
#else
		PendingWriting.reset();
#endif
	}

	void github_application::implementation::dispatch_tour_action(qt::tour_editor_action aAction)
	{
		if (Closing || !TourHost || !current(DraftWindow)) {
			return;
		}
		switch (aAction) {
		case qt::tour_editor_action::suggest_outline:
			if (const auto draft = TourHost->presentation().Draft; draft && !draft->ReadOnly) {
				request_writing(core::writing_hook::outline, { .TextGeneration = draft->Revision, .EditorGeneration = draft->Revision });
			}
			return;
		case qt::tour_editor_action::insert_outline:
			insert_tour_outline();
			return;
		case qt::tour_editor_action::preview_publication:
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
		case qt::tour_editor_action::map_problem:
		case qt::tour_editor_action::map_solution: {
			const auto draft = TourHost->presentation().Draft;
			if (!draft) {
				return;
			}
			const auto* view     = Window->overview_page()->body_view();
			const auto  span     = view->selected_source_bytes();
			const auto& snapshot = WindowRead->accepted_snapshot();
			if (!span || !snapshot || view->original_body() != snapshot->PullRequest.Body) {
				Window->tour_editor()->show_notice(QStringLiteral("Select text in the description's Source view first, or write your own explanation here."));
				return;
			}
			const auto& raw = view->original_body();
			TourHost->dispatch({ .Draft = draft->Id, .Owner = draft->Owner, .ExpectedRevision = draft->Revision, .Kind = aAction == qt::tour_editor_action::map_problem ? core::tour_command_kind::problem : core::tour_command_kind::solution, .Text = raw.substr(span->first, span->second - span->first), .Baseline = core::tour_field_baseline{ .Raw = raw, .RemoteEntity = std::string{ snapshot->PullRequest.Node.value() }, .SourceSpan = span } });
			return;
		}
		}
	}
}
