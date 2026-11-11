#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP

#include "bpr/core/agent_entry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2016]], RFC-BPR-0107. One portable file of selected agents
/// and prompts: its format, what it may hold, what an import of it would
/// change, and the profile's settings after the reviewer's choices.
///
/// \details Pure: the host reads and writes the file, mints identities and
/// stores the result. The file is one UTF-8 JSON document read and written
/// with glaze after `json_preflight`, at most
/// `maximum_agent_configuration_bytes`. It holds no program path, numeric
/// agent ID, host-minted identity, account, workspace, history or other
/// profile setting.
namespace bpr::core {
	inline constexpr std::string_view agent_configuration_format        = "bpr.agent-configuration";
	inline constexpr std::uint32_t    agent_configuration_version       = 1U;
	inline constexpr std::size_t      maximum_agent_configuration_bytes = 1024U * 1024U;
	/// The free-text label an exporter gives the file, and its own name.
	inline constexpr std::size_t maximum_agent_configuration_label_bytes = 128U;
	inline constexpr std::size_t maximum_portable_id_bytes               = 64U;
	/// Items one file may list. The profile's own bounds (16 agents) apply
	/// to what an import keeps, not to the file.
	inline constexpr std::size_t maximum_configuration_agents  = 64U;
	inline constexpr std::size_t maximum_configuration_prompts = 64U;
	inline constexpr std::size_t maximum_purpose_bytes         = 64U;
	inline constexpr std::size_t maximum_prompt_version_bytes  = 64U;

	/// \brief One agent as the file carries it: everything an entry keeps but
	/// where its program is, its numeric ID and its identity.
	struct portable_agent
	{
		/// Unique in the file; derived from the name, never from the profile.
		std::string                PortableId;
		std::string                Name;
		agent_protocol             Protocol = agent_protocol::claude_stream_json;
		std::optional<setup_agent> Preset;
		/// A custom agent's program file name (no folder), shown when the
		/// recipient chooses theirs; empty for a popular agent's own program.
		std::string              ProgramName;
		std::vector<std::string> Arguments;
		std::vector<std::string> CheckArguments;
		std::string              Model;
		std::string              Effort;
		bool                     StrictRunChecks = true;
		/// RFC-BPR-0104 (X17): requested values, applied as RFC-BPR-0107 says.
		bool ScratchStarts  = true;
		bool BuildsAndTests = false;

		auto operator<=>(const portable_agent&) const = default;
	};

	/// \brief Whether a prompt item is the exporter's edited text or the
	/// shipped default they chose to include (owner, 2026-11-11).
	enum class portable_prompt_origin : std::uint8_t {
		edited,
		built_in,
	};
	[[nodiscard]] auto stable_name(portable_prompt_origin aOrigin) noexcept -> std::string_view;

	/// \brief One prompt as the file carries it.
	struct portable_prompt
	{
		std::string            Purpose;
		portable_prompt_origin Origin = portable_prompt_origin::edited;
		/// The built-in version the text was written against (a shipped
		/// default is its own version); none when the exporter did not record it.
		std::optional<std::string>                  WrittenAgainst;
		std::optional<std::array<std::uint8_t, 32>> WrittenAgainstDigest;
		std::string                                 Text;

		auto operator<=>(const portable_prompt&) const = default;
	};

	struct agent_configuration
	{
		std::string                  Label;
		std::string                  ExportedBy;
		std::vector<portable_agent>  Agents;
		std::vector<portable_prompt> Prompts;

		auto operator<=>(const agent_configuration&) const = default;
	};

	/// \brief What kind of text a prompt purpose holds, for its validator.
	enum class prompt_kind : std::uint8_t {
		/// A writing hook: UTF-8, at most `maximum_writing_instruction_bytes`.
		writing_hook,
		/// The review skill: `inspect_review_skill` admits it (64 KiB).
		review_skill,
	};

	/// \brief One prompt this version recognizes, as the profile has it now.
	struct local_prompt
	{
		std::string Purpose;
		prompt_kind Kind = prompt_kind::writing_hook;
		/// The shipped default, its version and digest.
		std::string                  BuiltIn;
		std::string                  BuiltInVersion;
		std::array<std::uint8_t, 32> BuiltInDigest{ };
		/// The profile's edited text; empty: the shipped default.
		std::string Override;
		/// The built-in version the override was written against, if known.
		std::optional<std::string> WrittenAgainst;

