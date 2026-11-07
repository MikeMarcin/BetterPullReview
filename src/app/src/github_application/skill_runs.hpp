#ifndef BPR_APP_GITHUB_APPLICATION_SKILL_RUNS_HPP
#define BPR_APP_GITHUB_APPLICATION_SKILL_RUNS_HPP

#include "bpr/application/skill_catalog.hpp"
#include "bpr/application/skill_guidance.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1916]]. What one run is given for the skills its prompt
/// names, as the composition decides it.
namespace bpr::app {
	/// \brief The skills a run's prompt names, carried inline.
	struct skill_run
	{
		/// The supplement's `skill.N` fields: only the skills named.
		std::vector<core::supplement_skill> Inline;
		/// What the prompt ends with, saying what those fields are.
		std::string Directive;
		/// The quiet note for names that matched no skill.
		std::optional<std::string> Note;
	};

	/// \brief Resolves the texts' `/name` references against the library and
	/// carries the named skills inline, for any run and agent kind. Nothing
	/// named, nothing given.
	[[nodiscard]] auto prepare_skill_run(const application::skill_library& aLibrary, std::span<const std::string_view> aTexts) -> skill_run;

	/// \brief The person's skill folders: under their home, or for a fixture
	/// composition only the fixture home it names, never the person's own.
	[[nodiscard]] auto skill_roots_for(const std::optional<std::filesystem::path>& aFixtureHome, bool aFixture) -> std::vector<application::personal_skill_root>;
}

#endif
