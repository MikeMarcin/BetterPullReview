#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_CONFIGURATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_CONFIGURATION_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2032]], RFC-BPR-0112. A named optional author workflow: the
/// guidance it gives writing and the small set of actions it binds, as a
/// shared file carries it and as this profile keeps it.
///
/// \details Pure. A workflow travels inside the agent configuration file
/// (RFC-BPR-0107) as its `workflows` item list; a file that carries one is
/// written at version 2, so an earlier importer refuses it as a newer file
/// rather than dropping its actions as inert. Programs are machine bindings:
/// the file names a program's file name only and the recipient binds the
/// program on their computer. Nothing here holds a credential, an
/// enterprise rule or a script; a workflow only names what the author may
/// later hand off. Importing or selecting one runs nothing.
namespace bpr::core {
	inline constexpr std::size_t maximum_file_workflows          = 16U;
	inline constexpr std::size_t maximum_workflow_actions        = 16U;
	inline constexpr std::size_t maximum_workflow_name_bytes     = 128U;
	inline constexpr std::size_t maximum_workflow_task_bytes     = 128U;
	inline constexpr std::size_t maximum_workflow_text_bytes     = 16U * 1024U;
	inline constexpr std::size_t maximum_workflow_arguments      = 32U;
	inline constexpr std::size_t maximum_workflow_argument_bytes = 1024U;
	/// The profile keeps at most this many workflows.
	inline constexpr std::size_t maximum_profile_workflows = 32U;

	/// \brief The initial action slots. A workflow need not fill every one,
	/// and the slots are not a sequence the author must follow.
	enum class workflow_slot : std::uint8_t {
		gather_context,
		propose_description,
		check_preparation,
		publish_draft,
		finish_publication,
	};
	inline constexpr std::size_t workflow_slot_count = 5U;
	[[nodiscard]] auto           stable_name(workflow_slot aSlot) noexcept -> std::string_view;
	[[nodiscard]] auto           parse_workflow_slot(std::string_view aName) noexcept -> std::optional<workflow_slot>;
	/// \brief Whether the slot's work changes something outside this
	/// computer's draft: a ticket, a branch, a pull request.
	[[nodiscard]] auto effectful(workflow_slot aSlot) noexcept -> bool;

	/// \brief Who does an action's work.
	enum class workflow_binding : std::uint8_t {
		/// One of the profile's agents, in BPR's own tool-free writing lane.
		agent,
		/// A companion program the author runs in their own terminal or
		/// harness. BPR writes its request and reads its answer; it never
		/// starts the program.
		command,
	};
	[[nodiscard]] auto stable_name(workflow_binding aBinding) noexcept -> std::string_view;

	/// \brief An effect class an action declares. A declaration is what the
	/// file says, not a restriction BPR enforces on a program.
	enum class workflow_effect : std::uint8_t {
		read_context,
		write_proposal,
		run_check,
		change_ticket,
		publish_pull_request,
	};
	inline constexpr std::size_t workflow_effect_count = 5U;
	[[nodiscard]] auto           stable_name(workflow_effect aEffect) noexcept -> std::string_view;
	[[nodiscard]] auto           describe(workflow_effect aEffect) noexcept -> std::string_view;

	/// \brief One action as the file carries it.
	struct workflow_action
	{
		/// Unique in the workflow: lowercase letters, digits and hyphens.
		std::string   Id;
		workflow_slot Slot = workflow_slot::gather_context;
		/// The author's task, in their words ("Check the template").
		std::string      Task;
		workflow_binding Binding = workflow_binding::command;
		/// For an agent action: the file's portable agent ID, and what it is asked.
		std::string Agent;
		std::string Prompt;
		/// For a command action: the program's file name (no folder) and its
		/// argument array. `{request}` and `{response}` stand alone as whole
		/// arguments and become the exchange files' paths; nothing else is
		/// substituted, and no pull request text reaches an argument.
		std::string                  ProgramName;
		std::vector<std::string>     Arguments;
		std::vector<workflow_effect> Effects;
		/// A check the workflow's own publisher requires before it acts.
		bool Required = false;

