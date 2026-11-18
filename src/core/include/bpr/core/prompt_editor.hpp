#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_EDITOR_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_PROMPT_EDITOR_HPP

#include "bpr/core/prompt_macro.hpp"
#include "bpr/core/prompt_surfaces.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2179]], the architecture review of its close ([[BPR-2185]]).
/// Settings › Prompts as a reducer: what the page edits (the app-wide library
/// and surfaces, and each repository's layer), every edit as a transition
/// with an undo history, and the save protocol (the revision a save is
/// derived from, and what a stored, stale or failed write does).
///
/// \details The page holds a `prompt_editor` and calls these functions; it
/// decides nothing about the library itself. A new prompt's identity is
/// supplied by the caller (the host mints it). Every transition is pure.
namespace bpr::core {
	/// \brief What the page edits: the app-wide library and surfaces, and
	/// every repository's layer over them.
	struct prompt_edit_document
	{
		std::vector<prompt_macro>            Library;
		prompt_surfaces                      Surfaces;
		std::vector<prompt_repository_layer> Layers;

		auto operator<=>(const prompt_edit_document&) const = default;
	};

	/// \brief A merged set's library, surfaces and layers.
	[[nodiscard]] auto prompt_edit_document_of(const prompt_macro_set& aSet) -> prompt_edit_document;

	/// \brief Review N4. Every difference that makes `aDocument` from
	/// `aShipped`: each built-in whose body or On differs, every own prompt,
	/// each place's order where it is not the shipped order, each surface
	/// that differs from what the fields describe, and every repository's
	/// layer. `merge_prompt_macros` reads it back as `aDocument`.
	[[nodiscard]] auto prompt_macro_differences_of(std::span<const prompt_builtin> aShipped, const prompt_edit_document& aDocument) -> prompt_macro_differences;

	/// \brief How a save ended, as the store reports it.
	enum class prompt_save_outcome : std::uint8_t {
		stored,
		/// Written from an older revision: nothing changed.
		stale,
		/// The store refused the write: nothing changed.
		failed,
	};

	/// \brief What the page says about saving, besides Saving… and Saved.
	enum class prompt_edit_notice : std::uint8_t {
		none,
		/// The store cannot be read: the built-ins are shown and nothing saves.
		unreadable,
		/// The last write failed; the edits are kept and save with the next.
		save_failed,
		/// Another window wrote first; the page shows theirs.
		save_stale,
	};

	/// \brief A write to make: the differences, and the revision they were
	/// derived from, which the store compares before it writes.
	struct prompt_save_request
	{
		prompt_macro_differences Differences;
		std::uint64_t            From = 0U;

		auto operator<=>(const prompt_save_request&) const = default;
	};

	/// \brief One undo step: the document before an edit, and the prompt the
	/// edit was about (selected again when it is undone).
	struct prompt_edit_step
	{
		prompt_edit_document       Document;
		std::optional<std::string> Prompt;

		auto operator<=>(const prompt_edit_step&) const = default;
	};

	/// \brief The most undo steps kept; the oldest go first.
	inline constexpr std::size_t maximum_prompt_undo_steps = 200U;

	/// \brief The page's editing state.
	///
	/// \details `Base` is the stored revision `Document` was derived from; a
	/// save names it, so a write over another window's is refused as stale
	/// rather than accepted. While `Pending` or `Writing`, a projection does
	/// not replace the edits. The undo history belongs to `Repository`'s
	/// layer and to `Base`'s lineage: choosing another layer, or adopting a
	/// document someone else wrote, starts a new one.
	struct prompt_editor
	{
		std::vector<prompt_builtin> Shipped;
		prompt_edit_document        Document;
		/// The layer edited: empty for every repository.
		std::string   Repository;
		std::uint64_t Base     = 0U;
		bool          Loaded   = false;
		bool          Readable = false;
		/// Edits made since the last write was sent.
		bool Pending = false;
		/// The revision the write in flight was made from.
		std::optional<std::uint64_t> Writing;
		/// A write was stale: the next projection is adopted over any edits.
		bool Superseded = false;
		/// A write failed: the edits wait for the next edit rather than
		/// being sent again at once.
		bool                          Held = false;
		std::vector<prompt_edit_step> Undo;
		std::vector<prompt_edit_step> Redo;
		/// The prompt a run of typing is open on: its keystrokes are one step.
		std::optional<std::string> Typing;
		prompt_edit_notice         Notice = prompt_edit_notice::none;

