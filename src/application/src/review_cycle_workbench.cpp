#include "bpr/application/review_cycle_workbench.hpp"

#include "bpr/application/review_cycle_words.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

namespace bpr::application {
	namespace {
		auto view_words(const core::cycle_view_problem aProblem) -> std::string
		{
			switch (aProblem) {
			case core::cycle_view_problem::uncaptured:           return "Nothing is captured yet.";
			case core::cycle_view_problem::unknown_revision:     return "That revision was never captured here; nothing else is shown in its place.";
			case core::cycle_view_problem::no_review_checkpoint: return "No review is marked yet. Mark reviewed to compare since it.";
			case core::cycle_view_problem::unknown_round:        return "That is not a frozen round of this cycle.";
			}
			return { };
		}

		/// What a route does, in the words a status line uses.
		auto route_words(const core::cycle_share_route aRoute) -> std::string
		{
			switch (aRoute) {
			case core::cycle_share_route::copy_document:  return "copying to the clipboard";
			case core::cycle_share_route::save_document:  return "saving to a file";
			case core::cycle_share_route::save_bundle:    return "saving a bundle";
			case core::cycle_share_route::external_agent: return "reading by an external agent";
			case core::cycle_share_route::send_to_agent:  return "sending to an agent";
			}
			return "sharing";
		}

		/// A refused route: nothing left, and no other route stood in for it.
		auto decision_words(const core::cycle_authorization aDecision, const core::cycle_share_route aRoute) -> std::string
		{
			switch (aDecision) {
			case core::cycle_authorization::allowed:              return { };
			case core::cycle_authorization::channel_denied:       return "Policy for this project's source does not allow " + route_words(aRoute) + ". Nothing was frozen or shared, and no other way was used instead.";
			case core::cycle_authorization::channel_unconfigured: return "Nothing configured allows " + route_words(aRoute) + " for source of unknown origin. Nothing was frozen or shared, and no other way was used instead.";
			default:                                              return "Sharing was refused (" + std::string{ core::name(aDecision) } + "). Nothing was frozen or shared.";
			}
		}

		auto limit_words(const core::cycle_round_output_limit& aLimit, const core::cycle_share_route aRoute) -> std::string
		{
			const auto what = aRoute == core::cycle_share_route::copy_document ? std::string{ "the clipboard's" } : aRoute == core::cycle_share_route::save_bundle ? std::string{ "a bundle's" }
																																								   : std::string{ "a saved document's" };
			auto       text = "The round is " + grouped_count(aLimit.RequiredBytes) + " bytes (" + grouped_count(aLimit.Items) + " items, " + grouped_count(aLimit.Sources) + " sources), over " + what + " limit of "
				+ grouped_count(aLimit.BudgetBytes) + " bytes. Nothing was cut short, frozen or shared.";
			return aRoute == core::cycle_share_route::copy_document ? text + " Save to File keeps all of it." : text;
		}

		auto refusal_words(const core::cycle_round_refusal& aRefusal) -> std::string
		{
			const auto item = aRefusal.Item ? " (item " + std::to_string(*aRefusal.Item + 1U) + ")" : std::string{ };
			switch (aRefusal.Problem) {
			case core::cycle_round_problem::empty:            return "Include at least one note to share.";
			case core::cycle_round_problem::archived_item:    return "A note in the round is archived" + item + "; leave it out to share.";
			case core::cycle_round_problem::stale_wording:
			case core::cycle_round_problem::stale_suggestion:
			case core::cycle_round_problem::stale_draft:      return "The round changed while it was shared; look at it again, then share.";
			case core::cycle_round_problem::oversized:        return "The round is over its size budget; split it into rounds you choose. Nothing was frozen or shared.";
			default:                                          return "This round cannot be frozen as it stands (" + std::string{ core::stable_name(aRefusal.Problem) } + "); nothing was shared.";
			}
		}

		/// The file's own name, as UTF-8; never its folder.
		auto file_name(const std::string& aPath) -> std::string
		{
			const auto name = std::filesystem::path{ std::u8string{ aPath.begin(), aPath.end() } }.filename().u8string();
			return { name.begin(), name.end() };
		}

		auto revision_name(const core::cycle_source_revision_id aRevision) -> std::string
		{
			// The name the surface and the round's wire form use.
			return "R" + std::to_string(aRevision.value());
		}

	}

	struct review_cycle_workbench::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(std::unique_ptr<review_cycle_store> aStore, workspace_source_port& aSource, effect_executor& aEffects, decision_executor& aDecisions,
					   review_cycle_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed)
			: Runtime{ std::move(aStore), aEffects, aDecisions }
			, Source{ aSource }
			, Port{ aPort }
			, Seed{ aSeed }
		{
		}

		review_cycle_runtime                      Runtime;
		workspace_source_port&                    Source;
		review_cycle_workbench_port&              Port;
		std::array<std::uint8_t, 32>              Seed;
		std::uint64_t                             Counter = 0;
		review_cycle_workbench_projection         View;
		std::optional<core::review_cycle_catalog> Catalog;
		std::optional<core::review_cycle_state>   Cycle;
		std::optional<core::local_cycle_subject>  Subject;
		core::cycle_tabs_state                    Tabs;
		/// Advances whenever another cycle is opened or the cycle is closed.
		std::uint64_t Generation = 0;
		std::uint64_t SourceLoad = 0;
		bool          Closing    = false;
		/// After a capture, new tabs take the new revision once the reload completes.
		bool                                AdvanceNewTabs = false;
		review_cycle_workbench::file_reader Reader;
		/// Recipient statements, kept to describe notes as they reload.
		cycle_response_entries Responses;
		/// What Share feedback may use (BPR-1161).
		core::cycle_disclosure_policy               Policy;
		core::cycle_share_capabilities              Capabilities;
		review_cycle_clipboard*                     Clipboard = nullptr;
		review_cycle_workbench::destination_factory Destinations;
		/// Compose edits waiting for the one in flight.
		std::deque<core::cycle_compose_edit> Edits;
		/// Each listed note's code for the share sheet, kept with the note
		/// version and suggestion count it was read for; one read at a time.
		struct note_code
		{
			core::cycle_feedback_item_id Item;
			std::uint64_t                Version     = 0;
			std::size_t                  Suggestions = 0;
			review_cycle_note_context    Context;
		};
		std::vector<note_code> NoteCode;
		/// The read in flight, by request number: its completion alone clears it.
		std::optional<std::uint64_t> ReadingCode;
		std::uint64_t                CodeRequests = 0;
		/// The note the larger view was asked for.
		std::optional<core::cycle_feedback_item_id> LargerItem;

		auto operation() -> cycle_operation_id
		{
			auto bytes = std::vector<std::uint8_t>(Seed.begin(), Seed.end());
			++Counter;
			for (auto shift = 56; shift >= 0; shift -= 8) {
				bytes.push_back(static_cast<std::uint8_t>(Counter >> static_cast<unsigned>(shift)));
			}
			return cycle_operation_id{ core::sha256(bytes) };
		}

		void publish()
		{
			Port.publish(View);
		}

		void fail(const std::string& aMessage)
		{
			View.Activity = review_cycle_activity::idle;
			View.Message  = aMessage;
			publish();
		}

		/// A completion wrapper that runs only while this host lives and the
		/// same cycle is still open.
		template <typename T, typename TThen>
		auto guarded(TThen aThen) -> review_cycle_runtime::completion<T>
		{
			return [weak = weak_from_this(), generation = Generation, then = std::move(aThen)](cycle_store_result<T> aResult) mutable {
				const auto self = weak.lock();
				if (!self || self->Closing || self->Generation != generation) {
					return;
				}
				if (!aResult) {
					if (aResult.error().Failure != cycle_store_failure::cancelled) {
						self->fail(cycle_store_words(aResult.error()));
					}
					return;
				}
				if constexpr (std::is_void_v<T>) {
					then(*self);
				} else {
					then(*self, std::move(*aResult));
				}
			};
		}

