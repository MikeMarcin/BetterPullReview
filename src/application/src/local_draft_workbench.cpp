#include "bpr/application/local_draft_workbench.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/hex.hpp"
#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <utility>

namespace bpr::application {
	namespace {
		/// RFC-BPR-0111. A failed GitHub call, in the author's words.
		[[nodiscard]] auto publication_failure_words(const github_publication_error& aError) -> std::string
		{
			const auto sent = aError.Transmission == github_publication_transmission::indeterminate;
			switch (aError.Failure) {
			case github_publication_failure::unsupported_host: return "This connection cannot create pull requests on this host.";
			case github_publication_failure::refused:
				return "GitHub refused it" + (aError.HttpStatus != 0U ? " (HTTP " + std::to_string(aError.HttpStatus) + ")" : std::string{ }) + ".";
			case github_publication_failure::rate_limited: return "GitHub's rate limit for this account is spent.";
			case github_publication_failure::mismatch:     return "GitHub answered about something else.";
			case github_publication_failure::too_large:    return "The description is larger than GitHub takes.";
			case github_publication_failure::cancelled:    return sent ? "Stopped after sending: it may have reached GitHub." : "Stopped. Nothing was sent.";
			case github_publication_failure::unavailable:  break;
			}
			return sent ? "GitHub did not answer after it was sent: it may have reached GitHub." : "GitHub did not answer. Nothing was sent.";
		}

		/// RFC-BPR-0110. The captured candidate's files as the private store
		/// holds them (or as the capture just made them), never the worktree:
		/// `aFirst` first, then the rest, until `aBudget` bytes of text are
		/// read. With `aOnly`, just those paths.
		[[nodiscard]] auto read_candidate_files(local_draft_store& aStore, const core::local_draft_candidate& aCandidate, const std::span<const std::string> aOnly,
												const std::span<const local_draft_object> aFresh, const std::string_view aFirst, std::size_t aBudget)
			-> std::vector<core::local_file_reading>
		{
			auto order = std::vector<const core::draft_path_change*>{ };
			for (const auto& change : aCandidate.Changes) {
				if (!aOnly.empty() && std::ranges::find(aOnly, change.Path) == aOnly.end()) {
					continue;
				}
				if (change.Path == aFirst) {
					order.insert(order.begin(), &change);
				} else {
					order.push_back(&change);
				}
			}
			auto readings = std::vector<core::local_file_reading>{ };
			for (const auto* change : order) {
				auto reading = core::local_file_reading{ .Path = change->Path, .Deleted = !change->Right };
				if (change->Right && change->Right->Size <= aBudget) {
					auto bytes = std::optional<std::vector<std::uint8_t>>{ };
					if (const auto fresh = std::ranges::find(aFresh, change->Right->Object, &local_draft_object::Object); fresh != aFresh.end()) {
						bytes = fresh->Bytes;
					} else if (auto stored = aStore.object(change->Right->Object); stored && *stored) {
						bytes = std::move(**stored);
					}
					if (bytes) {
						auto text = std::string{ bytes->begin(), bytes->end() };
						if (text.find('\0') == std::string::npos && core::utf8::valid(text)) {
							aBudget -= text.size();
							reading.Text = std::move(text);
						}
					}
				}
				readings.push_back(std::move(reading));
			}
			return readings;
		}

