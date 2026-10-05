#include "bpr/application/review_cycle_capture.hpp"

#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <span>
#include <utility>

namespace bpr::application {
	namespace {
		constexpr std::size_t changed_examples = 16U;
		/// Content installed per store transaction: one connection per batch
		/// rather than per file.
		constexpr std::size_t   install_batch_objects     = 512U;
		constexpr std::uint64_t install_batch_bytes       = 32U * 1024U * 1024U;
		constexpr std::uint64_t maximum_rule_file_bytes   = 1024U * 1024U;
		constexpr std::uint64_t maximum_head_bytes        = 4096U;
		constexpr std::uint64_t maximum_packed_refs_bytes = 64U * 1024U * 1024U;

		auto join(const std::string& aDirectory, const std::string& aName) -> std::string
		{
			return aDirectory.empty() ? aName : aDirectory + "/" + aName;
		}

		/// True when `aPath` is `aAncestor` or lies beneath it.
		auto within(const std::string_view aAncestor, const std::string_view aPath) -> bool
		{
			return aPath.starts_with(aAncestor) && (aPath.size() == aAncestor.size() || aPath[aAncestor.size()] == '/');
		}

		auto ascii_lower(const char aValue) -> char
		{
			return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue - 'A' + 'a') : aValue;
		}

		auto repository_name(const std::string_view aName) -> bool
		{
			return aName.size() == 4 && ascii_lower(aName[0]) == '.' && ascii_lower(aName[1]) == 'g' && ascii_lower(aName[2]) == 'i' && ascii_lower(aName[3]) == 't';
		}

		auto git_root(const core::workspace_root_kind aKind) -> bool
		{
			return aKind == core::workspace_root_kind::git_checkout || aKind == core::workspace_root_kind::git_linked_worktree;
		}

		/// One tracked path, merged across conflict stages.
		struct tracked_path
		{
			std::string                       Path;
			std::uint32_t                     Mode = 0;
			std::optional<core::git_blob_oid> Staged;
			bool                              Unmerged     = false;
			bool                              SkipWorktree = false;
			bool                              IntentToAdd  = false;
			/// A sparse-index directory standing for everything beneath it.
			bool SparseDirectory = false;
		};

		/// What a Git root's metadata says, read as data.
		struct git_metadata
		{
			core::git_hash_algorithm            Format = core::git_hash_algorithm::invalid;
			core::cycle_material_digest         IndexDigest;
			std::vector<tracked_path>           Tracked;
			bool                                Unmerged = false;
			bool                                Sparse   = false;
			bool                                Split    = false;
			std::optional<std::string>          HeadReference;
			std::optional<core::git_commit_oid> HeadCommit;
			std::string                         InfoExclude;
			std::string                         InfoAttributes;
		};

		auto merge_stages(const std::vector<core::git_index_entry>& aEntries) -> std::vector<tracked_path>
		{
			auto merged = std::vector<tracked_path>{ };
			for (const auto& entry : aEntries) {
				const auto sparse = entry.Mode == core::git_mode_directory && entry.Path.ends_with('/');
				auto       path   = sparse ? entry.Path.substr(0, entry.Path.size() - 1) : entry.Path;
				if (merged.empty() || merged.back().Path != path) {
					merged.push_back({ .Path = std::move(path), .Mode = entry.Mode });
				}
				auto& current           = merged.back();
				current.SparseDirectory = current.SparseDirectory || sparse;
				current.SkipWorktree    = current.SkipWorktree || entry.SkipWorktree;
				current.IntentToAdd     = current.IntentToAdd || entry.IntentToAdd;
				if (entry.Stage == 0) {
					current.Staged = entry.Object;
					current.Mode   = entry.Mode;
				} else {
					current.Unmerged = true;
				}
			}
			return merged;
		}

		auto text_of(const std::vector<std::uint8_t>& aBytes) -> std::string
		{
			return { aBytes.begin(), aBytes.end() };
		}

		/// Reads a Git metadata file; a missing file reads as empty.
		auto read_optional(workspace_root_session& aSession, const workspace_git_area aArea, const std::string_view aPath, const std::uint64_t aMaximum,
						   std::stop_token aStop) -> std::expected<std::vector<std::uint8_t>, workspace_failure>
		{
			auto bytes = aSession.read_git(aArea, aPath, aMaximum, std::move(aStop));
			if (!bytes && bytes.error() == workspace_failure::missing) {
				return std::vector<std::uint8_t>{ };
			}
			return bytes;
		}

		auto read_head(workspace_root_session& aSession, git_metadata& aGit, const std::stop_token& aStop) -> std::optional<workspace_failure>
		{
			const auto head = read_optional(aSession, workspace_git_area::worktree, "HEAD", maximum_head_bytes, aStop);
			if (!head) {
				return head.error();
			}
			const auto parsed  = core::parse_git_head(text_of(*head), aGit.Format);
			aGit.HeadReference = parsed.Reference;
			aGit.HeadCommit    = parsed.Commit;
			if (!parsed.Reference || core::admit_workspace_path(*parsed.Reference) != core::workspace_path_refusal::none) {
				return std::nullopt;
			}
			const auto loose = read_optional(aSession, workspace_git_area::common, *parsed.Reference, maximum_head_bytes, aStop);
			if (loose && !loose->empty()) {
				aGit.HeadCommit = core::parse_git_reference(text_of(*loose), aGit.Format);
				return std::nullopt;
			}
			const auto packed = read_optional(aSession, workspace_git_area::common, "packed-refs", maximum_packed_refs_bytes, aStop);
			if (packed) {
				aGit.HeadCommit = core::find_packed_reference(text_of(*packed), *parsed.Reference, aGit.Format);
			}
			return std::nullopt;
		}