		auto operator<=>(const local_prompt&) const = default;
	};

	/// \brief An earlier purpose ID that names a current one.
	struct prompt_alias
	{
		std::string_view Retired;
		std::string_view Current;
	};

	/// \brief A portable ID for each name, unique within `aNames`: the name in
	/// lowercase letters and digits with other runs as one hyphen, and `-2`,
	/// `-3`… on a repeat.
	[[nodiscard]] auto portable_ids(std::span<const std::string> aNames) -> std::vector<std::string>;

	/// \brief An entry as the file carries it, under `aPortableId`.
	[[nodiscard]] auto portable_agent_of(const agent_entry& aEntry, std::string aPortableId) -> portable_agent;
	/// \brief A prompt as the file carries it: its edited text, or with
	/// `aBuiltIn` its shipped default whatever the profile has.
	[[nodiscard]] auto portable_prompt_of(const local_prompt& aPrompt, bool aBuiltIn) -> portable_prompt;

	/// \brief The file's exact bytes, or none when they would exceed
	/// `maximum_agent_configuration_bytes` or an item would not read back.
	[[nodiscard]] auto encode_agent_configuration(const agent_configuration& aConfiguration) -> std::optional<std::string>;

	/// \brief Why a whole file is refused before anything is shown.
	enum class configuration_file_refusal : std::uint8_t {
		too_large,
		not_text,
		malformed,
		not_a_configuration,
		unsupported_version,
		too_many_items,
	};
	[[nodiscard]] auto stable_name(configuration_file_refusal aRefusal) noexcept -> std::string_view;
	[[nodiscard]] auto describe(configuration_file_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Why one item cannot be applied. The rest of the file still can.
	enum class configuration_item_problem : std::uint8_t {
		/// A protocol or popular agent this version does not know.
		unsupported_protocol,
		/// A purpose this version does not recognize.
		unsupported_purpose,
		/// The same portable ID, or the same purpose, twice.
		duplicate,
		/// A value the profile's own rules refuse (`Refusal` says which).
		invalid_agent,
		/// Prompt text over its bound, not UTF-8, empty, or a review skill
		/// that `inspect_review_skill` refuses.
		invalid_prompt,
	};
	[[nodiscard]] auto stable_name(configuration_item_problem aProblem) noexcept -> std::string_view;
	[[nodiscard]] auto describe(configuration_item_problem aProblem) noexcept -> std::string_view;

	struct decoded_agent
	{
		portable_agent                            Agent;
		std::optional<configuration_item_problem> Problem;
		std::optional<agent_entry_refusal>        Refusal;
		/// The protocol or preset text as the file has it, for an unsupported one.
		std::string Unsupported;

		auto operator<=>(const decoded_agent&) const = default;
	};
	struct decoded_prompt
	{
		portable_prompt                           Prompt;
		std::optional<configuration_item_problem> Problem;
		/// Index into the `local_prompt` span the file was read against.
		std::optional<std::size_t> Local;

		auto operator<=>(const decoded_prompt&) const = default;
	};
	struct decoded_configuration
	{
		std::string                 Label;
		std::string                 ExportedBy;
		std::vector<decoded_agent>  Agents;
		std::vector<decoded_prompt> Prompts;

		auto operator<=>(const decoded_configuration&) const = default;
	};

	/// \brief Reads a file against the prompts this version recognizes. A
	/// whole-file refusal changes nothing; an item problem is listed with the item.
	[[nodiscard]] auto decode_agent_configuration(std::string_view aBytes, std::span<const local_prompt> aRecognized, std::span<const prompt_alias> aAliases = { })
		-> std::expected<decoded_configuration, configuration_file_refusal>;

	/// \brief An agent field an import can change.
	enum class agent_field : std::uint8_t {
		name,
		protocol,
		program,
		arguments,
		check_arguments,
		model,
		effort,
		strict_run_checks,
		scratch_starts,
		builds_and_tests,
	};
	[[nodiscard]] auto stable_name(agent_field aField) noexcept -> std::string_view;

	/// \brief The existing entry an imported agent meets: one of the same name,
	/// or the same popular agent; none when it meets none.
	[[nodiscard]] auto colliding_agent(std::span<const agent_entry> aAgents, const portable_agent& aAgent) noexcept -> const agent_entry*;
	/// \brief Whether an entry may stand for an imported agent (Use existing):
	/// the same protocol.
	[[nodiscard]] auto compatible_agent(const agent_entry& aLocal, const portable_agent& aAgent) noexcept -> bool;
	/// \brief Whether an edit from `aBefore` to `aAfter` keeps the entry's
	/// identity, and so its conversations: the rule Settings' Edit follows.
	[[nodiscard]] auto keeps_agent_identity(const agent_entry& aBefore, const agent_entry& aAfter) noexcept -> bool;
	/// \brief The fields that differ between two entries, in `agent_field` order.
	[[nodiscard]] auto agent_differences(const agent_entry& aBefore, const agent_entry& aAfter) -> std::vector<agent_field>;

	enum class agent_import_action : std::uint8_t {
		/// Not applied.
		leave,
		/// A new entry at the end of the list.
		add,
		/// An existing compatible entry stands for it; nothing changes.
		use_existing,
		/// The chosen entry takes the imported settings.
		replace,
	};
	/// \brief The reviewer's choice for one imported agent.
	struct agent_import_choice
	{
		std::size_t         Item   = 0U;
		agent_import_action Action = agent_import_action::leave;
		/// The entry `use_existing` or `replace` names.
		std::uint32_t Target = 0U;
		/// The program on this computer: required for a custom agent added,
		/// empty for a popular agent's own; for replace, empty keeps the
		/// target's program.
		std::string Program;
		/// For add: the name it is added under; empty keeps the file's.
		std::string Name;
		/// Accepted for this entry in the preview (RFC-BPR-0107, P05, X17).
		bool AcceptRelaxedChecks = false;
		bool AcceptBuilds        = false;

		auto operator<=>(const agent_import_choice&) const = default;
	};

	/// \brief The entry an imported agent would be under a choice, before
	/// identity: the host mints one where `Identity` is empty.
	///
	/// \details Relaxed run checks and builds stay off unless accepted;
	/// a popular agent stays strict. Scratch folder starts follows a
	/// requested Off, and otherwise the target's value (replace) or on (add).
	[[nodiscard]] auto imported_entry(const portable_agent& aAgent, const agent_import_choice& aChoice, const agent_entry* aTarget) -> agent_entry;

	/// \brief The profile's agents and prompts after the choices, and what
	/// they change.
	struct agent_configuration_result
	{
		std::vector<agent_entry> Agents;
		/// Replaced entries whose identity is new: their conversations stay
		/// readable and can no longer be continued.
		std::vector<std::uint32_t> Detached;
		/// Every replaced entry, whose kept check no longer describes it.
		std::vector<std::uint32_t> Replaced;
		/// For each `local_prompt`, its override after the import (empty: the
		/// shipped default) and the version it was written against.
		std::vector<local_prompt> Prompts;
		std::size_t               AgentsApplied  = 0U;
		std::size_t               PromptsApplied = 0U;

		auto operator<=>(const agent_configuration_result&) const = default;
	};

	enum class configuration_apply_refusal : std::uint8_t {
		/// A choice names an item with a problem, or one out of range.
		unusable_item,
		/// An item chosen twice.
		duplicate_choice,
		/// A custom agent added with no program, or a program the rules refuse.
		unbound_program,
		/// Use existing or replace names no entry, or one of another protocol.
		unknown_target,
		/// The list the choices make is refused (too many, a name twice…);
		/// `Entry` says why.
		refused_list,
	};
	[[nodiscard]] auto stable_name(configuration_apply_refusal aRefusal) noexcept -> std::string_view;
	struct configuration_apply_error
	{
		configuration_apply_refusal        Refusal = configuration_apply_refusal::unusable_item;
		std::size_t                        Item    = 0U;
		std::optional<agent_entry_refusal> Entry;

		auto operator<=>(const configuration_apply_error&) const = default;
	};

	/// \brief Applies the chosen items to the profile's agents and prompts,
	/// all or none. Items not chosen, and every prompt not in `aPrompts`, are
	/// left as they are; the order of existing entries is kept.
	[[nodiscard]] auto apply_agent_configuration(std::span<const agent_entry> aAgents, std::span<const local_prompt> aLocal, const decoded_configuration& aFile,
												 std::span<const agent_import_choice> aAgentChoices, std::span<const std::size_t> aPrompts)
		-> std::expected<agent_configuration_result, configuration_apply_error>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP
