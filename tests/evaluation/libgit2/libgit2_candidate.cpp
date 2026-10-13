#include "libgit2_candidate.hpp"

#include <git2.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::tests::workspace_contract {
	namespace {
		struct repository_release
		{
			void operator()(git_repository* aRepository) const noexcept
			{
				git_repository_free(aRepository);
			}
		};
		struct index_release
		{
			void operator()(git_index* aIndex) const noexcept
			{
				git_index_free(aIndex);
			}
		};
		struct reference_release
		{
			void operator()(git_reference* aReference) const noexcept
			{
				git_reference_free(aReference);
			}
		};
		struct config_release
		{
			void operator()(git_config* aConfig) const noexcept
			{
				git_config_free(aConfig);
			}
		};
		using repository_handle = std::unique_ptr<git_repository, repository_release>;
		using index_handle      = std::unique_ptr<git_index, index_release>;
		using reference_handle  = std::unique_ptr<git_reference, reference_release>;
		using config_handle     = std::unique_ptr<git_config, config_release>;

		/// libgit2's last error for this thread, as words for the report.
		auto refusal(const char* aWhat) -> std::unexpected<candidate_refusal>
		{
			const git_error* error = git_error_last();
			std::string      text  = aWhat;
			if (error != nullptr && error->message != nullptr) {
				text += ": ";
				text += error->message;
			}
			return std::unexpected(std::move(text));
		}

		/// \brief Process-global libgit2 state, set once: initialized, with
		/// the system, XDG, global and ProgramData search paths and the home
		/// directory empty, so only the repository's own files are read.
		/// \details `GIT_OPT_SET_SEARCH_PATH` with a null path restores the
		/// guessed default, so each level gets an empty string instead.
		auto initialization() -> const std::optional<candidate_refusal>&
		{
			static const std::optional<candidate_refusal> failure = []() -> std::optional<candidate_refusal> {
				if (git_libgit2_init() < 0) {
					return refusal("git_libgit2_init").error();
				}
				constexpr std::array levels{ GIT_CONFIG_LEVEL_SYSTEM, GIT_CONFIG_LEVEL_XDG, GIT_CONFIG_LEVEL_GLOBAL, GIT_CONFIG_LEVEL_PROGRAMDATA };
				for (const git_config_level_t level : levels) {
					if (git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, static_cast<int>(level), "") < 0) {
						return refusal("GIT_OPT_SET_SEARCH_PATH").error();
					}
				}
				if (git_libgit2_opts(GIT_OPT_SET_HOMEDIR, "") < 0) {
					return refusal("GIT_OPT_SET_HOMEDIR").error();
				}
				return std::nullopt;
			}();
			return failure;
		}

		/// Opens exactly the checkout at `aRoot`: no upward search and no
		/// environment overrides.
		auto open(const std::filesystem::path& aRoot) -> std::expected<repository_handle, candidate_refusal>
		{
			if (const auto& failure = initialization(); failure.has_value()) {
				return std::unexpected(*failure);
			}
			git_repository* repository = nullptr;
			if (git_repository_open_ext(&repository, aRoot.c_str(), GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr) < 0) {
				return refusal("git_repository_open_ext");
			}
			return repository_handle(repository);
		}

		auto hexadecimal(const git_oid& aObject) -> std::string
		{
			std::array<char, GIT_OID_MAX_HEXSIZE + 1> text{ };
			git_oid_tostr(text.data(), text.size(), &aObject);
			return std::string(text.data());
		}

		auto trimmed_directory(const char* aPath) -> std::filesystem::path
		{
			std::string text = aPath == nullptr ? std::string() : std::string(aPath);
			while (text.size() > 1 && text.back() == '/') {
				text.pop_back();
			}
			return std::filesystem::path(text);
		}

		auto index_entries(git_index& aIndex) -> index_answer
		{
			index_answer      answer;
			const std::size_t count = git_index_entrycount(&aIndex);
			answer.Entries.reserve(count);
			for (std::size_t position = 0; position < count; ++position) {
				const git_index_entry* entry = git_index_get_byindex(&aIndex, position);
				answer.Entries.push_back({
					.Path         = entry->path,
					.Mode         = entry->mode,
					.Object       = hexadecimal(entry->id),
					.Stage        = static_cast<std::uint8_t>(GIT_INDEX_ENTRY_STAGE(entry)),
					.SkipWorktree = (entry->flags_extended & GIT_INDEX_ENTRY_SKIP_WORKTREE) != 0,
					.IntentToAdd  = (entry->flags_extended & GIT_INDEX_ENTRY_INTENT_TO_ADD) != 0,
				});
			}
			return answer;
		}

		auto attribute_text(const char* aValue) -> attribute_answer
		{
			switch (git_attr_value(aValue)) {
			case GIT_ATTR_VALUE_TRUE:
				return "set";
			case GIT_ATTR_VALUE_FALSE:
				return "unset";
			case GIT_ATTR_VALUE_STRING:
				return aValue;
			case GIT_ATTR_VALUE_UNSPECIFIED:
				break;
			}
			return "unspecified";
		}

		class libgit2_candidate final : public git_metadata_candidate
		{
		public:
			[[nodiscard]] auto name() const -> std::string override
			{
				return "L";
			}

			auto index(const std::filesystem::path& aRoot) -> std::expected<index_answer, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				git_index* index = nullptr;
				if (git_repository_index(&index, repository->get()) < 0) {
					return refusal("git_repository_index");
				}
				const index_handle owned(index);
				return index_entries(*owned);
			}

			auto format(const std::filesystem::path& aRoot) -> std::expected<format_answer, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				format_answer answer;
				answer.ObjectFormat = git_repository_oid_type(repository->get()) == GIT_OID_SHA1 ? "sha1" : "sha256";
				// libgit2 1.9 has only the files backend and refuses any other
				// refStorage, so an opened repository is a files repository.
				answer.ReferenceFormat = "files";
				git_config* config     = nullptr;
				if (git_repository_config_snapshot(&config, repository->get()) < 0) {
					return refusal("git_repository_config_snapshot");
				}
				const config_handle owned(config);
				int                 ignoreCase = 0;
				if (const int status = git_config_get_bool(&ignoreCase, owned.get(), "core.ignorecase"); status < 0 && status != GIT_ENOTFOUND) {
					return refusal("core.ignorecase");
				}
				answer.IgnoreCase = ignoreCase != 0;
				return answer;
			}

			auto head(const std::filesystem::path& aRoot) -> std::expected<head_answer, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				git_reference* head = nullptr;
				if (git_reference_lookup(&head, repository->get(), "HEAD") < 0) {
					return refusal("HEAD");
				}
				const reference_handle owned(head);
				head_answer            answer;
				if (git_reference_type(owned.get()) == GIT_REFERENCE_SYMBOLIC) {
					answer.Symbolic = git_reference_symbolic_target(owned.get());
				}
				git_oid object{ };
				if (const int status = git_reference_name_to_id(&object, repository->get(), "HEAD"); status == 0) {
					answer.Commit = hexadecimal(object);
				} else if (status != GIT_ENOTFOUND) {
					return refusal("HEAD resolution");
				}
				return answer;
			}

			auto reference(const std::filesystem::path& aRoot, const std::string& aName) -> std::expected<std::optional<std::string>, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				git_oid   object{ };
				const int status = git_reference_name_to_id(&object, repository->get(), aName.c_str());
				if (status == GIT_ENOTFOUND) {
					return std::optional<std::string>();
				}
				if (status < 0) {
					return refusal("reference resolution");
				}
				return std::optional<std::string>(hexadecimal(object));
			}

			auto discover(const std::filesystem::path& aRoot) -> std::expected<discovery_answer, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				return discovery_answer{
					.GitDirectory    = trimmed_directory(git_repository_path(repository->get())),
					.CommonDirectory = trimmed_directory(git_repository_commondir(repository->get())),
					.LinkedWorktree  = git_repository_is_worktree(repository->get()) == 1,
				};
			}

			auto ignored(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<bool>, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				std::vector<bool> answers;
				answers.reserve(aPaths.size());
				for (const path_question& question : aPaths) {
					// A trailing `/` is libgit2's own way to say "directory".
					const std::string path    = question.Directory ? question.Path + "/" : question.Path;
					int               ignored = 0;
					if (git_ignore_path_is_ignored(&ignored, repository->get(), path.c_str()) < 0) {
						return refusal("git_ignore_path_is_ignored");
					}
					answers.push_back(ignored != 0);
				}
				return answers;
			}

			auto filter(const std::filesystem::path& aRoot, const std::vector<path_question>& aPaths) -> std::expected<std::vector<attribute_answer>, candidate_refusal> override
			{
				auto repository = open(aRoot);
				if (!repository) {
					return std::unexpected(repository.error());
				}
				std::vector<attribute_answer> answers;
				answers.reserve(aPaths.size());
				for (const path_question& question : aPaths) {
					const char* value = nullptr;
					// Working tree first, then the index, as `git check-attr`
					// reads them; never the system attributes file.
					if (git_attr_get(&value, repository->get(), GIT_ATTR_CHECK_FILE_THEN_INDEX | GIT_ATTR_CHECK_NO_SYSTEM, question.Path.c_str(), "filter") < 0) {
						return refusal("git_attr_get");
					}
					answers.push_back(attribute_text(value));
				}
				return answers;
			}
		};
	}

	auto make_libgit2_candidate() -> std::unique_ptr<git_metadata_candidate>
	{
		return std::make_unique<libgit2_candidate>();
	}
}