		auto read_git_metadata(workspace_root_session& aSession, const std::stop_token& aStop) -> std::expected<git_metadata, workspace_failure>
		{
			auto       git    = git_metadata{ };
			const auto config = read_optional(aSession, workspace_git_area::common, "config", maximum_rule_file_bytes, aStop);
			if (!config) {
				return std::unexpected(config.error());
			}
			git.Format = core::git_object_format(text_of(*config));
			if (git.Format == core::git_hash_algorithm::invalid) {
				return std::unexpected(workspace_failure::unsupported);
			}
			const auto index = read_optional(aSession, workspace_git_area::worktree, "index", maximum_workspace_metadata_bytes, aStop);
			if (!index) {
				return std::unexpected(index.error());
			}
			git.IndexDigest = core::cycle_material_digest{ core::sha256(*index) };
			if (!index->empty()) {
				const auto parsed = core::parse_git_index(*index, git.Format, core::maximum_cycle_capture_entries);
				if (!parsed) {
					return std::unexpected(parsed.error() == core::git_index_error::bounds ? workspace_failure::too_large : workspace_failure::unsupported);
				}
				git.Tracked  = merge_stages(parsed->Entries);
				git.Unmerged = parsed->Unmerged;
				git.Sparse   = parsed->SparseDirectories;
				git.Split    = parsed->SplitIndex;
			}
			if (const auto failure = read_head(aSession, git, aStop)) {
				return std::unexpected(*failure);
			}
			const auto exclude    = read_optional(aSession, workspace_git_area::common, "info/exclude", maximum_rule_file_bytes, aStop);
			const auto attributes = read_optional(aSession, workspace_git_area::common, "info/attributes", maximum_rule_file_bytes, aStop);
			if (!exclude || !attributes) {
				return std::unexpected(!exclude ? exclude.error() : attributes.error());
			}
			git.InfoExclude    = text_of(*exclude);
			git.InfoAttributes = text_of(*attributes);
			return git;
		}

		enum class pass_mode : std::uint8_t {
			/// Read, hash and install every captured byte.
			install,
			/// Read and hash; install nothing.
			hash,
			/// Observe paths and change stamps only.
			stamp,
		};

		enum class interruption : std::uint8_t {
			none,
			unstable,
			cancelled,
			store,
			workspace,
			too_many_entries,
		};

		struct pass_result
		{
			std::vector<core::manifest_entry>     Entries;
			std::vector<core::capture_stamp>      Stamps;
			std::vector<core::manifest_exclusion> Exclusions;
			std::vector<std::string>              Deletions;
			std::vector<cycle_object_ref>         Content;
			bool                                  Complete  = true;
			std::uint64_t                         Refused   = 0;
			std::uint64_t                         FilesRead = 0;
			std::uint64_t                         BytesRead = 0;
			interruption                          Stopped   = interruption::none;
			std::optional<workspace_failure>      Failure;
			std::optional<cycle_store_error>      StoreError;
			std::string                           ChangedPath;
		};

		/// One observation of the whole scope.
		class scope_pass
		{
		public:
			scope_pass(workspace_root_session& aSession, review_cycle_store* aStore, const pass_mode aMode, const core::review_cycle_state& aCycle,
					   const std::vector<std::string>& aExclusions, const git_metadata* aGit, const std::uint64_t aMaximumFileBytes, std::stop_token aStop,
					   std::function<void(std::uint64_t)> aCounted = { })
				: Session{ aSession }
				, Store{ aStore }
				, Mode{ aMode }
				, Cycle{ aCycle }
				, Requested{ aExclusions }
				, Git{ aGit }
				, MaximumFileBytes{ aMaximumFileBytes }
				, Stop{ std::move(aStop) }
				, Counted{ std::move(aCounted) }
			{
			}

			auto run() -> pass_result
			{
				if (Git != nullptr) {
					static_cast<void>(Ignore.add("", Git->InfoExclude));
					static_cast<void>(InfoAttributes.add("", Git->InfoAttributes));
				}
				if (Session.observation().Kind == core::workspace_root_kind::file) {
					observe_file_root();
				} else if (Cycle.Scope.Paths.empty()) {
					walk("", false);
				} else {
					for (const auto& path : Cycle.Scope.Paths) {
						observe_scope_path(path);
						if (Result.Stopped != interruption::none) {
							break;
						}
					}
				}
				if (Result.Stopped == interruption::none && Git != nullptr) {
					account_for_tracked_paths();
				}
				if (Result.Stopped == interruption::none) {
					static_cast<void>(flush());
				}
				std::ranges::sort(Result.Entries, { }, &core::manifest_entry::Path);
				std::ranges::sort(Result.Stamps, { }, &core::capture_stamp::Path);
				std::ranges::sort(Result.Exclusions, { }, &core::manifest_exclusion::Path);
				std::ranges::sort(Result.Deletions);
				return std::move(Result);
			}

		private:
			[[nodiscard]] auto stopping() -> bool
			{
				if (Result.Stopped == interruption::none && Stop.stop_requested()) {
					Result.Stopped = interruption::cancelled;
				}
				return Result.Stopped != interruption::none;
			}

			void unstable(const std::string& aPath)
			{
				Result.Stopped     = interruption::unstable;
				Result.ChangedPath = aPath;
			}

			/// Maps a port failure met while reading one path.
			void read_failed(const std::string& aPath, const workspace_failure aFailure, core::manifest_entry& aEntry)
			{
				switch (aFailure) {
				case workspace_failure::cancelled:
					Result.Stopped = interruption::cancelled;
					return;
				case workspace_failure::permission_denied:
				case workspace_failure::io:
					aEntry.Availability = core::manifest_availability::unreadable;
					return;
				default:
					// Missing, replaced, grown or swapped for a link since it was listed.
					unstable(aPath);
					return;
				}
			}