		/// The bytes of a question's reading: enough for any request.
		constexpr std::size_t question_read_budget = 2U * 1024U * 1024U;
		/// New draft identities come from the composition's seed.
		[[nodiscard]] auto seeded(const std::array<std::uint8_t, 32>& aSeed) -> std::mt19937_64
		{
			auto sequence = std::seed_seq(aSeed.begin(), aSeed.end());
			return std::mt19937_64{ sequence };
		}

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
			, Random{ seeded(aSeed) }
		{
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
				Projection.AgentAvailable = AgentRunner && AgentLane != nullptr;
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
			Projection.Publishing.AccountAvailable = account().has_value();
			project_plan();
			publish();
			open_first_file();
			refresh_home();
			settle_after_restart();
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
					aSelf.project_plan();
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
					// RFC-BPR-0110: every stop's lines on the successor, from its
					// captured bytes; the stops' own bindings never move.
					if (!next->Preparation.Tour.Stops.empty()) {
						auto paths = std::vector<std::string>{ };
						for (const auto& stop : next->Preparation.Tour.Stops) {
							paths.push_back(stop.Source.Path);
						}
						const auto readings = read_candidate_files(*stored, next->Candidates.back(), paths, capture->Objects, { }, core::maximum_local_draft_candidate_bytes);
						*next               = core::carry_local_tour(*next, readings);
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
						if (!bytes) {
							return std::nullopt;
						}
						return std::move(*bytes);
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

		// --- evidence (RFC-BPR-0109) ------------------------------------------

		void edit(std::expected<core::local_draft, core::draft_refusal> aChanged)
		{
			if (!Projection.Draft) {
				return;
			}
			if (!aChanged) {
				Projection.EvidenceRefusal = aChanged.error();
				publish();
				return;
			}
			Projection.EvidenceRefusal.reset();
			write(std::move(aChanged));
		}

		void import_evidence(std::string aDocument)
		{
			if (!Projection.Draft || Projection.Importing) {
				return;
			}
			ImportStop             = std::stop_source{ };
			Projection.Importing   = true;
			Projection.Imported    = std::nullopt;
			Projection.ImportError = std::nullopt;
			publish();
			run(
				[id = Projection.Draft->Id, document = std::move(aDocument), cancel = ImportStop.get_token()](implementation& aSelf, const std::stop_token aStop)
					-> std::expected<evidence_import_summary, evidence_import_error> {
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						return std::unexpected{ evidence_import_error{ .Failure = evidence_import_failure::store, .Store = local_draft_store_error{ } } };
					}
					// Closing the draft, or shutting down, stops the read.
					auto       either   = std::stop_source{ };
					const auto onStop   = std::stop_callback{ aStop, [&either] { either.request_stop(); } };
					const auto onCancel = std::stop_callback{ cancel, [&either] { either.request_stop(); } };
					return import_local_draft_evidence(*stored, aSelf.Source, id, document, aSelf.Now(), either.get_token());
				},
				[](implementation& aSelf, std::expected<evidence_import_summary, evidence_import_error> aResult) {
					auto& view     = aSelf.Projection;
					view.Importing = false;
					if (!aResult) {
						view.ImportError = aResult.error();
						aSelf.publish();
						return;
					}
					view.Imported = local_draft_import_report{ .Claims    = aResult->Claims,
															   .Checks    = aResult->Checks,
															   .Results   = aResult->Results,
															   .Questions = aResult->Questions,
															   .Bound     = aResult->Bound,
															   .Stored    = aResult->Stored,
															   .Unstored  = aResult->Unstored };
					view.Draft    = std::move(aResult->Draft);
					aSelf.update_refresh();
					aSelf.publish();
					aSelf.refresh_home();
				});
		}

		// --- RFC-BPR-0110: the description, the tour, conversations ---------

		void prepare(core::preparation_result<core::local_draft> aChanged)
		{
			if (!Projection.Draft) {
				return;
			}
			if (!aChanged) {
				Projection.PreparationRefusal = aChanged.error();
				publish();
				return;
			}
			Projection.PreparationRefusal.reset();
			write(std::expected<core::local_draft, core::draft_refusal>{ std::move(*aChanged) });
		}

		/// Reads one path of the latest candidate, then applies `aEdit` to the
		/// draft as it is then.
		template <typename TEdit>
		void with_reading(std::string aPath, TEdit aEdit)
		{
			const auto* latest = Projection.Draft ? core::latest_candidate(*Projection.Draft) : nullptr;
			if (latest == nullptr) {
				return;
			}
			run(
				[candidate = *latest, path = std::move(aPath)](implementation& aSelf, std::stop_token) -> core::local_file_reading {
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						return { .Path = path };
					}
					auto only     = std::array{ path };
					auto readings = read_candidate_files(*stored, candidate, only, { }, path, core::maximum_local_draft_file_bytes);
					return readings.empty() ? core::local_file_reading{ .Path = path } : std::move(readings.front());
				},
				[edit = std::move(aEdit)](implementation& aSelf, core::local_file_reading aReading) mutable {
					if (aSelf.Projection.Draft) {
						aSelf.prepare(edit(*aSelf.Projection.Draft, aReading));
					}
				});
		}

