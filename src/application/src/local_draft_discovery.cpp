#include "bpr/application/local_draft_discovery.hpp"

#include "bpr/application/review_cycle_git_metadata.hpp"
#include "bpr/core/review_cycle_ignore.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::uint64_t maximum_reference_bytes = 4096U;
		constexpr std::uint64_t maximum_ignore_bytes    = 1024U * 1024U;
		constexpr std::uint64_t maximum_index_entries   = 1'000'000U;
		constexpr std::uint64_t maximum_packed_refs     = 64U * 1024U * 1024U;
		/// Names a default branch is commonly given, offered when the clone
		/// holds them; never assumed.
		constexpr std::array usual_branches{ std::string_view{ "main" }, std::string_view{ "master" }, std::string_view{ "trunk" }, std::string_view{ "develop" } };

		[[nodiscard]] auto text_of(const std::vector<std::uint8_t>& aBytes) -> std::string
		{
			return { aBytes.begin(), aBytes.end() };
		}

		[[nodiscard]] auto opened(workspace_source_port& aSource, const std::string_view aLocation, const std::stop_token aStop)
			-> std::expected<std::unique_ptr<workspace_root_session>, local_draft_open_error>
		{
			auto session = aSource.open(aLocation, aStop);
			if (!session) {
				return std::unexpected{ local_draft_open_error{
					.Failure   = session.error() == workspace_failure::cancelled ? local_draft_open_failure::cancelled : local_draft_open_failure::workspace,
					.Workspace = session.error() } };
			}
			return std::move(*session);
		}

		[[nodiscard]] auto git_root(const core::workspace_root_observation& aRoot) noexcept -> bool
		{
			return (aRoot.Kind == core::workspace_root_kind::git_checkout || aRoot.Kind == core::workspace_root_kind::git_linked_worktree) && aRoot.Repository && aRoot.Worktree;
		}

		/// The location as a canonical path, for the chain's cycle check.
		[[nodiscard]] auto canonical_location(const std::string_view aLocation) -> std::string
		{
			std::error_code error;
			const auto      path = std::filesystem::weakly_canonical(std::filesystem::path{ aLocation }, error);
			return error ? std::string{ aLocation } : path.generic_string();
		}

		[[nodiscard]] auto remotes_of(workspace_root_session& aSession, const std::stop_token aStop) -> std::optional<std::vector<core::git_remote>>
		{
			auto config = aSession.read_git(workspace_git_area::common, "config", core::maximum_clone_config_bytes, aStop);
			if (!config) {
				return std::nullopt;
			}
			return core::parse_git_config_remotes(text_of(*config));
		}

		/// `refs/remotes/REMOTE/HEAD`'s branch, when it is a symbolic ref.
		[[nodiscard]] auto default_branch(workspace_root_session& aSession, const std::string& aRemote, const std::stop_token aStop) -> std::optional<std::string>
		{
			const auto prefix = "refs/remotes/" + aRemote + "/";
			auto       head   = aSession.read_git(workspace_git_area::common, prefix + "HEAD", maximum_reference_bytes, aStop);
			if (!head) {
				return std::nullopt;
			}
			auto text = text_of(*head);
			while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
				text.pop_back();
			}
			if (!text.starts_with("ref: " + prefix)) {
				return std::nullopt;
			}
			return text.substr(5U + prefix.size());
		}

		/// Remote-tracking branches named in `packed-refs` and the usual
		/// names the clone holds loose, for a choice.
		[[nodiscard]] auto branch_choices(workspace_root_session& aSession, const std::string& aRemote, const core::git_hash_algorithm aAlgorithm, const std::stop_token aStop)
			-> std::vector<std::string>
		{
			const auto prefix   = "refs/remotes/" + aRemote + "/";
			auto       branches = std::set<std::string>{ };
			if (const auto packed = aSession.read_git(workspace_git_area::common, "packed-refs", maximum_packed_refs, aStop)) {
				const auto text = text_of(*packed);
				auto       rest = std::string_view{ text };
				while (!rest.empty() && branches.size() < core::maximum_local_draft_branch_choices) {
					const auto end  = rest.find('\n');
					const auto line = rest.substr(0U, end);
					rest.remove_prefix(end == std::string_view::npos ? rest.size() : end + 1U);
					const auto space = line.find(' ');
					if (space != std::string_view::npos && line.substr(space + 1U).starts_with(prefix)) {
						const auto name = line.substr(space + 1U + prefix.size());
						if (name != "HEAD") {
							branches.emplace(name);
						}
					}
				}
			}
			for (const auto name : usual_branches) {
				if (resolve_reference(aSession, prefix + std::string{ name }, aAlgorithm, aStop)) {
					branches.emplace(name);
				}
			}
			return { branches.begin(), branches.end() };
		}

		/// Follows the remote chain from the checkout; returns the reached
		/// clone's session, or none when the chain was refused.
		[[nodiscard]] auto walk_chain(workspace_source_port& aSource, local_draft_discovery& aDiscovery, std::unique_ptr<workspace_root_session> aCheckout,
									  const std::stop_token aStop) -> std::unique_ptr<workspace_root_session>
		{
			auto walk     = core::remote_chain_walk{ };
			auto current  = std::move(aCheckout);
			auto location = canonical_location(aDiscovery.Root.Location);
			while (true) {
				const auto step = core::step_remote_chain(walk, location, remotes_of(*current, aStop));
				if (const auto* resolved = std::get_if<core::remote_chain_resolved>(&step)) {
					aDiscovery.Chain = *resolved;
					return current;
				}
				if (const auto* refused = std::get_if<core::remote_chain_refused>(&step)) {
					aDiscovery.ChainRefusal = *refused;
					return nullptr;
				}
				const auto& follow = std::get<core::remote_chain_follow>(step);
				auto        path   = std::filesystem::path{ follow.Path };
				if (path.is_relative()) {
					path = std::filesystem::path{ follow.From } / path;
				}
				location  = canonical_location(path.lexically_normal().generic_string());
				auto next = aSource.open(location, aStop);
				if (!next) {
					(void)core::step_remote_chain(walk, location, std::nullopt);
					aDiscovery.ChainRefusal = core::remote_chain_refused{ .Reason = core::remote_chain_refusal::unreadable, .Location = location };
					return nullptr;
				}
				walk.Hops.push_back({ .Remote = follow.Remote, .Location = location });
				current = std::move(*next);
			}
		}

		/// The tip, comparison and committed change for the chosen target.
		void compare(local_draft_discovery& aDiscovery, workspace_root_session* aReached, local_draft_git_port& aGit, const std::stop_token aStop)
		{
			aDiscovery.TargetTip.reset();
			aDiscovery.Comparison = { };
			aDiscovery.BaseEntries.clear();
			aDiscovery.Committed.clear();
			aDiscovery.Git.reset();
			if (aReached == nullptr || !aDiscovery.Target || !aDiscovery.Head) {
				return;
			}
			const auto& target = aDiscovery.Target->Target;
			const auto  tip    = resolve_reference(*aReached, "refs/remotes/" + target.Remote + "/" + target.Branch, aDiscovery.Algorithm, aStop);
			if (!tip) {
				return;
			}
			aDiscovery.TargetTip = *tip;
			const auto repository = aDiscovery.repository();
			auto       bases      = aGit.merge_bases(repository, *aDiscovery.Head, *tip, aStop);
			if (!bases) {
				// A tip the objects lack is unavailable, not a failed comparison.
				if (bases.error().Failure != publication_git_failure::object_unavailable) {
					aDiscovery.Git = bases.error();
				}
				return;
			}
			aDiscovery.Comparison = core::decide_comparison(*bases);
			if (aDiscovery.Comparison.State != core::comparison_state::ready) {
				return;
			}
			auto base = aGit.list_tree_of(repository, *aDiscovery.Comparison.Base, aStop);
			if (!base) {
				aDiscovery.Git        = base.error();
				aDiscovery.Comparison = { };
				return;
			}
			auto head = aGit.list_tree_of(repository, *aDiscovery.Head, aStop);
			if (!head) {
				aDiscovery.Git        = head.error();
				aDiscovery.Comparison = { };
				return;
			}
			aDiscovery.Committed   = core::compare_trees(base->Entries, head->Entries);
			aDiscovery.BaseEntries = std::move(base->Entries);
		}

		/// One file's blob identity as Git would compute it from these bytes.
		[[nodiscard]] auto hash_file(workspace_root_session& aSession, const std::string& aPath, const workspace_entry_observation& aEntry, const core::git_hash_algorithm aAlgorithm,
									 std::uint64_t& aBudget, const std::stop_token aStop) -> std::optional<core::git_entry_oid>
		{
			if (aEntry.Kind == workspace_entry_kind::symlink) {
				const auto target = aSession.read_link(aPath, aStop);
				if (!target) {
					return std::nullopt;
				}
				const auto blob = core::git_blob_identity(std::span{ reinterpret_cast<const std::uint8_t*>(target->data()), target->size() }, aAlgorithm);
				return blob ? std::optional{ core::retag<core::git_entry_oid_kind>(*blob) } : std::nullopt;
			}
			if (aEntry.Size > maximum_local_draft_hashed_file || aEntry.Size > aBudget) {
				return std::nullopt;
			}
			aBudget -= aEntry.Size;
			auto hasher = core::git_object_hasher{ aAlgorithm, core::git_object_type::blob, aEntry.Size };
			const auto read = aSession.read(aPath, aEntry.Size, [&hasher](const std::span<const std::uint8_t> aChunk) {
				hasher.update(aChunk);
				return true;
			}, aStop);
			if (!read || read->BytesRead != aEntry.Size) {
				return std::nullopt;
			}
			const auto digest = hasher.finish();
			if (!digest) {
				return std::nullopt;
			}
			return core::git_entry_oid{ aAlgorithm, *digest };
		}

		[[nodiscard]] auto mode_of(const workspace_entry_observation& aEntry) noexcept -> std::uint32_t
		{
			if (aEntry.Kind == workspace_entry_kind::symlink) {
				return 0120000U;
			}
			return aEntry.Executable.value_or(false) ? 0100755U : 0100644U;
		}

		struct scan_state
		{
			std::map<std::string, const core::git_tree_entry*> Tracked;
			std::set<std::string>                              TrackedDirectories;
			std::set<std::string>                              Sparse;
			std::set<std::string>                              Seen;
			core::workspace_ignore_rules                       Ignore;
			core::workspace_attribute_rules                    Attributes;
			std::uint64_t                                      Entries = 0;
			std::uint64_t                                      Budget  = maximum_local_draft_scan_bytes;
		};

		void scan_directory(workspace_root_session& aSession, local_draft_discovery& aDiscovery, scan_state& aState, const std::string& aDirectory, const std::stop_token aStop)
		{
			const auto listing = aSession.list(aDirectory, aStop);
			if (!listing) {
				aDiscovery.ScanIncomplete = true;
				return;
			}
			const auto child = [&aDirectory](const std::string& aName) { return aDirectory.empty() ? aName : aDirectory + "/" + aName; };
			for (const auto& entry : *listing) {
				if (entry.Kind == workspace_entry_kind::file && (entry.Name == ".gitignore" || entry.Name == ".gitattributes") && entry.Size <= maximum_ignore_bytes) {
					auto bytes = std::vector<std::uint8_t>{ };
					if (aSession.read(child(entry.Name), maximum_ignore_bytes, [&bytes](const std::span<const std::uint8_t> aChunk) {
							bytes.insert(bytes.end(), aChunk.begin(), aChunk.end());
							return true;
						}, aStop)) {
						if (entry.Name == ".gitignore") {
							(void)aState.Ignore.add(aDirectory, text_of(bytes));
						} else {
							(void)aState.Attributes.add(aDirectory, text_of(bytes));
						}
					}
				}
			}
			for (const auto& entry : *listing) {
				if (aStop.stop_requested() || ++aState.Entries > maximum_local_draft_scan_entries) {
					aDiscovery.ScanIncomplete = true;
					return;
				}
				const auto path    = child(entry.Name);
				const auto tracked = aState.Tracked.find(path);
				if (entry.Kind == workspace_entry_kind::directory) {
					// A tracked submodule is not this repository's material.
					if (tracked != aState.Tracked.end() || entry.Name == ".git") {
						aState.Seen.insert(path);
						continue;
					}
					if (aState.Ignore.ignored(path, true) && !aState.TrackedDirectories.contains(path)) {
						continue;
					}
					scan_directory(aSession, aDiscovery, aState, path, aStop);
					continue;
				}
				if (tracked != aState.Tracked.end()) {
					aState.Seen.insert(path);
				}
				if (tracked == aState.Tracked.end() && aState.Ignore.ignored(path, false)) {
					continue;
				}
				const auto filtered = aState.Attributes.filter(path).Kind != core::git_attribute_state::kind::unspecified
					&& aState.Attributes.filter(path).Kind != core::git_attribute_state::kind::unset;
				auto file = core::working_file{ .Path = path, .Edit = tracked == aState.Tracked.end() ? core::working_edit::untracked : core::working_edit::modified,
												.Mode = mode_of(entry), .Size = entry.Size };
				if (entry.Kind != workspace_entry_kind::file && entry.Kind != workspace_entry_kind::symlink) {
					file.Qualification = core::working_qualification::unsupported;
				} else if (filtered) {
					if (tracked != aState.Tracked.end()) {
						++aDiscovery.FilteredUncompared;
						continue;
					}
					file.Qualification = core::working_qualification::filtered;
				} else {
					file.Object = hash_file(aSession, path, entry, aDiscovery.Algorithm, aState.Budget, aStop);
					if (!file.Object) {
						file.Qualification = core::working_qualification::too_large;
					}
				}
				if (tracked != aState.Tracked.end() && file.Object && *file.Object == tracked->second->Object && file.Mode == tracked->second->Mode) {
					continue;
				}
				if (aDiscovery.Working.size() >= maximum_local_draft_listed_edits) {
					aDiscovery.ScanIncomplete = true;
					return;
				}
				aDiscovery.Working.push_back(std::move(file));
			}
		}

		/// Working-tree edits against HEAD's tree.
		void scan_working_tree(workspace_root_session& aSession, local_draft_discovery& aDiscovery, std::span<const core::git_tree_entry> aHead, const bool aIgnoreCase,
							   const std::stop_token aStop)
		{
			auto state = scan_state{ .Ignore = core::workspace_ignore_rules{ aIgnoreCase }, .Attributes = core::workspace_attribute_rules{ aIgnoreCase } };
			for (const auto& entry : aHead) {
				state.Tracked.emplace(entry.Path, &entry);
				for (auto slash = entry.Path.find('/'); slash != std::string::npos; slash = entry.Path.find('/', slash + 1U)) {
					state.TrackedDirectories.insert(entry.Path.substr(0U, slash));
				}
			}
			if (const auto exclude = aSession.read_git(workspace_git_area::common, "info/exclude", maximum_ignore_bytes, aStop)) {
				(void)state.Ignore.add("", text_of(*exclude));
			}
			if (const auto attributes = aSession.read_git(workspace_git_area::common, "info/attributes", maximum_ignore_bytes, aStop)) {
				(void)state.Attributes.add_info(text_of(*attributes));
			}
			if (const auto index = aSession.read_git(workspace_git_area::worktree, "index", maximum_workspace_metadata_bytes, aStop)) {
				if (const auto parsed = core::parse_git_index(*index, aDiscovery.Algorithm, maximum_index_entries)) {
					for (const auto& entry : parsed->Entries) {
						if (entry.SkipWorktree) {
							state.Sparse.insert(entry.Path);
						}
					}
					aDiscovery.Sparse = !state.Sparse.empty() || parsed->SparseDirectories;
				}
			}
			scan_directory(aSession, aDiscovery, state, "", aStop);
			if (aDiscovery.ScanIncomplete) {
				std::ranges::sort(aDiscovery.Working, { }, &core::working_file::Path);
				return;
			}
			for (const auto& entry : aHead) {
				if (entry.Mode == 0160000U || state.Seen.contains(entry.Path) || state.Sparse.contains(entry.Path)) {
					continue;
				}
				if (aDiscovery.Working.size() >= maximum_local_draft_listed_edits) {
					aDiscovery.ScanIncomplete = true;
					break;
				}
				aDiscovery.Working.push_back({ .Path = entry.Path, .Edit = core::working_edit::deleted, .Mode = entry.Mode });
			}
			std::ranges::sort(aDiscovery.Working, { }, &core::working_file::Path);
		}

		[[nodiscard]] auto mentions_autocrlf(const std::string& aConfig) -> bool
		{
			auto lower = aConfig;
			std::ranges::transform(lower, lower.begin(), [](const char aByte) { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; });
			const auto at = lower.find("autocrlf");
			if (at == std::string::npos) {
				return false;
			}
			const auto line = lower.substr(at, lower.find('\n', at) - at);
			return line.find("true") != std::string::npos || line.find("input") != std::string::npos;
		}

		[[nodiscard]] auto reached_location(const local_draft_discovery& aDiscovery) -> std::string
		{
			return aDiscovery.Chain && !aDiscovery.Chain->Hops.empty() ? aDiscovery.Chain->Hops.back().Location : aDiscovery.Root.Location;
		}
	}

	auto local_draft_discovery::repository() const -> local_draft_repository
	{
		auto repository = local_draft_repository{ .Checkout = Root.Location, .Algorithm = Algorithm };
		if (Chain) {
			for (const auto& hop : Chain->Hops) {
				repository.Borrowed.push_back(hop.Location);
			}
		}
		return repository;
	}

	auto worktree_of(const local_draft_discovery& aDiscovery) -> core::local_draft_worktree
	{
		return { .Location = aDiscovery.Root.Location, .Repository = aDiscovery.Root.Repository.value_or(core::workspace_object_identity{ }),
				 .Worktree = aDiscovery.Root.Worktree.value_or(core::workspace_object_identity{ }) };
	}

	auto discover_local_draft(workspace_source_port& aSource, local_draft_git_port& aGit, const std::string_view aLocation, const std::optional<core::chosen_target>& aPrior,
							  const std::stop_token aStop) -> std::expected<local_draft_discovery, local_draft_open_error>
	{
		auto checkout = opened(aSource, aLocation, aStop);
		if (!checkout) {
			return std::unexpected{ checkout.error() };
		}
		auto discovery = local_draft_discovery{ .Root = (*checkout)->observation() };
		if (!git_root(discovery.Root)) {
			return std::unexpected{ local_draft_open_error{ .Failure = local_draft_open_failure::not_git } };
		}
		const auto format = read_git_repository_format(**checkout, aStop);
		if (!format) {
			return std::unexpected{ local_draft_open_error{ .Failure = format.error().Repository ? local_draft_open_failure::repository : local_draft_open_failure::workspace,
															.Workspace = format.error().Workspace, .Repository = format.error().Repository } };
		}
		discovery.Algorithm = format->ObjectFormat;
		if (const auto config = (*checkout)->read_git(workspace_git_area::common, "config", core::maximum_clone_config_bytes, aStop)) {
			discovery.LineEndings = mentions_autocrlf(text_of(*config));
		}
		const auto head = read_git_head(**checkout, discovery.Algorithm, aStop);
		if (!head) {
			discovery.Source = core::source_state::unreadable;
		} else if (!head->Reference || !head->Reference->starts_with("refs/heads/")) {
			discovery.Source = core::source_state::detached;
			discovery.Head   = head->Commit;
		} else if (!head->Commit) {
			discovery.Source = core::source_state::unborn;
			discovery.Branch = head->Reference->substr(11U);
		} else {
			discovery.Source = core::source_state::branch;
			discovery.Branch = head->Reference->substr(11U);
			discovery.Head   = head->Commit;
		}

		// The working tree is scanned through the checkout's own session
		// before the chain walk takes it over.
		auto headEntries = std::vector<core::git_tree_entry>{ };
		if (discovery.Head) {
			auto listing = aGit.list_tree_of(discovery.repository(), *discovery.Head, aStop);
			if (!listing) {
				discovery.Git = listing.error();
			} else {
				headEntries = std::move(listing->Entries);
			}
		}
		if (discovery.Head && !discovery.Git) {
			scan_working_tree(**checkout, discovery, headEntries, format->IgnoreCase, aStop);
		}
		if (aStop.stop_requested()) {
			return std::unexpected{ local_draft_open_error{ .Failure = local_draft_open_failure::cancelled } };
		}

		auto reached = walk_chain(aSource, discovery, std::move(*checkout), aStop);
		if (reached != nullptr) {
			for (const auto& destination : discovery.Chain->Destinations) {
				discovery.Destinations.push_back({ .Destination = destination,
												   .DefaultBranch = default_branch(*reached, destination.Remote, aStop),
												   .Branches      = branch_choices(*reached, destination.Remote, discovery.Algorithm, aStop) });
			}
			auto chosen = core::choose_target(discovery.Destinations, aPrior);
			if (chosen) {
				discovery.Target = std::move(*chosen);
			} else {
				discovery.Selection = std::move(chosen.error());
			}
		}
		if (discovery.Source == core::source_state::branch && !discovery.Git) {
			compare(discovery, reached.get(), aGit, aStop);
		}
		if (aStop.stop_requested()) {
			return std::unexpected{ local_draft_open_error{ .Failure = local_draft_open_failure::cancelled } };
		}
		return discovery;
	}

	auto retarget_discovery(workspace_source_port& aSource, local_draft_git_port& aGit, local_draft_discovery aDiscovery, const core::chosen_target& aTarget, const std::stop_token aStop)
		-> std::expected<local_draft_discovery, local_draft_open_error>
	{
		auto reached = opened(aSource, reached_location(aDiscovery), aStop);
		if (!reached) {
			return std::unexpected{ reached.error() };
		}
		aDiscovery.Target = aTarget;
		aDiscovery.Selection.reset();
		compare(aDiscovery, reached->get(), aGit, aStop);
		return aDiscovery;
	}

	auto capture_local_draft(workspace_source_port& aSource, local_draft_git_port& aGit, const local_draft_discovery& aDiscovery, const std::span<const std::string> aIncluded,
							 const std::int64_t aNow, const std::stop_token aStop) -> std::expected<local_draft_capture, local_draft_capture_error>
	{
		if (!aDiscovery.ready()) {
			return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::not_ready } };
		}
		auto changes = core::compose_candidate_changes(aDiscovery.Committed, aDiscovery.BaseEntries, aDiscovery.Working, aIncluded);
		if (!changes) {
			return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::refused, .Refusal = changes.error() } };
		}
		auto session = std::unique_ptr<workspace_root_session>{ };
		if (!aIncluded.empty()) {
			auto opened = aSource.open(aDiscovery.Root.Location, aStop);
			if (!opened || (*opened)->observation().Repository != aDiscovery.Root.Repository || (*opened)->observation().Worktree != aDiscovery.Root.Worktree) {
				return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::changed } };
			}
			session = std::move(*opened);
		}
		const auto repository = aDiscovery.repository();
		auto       capture    = local_draft_capture{ };
		auto       total      = std::uint64_t{ 0U };
		auto       kept       = std::set<core::git_entry_oid>{ };
		for (auto& change : *changes) {
			for (auto* side : { &change.Left, &change.Right }) {
				if (!*side) {
					continue;
				}
				auto& material = **side;
				if (material.Mode == 0160000U) {
					continue;
				}
				auto bytes = std::vector<std::uint8_t>{ };
				if (material.Origin == core::draft_material_origin::working) {
					const auto entry = session->stat(change.Path, aStop);
					if (!entry) {
						return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::changed, .Path = change.Path } };
					}
					if (entry->Kind == workspace_entry_kind::symlink) {
						const auto target = session->read_link(change.Path, aStop);
						if (!target) {
							return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::changed, .Path = change.Path } };
						}
						bytes.assign(target->begin(), target->end());
					} else if (!session->read(change.Path, core::maximum_local_draft_file_bytes, [&bytes](const std::span<const std::uint8_t> aChunk) {
								   bytes.insert(bytes.end(), aChunk.begin(), aChunk.end());
								   return true;
							   }, aStop)) {
						return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::changed, .Path = change.Path } };
					}
					// The bytes read now must be the bytes that were listed.
					const auto blob = core::git_blob_identity(bytes, aDiscovery.Algorithm);
					if (!blob || core::retag<core::git_entry_oid_kind>(*blob) != material.Object) {
						return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::changed, .Path = change.Path } };
					}
				} else {
					auto read = aGit.read_blob(repository, material.Object, core::maximum_local_draft_file_bytes, aStop);
					if (!read) {
						if (read.error().Failure == publication_git_failure::bounds) {
							// Larger than a candidate keeps of one file: the side
							// is named and its bytes are not kept.
							continue;
						}
						return std::unexpected{ local_draft_capture_error{
							.Failure = read.error().Failure == publication_git_failure::cancelled ? local_draft_capture_failure::cancelled : local_draft_capture_failure::git,
							.Git     = read.error(),
							.Path    = change.Path } };
					}
					bytes = std::move(*read);
				}
				material.Size = bytes.size();
				total += bytes.size();
				if (total > core::maximum_local_draft_candidate_bytes) {
					return std::unexpected{ local_draft_capture_error{ .Failure = local_draft_capture_failure::too_large } };
				}
				if (kept.insert(material.Object).second) {
					capture.Objects.push_back({ .Object = material.Object, .Bytes = std::move(bytes) });
				}
			}
		}
		capture.Candidate = core::local_draft_candidate{
			.Head       = *aDiscovery.Head,
			.TargetTip  = *aDiscovery.TargetTip,
			.Base       = *aDiscovery.Comparison.Base,
			.Included   = { aIncluded.begin(), aIncluded.end() },
			.Changes    = std::move(*changes),
			.CapturedAt = aNow,
		};
		std::ranges::sort(capture.Candidate.Included);
		if (!aIncluded.empty()) {
			capture.Candidate.Qualifications.push_back(core::candidate_qualification::includes_working_files);
		}
		if (aDiscovery.Sparse) {
			capture.Candidate.Qualifications.push_back(core::candidate_qualification::sparse_checkout);
		}
		if (aDiscovery.ScanIncomplete) {
			capture.Candidate.Qualifications.push_back(core::candidate_qualification::working_scan_incomplete);
		}
		if (aDiscovery.LineEndings) {
			capture.Candidate.Qualifications.push_back(core::candidate_qualification::line_ending_conversion);
		}
		return capture;
	}
}
