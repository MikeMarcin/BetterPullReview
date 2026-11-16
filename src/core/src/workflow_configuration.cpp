#include "bpr/core/workflow_configuration.hpp"

#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <iterator>
#include <ranges>
#include <set>
#include <utility>

namespace bpr::core::detail {
	struct wire_workflow_action
	{
		std::string                Id;
		std::string                Slot;
		std::string                Task;
		std::string                Binding;
		std::optional<std::string> Agent;
		std::optional<std::string> Prompt;
		std::optional<std::string> ProgramName;
		std::vector<std::string>   Arguments;
		std::vector<std::string>   Effects;
		bool                       Required = false;
	};
	struct wire_workflow
	{
		std::string                       Id;
		std::string                       Name;
		std::string                       Origin;
		std::string                       Guidance;
		std::vector<wire_workflow_action> Actions;
	};
	struct wire_local_workflow
	{
		wire_workflow                             Workflow;
		std::vector<core::workflow_program>       Programs;
		std::vector<core::workflow_agent_binding> Agents;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_workflow_action>
{
	using value_type            = bpr::core::detail::wire_workflow_action;
	static constexpr auto value = object("id", &value_type::Id, "slot", &value_type::Slot, "task", &value_type::Task, "binding", &value_type::Binding, "agent", &value_type::Agent, "prompt",
										 &value_type::Prompt, "program_name", &value_type::ProgramName, "arguments", &value_type::Arguments, "effects", &value_type::Effects, "required",
										 &value_type::Required);
};
template <>
struct glz::meta<bpr::core::detail::wire_workflow>
{
	using value_type            = bpr::core::detail::wire_workflow;
	static constexpr auto value = object("id", &value_type::Id, "name", &value_type::Name, "origin", &value_type::Origin, "guidance", &value_type::Guidance, "actions", &value_type::Actions);
};
template <>
struct glz::meta<bpr::core::workflow_program>
{
	using value_type            = bpr::core::workflow_program;
	static constexpr auto value = object("action", &value_type::Action, "program", &value_type::Program);
};
template <>
struct glz::meta<bpr::core::workflow_agent_binding>
{
	using value_type            = bpr::core::workflow_agent_binding;
	static constexpr auto value = object("action", &value_type::Action, "agent", &value_type::AgentName);
};
template <>
struct glz::meta<bpr::core::detail::wire_local_workflow>
{
	using value_type            = bpr::core::detail::wire_local_workflow;
	static constexpr auto value = object("workflow", &value_type::Workflow, "programs", &value_type::Programs, "agents", &value_type::Agents);
};

namespace bpr::core {
	namespace {
		constexpr auto strict_options = glz::opts{ .error_on_unknown_keys = true };

		[[nodiscard]] auto plain(const std::string_view aText) noexcept -> bool
		{
			return utf8::valid(aText) && std::ranges::none_of(aText, [](const char aByte) { return static_cast<unsigned char>(aByte) < 0x20U || aByte == 0x7F; });
		}

		[[nodiscard]] auto bounded_plain(const std::string_view aText, const std::size_t aLimit) noexcept -> bool { return !aText.empty() && aText.size() <= aLimit && plain(aText); }

		[[nodiscard]] auto valid_text(const std::string_view aText) noexcept -> bool { return aText.size() <= maximum_workflow_text_bytes && utf8::valid(aText); }

		[[nodiscard]] auto valid_id(const std::string_view aId) noexcept -> bool
		{
			return !aId.empty() && aId.size() <= 64U && std::ranges::all_of(aId, [](const char aByte) { return (aByte >= 'a' && aByte <= 'z') || (aByte >= '0' && aByte <= '9') || aByte == '-'; });
		}

		[[nodiscard]] auto valid_program_name(const std::string_view aName) noexcept -> bool
		{
			return bounded_plain(aName, 255U) && aName.find_first_of("/\\") == std::string_view::npos;
		}

		[[nodiscard]] auto absolute(const std::string_view aPath) noexcept -> bool
		{
			if (!utf8::valid(aPath) || aPath.size() > 4096U || aPath.find('\0') != std::string_view::npos) {
				return false;
			}
			const auto windows = aPath.size() > 2U && aPath[1] == ':' && (aPath[2] == '\\' || aPath[2] == '/');
			return (!aPath.empty() && aPath.front() == '/') || windows;
		}