		void ask(local_draft_question aQuestion)
		{
			const auto* latest = Projection.Draft ? core::latest_candidate(*Projection.Draft) : nullptr;
			if (latest == nullptr || Projection.Asking || !AgentRunner || AgentLane == nullptr || aQuestion.Text.empty()) {
				return;
			}
			Projection.Asking       = true;
			Projection.AskingThread = aQuestion.Thread;
			Projection.AskingText   = aQuestion.Text;
			Projection.AskFailure.clear();
			publish();
			AskStop = std::stop_source{ };
			struct prepared
			{
				std::optional<core::local_request>        Request;
				std::vector<core::local_file_reading>     Files;
				std::optional<core::local_source_binding> Lines;
			};
			const auto first = !aQuestion.LinesPath.empty() ? aQuestion.LinesPath : std::string{ };
			run(
				[draft = *Projection.Draft, candidate = *latest, question = aQuestion, first](implementation& aSelf, std::stop_token) -> prepared {
					auto  out    = prepared{ };
					auto* stored = aSelf.store();
					if (stored == nullptr) {
						return out;
					}
					auto preferred = first;
					if (preferred.empty() && question.Stop != 0U) {
						if (const auto stop = std::ranges::find(draft.Preparation.Tour.Stops, question.Stop, &core::local_tour_stop::Id); stop != draft.Preparation.Tour.Stops.end()) {
							preferred = stop->Source.Path;
						}
					}
					out.Files  = read_candidate_files(*stored, candidate, { }, { }, preferred, question_read_budget);
					auto local = core::local_question{ .Subject      = question.Subject,
													   .Thread       = question.Thread,
													   .Stop         = question.Stop,
													   .Text         = question.Text,
													   .Instructions = question.Instructions,
													   .Version      = question.Version };
					if (!question.LinesPath.empty()) {
						const auto file = std::ranges::find(out.Files, question.LinesPath, &core::local_file_reading::Path);
						if (file != out.Files.end() && file->Text) {
							if (auto excerpt = core::local_excerpt(*file->Text, question.FirstLine, question.LastLine)) {
								out.Lines   = core::local_source_binding{ .Candidate = candidate.Ordinal,
																		  .Path      = question.LinesPath,
																		  .FirstLine = question.FirstLine,
																		  .LastLine  = question.LastLine,
																		  .Excerpt   = std::move(*excerpt) };
								local.Lines = out.Lines;
							}
						}
					}
					out.Request = core::compose_local_request(draft, local, core::current_conversation(draft, question.Thread, question.Agent), out.Files);
					return out;
				},
				// Both lanes read their own copy: argument order is unspecified.
				[question = aQuestion, ordinal = latest->Ordinal](implementation& aSelf, prepared aPrepared) mutable {
					auto& view = aSelf.Projection;
					if (!aPrepared.Request) {
						view.Asking     = false;
						view.AskFailure = "This question and its instructions are longer than one request can carry.";
						aSelf.publish();
						return;
					}
					view.Coverage = local_draft_request_coverage{ .FilesSent  = aPrepared.Request->FilesSent,
																  .FilesTotal = aPrepared.Request->FilesTotal,
																  .Replayed   = aPrepared.Request->Replayed,
																  .Omitted    = aPrepared.Request->Omitted };
					aSelf.publish();
					auto owner = aSelf.shared_from_this();
					auto made  = local_draft_agent_run{ .Agent = question.Agent, .AgentName = question.AgentName, .Profile = question.Profile, .Prompt = std::move(aPrepared.Request->Text) };
					++aSelf.AgentPending;
					aSelf.AgentLane->post([owner, asked = aSelf.Generation, ordinal, launch = std::move(made), sent = std::move(question), read = std::move(aPrepared.Files),
										   runner = aSelf.AgentRunner, stop = aSelf.AskStop.get_token()]() mutable {
						auto reply = runner(launch, stop);
						owner->Decisions.post([host = owner, asked, ordinal, answer = std::move(reply), posed = std::move(sent), files = std::move(read)]() mutable {
							--host->AgentPending;
							if (host->Stopped) {
								return;
							}
							host->answered(asked, ordinal, std::move(posed), std::move(files), std::move(answer));
						});
					});
				});
		}