		void with_catalog(std::function<void(implementation&)> aThen)
		{
			if (Catalog) {
				aThen(*this);
				return;
			}
			Runtime.open(guarded<core::review_cycle_catalog>([then = std::move(aThen)](implementation& aSelf, core::review_cycle_catalog aCatalog) mutable {
				aSelf.Catalog = aCatalog;
				then(aSelf);
			}));
		}

		void refresh_recents()
		{
			with_catalog([](implementation& aSelf) {
				aSelf.Runtime.list(std::nullopt, aSelf.guarded<cycle_page>([](implementation& aHost, cycle_page aPage) {
					auto cycles = std::move(aPage.Cycles);
					std::ranges::sort(cycles, [](const auto& aLeft, const auto& aRight) { return aLeft.Subject.Cycle > aRight.Subject.Cycle; });
					if (cycles.size() > maximum_recents) {
						cycles.resize(maximum_recents);
					}
					aHost.collect_recents(std::move(cycles), 0U, { });
				}));
			});
		}

		void collect_recents(std::vector<core::review_cycle_projection> aCycles, const std::size_t aIndex, std::vector<review_cycle_recent> aFound)
		{
			if (aIndex >= aCycles.size()) {
				Port.publish(aFound);
				return;
			}
			const auto subject = aCycles[aIndex].Subject;
			Runtime.load(subject, guarded<std::optional<core::review_cycle_state>>([cycles = std::move(aCycles), aIndex, found = std::move(aFound)](implementation& aSelf, std::optional<core::review_cycle_state> aState) mutable {
							 if (aState && aState->Lifecycle == core::cycle_lifecycle::active) {
								 found.push_back({ .Cycle = cycles[aIndex], .Location = aState->Workspace ? aState->Workspace->Root.Location : std::string{ } });
							 }
							 aSelf.collect_recents(std::move(cycles), aIndex + 1U, std::move(found));
						 }));
		}

		void open_project(std::string aLocation)
		{
			++Generation;
			View          = { };
			View.Activity = review_cycle_activity::opening;
			View.Location = aLocation;
			publish();
			with_catalog([location = std::move(aLocation)](implementation& aSelf) {
				aSelf.Runtime.observe_workspace(aSelf.Source, location, aSelf.guarded<workspace_root_observation_result>([location](implementation& aHost, workspace_root_observation_result aResult) {
					if (!aResult.Root) {
						aHost.fail(cycle_workspace_words(aResult.Failure.value_or(workspace_failure::io)));
						return;
					}
					aHost.find_bound(*aResult.Root, std::nullopt, { });
				}));
			});
		}

		/// Scans the catalog for a cycle already bound to this root.
		void find_bound(core::workspace_root_observation aRoot, const std::optional<core::review_cycle_id> aAfter, std::vector<core::local_cycle_subject> aSubjects)
		{
			Runtime.list(aAfter, guarded<cycle_page>([root = std::move(aRoot), subjects = std::move(aSubjects)](implementation& aSelf, cycle_page aPage) mutable {
							 for (const auto& cycle : aPage.Cycles) {
								 if (cycle.WorkspaceKind == root.Kind && cycle.Lifecycle == core::cycle_lifecycle::active) {
									 subjects.push_back(cycle.Subject);
								 }
							 }
							 if (aPage.Next) {
								 aSelf.find_bound(std::move(root), aPage.Next, std::move(subjects));
								 return;
							 }
							 aSelf.match_bound(std::move(root), std::move(subjects), 0U);
						 }));
		}

		void match_bound(core::workspace_root_observation aRoot, std::vector<core::local_cycle_subject> aSubjects, const std::size_t aIndex)
		{
			if (aIndex >= aSubjects.size()) {
				create_bound(std::move(aRoot));
				return;
			}
			const auto subject = aSubjects[aIndex];
			Runtime.load(subject, guarded<std::optional<core::review_cycle_state>>([root = std::move(aRoot), subjects = std::move(aSubjects), aIndex](implementation& aSelf, std::optional<core::review_cycle_state> aState) mutable {
							 if (aState && core::bound_to(*aState, root)) {
								 aSelf.select(aState->Subject);
								 aSelf.load(true);
								 return;
							 }
							 aSelf.match_bound(std::move(root), std::move(subjects), aIndex + 1U);
						 }));
		}

		void create_bound(core::workspace_root_observation aRoot)
		{
			const auto command = core::create_review_cycle{ .Authority = Catalog->Authority, .Title = default_cycle_title(View.Location), .Scope = { } };
			Runtime.create(operation(), command, guarded<cycle_write_receipt>([root = std::move(aRoot)](implementation& aSelf, cycle_write_receipt aReceipt) mutable {
							   aSelf.select({ .Authority = aSelf.Catalog->Authority, .Cycle = aReceipt.Cycle, .Generation = 1 });
							   aSelf.Runtime.apply_selected(aSelf.operation(), aReceipt.Revision, core::bind_cycle_workspace{ .Root = std::move(root) }, aSelf.guarded<cycle_write_receipt>([](implementation& aHost, cycle_write_receipt) {
								   aHost.load(true);
							   }));
						   }));
		}

		void select(const core::local_cycle_subject& aSubject)
		{
			Subject = aSubject;
			Cycle.reset();
			Tabs = { };
			Runtime.select(aSubject);
		}

		void capture(const bool aDeliberate = false)
		{
			View.Activity = review_cycle_activity::capturing;
			View.Notice.clear();
			publish();
			auto request = cycle_capture_request{ .Operation = operation(), .ExpectedRevision = Cycle->Revision, .Deliberate = aDeliberate };
			Runtime.capture_workspace(Source, std::move(request), guarded<cycle_capture_outcome>([](implementation& aSelf, cycle_capture_outcome aOutcome) {
										  if (aOutcome.Status != cycle_capture_status::captured) {
											  aSelf.fail(cycle_capture_words(aOutcome));
											  return;
										  }
										  aSelf.View.Message.clear();
										  if (aOutcome.Receipt && !aOutcome.Receipt->Changed && aSelf.Cycle && aSelf.Cycle->Latest) {
											  // Identical bytes: no revision, and the reviewer is told so.
											  aSelf.View.Notice = "No change since " + revision_name(aSelf.Cycle->Latest->Id.Id) + "; nothing new was recorded.";
										  } else {
											  aSelf.View.Quality   = aOutcome.Diagnostics.Explanation;
											  aSelf.AdvanceNewTabs = true;
										  }
										  aSelf.load(false);
									  }));
		}

		/// Loads the open cycle: state, revisions, files, tabs, notes and the active source.
		void load(const bool aCaptureWhenEmpty)
		{
			View.Activity = review_cycle_activity::loading;
			publish();
			Runtime.load_selected(guarded<std::optional<core::review_cycle_state>>([aCaptureWhenEmpty](implementation& aSelf, std::optional<core::review_cycle_state> aState) {
				if (!aState) {
					aSelf.fail("The cycle no longer exists.");
					return;
				}
				aSelf.Cycle      = std::move(*aState);
				aSelf.View.Cycle = core::project(*aSelf.Cycle);
				if (aSelf.Cycle->Workspace) {
					aSelf.View.Location = aSelf.Cycle->Workspace->Root.Location;
				}
				if (!aSelf.Cycle->Latest) {
					if (aCaptureWhenEmpty && aSelf.Cycle->Workspace) {
						aSelf.capture();
						return;
					}
					aSelf.View.Activity = review_cycle_activity::idle;
					aSelf.publish();
					return;
				}
				aSelf.load_revisions(std::nullopt, { });
			}));
		}

		void load_revisions(const std::optional<core::cycle_source_revision_id> aAfter, std::vector<core::cycle_source_revision_id> aFound)
		{
			Runtime.revisions_selected(aAfter, guarded<cycle_revision_page>([found = std::move(aFound)](implementation& aSelf, cycle_revision_page aPage) mutable {
										   for (const auto& revision : aPage.Revisions) {
											   found.push_back(revision.Revision.Id.Id);
										   }
										   if (aPage.Next) {
											   aSelf.load_revisions(aPage.Next, std::move(found));
											   return;
										   }
										   aSelf.View.Revisions = std::move(found);
										   aSelf.load_files(std::nullopt, { });
									   }));
		}

