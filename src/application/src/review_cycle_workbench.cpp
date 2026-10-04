#include "bpr/application/review_cycle_workbench.hpp"

#include "bpr/application/review_cycle_words.hpp"
#include "bpr/application/setup_destination_projection.hpp"

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

		// Publish as Pull Request (BPR-1177): what each outcome says, in the
		// reviewer's words. None of them claims more than was observed.

		auto lower_hex(const std::array<std::uint8_t, 32>& aBytes) -> std::string
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto           text   = std::string{ };
			for (const auto byte : aBytes) {
				text += digits[byte >> 4U];
				text += digits[byte & 0x0FU];
			}
			return text;
		}

		auto path_list(const std::vector<std::string>& aPaths) -> std::string
		{
			auto text = std::string{ };
			for (std::size_t index = 0; index < aPaths.size() && index < 3U; ++index) {
				text += (index == 0U ? "" : ", ") + aPaths[index];
			}
			return aPaths.size() > 3U ? text + " and " + std::to_string(aPaths.size() - 3U) + " more" : text;
		}

		auto candidate_words(const core::candidate_refusal& aRefusal) -> std::string
		{
			const auto paths = path_list(aRefusal.Paths);
			switch (aRefusal.Limit) {
			case core::candidate_limit::no_change:        return "Nothing differs from the base commit, so there is nothing to publish.";
			case core::candidate_limit::undecided:        return "Choose whether to include " + paths + ": each improvement there depends on starting work you left out.";
			case core::candidate_limit::filter_managed:   return "A Git filter such as LFS manages " + paths + ", which BetterPullReview cannot build. Save a patch and commit it with your own Git tools.";
			case core::candidate_limit::gitlink:          return "A submodule changed (" + paths + "), which BetterPullReview cannot publish. Save a patch and commit it with your own Git tools.";
			case core::candidate_limit::path_conflict:    return "A file and a folder would share a path (" + paths + "). Leave one of them out.";
			case core::candidate_limit::signing_required: return "This repository requires signed commits, which BetterPullReview does not make. Save a patch and commit it with your own Git tools.";
			case core::candidate_limit::normalization:    return "Line endings can be changed only for selected text files with CRLF endings.";
			case core::candidate_limit::metadata:         return "Give the commit a one-line title and an author name and email.";
			}
			return "The commit cannot be built as chosen.";
		}

		auto outgoing_words(const cycle_outgoing_outcome& aOutcome, const std::string& aBase) -> std::string
		{
			switch (aOutcome.Status) {
			case cycle_outgoing_status::planned:               return { };
			case cycle_outgoing_status::not_bound:             return "This cycle has no project folder. Publishing needs a Git checkout.";
			case cycle_outgoing_status::not_captured:          return "Capture the project before publishing.";
			case cycle_outgoing_status::root_changed:          return "The project folder is not the one this cycle captured. Open it again to publish from it.";
			case cycle_outgoing_status::workspace:             return aOutcome.Workspace ? cycle_workspace_words(*aOutcome.Workspace) : "The project folder could not be read.";
			case cycle_outgoing_status::reference_unavailable: return "\"" + aBase + "\" does not name a commit in this checkout. Name a branch, HEAD or a commit.";
			case cycle_outgoing_status::repository:            return "Git could not read the repository" + (aOutcome.Git && !aOutcome.Git->Detail.empty() ? " (" + aOutcome.Git->Detail + ")." : std::string{ "." });
			case cycle_outgoing_status::material:              return "Retained material for this cycle is missing or damaged; nothing was substituted.";
			case cycle_outgoing_status::refused:
				if (aOutcome.Refusal == core::outgoing_refusal::repository_required) {
					return "This project is not a Git checkout. A pull request starts from a commit, so publish from a checkout of the repository.";
				}
				return "The outgoing content cannot be planned (" + std::string{ aOutcome.Refusal ? core::name(*aOutcome.Refusal) : "refused" } + ").";
			case cycle_outgoing_status::limited:   return "Nothing differs from the base commit, so there is nothing to publish.";
			case cycle_outgoing_status::cancelled: return { };
			}
			return { };
		}

		/// How many of the journal's effects GitHub confirmed.
		auto confirmed_steps(const core::publication_journal& aJournal) -> std::ptrdiff_t
		{
			return std::ranges::count_if(aJournal.Effects, [](const core::publication_effect& aEffect) { return aEffect.State == core::publication_effect_state::confirmed; });
		}

		/// What GitHub already holds of this plan, when it holds anything:
		/// confirmed steps stay there, and nothing claims they were undone.
		auto held_words(const core::publication_journal& aJournal) -> std::string
		{
			const auto confirmed = confirmed_steps(aJournal);
			return confirmed == 0 ? std::string{ } : " GitHub holds the " + std::to_string(confirmed) + (confirmed == 1 ? " step" : " steps") + " already confirmed; nothing is undone.";
		}

		/// Why nothing was attempted, and what was sent before: a limit met
		/// after a recovery says nothing more was sent, never that nothing was
		/// ([[BPR-1178]]).
		auto publish_limit_words(const cycle_publish_limit aLimit, const core::publication_plan& aPlan, const core::publication_journal& aJournal) -> std::string
		{
			const auto& destination = aPlan.Destination;
			const auto  repository  = destination.Owner + "/" + destination.Repository;
			const auto  nothing     = confirmed_steps(aJournal) == 0 ? std::string{ " Nothing was sent." } : " Nothing more was sent." + held_words(aJournal);
			switch (aLimit) {
			case cycle_publish_limit::unsupported_host:     return "Native publication is admitted for GitHub.com only. Save a patch, publish it with your own Git tools, then link the pull request." + nothing;
			case cycle_publish_limit::object_format:        return "GitHub takes SHA-1 repositories only; this one is not. Save a patch instead." + nothing;
			case cycle_publish_limit::no_push_permission:   return destination.Login + " cannot push to " + repository + "." + nothing;
			case cycle_publish_limit::archived:             return repository + " is archived." + nothing;
			case cycle_publish_limit::protected_path:       return "The change touches workflow files, which need a permission this build cannot check. Save a patch instead." + nothing;
			case cycle_publish_limit::parent_not_on_remote: return "The base commit is not on GitHub yet. Push it, or choose a base that is there." + nothing;
			case cycle_publish_limit::target_moved:         return destination.BaseBranch + " is missing on GitHub or moved since the plan was made. Review the plan again." + nothing;
			case cycle_publish_limit::branch_exists:        return "A branch named " + destination.NewBranch + " already exists on GitHub. Choose another name. Nothing was overwritten." + nothing;
			case cycle_publish_limit::blob_too_large:       return "A file is over the 128 KiB upload bound. Save a patch instead." + nothing;
			case cycle_publish_limit::material:             return "Retained material for this plan is missing or damaged." + nothing;
			case cycle_publish_limit::pull_request_exists:  return "A pull request for " + destination.NewBranch + " already exists. If it is yours, link it. Nothing more was sent." + held_words(aJournal);
			}
			return "Nothing was sent.";
		}

		auto stage_of(const core::publication_journal& aJournal) -> review_cycle_publication_stage
		{
			switch (aJournal.Phase) {
			// A journal read back between sends waits for the reviewer: after a
			// restart, a recovery or a step that never left, the remaining steps
			// need the plan confirmed again ([[BPR-1178]]). Publishing is only
			// ever the stage while a confirmed send is in flight.
			case core::publication_phase::prepared:
			case core::publication_phase::authorized:
			case core::publication_phase::uploading_objects:
			case core::publication_phase::branch_created:
			case core::publication_phase::pr_creation_pending: return review_cycle_publication_stage::planned;
			case core::publication_phase::linked:              return review_cycle_publication_stage::linked;
			case core::publication_phase::failed:
			case core::publication_phase::uncertain:
			case core::publication_phase::target_drift:
			case core::publication_phase::cancelled:
				return review_cycle_publication_stage::stopped;
			case core::publication_phase::superseded: return review_cycle_publication_stage::choosing;
			}
			return review_cycle_publication_stage::choosing;
		}

		/// What the journal says now, when it is not simply a plan waiting.
		auto journal_words(const core::publication_plan& aPlan, const core::publication_journal& aJournal) -> std::string
		{
			const auto held = held_words(aJournal);
			switch (aJournal.Phase) {
			case core::publication_phase::linked:
				if (aJournal.Link && aJournal.Link->Drifted) {
					return "Pull request #" + std::to_string(aJournal.Link->Number) + " is open, but its head or base is not what the plan named: someone pushed or the target moved. GitHub now shows what is shared.";
				}
				return aJournal.Link ? "Pull request #" + std::to_string(aJournal.Link->Number) + " is open on " + aPlan.Destination.Owner + "/" + aPlan.Destination.Repository + "." : std::string{ };
			case core::publication_phase::uncertain:
				return "GitHub did not answer whether a step took effect. Check GitHub before anything else is sent." + held;
			case core::publication_phase::target_drift:
				return "Something else took the branch name, or the target moved. Nothing was overwritten." + held;
			case core::publication_phase::failed:
				return "GitHub refused a step, so nothing more was sent." + held;
			case core::publication_phase::cancelled:
				return "Publishing was cancelled." + held;
			case core::publication_phase::prepared:
			case core::publication_phase::authorized:
			case core::publication_phase::uploading_objects:
			case core::publication_phase::branch_created:
			case core::publication_phase::pr_creation_pending:
				// Effects but no pull request yet: a stop between sends, read back.
				return aJournal.Effects.empty() ? std::string{ }
												: "Publishing stopped before the pull request. Publish Pull Request confirms this plan again and sends only what GitHub does not hold." + held;
			case core::publication_phase::superseded: return { };
			}
			return { };
		}

		auto link_words(const core::cycle_link_refusal aRefusal, const std::string& aHost) -> std::string
		{
			switch (aRefusal) {
			case core::cycle_link_refusal::address:        return "That is not a pull request address. Paste its address from your browser.";
			case core::cycle_link_refusal::host:           return "That pull request is not on " + aHost + ", the host you are signed in to.";
			case core::cycle_link_refusal::mismatch:       return "GitHub answered with something other than that pull request; nothing was linked.";
			case core::cycle_link_refusal::not_found:      return "No such pull request, or your account cannot read it; nothing was linked.";
			case core::cycle_link_refusal::already_linked: return "This cycle is already linked to that pull request.";
			case core::cycle_link_refusal::bounds:         return "This cycle has as many linked pull requests as it keeps.";
			}
			return "Nothing was linked.";
		}

		auto provider_words(const github_publication_error& aError) -> std::string
		{
			switch (aError.Failure) {
			case github_publication_failure::unsupported_host: return "Pull requests can be read here on GitHub.com only.";
			case github_publication_failure::rate_limited:     return "GitHub is rate limiting this account; try again later.";
			case github_publication_failure::refused:          return "GitHub refused the read.";
			default:                                           return "GitHub could not be reached.";
			}
		}

	}

	struct review_cycle_workbench::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(std::unique_ptr<review_cycle_store> aStore, workspace_source_port& aSource, effect_executor& aEffects, decision_executor& aDecisions,
					   review_cycle_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed, effect_executor* aCaptures)
			: Runtime{ std::move(aStore), aEffects, aDecisions, aCaptures }
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
		bool AdvanceNewTabs = false;
		/// [[BPR-1184]]. The drawer's window of files: where it starts, where
		/// the next one would, and where each earlier one started.
		struct file_window
		{
			std::optional<std::string> After;
			std::uint64_t              First = 1;
		};
		file_window                Window;
		std::optional<std::string> NextWindow;
		std::vector<file_window>   EarlierWindows;
		/// Open the window's first or last file once it is listed.
		std::optional<int> OpenEdge;
		/// The tabs changed while a capture ran; saved when it ends.
		bool TabsUnsaved = false;
		/// \brief [[BPR-1278]]. Advances with every save of this window's tabs:
		/// a tab record read before a later save is older than the tabs here.
		std::uint64_t                       TabSaves = 0;
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
		/// [[BPR-1274]]. Another interface wrote the cycle since the notes were
		/// read (a note was tried again on the cycle as it is now): the next
		/// change reads every note again, not only its own.
		bool NotesStale = false;
		/// [[BPR-1274]]. A full read of the notes waiting for its replies and
		/// reconciliation, shown with them in one change (`commit_notes`).
		std::optional<std::vector<review_cycle_note_entry>> PendingNotes;
		/// The note the larger view was asked for.
		std::optional<core::cycle_feedback_item_id> LargerItem;
		/// Publish as Pull Request (BPR-1177): what the composition allows,
		/// the local catalog made at first use on the effect lane, and the
		/// cycles whose journals were recovered after this run started.
		review_cycle_publication_environment            Publishing;
		std::shared_ptr<review_cycle_candidate_catalog> Candidates;
		std::vector<core::review_cycle_id>              Recovered;

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
			// [[BPR-1184]]. Reads finish while a capture runs; the page still says it is capturing.
			if (View.Capturing && View.Activity == review_cycle_activity::idle) {
				View.Activity = review_cycle_activity::capturing;
			}
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
			// A round frozen or a draft begun since the cycle was read moved its
			// revision; the capture names the revision the store holds now.
			Runtime.load_selected(guarded<std::optional<core::review_cycle_state>>([aDeliberate](implementation& aSelf, std::optional<core::review_cycle_state> aState) {
				if (!aState) {
					aSelf.fail("The cycle no longer exists.");
					return;
				}
				aSelf.Cycle = std::move(*aState);
				aSelf.capture_current(aDeliberate);
			}));
		}

		void capture_current(const bool aDeliberate)
		{
			auto request = cycle_capture_request{ .Operation = operation(), .ExpectedRevision = Cycle->Revision, .Deliberate = aDeliberate };
			// [[BPR-1184]]. Capturing from here until the capture's end, however it ends.
			View.Capturing = cycle_capture_progress{ };
			publish();
			// How far it has got, for the Updates line; the runtime delivers it on this lane.
			request.Progress = [weak = weak_from_this(), generation = Generation](const cycle_capture_progress& aProgress) {
				const auto self = weak.lock();
				if (self && !self->Closing && self->Generation == generation && self->View.Capturing) {
					self->View.Capturing = aProgress;
					self->publish();
				}
			};
			// Not `guarded`: a capture's end clears its progress however it ends.
			Runtime.capture_workspace(Source, std::move(request), [weak = weak_from_this(), generation = Generation](cycle_store_result<cycle_capture_outcome> aResult) {
				const auto self = weak.lock();
				if (!self || self->Closing || self->Generation != generation) {
					return;
				}
				self->View.Capturing.reset();
				// Before any reload reads the tab record back.
				if (std::exchange(self->TabsUnsaved, false)) {
					self->save_tabs();
				}
				if (!aResult) {
					if (aResult.error().Failure == cycle_store_failure::cancelled) {
						self->capture_cancelled();
					} else {
						self->fail(cycle_store_words(aResult.error()));
					}
					return;
				}
				self->captured(std::move(*aResult));
			});
		}

		/// Asked for: nothing recorded, the latest revision as it was.
		void capture_cancelled()
		{
			if (View.Activity == review_cycle_activity::capturing) {
				View.Activity = review_cycle_activity::idle;
			}
			View.Notice = "Capture cancelled; nothing was recorded.";
			publish();
		}

		void captured(const cycle_capture_outcome& aOutcome)
		{
			if (aOutcome.Status == cycle_capture_status::cancelled) {
				capture_cancelled();
				return;
			}
			if (aOutcome.Status != cycle_capture_status::captured) {
				fail(cycle_capture_words(aOutcome));
				return;
			}
			View.Message.clear();
			if (aOutcome.Receipt && !aOutcome.Receipt->Changed && Cycle && Cycle->Latest) {
				// Identical bytes: no revision, and the reviewer is told so.
				View.Notice = "No change since " + revision_name(Cycle->Latest->Id.Id) + "; nothing new was recorded.";
			} else {
				View.Quality   = aOutcome.Diagnostics.Explanation;
				AdvanceNewTabs = true;
			}
			load(false);
		}

		/// Loads the open cycle: state, revisions, files, tabs, notes and the active source.
		void load(const bool aCaptureWhenEmpty)
		{
			View.Activity = review_cycle_activity::loading;
			// A read of the notes still waiting is read again below.
			PendingNotes.reset();
			// A reload lists from the first file again: a capture may have moved every position.
			Window = { };
			NextWindow.reset();
			EarlierWindows.clear();
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
										   aSelf.load_files(aSelf.Window.After, { }, false);
									   }));
		}

		/// \brief Lists one window of the latest capture's files, from `aAfter`,
		/// a bounded page at a time; then the rest of the reload, or with
		/// `aListingOnly` just the drawer.
		void load_files(const std::optional<std::string>& aAfter, std::vector<review_cycle_file_entry> aFound, const bool aListingOnly)
		{
			const auto pair = core::cycle_comparison_pair{ .Left = std::nullopt, .Right = Cycle->Latest->Id };
			Runtime.compare_selected(pair, aAfter, core::maximum_cycle_comparison_page, false, guarded<cycle_comparison_page_observation>([found = std::move(aFound), aListingOnly](implementation& aSelf, cycle_comparison_page_observation aPage) mutable {
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
											 aSelf.load_files(aPage.Page->Next, std::move(found), aListingOnly);
											 return;
										 }
										 aSelf.View.FilesTruncated  = full && aPage.Page->Next.has_value();
										 aSelf.NextWindow           = aSelf.View.FilesTruncated ? aPage.Page->Next : std::nullopt;
										 aSelf.View.FilesFirst      = aSelf.Window.First;
										 aSelf.View.FilesEarlier    = !aSelf.EarlierWindows.empty();
										 aSelf.View.CapturedEntries = aSelf.Cycle->Latest->Capture.Entries;
										 aSelf.View.Files           = std::move(found);
										 if (!aListingOnly) {
											 aSelf.load_facts();
											 return;
										 }
										 aSelf.View.Activity = review_cycle_activity::idle;
										 aSelf.publish();
										 if (const auto edge = std::exchange(aSelf.OpenEdge, std::nullopt); edge && !aSelf.View.Files.empty()) {
											 const auto& file = *edge > 0 ? aSelf.View.Files.front() : aSelf.View.Files.back();
											 aSelf.tab_changed(core::open_path(aSelf.Tabs, file.Path, false));
										 }
									 }));
		}

		/// \brief [[BPR-1184]]. The drawer's next (`aDirection` > 0) or
		/// previous window of files; with `aOpenEdge`, then opens its first
		/// or last file, as Next file and Previous file do at a window's edge.
		void list_files(const int aDirection, const bool aOpenEdge)
		{
			// While a capture runs the drawer still pages: only another load waits.
			if (!Cycle || !Cycle->Latest || (View.Activity != review_cycle_activity::idle && View.Activity != review_cycle_activity::capturing)) {
				return;
			}
			if (aDirection > 0) {
				if (!NextWindow) {
					return;
				}
				EarlierWindows.push_back(Window);
				Window = { .After = NextWindow, .First = Window.First + View.Files.size() };
			} else {
				if (EarlierWindows.empty()) {
					return;
				}
				Window = EarlierWindows.back();
				EarlierWindows.pop_back();
			}
			NextWindow.reset();
			OpenEdge      = aOpenEdge ? std::optional{ aDirection } : std::nullopt;
			View.Activity = review_cycle_activity::loading;
			publish();
			load_files(Window.After, { }, true);
		}

		/// The reviewer's checkpoint and the rounds' bound revisions, then the history.
		void load_facts()
		{
			Runtime.view_facts_selected(guarded<core::cycle_view_facts>([](implementation& aSelf, core::cycle_view_facts aFacts) {
				aSelf.View.Facts = std::move(aFacts);
				aSelf.load_links();
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
			Runtime.load_tabs_selected(guarded<std::optional<core::cycle_tabs_record>>([saves = TabSaves](implementation& aSelf, std::optional<core::cycle_tabs_record> aRecord) {
				// [[BPR-1278]]. A tab gesture since the read (Next file pressed
				// while a capture's reload runs) saved after it: the record
				// read is the one before the gesture, and these tabs are newer.
				if (aSelf.TabSaves != saves) {
					aRecord = core::persisted(aSelf.Tabs);
				}
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
			++TabSaves;
			// [[BPR-1184]]. While a capture writes, the tab record waits for it:
			// saved now, it would wait on the capture's write lock in front of
			// the next file the reviewer reads.
			if (View.Capturing) {
				TabsUnsaved = true;
				return;
			}
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
			NotesStale = false;
			collect_notes(std::nullopt, { });
		}

		/// \brief [[BPR-1274]]. This window changed one note: that note is read
		/// again, or every note when another interface wrote meanwhile.
		void note_changed(const core::cycle_feedback_item_ref& aItem)
		{
			if (NotesStale) {
				load_notes();
				return;
			}
			refresh_note(aItem);
		}

		/// \brief Every note, a feedback page of them per call, each read whole
		/// in that call ([[BPR-1274]]: not three calls a note).
		void collect_notes(const std::optional<core::cycle_feedback_item_id> aAfter, std::vector<review_cycle_note_entry> aFound)
		{
			Runtime.note_records_selected(aAfter, guarded<cycle_note_record_page>([found = std::move(aFound)](implementation& aSelf, cycle_note_record_page aPage) mutable {
											  for (const auto& record : aPage.Notes) {
												  found.push_back(describe(record));
											  }
											  if (aPage.Next) {
												  aSelf.collect_notes(aPage.Next, std::move(found));
												  return;
											  }
											  // Shown with their replies and reconciliation, as one change.
											  aSelf.PendingNotes = std::move(found);
											  aSelf.load_responses();
										  }));
		}

		/// \brief One note as the list shows it: the latest wording of its first
		/// turn, how many turns it has and how many suggestions.
		static auto describe(const cycle_note_record& aRecord) -> review_cycle_note_entry
		{
			const auto& history = aRecord.Revisions;
			const auto  wording = std::find_if(history.rbegin(), history.rend(), [](const core::cycle_feedback_revision& aRevision) {
				return aRevision.Identity.Position.Turn == core::private_review_turn_id{ 1 };
			});
			auto        turns   = std::vector<core::private_review_turn_id>{ };
			for (const auto& revision : history) {
				if (std::ranges::find(turns, revision.Identity.Position.Turn) == turns.end()) {
					turns.push_back(revision.Identity.Position.Turn);
				}
			}
			const auto& thread = aRecord.Thread;
			return {
				.Item        = thread.Id,
				.Primary     = thread.Primary,
				.Related     = thread.Related,
				.Body        = wording != history.rend() ? wording->Body : std::string{ },
				.Provenance  = wording != history.rend() ? wording->Author.Provenance : core::authored_provenance::unknown,
				.Turns       = turns.size(),
				.Suggestions = aRecord.Suggestions.size(),
				.Lifecycle   = thread.Lineage.Lifecycle,
				.Version     = thread.Lineage.Version,
			};
		}

		/// \brief [[BPR-1274]]. After this window changed one note: that note
		/// read again, listed in item order with what recipients said of it,
		/// and reconciled alone. Every other note is as it was; a change made
		/// elsewhere reloads them all (`observe_external_change`).
		void refresh_note(const core::cycle_feedback_item_ref aItem)
		{
			Runtime.note_record_selected(aItem, guarded<std::optional<cycle_note_record>>([aItem](implementation& aSelf, std::optional<cycle_note_record> aRecord) {
											 auto&      notes = aSelf.notes();
											 const auto place = std::ranges::lower_bound(notes, aItem.Id, std::less{ }, [](const review_cycle_note_entry& aNote) { return aNote.Item.Id; });
											 const auto known = place != notes.end() && place->Item.Id == aItem.Id;
											 if (!aRecord) {
												 if (known) {
													 notes.erase(place);
												 }
												 aSelf.want_note_code();
												 aSelf.publish();
												 return;
											 }
											 auto entry       = describe(*aRecord);
											 entry.Statements = core::item_statements(aSelf.Responses, core::cycle_wire_name(entry.Item.Id));
											 if (known) {
												 *place = std::move(entry);
											 } else {
												 notes.insert(place, std::move(entry));
											 }
											 aSelf.want_note_code();
											 aSelf.reconcile_note(aItem.Id);
										 }));
		}

		/// \brief [[BPR-1274]]. One note against the latest capture, as
		/// `load_reconciliation` does for every note.
		void reconcile_note(const core::cycle_feedback_item_id aItem)
		{
			if (!Cycle || !Cycle->Latest) {
				observe_notes();
				publish();
				return;
			}
			Runtime.reconcile_selected(
				Cycle->Latest->Id.Id, guarded<std::vector<core::cycle_item_reconciliation>>([aItem](implementation& aSelf, std::vector<core::cycle_item_reconciliation> aItems) {
					const auto note = std::ranges::find(aSelf.notes(), aItem, [](const review_cycle_note_entry& aNote) { return aNote.Item.Id; });
					if (note != aSelf.notes().end()) {
						const auto found     = std::ranges::find(aItems, aItem, &core::cycle_item_reconciliation::Item);
						note->Reconciliation = found != aItems.end() ? std::optional{ *found } : std::nullopt;
					}
					aSelf.observe_notes();
					aSelf.publish();
				}),
				aItem);
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
				commit_notes();
				observe_notes();
				publish();
				return;
			}
			Runtime.reconcile_selected(Cycle->Latest->Id.Id, guarded<std::vector<core::cycle_item_reconciliation>>([](implementation& aSelf, std::vector<core::cycle_item_reconciliation> aItems) {
										   for (auto& note : aSelf.notes()) {
											   const auto found    = std::ranges::find(aItems, note.Item.Id, &core::cycle_item_reconciliation::Item);
											   note.Reconciliation = found != aItems.end() ? std::optional{ *found } : std::nullopt;
										   }
										   aSelf.commit_notes();
										   aSelf.observe_notes();
										   aSelf.publish();
									   }));
		}

		/// \brief [[BPR-1274]]. The notes a change made in this window updates:
		/// those a full read is assembling until it is shown, else those shown.
		auto notes() -> std::vector<review_cycle_note_entry>&
		{
			return PendingNotes ? *PendingNotes : View.Notes;
		}

		/// \brief Shows a full read of the notes, now that their replies and
		/// reconciliation are read with them: never a list of notes the page
		/// has not yet reconciled.
		void commit_notes()
		{
			if (PendingNotes) {
				View.Notes = std::move(*PendingNotes);
				PendingNotes.reset();
				want_note_code();
			}
		}

		void describe_responses()
		{
			for (auto& note : notes()) {
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
					aHost.NotesStale = true;
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
				aSelf.note_changed({ .Cycle = aSelf.Subject->Cycle, .Id = core::cycle_feedback_item_id{ aReceipt.Allocated } });
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
				note_changed(aItem);
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
											aSelf.show_share(std::move(aDraft), std::move(status));
										}));
		}

		/// \brief Shows a draft already read, then every observation and the
		/// rounds the surface lists. [[BPR-1274]]: a share hands back the
		/// draft as frozen, so the round is not read back after it.
		void show_share(cycle_share_draft aDraft, std::string aStatus)
		{
			if (View.Share) {
				present_draft(std::move(aDraft));
				View.Share->Status = std::move(aStatus);
			}
			Runtime.deliveries_selected(guarded<std::vector<core::cycle_delivery_record>>([](implementation& aHost, std::vector<core::cycle_delivery_record> aRecords) {
				if (aHost.View.Share) {
					aHost.View.Share->Deliveries = std::move(aRecords);
				}
				aHost.Runtime.view_facts_selected(aHost.guarded<core::cycle_view_facts>([](implementation& aLast, core::cycle_view_facts aFacts) {
					aLast.View.Facts    = std::move(aFacts);
					aLast.View.Activity = review_cycle_activity::idle;
					aLast.publish();
				}));
			}));
		}

		/// After a share: the draft it handed back, or read again without one.
		void after_share(std::optional<cycle_share_draft> aShown, std::string aStatus)
		{
			if (aShown) {
				show_share(std::move(*aShown), std::move(aStatus));
			} else {
				reload_share(std::move(aStatus));
			}
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
			Runtime.record_delivery_selected(operation(), std::move(observation), guarded<cycle_write_receipt>([words, shown = std::move(aOutput.Shown)](implementation& aSelf, cycle_write_receipt) mutable { aSelf.after_share(std::move(shown), words); }));
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
			after_share(std::move(aResult.Output.Shown), same ? name + " already holds " + round + " exactly (" + bytes + " bytes); nothing was written." : "Saved " + round + " as " + name + " (" + bytes + " bytes).");
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

		// Publish as Pull Request (BPR-1177).

		/// A completion for one publication step: like `guarded`, but a store
		/// refusal is said in the publication view, not over the page.
		template <typename TThen>
		auto publication_guarded(TThen aThen) -> review_cycle_runtime::completion<review_cycle_publication_step>
		{
			return [weak = weak_from_this(), generation = Generation, then = std::move(aThen)](cycle_store_result<review_cycle_publication_step> aResult) mutable {
				const auto self = weak.lock();
				if (!self || self->Closing || self->Generation != generation) {
					return;
				}
				if (!aResult) {
					if (aResult.error().Failure == cycle_store_failure::cancelled) {
						return;
					}
					if (self->View.Publication) {
						auto& shown = *self->View.Publication;
						shown.Busy  = false;
						if (shown.Stage == review_cycle_publication_stage::publishing && shown.Current) {
							// A refused send leaves the sheet where its journal is, not
							// stuck publishing ([[BPR-1178]]).
							shown.Stage = stage_of(shown.Current->Journal);
						}
						// Nothing was sent by this step; what earlier steps sent stays said.
						const auto sent = shown.Current && confirmed_steps(shown.Current->Journal) != 0 ? " Nothing more was sent." + held_words(shown.Current->Journal)
																										: std::string{ " Nothing was sent." };
						shown.Status    = aResult.error().Publication ? "This plan cannot go ahead (" + std::string{ core::name(*aResult.error().Publication) } + ")." + sent
																	  : cycle_store_words(aResult.error());
						self->publish();
					} else {
						self->fail(cycle_store_words(aResult.error()));
					}
					return;
				}
				self->View.Links = aResult->Links;
				then(*self, std::move(*aResult));
			};
		}

		auto candidates() -> std::shared_ptr<review_cycle_candidate_catalog>
		{
			if (!Candidates) {
				Candidates = std::make_shared<review_cycle_candidate_catalog>(Publishing.Candidates);
			}
			return Candidates;
		}

		[[nodiscard]] auto account() const -> std::optional<review_cycle_publication_account>
		{
			return Publishing.Account ? Publishing.Account() : std::nullopt;
		}

		static auto record_of(const stored_cycle_publication& aStored) -> review_cycle_publication_record
		{
			return { .Id = aStored.Id.Id, .Revision = aStored.Revision, .Plan = aStored.Plan, .Journal = aStored.Journal };
		}

		static auto outgoing_request(const review_cycle_publication_form& aForm) -> cycle_outgoing_request
		{
			return {
				.Base      = aForm.Base,
				.Target    = aForm.Target.empty() ? std::nullopt : std::optional{ aForm.Target },
				.Mode      = aForm.Mode,
				.Overrides = aForm.Overrides,
			};
		}

		/// The links and, the first time this run reads them, the journals
		/// recovered after a restart; then the page's history.
		void load_links()
		{
			const auto cycle   = Subject ? Subject->Cycle : core::review_cycle_id{ };
			const auto recover = std::ranges::find(Recovered, cycle) == Recovered.end();
			if (recover) {
				Recovered.push_back(cycle);
			}
			Runtime.publication_selected([recover](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) { return read_publications(aStore, aCycle, recover, std::move(aStop)); },
										 publication_guarded([](implementation& aSelf, review_cycle_publication_step) { aSelf.load_history(std::nullopt, { }); }));
		}

		/// Shows a recorded publication as the view's current one.
		void adopt(const stored_cycle_publication& aStored)
		{
			auto& view   = *View.Publication;
			view.Current = record_of(aStored);
			view.Stage   = stage_of(aStored.Journal);
			view.Digest  = lower_hex(aStored.Journal.PlanDigest.bytes());
			view.Preview = core::render_publication_preview(aStored.Plan);
			if (const auto words = journal_words(aStored.Plan, aStored.Journal); !words.empty()) {
				view.Status = words;
			}
		}

		/// Plans what the form would send: the baselines and the paths.
		void plan_outgoing()
		{
			if (!View.Publication) {
				return;
			}
			View.Publication->Busy = true;
			publish();
			auto request = outgoing_request(View.Publication->Form);
			Runtime.publication_selected(
				[catalog = candidates(), source = &Source, request](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return plan_publication(aStore, *source, *catalog, aCycle, request, std::move(aStop));
				},
				publication_guarded([form = View.Publication->Form](implementation& aSelf, review_cycle_publication_step aStep) {
					if (!aSelf.View.Publication || aSelf.View.Publication->Form.Base != form.Base || aSelf.View.Publication->Form.Target != form.Target
						|| aSelf.View.Publication->Form.Mode != form.Mode || aSelf.View.Publication->Form.Overrides != form.Overrides) {
						return;
					}
					auto& shown = *aSelf.View.Publication;
					shown.Busy  = false;
					shown.Outgoing.reset();
					shown.OutgoingProblem.clear();
					if (aStep.Catalog) {
						shown.OutgoingProblem = "Installed Git is not available, so the outgoing commit cannot be planned" + (aStep.Catalog->Detail.empty() ? std::string{ "." } : " (" + aStep.Catalog->Detail + ").");
					} else if (aStep.Planned) {
						shown.Outgoing        = aStep.Planned->Plan;
						shown.OutgoingProblem = outgoing_words(*aStep.Planned, form.Base);
					}
					aSelf.publish();
				}));
		}

		void open_publication()
		{
			if (!Cycle || !Cycle->Latest) {
				return;
			}
			auto       view     = review_cycle_publication_view{ };
			const auto latest   = Cycle->Latest->Id.Id;
			view.Form.Title     = Cycle->Title;
			view.Form.NewBranch = core::suggested_publication_branch(Cycle->Subject.Cycle, latest);
			view.PatchName      = "cycle-" + std::to_string(Cycle->Subject.Cycle.value()) + "-" + revision_name(latest) + ".patch";
			if (const auto acting = account()) {
				view.Account          = review_cycle_publication_account_view{ .Host = acting->Host, .Login = acting->Login, .Account = acting->Account };
				view.Form.AuthorName  = acting->Login;
				view.Form.AuthorEmail = acting->Login + "@users.noreply." + acting->Host;
			} else {
				view.Status = "Sign in to GitHub to publish from here. You can still save a patch for your own Git tools.";
			}
			View.Publication       = std::move(view);
			View.Publication->Busy = true;
			publish();
			Runtime.publication_selected([](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) { return read_publications(aStore, aCycle, false, std::move(aStop)); },
										 publication_guarded([](implementation& aSelf, review_cycle_publication_step aStep) {
											 if (!aSelf.View.Publication) {
												 return;
											 }
											 auto& form = aSelf.View.Publication->Form;
											 // The newest publication still standing is the one shown; a
											 // superseded or cancelled one only lends its destination.
											 for (auto stored = aStep.Publications.rbegin(); stored != aStep.Publications.rend(); ++stored) {
												 const auto& destination = stored->Plan.Destination;
												 if (form.Owner.empty()) {
													 form.Owner      = destination.Owner;
													 form.Repository = destination.Repository;
													 form.BaseBranch = destination.BaseBranch;
												 }
												 const auto phase = stored->Journal.Phase;
												 if (phase != core::publication_phase::superseded && phase != core::publication_phase::cancelled) {
													 form.NewBranch = destination.NewBranch;
													 form.Title     = stored->Plan.PullTitle;
													 form.Draft     = stored->Plan.Draft;
													 aSelf.adopt(*stored);
													 break;
												 }
											 }
											 aSelf.plan_outgoing();
										 }));
		}

		void edit_publication(review_cycle_publication_form aForm)
		{
			if (!View.Publication || View.Publication->Stage == review_cycle_publication_stage::publishing) {
				return;
			}
			auto&      view    = *View.Publication;
			const auto replan  = aForm.Base != view.Form.Base || aForm.Target != view.Form.Target || aForm.Mode != view.Form.Mode || aForm.Overrides != view.Form.Overrides;
			const auto changed = aForm != view.Form;
			view.Form          = std::move(aForm);
			if (changed && view.Stage == review_cycle_publication_stage::planned) {
				// Any edit is another plan: the old digest authorizes nothing.
				view.Current.reset();
				view.Digest.clear();
				view.Preview.clear();
				view.Stage  = review_cycle_publication_stage::choosing;
				view.Status = "Changed. Review the plan again before publishing.";
			}
			if (replan) {
				plan_outgoing();
			} else {
				publish();
			}
		}

		void prepare_publication()
		{
			if (!View.Publication || !Cycle || View.Publication->Busy) {
				return;
			}
			auto&      view   = *View.Publication;
			const auto acting = account();
			if (!acting) {
				view.Status = "Sign in to GitHub to publish from here. You can still save a patch for your own Git tools.";
				publish();
				return;
			}
			const auto& form = view.Form;
			auto        body = std::optional<std::string>{ std::string{ } };
			if (!form.Problem.empty() || !form.Solution.empty()) {
				body = core::publication_body(form.Problem, form.Solution);
			}
			if (!body) {
				view.Status = "The description is longer than a pull request takes; shorten it.";
				publish();
				return;
			}
			auto now        = Publishing.Now ? Publishing.Now() : core::git_signature{ };
			now.Name        = form.AuthorName;
			now.Email       = form.AuthorEmail;
			auto commitBody = form.Problem;
			if (!form.Solution.empty()) {
				commitBody += (commitBody.empty() ? "" : "\n\n") + form.Solution;
			}
			auto request = cycle_publication_request{
				.Candidate   = { .Outgoing  = outgoing_request(form),
								 .Candidate = { .NormalizeLineEndings = form.NormalizeLineEndings, .Metadata = { .Author = now, .Committer = now, .Title = form.Title, .Body = commitBody } } },
				.Destination = { .Host = acting->Host, .Account = acting->Account, .Login = acting->Login, .Owner = form.Owner, .Repository = form.Repository, .BaseBranch = form.BaseBranch, .NewBranch = form.NewBranch },
				.PullTitle   = form.Title,
				.PullBody    = std::move(*body),
				.Draft       = form.Draft,
			};
			if (!core::valid(request.Destination)) {
				view.Status = "Name the repository as owner and name, an existing base branch and a new branch that differs from it.";
				publish();
				return;
			}
			view.Busy = true;
			view.Status.clear();
			view.Limit.reset();
			publish();
			Runtime.publication_selected(
				[catalog = candidates(), source = &Source, op = operation(), prepared = std::move(request)](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return application::prepare_publication(aStore, *source, *catalog, aCycle, op, prepared, std::move(aStop));
				},
				publication_guarded([base = form.Base](implementation& aSelf, review_cycle_publication_step aStep) {
					if (!aSelf.View.Publication) {
						return;
					}
					auto& shown = *aSelf.View.Publication;
					shown.Busy  = false;
					if (aStep.Catalog) {
						shown.Status = "Installed Git is not available, so the commit cannot be built.";
					} else if (aStep.Prepared && aStep.Prepared->Publication) {
						shown.Outgoing = aStep.Prepared->Built.Outcome.Plan;
						aSelf.adopt(*aStep.Prepared->Publication);
						shown.Status = "Nothing has been sent. Read the plan, then publish exactly it.";
					} else if (aStep.Prepared) {
						const auto& built = aStep.Prepared->Built;
						shown.Limit       = built.Limit;
						shown.Status      = built.Limit ? candidate_words(*built.Limit)
							: aStep.Prepared->Refusal   ? "The plan is not valid (" + std::string{ core::name(*aStep.Prepared->Refusal) } + ")."
							: aStep.Prepared->Store     ? cycle_store_words(*aStep.Prepared->Store)
														: outgoing_words(built.Outcome, base);
					}
					aSelf.publish();
				}));
		}

		void publish_publication()
		{
			if (!View.Publication || View.Publication->Busy || View.Publication->Stage != review_cycle_publication_stage::planned || !View.Publication->Current) {
				return;
			}
			auto&      view   = *View.Publication;
			const auto acting = account();
			if (!acting || acting->Account != view.Current->Plan.Destination.Account) {
				view.Status = "The account signed in now is not the one this plan names. Review the plan again.";
				publish();
				return;
			}
			const auto request = review_cycle_publish_request{
				.Publication      = view.Current->Id,
				.ExpectedRevision = view.Current->Revision,
				.Digest           = view.Current->Journal.PlanDigest,
				.Operation        = operation(),
			};
			view.Busy   = true;
			view.Stage  = review_cycle_publication_stage::publishing;
			view.Status = "Publishing: sending the objects, then the branch, then the pull request.";
			publish();
			Runtime.publication_selected(
				[catalog = candidates(), source = &Source, actor = *acting, request](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return application::publish_publication(aStore, *source, *catalog, aCycle, actor, request, std::move(aStop));
				},
				publication_guarded([](implementation& aSelf, review_cycle_publication_step aStep) { aSelf.published(std::move(aStep)); }));
		}

		/// What a publish or recovery observed.
		void published(review_cycle_publication_step aStep)
		{
			if (!View.Publication) {
				return;
			}
			auto& view = *View.Publication;
			view.Busy  = false;
			if (aStep.Catalog) {
				view.Stage  = view.Current ? stage_of(view.Current->Journal) : review_cycle_publication_stage::choosing;
				view.Status = "Installed Git is not available. Nothing was sent.";
				publish();
				return;
			}
			if (!aStep.Published) {
				publish();
				return;
			}
			const auto& outcome = *aStep.Published;
			if (outcome.Publication) {
				adopt(*outcome.Publication);
			}
			if (outcome.Status == cycle_publish_status::limited && outcome.Limit && view.Current) {
				// Nothing was attempted: the plan still waits for the reviewer.
				view.Stage  = review_cycle_publication_stage::planned;
				view.Status = publish_limit_words(*outcome.Limit, view.Current->Plan, view.Current->Journal);
			} else if (outcome.Status == cycle_publish_status::provider && outcome.Provider) {
				view.Status = provider_words(*outcome.Provider) + " Nothing more was sent.";
			} else if (outcome.Status == cycle_publish_status::store && outcome.Store) {
				view.Status = cycle_store_words(*outcome.Store);
			} else if (outcome.Status == cycle_publish_status::unauthorized) {
				view.Stage  = view.Current && view.Current->Journal.Phase == core::publication_phase::linked ? review_cycle_publication_stage::linked : review_cycle_publication_stage::planned;
				view.Status = "This plan has no current confirmation. Review it again to publish.";
			}
			publish();
		}

		void recover_publication()
		{
			if (!View.Publication || View.Publication->Busy || !View.Publication->Current) {
				return;
			}
			auto&      view   = *View.Publication;
			const auto acting = account();
			if (!acting) {
				view.Status = "Sign in to GitHub to check what it holds.";
				publish();
				return;
			}
			view.Busy   = true;
			view.Status = "Reading GitHub; nothing is sent.";
			publish();
			Runtime.publication_selected(
				[catalog = candidates(), source = &Source, actor = *acting, publication = view.Current->Id](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return application::recover_publication(aStore, *source, *catalog, aCycle, actor, publication, std::move(aStop));
				},
				publication_guarded([](implementation& aSelf, review_cycle_publication_step aStep) { aSelf.published(std::move(aStep)); }));
		}

		void cancel_publication()
		{
			if (!View.Publication || View.Publication->Busy || !View.Publication->Current) {
				return;
			}
			auto& view = *View.Publication;
			view.Busy  = true;
			publish();
			Runtime.publication_selected(
				[op = operation(), publication = view.Current->Id, revision = view.Current->Revision](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return application::cancel_publication(aStore, aCycle, op, publication, revision, std::move(aStop));
				},
				publication_guarded([](implementation& aSelf, review_cycle_publication_step aStep) {
					if (!aSelf.View.Publication || !aSelf.View.Publication->Current) {
						return;
					}
					auto& shown = *aSelf.View.Publication;
					shown.Busy  = false;
					const auto found
						= std::ranges::find_if(aStep.Publications, [&](const stored_cycle_publication& aStored) { return aStored.Id.Id == shown.Current->Id; });
					if (found != aStep.Publications.end()) {
						aSelf.adopt(*found);
					}
					aSelf.publish();
				}));
		}

		void save_patch(std::string aPath)
		{
			if (!View.Publication || View.Publication->Busy) {
				return;
			}
			auto& view        = *View.Publication;
			auto  destination = Destinations && !aPath.empty() ? std::shared_ptr<cycle_export_destination>{ Destinations(aPath) } : nullptr;
			if (!destination) {
				view.Status = "Saving needs a file to save to; nothing was saved.";
				publish();
				return;
			}
			view.Busy = true;
			publish();
			Runtime.publication_selected(
				[catalog = candidates(), source = &Source, request = outgoing_request(view.Form), destination, name = file_name(aPath)](review_cycle_store& aStore, const core::review_cycle_state& aCycle,
																																		std::stop_token aStop) {
					return application::save_publication_patch(aStore, *source, *catalog, aCycle, request, *destination, name, std::move(aStop));
				},
				publication_guarded([name = file_name(aPath), base = view.Form.Base](implementation& aSelf, review_cycle_publication_step aStep) {
					if (!aSelf.View.Publication) {
						return;
					}
					auto& shown = *aSelf.View.Publication;
					shown.Busy  = false;
					if (aStep.Catalog) {
						shown.Status = "Installed Git is not available, so no patch was made.";
					} else if (aStep.External && aStep.External->Preparation && aStep.PatchSave && aStep.PatchSave->Action != core::cycle_round_save_action::conflict) {
						shown.Instructions = aStep.External->Preparation->Instructions;
						shown.Status       = name + " is saved. It is not published: apply it with your own Git tools, push, open the pull request, then link it here.";
					} else if (aStep.PatchSave) {
						shown.Status = "A file named " + name + " is already there; it was kept. Choose another name.";
					} else if (aStep.PatchFailure) {
						shown.Status = cycle_export_words(*aStep.PatchFailure);
					} else if (aStep.External) {
						shown.Status = aStep.External->Limit ? candidate_words(*aStep.External->Limit) : outgoing_words(aStep.External->Outcome, base);
					}
					aSelf.publish();
				}));
		}

		void link(const std::string& aText)
		{
			if (!View.Publication || View.Publication->Busy) {
				return;
			}
			auto& view = *View.Publication;
			// The address as the Home bar reads it: a pasted link, or owner/name#number on the account's host.
			const auto named = parse_setup_destination(aText);
			if (!named) {
				view.Status = link_words(core::cycle_link_refusal::address, { });
				publish();
				return;
			}
			const auto acting = account();
			if (!acting) {
				view.Status = "Sign in to GitHub so the pull request can be read before it is linked.";
				publish();
				return;
			}
			const auto address = core::cycle_pull_address{
				.Host = named->Hostname.empty() ? acting->Host : named->Hostname, .Owner = named->Repository.Owner, .Repository = named->Repository.Name, .Number = named->Number.value()
			};
			view.Busy   = true;
			view.Status = "Reading the pull request; nothing is sent.";
			publish();
			Runtime.publication_selected(
				[actor = *acting, op = operation(), asked = address](review_cycle_store& aStore, const core::review_cycle_state& aCycle, std::stop_token aStop) {
					return application::link_pull_request(aStore, aCycle, actor, op, asked, std::move(aStop));
				},
				publication_guarded([host = acting->Host](implementation& aSelf, review_cycle_publication_step aStep) {
					if (!aSelf.View.Publication) {
						return;
					}
					auto& shown = *aSelf.View.Publication;
					shown.Busy  = false;
					if (aStep.Provider) {
						shown.Status = provider_words(*aStep.Provider) + " Nothing was linked.";
					} else if (aStep.LinkRefusal) {
						shown.Status = link_words(*aStep.LinkRefusal, host);
					} else if (aStep.Linked) {
						shown.Status = "Linked pull request #" + std::to_string(aStep.Linked->Address.Number) + " as externally published. BetterPullReview did not publish it.";
					}
					aSelf.publish();
				}));
		}
	};

	review_cycle_workbench::review_cycle_workbench(std::unique_ptr<review_cycle_store> aStore, workspace_source_port& aSource, effect_executor& aEffects, decision_executor& aDecisions,
												   review_cycle_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed, effect_executor* aCaptures)
		: Implementation{ std::make_shared<implementation>(std::move(aStore), aSource, aEffects, aDecisions, aPort, aSeed, aCaptures) }
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

	void review_cycle_workbench::cancel_capture()
	{
		if (Implementation->View.Capturing) {
			Implementation->Runtime.cancel_capture();
		}
	}

	void review_cycle_workbench::list_files(const int aDirection, const bool aOpenEdge)
	{
		Implementation->list_files(aDirection, aOpenEdge);
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
											  self.guarded<cycle_write_receipt>([item](implementation& aSelf, cycle_write_receipt) {
												  aSelf.View.Activity = review_cycle_activity::idle;
												  aSelf.View.Message.clear();
												  aSelf.note_changed(item);
											  }));
	}

	void review_cycle_workbench::capture_revision(const bool aDeliberate)
	{
		auto& self = *Implementation;
		// One capture at a time: a running one is cancelled, not repeated.
		if (!self.Cycle || self.View.Capturing || self.View.Activity == review_cycle_activity::capturing) {
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
		self.Runtime.decide_selected(self.operation(), { .Item = aItem, .Decision = aDecision, .Checked = checked }, self.guarded<cycle_decision_result>([item = aItem.Id](implementation& aSelf, cycle_decision_result aResult) {
			if (aResult.Refusal) {
				aSelf.fail("That decision could not be recorded: the note or the revision is not what is shown.");
				return;
			}
			aSelf.View.Activity = review_cycle_activity::idle;
			aSelf.View.Message.clear();
			aSelf.reconcile_note(item);
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

	void review_cycle_workbench::set_publication_environment(review_cycle_publication_environment aEnvironment)
	{
		Implementation->Publishing = std::move(aEnvironment);
		Implementation->Candidates.reset();
	}

	void review_cycle_workbench::open_publication()
	{
		Implementation->open_publication();
	}

	void review_cycle_workbench::close_publication()
	{
		auto& self = *Implementation;
		if (self.View.Publication) {
			self.View.Publication.reset();
			self.publish();
		}
	}

	void review_cycle_workbench::edit_publication(review_cycle_publication_form aForm)
	{
		Implementation->edit_publication(std::move(aForm));
	}

	void review_cycle_workbench::prepare_publication()
	{
		Implementation->prepare_publication();
	}

	void review_cycle_workbench::revise_publication()
	{
		auto& self = *Implementation;
		if (!self.View.Publication || self.View.Publication->Busy || self.View.Publication->Stage != review_cycle_publication_stage::planned) {
			return;
		}
		auto& view = *self.View.Publication;
		view.Current.reset();
		view.Digest.clear();
		view.Preview.clear();
		view.Stage  = review_cycle_publication_stage::choosing;
		view.Status = "Change what you need, then review the plan again. The plan you read authorizes nothing now.";
		self.publish();
	}

	void review_cycle_workbench::publish_publication()
	{
		Implementation->publish_publication();
	}

	void review_cycle_workbench::recover_publication()
	{
		Implementation->recover_publication();
	}

	void review_cycle_workbench::cancel_publication()
	{
		Implementation->cancel_publication();
	}

	void review_cycle_workbench::save_publication_patch(std::string aPath)
	{
		Implementation->save_patch(std::move(aPath));
	}

	void review_cycle_workbench::link_pull_request(std::string aAddress)
	{
		Implementation->link(aAddress);
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
