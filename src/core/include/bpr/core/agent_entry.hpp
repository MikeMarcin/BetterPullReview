#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ENTRY_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ENTRY_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_setup.hpp"
#include "bpr/core/agent_wrapper.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-PENDING-AQ6]], owner amendment 2026-10-23 ("I should be
	/// able to define more than one custom agent"). The bounds of a profile's
	/// agent list.
	inline constexpr std::size_t   maximum_agent_entries              = 16U;
	inline constexpr std::uint32_t maximum_agent_entry_id             = 64U;
	inline constexpr std::size_t   maximum_agent_entry_name_bytes     = 64U;
	inline constexpr std::size_t   maximum_agent_entry_arguments      = 16U;
	inline constexpr std::size_t   maximum_agent_entry_argument_bytes = 256U;

	/// \brief One agent the reviewer added: a name they chose, the protocol it
	/// speaks, the program that runs it and the fixed arguments it is always
	/// given, and the model and effort it runs with by default.
	///
	/// \details Two entries may run the same program: a wrapper given nothing
	/// for Claude models and `--codex` for Codex ones is "Work Claude" and
	/// "Work Codex". The arguments are fixed per entry and come before the
	/// protocol's own; nothing from a pull request, a question or a
	/// preference is put into them, and no shell runs them. An empty program
	/// is a preset's own (`claude` or `codex`), looked for at each launch as
	/// before. No field could hold a credential.
	struct agent_entry
	{
		/// Stable while the entry exists, from 1 to `maximum_agent_entry_id`.
		std::uint32_t            Id = 0U;
		std::string              Name;
		agent_protocol           Protocol = agent_protocol::claude_stream_json;
		std::string              Program;
		std::vector<std::string> Arguments;
		/// Empty: the agent's own default.
		std::string Model;
		std::string Effort;
		/// The popular agent it was added as (Claude Code or Codex); none for
		/// a custom agent.
		std::optional<setup_agent> Preset;

		auto operator<=>(const agent_entry&) const = default;
	};

	enum class agent_entry_refusal : std::uint8_t {
		invalid_name,
		duplicate_name,
		/// A program that is not an absolute path the wrapper rules accept.
		invalid_program,
		/// A custom agent, or the plain-text harness, needs a program.
		no_program,
		invalid_argument,
		too_many_arguments,
		invalid_model,
		/// An effort the protocol's agent does not take.
		invalid_effort,
		too_many_entries,
		unknown_entry,
	};
	[[nodiscard]] auto stable_name(agent_entry_refusal aRefusal) noexcept -> std::string_view;
	/// \brief One line for the reviewer.
	[[nodiscard]] auto describe(agent_entry_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Which program's model and effort names a protocol takes:
	/// Claude's, Codex's, or none (the plain-text harness takes neither).
	[[nodiscard]] auto entry_settings_program(agent_protocol aProtocol) noexcept -> std::optional<agent_program>;

	/// \brief Why one entry may not be kept, or nothing.
	[[nodiscard]] auto inspect_agent_entry(const agent_entry& aEntry) -> std::optional<agent_entry_refusal>;
	/// \brief Why a list may not be kept: an entry it refuses, a name twice,
	/// an identity twice, or more entries than its bound.
	[[nodiscard]] auto inspect_agent_entries(std::span<const agent_entry> aEntries) -> std::optional<agent_entry_refusal>;

	/// \brief The profile an entry launches as: one fixed slot per protocol
	/// and identity, so every host is told every slot once, at composition.
	[[nodiscard]] auto entry_profile_id(agent_protocol aProtocol, std::uint32_t aId) noexcept -> agent_profile_id;
	[[nodiscard]] auto entry_profile_id(const agent_entry& aEntry) noexcept -> agent_profile_id;
	/// \brief Whether a profile is an entry's slot rather than a built-in one.
	[[nodiscard]] auto is_agent_entry_profile(agent_profile_id aProfile) noexcept -> bool;
	/// \brief The entry a profile is the slot of, in `aEntries`, or none.
	[[nodiscard]] auto entry_of_profile(std::span<const agent_entry> aEntries, agent_profile_id aProfile) noexcept -> const agent_entry*;
	/// \brief Every entry slot: for each protocol, one profile per identity,
	/// with the built-in profile's environment and program name.
	[[nodiscard]] auto agent_entry_profiles() -> std::vector<agent_profile>;
	/// \brief An entry's slot, named as the reviewer named the entry.
	[[nodiscard]] auto entry_profile(const agent_entry& aEntry) -> agent_profile;

	/// \brief Adds `aEntry` at the end under the smallest free identity.
	[[nodiscard]] auto add_agent_entry(std::vector<agent_entry> aEntries, agent_entry aEntry) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>;
	/// \brief Replaces the entry with `aEntry`'s identity, in its place.
	[[nodiscard]] auto update_agent_entry(std::vector<agent_entry> aEntries, agent_entry aEntry) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>;
	[[nodiscard]] auto remove_agent_entry(std::vector<agent_entry> aEntries, std::uint32_t aId) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>;
	/// \brief Moves one entry up (`aDelta` -1) or down (1), staying in the list.
	[[nodiscard]] auto move_agent_entry(std::vector<agent_entry> aEntries, std::uint32_t aId, int aDelta) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>;

	/// \brief A popular agent as an entry: Claude Code, or Codex speaking the
	/// chosen protocol, under its plain name, running `aProgram` (empty: its
	/// own program, looked for).
	[[nodiscard]] auto preset_agent_entry(setup_agent aPreset, codex_protocol_choice aCodex, std::string aProgram) -> agent_entry;
	/// \brief The entry in `aEntries` that is the preset, if any.
	[[nodiscard]] auto find_preset_entry(std::span<const agent_entry> aEntries, setup_agent aPreset) noexcept -> const agent_entry*;

	/// \brief What a profile kept before agents were a list.
	struct legacy_agent_settings
	{
		std::optional<setup_agent> Chosen;
		codex_protocol_choice      CodexProtocol = codex_protocol_choice::app_server;
		std::string                ClaudeWrapper;
		std::string                ClaudeModel;
		std::string                ClaudeEffort;
		std::string                CodexProgram;
		std::string                CodexModel;
		std::string                CodexEffort;
		std::string                HarnessProgram;
	};

	/// \brief The list those settings mean: Claude Code (with its wrapper as
	/// its program) when it was chosen or had a wrapper, Codex when it was
	/// chosen or had a program, and the plain-text harness when it had a
	/// program, the chosen one first. A value that would not be kept is left
	/// at the agent's default rather than refusing the move.
	[[nodiscard]] auto migrate_agent_entries(const legacy_agent_settings& aSettings) -> std::vector<agent_entry>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_ENTRY_HPP