			[[nodiscard]] auto tracked(const std::string_view aPath) const -> const tracked_path*
			{
				if (Git == nullptr) {
					return nullptr;
				}
				const auto found = std::ranges::lower_bound(Git->Tracked, aPath, { }, [](const tracked_path& aTracked) { return std::string_view{ aTracked.Path }; });
				return found != Git->Tracked.end() && found->Path == aPath ? &*found : nullptr;
			}

			[[nodiscard]] auto tracks_beneath(const std::string& aDirectory) const -> bool
			{
				if (Git == nullptr) {
					return false;
				}
				const auto prefix = aDirectory + "/";
				const auto found  = std::ranges::lower_bound(Git->Tracked, prefix, { }, &tracked_path::Path);
				return found != Git->Tracked.end() && found->Path.starts_with(prefix);
			}

			[[nodiscard]] auto requested(const std::string& aPath) const -> bool
			{
				return std::ranges::any_of(Requested, [&aPath](const std::string& aExcluded) { return within(aExcluded, aPath); });
			}

			void exclude(const std::string& aPath, const bool aDirectory, const core::workspace_exclusion_reason aReason)
			{
				Result.Exclusions.push_back({ .Path = aPath, .Directory = aDirectory, .Reason = aReason });
			}

			void record(core::manifest_entry aEntry, const core::capture_stamp& aStamp)
			{
				if (Result.Entries.size() >= core::maximum_cycle_capture_entries) {
					Result.Stopped = interruption::too_many_entries;
					return;
				}
				Result.Stamps.push_back(aStamp);
				if (Mode != pass_mode::stamp) {
					Result.Entries.push_back(std::move(aEntry));
				}
				if (Counted && Result.Stamps.size() % cycle_capture_progress_files == 0U) {
					Counted(Result.Stamps.size());
				}
			}

			static auto stamp_of(const std::string& aPath, const core::manifest_entry_kind aKind, const workspace_entry_observation& aObservation) -> core::capture_stamp
			{
				return { .Path = aPath, .Kind = aKind, .Identity = aObservation.Identity, .Size = aObservation.Size, .Modified = aObservation.Modified, .Changed = aObservation.Changed };
			}

			/// A directory whose contents are unknown to this capture.
			void opaque(const std::string& aPath, const core::manifest_availability aAvailability, const workspace_entry_observation& aObservation)
			{
				Opaque.push_back(aPath);
				Result.Complete = Result.Complete && aAvailability != core::manifest_availability::unreadable;
				record({ .Path = aPath, .Kind = core::manifest_entry_kind::directory, .Origin = origin_of(aPath), .Availability = aAvailability },
					   stamp_of(aPath, core::manifest_entry_kind::directory, aObservation));
			}

			[[nodiscard]] auto origin_of(const std::string& aPath) const -> core::manifest_origin
			{
				if (Git == nullptr) {
					return core::manifest_origin::unversioned;
				}
				return tracked(aPath) != nullptr || tracks_beneath(aPath) ? core::manifest_origin::tracked : core::manifest_origin::untracked;
			}

			/// Reads one rule file named in a directory listing.
			auto rule_file(const std::string& aDirectory, const std::vector<workspace_entry_observation>& aListing, const std::string_view aName) -> std::optional<std::string>
			{
				const auto found = std::ranges::find(aListing, aName, &workspace_entry_observation::Name);
				if (found == aListing.end() || found->Kind != workspace_entry_kind::file || found->Size > maximum_rule_file_bytes) {
					return std::nullopt;
				}
				auto       contents = std::string{ };
				const auto read     = Session.read(join(aDirectory, std::string{ aName }), maximum_rule_file_bytes, [&contents](const std::span<const std::uint8_t> aChunk) {
                    contents.append(aChunk.begin(), aChunk.end());
                    return true; }, Stop);
				return read ? std::optional{ std::move(contents) } : std::nullopt;
			}

			/// Adds a directory's `.gitignore` and `.gitattributes` rules once.
			void load_rules(const std::string& aDirectory, const std::vector<workspace_entry_observation>& aListing)
			{
				if (Git == nullptr || !Loaded.insert(aDirectory).second) {
					return;
				}
				if (const auto ignore = rule_file(aDirectory, aListing, ".gitignore")) {
					static_cast<void>(Ignore.add(aDirectory, *ignore));
				}
				if (const auto attributes = rule_file(aDirectory, aListing, ".gitattributes")) {
					static_cast<void>(Attributes.add(aDirectory, *attributes));
				}
			}

			void load_ancestor_rules(const std::string& aPath)
			{
				for (auto separator = std::string::size_type{ 0 };; ++separator) {
					const auto directory = separator == 0 ? std::string{ } : aPath.substr(0, separator - 1);
					if (!Loaded.contains(directory)) {
						const auto listing = Session.list(directory, Stop);
						if (listing) {
							load_rules(directory, *listing);
						}
					}
					separator = aPath.find('/', separator);
					if (separator == std::string::npos) {
						return;
					}
				}
			}

			void observe_file_root()
			{
				const auto listing = Session.list("", Stop);
				if (!listing || listing->size() != 1) {
					Result.Stopped = interruption::workspace;
					Result.Failure = listing ? workspace_failure::changed : listing.error();
					return;
				}
				visit(listing->front().Name, listing->front(), false);
			}

