#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_PACK_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_PACK_HPP

#include "bpr/core/prompt_macro.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief RFC-BPR-0125 AC24 and AC26 ([[BPR-2149]]), as amended by the owner
/// on 2026-11-18: a prompt pack, the reviewer's own suggested prompts shared
/// through version 2 of RFC-BPR-0107's exchange, that an import merges in.
///
/// \details The owner: "I want to make sure that we can import that such that
/// it merges in. Aka doesn't replace other different prompts, updates to the
/// prompt pack can update older imports in-place." and "I'm thinking uuids
/// probably make the most sense so that we dont get collisions if multiple
/// people all over are making prompts".
///
/// An import matches each prompt of the file by its UUID, never by its name.
/// Everything here is pure: the host mints every UUID and passes it in, reads
/// and writes the file and the store. Equal inputs give equal plans and
/// equal results.
namespace bpr::core {
	/// \brief Whether `aText` is a lowercase version 4 UUID:
	/// `xxxxxxxx-xxxx-4xxx-[89ab]xxx-xxxxxxxxxxxx`.
	[[nodiscard]] auto valid_prompt_uuid(std::string_view aText) noexcept -> bool;

	/// \brief The most prompts one pack may hold: a profile's own bound.
	inline constexpr std::size_t maximum_prompt_pack_macros = maximum_prompt_macros;
	/// \brief The longest pack label, in bytes.
	inline constexpr std::size_t maximum_prompt_pack_label_bytes = 128U;

	/// \brief Which pack a file is, and which version of it.
	struct prompt_pack_identity
	{
		/// A UUID the exporter's host minted when the pack was first exported.
		std::string Id;
		std::string Label;
		/// Increases with each export of the same pack; 1 for the first.
		std::uint32_t Version = 1U;

		auto operator<=>(const prompt_pack_identity&) const = default;
	};

	/// \brief One prompt as the file carries it: its UUID, place and body.
	/// On and placements are each reviewer's own and do not travel.
	struct portable_prompt_macro
	{
		std::string       Id;
		prompt_place      Place = prompt_place::agent_tab;
		prompt_macro_body Body;

		auto operator<=>(const portable_prompt_macro&) const = default;
	};

	/// \brief Why one prompt of a pack cannot be imported. The rest still can.
	enum class prompt_pack_item_problem : std::uint8_t {
		/// It names a place, About, Ask as, fact, match or variable this
		/// version does not have; `decoded_prompt_macro::Detail` names it.
		unsupported,
		/// Its ID is not a UUID.
		invalid_id,
		/// Its ID is listed twice; only the first is used.
		duplicate,
		/// Its ID is a built-in's.
		built_in_id,
		/// A value the prompt editor would refuse (no name, too long…).
		invalid,
	};
	[[nodiscard]] auto stable_name(prompt_pack_item_problem aProblem) noexcept -> std::string_view;
	[[nodiscard]] auto describe(prompt_pack_item_problem aProblem) noexcept -> std::string_view;

	struct decoded_prompt_macro
	{
		portable_prompt_macro                   Macro;
		std::optional<prompt_pack_item_problem> Problem;
		/// The unknown name, for `unsupported`.
		std::string Detail;

		auto operator<=>(const decoded_prompt_macro&) const = default;
	};

	struct decoded_prompt_pack
	{
		prompt_pack_identity              Pack;
		std::vector<decoded_prompt_macro> Macros;

		auto operator<=>(const decoded_prompt_pack&) const = default;
	};

	/// \brief The exchange's `prompt_pack` member: one JSON object, or none
	/// when a value would not read back (a pack ID that is not a UUID, a
	/// prompt ID that is not, too many prompts).
	[[nodiscard]] auto encode_prompt_pack(const prompt_pack_identity& aPack, std::span<const portable_prompt_macro> aMacros) -> std::optional<std::string>;
	/// \brief Reads the `prompt_pack` member. None makes the whole file
	/// malformed: JSON this version cannot read, a pack ID that is not a
	/// UUID, a label that is not one plain line, version 0, or more than
	/// `maximum_prompt_pack_macros` prompts. An unknown name in one prompt
	/// is that prompt's problem.
	[[nodiscard]] auto decode_prompt_pack(std::string_view aJson) -> std::optional<decoded_prompt_pack>;

	/// \brief Whether one of the reviewer's own prompts is linked to a pack
	/// and its place or body differs from what that pack had.
	[[nodiscard]] auto prompt_pack_edited(const prompt_macro& aMacro) noexcept -> bool;

	/// \brief A pack this profile holds, read from its prompts' links.
	struct prompt_pack_here
	{
		/// The pack's ID, the label it was last written under and the
		/// highest version any of its prompts here was written against.
		prompt_pack_identity     Pack;
		std::vector<std::string> Members;

		auto operator<=>(const prompt_pack_here&) const = default;
	};
	/// \brief Every pack the own prompts of `aStored` are linked to, by
	/// label and then ID; each lists its members in library order.
	[[nodiscard]] auto prompt_packs_here(const prompt_macro_differences& aStored) -> std::vector<prompt_pack_here>;
	/// \brief The identity the next export of `aHere` is written under: its
	/// ID, `aLabel` (its own when empty) and one more than its version.
	[[nodiscard]] auto next_prompt_pack(const prompt_pack_here& aHere, std::string aLabel) -> prompt_pack_identity;

