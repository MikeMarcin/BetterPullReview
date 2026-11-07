#include "bpr/application/skill_catalog.hpp"

#include "bpr/core/epoch_model.hpp"
#include "bpr/core/review_skill.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto path_text(const std::filesystem::path& aPath) -> std::string
		{
			const auto text = aPath.generic_u8string();
			return std::string{ reinterpret_cast<const char*>(text.data()), text.size() };
		}

		/// A regular file's bytes when it is at most `aLimit`; nothing for a
		/// link, a folder, a larger file or a read that failed.
		[[nodiscard]] auto read_bounded(const std::filesystem::path& aPath, const std::size_t aLimit) -> std::optional<std::string>
		{
			std::error_code error;
			const auto      status = std::filesystem::symlink_status(aPath, error);
			if (error || !std::filesystem::is_regular_file(status)) {
				return std::nullopt;
			}
			const auto size = std::filesystem::file_size(aPath, error);
			if (error || size > aLimit) {
				return std::nullopt;
			}
			std::ifstream input{ aPath, std::ios::binary };
			auto          text = std::string{ std::istreambuf_iterator<char>{ input }, std::istreambuf_iterator<char>{ } };
			if (input.bad() || text.size() > aLimit) {
				return std::nullopt;
			}
			return text;
		}

		/// The skill folder's other files, relative to it, three folders
		/// deep, no link followed and nothing hidden.
		[[nodiscard]] auto supporting_files(const std::filesystem::path& aFolder) -> std::vector<std::string>
		{
			auto            files = std::vector<std::string>{ };
			std::error_code error;
			auto            walk = std::filesystem::recursive_directory_iterator{ aFolder, std::filesystem::directory_options::none, error };
			for (; !error && walk != std::filesystem::recursive_directory_iterator{ }; walk.increment(error)) {
				const auto name = path_text(walk->path().filename());
				if (name.starts_with('.')) {
					if (walk->is_directory(error)) {
						walk.disable_recursion_pending();
					}
					continue;
				}
				if (walk.depth() >= 2) {
					walk.disable_recursion_pending();
				}
				if (walk->is_symlink(error) || !walk->is_regular_file(error)) {
					continue;
				}
				auto relative = path_text(walk->path().lexically_relative(aFolder));
				if (relative == "SKILL.md") {
					continue;
				}
				files.push_back(std::move(relative));
				if (files.size() == maximum_skill_supporting_files) {
					break;
				}
			}
			return files;
		}
	}

	auto personal_skill_roots(const std::filesystem::path& aHome) -> std::vector<personal_skill_root>
	{
		if (aHome.empty()) {
			return { };
		}
		return {
			{ .Path = aHome / ".claude" / "skills", .Label = "~/.claude/skills" },
			{ .Path = aHome / ".agents" / "skills", .Label = "~/.agents/skills" },
			{ .Path = aHome / ".codex" / "skills", .Label = "~/.codex/skills" },
		};
	}

	auto read_personal_skills(const std::span<const personal_skill_root> aRoots) -> std::vector<skill_document>
	{
		auto documents = std::vector<skill_document>{ };
		for (const auto& root : aRoots) {
			std::error_code error;
			if (!std::filesystem::is_directory(root.Path, error) || error) {
				continue;
			}
			auto folders = std::vector<std::filesystem::path>{ };
			for (auto entry = std::filesystem::directory_iterator{ root.Path, error }; !error && entry != std::filesystem::directory_iterator{ }; entry.increment(error)) {
				// A skill folder may be a link to where a skill manager keeps it.
				if (entry->is_directory(error) && !path_text(entry->path().filename()).starts_with('.')) {
					folders.push_back(entry->path());
				}
			}
			std::ranges::sort(folders);
			auto kept = std::size_t{ 0U };
			for (const auto& folder : folders) {
				if (kept == core::maximum_catalog_skills) {
					break;
				}
				const auto text = read_bounded(folder / "SKILL.md", core::maximum_review_skill_bytes);
				if (!text) {
					continue;
				}
				auto document = skill_document_of(*text, path_text(folder.filename()), core::skill_source::personal, root.Label, supporting_files(folder));
				if (document) {
					documents.push_back(std::move(*document));
					++kept;
				}
			}
		}
		return documents;
	}

	auto repository_skill_documents(const std::span<const repository_skill_file> aFiles, const core::git_commit_oid& aCommit) -> std::vector<skill_document>
	{
		const auto short_commit = std::string{ core::canonical_hex(aCommit).view().substr(0U, 7U) };
		auto       documents    = std::vector<skill_document>{ };
		for (const auto& file : aFiles) {
			auto document = skill_document_of(file.Text, file.Folder, core::skill_source::repository, "this pull request's repository, " + file.Root + " at base " + short_commit, file.Supporting);
			if (document) {
				documents.push_back(std::move(*document));
			}
		}
		return documents;
	}

	struct skill_catalog_loader::implementation final : std::enable_shared_from_this<implementation>
	{
		implementation(effect_executor& aEffects, decision_executor& aDecision, std::vector<personal_skill_root> aPersonal, repository_reader aRepository)
			: Effects{ aEffects }
			, Decision{ aDecision }
			, Personal{ std::move(aPersonal) }
			, Repository{ std::move(aRepository) }
		{ }

		effect_executor&                 Effects;
		decision_executor&               Decision;
		std::vector<personal_skill_root> Personal;
		repository_reader                Repository;
		std::uint64_t                    Generation = 0U;
		std::atomic_bool                 Closing{ false };
		std::atomic_size_t               Outstanding{ 0U };
		std::stop_source                 Stop;
	};

	skill_catalog_loader::skill_catalog_loader(effect_executor& aEffects, decision_executor& aDecision, std::vector<personal_skill_root> aPersonal, repository_reader aRepository)
		: Implementation{ std::make_shared<implementation>(aEffects, aDecision, std::move(aPersonal), std::move(aRepository)) }
	{ }

	skill_catalog_loader::~skill_catalog_loader() { shutdown(); }

	void skill_catalog_loader::load(std::optional<repository_skill_source> aRepository, std::string aMissingRepositoryNote, answered aAnswered)
	{
		auto& loader = *Implementation;
		if (loader.Closing) {
			return;
		}
		const auto generation = ++loader.Generation;
		++loader.Outstanding;
		try {
			loader.Effects.post([self = Implementation, generation, repository = std::move(aRepository), missing = std::move(aMissingRepositoryNote), done = std::move(aAnswered)]() mutable {
				if (self->Closing) {
					--self->Outstanding;
					return;
				}
				auto personal = std::vector<skill_document>{ };
				auto shared   = std::vector<skill_document>{ };
				auto note     = std::optional<std::string>{ };
				try {
					personal = read_personal_skills(self->Personal);
				} catch (...) { }
				if (!repository) {
					note = missing.empty() ? std::nullopt : std::optional{ missing };
				} else {
					try {
						auto reader = self->Repository ? self->Repository(self->Stop.get_token()) : nullptr;
						auto files  = reader ? reader->read_skills(repository->Repository, repository->Base, self->Stop.get_token())
											 : publication_git_result<std::vector<repository_skill_file>>{ std::unexpected{ publication_git_error{ } } };
						if (files) {
							shared = repository_skill_documents(*files, repository->Base);
						} else {
							note = files.error().Failure == publication_git_failure::object_unavailable
								? "The linked clone doesn't have this pull request's base commit, so its repository skills aren't listed. Fetch in the clone to add them."
								: "The repository's skills couldn't be read from the linked clone.";
						}
					} catch (...) {
						note = "The repository's skills couldn't be read from the linked clone.";
					}
				}
				auto library = make_skill_library(std::move(personal), std::move(shared), std::move(note));
				try {
					self->Decision.post([self, generation, result = std::move(library), reply = std::move(done)] {
						--self->Outstanding;
						if (!self->Closing && generation == self->Generation && reply) {
							reply(result);
						}
					});
				} catch (...) {
					--self->Outstanding;
				}
			});
		} catch (...) {
			--loader.Outstanding;
		}
	}

	void skill_catalog_loader::shutdown() noexcept
	{
		Implementation->Closing = true;
		Implementation->Stop.request_stop();
	}

	auto skill_catalog_loader::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0U; }
}
