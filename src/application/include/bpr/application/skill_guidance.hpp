#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SKILL_GUIDANCE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SKILL_GUIDANCE_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/core/skill_references.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1916]], RFC-BPR-0092 amendment. How a skill a prompt names
/// reaches the run.
///
/// \details One mechanism for every run and agent kind (owner, 2026-11-06),
/// recorded in the
/// [AI agent boundary](../../../../../docs/architecture/ai-agent-boundary.md#skills-a-prompt-names-bpr-1916):
/// the host reads each skill the prompt names and its `SKILL.md` rides in the
/// request's supplement as its own escaped `skill.N.*` fields, its supporting
/// files named but not loaded. No run gets the `Skill` tool, a plugin or a
/// setting source for it: a person's Claude wrapper can replace the settings
/// this application passes, so native loading could not be relied on. Only
/// skills the prompt names are carried; a skill no prompt names never
/// reaches a run, whatever its description says.
namespace bpr::application {
	/// \brief The most of one `SKILL.md` a prompt carries.
	inline constexpr std::size_t maximum_skill_document_bytes = 32U * 1024U;
	/// \brief The most supporting files one skill lists.
	inline constexpr std::size_t maximum_skill_supporting_files = 24U;

	/// \brief One skill as the host read it.
	struct skill_document
	{
		core::skill_entry Entry;
		/// The whole `SKILL.md`, at most `core::maximum_review_skill_bytes`.
		std::string Text;
		/// The skill folder's other files, relative to it, sorted.
		std::vector<std::string> Supporting;

		auto operator<=>(const skill_document&) const = default;
	};

	/// \brief A skill document from one `SKILL.md`, or nothing when its front
	/// matter names no skill. The name is the front matter's when it is a
	/// valid skill name, otherwise the folder's when that is.
	[[nodiscard]] auto skill_document_of(std::string_view aText, std::string_view aFolder, core::skill_source aSource, std::string aOrigin, std::vector<std::string> aSupporting)
		-> std::optional<skill_document>;

	/// \brief Every skill a prompt can name: `Catalog` is
	/// `core::merge_skill_catalog`'s order and `Documents` is parallel to it.
	struct skill_library
	{
		std::vector<core::skill_entry> Catalog;
		std::vector<skill_document>    Documents;
		/// Why the repository's skills are missing, when they are.
		std::optional<std::string> RepositoryNote;

		auto operator<=>(const skill_library&) const = default;
	};
	[[nodiscard]] auto make_skill_library(std::vector<skill_document> aPersonal, std::vector<skill_document> aRepository, std::optional<std::string> aRepositoryNote = std::nullopt)
		-> skill_library;

	/// \brief The skills some prompt texts name, and the note for names that
	/// matched none.
	struct skill_guidance
	{
		std::vector<std::size_t>   Skills;
		std::optional<std::string> Note;

		[[nodiscard]] auto empty() const noexcept -> bool { return Skills.empty(); }
		auto               operator<=>(const skill_guidance&) const = default;
	};
	[[nodiscard]] auto resolve_skill_guidance(const skill_library& aLibrary, std::span<const std::string_view> aTexts) -> skill_guidance;

	/// \brief The named skills as the supplement carries them inline
	/// (`skill.N.*` fields), each `SKILL.md` cut to
	/// `maximum_skill_document_bytes` at a character boundary.
	[[nodiscard]] auto inline_skills(const skill_library& aLibrary, const skill_guidance& aGuidance) -> std::vector<core::supplement_skill>;
	/// \brief The line a prompt with inline skills ends with, telling the
	/// run what the `skill.N` fields are.
	[[nodiscard]] auto inline_skill_directive() -> std::string_view;

	/// \brief [[BPR-1917]]. What a writing hook's run receives, for the
	/// settings preview: the contract, the instructions, the skills they name
	/// as the run would get them, and where the request's evidence goes.
	struct writing_hook_preview
	{
		std::string    Text;
		skill_guidance Guidance;
		auto           operator<=>(const writing_hook_preview&) const = default;
	};
	[[nodiscard]] auto preview_writing_hook(core::writing_hook aHook, std::string_view aInstructions, const skill_library& aLibrary) -> writing_hook_preview;
}

#endif