		template <typename TEnum, std::size_t TCount>
		[[nodiscard]] auto parse_enum(const std::string_view aName) noexcept -> std::optional<TEnum>
		{
			for (std::size_t index = 0U; index < TCount; ++index) {
				if (stable_name(static_cast<TEnum>(index)) == aName) {
					return static_cast<TEnum>(index);
				}
			}
			return std::nullopt;
		}

		/// The effects an agent action may declare: BPR's tool-free lane
		/// reads what it is sent and answers with proposals or findings.
		[[nodiscard]] auto agent_may(const workflow_effect aEffect) noexcept -> bool
		{
			return aEffect == workflow_effect::read_context || aEffect == workflow_effect::write_proposal;
		}

		[[nodiscard]] auto wire_of(const workflow_configuration& aWorkflow) -> detail::wire_workflow
		{
			auto wire = detail::wire_workflow{ .Id = aWorkflow.PortableId, .Name = aWorkflow.Name, .Origin = aWorkflow.Origin, .Guidance = aWorkflow.Guidance, .Actions = { } };
			for (const auto& action : aWorkflow.Actions) {
				auto item    = detail::wire_workflow_action{ };
				item.Id      = action.Id;
				item.Slot    = std::string{ stable_name(action.Slot) };
				item.Task    = action.Task;
				item.Binding = std::string{ stable_name(action.Binding) };
				if (action.Binding == workflow_binding::agent) {
					item.Agent  = action.Agent;
					item.Prompt = action.Prompt;
				} else {
					item.ProgramName = action.ProgramName;
					item.Arguments   = action.Arguments;
				}
				std::ranges::transform(action.Effects, std::back_inserter(item.Effects), [](const workflow_effect aEffect) { return std::string{ stable_name(aEffect) }; });
				item.Required = action.Required;
				wire.Actions.push_back(std::move(item));
			}
			return wire;
		}

