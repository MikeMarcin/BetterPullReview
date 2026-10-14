#include "bpr/application/review_cycle_git_metadata.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		constexpr std::uint64_t maximum_config_bytes         = 1024U * 1024U;
		constexpr std::uint64_t maximum_reference_file_bytes = 64U * 1024U;

		[[nodiscard]] auto text(const std::vector<std::uint8_t>& aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}

		/// One Git metadata file; a missing one reads as empty.
		auto read_optional(workspace_root_session& aSession, const workspace_git_area aArea, const std::string_view aPath, const std::uint64_t aMaximum, std::stop_token aStop)
			-> std::expected<std::vector<std::uint8_t>, workspace_failure>
		{
			auto bytes = aSession.read_git(aArea, aPath, aMaximum, std::move(aStop));
			if (!bytes && bytes.error() == workspace_failure::missing) {
				return std::vector<std::uint8_t>{ };
			}
			return bytes;
		}

		/// Git's `SYMREF_MAXDEPTH`: a name and its symbolic targets are read at
		/// most this many times before the reference counts as unresolvable.
		constexpr int maximum_reference_reads = 5;

		/// One reference: its loose file, else `packed-refs`. A loose symbolic
		/// reference is followed while `aReads` allows another read, as Git
		/// follows it; deeper than that it names no commit, as in Git.
		auto resolve_named(workspace_root_session& aSession, const std::string_view aName, const core::git_hash_algorithm aAlgorithm, const int aReads, const std::stop_token& aStop)
			-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>
		{
			if (!core::admissible_git_reference(aName)) {
				return std::unexpected(std::nullopt);
			}
			const auto loose = aSession.read_git(workspace_git_area::common, aName, maximum_reference_file_bytes, aStop);
			if (loose) {
				const auto parsed = core::parse_git_head(text(*loose), aAlgorithm);
				if (parsed.Commit) {
					return *parsed.Commit;
				}
				if (parsed.Reference && aReads > 1) {
					return resolve_named(aSession, *parsed.Reference, aAlgorithm, aReads - 1, aStop);
				}
				return std::unexpected(std::nullopt);
			}
			if (loose.error() != workspace_failure::missing) {
				return std::unexpected(loose.error());
			}
			const auto packed = aSession.read_git(workspace_git_area::common, "packed-refs", maximum_workspace_metadata_bytes, aStop);
			if (!packed) {
				return std::unexpected(packed.error() == workspace_failure::missing ? std::nullopt : std::optional{ packed.error() });
			}
			if (const auto commit = core::find_packed_reference(text(*packed), aName, aAlgorithm)) {
				return *commit;
			}
			return std::unexpected(std::nullopt);
		}
	}

	auto read_git_repository_format(workspace_root_session& aSession, std::stop_token aStop) -> std::expected<core::git_repository_format, git_metadata_failure>
	{
		const auto config = read_optional(aSession, workspace_git_area::common, "config", maximum_config_bytes, aStop);
		if (!config) {
			return std::unexpected(git_metadata_failure{ .Workspace = config.error() });
		}
		auto format = core::read_git_repository_format(text(*config));
		if (!format) {
			return std::unexpected(git_metadata_failure{ .Workspace = workspace_failure::unsupported, .Repository = format.error() });
		}
		if (format->WorktreeConfig) {
			const auto worktree = read_optional(aSession, workspace_git_area::worktree, "config.worktree", maximum_config_bytes, std::move(aStop));
			if (!worktree) {
				return std::unexpected(git_metadata_failure{ .Workspace = worktree.error() });
			}
			if (auto problem = core::apply_git_worktree_config(text(*worktree), *format)) {
				return std::unexpected(git_metadata_failure{ .Workspace = workspace_failure::unsupported, .Repository = std::move(problem) });
			}
		}
		return *format;
	}

	auto read_git_head(workspace_root_session& aSession, const core::git_hash_algorithm aAlgorithm, std::stop_token aStop) -> std::expected<git_head_state, workspace_failure>
	{
		const auto head = read_optional(aSession, workspace_git_area::worktree, "HEAD", maximum_reference_file_bytes, aStop);
		if (!head) {
			return std::unexpected(head.error());
		}
		const auto parsed = core::parse_git_head(text(*head), aAlgorithm);
		auto       state  = git_head_state{ .Reference = parsed.Reference, .Commit = parsed.Commit };
		if (!parsed.Reference) {
			return state;
		}
		// HEAD itself was the first read.
		const auto resolved = resolve_named(aSession, *parsed.Reference, aAlgorithm, maximum_reference_reads - 1, aStop);
		if (resolved) {
			state.Commit = *resolved;
		} else if (resolved.error()) {
			return std::unexpected(*resolved.error());
		}
		return state;
	}

	auto resolve_reference(workspace_root_session& aSession, const std::string_view aName, const core::git_hash_algorithm aAlgorithm, std::stop_token aStop)
		-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>
	{
		if (const auto exact = core::parse_git_commit_oid(aName)) {
			if (exact->algorithm() != aAlgorithm) {
				return std::unexpected(std::nullopt);
			}
			return *exact;
		}
		if (aName == "HEAD") {
			const auto head = read_git_head(aSession, aAlgorithm, std::move(aStop));
			if (!head) {
				return std::unexpected(head.error() == workspace_failure::missing ? std::nullopt : std::optional{ head.error() });
			}
			if (head->Commit) {
				return *head->Commit;
			}
			return std::unexpected(std::nullopt);
		}
		return resolve_named(aSession, aName, aAlgorithm, maximum_reference_reads, aStop);
	}
}
