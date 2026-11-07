#include "github_application/window_composition.hpp"

#include "bpr/adapters/git/installed_git_publication.hpp"
#include "bpr/adapters/sqlite/storage_root.hpp"
#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/review_tour_editor.hpp"
#include "bpr/qt/tour_agent_panel.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"
#include "github_application/profile_hosts.hpp"

#include <array>
#include <string_view>

// [[BPR-1916]]. The skills a prompt can name: loading the catalog, showing it
// to completion, and carrying the named skills to a run.
namespace bpr::app {
	void github_application::implementation::compose_skill_catalog()
	{
		// The repository's skills are read by a private installed-Git catalog
		// of their own, from the linked clone's objects at the base commit.
		SkillCatalog = std::make_unique<application::skill_catalog_loader>(
			WorkbenchEffects, DecisionLane,
#if defined(BPR_BUILD_TESTING)
			skill_roots_for(Profile->PersonalSkillHome, Fixture != nullptr),
#else
			skill_roots_for(std::nullopt, false),
#endif
			[git = Profile->InstalledGit, system = windows_system_root(), isolation = ApplicationDataRoot / "skill-git-v1"](std::stop_token aStop) -> std::unique_ptr<application::repository_skill_port> {
				if (git.empty() || !adapters::sqlite::create_storage_root_owner_private(isolation)) {
					return nullptr;
				}
				auto made = adapters::git::make_installed_git_skill_reader({ .GitExecutable = git, .IsolationRoot = isolation, .WindowsSystemRoot = system }, std::move(aStop));
				return made ? std::move(*made) : nullptr;
			});
		refresh_skills();
	}

	void github_application::implementation::refresh_skills()
	{
		if (Closing || !SkillCatalog) {
			return;
		}
		auto source  = std::optional<application::repository_skill_source>{ };
		auto missing = std::string{ };
		if (WindowRead && WindowRead->accepted_snapshot()) {
			const auto& snapshot = WindowRead->accepted_snapshot();
#if defined(BPR_AGENT_INTEGRATIONS)
			const auto clone = linked_history_clone(false);
#else
			const auto clone = std::optional<std::filesystem::path>{ };
#endif
			const auto base = core::parse_git_commit_oid(snapshot->PullRequest.BaseOid.value());
			if (!clone) {
				missing = "Link a clone of this repository in Settings, Repositories to list its skills. They are read at the pull request's base.";
			} else if (!base) {
				missing = "The pull request's base commit isn't known yet, so the repository's skills aren't listed.";
			} else {
				source = application::repository_skill_source{ .Repository = { .Checkout = clone->string(), .Algorithm = base->algorithm() }, .Base = *base };
			}
		}
		SkillCatalog->load(std::move(source), std::move(missing), [this](const application::skill_library& aLibrary) { present_skills(aLibrary); });
	}

	void github_application::implementation::present_skills(const application::skill_library& aLibrary)
	{
		if (Closing || Window == nullptr) {
			return;
		}
		SkillLibrary = aLibrary;
		Window->writing_hooks()->present_skills(SkillLibrary);
		if (auto* const tab = Window->agent_tab(); tab != nullptr) {
			tab->present_skills(SkillLibrary.Catalog);
		}
		if (auto* const tour = Window->tour_editor() != nullptr ? Window->tour_editor()->tour_agent() : nullptr; tour != nullptr) {
			tour->present_skills(SkillLibrary.Catalog);
		}
	}

	auto github_application::implementation::skill_run_for(const core::writing_hook aHook) const -> skill_run
	{
		const auto& prefs = Profile->WorkbenchPreferences->accepted();
		if (!prefs) {
			return { };
		}
		const auto text  = application::effective_writing_hook(prefs->WritingHooks, aHook);
		const auto texts = std::array{ text };
		return prepare_skill_run(SkillLibrary, texts);
	}
}
