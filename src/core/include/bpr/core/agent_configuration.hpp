#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP

#include "bpr/core/agent_entry.hpp"
#include "bpr/core/workflow_configuration.hpp"

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
/// \brief [[BPR-2016]], RFC-BPR-0107; [[BPR-2198]]. One Profile Settings
/// file: its format, the sections it may hold, what an import of it would
/// change, and the profile's settings after the reviewer's choices.
///
/// \details Pure: the host reads and writes the file, mints identities and
/// stores the result. The file is one UTF-8 JSON document read and written
/// with glaze after `json_preflight`, at most
/// `maximum_agent_configuration_bytes`, in five sections: Agents, Prompts,
/// Workflows, Identity and Repositories. It holds no credential of any kind
/// (no token, keychain item, `gh` sign-in or cookie), no program path, no
/// local folder, numeric agent ID, host-minted identity, workspace or
/// history. There is one format and one version: the file was never
/// released before it, so nothing older is read.
namespace bpr::core {
	/// [[BPR-2198]]. The one format marker and its one version; a file of
	/// any other version is refused as newer.
	inline constexpr std::string_view agent_configuration_format        = "bpr.profile-settings";
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
	/// [[BPR-2198]]. Accounts and repositories one file may list.
	inline constexpr std::size_t maximum_configuration_accounts     = 64U;
	inline constexpr std::size_t maximum_configuration_repositories = 1024U;
	inline constexpr std::size_t maximum_host_bytes                 = 253U;
	inline constexpr std::size_t maximum_login_bytes                = 39U;
	inline constexpr std::size_t maximum_repository_name_bytes      = 201U;

	/// \brief [[BPR-2198]] (owner, 2026-11-15). The parts of a profile a
	/// Profile Settings file can carry, in the order Export and Import list them.
	enum class profile_settings_section : std::uint8_t {
		agents,
		prompts,
		workflows,
		/// The profile's name and the accounts it uses: host and login, never
		/// a sign-in.
		identity,
		/// Saved and followed repositories by host and owner/name; never a
		/// local folder.
		repositories,
	};
	inline constexpr std::size_t profile_settings_section_count = 5U;
	[[nodiscard]] auto           stable_name(profile_settings_section aSection) noexcept -> std::string_view;
	/// \brief Whether Export starts with a section ticked: every section but
	/// Identity, which a reviewer shares only on purpose.
	[[nodiscard]] auto exported_by_default(profile_settings_section aSection) noexcept -> bool;

	/// \brief One account a profile uses: the host and the login there.
	struct portable_account
	{
		/// `github.com` or an enterprise server's hostname.
		std::string Host;
		std::string Login;

		auto operator<=>(const portable_account&) const = default;
	};
	/// \brief The Identity section.
	struct portable_identity
	{
		std::string                   ProfileName;
		std::vector<portable_account> Accounts;

		auto operator<=>(const portable_identity&) const = default;
	};
	/// \brief One repository of the Repositories section.
	///
	/// \details A saved repository is saved under an account on its host
	/// (Home finds pull requests there); a followed one is an observer rule
	/// that adds its pull requests to the queue on any host, so it names no
	/// host. Per-repository settings beyond these two have no home in a
	/// profile yet, and no local path is ever carried.
	struct portable_repository
	{
		/// Empty for a repository only followed.
		std::string Host;
		/// "owner/name".
		std::string Repository;
		bool        Saved    = false;
		bool        Followed = false;

		auto operator<=>(const portable_repository&) const = default;
	};
	/// \brief The profile as Identity and Repositories see it here: its
	/// name, the accounts signed in, the servers it remembers and its
	/// repositories. The host reads these; nothing here is a credential.
	struct profile_facts
	{
		std::string                      ProfileName;
		std::vector<portable_account>    SignedIn;
		std::vector<portable_account>    Remembered;
		std::vector<portable_repository> Repositories;