		/// One action's problem, or none; `aAction` is filled as far as the wire allows.
		[[nodiscard]] auto read_action(detail::wire_workflow_action& aWire, workflow_action& aAction, std::span<const std::string> aAgents, std::string& aUnsupported)
			-> std::optional<workflow_problem>
		{
			aAction.Id         = aWire.Id;
			aAction.Task       = aWire.Task;
			aAction.Required   = aWire.Required;
			const auto slot    = parse_workflow_slot(aWire.Slot);
			const auto binding = parse_enum<workflow_binding, 2U>(aWire.Binding);
			if (!slot || !binding) {
				aUnsupported = !slot ? aWire.Slot : aWire.Binding;
				return workflow_problem::unsupported;
			}
			aAction.Slot    = *slot;
			aAction.Binding = *binding;
			for (const auto& name : aWire.Effects) {
				const auto effect = parse_enum<workflow_effect, workflow_effect_count>(name);
				if (!effect) {
					aUnsupported = name;
					return workflow_problem::unsupported;
				}
				aAction.Effects.push_back(*effect);
			}
			if (!valid_id(aAction.Id) || !bounded_plain(aAction.Task, maximum_workflow_task_bytes) || aAction.Effects.empty() || aAction.Effects.size() > workflow_effect_count) {
				return workflow_problem::invalid;
			}
			if (aAction.Binding == workflow_binding::agent) {
				aAction.Agent  = aWire.Agent.value_or(std::string{ });
				aAction.Prompt = aWire.Prompt.value_or(std::string{ });
				if (aWire.ProgramName || !aWire.Arguments.empty() || aAction.Prompt.empty() || !valid_text(aAction.Prompt)) {
					return workflow_problem::invalid;
				}
				if (effectful(aAction.Slot) || !std::ranges::all_of(aAction.Effects, agent_may)) {
					return workflow_problem::agent_effect;
				}
				if (std::ranges::find(aAgents, aAction.Agent) == aAgents.end()) {
					return workflow_problem::unknown_agent;
				}
				return std::nullopt;
			}
			aAction.ProgramName   = aWire.ProgramName.value_or(std::string{ });
			aAction.Arguments     = std::move(aWire.Arguments);
			const auto argumentOk = [](const std::string& aArgument) { return aArgument.size() <= maximum_workflow_argument_bytes && plain(aArgument); };
			if (aWire.Agent || aWire.Prompt || !valid_program_name(aAction.ProgramName) || aAction.Arguments.size() > maximum_workflow_arguments || !std::ranges::all_of(aAction.Arguments, argumentOk)) {
				return workflow_problem::invalid;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto read_workflow(detail::wire_workflow& aWire, std::span<const std::string> aAgents) -> decoded_workflow
		{
			auto item                = decoded_workflow{ };
			item.Workflow.PortableId = aWire.Id;
			item.Workflow.Name       = aWire.Name;
			item.Workflow.Origin     = aWire.Origin;
			item.Workflow.Guidance   = aWire.Guidance;
			if (!valid_id(aWire.Id) || !bounded_plain(aWire.Name, maximum_workflow_name_bytes) || aWire.Origin.size() > maximum_workflow_name_bytes || !plain(aWire.Origin)
				|| !valid_text(aWire.Guidance) || aWire.Actions.empty() || aWire.Actions.size() > maximum_workflow_actions) {
				item.Problem = workflow_problem::invalid;
			}
			auto ids = std::set<std::string>{ };
			for (std::size_t index = 0U; index < aWire.Actions.size(); ++index) {
				auto action  = workflow_action{ };
				auto problem = read_action(aWire.Actions[index], action, aAgents, item.Unsupported);
				if (!problem && !ids.insert(action.Id).second) {
					problem = workflow_problem::duplicate;
				}
				if (problem && !item.Problem) {
					item.Problem = problem;
					item.Action  = index;
				}
				item.Workflow.Actions.push_back(std::move(action));
			}
			return item;
		}
	} // namespace

	auto stable_name(const workflow_slot aSlot) noexcept -> std::string_view
	{
		switch (aSlot) {
		case workflow_slot::gather_context:      return "gather_context";
		case workflow_slot::propose_description: return "propose_description";
		case workflow_slot::check_preparation:   return "check_preparation";
		case workflow_slot::publish_draft:       return "publish_draft";
		case workflow_slot::finish_publication:  return "finish_publication";
		}
		return "gather_context";
	}

	auto parse_workflow_slot(const std::string_view aName) noexcept -> std::optional<workflow_slot> { return parse_enum<workflow_slot, workflow_slot_count>(aName); }

	auto effectful(const workflow_slot aSlot) noexcept -> bool { return aSlot == workflow_slot::publish_draft || aSlot == workflow_slot::finish_publication; }

	auto stable_name(const workflow_binding aBinding) noexcept -> std::string_view { return aBinding == workflow_binding::agent ? "agent" : "command"; }

	auto stable_name(const workflow_effect aEffect) noexcept -> std::string_view
	{
		switch (aEffect) {
		case workflow_effect::read_context:         return "read_context";
		case workflow_effect::write_proposal:       return "write_proposal";
		case workflow_effect::run_check:            return "run_check";
		case workflow_effect::change_ticket:        return "change_ticket";
		case workflow_effect::publish_pull_request: return "publish_pull_request";
		}
		return "read_context";
	}

	auto describe(const workflow_effect aEffect) noexcept -> std::string_view
	{
		switch (aEffect) {
		case workflow_effect::read_context:         return "reads the context it is sent";
		case workflow_effect::write_proposal:       return "proposes text you accept or reject";
		case workflow_effect::run_check:            return "runs checks in your workspace";
		case workflow_effect::change_ticket:        return "changes a ticket";
		case workflow_effect::publish_pull_request: return "publishes a pull request";
		}
		return "";
	}

	auto stable_name(const workflow_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case workflow_problem::unsupported:   return "unsupported";
		case workflow_problem::invalid:       return "invalid";
		case workflow_problem::duplicate:     return "duplicate";
		case workflow_problem::agent_effect:  return "agent_effect";
		case workflow_problem::unknown_agent: return "unknown_agent";
		}
		return "invalid";
	}

	auto describe(const workflow_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case workflow_problem::unsupported:   return "Uses an action this version does not support; the workflow cannot be imported.";
		case workflow_problem::invalid:       return "Has a value this version refuses.";
		case workflow_problem::duplicate:     return "Repeats an ID.";
		case workflow_problem::agent_effect:  return "Asks an agent for something it cannot do here: an agent only reads and proposes.";
		case workflow_problem::unknown_agent: return "Names an agent the file does not carry.";
		}
		return "";
	}