		void load_files(const std::optional<std::string>& aAfter, std::vector<review_cycle_file_entry> aFound)
		{
			const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = Cycle->Latest->Id };
			Runtime.compare_selected(pair, aAfter, core::maximum_cycle_comparison_page, false, guarded<cycle_comparison_page_observation>([found = std::move(aFound)](implementation& aSelf, cycle_comparison_page_observation aPage) mutable {
										 if (!aPage.Page) {
											 aSelf.fail("The captured file list could not be read.");
											 return;
										 }
										 for (const auto& entry : aPage.Page->Entries) {
											 if (entry.Right.Kind == core::manifest_entry_kind::file) {
												 found.push_back({ .Path = entry.Path, .State = entry.Right.State, .Text = entry.Right.Text, .Size = entry.Right.Size });
											 }
										 }
										 const auto full = found.size() >= maximum_listed_files;
										 if (aPage.Page->Next && !full) {
											 aSelf.load_files(aPage.Page->Next, std::move(found));
											 return;
										 }
										 aSelf.View.FilesTruncated = full && aPage.Page->Next.has_value();
										 aSelf.View.Files          = std::move(found);
										 aSelf.load_facts();
									 }));
		}

		/// The reviewer's checkpoint and the rounds' bound revisions, then the history.
		void load_facts()
		{
			Runtime.view_facts_selected(guarded<core::cycle_view_facts>([](implementation& aSelf, core::cycle_view_facts aFacts) {
				aSelf.View.Facts = std::move(aFacts);
				aSelf.load_history(std::nullopt, { });
			}));
		}

		static constexpr std::size_t maximum_history_entries = 256U;

		void load_history(const std::optional<core::cycle_source_revision_id> aAfter, core::cycle_history aFound)
		{
			Runtime.history_selected(aAfter, maximum_cycle_history_page, guarded<cycle_history_page>([found = std::move(aFound)](implementation& aSelf, cycle_history_page aPage) mutable {
										 std::ranges::move(aPage.History.Entries, std::back_inserter(found.Entries));
										 found.Unlisted += aPage.History.Unlisted;
										 if (aPage.Next && found.Entries.size() < maximum_history_entries) {
											 aSelf.load_history(aPage.Next, std::move(found));
											 return;
										 }
										 aSelf.View.History = std::move(found);
										 aSelf.load_tabs();
									 }));
		}

		void load_tabs()
		{
			Runtime.load_tabs_selected(guarded<std::optional<core::cycle_tabs_record>>([](implementation& aSelf, std::optional<core::cycle_tabs_record> aRecord) {
				auto restored = false;
				if (aRecord) {
					restored = core::restore(aSelf.Tabs, *aSelf.Cycle, *aRecord).Reason == core::document_tab_reason::none;
				}
				if (!restored || !aSelf.Tabs.Revision || aSelf.AdvanceNewTabs) {
					// Open tabs keep their revisions; only new tabs take the new capture.
					aSelf.Tabs.Revision = aSelf.Cycle->Latest->Id.Id;
					if (aSelf.AdvanceNewTabs) {
						aSelf.AdvanceNewTabs = false;
						aSelf.save_tabs();
					}
				}
				// Full source first: the first readable file opens in one pane.
				if (aSelf.Tabs.Tabs.empty()) {
					const auto first = std::ranges::find_if(aSelf.View.Files, [](const review_cycle_file_entry& aFile) {
						return aFile.State == core::cycle_path_state::present && aFile.Text != core::material_text_kind::binary && aFile.Text != core::material_text_kind::unsupported_encoding;
					});
					if (first != aSelf.View.Files.end()) {
						static_cast<void>(core::open_path(aSelf.Tabs, first->Path, false));
					}
				}
				aSelf.publish_tabs();
				aSelf.load_notes();
				aSelf.load_source();
			}));
		}

		void publish_tabs()
		{
			View.Tabs           = Tabs.Tabs;
			View.ActiveTab      = Tabs.Active;
			View.NewerAvailable = false;
			const auto* tab     = core::active_tab(Tabs);
			if (tab == nullptr || !Cycle || !Cycle->Latest) {
				return;
			}
			if (tab->View.Comparison) {
				View.NewerAvailable = core::view_newer_available(*Cycle, View.Facts, tab->View.Comparison->Request, core::shown_pair(*Subject, tab->View));
			} else {
				View.NewerAvailable = tab->View.Revision != Cycle->Latest->Id.Id;
			}
		}

		void save_tabs()
		{
			Runtime.save_tabs_selected(core::persisted(Tabs), guarded<void>([](implementation&) { }));
		}

		void load_source()
		{
			const auto* tab = core::active_tab(Tabs);
			if (tab == nullptr) {
				View.Source.reset();
				View.Activity = review_cycle_activity::idle;
				publish();
				return;
			}
			const auto request = ++SourceLoad;
			const auto view    = *tab;
			if (view.View.Comparison) {
				load_comparison(request, view);
				return;
			}
			const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = { .Cycle = Subject->Cycle, .Id = view.View.Revision } };
			Runtime.compare_file_selected(pair, view.View.Path, { }, guarded<cycle_file_observation>([request, view](implementation& aSelf, cycle_file_observation aFile) {
											  // A newer tab gesture's load wins; this one is dropped.
											  if (request != aSelf.SourceLoad) {
												  return;
											  }
											  aSelf.View.Comparison.reset();
											  aSelf.View.Source = review_cycle_source_view{
												  .Tab      = view.Id,
												  .Revision = view.View.Revision,
												  .Path     = view.View.Path,
												  .Status   = aFile.Status,
												  .Bytes    = std::move(aFile.Source),
												  .Text     = aFile.Path ? aFile.Path->Right.Text : core::material_text_kind::empty,
												  .Line     = view.View.Line,
											  };
											  aSelf.View.Activity = review_cycle_activity::idle;
											  aSelf.observe_notes();
											  aSelf.publish();
										  }));
		}

		/// The active tab's comparison of its file through the shared engine.
		void load_comparison(const std::uint64_t aRequest, const core::cycle_tab aTab)
		{
			const auto pair = core::shown_pair(*Subject, aTab.View);
			Runtime.compare_file_selected(pair, aTab.View.Path, { }, guarded<cycle_file_observation>([aRequest, aTab, pair](implementation& aSelf, cycle_file_observation aFile) {
											  if (aRequest != aSelf.SourceLoad) {
												  return;
											  }
											  auto shown = review_cycle_comparison_view{
												  .Tab     = aTab.Id,
												  .Request = aTab.View.Comparison->Request,
												  .Pair    = pair,
												  .Path    = aTab.View.Path,
												  .Pinned  = core::pinned(aTab.View.Comparison->Request),
												  .Status  = aFile.Status,
												  .File    = std::move(aFile.Path),
												  .Reason  = aFile.Reason,
											  };
											  if (aFile.Diff) {
												  shown.Rows = core::diff_rows(*aFile.Diff);
											  }
											  aSelf.View.Source.reset();
											  aSelf.View.Comparison = std::move(shown);
											  aSelf.View.Activity   = review_cycle_activity::idle;
											  aSelf.publish();
										  }));
		}

		void load_notes()
		{
			collect_threads(std::nullopt, { });
		}

		void collect_threads(const std::optional<core::cycle_feedback_item_id> aAfter, std::vector<core::cycle_feedback_thread> aFound)
		{
			Runtime.feedback_selected(aAfter, guarded<cycle_feedback_page>([found = std::move(aFound)](implementation& aSelf, cycle_feedback_page aPage) mutable {
										  std::ranges::move(aPage.Threads, std::back_inserter(found));
										  if (aPage.Next) {
											  aSelf.collect_threads(aPage.Next, std::move(found));
											  return;
										  }
										  aSelf.describe_threads(std::move(found), 0U, { });
									  }));
		}

		void describe_threads(std::vector<core::cycle_feedback_thread> aThreads, const std::size_t aIndex, std::vector<review_cycle_note_entry> aNotes)
		{
			if (aIndex >= aThreads.size()) {
				View.Notes = std::move(aNotes);
				want_note_code();
				load_responses();
				return;
			}
			const auto item = aThreads[aIndex].Id;
			describe_revisions(std::move(aThreads), aIndex, std::move(aNotes), item, std::nullopt, { });
		}

		void describe_revisions(std::vector<core::cycle_feedback_thread> aThreads, const std::size_t aIndex, std::vector<review_cycle_note_entry> aNotes,
								const core::cycle_feedback_item_ref aItem, const std::optional<core::private_review_turn_revision_id> aAfter,
								std::vector<core::cycle_feedback_revision> aRevisions)
		{
			Runtime.feedback_revisions_selected(aItem, aAfter, guarded<cycle_feedback_revision_page>([threads = std::move(aThreads), aIndex, notes = std::move(aNotes), aItem, revisions = std::move(aRevisions)](implementation& aSelf, cycle_feedback_revision_page aPage) mutable {
													std::ranges::move(aPage.Revisions, std::back_inserter(revisions));
													if (aPage.Next) {
														aSelf.describe_revisions(std::move(threads), aIndex, std::move(notes), aItem, aPage.Next, std::move(revisions));
														return;
													}
													aSelf.Runtime.suggestions_selected(aItem, aSelf.guarded<std::vector<core::cycle_suggestion>>([pending = std::move(threads), aIndex, listed = std::move(notes), history = std::move(revisions)](implementation& aHost, std::vector<core::cycle_suggestion> aSuggestions) mutable {
														const auto& thread  = pending[aIndex];
														auto        wording = std::find_if(history.rbegin(), history.rend(), [](const core::cycle_feedback_revision& aRevision) {
															return aRevision.Identity.Position.Turn == core::private_review_turn_id{ 1 };
														});
														auto        turns   = std::vector<core::private_review_turn_id>{ };
														for (const auto& revision : history) {
															if (std::ranges::find(turns, revision.Identity.Position.Turn) == turns.end()) {
																turns.push_back(revision.Identity.Position.Turn);
															}
														}
														listed.push_back({
															.Item        = thread.Id,
															.Primary     = thread.Primary,
															.Related     = thread.Related,
															.Body        = wording != history.rend() ? wording->Body : std::string{ },
															.Provenance  = wording != history.rend() ? wording->Author.Provenance : core::authored_provenance::unknown,
															.Turns       = turns.size(),
															.Suggestions = aSuggestions.size(),
															.Lifecycle   = thread.Lineage.Lifecycle,
															.Version     = thread.Lineage.Version,
														});
														aHost.describe_threads(std::move(pending), aIndex + 1U, std::move(listed));
													}));
												}));
		}

		/// What recipients said, beside each note and in the list of replies.
		void load_responses()
		{
			Runtime.responses_selected(guarded<cycle_response_entries>([](implementation& aSelf, cycle_response_entries aEntries) {
				aSelf.Responses = std::move(aEntries);
				aSelf.describe_responses();
				aSelf.load_reconciliation();
			}));
		}

		/// Each note against the latest capture: correspondence, suggestion
		/// results, claims and the reviewer's decisions, kept apart.
		void load_reconciliation()
		{
			if (!Cycle || !Cycle->Latest) {
				observe_notes();
				publish();
				return;
			}
			Runtime.reconcile_selected(Cycle->Latest->Id.Id, guarded<std::vector<core::cycle_item_reconciliation>>([](implementation& aSelf, std::vector<core::cycle_item_reconciliation> aItems) {
										   for (auto& note : aSelf.View.Notes) {
											   const auto found    = std::ranges::find(aItems, note.Item.Id, &core::cycle_item_reconciliation::Item);
											   note.Reconciliation = found != aItems.end() ? std::optional{ *found } : std::nullopt;
										   }
										   aSelf.observe_notes();
										   aSelf.publish();
									   }));
		}

		void describe_responses()
		{
			for (auto& note : View.Notes) {
				note.Statements = core::item_statements(Responses, core::cycle_wire_name(note.Item.Id));
			}
			View.Responses.clear();
			for (const auto& [record, content] : Responses) {
				auto entry = review_cycle_response_entry{ .Id = record.Id, .Round = record.Round, .Channel = record.Channel, .ForeignId = record.ForeignId };
				if (const auto* document = std::get_if<core::cycle_response_document>(&content)) {
					entry.From          = document->ClaimedAuthor.DisplayName;
					entry.ClaimedAuthor = document->ClaimedAuthor;
					entry.Items         = document->Entries.size();
					entry.ClaimedResult = document->ResultRevision;
					if (Cycle) {
						entry.CapturedResult = core::resolve_claimed_result(*Cycle, document->ResultRevision);
					}
				} else {
					const auto& reply = std::get<core::cycle_manual_reply>(content);
					entry.From        = reply.From;
					entry.Items       = reply.Items.size();
					if (reply.Items.empty()) {
						entry.Body = reply.Body;
					}
				}
				View.Responses.push_back(std::move(entry));
			}
		}

		void response_done(const cycle_response_result& aResult)
		{
			View.Activity    = review_cycle_activity::idle;
			const auto words = cycle_response_words(aResult);
			if (!words.empty()) {
				fail(words);
				return;
			}
			View.Message.clear();
			View.Notice = aResult.Receipt && !aResult.Receipt->Changed ? "That response was already added; nothing changed." : std::string{ };
			load_responses();
		}

		/// Observes, for notes on the active tab's file from another revision,
		/// where their original attachment stands in the tab's revision.
		void observe_notes()
		{
			if (!View.Source) {
				return;
			}
			const auto revision = View.Source->Revision;
			const auto path     = View.Source->Path;
			for (std::size_t index = 0; index < View.Notes.size(); ++index) {
				auto&       note     = View.Notes[index];
				const auto  original = core::source_revision_of(note.Primary);
				const auto* file     = std::get_if<core::cycle_file_target>(&note.Primary);
				const auto* anchor   = std::get_if<core::cycle_source_anchor>(&note.Primary);
				const auto  target   = file != nullptr ? file->Path : anchor != nullptr ? anchor->Path
																						: std::string{ };
				if (!original || target != path || *original == revision || (note.Correspondence && note.Correspondence->Against == revision)) {
					continue;
				}
				const auto item = note.Item;
				Runtime.observe_feedback_selected(note.Primary, revision, guarded<cycle_correspondence_observation>([item, revision](implementation& aSelf, cycle_correspondence_observation aObserved) {
													  const auto found = std::ranges::find(aSelf.View.Notes, item, &review_cycle_note_entry::Item);
													  if (found != aSelf.View.Notes.end() && aSelf.View.Source && aSelf.View.Source->Revision == revision) {
														  found->Correspondence = aObserved.Correspondence;
														  aSelf.publish();
													  }
												  }));
			}
		}

		void tab_changed(const core::cycle_tabs_output aOutput)
		{
			if (!aOutput.Changed) {
				return;
			}
			publish_tabs();
			save_tabs();
			load_source();
		}

		/// Whether another write moved the cycle on since this host read it.
		static auto revision_moved(const cycle_store_error& aError) -> bool
		{
			return aError.Failure == cycle_store_failure::conflict || (aError.Failure == cycle_store_failure::refused && aError.Refusal == core::cycle_refusal::conflict);
		}

		/// Starts a note on the cycle as it stands. A note stands on its own
		/// target, so when another interface wrote the cycle since it was read
		/// (BPR-1168) the note is tried once more, under the same operation, on
		/// the cycle as it is now; the core checks it again. The reviewer's text
		/// is never dropped for a revision they could not see.
		void begin_note(core::begin_cycle_feedback aNote, std::function<void(implementation&, cycle_write_receipt)> aThen)
		{
			const auto identity = operation();
			Runtime.begin_feedback_selected(identity, Cycle->Revision, aNote, [weak = weak_from_this(), generation = Generation, identity, note = aNote, then = std::move(aThen)](cycle_store_result<cycle_write_receipt> aResult) mutable {
				const auto self = weak.lock();
				if (!self || self->Closing || self->Generation != generation) {
					return;
				}
				if (aResult) {
					then(*self, *aResult);
					return;
				}
				if (!revision_moved(aResult.error())) {
					if (aResult.error().Failure != cycle_store_failure::cancelled) {
						self->fail(cycle_store_words(aResult.error()));
					}
					return;
				}
				self->Runtime.load_selected(self->guarded<std::optional<core::review_cycle_state>>([identity, retried = std::move(note), afterwards = std::move(then)](implementation& aHost, std::optional<core::review_cycle_state> aState) mutable {
					if (!aState) {
						aHost.fail("The cycle no longer exists.");
						return;
					}
					aHost.Cycle      = std::move(*aState);
					aHost.View.Cycle = core::project(*aHost.Cycle);
					aHost.Runtime.begin_feedback_selected(identity, aHost.Cycle->Revision, std::move(retried), aHost.guarded<cycle_write_receipt>(std::move(afterwards)));
				}));
			});
		}

		void comment(core::begin_cycle_feedback aNote)
		{
			if (!Cycle) {
				return;
			}
			View.Activity = review_cycle_activity::saving;
			publish();
			begin_note(std::move(aNote), [](implementation& aSelf, cycle_write_receipt aReceipt) {
				aSelf.Cycle->Revision = aReceipt.Revision;
				++aSelf.Cycle->Next.FeedbackItem;
				aSelf.View.Activity = review_cycle_activity::idle;
				aSelf.View.Message.clear();
				aSelf.load_notes();
			});
		}

		void suggest(review_cycle_suggestion_request aRequest)
		{
			if (!Cycle || aRequest.Edits.empty()) {
				return;
			}
			View.Activity = review_cycle_activity::saving;
			publish();
			auto note = core::begin_cycle_feedback{ .Primary = aRequest.Edits.front().Selection, .Body = aRequest.Rationale };
			begin_note(std::move(note), [request = std::make_shared<review_cycle_suggestion_request>(std::move(aRequest))](implementation& aSelf, cycle_write_receipt aReceipt) {
				aSelf.Cycle->Revision = aReceipt.Revision;
				++aSelf.Cycle->Next.FeedbackItem;
				const auto item = core::cycle_feedback_item_ref{ .Cycle = aSelf.Subject->Cycle, .Id = core::cycle_feedback_item_id{ aReceipt.Allocated } };
				aSelf.add_suggestions(item, std::move(*request), 0U);
			});
		}

		/// Adds a note's suggestions one after another; each write moves the
		/// note to its next version.
		void add_suggestions(const core::cycle_feedback_item_ref& aItem, review_cycle_suggestion_request aRequest, const std::size_t aNext)
		{
			if (aNext >= aRequest.Edits.size()) {
				View.Activity = review_cycle_activity::idle;
				View.Message.clear();
				load_notes();
				return;
			}
			auto& edit   = aRequest.Edits[aNext];
			auto  change = core::suggest_change{ .Selection = std::move(edit.Selection), .Replacement = std::move(edit.Replacement), .Intent = aRequest.Intent };
			Runtime.add_suggestion_selected(operation(), aItem, 1U + aNext, std::move(change),
											guarded<cycle_write_receipt>([aItem, request = std::move(aRequest), aNext](implementation& aSelf, cycle_write_receipt) mutable {
												aSelf.add_suggestions(aItem, std::move(request), aNext + 1U);
											}));
		}

		/// Applies the oldest waiting edit to the revision shown now.
		void next_edit()
		{
			if (Edits.empty() || !View.Share) {
				Edits.clear();
				return;
			}
			View.Activity = review_cycle_activity::saving;
			View.Share->Replace.reset();
			publish();
			Runtime.edit_share_selected(operation(), View.Share->Draft.Id, View.Share->Draft.Revision, Edits.front(), [weak = weak_from_this(), generation = Generation](cycle_store_result<cycle_share_draft> aResult) {
				const auto self = weak.lock();
				if (!self || self->Closing || self->Generation != generation) {
					return;
				}
				auto applied = std::optional<core::cycle_compose_edit>{ };
				if (!self->Edits.empty()) {
					applied = std::move(self->Edits.front());
					self->Edits.pop_front();
				}
				if (!aResult) {
					self->Edits.clear();
					if (revision_moved(aResult.error()) && self->View.Share) {
						// Another interface changed the draft first (BPR-1168). Its
						// change stands; a goal typed here is kept to save again.
						const auto* goal = applied ? std::get_if<core::set_round_goal>(&*applied) : nullptr;
						if (goal != nullptr) {
							self->View.Share->UnsavedGoal = goal->Goal;
						}
						self->reload_share(goal != nullptr ? "The draft changed elsewhere first, so your goal was not saved. It is still in Goal: press Return to save it over the new revision."
														   : "The draft changed elsewhere first, so that change was not made. Here is the draft as it is now.");
						return;
					}
					if (aResult.error().Failure != cycle_store_failure::cancelled) {
						self->fail(cycle_store_words(aResult.error()));
					}
					return;
				}
				if (applied && std::holds_alternative<core::set_round_goal>(*applied) && self->View.Share) {
					self->View.Share->UnsavedGoal.reset();
				}
				if (self->View.Share) {
					self->present_draft(std::move(*aResult));
					self->View.Share->Status.clear();
					self->View.Share->Invocation.clear();
				}
				if (!self->Edits.empty()) {
					self->next_edit();
					return;
				}
				self->View.Activity = review_cycle_activity::idle;
				self->publish();
			});
		}

		/// The notes the share sheet lists: the round's, in its order, then
		/// open notes left out.
		[[nodiscard]] auto listed_notes() const -> std::vector<const review_cycle_note_entry*>
		{
			auto listed = std::vector<const review_cycle_note_entry*>{ };
			if (!View.Share) {
				return listed;
			}
			for (const auto& selected : View.Share->Selection.Items) {
				const auto found = std::ranges::find(View.Notes, selected.Item, [](const review_cycle_note_entry& aNote) { return aNote.Item.Id; });
				if (found != View.Notes.end()) {
					listed.push_back(&*found);
				}
			}
			for (const auto& note : View.Notes) {
				if (note.Lifecycle == core::private_thread_lifecycle::open && std::ranges::find(listed, &note) == listed.end()) {
					listed.push_back(&note);
				}
			}
			return listed;
		}

		[[nodiscard]] auto code_of(const review_cycle_note_entry& aNote) const -> const note_code*
		{
			const auto found = std::ranges::find_if(NoteCode, [&](const note_code& aCode) {
				return aCode.Item == aNote.Item.Id && aCode.Version == aNote.Version && aCode.Suggestions == aNote.Suggestions;
			});
			return found == NoteCode.end() ? nullptr : &*found;
		}

		/// The share sheet's code for each listed note read so far, in its order.
		void apply_note_code()
		{
			if (!View.Share) {
				return;
			}
			auto contexts = std::vector<review_cycle_note_context>{ };
			for (const auto* note : listed_notes()) {
				if (const auto* code = code_of(*note)) {
					contexts.push_back(code->Context);
				}
			}
			View.Share->Contexts = std::move(contexts);
		}

		/// Reads the next listed note's code that is missing or stale, one at
		/// a time so a share is never queued behind all of them.
		void want_note_code()
		{
			if (!View.Share || ReadingCode) {
				return;
			}
			apply_note_code();
			const auto listed = listed_notes();
			const auto next   = std::ranges::find_if(listed, [&](const review_cycle_note_entry* aNote) { return code_of(*aNote) == nullptr; });
			if (next == listed.end()) {
				return;
			}
			const auto note    = **next;
			const auto request = ++CodeRequests;
			ReadingCode        = request;
			const auto queued  = Runtime.note_context_selected(note.Item.Id, core::cycle_excerpt_context_lines, core::cycle_excerpt_rows,
															   [weak = weak_from_this(), generation = Generation, note, request](cycle_store_result<review_cycle_note_context> aResult) {
																  const auto self = weak.lock();
																  if (!self || self->ReadingCode != request) {
																	  return;
																  }
																  self->ReadingCode.reset();
																  if (self->Closing || self->Generation != generation || !aResult) {
																	  // A note gone since it was listed has no code to show; nothing else is said.
																	  return;
																  }
																  std::erase_if(self->NoteCode, [&](const note_code& aCode) { return aCode.Item == note.Item.Id; });
																  self->NoteCode.push_back({ .Item = note.Item.Id, .Version = note.Version, .Suggestions = note.Suggestions, .Context = std::move(*aResult) });
																  if (self->View.Share) {
																	  self->apply_note_code();
																	  self->publish();
																	  self->want_note_code();
																  }
															   });
			if (!queued) {
				ReadingCode.reset();
			}
		}

		/// Shows a draft on Compose feedback, keeping what was said about the last share.
		void present_draft(cycle_share_draft aDraft)
		{
			auto        share   = View.Share ? std::move(*View.Share) : review_cycle_share_view{ };
			const auto& preview = aDraft.Preview;
			share.Draft         = preview.Draft;
			share.Selection     = preview.Selection;
			share.Round         = preview.Round;
			share.Frozen        = preview.Frozen.has_value();
			share.Amends        = !share.Frozen && preview.Prior ? std::optional{ preview.Prior->Round.Id.Id } : std::nullopt;
			share.Refusal       = preview.Refusal;
			share.Digest.reset();
			share.Items = share.Suggestions = share.Sources = 0U;
			share.DocumentName.clear();
			share.BundleName.clear();
			if (preview.Content) {
				const auto& payload = preview.Content->Payload;
				share.Digest        = preview.Content->Digest;
				share.Items         = payload.Items.size();
				share.Sources       = payload.Sources.size();
				for (const auto& item : payload.Items) {
					share.Suggestions += item.Suggestions.size();
				}
				share.DocumentName = core::round_file_name(payload, core::cycle_round_file_kind::markdown);
				share.BundleName   = core::round_file_name(payload, core::cycle_round_file_kind::bundle);
			}
			share.DocumentShown = aDraft.Document.has_value();
			share.Document      = aDraft.Document ? std::move(*aDraft.Document) : std::string{ };
			share.DocumentBytes = aDraft.DocumentBytes;
			share.Routes        = core::share_routes(core::combine_policies(aDraft.Policy, Policy), Cycle ? core::cycle_origin(*Cycle) : core::cycle_workspace_origin{ }, Capabilities);
			View.Share          = std::move(share);
			want_note_code();
		}

		/// The draft again, then every observation and the rounds the
		/// surface lists; `aStatus` says what just happened.
		void reload_share(std::string aStatus)
		{
			Runtime.open_share_selected(operation(), operation(), guarded<cycle_share_draft>([status = std::move(aStatus)](implementation& aSelf, cycle_share_draft aDraft) mutable {
											if (aSelf.View.Share) {
												aSelf.present_draft(std::move(aDraft));
												aSelf.View.Share->Status = std::move(status);
											}
											aSelf.Runtime.deliveries_selected(aSelf.guarded<std::vector<core::cycle_delivery_record>>([](implementation& aHost, std::vector<core::cycle_delivery_record> aRecords) {
												if (aHost.View.Share) {
													aHost.View.Share->Deliveries = std::move(aRecords);
												}
												aHost.Runtime.view_facts_selected(aHost.guarded<core::cycle_view_facts>([](implementation& aLast, core::cycle_view_facts aFacts) {
													aLast.View.Facts    = std::move(aFacts);
													aLast.View.Activity = review_cycle_activity::idle;
													aLast.publish();
												}));
											}));
										}));
		}

		/// Why nothing left, in plain words; empty when the share produced a round.
		static auto unshared_words(const cycle_share_output& aOutput, const core::cycle_share_route aRoute) -> std::string
		{
			if (aOutput.Decision != core::cycle_authorization::allowed) {
				return decision_words(aOutput.Decision, aRoute);
			}
			if (aOutput.Limit) {
				return limit_words(*aOutput.Limit, aRoute);
			}
			if (aOutput.Refusal || !aOutput.Round) {
				return refusal_words(aOutput.Refusal.value_or(core::cycle_round_refusal{ }));
			}
			return { };
		}

		/// Copy to Clipboard or Use in external agent, once the round is frozen.
		void shared(const core::cycle_share_route aRoute, cycle_share_output aOutput)
		{
			if (const auto words = unshared_words(aOutput, aRoute); !words.empty()) {
				reload_share(words);
				return;
			}
			const auto& round = *aOutput.Round;
			const auto  name  = round.Content.Payload.RoundId;
			if (aRoute == core::cycle_share_route::external_agent) {
				// Nothing is sent or started: the round is exposed by a stored
				// grant, which the same channel decision allows, and the
				// reviewer's own harness reads it ([[BPR-1169]]).
				auto command = "bpr cycle round read " + round.Content.Payload.CycleId + " --round " + name;
				Runtime.grant_round_selected(operation(), round.Round.Round.Id.Id, false, Policy,
											 guarded<cycle_grant_result>([name, invocation = std::move(command)](implementation& aSelf, cycle_grant_result aGranted) mutable {
												 if (aGranted.Decision != core::cycle_authorization::allowed || !aGranted.Grant) {
													 aSelf.reload_share(decision_words(aGranted.Decision, core::cycle_share_route::external_agent));
													 return;
												 }
												 if (aSelf.View.Share) {
													 aSelf.View.Share->Invocation = std::move(invocation);
												 }
												 aSelf.reload_share(name + " is ready to be read by your own harness through the local interface: it may read this round and add a response to it, nothing else. Nothing was sent or started.");
											 }));
				return;
			}
			const auto outcome     = Clipboard != nullptr ? Clipboard->write(aOutput.Bytes) : review_cycle_clipboard::outcome::unavailable;
			const auto state       = outcome == review_cycle_clipboard::outcome::confirmed ? core::cycle_delivery_state::copied
				: outcome == review_cycle_clipboard::outcome::unconfirmed                  ? core::cycle_delivery_state::uncertain
																						   : core::cycle_delivery_state::failed;
			const auto words       = state == core::cycle_delivery_state::copied ? "Copied " + name + " to the clipboard (" + grouped_count(aOutput.Bytes.size()) + " bytes)."
				: state == core::cycle_delivery_state::uncertain                 ? "The clipboard did not confirm it holds " + name + "; paste to check before relying on it."
																				 : "There is no clipboard to copy " + name + " to; nothing was copied.";
			auto       observation = delivery_observation(round, state, core::cycle_disclosure_channel::clipboard, aOutput.Bytes, aOutput.Format);
			Runtime.record_delivery_selected(operation(), std::move(observation), guarded<cycle_write_receipt>([words](implementation& aSelf, cycle_write_receipt) { aSelf.reload_share(words); }));
		}

		/// Save to File or Save as Bundle: a conflict asks before replacing anything.
		void saved(const core::cycle_share_route aRoute, const std::string& aPath, cycle_file_share aResult)
		{
			if (const auto words = unshared_words(aResult.Output, aRoute); !words.empty()) {
				reload_share(words);
				return;
			}
			const auto name = file_name(aPath);
			if (aResult.Failure) {
				reload_share(cycle_export_words(*aResult.Failure));
				return;
			}
			if (aResult.Save && aResult.Save->Action == core::cycle_round_save_action::conflict && aResult.Save->Existing) {
				if (View.Share) {
					View.Share->Replace = review_cycle_replace_request{ .Route = aRoute, .Path = aPath, .Name = name, .Existing = aResult.Save->Existing->Digest, .Size = aResult.Save->Existing->Size };
				}
				reload_share(name + " already holds other content (" + grouped_count(aResult.Save->Existing->Size) + " bytes). Replace it, or keep it and choose another file.");
				return;
			}
			const auto round = aResult.Output.Round->Content.Payload.RoundId;
			const auto bytes = grouped_count(aResult.Output.Bytes.size());
			const auto same  = aResult.Save && aResult.Save->Action == core::cycle_round_save_action::already_saved;
			reload_share(same ? name + " already holds " + round + " exactly (" + bytes + " bytes); nothing was written." : "Saved " + round + " as " + name + " (" + bytes + " bytes).");
		}

		void share(const core::cycle_share_route aRoute, std::string aPath, const std::optional<core::cycle_material_digest>& aConfirmed)
		{
			if (!View.Share || !Cycle) {
				return;
			}
			auto request = cycle_share_request{
				.Freeze                = operation(),
				.Draft                 = View.Share->Draft.Id,
				.ExpectedDraftRevision = View.Share->Draft.Revision,
				.Route                 = aRoute,
				.Policy                = Policy,
			};
			View.Share->Replace.reset();
			View.Share->Invocation.clear();
			View.Activity = review_cycle_activity::saving;
			publish();
			switch (aRoute) {
			case core::cycle_share_route::copy_document:
			case core::cycle_share_route::external_agent:
				Runtime.prepare_share_selected(std::move(request), guarded<cycle_share_output>([aRoute](implementation& aSelf, cycle_share_output aOutput) { aSelf.shared(aRoute, std::move(aOutput)); }));
				return;
			case core::cycle_share_route::save_document:
			case core::cycle_share_route::save_bundle:   {
				auto destination = Destinations && !aPath.empty() ? std::shared_ptr<cycle_export_destination>{ Destinations(aPath) } : nullptr;
				if (!destination) {
					reload_share("Saving needs a file to save to; nothing was saved.");
					return;
				}
				Runtime.share_to_file_selected(std::move(request), std::move(destination), file_name(aPath), aConfirmed, operation(),
											   guarded<cycle_file_share>([aRoute, path = aPath](implementation& aSelf, cycle_file_share aResult) { aSelf.saved(aRoute, path, std::move(aResult)); }));
				return;
			}
			case core::cycle_share_route::send_to_agent:
				// No adapter ships in this build; the route is never offered by it.
				reload_share("No agent adapter is installed here; nothing was sent.");
				return;
			}
		}
	};

	review_cycle_workbench::review_cycle_workbench(std::unique_ptr<review_cycle_store> aStore, workspace_source_port& aSource, effect_executor& aEffects, decision_executor& aDecisions,
												   review_cycle_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed)
		: Implementation{ std::make_shared<implementation>(std::move(aStore), aSource, aEffects, aDecisions, aPort, aSeed) }
	{
	}

	review_cycle_workbench::~review_cycle_workbench()
	{
		shutdown();
	}

	void review_cycle_workbench::refresh_recents()
	{
		Implementation->refresh_recents();
	}

	void review_cycle_workbench::observe_external_change(const core::review_cycle_id aCycle)
	{
		auto& self = *Implementation;
		if (self.Closing || !self.Subject || self.Subject->Cycle != aCycle || !self.Cycle) {
			return;
		}
		self.load(false);
		if (self.View.Share) {
			self.reload_share(self.View.Share->Status);
		}
	}

	auto review_cycle_workbench::saving_draft() const noexcept -> bool
	{
		return !Implementation->Edits.empty();
	}

	void review_cycle_workbench::open_project(std::string aLocation)
	{
		Implementation->open_project(std::move(aLocation));
	}

	void review_cycle_workbench::resume(const core::local_cycle_subject aSubject)
	{
		auto& self = *Implementation;
		++self.Generation;
		self.View = { };
		self.select(aSubject);
		self.load(true);
	}

	void review_cycle_workbench::close()
	{
		auto& self = *Implementation;
		++self.Generation;
		self.Subject.reset();
		self.Cycle.reset();
		self.Tabs = { };
		self.View = { };
		self.Runtime.select(core::review_subject{ core::pull_request_authority{ } });
		self.publish();
	}

	void review_cycle_workbench::tab(const core::document_tab_intent& aIntent)
	{
		auto& self = *Implementation;
		if (!self.Subject) {
			return;
		}
		auto paths = std::vector<std::string>{ };
		paths.reserve(self.View.Files.size());
		for (const auto& file : self.View.Files) {
			paths.push_back(file.Path);
		}
		self.tab_changed(core::transition(self.Tabs, *self.Subject, aIntent, paths));
	}

	void review_cycle_workbench::open_file(const std::string& aPath, const bool aNewTab)
	{
		auto& self = *Implementation;
		if (self.Subject) {
			self.tab_changed(core::open_path(self.Tabs, aPath, aNewTab));
		}
	}

	void review_cycle_workbench::observe_line(const core::document_tab_id aTab, const std::uint64_t aLine)
	{
		auto& self = *Implementation;
		if (core::observe_line(self.Tabs, aTab, aLine).Changed) {
			self.publish_tabs();
			self.save_tabs();
		}
	}

	void review_cycle_workbench::comment(core::begin_cycle_feedback aNote)
	{
		Implementation->comment(std::move(aNote));
	}

	void review_cycle_workbench::suggest(review_cycle_suggestion_request aRequest)
	{
		Implementation->suggest(std::move(aRequest));
	}

	void review_cycle_workbench::relate(const core::cycle_feedback_item_ref& aItem, const std::uint64_t aVersion, core::cycle_related_reference aReference)
	{
		auto&      self = *Implementation;
		const auto item = aItem;
		if (!self.Cycle) {
			return;
		}
		self.View.Activity = review_cycle_activity::saving;
		self.publish();
		self.Runtime.change_feedback_selected(self.operation(), item, aVersion, core::add_feedback_reference{ std::move(aReference) },
											  self.guarded<cycle_write_receipt>([](implementation& aSelf, cycle_write_receipt) {
												  aSelf.View.Activity = review_cycle_activity::idle;
												  aSelf.View.Message.clear();
												  aSelf.load_notes();
											  }));
	}

	void review_cycle_workbench::capture_revision(const bool aDeliberate)
	{
		auto& self = *Implementation;
		if (!self.Cycle) {
			return;
		}
		if (!self.Cycle->Workspace) {
			self.fail("The cycle has no folder to capture.");
			return;
		}
		self.capture(aDeliberate);
	}

	void review_cycle_workbench::compare(const core::cycle_view_request& aRequest)
	{
		auto& self = *Implementation;
		if (!self.Cycle || core::active_tab(self.Tabs) == nullptr) {
			return;
		}
		const auto resolved = core::resolve_view(*self.Cycle, self.View.Facts, aRequest);
		if (!resolved) {
			self.fail(view_words(resolved.error()));
			return;
		}
		self.View.Message.clear();
		self.tab_changed(core::show_comparison(self.Tabs, aRequest, *resolved));
	}

	void review_cycle_workbench::full_source()
	{
		auto& self = *Implementation;
		if (self.Cycle) {
			self.tab_changed(core::show_full_source(self.Tabs));
		}
	}

	void review_cycle_workbench::refresh_view()
	{
		auto&       self = *Implementation;
		const auto* tab  = core::active_tab(self.Tabs);
		if (!self.Cycle || !self.Cycle->Latest || tab == nullptr) {
			return;
		}
		auto pair = std::optional<core::cycle_comparison_pair>{ };
		if (tab->View.Comparison) {
			if (core::pinned(tab->View.Comparison->Request)) {
				self.fail("Chosen revisions never move. Choose revisions again to see the newer capture.");
				return;
			}
			const auto resolved = core::resolve_view(*self.Cycle, self.View.Facts, tab->View.Comparison->Request);
			if (!resolved) {
				self.fail(view_words(resolved.error()));
				return;
			}
			pair = *resolved;
		}
		self.View.Message.clear();
		self.tab_changed(core::refresh_active(self.Tabs, pair, self.Cycle->Latest->Id.Id));
	}

	void review_cycle_workbench::mark_reviewed()
	{
		auto& self = *Implementation;
		if (!self.Cycle || !self.Cycle->Latest) {
			return;
		}
		const auto* tab      = core::active_tab(self.Tabs);
		const auto  revision = tab != nullptr ? tab->View.Revision : self.Cycle->Latest->Id.Id;
		self.View.Activity   = review_cycle_activity::saving;
		self.publish();
		self.Runtime.mark_reviewed_selected(self.operation(), revision, self.guarded<cycle_write_receipt>([revision](implementation& aSelf, cycle_write_receipt aReceipt) {
			aSelf.View.Facts.LastReview = core::cycle_review_checkpoint{ .Ordinal = aReceipt.Allocated, .Revision = revision };
			aSelf.View.Activity         = review_cycle_activity::idle;
			aSelf.View.Message.clear();
			aSelf.publish_tabs();
			aSelf.publish();
		}));
	}

	void review_cycle_workbench::set_file_reader(file_reader aReader)
	{
		Implementation->Reader = std::move(aReader);
	}

	void review_cycle_workbench::import_response(const core::cycle_round_id aRound, std::string aPath)
	{
		auto& self = *Implementation;
		if (!self.Cycle || !self.Reader) {
			return;
		}
		self.View.Activity = review_cycle_activity::saving;
		self.publish();
		auto read = [reader = self.Reader, path = std::move(aPath)](std::stop_token aStop) { return reader(path, core::maximum_response_document_bytes, std::move(aStop)); };
		self.Runtime.import_response_selected(self.operation(), aRound, std::move(read), self.guarded<cycle_response_result>([](implementation& aSelf, cycle_response_result aResult) { aSelf.response_done(aResult); }));
	}

	void review_cycle_workbench::attach_reply(const core::cycle_round_id aRound, core::cycle_manual_reply aReply)
	{
		auto& self = *Implementation;
		if (!self.Cycle) {
			return;
		}
		self.View.Activity = review_cycle_activity::saving;
		self.publish();
		self.Runtime.attach_reply_selected(self.operation(), aRound, std::move(aReply), self.guarded<cycle_response_result>([](implementation& aSelf, cycle_response_result aResult) { aSelf.response_done(aResult); }));
	}

	void review_cycle_workbench::decide(const core::cycle_feedback_item_ref& aItem, const std::optional<core::cycle_reviewer_decision> aDecision)
	{
		auto& self = *Implementation;
		if (!self.Cycle || !self.Cycle->Latest) {
			return;
		}
		const auto* tab     = core::active_tab(self.Tabs);
		const auto  checked = tab != nullptr ? tab->View.Revision : self.Cycle->Latest->Id.Id;
		self.View.Activity  = review_cycle_activity::saving;
		self.publish();
		self.Runtime.decide_selected(self.operation(), { .Item = aItem, .Decision = aDecision, .Checked = checked }, self.guarded<cycle_decision_result>([](implementation& aSelf, cycle_decision_result aResult) {
			if (aResult.Refusal) {
				aSelf.fail("That decision could not be recorded: the note or the revision is not what is shown.");
				return;
			}
			aSelf.View.Activity = review_cycle_activity::idle;
			aSelf.View.Message.clear();
			aSelf.load_reconciliation();
		}));
	}

	void review_cycle_workbench::prepare_next_round()
	{
		auto& self = *Implementation;
		if (!self.Cycle) {
			return;
		}
		self.View.Activity = review_cycle_activity::saving;
		self.publish();
		const auto begin = self.operation();
		const auto save  = self.operation();
		self.Runtime.prepare_next_round_selected(begin, save, self.guarded<cycle_next_round_result>([](implementation& aSelf, cycle_next_round_result aResult) {
			if (!aResult.Draft || !aResult.From) {
				aSelf.fail("No round has been frozen yet, so there is nothing to carry forward.");
				return;
			}
			aSelf.View.NextRound = review_cycle_next_round{
				.Draft    = *aResult.Draft,
				.From     = *aResult.From,
				.Carried  = aResult.Plan.Carried.size(),
				.Added    = aResult.Plan.Added.size(),
				.Resolved = aResult.Plan.Resolved.size(),
				.Archived = aResult.Plan.Archived.size(),
			};
			aSelf.View.Activity = review_cycle_activity::idle;
			aSelf.View.Message.clear();
			aSelf.publish();
		}));
	}

	void review_cycle_workbench::set_share_environment(core::cycle_disclosure_policy aPolicy, const core::cycle_share_capabilities aCapabilities)
	{
		Implementation->Policy       = std::move(aPolicy);
		Implementation->Capabilities = aCapabilities;
	}

	void review_cycle_workbench::set_clipboard(review_cycle_clipboard* aClipboard)
	{
		Implementation->Clipboard = aClipboard;
	}

	void review_cycle_workbench::set_destination_factory(destination_factory aFactory)
	{
		Implementation->Destinations = std::move(aFactory);
	}

	void review_cycle_workbench::compose()
	{
		auto& self = *Implementation;
		if (!self.Cycle || !self.Cycle->Latest) {
			return;
		}
		self.View.Activity = review_cycle_activity::loading;
		self.publish();
		self.Runtime.open_share_selected(self.operation(), self.operation(), self.guarded<cycle_share_draft>([](implementation& aSelf, cycle_share_draft aDraft) {
			aSelf.View.Share = review_cycle_share_view{ };
			aSelf.present_draft(std::move(aDraft));
			aSelf.Runtime.deliveries_selected(aSelf.guarded<std::vector<core::cycle_delivery_record>>([](implementation& aHost, std::vector<core::cycle_delivery_record> aRecords) {
				if (aHost.View.Share) {
					aHost.View.Share->Deliveries = std::move(aRecords);
				}
				aHost.View.Activity = review_cycle_activity::idle;
				aHost.publish();
			}));
		}));
	}

	void review_cycle_workbench::show_note_code(const core::cycle_feedback_item_id aItem)
	{
		auto& self = *Implementation;
		if (!self.View.Share || !self.Cycle) {
			return;
		}
		self.LargerItem = aItem;
		self.Runtime.note_context_selected(aItem, core::cycle_excerpt_larger_rows, core::cycle_excerpt_larger_rows,
										   self.guarded<review_cycle_note_context>([aItem](implementation& aSelf, review_cycle_note_context aContext) {
											   // Only the view still asked for, and only while the sheet is open.
											   if (aSelf.View.Share && aSelf.LargerItem == aItem) {
												   aSelf.View.Share->Larger = std::move(aContext);
												   aSelf.publish();
											   }
										   }));
	}

	void review_cycle_workbench::close_note_code()
	{
		auto& self = *Implementation;
		self.LargerItem.reset();
		if (self.View.Share && self.View.Share->Larger) {
			self.View.Share->Larger.reset();
			self.publish();
		}
	}

	void review_cycle_workbench::close_compose()
	{
		auto& self = *Implementation;
		self.Edits.clear();
		self.LargerItem.reset();
		self.NoteCode.clear();
		if (self.View.Share) {
			self.View.Share.reset();
			self.publish();
		}
	}

	void review_cycle_workbench::edit_composition(core::cycle_compose_edit aEdit)
	{
		auto& self = *Implementation;
		if (!self.View.Share || !self.Cycle) {
			return;
		}
		// Edits apply one after another, each to the revision the last one saved.
		self.Edits.push_back(std::move(aEdit));
		if (self.Edits.size() == 1U) {
			self.next_edit();
		}
	}

	void review_cycle_workbench::share(const core::cycle_share_route aRoute, std::string aPath)
	{
		Implementation->share(aRoute, std::move(aPath), std::nullopt);
	}

	void review_cycle_workbench::resolve_replace(const bool aReplace)
	{
		auto& self = *Implementation;
		if (!self.View.Share || !self.View.Share->Replace) {
			return;
		}
		const auto pending = *self.View.Share->Replace;
		self.View.Share->Replace.reset();
		if (!aReplace) {
			self.View.Share->Status = pending.Name + " was kept as it is; nothing was saved.";
			self.publish();
			return;
		}
		self.share(pending.Route, pending.Path, pending.Existing);
	}

	auto review_cycle_workbench::projection() const noexcept -> const review_cycle_workbench_projection&
	{
		return Implementation->View;
	}

	auto review_cycle_workbench::selected() const noexcept -> std::optional<core::local_cycle_subject>
	{
		return Implementation->Subject;
	}

	void review_cycle_workbench::shutdown()
	{
		Implementation->Closing = true;
		Implementation->Runtime.shutdown();
	}

	auto review_cycle_workbench::is_quiescent() const noexcept -> bool
	{
		return Implementation->Runtime.is_quiescent();
	}
}