		auto operator<=>(const workflow_action&) const = default;
	};

	struct workflow_configuration
	{
		std::string PortableId;
		std::string Name;
		/// Who the workflow says it comes from ("Synthetic Org"); shown, never trusted.
		std::string Origin;
		/// Writing guidance the workflow's agent actions are given.
		std::string                  Guidance;
		std::vector<workflow_action> Actions;

		auto operator<=>(const workflow_configuration&) const = default;
	};

	/// \brief Why one action, and so its workflow, cannot be applied.
	enum class workflow_problem : std::uint8_t {
		/// A slot, binding or effect this version does not know.
		unsupported,
		/// An ID, name, task, program name or argument out of its bounds.
		invalid,
		/// The same action ID twice, or the same workflow ID twice in the file.
		duplicate,
		/// An agent action declares an effect BPR's tool-free lane cannot
		/// perform, or fills an effectful slot.
		agent_effect,
		/// An agent action names no agent the file carries.
		unknown_agent,
	};
	[[nodiscard]] auto stable_name(workflow_problem aProblem) noexcept -> std::string_view;
	[[nodiscard]] auto describe(workflow_problem aProblem) noexcept -> std::string_view;

	struct decoded_workflow
	{
		workflow_configuration          Workflow;
		std::optional<workflow_problem> Problem;
		/// The action the problem is in, by its index; none for the workflow itself.
		std::optional<std::size_t> Action;
		/// The unknown text as the file has it, for an unsupported item.
		std::string Unsupported;

		auto operator<=>(const decoded_workflow&) const = default;
	};

	/// \brief The `workflows` array's exact JSON, or none when an item is out of bounds.
	[[nodiscard]] auto encode_workflows(std::span<const workflow_configuration> aWorkflows) -> std::optional<std::string>;
	/// \brief Reads the `workflows` array. A shape it cannot read is none;
	/// an unknown or invalid value is the item's problem. `aAgents` are the
	/// portable agent IDs the same file carries.
	[[nodiscard]] auto decode_workflows(std::string_view aJson, std::span<const std::string> aAgents) -> std::optional<std::vector<decoded_workflow>>;

	/// \brief The workflow's revision: SHA-256 of its canonical encoding, in
	/// lowercase hex. A draft pins the revision it selected.
	[[nodiscard]] auto workflow_revision(const workflow_configuration& aWorkflow) -> std::string;

	/// \brief A program bound on this computer for one command action.
	struct workflow_program
	{
		std::string Action;
		/// An absolute path, chosen by the author.
		std::string Program;

		auto operator<=>(const workflow_program&) const = default;
	};
	/// \brief The profile agent an agent action runs as, by entry name.
	struct workflow_agent_binding
	{
		std::string Action;
		std::string AgentName;

		auto operator<=>(const workflow_agent_binding&) const = default;
	};

	/// \brief A workflow as this profile keeps it: the shared definition and
	/// its local bindings.
	struct local_workflow
	{
		workflow_configuration              Workflow;
		std::vector<workflow_program>       Programs;
		std::vector<workflow_agent_binding> Agents;

		auto operator<=>(const local_workflow&) const = default;
	};
	[[nodiscard]] auto valid(const local_workflow& aWorkflow) -> bool;
	[[nodiscard]] auto encode_local_workflow(const local_workflow& aWorkflow) -> std::string;
	[[nodiscard]] auto decode_local_workflow(std::string_view aText) -> std::optional<local_workflow>;
	[[nodiscard]] auto bound_program(const local_workflow& aWorkflow, std::string_view aAction) -> const workflow_program*;
	[[nodiscard]] auto bound_agent(const local_workflow& aWorkflow, std::string_view aAction) -> const workflow_agent_binding*;