		void answered(const std::uint64_t aGeneration, const std::uint64_t aOrdinal, local_draft_question aQuestion, std::vector<core::local_file_reading> aFiles,
					  local_draft_agent_answer aAnswer)
		{
			auto& view  = Projection;
			view.Asking = false;
			view.AskingThread.clear();
			view.AskingText.clear();
			if (aGeneration != Generation || !view.Draft) {
				publish();
				return;
			}
			const auto* latest = core::latest_candidate(*view.Draft);
			if (latest == nullptr || latest->Ordinal != aOrdinal) {
				view.AskFailure = "The draft was refreshed while the agent answered. Ask again on this candidate.";
				publish();
				return;
			}
			if (!aAnswer.Answered) {
				view.AskFailure = aAnswer.Failure.empty() ? std::string{ "The agent did not answer." } : aAnswer.Failure;
				publish();
				return;
			}
			const auto now   = Now();
			auto       asked = core::append_turn(*view.Draft, view.Draft->Version, aQuestion.Thread, aQuestion.Agent, aQuestion.AgentName,
												 core::local_turn{ .Author = true, .Subject = aQuestion.Subject, .Text = aQuestion.Text }, now);
			if (!asked) {
				prepare(std::unexpected{ asked.error() });
				return;
			}
			auto [draft, conversation] = std::move(*asked);
			const auto parsed          = core::parse_local_answer(aAnswer.Text);
			auto       proposals       = std::vector<std::uint64_t>{ };
			if (!parsed.Proposals.empty()) {
				auto recorded = core::record_proposals(draft, draft.Version, conversation, aOrdinal, parsed.Proposals, aFiles, aQuestion.AgentName, aQuestion.Version, now);
				if (recorded) {
					draft     = std::move(recorded->first);
					proposals = std::move(recorded->second);
				}
			}
			auto reply = core::append_turn(draft, draft.Version, aQuestion.Thread, aQuestion.Agent, aQuestion.AgentName,
										   core::local_turn{ .Author     = false,
															 .Subject    = aQuestion.Subject,
															 .Text       = parsed.Prose.size() > core::maximum_local_turn_bytes ? parsed.Prose.substr(0U, core::maximum_local_turn_bytes) : parsed.Prose,
															 .Proposals  = std::move(proposals),
															 .CostMicros = aAnswer.CostMicros },
										   now);
			if (!reply) {
				prepare(std::unexpected{ reply.error() });
				return;
			}
			prepare(std::move(reply->first));
		}

		void open_artifact(const std::uint64_t aResult, const std::size_t aIndex)
		{
			if (!Projection.Draft) {
				return;
			}
			const auto& results = Projection.Draft->Evidence.Results;
			const auto  result  = std::ranges::find(results, aResult, &core::evidence_result::Id);
			if (result == results.end() || aIndex >= result->Artifacts.size()) {
				return;
			}
			const auto& artifact = result->Artifacts[aIndex];
			Projection.Artifact  = local_draft_artifact_view{ .Result = aResult, .Index = aIndex, .Name = artifact.Name, .Size = artifact.Size };
			if (!artifact.Object) {
				Projection.Artifact->Status = review_material_status::missing;
				publish();
				return;
			}
			Projection.Artifact->Reading = true;
			publish();
			run(
				[object = *artifact.Object, view = *Projection.Artifact](implementation& aSelf, std::stop_token) mutable -> local_draft_artifact_view {
					view.Reading = false;
					auto* stored = aSelf.store();
					auto  bytes  = stored == nullptr ? local_draft_store_result<std::optional<std::vector<std::uint8_t>>>{ std::unexpected{ local_draft_store_error{ } } }
													 : stored->object(object);
					if (!bytes) {
						view.Status = review_material_status::failure;
						return view;
					}
					if (!*bytes) {
						view.Status = review_material_status::missing;
						return view;
					}
					const auto& data = **bytes;
					view.Status      = review_material_status::available;
					view.Size        = data.size();
					auto shown       = std::string_view{ reinterpret_cast<const char*>(data.data()), std::min(data.size(), maximum_local_draft_artifact_view_bytes) };
					view.Truncated   = shown.size() < data.size();
					// A cut through a character is moved back to its start.
					for (auto trim = 0; trim < 3 && view.Truncated && !core::utf8::valid(shown); ++trim) {
						shown.remove_suffix(1U);
					}
					view.Binary = shown.find('\0') != std::string_view::npos || !core::utf8::valid(shown);
					if (!view.Binary) {
						view.Text = std::string{ shown };
					}
					return view;
				},
				[](implementation& aSelf, local_draft_artifact_view aView) {
					auto& shown = aSelf.Projection.Artifact;
					if (shown && shown->Result == aView.Result && shown->Index == aView.Index) {
						shown = std::move(aView);
						aSelf.publish();
					}
				});
		}