			void observe_scope_path(const std::string& aPath)
			{
				if (Git != nullptr) {
					load_ancestor_rules(aPath);
				}
				const auto observed = Session.stat(aPath, Stop);
				if (!observed) {
					if (observed.error() == workspace_failure::missing) {
						// Tracked paths beneath it become explicit deletions later.
						if (tracked(aPath) == nullptr && !tracks_beneath(aPath)) {
							opaque(aPath, core::manifest_availability::missing_scope, { });
						}
						return;
					}
					if (observed.error() == workspace_failure::escape) {
						opaque(aPath, core::manifest_availability::outside_root, { });
						return;
					}
					Result.Stopped = observed.error() == workspace_failure::cancelled ? interruption::cancelled : interruption::workspace;
					Result.Failure = observed.error();
					return;
				}
				if (observed->Kind == workspace_entry_kind::directory) {
					walk(aPath, false);
					return;
				}
				visit(aPath, *observed, false);
			}

			void walk(const std::string& aDirectory, const bool aIgnoredAncestor)
			{
				if (stopping()) {
					return;
				}
				const auto listing = Session.list(aDirectory, Stop);
				if (!listing) {
					walk_failed(aDirectory, listing.error());
					return;
				}
				if (!aDirectory.empty() && std::ranges::any_of(*listing, [](const workspace_entry_observation& aEntry) { return repository_name(aEntry.Name); })) {
					// A nested repository is a boundary, never recursed.
					const auto self = Session.stat(aDirectory, Stop);
					record({ .Path = aDirectory, .Kind = core::manifest_entry_kind::gitlink, .Origin = origin_of(aDirectory), .Availability = core::manifest_availability::not_materialized, .Mode = core::git_mode_gitlink },
						   stamp_of(aDirectory, core::manifest_entry_kind::gitlink, self ? *self : workspace_entry_observation{ }));
					Opaque.push_back(aDirectory);
					return;
				}
				load_rules(aDirectory, *listing);
				for (const auto& entry : *listing) {
					visit(join(aDirectory, entry.Name), entry, aIgnoredAncestor);
					if (stopping()) {
						return;
					}
				}
			}

			void walk_failed(const std::string& aDirectory, const workspace_failure aFailure)
			{
				switch (aFailure) {
				case workspace_failure::cancelled:
					Result.Stopped = interruption::cancelled;
					return;
				case workspace_failure::escape:
					opaque(aDirectory, core::manifest_availability::outside_root, { });
					return;
				case workspace_failure::too_large:
					Result.Stopped = interruption::too_many_entries;
					return;
				case workspace_failure::permission_denied:
				case workspace_failure::io:
				case workspace_failure::unsupported:
					if (aDirectory.empty()) {
						Result.Stopped = interruption::workspace;
						Result.Failure = aFailure;
						return;
					}
					opaque(aDirectory, core::manifest_availability::unreadable, { });
					return;
				default:
					if (aDirectory.empty()) {
						Result.Stopped = interruption::workspace;
						Result.Failure = aFailure;
						return;
					}
					unstable(aDirectory);
					return;
				}
			}

			void visit(const std::string& aPath, const workspace_entry_observation& aEntry, const bool aIgnoredAncestor)
			{
				if (core::admit_workspace_path(aPath) != core::workspace_path_refusal::none) {
					++Result.Refused;
					return;
				}
				const auto directory = aEntry.Kind == workspace_entry_kind::directory;
				if (requested(aPath)) {
					exclude(aPath, directory, core::workspace_exclusion_reason::requested);
					return;
				}
				const auto* index     = tracked(aPath);
				const auto  name      = std::string_view{ aPath }.substr(aPath.find_last_of('/') + 1);
				const auto  isTracked = index != nullptr || (directory && tracks_beneath(aPath));
				if (core::excluded_by_default(name) && !isTracked) {
					exclude(aPath, directory, core::workspace_exclusion_reason::default_set);
					return;
				}
				if (directory) {
					visit_directory(aPath, aEntry, aIgnoredAncestor, index);
					return;
				}
				if (Git != nullptr && index == nullptr && (aIgnoredAncestor || Ignore.ignored(aPath, false))) {
					exclude(aPath, false, core::workspace_exclusion_reason::ignored);
					return;
				}
				capture(aPath, aEntry, index);
			}

			void visit_directory(const std::string& aPath, const workspace_entry_observation& aEntry, const bool aIgnoredAncestor, const tracked_path* aIndex)
			{
				if (aIndex != nullptr && aIndex->Mode == core::git_mode_gitlink) {
					record({ .Path = aPath, .Kind = core::manifest_entry_kind::gitlink, .Origin = core::manifest_origin::tracked, .Availability = core::manifest_availability::not_materialized, .Mode = core::git_mode_gitlink, .Staged = aIndex->Staged },
						   stamp_of(aPath, core::manifest_entry_kind::gitlink, aEntry));
					Opaque.push_back(aPath);
					return;
				}
				const auto ignored = Git != nullptr && (aIgnoredAncestor || Ignore.ignored(aPath, true));
				if (ignored && !tracks_beneath(aPath)) {
					exclude(aPath, true, core::workspace_exclusion_reason::ignored);
					return;
				}
				walk(aPath, ignored);
			}

			[[nodiscard]] auto mode_of(const workspace_entry_observation& aEntry, const tracked_path* aIndex, bool& aFromIndex) const -> std::uint32_t
			{
				aFromIndex = false;
				if (aEntry.Executable) {
					return *aEntry.Executable ? core::git_mode_executable : core::git_mode_regular;
				}
				// No execute bit on this platform: keep the staged mode, and
				// never invent one for a new file.
				if (aIndex != nullptr && (aIndex->Mode == core::git_mode_regular || aIndex->Mode == core::git_mode_executable)) {
					aFromIndex = true;
					return aIndex->Mode;
				}
				return core::git_mode_regular;
			}

