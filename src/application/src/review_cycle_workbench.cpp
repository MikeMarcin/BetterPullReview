#include "bpr/application/review_cycle_workbench.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>

namespace bpr::application {
	namespace {
		auto store_words(const cycle_store_error& aError) -> std::string
		{
			switch (aError.Failure) {
			case cycle_store_failure::quota:              return "The retained-material budget is full; nothing was evicted. Remove an old cycle to make room.";
			case cycle_store_failure::conflict:           return "The cycle changed while this was prepared. Try again.";
			case cycle_store_failure::object_unavailable: return "Retained material is missing; nothing was substituted.";
			case cycle_store_failure::corrupt:            return "Retained material is damaged; nothing was substituted.";
			case cycle_store_failure::refused:            return "The cycle refused this change.";
			case cycle_store_failure::cancelled:          return { };
			default:                                      return "The local cycle store is unavailable.";
			}
		}

		auto workspace_words(const workspace_failure aFailure) -> std::string
		{
			switch (aFailure) {
			case workspace_failure::missing:           return "The chosen folder or file no longer exists.";
			case workspace_failure::permission_denied: return "The chosen folder or file cannot be read.";
			case workspace_failure::unsupported:       return "The chosen item is not a folder or a regular file.";
			default:                                   return "The chosen folder or file could not be opened.";
			}
		}

		auto capture_words(const cycle_capture_outcome& aOutcome) -> std::string
		{
			switch (aOutcome.Status) {
			case cycle_capture_status::captured:         return { };
			case cycle_capture_status::unstable:         return "Files kept changing while they were read; nothing was recorded. Capture again when the folder is quiet.";
			case cycle_capture_status::root_changed:     return "The chosen folder was replaced while it was read; nothing was recorded.";
			case cycle_capture_status::too_many_entries: return "The folder holds more entries than one capture keeps; narrow the scope.";
			case cycle_capture_status::store:            return aOutcome.Store ? store_words(*aOutcome.Store) : "The local cycle store is unavailable.";
			case cycle_capture_status::workspace:        return aOutcome.Workspace ? workspace_words(*aOutcome.Workspace) : "The chosen folder could not be read.";
			case cycle_capture_status::not_bound:        return "The cycle has no folder to capture.";
			case cycle_capture_status::cancelled:        return { };
			}
			return { };
		}

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

		auto response_words(const cycle_response_result& aResult) -> std::string
		{
			if (aResult.Read) {
				return "The chosen file could not be read; nothing was added.";
			}
			if (aResult.Decode) {
				return "The file is not a response this build reads (" + std::string{ core::stable_name(aResult.Decode->Problem) } + "); nothing was added.";
			}
			if (!aResult.Refusal) {
				return { };
			}
			switch (aResult.Refusal->Problem) {
			case core::cycle_response_problem::wrong_cycle:       return "The response is for another cycle; nothing was added.";
			case core::cycle_response_problem::wrong_round:       return "The response is for another round; nothing was added.";
			case core::cycle_response_problem::wrong_digest:      return "The response names this round with other content than was frozen; nothing was added.";
			case core::cycle_response_problem::unknown_item:      return "The reply names an item this round does not hold; nothing was added.";
			case core::cycle_response_problem::stale_item:        return "The response answers another revision of an item; nothing was added.";
			case core::cycle_response_problem::empty:             return "A reply needs some text.";
			case core::cycle_response_problem::reused_identifier: return "A different response with the same identifier was already added; nothing was added.";
			}
			return { };
		}

		auto revision_name(const core::cycle_source_revision_id aRevision) -> std::string
		{
			// The name the surface and the round's wire form use.
			return "R" + std::to_string(aRevision.value());
		}

		/// An intelligible default title: the chosen folder's or file's name.
		auto default_title(const std::string& aLocation) -> std::string
		{
			auto path = std::filesystem::path{ aLocation };
			while (path.has_relative_path() && path.filename().empty()) {
				path = path.parent_path();
			}
			auto name = path.filename().string();
			if (name.empty()) {
				return "Local project";
			}
			return name.substr(0, core::maximum_cycle_title_bytes);
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
						self->fail(store_words(aResult.error()));
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
						aHost.fail(workspace_words(aResult.Failure.value_or(workspace_failure::io)));
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
							 // The same root object, wherever it now is, is the same project.
							 if (aState && aState->Workspace && aState->Workspace->Root.Root == root.Root) {
								 aSelf.select(aState->Subject);
								 aSelf.load(true);
								 return;
							 }
							 aSelf.match_bound(std::move(root), std::move(subjects), aIndex + 1U);
						 }));
		}

		void create_bound(core::workspace_root_observation aRoot)
		{
			const auto command = core::create_review_cycle{ .Authority = Catalog->Authority, .Title = default_title(View.Location), .Scope = { } };
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
											  aSelf.fail(capture_words(aOutcome));
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
			const auto words = response_words(aResult);
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

		void comment(core::begin_cycle_feedback aNote)
		{
			if (!Cycle) {
				return;
			}
			View.Activity = review_cycle_activity::saving;
			publish();
			Runtime.begin_feedback_selected(operation(), Cycle->Revision, std::move(aNote), guarded<cycle_write_receipt>([](implementation& aSelf, cycle_write_receipt aReceipt) {
												aSelf.Cycle->Revision = aReceipt.Revision;
												++aSelf.Cycle->Next.FeedbackItem;
												aSelf.View.Activity = review_cycle_activity::idle;
												aSelf.View.Message.clear();
												aSelf.load_notes();
											}));
		}

		void suggest(review_cycle_suggestion_request aRequest)
		{
			if (!Cycle) {
				return;
			}
			View.Activity = review_cycle_activity::saving;
			publish();
			auto note = core::begin_cycle_feedback{ .Primary = aRequest.Selection, .Body = aRequest.Rationale };
			Runtime.begin_feedback_selected(operation(), Cycle->Revision, std::move(note), guarded<cycle_write_receipt>([request = std::move(aRequest)](implementation& aSelf, cycle_write_receipt aReceipt) mutable {
												aSelf.Cycle->Revision = aReceipt.Revision;
												++aSelf.Cycle->Next.FeedbackItem;
												const auto item   = core::cycle_feedback_item_ref{ .Cycle = aSelf.Subject->Cycle, .Id = core::cycle_feedback_item_id{ aReceipt.Allocated } };
												auto       change = core::suggest_change{ .Selection = std::move(request.Selection), .Replacement = std::move(request.Replacement), .Intent = request.Intent };
												aSelf.Runtime.add_suggestion_selected(aSelf.operation(), item, 1, std::move(change), aSelf.guarded<cycle_write_receipt>([](implementation& aHost, cycle_write_receipt) {
													aHost.View.Activity = review_cycle_activity::idle;
													aHost.View.Message.clear();
													aHost.load_notes();
												}));
											}));
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
