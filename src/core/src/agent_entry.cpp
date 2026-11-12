#include "bpr/core/agent_entry.hpp"

#include "bpr/core/agent_wrapper.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace bpr::core {
	namespace {
		/// Entry slots are numbered from here: a thousand above the built-in
		/// profiles, a hundred per protocol.
		constexpr std::uint64_t entry_profile_base  = 1000U;
		constexpr std::uint64_t entry_profile_block = 100U;

		[[nodiscard]] auto control(const char aByte) noexcept -> bool
		{
			const auto byte = static_cast<unsigned char>(aByte);
			return byte < 0x20U || byte == 0x7FU;
		}

		[[nodiscard]] auto plain_text(const std::string_view aText) noexcept -> bool { return std::ranges::none_of(aText, control); }

		/// An absolute path on either platform: `/…`, `X:\…` or `X:/…`, or `\\…`.
		[[nodiscard]] auto absolute(const std::string_view aPath) noexcept -> bool
		{
			if (aPath.starts_with('/') || aPath.starts_with("\\\\")) {
				return true;
			}
			return aPath.size() >= 3U && ((aPath[0] >= 'A' && aPath[0] <= 'Z') || (aPath[0] >= 'a' && aPath[0] <= 'z')) && aPath[1] == ':' && (aPath[2] == '\\' || aPath[2] == '/');
		}

		constexpr auto protocols = std::array{
			agent_protocol::codex_app_server,
			agent_protocol::codex_jsonl_exec,
			agent_protocol::claude_stream_json,
			agent_protocol::external_terminal,
		};

		[[nodiscard]] auto built_in_for(const agent_protocol aProtocol) -> agent_profile
		{
			auto profiles = built_in_agent_profiles();
			auto found    = std::ranges::find(profiles, aProtocol, &agent_profile::Protocol);
			return found == profiles.end() ? agent_profile{ } : *found;
		}

		[[nodiscard]] auto free_id(const std::span<const agent_entry> aEntries) noexcept -> std::uint32_t
		{
			for (auto id = std::uint32_t{ 1U }; id <= maximum_agent_entry_id; ++id) {
				if (std::ranges::find(aEntries, id, &agent_entry::Id) == aEntries.end()) {
					return id;
				}
			}
			return 0U;
		}
	} // namespace

	auto stable_name(const agent_entry_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case agent_entry_refusal::invalid_name:          return "invalid_name";
		case agent_entry_refusal::duplicate_name:        return "duplicate_name";
		case agent_entry_refusal::invalid_program:       return "invalid_program";
		case agent_entry_refusal::no_program:            return "no_program";
		case agent_entry_refusal::invalid_argument:      return "invalid_argument";
		case agent_entry_refusal::too_many_arguments:    return "too_many_arguments";
		case agent_entry_refusal::invalid_model:         return "invalid_model";
		case agent_entry_refusal::invalid_effort:        return "invalid_effort";
		case agent_entry_refusal::too_many_entries:      return "too_many_entries";
		case agent_entry_refusal::unknown_entry:         return "unknown_entry";
		case agent_entry_refusal::relaxed_popular_agent: return "relaxed_popular_agent";
		}
		return "invalid_name";
	}

	auto describe(const agent_entry_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case agent_entry_refusal::invalid_name:          return "Give the agent a name of up to 64 characters.";
		case agent_entry_refusal::duplicate_name:        return "Another agent already has that name.";
		case agent_entry_refusal::invalid_program:       return "The program must be a full path, such as /usr/local/bin/my-agent.";
		case agent_entry_refusal::no_program:            return "Choose the program that runs this agent.";
		case agent_entry_refusal::invalid_argument:      return "An argument is empty, too long, or holds a control character.";
		case agent_entry_refusal::too_many_arguments:    return "An agent takes at most 16 fixed arguments.";
		case agent_entry_refusal::invalid_model:         return "That is not a model name this agent's protocol takes.";
		case agent_entry_refusal::invalid_effort:        return "That is not an effort this agent's protocol takes.";
		case agent_entry_refusal::too_many_entries:      return "A profile keeps at most 16 agents.";
		case agent_entry_refusal::unknown_entry:         return "That agent is no longer in the list.";
		case agent_entry_refusal::relaxed_popular_agent: return "Only a custom agent can turn off strict run checks.";
		}
		return "That agent cannot be kept.";
	}

	auto entry_settings_program(const agent_protocol aProtocol) noexcept -> std::optional<agent_program>
	{
		switch (aProtocol) {
		case agent_protocol::claude_stream_json: return agent_program::claude;
		case agent_protocol::codex_app_server:
		case agent_protocol::codex_jsonl_exec:   return agent_program::codex;
		case agent_protocol::external_terminal:  break;
		}
		return std::nullopt;
	}

	auto inspect_agent_entry(const agent_entry& aEntry) -> std::optional<agent_entry_refusal>
	{
		if (aEntry.Name.empty() || aEntry.Name.size() > maximum_agent_entry_name_bytes || !plain_text(aEntry.Name)
			|| std::ranges::all_of(aEntry.Name, [](const char aByte) { return aByte == ' '; })) {
			return agent_entry_refusal::invalid_name;
		}
		if (static_cast<std::size_t>(aEntry.Protocol) >= agent_protocol_count) {
			return agent_entry_refusal::invalid_name;
		}
		if (aEntry.Program.empty()) {
			// Only a popular agent's own program is looked for.
			if (!aEntry.Preset || *aEntry.Preset == setup_agent::plain_text_harness || aEntry.Protocol == agent_protocol::external_terminal) {
				return agent_entry_refusal::no_program;
			}
		} else if (!storable_agent_wrapper(aEntry.Program) || !absolute(aEntry.Program)) {
			return agent_entry_refusal::invalid_program;
		}
		if (aEntry.Arguments.size() > maximum_agent_entry_arguments) {
			return agent_entry_refusal::too_many_arguments;
		}
		const auto badArgument = [](const std::string& aArgument) {
			return aArgument.empty() || aArgument.size() > maximum_agent_entry_argument_bytes || !plain_text(aArgument);
		};
		if (std::ranges::any_of(aEntry.Arguments, badArgument)) {
			return agent_entry_refusal::invalid_argument;
		}
		// A check of its own is a custom agent's, under the same rules.
		if (aEntry.CheckArguments.size() > maximum_agent_entry_arguments) {
			return agent_entry_refusal::too_many_arguments;
		}
		if (std::ranges::any_of(aEntry.CheckArguments, badArgument) || (aEntry.Preset && !aEntry.CheckArguments.empty())) {
			return agent_entry_refusal::invalid_argument;
		}
		if (aEntry.Preset && !aEntry.StrictRunChecks) {
			return agent_entry_refusal::relaxed_popular_agent;
		}
		const auto program = entry_settings_program(aEntry.Protocol);
		if (!aEntry.Model.empty() && (!program || !valid_claude_model(aEntry.Model))) {
			return agent_entry_refusal::invalid_model;
		}
		if (!aEntry.Effort.empty() && (!program || !valid_agent_effort(*program, aEntry.Effort))) {
			return agent_entry_refusal::invalid_effort;
		}
		return std::nullopt;
	}

	auto inspect_agent_entries(const std::span<const agent_entry> aEntries) -> std::optional<agent_entry_refusal>
	{
		if (aEntries.size() > maximum_agent_entries) {
			return agent_entry_refusal::too_many_entries;
		}
		for (auto index = std::size_t{ 0U }; index < aEntries.size(); ++index) {
			const auto& entry = aEntries[index];
			if (entry.Id == 0U || entry.Id > maximum_agent_entry_id) {
				return agent_entry_refusal::unknown_entry;
			}
			if (const auto refusal = inspect_agent_entry(entry)) {
				return refusal;
			}
			for (auto other = index + 1U; other < aEntries.size(); ++other) {
				if (aEntries[other].Id == entry.Id) {
					return agent_entry_refusal::unknown_entry;
				}
				if (aEntries[other].Name == entry.Name) {
					return agent_entry_refusal::duplicate_name;
				}
			}
		}
		return std::nullopt;
	}

	auto entry_profile_id(const agent_protocol aProtocol, const std::uint32_t aId) noexcept -> agent_profile_id
	{
		return agent_profile_id{ entry_profile_base + (static_cast<std::uint64_t>(aProtocol) * entry_profile_block) + aId };
	}

	auto entry_profile_id(const agent_entry& aEntry) noexcept -> agent_profile_id { return entry_profile_id(aEntry.Protocol, aEntry.Id); }

	auto is_agent_entry_profile(const agent_profile_id aProfile) noexcept -> bool
	{
		const auto value = aProfile.value();
		if (value <= entry_profile_base) {
			return false;
		}
		const auto offset   = value - entry_profile_base;
		const auto protocol = offset / entry_profile_block;
		const auto id       = offset % entry_profile_block;
		return protocol < agent_protocol_count && id >= 1U && id <= maximum_agent_entry_id;
	}

	auto entry_of_profile(const std::span<const agent_entry> aEntries, const agent_profile_id aProfile) noexcept -> const agent_entry*
	{
		const auto found = std::ranges::find_if(aEntries, [aProfile](const agent_entry& aEntry) { return entry_profile_id(aEntry) == aProfile; });
		return found == aEntries.end() ? nullptr : &*found;
	}

	auto agent_entry_profiles() -> std::vector<agent_profile>
	{
		auto profiles = std::vector<agent_profile>{ };
		profiles.reserve(protocols.size() * maximum_agent_entry_id);
		for (const auto protocol : protocols) {
			const auto base = built_in_for(protocol);
			for (auto id = std::uint32_t{ 1U }; id <= maximum_agent_entry_id; ++id) {
				auto profile        = base;
				profile.Id          = entry_profile_id(protocol, id);
				profile.DisplayName = "Agent " + std::to_string(id);
				profiles.push_back(std::move(profile));
			}
		}
		return profiles;
	}

	auto entry_profile(const agent_entry& aEntry) -> agent_profile
	{
		auto profile        = built_in_for(aEntry.Protocol);
		profile.Id          = entry_profile_id(aEntry);
		profile.DisplayName = aEntry.Name;
		return profile;
	}

	auto add_agent_entry(std::vector<agent_entry> aEntries, agent_entry aEntry) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>
	{
		if (aEntries.size() >= maximum_agent_entries) {
			return std::unexpected(agent_entry_refusal::too_many_entries);
		}
		aEntry.Id = free_id(aEntries);
		if (aEntry.Id == 0U) {
			return std::unexpected(agent_entry_refusal::too_many_entries);
		}
		aEntries.push_back(std::move(aEntry));
		if (const auto refusal = inspect_agent_entries(aEntries)) {
			return std::unexpected(*refusal);
		}
		return aEntries;
	}

	auto update_agent_entry(std::vector<agent_entry> aEntries, agent_entry aEntry) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>
	{
		const auto found = std::ranges::find(aEntries, aEntry.Id, &agent_entry::Id);
		if (found == aEntries.end()) {
			return std::unexpected(agent_entry_refusal::unknown_entry);
		}
		*found = std::move(aEntry);
		if (const auto refusal = inspect_agent_entries(aEntries)) {
			return std::unexpected(*refusal);
		}
		return aEntries;
	}

	auto remove_agent_entry(std::vector<agent_entry> aEntries, const std::uint32_t aId) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>
	{
		const auto found = std::ranges::find(aEntries, aId, &agent_entry::Id);
		if (found == aEntries.end()) {
			return std::unexpected(agent_entry_refusal::unknown_entry);
		}
		aEntries.erase(found);
		return aEntries;
	}

	auto move_agent_entry(std::vector<agent_entry> aEntries, const std::uint32_t aId, const int aDelta) -> std::expected<std::vector<agent_entry>, agent_entry_refusal>
	{
		const auto found = std::ranges::find(aEntries, aId, &agent_entry::Id);
		if (found == aEntries.end()) {
			return std::unexpected(agent_entry_refusal::unknown_entry);
		}
		const auto from = std::distance(aEntries.begin(), found);
		const auto to   = std::clamp<std::ptrdiff_t>(from + aDelta, 0, static_cast<std::ptrdiff_t>(aEntries.size()) - 1);
		if (to != from) {
			std::swap(aEntries[static_cast<std::size_t>(from)], aEntries[static_cast<std::size_t>(to)]);
		}
		return aEntries;
	}

	auto preset_agent_entry(const setup_agent aPreset, const codex_protocol_choice aCodex, std::string aProgram) -> agent_entry
	{
		auto entry     = agent_entry{ .Name = std::string{ plain_name(aPreset) }, .Program = std::move(aProgram) };
		entry.Preset   = aPreset;
		entry.Protocol = aPreset == setup_agent::claude_code ? agent_protocol::claude_stream_json
			: aPreset == setup_agent::codex                  ? (aCodex == codex_protocol_choice::jsonl ? agent_protocol::codex_jsonl_exec : agent_protocol::codex_app_server)
															 : agent_protocol::external_terminal;
		if (aPreset == setup_agent::plain_text_harness) {
			entry.Preset.reset();
		}
		return entry;
	}

	auto find_preset_entry(const std::span<const agent_entry> aEntries, const setup_agent aPreset) noexcept -> const agent_entry*
	{
		const auto found = std::ranges::find(aEntries, std::optional{ aPreset }, &agent_entry::Preset);
		return found == aEntries.end() ? nullptr : &*found;
	}

	namespace {
		[[nodiscard]] auto found_of(const std::span<const agent_detection> aFound, const setup_agent aAgent) noexcept -> const agent_detection*
		{
			const auto found = std::ranges::find_if(aFound, [aAgent](const agent_detection& aDetection) {
				return aDetection.Agent == aAgent && !aDetection.Path.empty() && aDetection.Path.size() <= maximum_agent_text_bytes;
			});
			return found == aFound.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto verdict_for(const agent_setup_choice_facts& aFacts, const agent_protocol aProtocol) -> agent_invocation_verdict
		{
			const auto profile = std::ranges::find(aFacts.Profiles, aProtocol, &agent_profile::Protocol);
			if (profile == aFacts.Profiles.end()) {
				return { .Refusal = agent_invocation_refusal::profile_unknown };
			}
			return decide_agent_invocation(*profile, aFacts.Policy);
		}

		[[nodiscard]] auto entry_choice(const agent_setup_choice_facts& aFacts, const agent_entry& aEntry) -> agent_setup_choice
		{
			auto choice = agent_setup_choice{ .Entry = aEntry.Id, .Agent = setup_agent_of(aEntry.Protocol), .Name = aEntry.Name };
			if (!aEntry.Program.empty()) {
				choice.Path = aEntry.Program;
				choice.How  = agent_found_how::saved_program;
			} else if (const auto* const located = found_of(aFacts.Located, choice.Agent); located != nullptr) {
				choice.Path = located->Path;
				choice.How  = located->How;
			}
			choice.Verdict = verdict_for(aFacts, aEntry.Protocol);
			return choice;
		}

		/// The card that is the setup's choice: as `preferred_entry` picks it.
		void mark_chosen(std::vector<agent_setup_choice>& aChoices, const agent_setup_choice_facts& aFacts)
		{
			if (!aFacts.Chosen) {
				return;
			}
			const auto kind      = *aFacts.Chosen;
			const auto preferred = std::ranges::find_if(aChoices, [&aFacts, kind](const agent_setup_choice& aChoice) {
				return aFacts.Preferred != 0U && aChoice.Entry == aFacts.Preferred && aChoice.Agent == kind;
			});
			if (preferred != aChoices.end()) {
				preferred->Chosen = true;
				return;
			}
			const auto* const preset = find_preset_entry(aFacts.Entries, kind);
			const auto        target = preset != nullptr ? preset->Id : 0U;
			const auto        chosen = std::ranges::find_if(aChoices, [kind, target](const agent_setup_choice& aChoice) { return aChoice.Entry == target && aChoice.Agent == kind; });
			if (chosen != aChoices.end()) {
				chosen->Chosen = true;
			}
		}
	} // namespace

	auto agent_setup_choices(const agent_setup_choice_facts& aFacts) -> std::vector<agent_setup_choice>
	{
		auto choices = std::vector<agent_setup_choice>{ };
		std::ranges::transform(aFacts.Entries, std::back_inserter(choices), [&aFacts](const agent_entry& aEntry) { return entry_choice(aFacts, aEntry); });
		// Popular agents found and not added: picking one adds it.
		for (const auto agent : { setup_agent::claude_code, setup_agent::codex }) {
			const auto* const located = found_of(aFacts.Located, agent);
			if (located == nullptr || find_preset_entry(aFacts.Entries, agent) != nullptr) {
				continue;
			}
			const auto protocol = preset_agent_entry(agent, aFacts.Codex, { }).Protocol;
			choices.push_back({ .Agent = agent, .Name = std::string{ plain_name(agent) }, .Path = located->Path, .How = located->How, .Verdict = verdict_for(aFacts, protocol) });
		}
		// A saved harness program no entry runs yet.
		if (const auto* const harness = found_of(aFacts.Found, setup_agent::plain_text_harness); harness != nullptr && harness->How == agent_found_how::saved_program) {
			const auto added = std::ranges::any_of(aFacts.Entries, [harness](const agent_entry& aEntry) {
				return aEntry.Protocol == agent_protocol::external_terminal && aEntry.Program == harness->Path;
			});
			if (!added) {
				choices.push_back({ .Agent = setup_agent::plain_text_harness, .Name = std::string{ plain_name(setup_agent::plain_text_harness) }, .Path = harness->Path, .How = harness->How, .Verdict = verdict_for(aFacts, agent_protocol::external_terminal) });
			}
		}
		mark_chosen(choices, aFacts);
		return choices;
	}

	auto migrate_agent_entries(const legacy_agent_settings& aSettings) -> std::vector<agent_entry>
	{
		auto       entries = std::vector<agent_entry>{ };
		const auto keep    = [&entries](agent_entry aEntry) {
			// A setting that would not be kept goes back to the agent's own.
			if (inspect_agent_entry(aEntry) == agent_entry_refusal::invalid_model) {
				aEntry.Model.clear();
			}
			if (inspect_agent_entry(aEntry) == agent_entry_refusal::invalid_effort) {
				aEntry.Effort.clear();
			}
			if (inspect_agent_entry(aEntry) == agent_entry_refusal::invalid_program && aEntry.Preset) {
				aEntry.Program.clear();
			}
			if (auto added = add_agent_entry(entries, std::move(aEntry))) {
				entries = std::move(*added);
			}
		};
		const auto claude = [&] {
			auto entry   = preset_agent_entry(setup_agent::claude_code, aSettings.CodexProtocol, aSettings.ClaudeWrapper);
			entry.Model  = aSettings.ClaudeModel;
			entry.Effort = aSettings.ClaudeEffort;
			keep(std::move(entry));
		};
		const auto codex = [&] {
			auto entry   = preset_agent_entry(setup_agent::codex, aSettings.CodexProtocol, aSettings.CodexProgram);
			entry.Model  = aSettings.CodexModel;
			entry.Effort = aSettings.CodexEffort;
			keep(std::move(entry));
		};
		const auto harness = [&] { keep(preset_agent_entry(setup_agent::plain_text_harness, aSettings.CodexProtocol, aSettings.HarnessProgram)); };
		const auto wanted  = [&aSettings](const setup_agent aAgent) {
			switch (aAgent) {
			case setup_agent::claude_code:        return aSettings.Chosen == aAgent || !aSettings.ClaudeWrapper.empty();
			case setup_agent::codex:              return aSettings.Chosen == aAgent || !aSettings.CodexProgram.empty();
			case setup_agent::plain_text_harness: return !aSettings.HarnessProgram.empty();
			}
			return false;
		};
		auto order = std::vector<setup_agent>{ setup_agent::claude_code, setup_agent::codex, setup_agent::plain_text_harness };
		if (aSettings.Chosen) {
			std::ranges::stable_partition(order, [&aSettings](const setup_agent aAgent) { return aAgent == aSettings.Chosen; });
		}
		for (const auto agent : order) {
			if (!wanted(agent)) {
				continue;
			}
			switch (agent) {
			case setup_agent::claude_code:        claude(); break;
			case setup_agent::codex:              codex(); break;
			case setup_agent::plain_text_harness: harness(); break;
			}
		}
		return entries;
	}
} // namespace bpr::core