			[[nodiscard]] auto filtered(const std::string& aPath) const -> bool
			{
				if (Git == nullptr) {
					return false;
				}
				return InfoAttributes.filter(aPath).value_or(Attributes.filter(aPath).value_or(false));
			}

			void capture(const std::string& aPath, const workspace_entry_observation& aEntry, const tracked_path* aIndex)
			{
				auto entry = core::manifest_entry{
					.Path          = aPath,
					.Origin        = Git == nullptr ? core::manifest_origin::unversioned : aIndex != nullptr ? core::manifest_origin::tracked
																											 : core::manifest_origin::untracked,
					.Size          = aEntry.Size,
					.Staged        = aIndex != nullptr ? aIndex->Staged : std::nullopt,
					.Unmerged      = aIndex != nullptr && aIndex->Unmerged,
					.IntentToAdd   = aIndex != nullptr && aIndex->IntentToAdd,
					.FilterManaged = filtered(aPath),
				};
				switch (aEntry.Kind) {
				case workspace_entry_kind::symlink:
					capture_link(std::move(entry), aEntry);
					return;
				case workspace_entry_kind::file:
					entry.Mode = mode_of(aEntry, aIndex, entry.ModeFromIndex);
					capture_file(std::move(entry), aEntry);
					return;
				default:
					entry.Availability = core::manifest_availability::unsupported_kind;
					record(std::move(entry), stamp_of(aPath, core::manifest_entry_kind::file, aEntry));
					return;
				}
			}