	auto encode_workflows(const std::span<const workflow_configuration> aWorkflows) -> std::optional<std::string>
	{
		auto wires = std::vector<detail::wire_workflow>{ };
		std::ranges::transform(aWorkflows, std::back_inserter(wires), wire_of);
		auto bytes = std::string{ };
		if (glz::write<glz::opts{ }>(wires, bytes)) {
			return std::nullopt;
		}
		// Every item must read back as written.
		auto agents = std::vector<std::string>{ };
		for (const auto& workflow : aWorkflows) {
			for (const auto& action : workflow.Actions) {
				agents.push_back(action.Agent);
			}
		}
		const auto back = decode_workflows(bytes, agents);
		if (!back || std::ranges::any_of(*back, [](const decoded_workflow& aItem) { return aItem.Problem.has_value(); })) {
			return std::nullopt;
		}
		return bytes;
	}

	auto decode_workflows(const std::string_view aJson, const std::span<const std::string> aAgents) -> std::optional<std::vector<decoded_workflow>>
	{
		auto wires = std::vector<detail::wire_workflow>{ };
		if (glz::read<strict_options>(wires, aJson) || wires.size() > maximum_file_workflows) {
			return std::nullopt;
		}
		auto decoded = std::vector<decoded_workflow>{ };
		auto ids     = std::set<std::string>{ };
		for (auto& wire : wires) {
			auto item = read_workflow(wire, aAgents);
			if (!ids.insert(item.Workflow.PortableId).second && !item.Problem) {
				item.Problem = workflow_problem::duplicate;
			}
			decoded.push_back(std::move(item));
		}
		return decoded;
	}

	auto workflow_revision(const workflow_configuration& aWorkflow) -> std::string
	{
		auto bytes = std::string{ };
		static_cast<void>(glz::write<glz::opts{ }>(wire_of(aWorkflow), bytes));
		return lower_hex(std::span<const std::uint8_t>{ sha256(std::span{ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() }) });
	}

	auto valid(const local_workflow& aWorkflow) -> bool
	{
		auto agents = std::vector<std::string>{ };
		std::ranges::transform(aWorkflow.Workflow.Actions, std::back_inserter(agents), &workflow_action::Agent);
		auto wire  = std::vector{ wire_of(aWorkflow.Workflow) };
		auto bytes = std::string{ };
		if (glz::write<glz::opts{ }>(wire, bytes)) {
			return false;
		}
		const auto back = decode_workflows(bytes, agents);
		if (!back || back->size() != 1U || back->front().Problem) {
			return false;
		}
		const auto names = [&aWorkflow](const workflow_binding aBinding) {
			auto ids = std::set<std::string>{ };
			for (const auto& action : aWorkflow.Workflow.Actions) {
				if (action.Binding == aBinding) {
					ids.insert(action.Id);
				}
			}
			return ids;
		};
		const auto commands  = names(workflow_binding::command);
		const auto agentIds  = names(workflow_binding::agent);
		const auto programOk = [&commands](const workflow_program& aProgram) { return commands.contains(aProgram.Action) && absolute(aProgram.Program); };
		const auto agentOk   = [&agentIds](const workflow_agent_binding& aAgent) { return agentIds.contains(aAgent.Action) && bounded_plain(aAgent.AgentName, 256U); };
		return std::ranges::all_of(aWorkflow.Programs, programOk) && std::ranges::all_of(aWorkflow.Agents, agentOk);
	}

	auto encode_local_workflow(const local_workflow& aWorkflow) -> std::string
	{
		auto wire  = detail::wire_local_workflow{ .Workflow = wire_of(aWorkflow.Workflow), .Programs = aWorkflow.Programs, .Agents = aWorkflow.Agents };
		auto bytes = std::string{ };
		static_cast<void>(glz::write<glz::opts{ }>(wire, bytes));
		return bytes;
	}

