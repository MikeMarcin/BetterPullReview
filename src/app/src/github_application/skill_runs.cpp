#include "skill_runs.hpp"

#include <QDir>

namespace bpr::app {
	auto prepare_skill_run(const application::skill_library& aLibrary, const std::span<const std::string_view> aTexts) -> skill_run
	{
		auto       run      = skill_run{ };
		const auto guidance = application::resolve_skill_guidance(aLibrary, aTexts);
		run.Note            = guidance.Note;
		if (guidance.empty()) {
			return run;
		}
		run.Inline    = application::inline_skills(aLibrary, guidance);
		run.Directive = std::string{ application::inline_skill_directive() };
		return run;
	}

	auto skill_roots_for(const std::optional<std::filesystem::path>& aFixtureHome, const bool aFixture) -> std::vector<application::personal_skill_root>
	{
		if (aFixtureHome) {
			return application::personal_skill_roots(*aFixtureHome);
		}
		if (aFixture) {
			return { };
		}
		return application::personal_skill_roots(std::filesystem::path{ QDir::homePath().toStdU16String() });
	}
}