		auto operator<=>(const prompt_editor&) const = default;
	};

	/// \brief An editor over `aShipped` with nothing stored, not yet loaded.
	[[nodiscard]] auto make_prompt_editor(std::vector<prompt_builtin> aShipped) -> prompt_editor;

	/// \brief The chosen layer as the page shows it: the app-wide library and
	/// surfaces, or a repository's layer applied over them.
	[[nodiscard]] auto prompt_edit_view(const prompt_editor& aEditor) -> prompt_layered;

	/// \brief A projection arrived. Adopted unless edits are waiting or a
	/// write is in flight (after a stale write it is adopted whatever is
	/// waiting). Adopting another document clears the undo history.
	/// \returns Whether the document shown changed.
	auto adopt_prompt_projection(prompt_editor& aEditor, const prompt_macro_set& aSet, std::uint64_t aRevision, bool aReadable, bool aLoaded) -> bool;
	/// \brief The write to send now, if any: edits are waiting, none is in
	/// flight, and the store can be read. Marks it in flight.
	[[nodiscard]] auto next_prompt_save(prompt_editor& aEditor) -> std::optional<prompt_save_request>;
	/// \brief The write in flight ended. Stored: the store is at its `From`
	/// plus one. Failed: the edits are kept and wait for the next write.
	/// Stale: another window wrote first; the next projection is adopted.
	void settle_prompt_save(prompt_editor& aEditor, prompt_save_outcome aOutcome);
	/// \brief Saving… while edits wait or a write is in flight.
	[[nodiscard]] auto prompt_edits_saving(const prompt_editor& aEditor) noexcept -> bool;

	/// \brief Why an edit was refused.
	enum class prompt_edit_refusal : std::uint8_t {
		none,
		/// No prompt has that identity here.
		unknown_prompt,
		/// The library already holds `maximum_prompt_macros` (a repository,
		/// as many of its own).
		library_full,
		/// The identity is already used, or empty.
		identity_taken,
		/// A built-in prompt cannot be deleted.
		built_in,
		/// In a repository, an app-wide prompt cannot be deleted (turn it off).
		inherited,
		/// The surface does not take it; `prompt_edit_outcome::Placement` says why.
		placement,
		/// Only a repository's layer has an app-wide value to go back to.
		not_in_repository,
		/// The conditions are full where it was asked, or the path names
		/// no group.
		conditions_full,
		/// The prompt's chats do not hold that fact (`prompt_fact_allowed`).
		fact_not_allowed,
		/// Nothing to undo or redo.
		no_step,
	};

	/// \brief What an edit did.
	struct prompt_edit_outcome
	{
		/// The document changed (and an undo step was kept).
		bool                Changed   = false;
		prompt_edit_refusal Refusal   = prompt_edit_refusal::none;
		placement_refusal   Placement = placement_refusal::none;
		/// The prompt the edit was about, for the page to select.
		std::optional<std::string> Prompt;

		auto operator<=>(const prompt_edit_outcome&) const = default;
	};

	/// \brief Places `aId` on `aSurface` at `aAt`; a placement there moves.
	auto place_prompt(prompt_editor& aEditor, const std::string& aId, prompt_surface aSurface, std::optional<std::size_t> aAt = std::nullopt) -> prompt_edit_outcome;
	/// \brief A drop: from the library (`aFrom` none) or another surface,
	/// which it leaves unless `aCopy`. `aAt` counts the row being moved.
	auto drop_prompt(prompt_editor& aEditor, const std::string& aId, std::optional<prompt_surface> aFrom, prompt_surface aTo, std::size_t aAt, bool aCopy) -> prompt_edit_outcome;
	/// \brief Takes a placement off; the prompt stays in the library.
	auto unplace_prompt(prompt_editor& aEditor, const std::string& aId, prompt_surface aSurface) -> prompt_edit_outcome;
	/// \brief Moves a placement `aBy` places within its surface.
	auto move_prompt(prompt_editor& aEditor, const std::string& aId, prompt_surface aSurface, int aBy) -> prompt_edit_outcome;
	auto set_prompt_on(prompt_editor& aEditor, const std::string& aId, bool aOn) -> prompt_edit_outcome;
	/// \brief New prompt: one of the reviewer's own named `aName`, on no
	/// surface, under the identity `aId` the host minted.
	auto add_prompt(prompt_editor& aEditor, std::string aId, std::string aName) -> prompt_edit_outcome;
	/// \brief Duplicate: an own copy of `aFrom` named `aName` (cut at the name
	/// bound, on a character boundary), on no surface, under the new
	/// identity `aId` and linked to no prompt pack.
	auto duplicate_prompt(prompt_editor& aEditor, const std::string& aFrom, std::string aId, std::string aName) -> prompt_edit_outcome;
	/// \brief Reset to default: a built-in's shipped body; On and placements stay.
	auto reset_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome;
	/// \brief Deletes one of the reviewer's own prompts and its placements.
	auto delete_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome;
	/// \brief Restore built-ins: every built-in's shipped body and On, and the
	/// shipped surfaces, the reviewer's own placements kept after them.
	auto restore_built_in_prompts(prompt_editor& aEditor) -> prompt_edit_outcome;
	/// \brief An edit of a prompt's body and On (identity, place and kind
	/// stay). `aTyping`: one keystroke of a run that is one undo step.
	auto edit_prompt(prompt_editor& aEditor, const prompt_macro& aEdited, bool aTyping = false) -> prompt_edit_outcome;
	/// \brief Ends a run of typing: the next keystroke is a new step.
	void end_prompt_typing(prompt_editor& aEditor) noexcept;

	/// \brief A group in a prompt's condition tree: indices into each level's
	/// groups, the list itself empty.
	using prompt_condition_path = std::vector<std::size_t>;
	/// \brief What the Add menu of a group offers.
	struct prompt_condition_room
	{
		/// The group exists.
		bool Exists = false;
		/// A condition can be added there.
		bool Condition = false;
		/// A group can be added there (two levels below the list at most).
		bool Group = false;
		/// The whole tree holds `maximum_prompt_condition_leaves`.
		bool Full = false;
		/// The group holds `maximum_prompt_conditions` conditions.
		bool Crowded = false;
		/// The facts a condition here may read, in `prompt_fact` order.
		std::vector<prompt_fact> Facts;