	/// \brief What an update would change, for the import preview.
	enum class workflow_change : std::uint8_t {
		name,
		origin,
		guidance,
		action_added,
		action_removed,
		action_changed,
		/// An action declares an effect it did not before.
		effects_widened,
	};
	[[nodiscard]] auto stable_name(workflow_change aChange) noexcept -> std::string_view;
	struct workflow_difference
	{
		workflow_change Change = workflow_change::name;
		/// The action, for the action changes.
		std::string Action;

		auto operator<=>(const workflow_difference&) const = default;
	};
	[[nodiscard]] auto workflow_differences(const workflow_configuration& aBefore, const workflow_configuration& aAfter) -> std::vector<workflow_difference>;

	enum class workflow_import_action : std::uint8_t {
		leave,
		/// A new workflow; the ID must not be in the profile.
		add,
		/// The profile's workflow of the same ID takes the file's definition.
		replace,
	};
	/// \brief The author's choice for one imported workflow.
	struct workflow_import_choice
	{
		std::size_t            Item   = 0U;
		workflow_import_action Action = workflow_import_action::leave;
		/// A program for each command action the author binds now. An
		/// unbound action stays unavailable until bound; replace keeps the
		/// earlier binding of an action whose program name is unchanged.
		std::vector<workflow_program> Programs;
		/// The profile agent each agent action runs as.
		std::vector<workflow_agent_binding> Agents;

		auto operator<=>(const workflow_import_choice&) const = default;
	};

	enum class workflow_apply_refusal : std::uint8_t {
		unusable_item,
		duplicate_choice,
		/// Add names an ID the profile has; replace names one it does not.
		conflict,
		/// A binding names an action the workflow does not have, or a
		/// program that is not an absolute path.
		invalid_binding,
		too_many,
	};
	[[nodiscard]] auto stable_name(workflow_apply_refusal aRefusal) noexcept -> std::string_view;
	struct workflow_apply_error
	{
		workflow_apply_refusal Refusal = workflow_apply_refusal::unusable_item;
		std::size_t            Item    = 0U;

		auto operator<=>(const workflow_apply_error&) const = default;
	};
	/// \brief The profile's workflows after the choices, all or none. Items
	/// not chosen are left as they are, and so is every other workflow.
	[[nodiscard]] auto apply_workflow_choices(std::span<const local_workflow> aProfile, std::span<const decoded_workflow> aFile, std::span<const workflow_import_choice> aChoices)
		-> std::expected<std::vector<local_workflow>, workflow_apply_error>;

	/// \brief What one action still needs on this computer before the author
	/// can use it. Nothing here runs or probes a program; `aProgramExists`
	/// and `aAgentExists` are the host's observations.
	enum class workflow_prerequisite : std::uint8_t {
		ready,
		/// A command action with no program bound here.
		program_unbound,
		/// The bound program is not an executable file now.
		program_missing,
		/// An agent action with no agent, or one no longer in the profile.
		agent_missing,
	};
	[[nodiscard]] auto stable_name(workflow_prerequisite aPrerequisite) noexcept -> std::string_view;
	[[nodiscard]] auto describe(workflow_prerequisite aPrerequisite) noexcept -> std::string_view;
	[[nodiscard]] auto action_prerequisite(const local_workflow& aWorkflow, const workflow_action& aAction, bool aProgramExists, bool aAgentExists) -> workflow_prerequisite;

	/// \brief An absolute program path's argument array for a command action:
	/// the program, then each argument with a whole `{request}` or
	/// `{response}` replaced by the path. Nothing else is substituted.
	[[nodiscard]] auto workflow_command(std::string_view aProgram, std::span<const std::string> aArguments, std::string_view aRequest, std::string_view aResponse) -> std::vector<std::string>;
	/// \brief The same array as one line the author can paste into a POSIX
	/// shell: every element single-quoted.
	[[nodiscard]] auto shell_line(std::span<const std::string> aCommand) -> std::string;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_WORKFLOW_CONFIGURATION_HPP
