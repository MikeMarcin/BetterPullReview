#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_SKILL_REFERENCES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_SKILL_REFERENCES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1916]], RFC-BPR-0092 amendment. `/skill-name` in a prompt
/// the person writes: where the reference is, which skill it names, and
/// what completion offers while it is typed.
///
/// \details The owner, 2026-11-06: "I wonder if I can't have a hook to let
/// me reference a custom skill in my prompts if the skill is available in as
/// a personal user skill or a skill in the repo being reviewed." Everything
/// here is a value and a pure decision over a catalog the host read: the
/// person's own skills, and the reviewed repository's skills as they are on
/// the base side, never the pull request's head. The host reads the files
/// and decides nothing; Qt shows the completion these return.
namespace bpr::core {
	/// \brief Where a skill was found.
	enum class skill_source : std::uint8_t {
		/// The person's own skills (`~/.claude/skills`, `~/.agents/skills`,
		/// `~/.codex/skills`).
		personal,
		/// The reviewed repository's skills, read at the pull request's base
		/// commit from the linked or kept clone.
		repository,
	};
	/// \brief `personal` or `repository`.
	[[nodiscard]] auto stable_name(skill_source aSource) noexcept -> std::string_view;

	/// \brief The longest skill name, as Claude Code's skills allow.
	inline constexpr std::size_t maximum_skill_name_bytes = 64U;
	/// \brief The most skills one prompt brings in; the rest are noted.
	inline constexpr std::size_t maximum_referenced_skills = 4U;
	/// \brief The most catalog entries kept from one source.
	inline constexpr std::size_t maximum_catalog_skills = 128U;
	/// \brief The qualifier that picks the repository's skill over a
	/// personal one of the same name: `/repo:name`.
	inline constexpr std::string_view repository_skill_qualifier = "repo:";
	/// \brief The qualifier that names the personal skill explicitly.
	inline constexpr std::string_view personal_skill_qualifier = "personal:";

	/// \brief Lowercase letters, digits and hyphens, at most
	/// `maximum_skill_name_bytes`, starting with a letter or digit.
	[[nodiscard]] auto valid_skill_name(std::string_view aName) noexcept -> bool;

	/// \brief One skill the host found, with what completion shows for it.
	struct skill_entry
	{
		std::string Name;
		/// The front matter's `description`, bounded by the host.
		std::string  Description;
		skill_source Source = skill_source::personal;
		/// Where it came from, as shown: `~/.claude/skills`, or
		/// `.claude/skills at base 1a2b3c4`.
		std::string Origin;

		auto operator<=>(const skill_entry&) const = default;
	};

	/// \brief One `/name` in a text: the bytes it spans, from the slash, and
	/// the name with any qualifier taken off.
	struct skill_reference
	{
		std::size_t                 Begin = 0U;
		std::size_t                 End   = 0U;
		std::string                 Name;
		std::optional<skill_source> Qualifier;

		auto operator<=>(const skill_reference&) const = default;
	};

	/// \brief Every `/name` in a text, in order.
	///
	/// \details A reference starts with `/` at the start of the text or after
	/// whitespace or one of `(["'`, may carry `repo:` or `personal:`, and
	/// ends where the name ends at the end of the text, whitespace, or
	/// punctuation other than `/`, `_` and a `.` that a word follows. So
	/// `/usr/bin`, `a/b`, `/name.md` and `https://x` are not references.
	[[nodiscard]] auto find_skill_references(std::string_view aText) -> std::vector<skill_reference>;

	/// \brief The skills a set of texts names, resolved against a catalog.
	struct skill_resolution
	{
		/// Indexes into the catalog, in first-reference order, each once, at
		/// most `maximum_referenced_skills`.
		std::vector<std::size_t> Resolved;
		/// Names no skill answers, each once, in order. Left in the text.
		std::vector<std::string> Unknown;
		/// Skills named past `maximum_referenced_skills`.
		std::size_t Dropped = 0U;

		auto operator<=>(const skill_resolution&) const = default;
	};

	/// \brief Resolves every reference in the texts.
	///
	/// \details Without a qualifier the personal skill wins over a
	/// repository skill with the same name; `/repo:name` picks the
	/// repository's and `/personal:name` the person's. Two entries of one
	/// source with one name resolve to the first.
	[[nodiscard]] auto resolve_skill_references(std::span<const std::string_view> aTexts, std::span<const skill_entry> aCatalog) -> skill_resolution;

	/// \brief The quiet note for names that matched no skill, or nothing.
	[[nodiscard]] auto skill_resolution_note(const skill_resolution& aResolution) -> std::optional<std::string>;

	/// \brief Where completion is being typed: the slash and the text after
	/// it up to the cursor.
	struct skill_completion_context
	{
		std::size_t Begin = 0U;
		std::string Prefix;

		auto operator<=>(const skill_completion_context&) const = default;
	};
	/// \brief The reference being typed at a cursor, or nothing when the
	/// cursor is not inside one.
	[[nodiscard]] auto skill_completion_at(std::string_view aText, std::size_t aCursor) -> std::optional<skill_completion_context>;

	/// \brief One completion row.
	struct skill_completion
	{
		/// What replaces the typed text after the slash.
		std::string Insert;
		/// The catalog entry it names.
		std::size_t Entry = 0U;
		/// A repository skill a personal skill of the same name hides; it is
		/// inserted with `repo:`.
		bool Shadowed = false;

		auto operator<=>(const skill_completion&) const = default;
	};
	/// \brief The rows for a typed prefix (after the slash): every entry
	/// whose name starts with it, by name, the personal one first.
	[[nodiscard]] auto complete_skill_references(std::string_view aPrefix, std::span<const skill_entry> aCatalog) -> std::vector<skill_completion>;

	/// \brief The catalog completion and resolution read: personal entries
	/// then repository entries, each source sorted by name, each name once
	/// per source, each source at most `maximum_catalog_skills`, and only
	/// valid names.
	[[nodiscard]] auto merge_skill_catalog(std::vector<skill_entry> aPersonal, std::vector<skill_entry> aRepository) -> std::vector<skill_entry>;
}

#endif
