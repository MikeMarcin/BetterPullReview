#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SKILL_CATALOG_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SKILL_CATALOG_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/repository_skill_port.hpp"
#include "bpr/application/skill_guidance.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// \file
/// \brief [[BPR-1916]]. Reading the skills a prompt can name: the person's
/// own folders, and the reviewed repository's at the pull request's base.
namespace bpr::application {
	/// \brief One folder of personal skills, each `FOLDER/NAME/SKILL.md`.
	struct personal_skill_root
	{
		std::filesystem::path Path;
		/// As shown: `~/.claude/skills`.
		std::string Label;

		auto operator<=>(const personal_skill_root&) const = default;
	};
	/// \brief Where the person's own skills are, under their home folder, in
	/// the order a name is taken from: Claude Code's `~/.claude/skills`, the
	/// shared Agent Skills folder `~/.agents/skills`, and Codex's
	/// `~/.codex/skills`.
	[[nodiscard]] auto personal_skill_roots(const std::filesystem::path& aHome) -> std::vector<personal_skill_root>;

	/// \brief Reads every personal skill under the roots, in their order.
	///
	/// \details A skill folder may be a link (personal skills often are, to
	/// where a skill manager keeps them): the person's own folders are
	/// trusted as their own. Inside a skill folder no link is followed. A
	/// `SKILL.md` over `core::maximum_review_skill_bytes`, not UTF-8 or naming
	/// no skill is skipped; at most `core::maximum_catalog_skills` per root.
	/// Supporting files are listed, at most `maximum_skill_supporting_files`
	/// and three folders deep, never read.
	[[nodiscard]] auto read_personal_skills(std::span<const personal_skill_root> aRoots) -> std::vector<skill_document>;

	/// \brief The repository's skills at a commit as skill documents, each
	/// origin naming its root and the commit: `this pull request's repository,
	/// .claude/skills at base 1a2b3c4`.
	[[nodiscard]] auto repository_skill_documents(std::span<const repository_skill_file> aFiles, const core::git_commit_oid& aCommit) -> std::vector<skill_document>;

	/// \brief Where the repository's skills are read from: a checkout whose
	/// objects hold the base commit, and that commit.
	struct repository_skill_source
	{
		publication_repository Repository;
		core::git_commit_oid   Base;
	};

	/// \brief Loads the catalog on the effect lane and answers on the
	/// decision lane; only the latest load is answered.
	class skill_catalog_loader final
	{
	public:
		using answered = std::function<void(const skill_library&)>;
		/// Makes a reader of repository skills, on the effect lane.
		using repository_reader = std::function<std::unique_ptr<repository_skill_port>(std::stop_token)>;

		skill_catalog_loader(effect_executor& aEffects, decision_executor& aDecision, std::vector<personal_skill_root> aPersonal, repository_reader aRepository);
		~skill_catalog_loader();
		skill_catalog_loader(const skill_catalog_loader&)                    = delete;
		auto operator=(const skill_catalog_loader&) -> skill_catalog_loader& = delete;

		/// Reads the personal skills and, with a source, the repository's.
		/// Without one the library says why the repository has none.
		void               load(std::optional<repository_skill_source> aRepository, std::string aMissingRepositoryNote, answered aAnswered);
		void               shutdown() noexcept;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