		// --- RFC-BPR-0111: Create draft PR… ------------------------------------

		/// The store, one call at a time under the effect lane's lock, for
		/// the publication lane: GitHub is never called under the lock.
		class locked_store final : public local_draft_store
		{
		public:
			explicit locked_store(implementation& aSelf)
				: Self{ aSelf }
			{
			}
			auto list() -> local_draft_store_result<std::vector<core::local_draft>> override
			{
				const auto lock = std::scoped_lock{ Self.EffectMutex };
				auto*      held = Self.store();
				return held == nullptr ? std::unexpected{ local_draft_store_error{ } } : held->list();
			}
			auto find(const core::local_draft_id& aId) -> local_draft_store_result<core::local_draft> override
			{
				const auto lock = std::scoped_lock{ Self.EffectMutex };
				auto*      held = Self.store();
				return held == nullptr ? local_draft_store_result<core::local_draft>{ std::unexpected{ local_draft_store_error{ } } } : held->find(aId);
			}
			auto put(const core::local_draft& aDraft, std::optional<std::uint64_t> aPrevious, std::span<const local_draft_object> aObjects) -> local_draft_store_result<void> override
			{
				const auto lock = std::scoped_lock{ Self.EffectMutex };
				auto*      held = Self.store();
				return held == nullptr ? local_draft_store_result<void>{ std::unexpected{ local_draft_store_error{ } } } : held->put(aDraft, aPrevious, aObjects);
			}
			auto remove(const core::local_draft_id& aId) -> local_draft_store_result<void> override
			{
				const auto lock = std::scoped_lock{ Self.EffectMutex };
				auto*      held = Self.store();
				return held == nullptr ? local_draft_store_result<void>{ std::unexpected{ local_draft_store_error{ } } } : held->remove(aId);
			}
			auto object(const core::git_entry_oid& aObject) -> local_draft_store_result<std::optional<std::vector<std::uint8_t>>> override
			{
				const auto lock = std::scoped_lock{ Self.EffectMutex };
				auto*      held = Self.store();
				return held == nullptr ? local_draft_store_result<std::optional<std::vector<std::uint8_t>>>{ std::unexpected{ local_draft_store_error{ } } } : held->object(aObject);
			}

		private:
			implementation& Self;
		};

		[[nodiscard]] auto account() const -> std::optional<github_publication_account> { return Account ? Account() : std::nullopt; }

		/// The recorded plan in words, and the digest Create confirms.
		void project_plan()
		{
			auto& view = Projection.Publishing;
			view.Preview.clear();
			view.Digest.clear();
			if (Projection.Draft && Projection.Draft->Publication) {
				const auto& publication = *Projection.Draft->Publication;
				view.Preview            = core::render_local_publication_preview(publication.Plan);
				view.Digest             = publication.Journal.Digest;
			}
		}

		template <typename TEffect>
		void publishing(TEffect aEffect, const bool aCreating)
		{
			if (PublishLane == nullptr || !Projection.Draft || Projection.Publishing.Checking || Projection.Publishing.Creating) {
				return;
			}
			auto& view = Projection.Publishing;
			(aCreating ? view.Creating : view.Checking) = true;
			view.Failure.clear();
			publish();
			auto self = shared_from_this();
			++PublishPending;
			PublishLane->post([self, effect = std::move(aEffect), draft = *Projection.Draft, generation = Generation, stop = PublishStop.get_token()]() mutable {
				auto store = locked_store{ *self };
				auto step  = effect(store, draft, self->Now(), stop);
				self->Decisions.post([host = self, result = std::move(step), generation]() mutable {
					--host->PublishPending;
					if (host->Stopped) {
						return;
					}
					host->published(generation, std::move(result));
				});
			});
		}