		auto operator<=>(const prompt_condition_room&) const = default;
	};
	[[nodiscard]] auto prompt_condition_room_of(const prompt_macro& aMacro, const prompt_condition_path& aPath) -> prompt_condition_room;
	auto               add_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, prompt_fact aFact) -> prompt_edit_outcome;
	auto               add_prompt_condition_group(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath) -> prompt_edit_outcome;
	auto               remove_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, std::size_t aAt) -> prompt_edit_outcome;
	auto               remove_prompt_condition_group(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, std::size_t aAt) -> prompt_edit_outcome;
	auto               set_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, std::size_t aAt, prompt_condition aCondition) -> prompt_edit_outcome;
	/// \brief A group's All, Any or None; the list's with an empty path.
	auto set_prompt_group_match(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, prompt_match aMatch) -> prompt_edit_outcome;

	/// \brief A field a repository may set on its own, with its way back.
	enum class prompt_field : std::uint8_t {
		name,
		text,
		answer,
		/// The conditions as one: the list, its match, the nested groups and
		/// what the row does while they are unmet.
		conditions,
		on,
	};
	/// \brief `name`, `text`, `answer`, `conditions`, `on`.
	[[nodiscard]] auto stable_name(prompt_field aField) noexcept -> std::string_view;
	[[nodiscard]] auto find_prompt_field(std::string_view aName) noexcept -> std::optional<prompt_field>;
	/// \brief The fields of `aHere` that differ from `aApp`, in field order.
	[[nodiscard]] auto prompt_fields_differing(const prompt_macro& aApp, const prompt_macro& aHere) -> std::vector<prompt_field>;

	/// \brief In a repository: every field back to the app-wide value.
	auto use_app_wide_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome;
	/// \brief In a repository: one field back to the app-wide value.
	auto use_app_wide_field(prompt_editor& aEditor, const std::string& aId, prompt_field aField) -> prompt_edit_outcome;
	/// \brief In a repository: a placement added or removed here back as it
	/// is app-wide.
	auto revert_prompt_placement(prompt_editor& aEditor, const std::string& aId, prompt_surface aSurface) -> prompt_edit_outcome;
	/// \brief In a repository: the surface's app-wide order again.
	auto revert_prompt_order(prompt_editor& aEditor, prompt_surface aSurface) -> prompt_edit_outcome;

	/// \brief Prompts for: a repository's layer, or every repository (empty).
	/// The undo history is the layer's own, so it starts again.
	/// \returns Whether the layer changed.
	auto choose_prompt_repository(prompt_editor& aEditor, std::string aRepository) -> bool;

	/// \brief Undo the last edit, whatever it was; Redo it again.
	auto undo_prompt_edit(prompt_editor& aEditor) -> prompt_edit_outcome;
	auto redo_prompt_edit(prompt_editor& aEditor) -> prompt_edit_outcome;

	/// \brief How a prompt stands in the layer shown.
	struct prompt_standing
	{
		/// A built-in whose body differs from the shipped one.
		bool Edited = false;
		/// In a repository: one of its own.
		bool OnlyHere = false;
		/// In a repository: off here, on app-wide.
		bool OffHere = false;
		/// In a repository: its body differs from app-wide.
		bool ChangedHere = false;
		/// In a repository: every field that differs from app-wide.
		std::vector<prompt_field> Differing;

		/// \brief Anything here differs from app-wide.
		[[nodiscard]] auto differs_here() const noexcept -> bool { return OnlyHere || !Differing.empty(); }

		auto operator<=>(const prompt_standing&) const = default;
	};
	[[nodiscard]] auto prompt_standing_of(const prompt_editor& aEditor, const prompt_macro& aMacro) -> prompt_standing;

	/// \brief How a surface stands in a repository: the placements it added
	/// and removed, and whether it keeps its own order.
	struct prompt_surface_standing
	{
		std::vector<std::string> Added;
		std::vector<std::string> Removed;
		bool                     Ordered = false;

		auto operator<=>(const prompt_surface_standing&) const = default;
	};
	[[nodiscard]] auto prompt_surface_standing_of(const prompt_editor& aEditor, prompt_surface aSurface) -> prompt_surface_standing;

	/// \brief One surface where a prompt is placed and is not offered: the
	/// first blocking problem of the prompt as placed there.
	struct prompt_placement_problem
	{
		prompt_surface Surface = prompt_surface::pull_request_menu;
		prompt_problem Problem = prompt_problem::no_name;

		auto operator<=>(const prompt_placement_problem&) const = default;
	};
	[[nodiscard]] auto prompt_placement_problems(const prompt_macro& aMacro, const prompt_surfaces& aSurfaces) -> std::vector<prompt_placement_problem>;
} // namespace bpr::core

#endif
