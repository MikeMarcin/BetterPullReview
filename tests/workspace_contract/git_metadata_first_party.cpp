// [[BPR-1455]]. The first-party Git metadata readers (K) composed as
// capture composes them, behind RFC-BPR-0083's candidate interface. Every
// byte comes through the workspace port; nothing here runs Git.

#include "bpr/adapters/workspace/local_workspace_source.hpp"
#include "bpr/application/review_cycle_git_metadata.hpp"
#include "bpr/application/review_cycle_words.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "git_metadata_candidate.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::tests::workspace_contract {
	namespace {
		using application::workspace_failure;
		using application::workspace_git_area;
		using application::workspace_root_session;

		/// Capture's bound on one `.gitignore` or `.gitattributes` file.
		constexpr std::uint64_t maximum_rule_file_bytes = 1024U * 1024U;

		auto failure_name(const workspace_failure aFailure) -> std::string
		{
			switch (aFailure) {
			case workspace_failure::missing:           return "missing";
			case workspace_failure::unsupported:       return "unsupported";
			case workspace_failure::permission_denied: return "permission denied";
			case workspace_failure::escape:            return "escape";
			case workspace_failure::replaced:          return "replaced";
			case workspace_failure::unsafe_path:       return "unsafe path";
			case workspace_failure::too_large:         return "too large";
			case workspace_failure::changed:           return "changed";
			case workspace_failure::cancelled:         return "cancelled";
			case workspace_failure::io:                return "io";
			}
			return "unknown";
		}

		auto describe(const application::git_metadata_failure& aFailure) -> candidate_refusal
		{
			if (aFailure.Repository) {
				return application::cycle_repository_words(*aFailure.Repository);
			}
			return "workspace: " + failure_name(aFailure.Workspace);
		}

		auto hash_name(const core::git_hash_algorithm aAlgorithm) -> std::string
		{
			return aAlgorithm == core::git_hash_algorithm::sha256 ? "sha256" : "sha1";
		}

		auto git_root(const core::workspace_root_kind aKind) -> bool
		{
			return aKind == core::workspace_root_kind::git_checkout || aKind == core::workspace_root_kind::git_linked_worktree;
		}

		/// A root-relative directory's parent prefixes, shortest first.
		auto ancestors(const std::string_view aPath) -> std::vector<std::string>
		{
			auto result = std::vector<std::string>{ };
			for (auto slash = aPath.find('/'); slash != std::string_view::npos; slash = aPath.find('/', slash + 1)) {
				result.emplace_back(aPath.substr(0, slash));
			}
			return result;
		}

		auto join(const std::string& aDirectory, const std::string_view aName) -> std::string
		{
			return aDirectory.empty() ? std::string{ aName } : aDirectory + "/" + std::string{ aName };
		}

		/// One rule file in a directory, read as capture's `rule_file` reads it:
		/// only a regular file, through the session. A file over capture's bound
		/// is refused, as capture refuses it.
		auto rule_file(workspace_root_session& aSession, const std::string& aDirectory, const std::string_view aName)
			-> std::expected<std::optional<std::string>, candidate_refusal>
		{
			const auto listing = aSession.list(aDirectory, { });
			if (!listing) {
				return std::nullopt;
			}
			const auto found = std::ranges::find(*listing, aName, &application::workspace_entry_observation::Name);
			if (found == listing->end() || found->Kind != application::workspace_entry_kind::file) {
				return std::nullopt;
			}
			const auto path = join(aDirectory, aName);
			if (found->Size > maximum_rule_file_bytes) {
				return std::unexpected("rule file over 1 MiB: " + path);
			}
			auto       contents = std::string{ };
			const auto read     = aSession.read(path, maximum_rule_file_bytes, [&contents](const std::span<const std::uint8_t> aChunk) {
                contents.append(aChunk.begin(), aChunk.end());
                return true; }, { });
			if (!read) {
				return std::nullopt;
			}
			return std::optional{ std::move(contents) };
		}

		auto read_info(workspace_root_session& aSession, const std::string_view aPath) -> std::expected<std::string, candidate_refusal>
		{
			const auto bytes = aSession.read_git(workspace_git_area::common, aPath, maximum_rule_file_bytes, { });
			if (!bytes) {
				if (bytes.error() == workspace_failure::missing) {
					return std::string{ };
				}
				return std::unexpected(std::string{ aPath } + ": " + failure_name(bytes.error()));
			}
			return std::string{ bytes->begin(), bytes->end() };
		}

		class first_party_candidate final : public git_metadata_candidate
		{
		public:
			[[nodiscard]] auto name() const -> std::string override
			{
				return "K";
			}

			auto index(const std::filesystem::path& aRoot) -> std::expected<index_answer, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				const auto bytes = (*session)->read_git(workspace_git_area::worktree, "index", application::maximum_workspace_metadata_bytes, { });
				if (!bytes) {
					if (bytes.error() == workspace_failure::missing) {
						return index_answer{ };
					}
					return std::unexpected("index: " + failure_name(bytes.error()));
				}
				if (bytes->empty()) {
					return index_answer{ };
				}
				const auto parsed = core::parse_git_index(*bytes, format->ObjectFormat, core::maximum_cycle_capture_entries);
				if (!parsed) {
					return std::unexpected(application::cycle_repository_words(core::git_index_problem(parsed.error())));
				}
				auto answer = index_answer{ .Partial = parsed->SplitIndex };
				answer.Entries.reserve(parsed->Entries.size());
				for (const auto& entry : parsed->Entries) {
					answer.Entries.push_back({ .Path         = entry.Path,
											   .Mode         = entry.Mode,
											   .Object       = std::string{ core::canonical_hex(entry.Object).view() },
											   .Stage        = entry.Stage,
											   .SkipWorktree = entry.SkipWorktree,
											   .IntentToAdd  = entry.IntentToAdd });
				}
				return answer;
			}

			auto format(const std::filesystem::path& aRoot) -> std::expected<format_answer, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				// A reftable repository is refused above, so what is read is files.
				return format_answer{ .ObjectFormat = hash_name(format->ObjectFormat), .ReferenceFormat = "files", .IgnoreCase = format->IgnoreCase };
			}

			auto head(const std::filesystem::path& aRoot) -> std::expected<head_answer, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				const auto head = application::read_git_head(**session, format->ObjectFormat, { });
				if (!head) {
					return std::unexpected("HEAD: " + failure_name(head.error()));
				}
				auto answer     = head_answer{ };
				answer.Symbolic = head->Reference;
				if (head->Commit) {
					answer.Commit = std::string{ core::canonical_hex(*head->Commit).view() };
				}
				return answer;
			}

			auto reference(const std::filesystem::path& aRoot, const std::string& aName) -> std::expected<std::optional<std::string>, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				const auto resolved = application::resolve_reference(**session, aName, format->ObjectFormat, { });
				if (resolved) {
					return std::optional{ std::string{ core::canonical_hex(*resolved).view() } };
				}
				if (resolved.error()) {
					return std::unexpected(aName + ": " + failure_name(*resolved.error()));
				}
				return std::optional<std::string>{ };
			}

			auto discover(const std::filesystem::path& aRoot) -> std::expected<discovery_answer, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto& observed = (*session)->observation();
				if (!observed.Repository || !observed.Worktree) {
					return std::unexpected(std::string{ "no Git directories observed" });
				}
				// POSIX identities are the device and inode the adapter recorded.
				return discovery_answer{ .LinkedWorktree          = observed.Kind == core::workspace_root_kind::git_linked_worktree,
										 .GitDirectoryIdentity    = file_identity{ .Device = observed.Worktree->Volume, .Object = observed.Worktree->ObjectLow },
										 .CommonDirectoryIdentity = file_identity{ .Device = observed.Repository->Volume, .Object = observed.Repository->ObjectLow } };
			}

			auto ignored(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<bool>, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				const auto exclude = read_info(**session, "info/exclude");
				if (!exclude) {
					return std::unexpected(exclude.error());
				}
				// As capture's scope pass: info/exclude first, then each directory's
				// `.gitignore` once, parents before children, as the walk reaches it.
				auto rules = core::workspace_ignore_rules{ format->IgnoreCase };
				if (!rules.add("", *exclude)) {
					return std::unexpected(std::string{ "info/exclude past the rule bound" });
				}
				auto loaded = std::set<std::string>{ };
				auto load   = [&](const std::string& aDirectory) -> std::optional<candidate_refusal> {
					if (!loaded.insert(aDirectory).second) {
						return std::nullopt;
					}
					auto contents = rule_file(**session, aDirectory, ".gitignore");
					if (!contents) {
						return contents.error();
					}
					if (*contents && !rules.add(aDirectory, **contents)) {
						return "rule bound exceeded at " + join(aDirectory, ".gitignore");
					}
					return std::nullopt;
				};
				auto sorted = std::vector<std::size_t>(aPaths.size());
				for (std::size_t index = 0; index < sorted.size(); ++index) {
					sorted[index] = index;
				}
				// Parents before children: a sorted walk reaches each directory
				// before anything beneath it.
				std::ranges::sort(sorted, [&aPaths](const std::size_t aLeft, const std::size_t aRight) { return aPaths[aLeft].Path < aPaths[aRight].Path; });
				auto answers = std::vector<bool>(aPaths.size(), false);
				for (const auto position : sorted) {
					const auto& question = aPaths[position];
					if (auto problem = load("")) {
						return std::unexpected(std::move(*problem));
					}
					auto parentIgnored = false;
					for (const auto& directory : ancestors(question.Path)) {
						// A directory Git excludes is never entered: its own
						// `.gitignore` is not read and everything beneath is ignored.
						if (rules.ignored(directory, true)) {
							parentIgnored = true;
							break;
						}
						if (auto problem = load(directory)) {
							return std::unexpected(std::move(*problem));
						}
					}
					answers[position] = parentIgnored || rules.ignored(question.Path, question.Directory);
				}
				return answers;
			}

			auto filter(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<attribute_answer>, candidate_refusal> override
			{
				auto session = open_git(aRoot);
				if (!session) {
					return std::unexpected(session.error());
				}
				const auto format = application::read_git_repository_format(**session, { });
				if (!format) {
					return std::unexpected(describe(format.error()));
				}
				const auto info = read_info(**session, "info/attributes");
				if (!info) {
					return std::unexpected(info.error());
				}
				auto rules = core::workspace_attribute_rules{ format->IgnoreCase };
				if (!rules.add_info(*info)) {
					return std::unexpected(std::string{ "info/attributes past the rule bound" });
				}
				// Every directory's `.gitattributes`, parents before children.
				auto directories = std::set<std::string>{ "" };
				for (const auto& question : aPaths) {
					for (auto& directory : ancestors(question.Path)) {
						directories.insert(std::move(directory));
					}
				}
				for (const auto& directory : directories) {
					auto contents = rule_file(**session, directory, ".gitattributes");
					if (!contents) {
						return std::unexpected(contents.error());
					}
					if (*contents && !rules.add(directory, **contents)) {
						return std::unexpected("rule bound exceeded at " + join(directory, ".gitattributes"));
					}
				}
				auto answers = std::vector<attribute_answer>{ };
				answers.reserve(aPaths.size());
				for (const auto& question : aPaths) {
					const auto state = rules.filter(question.Path);
					switch (state.Kind) {
					case core::git_attribute_state::kind::unspecified: answers.emplace_back("unspecified"); break;
					case core::git_attribute_state::kind::set:         answers.emplace_back("set"); break;
					case core::git_attribute_state::kind::unset:       answers.emplace_back("unset"); break;
					case core::git_attribute_state::kind::value:       answers.push_back(state.Value); break;
					}
				}
				return answers;
			}

		private:
			auto open_git(const std::filesystem::path& aRoot) -> std::expected<std::unique_ptr<workspace_root_session>, candidate_refusal>
			{
				auto session = Source->open(aRoot.string(), { });
				if (!session) {
					return std::unexpected("open: " + failure_name(session.error()));
				}
				if (!git_root((*session)->observation().Kind)) {
					return std::unexpected(std::string{ "not a Git root" });
				}
				return std::move(*session);
			}

			std::unique_ptr<application::workspace_source_port> Source = adapters::workspace::make_local_workspace_source();
		};
	}

	auto make_first_party_candidate() -> std::unique_ptr<git_metadata_candidate>
	{
		return std::make_unique<first_party_candidate>();
	}
}