		void published(const std::uint64_t aGeneration, local_publication_step aStep)
		{
			auto& view    = Projection.Publishing;
			view.Checking = false;
			view.Creating = false;
			if (aGeneration != Generation) {
				publish();
				return;
			}
			view.Checked          = true;
			view.Branch           = aStep.Branch;
			view.Push             = std::move(aStep.Push);
			view.Refusal          = aStep.Refusal;
			view.Existing         = std::move(aStep.Existing);
			view.AccountAvailable = account().has_value();
			view.Failure          = aStep.Provider ? publication_failure_words(*aStep.Provider) : std::string{ };
			if (aStep.Store) {
				Projection.Store  = aStep.Store;
				Projection.Reread = aStep.Store->Failure == local_draft_store_failure::stale;
			}
			if (aStep.Draft) {
				Projection.Draft = std::move(aStep.Draft);
			}
			project_plan();
			publish();
			refresh_home();
		}

		void check_publication(const bool aTour)
		{
			auto signedIn = account();
			publishing(
				[chosen = std::move(signedIn), aTour](local_draft_store& aStore, const core::local_draft& aDraft, const std::int64_t aNow, std::stop_token aStop) {
					return check_local_publication(aStore, aDraft, chosen ? &*chosen : nullptr, aTour, aNow, aStop);
				},
				false);
		}

		void create_pull_request(std::string aDigest)
		{
			auto signedIn = account();
			if (!signedIn) {
				Projection.Publishing.AccountAvailable = false;
				Projection.Publishing.Refusal          = core::local_publication_refusal::no_account;
				publish();
				return;
			}
			publishing(
				[chosen = std::move(*signedIn), digest = std::move(aDigest)](local_draft_store& aStore, const core::local_draft& aDraft, const std::int64_t aNow, std::stop_token aStop) {
					return create_local_pull_request(aStore, aDraft, chosen, digest, aNow, aStop);
				},
				true);
		}

		void recover_publication()
		{
			auto signedIn = account();
			if (!signedIn) {
				return;
			}
			publishing([chosen = std::move(*signedIn)](local_draft_store& aStore, const core::local_draft& aDraft, const std::int64_t aNow,
													 std::stop_token aStop) { return recover_local_publication(aStore, aDraft, chosen, aNow, aStop); },
					   true);
		}

		void associate_pull_request(const std::uint64_t aNumber)
		{
			auto signedIn = account();
			if (!signedIn) {
				return;
			}
			publishing([chosen = std::move(*signedIn), aNumber](local_draft_store& aStore, const core::local_draft& aDraft, const std::int64_t aNow,
															  std::stop_token aStop) { return associate_local_pull_request(aStore, aDraft, chosen, aNumber, aNow, aStop); },
					   true);
		}

		/// After a restart or a reopen: a creation recorded and never answered
		/// is uncertain, and no confirmation survives.
		void settle_after_restart()
		{
			if (!Projection.Draft || !Projection.Draft->Publication || PublishPending != 0U) {
				return;
			}
			const auto phase = Projection.Draft->Publication->Journal.Phase;
			if (phase == core::local_publication_phase::creating || phase == core::local_publication_phase::authorized) {
				write(core::restart_local_publication(*Projection.Draft));
			}
		}