			void capture_file(core::manifest_entry aEntry, const workspace_entry_observation& aObserved)
			{
				const auto path = aEntry.Path;
				if (Mode == pass_mode::stamp || aObserved.Size > MaximumFileBytes) {
					if (aObserved.Size > MaximumFileBytes) {
						aEntry.Availability = core::manifest_availability::too_large;
					}
					record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::file, aObserved));
					return;
				}
				auto       hasher     = core::sha256_stream{ };
				auto       classifier = core::material_classifier{ };
				auto       bytes      = std::vector<std::uint8_t>{ };
				const auto read       = Session.read(path, MaximumFileBytes, [&](const std::span<const std::uint8_t> aChunk) {
					hasher.update(aChunk);
					classifier.add(aChunk);
					if (Mode == pass_mode::install) {
						bytes.insert(bytes.end(), aChunk.begin(), aChunk.end());
					}
					return !Stop.stop_requested(); }, Stop);
				if (!read) {
					read_failed(path, read.error(), aEntry);
					if (Result.Stopped == interruption::none) {
						record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::file, aObserved));
					}
					return;
				}
				const auto digest = core::cycle_material_digest{ hasher.finish() };
				aEntry.Size       = read->BytesRead;
				aEntry.Profile    = classifier.finish();
				aEntry.Content    = digest;
				++Result.FilesRead;
				Result.BytesRead += read->BytesRead;
				const auto before = read->Before;
				if (Mode == pass_mode::install && !install(std::move(bytes), digest)) {
					return;
				}
				record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::file, before));
			}

			void capture_link(core::manifest_entry aEntry, const workspace_entry_observation& aObserved)
			{
				const auto path = aEntry.Path;
				aEntry.Kind     = core::manifest_entry_kind::symlink;
				aEntry.Mode     = core::git_mode_symlink;
				if (Mode == pass_mode::stamp) {
					record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::symlink, aObserved));
					return;
				}
				const auto target = Session.read_link(path, Stop);
				if (!target) {
					if (target.error() == workspace_failure::unsupported) {
						aEntry.Availability = core::manifest_availability::link_target_unsupported;
					} else {
						read_failed(path, target.error(), aEntry);
					}
					if (Result.Stopped == interruption::none) {
						record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::symlink, aObserved));
					}
					return;
				}
				auto       bytes  = std::vector<std::uint8_t>{ target->begin(), target->end() };
				const auto digest = core::cycle_material_digest{ core::sha256(bytes) };
				aEntry.Size       = bytes.size();
				aEntry.Content    = digest;
				if (Mode == pass_mode::install && !install(std::move(bytes), digest)) {
					return;
				}
				record(std::move(aEntry), stamp_of(path, core::manifest_entry_kind::symlink, aObserved));
			}

			/// Queues bytes for the next batched install.
			auto install(std::vector<std::uint8_t> aBytes, const core::cycle_material_digest& aDigest) -> bool
			{
				PendingBytes += aBytes.size();
				Pending.emplace_back(std::move(aBytes), aDigest);
				return (Pending.size() < install_batch_objects && PendingBytes < install_batch_bytes) || flush();
			}

			/// Installs the queued bytes in one transaction and checks that the
			/// store holds exactly the digests the pass computed.
			auto flush() -> bool
			{
				if (Pending.empty()) {
					return true;
				}
				auto spans = std::vector<std::span<const std::uint8_t>>{ };
				spans.reserve(Pending.size());
				for (const auto& item : Pending) {
					spans.emplace_back(item.first);
				}
				const auto installed = Store->install_all(spans, Stop);
				if (!installed) {
					Result.Stopped    = installed.error().Failure == cycle_store_failure::cancelled ? interruption::cancelled : interruption::store;
					Result.StoreError = installed.error();
					return false;
				}
				for (std::size_t index = 0; index < Pending.size(); ++index) {
					if ((*installed)[index].Digest != Pending[index].second) {
						Result.Stopped    = interruption::store;
						Result.StoreError = cycle_store_error{ .Failure = cycle_store_failure::corrupt };
						return false;
					}
					Result.Content.push_back((*installed)[index]);
				}
				Pending.clear();
				PendingBytes = 0;
				return true;
			}

			[[nodiscard]] auto hidden(const std::string& aPath) const -> bool
			{
				const auto under = [&aPath](const std::string& aDirectory) { return within(aDirectory, aPath); };
				return std::ranges::any_of(Opaque, under)
					|| std::ranges::any_of(Result.Exclusions, [&aPath](const core::manifest_exclusion& aExclusion) { return within(aExclusion.Path, aPath); })
					|| requested(aPath);
			}

			/// Tracked paths the walk did not meet: sparse entries stay visible
			/// as unavailable; anything else is an explicit deletion.
			void account_for_tracked_paths()
			{
				auto seen = std::set<std::string_view>{ };
				for (const auto& stamp : Result.Stamps) {
					seen.insert(stamp.Path);
				}
				auto missing = std::vector<const tracked_path*>{ };
				for (const auto& path : Git->Tracked) {
					if (core::scope_contains(Cycle.Scope, path.Path) && !seen.contains(path.Path) && !hidden(path.Path)) {
						missing.push_back(&path);
					}
				}
				for (const auto* path : missing) {
					if (core::admit_workspace_path(path->Path) != core::workspace_path_refusal::none) {
						++Result.Refused;
						continue;
					}
					if (path->SkipWorktree || path->SparseDirectory) {
						record_sparse(*path);
						continue;
					}
					Result.Deletions.push_back(path->Path);
					Result.Stamps.push_back({ .Path = path->Path });
				}
			}

			void record_sparse(const tracked_path& aPath)
			{
				auto kind = core::manifest_entry_kind::file;
				if (aPath.SparseDirectory) {
					kind = core::manifest_entry_kind::directory;
				} else if (aPath.Mode == core::git_mode_symlink) {
					kind = core::manifest_entry_kind::symlink;
				} else if (aPath.Mode == core::git_mode_gitlink) {
					kind = core::manifest_entry_kind::gitlink;
				}
				record({ .Path = aPath.Path, .Kind = kind, .Origin = core::manifest_origin::tracked, .Availability = core::manifest_availability::sparse, .Mode = aPath.Mode, .Staged = aPath.Staged, .Unmerged = aPath.Unmerged },
					   { .Path = aPath.Path, .Kind = kind });
			}

			workspace_root_session&         Session;
			review_cycle_store*             Store;
			pass_mode                       Mode;
			const core::review_cycle_state& Cycle;
			const std::vector<std::string>& Requested;
			const git_metadata*             Git;
			std::uint64_t                   MaximumFileBytes;
			std::stop_token                 Stop;
			/// [[BPR-1184]]. Told the paths observed so far, every so many.
			std::function<void(std::uint64_t)>                                             Counted;
			core::workspace_ignore_rules                                                   Ignore;
			core::workspace_attribute_rules                                                Attributes;
			core::workspace_attribute_rules                                                InfoAttributes;
			std::set<std::string>                                                          Loaded;
			std::vector<std::pair<std::vector<std::uint8_t>, core::cycle_material_digest>> Pending;
			std::uint64_t                                                                  PendingBytes = 0;
			std::vector<std::string>                                                       Opaque;
			pass_result                                                                    Result;
		};

		auto admitted_exclusions(const std::vector<std::string>& aExclusions) -> bool
		{
			return aExclusions.size() <= core::maximum_cycle_scope_paths
				&& std::ranges::all_of(aExclusions, [](const std::string& aPath) { return core::admit_workspace_path(aPath) == core::workspace_path_refusal::none; });
		}

		auto from_interruption(const pass_result& aPass, cycle_capture_outcome& aOutcome) -> void
		{
			switch (aPass.Stopped) {
			case interruption::cancelled:
				aOutcome.Status = cycle_capture_status::cancelled;
				return;
			case interruption::store:
				aOutcome.Status = cycle_capture_status::store;
				aOutcome.Store  = aPass.StoreError;
				return;
			case interruption::too_many_entries:
				aOutcome.Status = cycle_capture_status::too_many_entries;
				return;
			case interruption::workspace:
				aOutcome.Status    = cycle_capture_status::workspace;
				aOutcome.Workspace = aPass.Failure;
				return;
			case interruption::unstable:
			case interruption::none:
				aOutcome.Status = cycle_capture_status::unstable;
				return;
			}
		}

		struct opened
		{
			std::unique_ptr<workspace_root_session> Session;
			std::optional<git_metadata>             Git;
		};

		/// Opens a root and reads its Git metadata when it has any.
		auto open_root(workspace_source_port& aSource, const std::string_view aLocation, const std::stop_token& aStop) -> std::expected<opened, workspace_failure>
		{
			auto session = aSource.open(aLocation, aStop);
			if (!session) {
				return std::unexpected(session.error());
			}
			auto result = opened{ .Session = std::move(*session) };
			if (git_root(result.Session->observation().Kind)) {
				auto git = read_git_metadata(*result.Session, aStop);
				if (!git) {
					return std::unexpected(git.error());
				}
				result.Git = std::move(*git);
			}
			return result;
		}

		auto build_manifest(const core::review_cycle_state& aCycle, const core::workspace_root_observation& aRoot, const std::optional<git_metadata>& aGit, pass_result& aPass)
			-> core::cycle_manifest
		{
			auto manifest = core::cycle_manifest{
				.ScopeVersion = aCycle.Scope.Version,
				.Scope        = aCycle.Scope.Paths,
				.RootKind     = aRoot.Kind,
				.Coverage     = aPass.Complete && !(aGit && aGit->Split) ? core::cycle_capture_coverage::complete : core::cycle_capture_coverage::partial,
				.Quality      = core::cycle_capture_quality::observed_stable,
				.RefusedPaths = aPass.Refused,
				.Entries      = std::move(aPass.Entries),
				.Deletions    = std::move(aPass.Deletions),
				.Exclusions   = std::move(aPass.Exclusions),
			};
			if (aGit) {
				manifest.ObjectFormat  = aGit->Format;
				manifest.HeadReference = aGit->HeadReference;
				manifest.HeadCommit    = aGit->HeadCommit;
				manifest.IndexUnmerged = aGit->Unmerged;
				manifest.IndexSparse   = aGit->Sparse;
				manifest.IndexSplit    = aGit->Split;
			}
			return manifest;
		}

		auto distinct_content(std::vector<cycle_object_ref> aContent) -> std::vector<cycle_object_ref>
		{
			std::ranges::sort(aContent);
			const auto [first, last] = std::ranges::unique(aContent);
			aContent.erase(first, last);
			return aContent;
		}
	}

	review_cycle_capture_host::review_cycle_capture_host(workspace_source_port& aSource, review_cycle_store& aStore) noexcept
		: Source{ aSource }
		, Store{ aStore }
	{
	}

	auto review_cycle_capture_host::observe(const std::string_view aLocation, std::stop_token aStop) -> std::expected<core::workspace_root_observation, workspace_failure>
	{
		auto session = Source.open(aLocation, std::move(aStop));
		if (!session) {
			return std::unexpected(session.error());
		}
		return (*session)->observation();
	}

	auto review_cycle_capture_host::capture(const core::review_cycle_state& aCycle, const cycle_capture_request& aRequest, std::stop_token aStop) -> cycle_capture_outcome
	{
		auto outcome = cycle_capture_outcome{ };
		if (!aCycle.Workspace) {
			outcome.Status = cycle_capture_status::not_bound;
			return outcome;
		}
		if (!admitted_exclusions(aRequest.Exclusions)) {
			outcome.Status    = cycle_capture_status::workspace;
			outcome.Workspace = workspace_failure::unsafe_path;
			return outcome;
		}
		const auto& binding = *aCycle.Workspace;
		// [[BPR-1184]]. Progress for a line that needs no dialog, when asked for.
		const auto report = [&aRequest](const cycle_capture_progress::phase aPhase, const std::uint32_t aAttempt) -> std::function<void(std::uint64_t)> {
			if (!aRequest.Progress) {
				return { };
			}
			return [&aRequest, aPhase, aAttempt](const std::uint64_t aFiles) { aRequest.Progress({ .Phase = aPhase, .Files = aFiles, .Attempt = aAttempt }); };
		};
		for (std::uint32_t attempt = 0; attempt <= core::maximum_capture_retries; ++attempt) {
			outcome.Diagnostics.Attempts = attempt + 1;
			auto root                    = open_root(Source, binding.Root.Location, aStop);
			if (!root) {
				outcome.Status    = root.error() == workspace_failure::cancelled ? cycle_capture_status::cancelled : cycle_capture_status::workspace;
				outcome.Workspace = root.error();
				return outcome;
			}
			const auto verification = core::verify_workspace(binding, root->Session->observation());
			if (verification != core::workspace_verification::matches) {
				outcome.Status       = cycle_capture_status::root_changed;
				outcome.Verification = verification;
				return outcome;
			}
			const auto* git = root->Git ? &*root->Git : nullptr;
			if (const auto starting = report(cycle_capture_progress::phase::reading, attempt + 1)) {
				starting(0);
			}
			auto first = scope_pass{ *root->Session, &Store, pass_mode::install, aCycle, aRequest.Exclusions, git, aRequest.MaximumFileBytes, aStop, report(cycle_capture_progress::phase::reading, attempt + 1) }.run();
			outcome.Diagnostics.FilesRead += first.FilesRead;
			outcome.Diagnostics.BytesRead += first.BytesRead;
			if (first.Stopped == interruption::unstable) {
				outcome.Diagnostics.Changed         = 1;
				outcome.Diagnostics.ChangedExamples = { core::workspace_path_label(first.ChangedPath) };
				continue;
			}
			if (first.Stopped != interruption::none) {
				from_interruption(first, outcome);
				return outcome;
			}

			// The second observation: the same scope, stamps only, then the
			// index and the root itself again.
			if (const auto checking = report(cycle_capture_progress::phase::checking, attempt + 1)) {
				checking(0);
			}
			auto second = scope_pass{ *root->Session, nullptr, pass_mode::stamp, aCycle, aRequest.Exclusions, git, aRequest.MaximumFileBytes, aStop, report(cycle_capture_progress::phase::checking, attempt + 1) }.run();
			if (second.Stopped == interruption::cancelled) {
				outcome.Status = cycle_capture_status::cancelled;
				return outcome;
			}
			auto comparison = core::compare_capture_passes(first.Stamps, second.Stamps, changed_examples);
			if (second.Stopped == interruption::unstable) {
				++comparison.Changed;
				comparison.Examples.push_back(second.ChangedPath);
			}
			if (git != nullptr) {
				const auto again = read_git_metadata(*root->Session, aStop);
				if (!again || again->IndexDigest != git->IndexDigest || again->HeadCommit != git->HeadCommit || again->HeadReference != git->HeadReference) {
					++comparison.Changed;
					comparison.Examples.emplace_back(".git/index");
				}
			}
			const auto still = root->Session->verify(aStop);
			if (!still || *still != core::workspace_verification::matches) {
				outcome.Status       = cycle_capture_status::root_changed;
				outcome.Verification = still ? std::optional{ *still } : std::nullopt;
				outcome.Workspace    = still ? std::nullopt : std::optional{ still.error() };
				return outcome;
			}
			if (!comparison.stable()) {
				outcome.Diagnostics.Changed = comparison.Changed;
				outcome.Diagnostics.ChangedExamples.clear();
				for (const auto& path : comparison.Examples) {
					outcome.Diagnostics.ChangedExamples.push_back(core::workspace_path_label(path));
				}
				continue;
			}

			outcome.Diagnostics.Changed = 0;
			outcome.Diagnostics.ChangedExamples.clear();
			if (const auto recording = report(cycle_capture_progress::phase::recording, attempt + 1)) {
				recording(first.Stamps.size());
			}
			auto       content  = distinct_content(std::move(first.Content));
			const auto manifest = build_manifest(aCycle, root->Session->observation(), root->Git, first);
			if (core::validate_manifest(manifest)) {
				outcome.Status    = cycle_capture_status::workspace;
				outcome.Workspace = workspace_failure::unsafe_path;
				return outcome;
			}
			const auto encoded = core::encode_manifest(manifest);
			const auto object  = Store.install(encoded, aStop);
			if (!object) {
				outcome.Status = object.error().Failure == cycle_store_failure::cancelled ? cycle_capture_status::cancelled : cycle_capture_status::store;
				outcome.Store  = object.error();
				return outcome;
			}
			const auto observation = core::observe_manifest(manifest, object->Digest, binding.Version);
			const auto receipt     = Store.record_capture(aRequest.Operation, { .Subject = aCycle.Subject, .ExpectedRevision = aRequest.ExpectedRevision, .Capture = { .Capture = observation, .Deliberate = aRequest.Deliberate }, .Manifest = *object, .Content = std::move(content) }, aStop);
			if (!receipt) {
				outcome.Status = receipt.error().Failure == cycle_store_failure::cancelled ? cycle_capture_status::cancelled : cycle_capture_status::store;
				outcome.Store  = receipt.error();
				return outcome;
			}
			outcome.Status                  = cycle_capture_status::captured;
			outcome.Diagnostics.Explanation = core::explain_capture(observation, outcome.Diagnostics.Attempts);
			outcome.Receipt                 = *receipt;
			outcome.Capture                 = observation;
			outcome.Manifest                = *object;
			return outcome;
		}
		outcome.Status = cycle_capture_status::unstable;
		return outcome;
	}

	auto review_cycle_capture_host::check_material(const core::review_cycle_state& aCycle, const std::string_view aLocation, std::stop_token aStop) -> cycle_material_check_outcome
	{
		auto outcome = cycle_material_check_outcome{ };
		auto latest  = core::cycle_manifest{ };
		if (aCycle.Latest) {
			const auto page = Store.revisions(aCycle.Subject, aCycle.Latest->Id.Id.value() > 1 ? std::optional{ core::cycle_source_revision_id{ aCycle.Latest->Id.Id.value() - 1 } } : std::nullopt, aStop);
			if (!page || page->Revisions.empty() || page->Revisions.front().Revision.Id != aCycle.Latest->Id) {
				outcome.Status = cycle_capture_status::store;
				outcome.Store  = page ? cycle_store_error{ .Failure = cycle_store_failure::not_found } : page.error();
				return outcome;
			}
			const auto bytes = Store.read_object(page->Revisions.front().Manifest, aStop);
			if (!bytes) {
				outcome.Status = cycle_capture_status::store;
				outcome.Store  = bytes.error();
				return outcome;
			}
			auto decoded = core::decode_manifest(*bytes);
			if (!decoded) {
				outcome.Status = cycle_capture_status::store;
				outcome.Store  = cycle_store_error{ .Failure = cycle_store_failure::corrupt };
				return outcome;
			}
			latest = std::move(*decoded);
		}
		auto root = open_root(Source, aLocation, aStop);
		if (!root) {
			outcome.Status    = root.error() == workspace_failure::cancelled ? cycle_capture_status::cancelled : cycle_capture_status::workspace;
			outcome.Workspace = root.error();
			return outcome;
		}
		outcome.Root             = root->Session->observation();
		const auto  noExclusions = std::vector<std::string>{ };
		const auto* git          = root->Git ? &*root->Git : nullptr;
		auto        candidate    = scope_pass{ *root->Session, nullptr, pass_mode::hash, aCycle, noExclusions, git, core::maximum_captured_file_bytes, aStop }.run();
		if (candidate.Stopped != interruption::none) {
			auto failed = cycle_capture_outcome{ };
			from_interruption(candidate, failed);
			outcome.Status    = failed.Status;
			outcome.Workspace = failed.Workspace;
			outcome.Store     = failed.Store;
			return outcome;
		}
		auto check = core::workspace_material_check{ .Compared = latest.Entries.size() };
		// Merge the two sorted entry lists.
		const auto& before = latest.Entries;
		auto        left   = before.begin();
		auto        right  = candidate.Entries.begin();
		while (left != before.end() || right != candidate.Entries.end()) {
			if (right == candidate.Entries.end() || (left != before.end() && left->Path < right->Path)) {
				++check.Missing;
				++left;
			} else if (left == before.end() || right->Path < left->Path) {
				++check.Added;
				++right;
			} else {
				if (left->Content == right->Content && left->Kind == right->Kind && left->Mode == right->Mode) {
					++check.Matching;
				} else {
					++check.Differing;
				}
				++left;
				++right;
			}
		}
		outcome.Status = cycle_capture_status::captured;
		outcome.Check  = check;
		return outcome;
	}
}