	/// \brief What an export writes and what it records.
	struct prompt_pack_export
	{
		prompt_pack_identity               Pack;
		std::vector<portable_prompt_macro> Macros;
		/// The stored differences after the export: each exported prompt
		/// linked to the pack at its version, and a prompt whose identity
		/// was not a UUID under the new one, every reference rewritten.
		prompt_macro_differences After;

		auto operator<=>(const prompt_pack_export&) const = default;
	};
	/// \brief Exports the own prompts `aChosen` names, in library order.
	///
	/// \details A chosen prompt whose identity is not a UUID takes the next
	/// of `aFresh` (UUIDs the host minted), in its order, everywhere the
	/// differences name it. None when a chosen identity is not one of the
	/// reviewer's own app-wide prompts, `aPack`'s ID is not a UUID, a label
	/// is not one line within its bound, or `aFresh` runs out.
	[[nodiscard]] auto export_prompt_pack(const prompt_macro_differences& aStored, const prompt_pack_identity& aPack, std::span<const std::string> aChosen,
										  std::span<const std::string> aFresh) -> std::optional<prompt_pack_export>;

	/// \brief What one prompt of the file is here.
	enum class prompt_pack_standing : std::uint8_t {
		/// No prompt here has its ID.
		added,
		/// The prompt here is linked to this pack and not edited since: it is
		/// updated in place.
		updated,
		/// The prompt here was edited since, or is not linked to this pack:
		/// Replace or Keep mine.
		conflict,
		/// The prompt here is already what the file says.
		unchanged,
		/// It cannot be imported (`prompt_pack_item_plan::Problem`).
		refused,
	};
	[[nodiscard]] auto stable_name(prompt_pack_standing aStanding) noexcept -> std::string_view;

	struct prompt_pack_item_plan
	{
		prompt_pack_standing                    Standing = prompt_pack_standing::refused;
		std::optional<prompt_pack_item_problem> Problem;
		/// Whether it starts chosen: added, and updated unless the file is
		/// older. A conflict starts at Keep mine.
		bool Default = false;
		/// The prompt here of the same ID.
		std::optional<prompt_macro> Here;

		auto operator<=>(const prompt_pack_item_plan&) const = default;
	};

	/// \brief A prompt here that an earlier version of the pack held and the
	/// file does not: Remove or Keep, never removed silently.
	struct prompt_pack_drop_plan
	{
		prompt_macro Here;
		bool         Edited = false;
		/// Remove starts chosen when it is not edited and the file is not older.
		bool Default = false;

		auto operator<=>(const prompt_pack_drop_plan&) const = default;
	};

	struct prompt_pack_import_plan
	{
		/// One per prompt of the file, in its order.
		std::vector<prompt_pack_item_plan> Items;
		/// In library order.
		std::vector<prompt_pack_drop_plan> Dropped;
		/// The highest version of this pack here, if any prompt is linked to it.
		std::optional<std::uint32_t> Imported;
		/// The file's version is lower than `Imported`.
		bool Older = false;

		auto operator<=>(const prompt_pack_import_plan&) const = default;
	};
	/// \brief Each prompt's standing and starting choice against the stored
	/// differences and the shipped built-ins.
	[[nodiscard]] auto plan_prompt_pack_import(const decoded_prompt_pack& aFile, const prompt_macro_differences& aStored, std::span<const prompt_builtin> aShipped)
		-> prompt_pack_import_plan;

	/// \brief The reviewer's choices: the file's prompts to take (Add,
	/// Update or Replace, by index) and the dropped prompts to remove (by
	/// ID). A dropped prompt not removed is kept as the reviewer's own.
	struct prompt_pack_choices
	{
		std::vector<std::size_t> Take;
		std::vector<std::string> Remove;

		auto operator<=>(const prompt_pack_choices&) const = default;
	};
	/// \brief The starting choices of a plan.
	[[nodiscard]] auto default_prompt_pack_choices(const prompt_pack_import_plan& aPlan) -> prompt_pack_choices;

	enum class prompt_pack_apply_refusal : std::uint8_t {
		/// A choice names a refused or unchanged prompt, or one out of range.
		unusable_item,
		/// A prompt chosen twice.
		duplicate_choice,
		/// Remove names no prompt the pack dropped.
		unknown_drop,
		/// The profile would hold more than `maximum_prompt_macros`.
		library_full,
	};
	[[nodiscard]] auto stable_name(prompt_pack_apply_refusal aRefusal) noexcept -> std::string_view;

	/// \brief The stored differences after the choices, all or none.
	///
	/// \details Taken: an added prompt is on and placed as its fields say; an
	/// updated or replaced one takes the file's place and body and keeps its
	/// On, placements and order. Each is linked to the file's pack and
	/// version. An unchanged prompt linked to this pack records the file's
	/// version. A removed prompt goes with its placements; a kept one loses
	/// its link. Nothing else changes.
	[[nodiscard]] auto apply_prompt_pack_import(const prompt_macro_differences& aStored, std::span<const prompt_builtin> aShipped, const decoded_prompt_pack& aFile,
												const prompt_pack_choices& aChoices) -> std::expected<prompt_macro_differences, prompt_pack_apply_refusal>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_PACK_HPP