		void close()
		{
			ImportStop.request_stop();
			AskStop.request_stop();
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
		/// The import being read; closing the draft stops it.
		std::stop_source ImportStop;
		/// RFC-BPR-0110. The agent runs: composed by the application, off both lanes.
		local_draft_agent_runner   AgentRunner;
		effect_executor*           AgentLane = nullptr;
		std::stop_source           AskStop;
		std::atomic<std::uint64_t> AgentPending{ 0U };
		/// RFC-BPR-0111. Create draft PR…: the account, asked on the decision
		/// lane, and the lane GitHub is called on.
		std::function<std::optional<github_publication_account>()> Account;
		effect_executor*                                           PublishLane = nullptr;
		std::stop_source                                           PublishStop;
		std::atomic<std::uint64_t>                                 PublishPending{ 0U };

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

	void local_draft_workbench::add_claim(const core::claim_kind aKind, std::string aText)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::add_claim(*self.Projection.Draft, self.Projection.Draft->Version, aKind, std::move(aText), self.Now()));
		}
	}

	void local_draft_workbench::revise_claim(const std::uint64_t aClaim, std::string aText)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::revise_claim(*self.Projection.Draft, self.Projection.Draft->Version, aClaim, std::move(aText), self.Now()));
		}
	}

	void local_draft_workbench::add_check(const core::check_method aMethod, std::string aTitle, std::string aCommand, std::vector<std::uint64_t> aClaims, core::formal_scope aFormal)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::add_check(*self.Projection.Draft, self.Projection.Draft->Version, aMethod, std::move(aTitle), std::move(aCommand), std::move(aClaims), std::move(aFormal),
									  self.Now()));
		}
	}

	void local_draft_workbench::record_result(core::manual_result aResult)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::record_result(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aResult), self.Now()));
		}
	}

	void local_draft_workbench::add_question(std::string aText, const std::uint64_t aClaim)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::add_question(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aText), aClaim, self.Now()));
		}
	}

	void local_draft_workbench::resolve_question(const std::uint64_t aQuestion, const bool aResolved)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::resolve_question(*self.Projection.Draft, self.Projection.Draft->Version, aQuestion, aResolved, self.Now()));
		}
	}

	void local_draft_workbench::decide_evidence(const std::uint64_t aRecord, const core::evidence_disposition aDisposition)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::decide_evidence(*self.Projection.Draft, self.Projection.Draft->Version, aRecord, aDisposition, self.Now()));
		}
	}

	void local_draft_workbench::select_evidence(const std::uint64_t aRecord, const bool aSelected)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.edit(core::select_evidence(*self.Projection.Draft, self.Projection.Draft->Version, aRecord, aSelected, self.Now()));
		}
	}

	void local_draft_workbench::save_description(std::string aTitle, std::string aBody)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::edit_description(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aTitle), std::move(aBody), self.Now()));
		}
	}

	void local_draft_workbench::save_brief(std::string aProblem, std::string aSolution)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::edit_tour_brief(*self.Projection.Draft, self.Projection.Draft->Version, std::move(aProblem), std::move(aSolution), self.Now()));
		}
	}

	void local_draft_workbench::transfer_to_description(std::string aPassage)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::transfer_to_description(*self.Projection.Draft, self.Projection.Draft->Version, aPassage, self.Now()));
		}
	}

	void local_draft_workbench::add_stop(std::string aPath, const std::uint32_t aFirst, const std::uint32_t aLast, std::string aTitle, std::string aText)
	{
		auto& self = *Implementation;
		self.with_reading(std::move(aPath), [aFirst, aLast, title = std::move(aTitle), text = std::move(aText), now = self.Now()](const core::local_draft& aDraft, const core::local_file_reading& aFile) mutable {
			return core::add_local_stop(aDraft, aDraft.Version, std::move(title), std::move(text), aFile, core::diff_side::right, aFirst, aLast, now);
		});
	}

	void local_draft_workbench::save_stop(const std::uint64_t aStop, std::string aTitle, std::string aText, std::string aPath, const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		auto& self = *Implementation;
		if (!self.Projection.Draft) {
			return;
		}
		if (aPath.empty()) {
			self.prepare(core::save_local_stop(*self.Projection.Draft, self.Projection.Draft->Version, aStop, std::move(aTitle), std::move(aText), std::nullopt, core::diff_side::right, 0U,
											   0U, self.Now()));
			return;
		}
		self.with_reading(std::move(aPath), [aStop, aFirst, aLast, title = std::move(aTitle), text = std::move(aText), now = self.Now()](const core::local_draft& aDraft, const core::local_file_reading& aFile) mutable {
			return core::save_local_stop(aDraft, aDraft.Version, aStop, std::move(title), std::move(text), aFile, core::diff_side::right, aFirst, aLast, now);
		});
	}

	void local_draft_workbench::accept_stop(const std::uint64_t aStop)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::accept_local_stop(*self.Projection.Draft, self.Projection.Draft->Version, aStop, self.Now()));
		}
	}

	void local_draft_workbench::remove_stop(const std::uint64_t aStop)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::remove_local_stop(*self.Projection.Draft, self.Projection.Draft->Version, aStop, self.Now()));
		}
	}

	void local_draft_workbench::move_stop(const std::uint64_t aStop, const std::size_t aIndex)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::move_local_stop(*self.Projection.Draft, self.Projection.Draft->Version, aStop, aIndex, self.Now()));
		}
	}

	void local_draft_workbench::cite(const std::uint64_t aStop, const std::uint64_t aRecord, const bool aCited)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::cite_evidence(*self.Projection.Draft, self.Projection.Draft->Version, aStop, aRecord, aCited, self.Now()));
		}
	}

	void local_draft_workbench::decide_proposal(const std::uint64_t aProposal, const bool aAccept)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(aAccept ? core::accept_proposal(*self.Projection.Draft, self.Projection.Draft->Version, aProposal, self.Now())
								 : core::reject_proposal(*self.Projection.Draft, self.Projection.Draft->Version, aProposal, self.Now()));
		}
	}

	void local_draft_workbench::set_agent(local_draft_agent_runner aRunner, effect_executor* const aLane)
	{
		auto& self                     = *Implementation;
		self.AgentRunner               = std::move(aRunner);
		self.AgentLane                 = aLane;
		self.Projection.AgentAvailable = self.AgentRunner && aLane != nullptr;
		self.publish();
	}

	void local_draft_workbench::ask(local_draft_question aQuestion)
	{
		Implementation->ask(std::move(aQuestion));
	}

	void local_draft_workbench::stop_asking()
	{
		Implementation->AskStop.request_stop();
	}

	void local_draft_workbench::new_session(const std::uint64_t aConversation)
	{
		auto& self = *Implementation;
		if (self.Projection.Draft) {
			self.prepare(core::end_conversation(*self.Projection.Draft, self.Projection.Draft->Version, aConversation, self.Now()));
		}
	}

	void local_draft_workbench::import_evidence(std::string aDocument)
	{
		Implementation->import_evidence(std::move(aDocument));
	}

	void local_draft_workbench::open_artifact(const std::uint64_t aResult, const std::size_t aIndex)
	{
		Implementation->open_artifact(aResult, aIndex);
	}

	void local_draft_workbench::close_artifact()
	{
		auto& self = *Implementation;
		if (self.Projection.Artifact) {
			self.Projection.Artifact.reset();
			self.publish();
		}
	}

	void local_draft_workbench::set_publishing(std::function<std::optional<github_publication_account>()> aAccount, effect_executor* const aLane)
	{
		auto& self                                = *Implementation;
		self.Account                              = std::move(aAccount);
		self.PublishLane                          = aLane;
		self.Projection.Publishing.AccountAvailable = self.account().has_value();
		self.publish();
	}

	void local_draft_workbench::check_publication(const bool aTour)
	{
		Implementation->check_publication(aTour);
	}

	void local_draft_workbench::create_pull_request(std::string aDigest)
	{
		Implementation->create_pull_request(std::move(aDigest));
	}

	void local_draft_workbench::recover_publication()
	{
		Implementation->recover_publication();
	}

	void local_draft_workbench::associate_pull_request(const std::uint64_t aNumber)
	{
		Implementation->associate_pull_request(aNumber);
	}

	void local_draft_workbench::cancel_publication()
	{
		auto& self = *Implementation;
		if (self.Projection.Draft && !self.Projection.Publishing.Creating) {
			auto changed = core::cancel_local_publication(*self.Projection.Draft, self.Projection.Draft->Version, self.Now());
			if (changed) {
				self.write(std::move(*changed));
			}
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
			Implementation->ImportStop.request_stop();
			Implementation->AskStop.request_stop();
			Implementation->PublishStop.request_stop();
		}
	}

	auto local_draft_workbench::is_quiescent() const noexcept -> bool
	{
		return Implementation->Pending == 0U && Implementation->HomePending == 0U && Implementation->AgentPending == 0U && Implementation->PublishPending == 0U;
	}
}