		auto operator<=>(const profile_facts&) const = default;
	};
	/// \brief The Identity and Repositories an export of `aFacts` carries:
	/// every account signed in or remembered once each, and every saved or
	/// followed repository once each, in a stable order.
	[[nodiscard]] auto portable_identity_of(const profile_facts& aFacts) -> portable_identity;
	[[nodiscard]] auto portable_repositories_of(const profile_facts& aFacts) -> std::vector<portable_repository>;

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
		/// [[BPR-2139]], [[BPR-2140]] (owner, 2026-11-12). A file written
		/// before them reads as Off and the system prompt.
		agent_mcp_servers          McpServers   = agent_mcp_servers::off;
		agent_instruction_delivery Instructions = agent_instruction_delivery::system_prompt;

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
		/// [[BPR-2032]]. Each agent action names an agent in `Agents`.
		std::vector<workflow_configuration> Workflows;
		/// [[BPR-2198]]. Absent unless Identity was chosen.
		std::optional<portable_identity> Identity;
		std::vector<portable_repository> Repositories;

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
		/// [[BPR-2032]]. A workflow with a problem is listed and cannot be applied.
		std::vector<decoded_workflow> Workflows;
		/// [[BPR-2198]].
		std::optional<portable_identity> Identity;
		std::vector<portable_repository> Repositories;

