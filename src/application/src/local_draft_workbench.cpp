#include "bpr/application/local_draft_workbench.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/hex.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto recovery_for(const local_draft_discovery& aDiscovery) -> local_draft_recovery
		{
			switch (aDiscovery.Source) {
			case core::source_state::detached:   return local_draft_recovery::detached_head;
			case core::source_state::unborn:     return local_draft_recovery::unborn_branch;
			case core::source_state::unreadable: return local_draft_recovery::unreadable;
			case core::source_state::branch:     break;
			}
			if (aDiscovery.Git) {
				return local_draft_recovery::git;
			}
			if (aDiscovery.ChainRefusal) {
				return local_draft_recovery::remote_chain;
			}
			if (!aDiscovery.TargetTip) {
				return local_draft_recovery::target_unavailable;
			}
			switch (aDiscovery.Comparison.State) {
			case core::comparison_state::ready:              return local_draft_recovery::none;
			case core::comparison_state::target_unavailable: return local_draft_recovery::target_unavailable;
			case core::comparison_state::no_common_base:     return local_draft_recovery::no_common_base;
			case core::comparison_state::several_bases:      return local_draft_recovery::several_bases;
			}
			return local_draft_recovery::git;
		}

		[[nodiscard]] auto recovery_for(const local_draft_open_error& aError) -> local_draft_recovery
		{
			switch (aError.Failure) {
			case local_draft_open_failure::not_git:    return local_draft_recovery::not_git;
			case local_draft_open_failure::repository: return local_draft_recovery::repository;
			case local_draft_open_failure::workspace:
			case local_draft_open_failure::cancelled:  return local_draft_recovery::unreadable;
			}
			return local_draft_recovery::unreadable;
		}

		/// The working files still listed: a file committed since it was
		/// included leaves the selection rather than fail the capture.
		[[nodiscard]] auto still_listed(const std::vector<std::string>& aIncluded, const std::vector<core::working_file>& aWorking) -> std::vector<std::string>
		{
			auto kept = std::vector<std::string>{ };
			for (const auto& path : aIncluded) {
				const auto found = std::ranges::find(aWorking, path, &core::working_file::Path);
				if (found != aWorking.end() && found->Qualification == core::working_qualification::none) {
					kept.push_back(path);
				}
			}
			return kept;
		}

		/// Whether the worktree now proposes something other than the candidate.
		[[nodiscard]] auto differs(const core::local_draft_candidate& aCandidate, const local_draft_discovery& aDiscovery, const std::vector<std::string>& aIncluded) -> bool
		{
			if (!aDiscovery.ready()) {
				return false;
			}
			auto included = aIncluded;
			std::ranges::sort(included);
			if (aCandidate.Head != *aDiscovery.Head || aCandidate.TargetTip != *aDiscovery.TargetTip || aCandidate.Base != *aDiscovery.Comparison.Base || aCandidate.Included != included) {
				return true;
			}
			// An included file edited again since it was captured.
			return std::ranges::any_of(aCandidate.Changes, [&aDiscovery](const core::draft_path_change& aChange) {
				if (!aChange.Right || aChange.Right->Origin != core::draft_material_origin::working) {
					return false;
				}
				const auto found = std::ranges::find(aDiscovery.Working, aChange.Path, &core::working_file::Path);
				return found == aDiscovery.Working.end() || found->Object != aChange.Right->Object;
			});
		}

		[[nodiscard]] auto find_candidate(const core::local_draft& aDraft, const std::uint64_t aOrdinal) -> const core::local_draft_candidate*
		{
			const auto found = std::ranges::find(aDraft.Candidates, aOrdinal, &core::local_draft_candidate::Ordinal);
			return found == aDraft.Candidates.end() ? nullptr : &*found;
		}
	}

	auto name(const local_draft_open_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case local_draft_open_outcome::resumed:       return "resumed";
		case local_draft_open_outcome::created:       return "created";
		case local_draft_open_outcome::choose_target: return "choose_target";
		case local_draft_open_outcome::moved:         return "moved";
		case local_draft_open_outcome::recovery:      return "recovery";
		}
		return "unknown";
	}

	auto name(const local_draft_recovery aRecovery) noexcept -> std::string_view
	{
		switch (aRecovery) {
		case local_draft_recovery::none:               return "none";
		case local_draft_recovery::not_git:            return "not_git";
		case local_draft_recovery::unreadable:         return "unreadable";
		case local_draft_recovery::repository:         return "repository";
		case local_draft_recovery::detached_head:      return "detached_head";
		case local_draft_recovery::unborn_branch:      return "unborn_branch";
		case local_draft_recovery::remote_chain:       return "remote_chain";
		case local_draft_recovery::target_unavailable: return "target_unavailable";
		case local_draft_recovery::no_common_base:     return "no_common_base";
		case local_draft_recovery::several_bases:      return "several_bases";
		case local_draft_recovery::git:                return "git";
		case local_draft_recovery::store:              return "store";
		}
		return "unknown";
	}

	auto open_local_draft(local_draft_store& aStore, workspace_source_port& aSource, local_draft_git_port& aGit, const std::string_view aLocation,
						  const std::optional<std::string>& aContext, core::local_draft_id aNewId, const std::int64_t aNow, const std::stop_token aStop)
		-> std::expected<local_draft_open_result, local_draft_open_error>
	{
		auto drafts = aStore.list();
		if (!drafts) {
			auto discovered = discover_local_draft(aSource, aGit, aLocation, std::nullopt, aStop);
			if (!discovered) {
				return std::unexpected{ discovered.error() };
			}
			return local_draft_open_result{ .Recovery = local_draft_recovery::store, .Discovery = std::move(*discovered), .Store = drafts.error() };
		}
		auto discovered = discover_local_draft(aSource, aGit, aLocation, std::nullopt, aStop);
		if (!discovered) {
			return std::unexpected{ discovered.error() };
		}
		auto  result    = local_draft_open_result{ .Discovery = std::move(*discovered) };
		auto& discovery = result.Discovery;
		if (discovery.Source != core::source_state::branch) {
			result.Recovery = recovery_for(discovery);
			return result;
		}
		const auto resolution = core::resolve_worktree(*drafts, discovery.Root, discovery.Branch);
		if (resolution.Moved) {
			result.Outcome = local_draft_open_outcome::moved;
			result.Draft   = (*drafts)[*resolution.Moved];
			return result;
		}
		const auto note = [&](core::local_draft aDraft) -> std::expected<core::local_draft, local_draft_store_error> {
			if (!aContext || aContext->empty()) {
				return aDraft;
			}
			const auto previous = aDraft.Version;
			auto       noted    = core::add_note(aDraft, previous, *aContext, { }, core::note_origin::harness, aNow);
			if (!noted) {
				return aDraft;
			}
			if (auto written = aStore.put(*noted, previous, { }); !written) {
				return std::unexpected{ written.error() };
			}
			return std::move(*noted);
		};
		if (resolution.Resume) {
			auto draft = (*drafts)[*resolution.Resume];
			// The draft's own target wins over what the remotes suggest.
			if (const auto kept = core::choose_target(discovery.Destinations, draft.Target); kept && kept->Target != (discovery.Target ? discovery.Target->Target : core::local_draft_target{ })) {
				if (auto retargeted = retarget_discovery(aSource, aGit, std::move(discovery), *kept, aStop)) {
					discovery = std::move(*retargeted);
				} else {
					return std::unexpected{ retargeted.error() };
				}
			}
			auto noted = note(std::move(draft));
			if (!noted) {
				result.Store = noted.error();
			}
			result.Outcome = local_draft_open_outcome::resumed;
			result.Draft   = noted ? std::move(*noted) : (*drafts)[*resolution.Resume];
			return result;
		}
		if (discovery.Selection) {
			result.Outcome = local_draft_open_outcome::choose_target;
			return result;
		}
		result.Recovery = recovery_for(discovery);
		if (result.Recovery != local_draft_recovery::none) {
			return result;
		}
		auto capture = capture_local_draft(aSource, aGit, discovery, { }, aNow, aStop);
		if (!capture) {
			result.Capture  = capture.error();
			result.Recovery = local_draft_recovery::git;
			return result;
		}
		auto created = core::create_local_draft(*drafts, std::move(aNewId), worktree_of(discovery), discovery.Branch, *discovery.Target,
												discovery.Chain ? discovery.Chain->Hops : std::vector<core::local_remote_hop>{ }, std::move(capture->Candidate), aNow);
		if (!created) {
			result.Recovery = local_draft_recovery::store;
			result.Store    = local_draft_store_error{ .Failure = created.error() == core::draft_refusal::drafts_full ? local_draft_store_failure::full : local_draft_store_failure::corrupt,
													   .Used    = drafts->size(),
													   .Limit   = core::maximum_local_drafts };
			return result;
		}
		if (auto written = aStore.put(*created, std::nullopt, capture->Objects); !written) {
			result.Recovery = local_draft_recovery::store;
			result.Store    = written.error();
			return result;
		}
		const auto createdId = created->Id;
		auto       noted     = note(std::move(*created));
		if (!noted) {
			result.Store = noted.error();
		}
		result.Outcome = local_draft_open_outcome::created;
		result.Draft   = noted ? std::move(*noted) : std::optional<core::local_draft>{ };
		if (!result.Draft) {
			if (auto stored = aStore.find(createdId); stored) {
				result.Draft = std::move(*stored);
			}
		}
		return result;
	}

	struct local_draft_workbench::implementation : std::enable_shared_from_this<implementation>
	{
		implementation(std::function<std::unique_ptr<local_draft_store>()> aStore, workspace_source_port& aSource, local_draft_git_factory aGit, effect_executor& aEffects,
					   decision_executor& aDecisions, local_draft_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed, std::function<std::int64_t()> aNow,
					   std::shared_ptr<diff_line_matcher_port> aMatcher)
			: StoreFactory{ std::move(aStore) }
			, Source{ aSource }
			, GitFactory{ std::move(aGit) }
			, Effects{ aEffects }
			, Decisions{ aDecisions }
			, Port{ aPort }
			, Now{ std::move(aNow) }
			, Matcher{ std::move(aMatcher) }
		{
			auto sequence = std::seed_seq(aSeed.begin(), aSeed.end());
			Random.seed(sequence);
		}

		// --- effect lane ---------------------------------------------------

		[[nodiscard]] auto store() -> local_draft_store*
		{
			if (!Store && StoreFactory) {
				Store = StoreFactory();
			}
			return Store.get();
		}

		[[nodiscard]] auto git(const std::stop_token aStop) -> std::expected<local_draft_git_port*, publication_git_error>
		{
			if (!Git) {
				if (!GitFactory) {
					return std::unexpected{ publication_git_error{ .Failure = publication_git_failure::unavailable } };
				}
				auto made = GitFactory(aStop);
				if (!made) {
					return std::unexpected{ made.error() };
				}
				Git = std::move(*made);
			}
			return Git.get();
		}

		[[nodiscard]] auto new_id() -> core::local_draft_id
		{
			auto bytes = std::array<std::uint8_t, 16>{ };
			for (auto& byte : bytes) {
				byte = static_cast<std::uint8_t>(Random() & 0xFFU);
			}
			return { core::lower_hex(bytes) };
		}

		// --- lanes ---------------------------------------------------------

		template <typename TEffect, typename TThen>
		void run(TEffect aEffect, TThen aThen)
		{
			if (Stopped) {
				return;
			}
			auto self       = shared_from_this();
			auto generation = Generation;
			++Pending;
			Projection.Busy = true;
			Effects.post([self, effect = std::move(aEffect), then = std::move(aThen), generation]() mutable {
				auto result = [&] {
					const auto lock = std::scoped_lock{ self->EffectMutex };
					return effect(*self, self->Stop.get_token());
				}();
				self->Decisions.post([host = self, finish = std::move(then), outcome = std::move(result), generation]() mutable {
					--host->Pending;
					if (host->Stopped) {
						return;
					}
					host->Projection.Busy = host->Pending > 0U;
					if (generation != host->Generation) {
						host->publish();
						return;
					}
					finish(*host, std::move(outcome));
				});
			});
		}

		void publish()
		{
			if (!Stopped) {
				Port.publish(Projection);
			}
		}

		// --- decision lane ------------------------------------------------

		void refresh_home()
		{
			auto self = shared_from_this();
			++HomePending;
			Effects.post([self] {
				auto home = local_draft_home_projection{ };
				{
					const auto lock   = std::scoped_lock{ self->EffectMutex };
					auto*      stored = self->store();
					auto       drafts = stored == nullptr ? local_draft_store_result<std::vector<core::local_draft>>{ std::unexpected{ local_draft_store_error{ } } } : stored->list();
					if (!drafts) {
						home.Unavailable = true;
					} else {
						for (const auto index : core::drafts_in_preparation(*drafts)) {
							const auto& draft = (*drafts)[index];
							home.InPreparation.push_back({ .Id         = draft.Id,
														   .Title      = draft.Title,
														   .Repository = draft.Target.Target.Repository.Owner + "/" + draft.Target.Target.Repository.Name,
														   .Branch     = draft.Branch,
														   .Updated    = draft.Updated });
						}
						for (const auto& draft : *drafts) {
							if (draft.Association) {
								home.Marks.push_back({ .Id = draft.Id, .Host = draft.Association->Host, .Owner = draft.Association->Owner, .Name = draft.Association->Name, .Number = draft.Association->Number });
							}
						}
					}
				}
				self->Decisions.post([host = self, rows = std::move(home)]() mutable {
					--host->HomePending;
					if (host->Stopped) {
						return;
					}
					host->Home = std::move(rows);
					host->Port.publish(host->Home);
				});
			});
		}

		/// Shows what one open or resume found.
		void show(local_draft_open_result aResult)
		{
			auto& view    = Projection;
			view.Stage    = local_draft_stage::open;
			view.Outcome  = aResult.Outcome;
			view.Recovery = aResult.Recovery;
			view.Store    = aResult.Store;
			view.Capture  = aResult.Capture;
			view.Git      = aResult.Discovery.Git;
			view.Location = aResult.Discovery.Root.Location.empty() ? view.Location : aResult.Discovery.Root.Location;
			view.Draft    = std::move(aResult.Draft);
			view.File.reset();
			observe(std::move(aResult.Discovery));
			const auto* latest = view.Draft ? core::latest_candidate(*view.Draft) : nullptr;
			view.Shown         = latest == nullptr ? 0U : latest->Ordinal;
			view.Included      = latest == nullptr ? std::vector<std::string>{ } : still_listed(latest->Included, view.Working);
			update_refresh();
			publish();
			open_first_file();
			refresh_home();
		}

		/// Takes a new observation of the worktree into the view.
		void observe(local_draft_discovery aDiscovery)
		{
			auto& view              = Projection;
			view.Branch             = aDiscovery.Branch;
			view.Source             = aDiscovery.Source;
			view.Target             = aDiscovery.Target;
			view.Chain              = aDiscovery.Chain ? aDiscovery.Chain->Hops : std::vector<core::local_remote_hop>{ };
			view.ChainRefusal       = aDiscovery.ChainRefusal;
			view.Selection          = aDiscovery.Selection;
			view.Head               = aDiscovery.Head;
			view.TargetTip          = aDiscovery.TargetTip;
			view.Comparison         = aDiscovery.Comparison;
			view.Working            = aDiscovery.Working;
			view.ScanIncomplete     = aDiscovery.ScanIncomplete;
			view.FilteredUncompared = aDiscovery.FilteredUncompared;
			view.Match              = view.Draft ? core::match_worktree(*view.Draft, aDiscovery.Root, aDiscovery.Branch) : core::worktree_match::same;
			Discovery               = std::move(aDiscovery);
		}

		void update_refresh()
		{
			auto&       view    = Projection;
			const auto* latest  = view.Draft ? core::latest_candidate(*view.Draft) : nullptr;
			view.RefreshOffered = latest != nullptr && Discovery && view.Match == core::worktree_match::same && differs(*latest, *Discovery, view.Included);
		}

		void open_first_file()
		{
			const auto* shown = shown_candidate();
			if (shown != nullptr && !shown->Changes.empty()) {
				open_file(shown->Changes.front().Path);
			}
		}

		[[nodiscard]] auto shown_candidate() const -> const core::local_draft_candidate*
		{
			return Projection.Draft ? find_candidate(*Projection.Draft, Projection.Shown) : nullptr;
		}

		void open_location(std::string aLocation, std::optional<std::string> aContext)
		{
			++Generation;
			Projection          = { };
			Projection.Stage    = local_draft_stage::opening;
			Projection.Location = aLocation;
			Discovery.reset();
			publish();
			run(
				[location = std::move(aLocation), context = std::move(aContext)](implementation& aSelf, const std::stop_token aStop)
					-> std::expected<local_draft_open_result, local_draft_open_error> {
					auto* stored = aSelf.store();
					auto  reader = aSelf.git(aStop);
					if (stored == nullptr || !reader) {
						auto failure = local_draft_open_result{ .Recovery = stored == nullptr ? local_draft_recovery::store : local_draft_recovery::git };
						if (stored == nullptr) {
							failure.Store = local_draft_store_error{ };
						}
						if (!reader) {
							failure.Discovery.Git = reader.error();
						}
						return failure;
					}
					return open_local_draft(*stored, aSelf.Source, **reader, location, context, aSelf.new_id(), aSelf.Now(), aStop);
				},
				[](implementation& aSelf, std::expected<local_draft_open_result, local_draft_open_error> aResult) {
					if (!aResult) {
						aSelf.Projection.Stage    = local_draft_stage::open;
						aSelf.Projection.Recovery = recovery_for(aResult.error());
						aSelf.publish();
						return;
					}
					aSelf.show(std::move(*aResult));
				});
		}

		void resume(core::local_draft_id aId)
		{
			++Generation;
			Projection       = { };
			Projection.Stage = local_draft_stage::opening;
			Discovery.reset();
			publish();
			run(
				[id = std::move(aId)](implementation& aSelf, const std::stop_token aStop) -> local_draft_open_result {
					auto  result = local_draft_open_result{ .Outcome = local_draft_open_outcome::resumed };
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						result.Recovery = local_draft_recovery::store;
						result.Store    = local_draft_store_error{ };
						return result;
					}
					auto draft = stored->find(id);
					if (!draft) {
						result.Recovery = local_draft_recovery::store;
						result.Store    = draft.error();
						return result;
					}
					result.Draft = *draft;
					auto reader  = aSelf.git(aStop);
					if (!reader) {
						result.Discovery.Git = reader.error();
						return result;
					}
					auto discovered = discover_local_draft(aSelf.Source, **reader, draft->Worktree.Location, draft->Target, aStop);
					if (discovered) {
						result.Discovery = std::move(*discovered);
					} else {
						result.Discovery.Root.Location = draft->Worktree.Location;
					}
					return result;
				},
				[](implementation& aSelf, local_draft_open_result aResult) {
					const auto missing = aResult.Draft && aResult.Discovery.Root.Repository == std::nullopt;
					aSelf.show(std::move(aResult));
					aSelf.Projection.WorktreeMissing = missing;
					if (missing) {
						aSelf.Projection.Match = core::worktree_match::other;
					}
					aSelf.update_refresh();
					aSelf.publish();
				});
		}

		/// Writes a changed draft: the version read is the one replaced. A
		/// lost race re-reads and shows the stored draft.
		void write(std::expected<core::local_draft, core::draft_refusal> aChanged, std::vector<local_draft_object> aObjects = { })
		{
			if (!aChanged || !Projection.Draft) {
				publish();
				return;
			}
			const auto previous = Projection.Draft->Version;
			run(
				[draft = std::move(*aChanged), previous, objects = std::move(aObjects)](implementation& aSelf, std::stop_token)
					-> std::pair<local_draft_store_result<void>, std::optional<core::local_draft>> {
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						return { std::unexpected{ local_draft_store_error{ } }, std::nullopt };
					}
					auto written = stored->put(draft, previous, objects);
					if (written) {
						return { written, draft };
					}
					auto current = stored->find(draft.Id);
					return { written, current ? std::optional{ std::move(*current) } : std::nullopt };
				},
				[](implementation& aSelf, std::pair<local_draft_store_result<void>, std::optional<core::local_draft>> aResult) {
					aSelf.Projection.Reread = !aResult.first && aResult.first.error().Failure == local_draft_store_failure::stale;
					aSelf.Projection.Store  = aResult.first ? std::nullopt : std::optional{ aResult.first.error() };
					if (aResult.second) {
						aSelf.Projection.Draft = std::move(aResult.second);
						if (const auto* latest = core::latest_candidate(*aSelf.Projection.Draft); latest != nullptr && aResult.first) {
							if (aSelf.Projection.Shown != latest->Ordinal) {
								aSelf.Projection.Shown = latest->Ordinal;
								aSelf.Projection.File.reset();
							}
						}
					}
					aSelf.update_refresh();
					aSelf.publish();
					if (!aSelf.Projection.File) {
						aSelf.open_first_file();
					}
					aSelf.refresh_home();
				});
		}

		void choose_target(core::local_draft_target aTarget)
		{
			if (!Discovery) {
				return;
			}
			const auto chosen = core::choose_target_explicitly(Discovery->Destinations, aTarget);
			if (!chosen) {
				return;
			}
			run(
				[discovery = *Discovery, target = *chosen, draft = Projection.Draft](implementation& aSelf, const std::stop_token aStop) mutable
					-> std::expected<local_draft_open_result, local_draft_open_error> {
					auto reader = aSelf.git(aStop);
					if (!reader) {
						auto failure          = local_draft_open_result{ .Recovery = local_draft_recovery::git, .Draft = std::move(draft) };
						failure.Discovery.Git = reader.error();
						return failure;
					}
					auto retargeted = retarget_discovery(aSelf.Source, **reader, std::move(discovery), target, aStop);
					if (!retargeted) {
						return std::unexpected{ retargeted.error() };
					}
					auto  result = local_draft_open_result{ .Outcome = draft ? local_draft_open_outcome::resumed : local_draft_open_outcome::created, .Discovery = std::move(*retargeted) };
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						result.Recovery = local_draft_recovery::store;
						result.Store    = local_draft_store_error{ };
						result.Draft    = std::move(draft);
						return result;
					}
					if (draft) {
						auto changed = core::retarget_draft(*draft, draft->Version, target, result.Discovery.Chain ? result.Discovery.Chain->Hops : std::vector<core::local_remote_hop>{ },
															aSelf.Now());
						if (changed) {
							if (auto written = stored->put(*changed, draft->Version, { }); written) {
								draft = std::move(*changed);
							} else {
								result.Store = written.error();
							}
						}
						result.Draft = std::move(draft);
						return result;
					}
					result.Recovery = recovery_for(result.Discovery);
					if (result.Recovery != local_draft_recovery::none) {
						result.Outcome = local_draft_open_outcome::recovery;
						return result;
					}
					auto capture = capture_local_draft(aSelf.Source, **reader, result.Discovery, { }, aSelf.Now(), aStop);
					if (!capture) {
						result.Outcome  = local_draft_open_outcome::recovery;
						result.Recovery = local_draft_recovery::git;
						result.Capture  = capture.error();
						return result;
					}
					auto drafts  = stored->list();
					auto created = core::create_local_draft(drafts ? *drafts : std::vector<core::local_draft>{ }, aSelf.new_id(), worktree_of(result.Discovery), result.Discovery.Branch,
															target, result.Discovery.Chain ? result.Discovery.Chain->Hops : std::vector<core::local_remote_hop>{ },
															std::move(capture->Candidate), aSelf.Now());
					if (!created) {
						result.Outcome  = local_draft_open_outcome::recovery;
						result.Recovery = local_draft_recovery::store;
						result.Store    = local_draft_store_error{ .Failure = local_draft_store_failure::full, .Used = drafts ? drafts->size() : 0U, .Limit = core::maximum_local_drafts };
						return result;
					}
					if (auto written = stored->put(*created, std::nullopt, capture->Objects); !written) {
						result.Outcome  = local_draft_open_outcome::recovery;
						result.Recovery = local_draft_recovery::store;
						result.Store    = written.error();
						return result;
					}
					result.Draft = std::move(*created);
					return result;
				},
				[](implementation& aSelf, std::expected<local_draft_open_result, local_draft_open_error> aResult) {
					if (!aResult) {
						aSelf.Projection.Recovery = recovery_for(aResult.error());
						aSelf.publish();
						return;
					}
					aSelf.show(std::move(*aResult));
				});
		}

		void include(std::string aPath, const bool aIncluded)
		{
			auto& included = Projection.Included;
			std::erase(included, aPath);
			if (aIncluded) {
				const auto found = std::ranges::find(Projection.Working, aPath, &core::working_file::Path);
				if (found == Projection.Working.end() || found->Qualification != core::working_qualification::none) {
					publish();
					return;
				}
				included.push_back(std::move(aPath));
				std::ranges::sort(included);
			}
			update_refresh();
			publish();
		}

		void refresh()
		{
			if (!Projection.Draft) {
				return;
			}
			run(
				[draft = *Projection.Draft, included = Projection.Included](implementation& aSelf, const std::stop_token aStop) -> local_draft_open_result {
					auto  result = local_draft_open_result{ .Outcome = local_draft_open_outcome::resumed, .Draft = draft };
					auto  reader = aSelf.git(aStop);
					auto* stored = aSelf.store();
					if (!reader || stored == nullptr) {
						result.Recovery = !reader ? local_draft_recovery::git : local_draft_recovery::store;
						if (!reader) {
							result.Discovery.Git = reader.error();
						}
						return result;
					}
					auto discovered = discover_local_draft(aSelf.Source, **reader, draft.Worktree.Location, draft.Target, aStop);
					if (!discovered) {
						result.Discovery.Root.Location = draft.Worktree.Location;
						result.Recovery                = recovery_for(discovered.error());
						return result;
					}
					result.Discovery = std::move(*discovered);
					// Another branch, worktree or location records nothing.
					if (core::match_worktree(draft, result.Discovery.Root, result.Discovery.Branch) != core::worktree_match::same) {
						return result;
					}
					result.Recovery = recovery_for(result.Discovery);
					if (result.Recovery != local_draft_recovery::none) {
						return result;
					}
					const auto kept    = still_listed(included, result.Discovery.Working);
					auto       capture = capture_local_draft(aSelf.Source, **reader, result.Discovery, kept, aSelf.Now(), aStop);
					if (!capture) {
						result.Capture = capture.error();
						return result;
					}
					auto next = core::add_candidate(draft, draft.Version, std::move(capture->Candidate), aSelf.Now());
					if (!next) {
						if (next.error() == core::draft_refusal::candidates_full) {
							result.Store = local_draft_store_error{ .Failure = local_draft_store_failure::full, .Used = draft.Candidates.size(), .Limit = core::maximum_local_draft_candidates };
						}
						return result;
					}
					if (auto written = stored->put(*next, draft.Version, capture->Objects); !written) {
						result.Store = written.error();
						if (auto current = stored->find(draft.Id)) {
							result.Draft = std::move(*current);
						}
						return result;
					}
					result.Draft = std::move(*next);
					return result;
				},
				[](implementation& aSelf, local_draft_open_result aResult) {
					auto& view     = aSelf.Projection;
					view.Reread    = aResult.Store && aResult.Store->Failure == local_draft_store_failure::stale;
					view.Store     = aResult.Store;
					view.Capture   = aResult.Capture;
					view.Recovery  = aResult.Recovery;
					view.Git       = aResult.Discovery.Git;
					const auto old = view.Shown;
					view.Draft     = std::move(aResult.Draft);
					aSelf.observe(std::move(aResult.Discovery));
					const auto* latest = view.Draft ? core::latest_candidate(*view.Draft) : nullptr;
					view.Shown         = latest == nullptr ? 0U : latest->Ordinal;
					if (latest != nullptr) {
						view.Included = still_listed(latest->Included, view.Working);
					}
					aSelf.update_refresh();
					if (old != view.Shown) {
						view.File.reset();
					}
					aSelf.publish();
					if (!view.File) {
						aSelf.open_first_file();
					}
					aSelf.refresh_home();
				});
		}

		void open_file(std::string aPath)
		{
			const auto* shown = shown_candidate();
			if (shown == nullptr) {
				return;
			}
			const auto change = std::ranges::find(shown->Changes, aPath, &core::draft_path_change::Path);
			if (change == shown->Changes.end()) {
				return;
			}
			Projection.File = local_draft_file_view{ .Path = aPath, .Change = change->Change, .Status = review_material_status::failure };
			publish();
			run(
				[wanted = *change, draft = Projection.Draft->Id, ordinal = shown->Ordinal](implementation& aSelf, const std::stop_token aStop) -> local_draft_file_view {
					auto  view   = local_draft_file_view{ .Path = wanted.Path, .Change = wanted.Change };
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						return view;
					}
					const auto side = [&](const std::optional<core::draft_side>& aSide) -> std::optional<std::vector<std::uint8_t>> {
						if (!aSide) {
							return std::vector<std::uint8_t>{ };
						}
						auto bytes = stored->object(aSide->Object);
						return bytes && *bytes ? std::optional{ std::move(**bytes) } : std::nullopt;
					};
					const auto left  = side(wanted.Left);
					const auto right = side(wanted.Right);
					if (!left || !right) {
						view.Status = review_material_status::missing;
						return view;
					}
					const auto options = core::diff_options{ };
					const auto key     = core::local_draft_file_comparison(draft, ordinal, wanted.Path, wanted.Left ? std::optional{ wanted.Left->Object } : std::nullopt,
																		   wanted.Right ? std::optional{ wanted.Right->Object } : std::nullopt, options);
					if (!key || !aSelf.Matcher) {
						view.Status = review_material_status::failure;
						return view;
					}
					auto check    = stop_token_check{ aStop };
					auto matching = abandonable_diff_line_matching{ aSelf.Matcher, aStop };
					auto run      = core::compute_material_diff(
						{ .Key        = *key,
						  .Options    = options,
						  .LeftBytes  = wanted.Left ? std::optional{ std::span<const std::uint8_t>{ *left } } : std::nullopt,
						  .RightBytes = wanted.Right ? std::optional{ std::span<const std::uint8_t>{ *right } } : std::nullopt,
						  .Limits     = core::diff_compute_limits{ } },
						core::diff_compute_control{ .Stop = &check }, matching);
					if (!run.Outcome.has_value() || aStop.stop_requested()) {
						view.Status = review_material_status::cancelled;
						return view;
					}
					view.Reason = run.Outcome->Reason;
					if (run.Outcome->Result) {
						view.Rows = core::diff_rows(*run.Outcome->Result);
					}
					view.Status = review_material_status::available;
					return view;
				},
				[](implementation& aSelf, local_draft_file_view aView) {
					if (aSelf.Projection.File && aSelf.Projection.File->Path == aView.Path) {
						aSelf.Projection.File = std::move(aView);
						aSelf.publish();
					}
				});
		}

		void rebind()
		{
			if (!Projection.Draft || Projection.Outcome != local_draft_open_outcome::moved || !Discovery) {
				return;
			}
			auto changed = core::rebind_draft(*Projection.Draft, Projection.Draft->Version, Discovery->Root.Location, Now());
			if (!changed) {
				return;
			}
			const auto id = changed->Id;
			run(
				[draft = std::move(*changed), previous = Projection.Draft->Version](implementation& aSelf, std::stop_token) -> local_draft_store_result<void> {
					auto* stored = aSelf.store();
					return stored == nullptr ? local_draft_store_result<void>{ std::unexpected{ local_draft_store_error{ } } } : stored->put(draft, previous, { });
				},
				[id](implementation& aSelf, local_draft_store_result<void> aWritten) {
					if (!aWritten) {
						aSelf.Projection.Store = aWritten.error();
						aSelf.publish();
						return;
					}
					aSelf.resume(id);
				});
		}

		void discard()
		{
			if (!Projection.Draft) {
				return;
			}
			run(
				[id = Projection.Draft->Id](implementation& aSelf, std::stop_token) -> local_draft_store_result<void> {
					auto* stored = aSelf.store();
					return stored == nullptr ? local_draft_store_result<void>{ std::unexpected{ local_draft_store_error{ } } } : stored->remove(id);
				},
				[](implementation& aSelf, local_draft_store_result<void> aRemoved) {
					if (!aRemoved && aRemoved.error().Failure != local_draft_store_failure::missing) {
						aSelf.Projection.Store = aRemoved.error();
						aSelf.publish();
						return;
					}
					aSelf.close();
					aSelf.refresh_home();
				});
		}

		void close()
		{
			++Generation;
			Projection = { };
			Discovery.reset();
			publish();
		}

		std::function<std::unique_ptr<local_draft_store>()> StoreFactory;
		workspace_source_port&                              Source;
		local_draft_git_factory                             GitFactory;
		effect_executor&                                    Effects;
		decision_executor&                                  Decisions;
		local_draft_workbench_port&                         Port;
		std::function<std::int64_t()>                       Now;
		std::shared_ptr<diff_line_matcher_port>             Matcher;

		/// Effect lane only, under `EffectMutex`.
		std::mutex                            EffectMutex;
		std::unique_ptr<local_draft_store>    Store;
		std::unique_ptr<local_draft_git_port> Git;
		std::mt19937_64                       Random;
		std::stop_source                      Stop;

		/// Decision lane.
		local_draft_workbench_projection     Projection;
		local_draft_home_projection          Home;
		std::optional<local_draft_discovery> Discovery;
		std::uint64_t                        Generation = 0;
		std::atomic<std::uint64_t>           Pending{ 0U };
		/// Home's reads, which never make the page busy.
		std::atomic<std::uint64_t> HomePending{ 0U };
		bool                       Stopped = false;
	};

	local_draft_workbench::local_draft_workbench(std::function<std::unique_ptr<local_draft_store>()> aStore, workspace_source_port& aSource, local_draft_git_factory aGit,
												 effect_executor& aEffects, decision_executor& aDecisions, local_draft_workbench_port& aPort, const std::array<std::uint8_t, 32>& aSeed,
												 std::function<std::int64_t()> aNow, std::shared_ptr<diff_line_matcher_port> aMatcher)
		: Implementation{ std::make_shared<implementation>(std::move(aStore), aSource, std::move(aGit), aEffects, aDecisions, aPort, aSeed, std::move(aNow), std::move(aMatcher)) }
	{
	}

	local_draft_workbench::~local_draft_workbench()
	{
		shutdown();
	}

	void local_draft_workbench::refresh_home()
	{
		Implementation->refresh_home();
	}

	void local_draft_workbench::open_location(std::string aLocation, std::optional<std::string> aContext)
	{
		Implementation->open_location(std::move(aLocation), std::move(aContext));
	}

	void local_draft_workbench::resume(core::local_draft_id aId)
	{
		Implementation->resume(std::move(aId));
	}

	void local_draft_workbench::choose_target(core::local_draft_target aTarget)
	{
		Implementation->choose_target(std::move(aTarget));
	}

	void local_draft_workbench::include(std::string aPath, const bool aIncluded)
	{
		Implementation->include(std::move(aPath), aIncluded);
	}

	void local_draft_workbench::refresh()
	{
		Implementation->refresh();
	}

	void local_draft_workbench::show_candidate(const std::uint64_t aOrdinal)
	{
		auto& self = *Implementation;
		if (!self.Projection.Draft || find_candidate(*self.Projection.Draft, aOrdinal) == nullptr || self.Projection.Shown == aOrdinal) {
			return;
		}
		self.Projection.Shown = aOrdinal;
		self.Projection.File.reset();
		self.update_refresh();
		self.publish();
		self.open_first_file();
	}

	void local_draft_workbench::open_file(std::string aPath)
	{
		Implementation->open_file(std::move(aPath));
	}

	void local_draft_workbench::add_note(std::string aText, std::string aPath)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.write(core::add_note(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aText), std::move(aPath), core::note_origin::author, self.Now()));
		}
	}

	void local_draft_workbench::remove_note(const std::uint64_t aNote)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.write(core::remove_note(*self.Projection.Draft, self.Projection.Draft->Version, aNote, self.Now()));
		}
	}

	void local_draft_workbench::rename(std::string aTitle)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft && aTitle != self.Projection.Draft->Title) {
			self.write(core::rename_draft(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aTitle), self.Now()));
		}
	}

	void local_draft_workbench::rebind()
	{
		Implementation->rebind();
	}

	void local_draft_workbench::discard()
	{
		Implementation->discard();
	}

	void local_draft_workbench::close()
	{
		Implementation->close();
	}

	auto local_draft_workbench::projection() const noexcept -> const local_draft_workbench_projection&
	{
		return Implementation->Projection;
	}

	auto local_draft_workbench::home() const noexcept -> const local_draft_home_projection&
	{
		return Implementation->Home;
	}

	void local_draft_workbench::shutdown()
	{
		if (Implementation && !Implementation->Stopped) {
			Implementation->Stopped = true;
			Implementation->Stop.request_stop();
		}
	}

	auto local_draft_workbench::is_quiescent() const noexcept -> bool
	{
		return Implementation->Pending == 0U && Implementation->HomePending == 0U;
	}
}