	auto decode_local_workflow(const std::string_view aText) -> std::optional<local_workflow>
	{
		auto wire = detail::wire_local_workflow{ };
		if (glz::read<strict_options>(wire, aText)) {
			return std::nullopt;
		}
		auto agents = std::vector<std::string>{ };
		std::ranges::transform(wire.Workflow.Actions, std::back_inserter(agents), [](const detail::wire_workflow_action& aAction) { return aAction.Agent.value_or(std::string{ }); });
		auto item = read_workflow(wire.Workflow, agents);
		if (item.Problem) {
			return std::nullopt;
		}
		auto workflow = local_workflow{ .Workflow = std::move(item.Workflow), .Programs = std::move(wire.Programs), .Agents = std::move(wire.Agents) };
		if (!valid(workflow)) {
			return std::nullopt;
		}
		return workflow;
	}

	auto bound_program(const local_workflow& aWorkflow, const std::string_view aAction) -> const workflow_program*
	{
		const auto found = std::ranges::find(aWorkflow.Programs, aAction, &workflow_program::Action);
		return found == aWorkflow.Programs.end() ? nullptr : &*found;
	}

	auto bound_agent(const local_workflow& aWorkflow, const std::string_view aAction) -> const workflow_agent_binding*
	{
		const auto found = std::ranges::find(aWorkflow.Agents, aAction, &workflow_agent_binding::Action);
		return found == aWorkflow.Agents.end() ? nullptr : &*found;
	}

	auto stable_name(const workflow_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case workflow_change::name:            return "name";
		case workflow_change::origin:          return "origin";
		case workflow_change::guidance:        return "guidance";
		case workflow_change::action_added:    return "action_added";
		case workflow_change::action_removed:  return "action_removed";
		case workflow_change::action_changed:  return "action_changed";
		case workflow_change::effects_widened: return "effects_widened";
		}
		return "name";
	}

	auto workflow_differences(const workflow_configuration& aBefore, const workflow_configuration& aAfter) -> std::vector<workflow_difference>
	{
		auto       differences = std::vector<workflow_difference>{ };
		const auto note        = [&differences](const bool aDiffers, const workflow_change aChange, std::string aAction = { }) {
			if (aDiffers) {
				differences.push_back(workflow_difference{ .Change = aChange, .Action = std::move(aAction) });
			}
		};
		note(aBefore.Name != aAfter.Name, workflow_change::name);
		note(aBefore.Origin != aAfter.Origin, workflow_change::origin);
		note(aBefore.Guidance != aAfter.Guidance, workflow_change::guidance);
		for (const auto& action : aAfter.Actions) {
			const auto earlier = std::ranges::find(aBefore.Actions, action.Id, &workflow_action::Id);
			if (earlier == aBefore.Actions.end()) {
				note(true, workflow_change::action_added, action.Id);
				continue;
			}
			note(*earlier != action, workflow_change::action_changed, action.Id);
			const auto widened = std::ranges::any_of(action.Effects, [&earlier](const workflow_effect aEffect) { return std::ranges::find(earlier->Effects, aEffect) == earlier->Effects.end(); });
			note(widened, workflow_change::effects_widened, action.Id);
		}
		for (const auto& action : aBefore.Actions) {
			note(std::ranges::find(aAfter.Actions, action.Id, &workflow_action::Id) == aAfter.Actions.end(), workflow_change::action_removed, action.Id);
		}
		return differences;
	}

	auto stable_name(const workflow_apply_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case workflow_apply_refusal::unusable_item:    return "unusable_item";
		case workflow_apply_refusal::duplicate_choice: return "duplicate_choice";
		case workflow_apply_refusal::conflict:         return "conflict";
		case workflow_apply_refusal::invalid_binding:  return "invalid_binding";
		case workflow_apply_refusal::too_many:         return "too_many";
		}
		return "unusable_item";
	}