		auto operator<=>(const decoded_configuration&) const = default;
	};

	/// \brief Reads a file against the prompts this version recognizes. A
	/// whole-file refusal changes nothing; an item problem is listed with the item.
	[[nodiscard]] auto decode_agent_configuration(std::string_view aBytes, std::span<const local_prompt> aRecognized)
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
		mcp_servers,
		instructions,
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
		/// [[BPR-2032]]. A workflow choice the profile's workflows refuse.
		std::optional<workflow_apply_error> Workflow;

		auto operator<=>(const configuration_apply_error&) const = default;
	};

	/// \brief Applies the chosen items to the profile's agents and prompts,
	/// all or none. Items not chosen, and every prompt not in `aPrompts`, are
	/// left as they are; the order of existing entries is kept.
	[[nodiscard]] auto apply_agent_configuration(std::span<const agent_entry> aAgents, std::span<const local_prompt> aLocal, const decoded_configuration& aFile,
												 std::span<const agent_import_choice> aAgentChoices, std::span<const std::size_t> aPrompts)
		-> std::expected<agent_configuration_result, configuration_apply_error>;

	/// \brief [[BPR-2032]] (design workflow-import-v1, B, the owner's choice
	/// of 2026-11-15). What one item of the file is on this computer.
	enum class import_standing : std::uint8_t {
		/// Nothing here has it: an agent of no name here, a workflow of no ID here.
		added,
		/// Something here has it and the file's differs.
		updated,
		/// Something here already is what the file says; there is nothing to choose.
		unchanged,
		/// This version cannot import it; the rest of the file still can.
		refused,
	};
	[[nodiscard]] auto stable_name(import_standing aStanding) noexcept -> std::string_view;

	/// \brief One agent of the file: what it is here and its starting choice.
	struct agent_import_plan
	{
		import_standing Standing = import_standing::refused;
		/// The starting choice: everything the file can bring starts chosen,
		/// added when new and replacing yours when updated. Relaxed run checks
		/// and builds start unaccepted (RFC-BPR-0107, RFC-BPR-0112 AC09).
		agent_import_choice Default;
		/// The entry of the same name here, which an update replaces when it
		/// is of the same kind (`compatible_agent`); one of another kind
		/// starts left out.
		std::optional<std::uint32_t> Meets;
		/// The entries that may stand for it as they are (Use …), in profile order.
		std::vector<std::uint32_t> StandIns;
		/// Added, it needs a program chosen on this computer before it applies:
		/// no program path travels in the file.
		bool NeedsProgram = false;

		auto operator<=>(const agent_import_plan&) const = default;
	};
	/// \brief One prompt of the file. Updated starts chosen; unchanged and
	/// refused have no choice.
	struct prompt_import_plan
	{
		import_standing Standing = import_standing::refused;
		bool            Default  = false;

		auto operator<=>(const prompt_import_plan&) const = default;
	};
	/// \brief One workflow of the file. An update is compared before Import:
	/// `Differences` are what it changes, in `workflow_differences` order.
	struct workflow_import_plan
	{
		import_standing                  Standing = import_standing::refused;
		workflow_import_action           Default  = workflow_import_action::leave;
		std::optional<std::size_t>       Local;
		std::vector<workflow_difference> Differences;

		auto operator<=>(const workflow_import_plan&) const = default;
	};
	/// \brief [[BPR-2198]]. Whether an account named by the file can be
	/// used here now.
	enum class account_standing : std::uint8_t {
		signed_in,
		/// No sign-in here for that login on that host: "Sign in to use".
		sign_in_to_use,
	};
	/// \brief The file's Identity. It starts unchosen: importing it renames
	/// this profile and remembers its enterprise servers.
	struct identity_import_plan
	{
		import_standing Standing = import_standing::refused;
		bool            Default  = false;
		/// Whether the file's name differs from this profile's.
		bool Renames = false;
		/// One per account of the file, in its order.
		std::vector<account_standing> Accounts;

		auto operator<=>(const identity_import_plan&) const = default;
	};
	/// \brief One repository of the file. New ones start chosen; a saved
	/// one whose host has no account signed in here says "Sign in to use" and
	/// cannot be saved until one is.
	struct repository_import_plan
	{
		import_standing Standing    = import_standing::refused;
		bool            Default     = false;
		bool            SignInToUse = false;

		auto operator<=>(const repository_import_plan&) const = default;
	};
	struct configuration_import_plan
	{
		std::vector<agent_import_plan>      Agents;
		std::vector<prompt_import_plan>     Prompts;
		std::vector<workflow_import_plan>   Workflows;
		std::optional<identity_import_plan> Identity;
		std::vector<repository_import_plan> Repositories;

		auto operator<=>(const configuration_import_plan&) const = default;
	};
	/// \brief Each item's standing and starting choice, against the profile's
	/// agents, recognized prompts and workflows. An agent is updated or
	/// unchanged only against an entry of its own name; unchanged when
	/// replacing that entry, its relaxed checks and builds as the entry has
	/// them, would change no field.
	/// [[BPR-2198]]: Identity and each repository against `aHere`.
	[[nodiscard]] auto plan_configuration_import(const decoded_configuration& aFile, std::span<const agent_entry> aAgents, std::span<const local_prompt> aPrompts,
												 std::span<const local_workflow> aWorkflows, const profile_facts& aHere = { }) -> configuration_import_plan;

	/// \brief [[BPR-2198]]. What importing Identity and the chosen
	/// repositories does to the profile, for the host to apply through the
	/// profile's own paths. Nothing here signs in.
	struct saved_repository_change
	{
		portable_account Account;
		std::string      Repository;

		auto operator<=>(const saved_repository_change&) const = default;
	};
	struct profile_settings_changes
	{
		/// The profile's new name; none when Identity is not chosen or the
		/// name is the same.
		std::optional<std::string> Rename;
		/// Enterprise servers to remember, with the login to sign in as.
		std::vector<portable_account> RememberServers;
		/// Repositories to save, each under the account signed in on its host.
		std::vector<saved_repository_change> Save;
		/// Repositories to follow ("owner/name").
		std::vector<std::string> Follow;

		auto operator<=>(const profile_settings_changes&) const = default;
	};
	/// \brief The changes `aIdentity` (chosen or not) and the chosen
	/// repositories (indexes into the file's) make against `aHere`. A
	/// repository with no choice in the plan is skipped.
	[[nodiscard]] auto profile_changes(const decoded_configuration& aFile, const configuration_import_plan& aPlan, bool aIdentity, std::span<const std::size_t> aRepositories,
									   const profile_facts& aHere) -> profile_settings_changes;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_CONFIGURATION_HPP
