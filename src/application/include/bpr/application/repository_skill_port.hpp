#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPOSITORY_SKILL_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REPOSITORY_SKILL_PORT_HPP

#include "bpr/application/publication_candidate_port.hpp"
#include "bpr/core/git_objects.hpp"

#include <array>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1916]]. The reviewed repository's skills, read from Git
/// objects at one commit: the pull request's base, never its head.
namespace bpr::application {
	/// \brief Where a repository keeps skills, each `ROOT/NAME/SKILL.md`.
	inline constexpr std::array<std::string_view, 3> repository_skill_roots = { ".claude/skills", ".agents/skills", ".codex/skills" };
	/// \brief The most skill folders one commit is read for.
	inline constexpr std::size_t maximum_repository_skills = 64U;
	/// \brief The most supporting files listed for one skill folder.
	inline constexpr std::size_t maximum_repository_skill_files = 24U;

	/// \brief One `SKILL.md` as the commit holds it.
	struct repository_skill_file
	{
		/// One of `repository_skill_roots`.
		std::string Root;
		/// The skill's folder name under the root.
		std::string Folder;
		/// The `SKILL.md` bytes, at most `core::maximum_review_skill_bytes`.
		std::string Text;
		/// The folder's other files, relative to it.
		std::vector<std::string> Supporting;

		auto operator<=>(const repository_skill_file&) const = default;
	};

	/// \brief Reads a commit's skills from a checkout's objects.
	///
	/// \details The checkout's objects are a read-only alternate of a private
	/// store; its working tree, index, branches and configuration are never
	/// read or changed, so what is checked out (the pull request's head, say)
	/// changes nothing. A `SKILL.md` that is a link, a submodule or over the
	/// bound is skipped. A commit the checkout does not hold is
	/// `object_unavailable`.
	class repository_skill_port
	{
	public:
		virtual ~repository_skill_port() = default;

		[[nodiscard]] virtual auto read_skills(const publication_repository& aRepository, const core::git_commit_oid& aCommit, std::stop_token aStop)
			-> publication_git_result<std::vector<repository_skill_file>> = 0;
	};
}

#endif
