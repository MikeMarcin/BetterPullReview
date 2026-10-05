#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_COMMAND_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_COMMAND_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

/// \file
/// \brief The finite `bpr cycle` command registry and the single-owner rules
/// both interfaces follow (RFC-BPR-0058, BPR-1167, BPR-1168).
/// \details One table names every command, the options it takes and what it
/// may change. The terminal parses against it; a profile owner admits a
/// forwarded command only if it is in it. Nothing here runs a command.
namespace bpr::core {
	enum class cycle_command : std::uint8_t {
		list,
		open,
		show,
		capture,
		note,
		review,
		freeze,
		export_round,
		compare,
		response_import,
		/// Records what standard output did with an exported round; the
		/// terminal issues it after writing, never a person.
		export_record,
		/// [[BPR-1169]]. Lets a harness read one frozen round, and by default
		/// append a response to it; nothing else.
		round_grant,
		round_revoke,
		/// Read the round a grant names: its assignment and a page of items.
		round_read,
		/// The next page of items, by the cursor the previous one gave.
		round_page,
		/// The round's source attachments: listed, or one's bytes.
		round_source,
		/// A granted harness's structured response to the round.
		response_append,
		/// The profile's disclosure policy: shown, or one channel set.
		policy,
		/// [[BPR-1279]]. Binds the cycle to its folder's new location, or to a
		/// replacement, once the reviewer confirms the material check shown.
		rebind,
	};
	/// The last command in the registry, for codecs that check a range.
	inline constexpr auto last_cycle_command = cycle_command::rebind;

	/// \brief On whose authority a command acts ([[BPR-1169]]).
	enum class cycle_command_authority : std::uint8_t {
		/// The reviewer's local authoring authority.
		reviewer,
		/// Only what a stored grant for one frozen round allows: never the
		/// reviewer's, whoever runs it.
		granted_consumer,
	};
	[[nodiscard]] auto authority_of(cycle_command aCommand) noexcept -> cycle_command_authority;

	/// \brief The command as typed after `bpr cycle` ("export", "response import").
	[[nodiscard]] auto name(cycle_command aCommand) noexcept -> std::string_view;
	/// \brief The command a person may type by that name; never `export_record`.
	[[nodiscard]] auto parse_cycle_command(std::string_view aName) noexcept -> std::optional<cycle_command>;
	/// \brief Every command a person may type, in the order help lists them.
	[[nodiscard]] auto typed_cycle_commands() noexcept -> std::span<const cycle_command>;
	/// \brief The options this command takes beyond the common ones.
	[[nodiscard]] auto cycle_command_options(cycle_command aCommand) noexcept -> std::span<const std::string_view>;
	/// \brief Whether `aOption` names a flag, which takes no value.
	[[nodiscard]] auto cycle_option_is_flag(std::string_view aOption) noexcept -> bool;
	/// \brief Options every command takes: output form, profile and data root.
	[[nodiscard]] auto common_cycle_options() noexcept -> std::span<const std::string_view>;
	/// \brief Whether the command may change the cycle store or a chosen file.
	[[nodiscard]] auto may_write(cycle_command aCommand) noexcept -> bool;
	/// \brief Whether the command reads or changes the next round's draft,
	/// which a person may be editing in an open window.
	[[nodiscard]] auto touches_round_draft(cycle_command aCommand) noexcept -> bool;

	/// \brief Longest operation identity a caller may name.
	inline constexpr std::size_t maximum_cycle_operation_id_bytes = 64U;
	/// \brief An operation identity: 1 to 64 ASCII letters, digits or hyphens.
	[[nodiscard]] auto valid_cycle_operation_id(std::string_view aOperation) noexcept -> bool;

	/// \brief What an open window holds of the next round's draft that the
	/// store does not yet have.
	enum class cycle_draft_editor : std::uint8_t {
		/// Nothing: what the window shows is what is stored.
		settled,
		/// Committed text the window has not saved yet.
		unsaved,
		/// Input-method text still being composed: not text yet.
		composing,
	};
	enum class cycle_owner_admission : std::uint8_t {
		/// Serve the command now.
		serve,
		/// Save the window's text first, then serve against what was saved.
		save_first,
		/// Serve nothing: retry once the composition ends.
		busy,
	};
	/// \brief Whether the profile owner may serve a forwarded command while a
	/// window holds `aEditor` of the same cycle's draft.
	/// \details Committed text is saved before a command that reads or
	/// changes the draft, so the command can neither omit it nor overwrite it
	/// unseen; composing text is never saved or read past.
	[[nodiscard]] auto admit_owner_command(cycle_command aCommand, cycle_draft_editor aEditor) noexcept -> cycle_owner_admission;
}

#endif