	auto apply_workflow_choices(const std::span<const local_workflow> aProfile, const std::span<const decoded_workflow> aFile, const std::span<const workflow_import_choice> aChoices)
		-> std::expected<std::vector<local_workflow>, workflow_apply_error>
	{
		auto result = std::vector<local_workflow>{ aProfile.begin(), aProfile.end() };
		auto chosen = std::set<std::size_t>{ };
		for (const auto& choice : aChoices) {
			const auto fail = [&choice](const workflow_apply_refusal aRefusal) { return std::unexpected(workflow_apply_error{ .Refusal = aRefusal, .Item = choice.Item }); };
			if (choice.Action == workflow_import_action::leave) {
				continue;
			}
			if (choice.Item >= aFile.size() || aFile[choice.Item].Problem) {
				return fail(workflow_apply_refusal::unusable_item);
			}
			if (!chosen.insert(choice.Item).second) {
				return fail(workflow_apply_refusal::duplicate_choice);
			}
			const auto& incoming = aFile[choice.Item].Workflow;
			const auto  existing = std::ranges::find(result, incoming.PortableId, [](const local_workflow& aLocal) -> const std::string& { return aLocal.Workflow.PortableId; });
			if ((choice.Action == workflow_import_action::add) == (existing != result.end())) {
				return fail(workflow_apply_refusal::conflict);
			}
			auto local = local_workflow{ .Workflow = incoming, .Programs = choice.Programs, .Agents = choice.Agents };
			if (existing != result.end()) {
				// An unchanged program name keeps the program this computer bound for it.
				for (const auto& action : incoming.Actions) {
					const auto before = std::ranges::find(existing->Workflow.Actions, action.Id, &workflow_action::Id);
					const auto kept   = bound_program(*existing, action.Id);
					if (action.Binding == workflow_binding::command && kept != nullptr && bound_program(local, action.Id) == nullptr && before != existing->Workflow.Actions.end()
						&& before->ProgramName == action.ProgramName) {
						local.Programs.push_back(*kept);
					}
				}
			}
			if (!valid(local)) {
				return fail(workflow_apply_refusal::invalid_binding);
			}
			if (existing != result.end()) {
				*existing = std::move(local);
			} else {
				result.push_back(std::move(local));
			}
		}
		if (result.size() > maximum_profile_workflows) {
			return std::unexpected(workflow_apply_error{ .Refusal = workflow_apply_refusal::too_many });
		}
		return result;
	}

	auto stable_name(const workflow_prerequisite aPrerequisite) noexcept -> std::string_view
	{
		switch (aPrerequisite) {
		case workflow_prerequisite::ready:           return "ready";
		case workflow_prerequisite::program_unbound: return "program_unbound";
		case workflow_prerequisite::program_missing: return "program_missing";
		case workflow_prerequisite::agent_missing:   return "agent_missing";
		}
		return "ready";
	}

	auto describe(const workflow_prerequisite aPrerequisite) noexcept -> std::string_view
	{
		switch (aPrerequisite) {
		case workflow_prerequisite::ready:           return "Ready";
		case workflow_prerequisite::program_unbound: return "Choose its program on this computer";
		case workflow_prerequisite::program_missing: return "Its program is not on this computer";
		case workflow_prerequisite::agent_missing:   return "Its agent is not in this profile";
		}
		return "";
	}

	auto action_prerequisite(const local_workflow& aWorkflow, const workflow_action& aAction, const bool aProgramExists, const bool aAgentExists) -> workflow_prerequisite
	{
		if (aAction.Binding == workflow_binding::agent) {
			return bound_agent(aWorkflow, aAction.Id) != nullptr && aAgentExists ? workflow_prerequisite::ready : workflow_prerequisite::agent_missing;
		}
		if (bound_program(aWorkflow, aAction.Id) == nullptr) {
			return workflow_prerequisite::program_unbound;
		}
		return aProgramExists ? workflow_prerequisite::ready : workflow_prerequisite::program_missing;
	}

	auto workflow_command(const std::string_view aProgram, const std::span<const std::string> aArguments, const std::string_view aRequest, const std::string_view aResponse)
		-> std::vector<std::string>
	{
		auto command = std::vector<std::string>{ std::string{ aProgram } };
		for (const auto& argument : aArguments) {
			command.emplace_back(argument == "{request}" ? aRequest : argument == "{response}" ? aResponse
																							   : std::string_view{ argument });
		}
		return command;
	}

	auto shell_line(const std::span<const std::string> aCommand) -> std::string
	{
		auto line = std::string{ };
		for (const auto& element : aCommand) {
			if (!line.empty()) {
				line += ' ';
			}
			line += '\'';
			for (const auto byte : element) {
				if (byte == '\'') {
					line += "'\\''";
				} else {
					line += byte;
				}
			}
			line += '\'';
		}
		return line;
	}
} // namespace bpr::core
