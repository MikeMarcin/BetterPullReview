#include "bpr/application/agent_configuration_exchange.hpp"

#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/review_skill.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <set>
#include <utility>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto digest_of(const std::string_view aText) -> std::array<std::uint8_t, 32>
		{
			return core::sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() });
		}

		[[nodiscard]] auto skill_version_name(const std::uint32_t aVersion) -> std::string { return "bpr-pr-review-v" + std::to_string(aVersion); }

		[[nodiscard]] auto skill_version_of(const std::string_view aName) -> std::optional<std::uint32_t>
		{
			constexpr auto prefix = std::string_view{ "bpr-pr-review-v" };
			if (!aName.starts_with(prefix) || aName.size() == prefix.size() || aName.size() > prefix.size() + 9U) {
				return std::nullopt;
			}
			auto value = std::uint32_t{ 0U };
			for (const auto digit : aName.substr(prefix.size())) {
				if (digit < '0' || digit > '9') {
					return std::nullopt;
				}
				value = (value * 10U) + static_cast<std::uint32_t>(digit - '0');
			}
			return value;
		}

		[[nodiscard]] auto built_in_skill_version() -> std::uint32_t
		{
			const auto facts = core::inspect_review_skill(built_in_review_skill());
			return facts && facts->Version ? *facts->Version : 0U;
		}
	} // namespace

	auto prompt_hook(const std::string_view aPurpose) noexcept -> std::optional<core::writing_hook>
	{
		return aPurpose == review_skill_purpose ? std::nullopt : writing_hook_of_purpose(aPurpose);
	}

	auto local_prompts(const agent_configuration_snapshot& aSnapshot) -> std::vector<core::local_prompt>
	{
		auto prompts = std::vector<core::local_prompt>{ };
		for (std::size_t index = 0U; index < core::writing_hook_count; ++index) {
			const auto hook       = static_cast<core::writing_hook>(index);
			auto       prompt     = core::local_prompt{ };
			prompt.Purpose        = std::string{ writing_hook_purpose(hook) };
			prompt.BuiltIn        = std::string{ default_writing_hook(hook) };
			prompt.BuiltInVersion = std::string{ writing_hook_version_of(hook) };
			prompt.BuiltInDigest  = default_writing_hook_digest(hook);
			prompt.Override       = aSnapshot.Hooks.Overrides[index];
			if (!prompt.Override.empty()) {
				prompt.WrittenAgainst = aSnapshot.WrittenAgainst[index];
			}
			prompts.push_back(std::move(prompt));
		}
		auto skill           = core::local_prompt{ };
		skill.Purpose        = std::string{ review_skill_purpose };
		skill.Kind           = core::prompt_kind::review_skill;
		skill.BuiltIn        = std::string{ built_in_review_skill() };
		skill.BuiltInVersion = skill_version_name(built_in_skill_version());
		skill.BuiltInDigest  = digest_of(skill.BuiltIn);
		if (aSnapshot.ReviewSkill) {
			skill.Override       = aSnapshot.ReviewSkill->Text;
			skill.WrittenAgainst = skill_version_name(aSnapshot.ReviewSkill->Record.BaseVersion);
		}
		prompts.push_back(std::move(skill));
		return prompts;
	}

	auto export_agent_configuration(const agent_configuration_snapshot& aSnapshot, const std::span<const std::uint32_t> aAgents, const std::span<const exported_prompt> aPrompts,
									std::string aLabel, std::string aExportedBy, const std::span<const std::string> aWorkflows,
									const core::portable_identity* const aIdentity, const std::span<const core::portable_repository> aRepositories) -> std::optional<std::string>
	{
		// [[BPR-2032]]. A chosen workflow's agents travel with it.
		auto workflows = std::vector<const core::local_workflow*>{ };
		auto needed    = std::set<std::string>{ };
		for (const auto& workflow : aSnapshot.Workflows) {
			if (!std::ranges::contains(aWorkflows, workflow.Workflow.PortableId)) {
				continue;
			}
			workflows.push_back(&workflow);
			for (const auto& binding : workflow.Agents) {
				needed.insert(binding.AgentName);
			}
		}
		auto chosen = std::vector<const core::agent_entry*>{ };
		auto names  = std::vector<std::string>{ };
		for (const auto& entry : aSnapshot.Agents.Agents) {
			if (std::ranges::contains(aAgents, entry.Id) || needed.contains(entry.Name)) {
				chosen.push_back(&entry);
				names.push_back(entry.Name);
			}
		}
		const auto ids           = core::portable_ids(names);
		auto       configuration = core::agent_configuration{ .Label = std::move(aLabel), .ExportedBy = std::move(aExportedBy) };
		// [[BPR-2198]]. Names only: the composition hands over hosts, logins
		// and repository names, and nothing here can reach a sign-in.
		if (aIdentity != nullptr) {
			configuration.Identity = *aIdentity;
		}
		configuration.Repositories.assign(aRepositories.begin(), aRepositories.end());
		for (std::size_t index = 0U; index < chosen.size(); ++index) {
			configuration.Agents.push_back(core::portable_agent_of(*chosen[index], ids[index]));
		}
		const auto local = local_prompts(aSnapshot);
		for (const auto& prompt : local) {
			const auto pick = std::ranges::find(aPrompts, prompt.Purpose, &exported_prompt::Purpose);
			if (pick != aPrompts.end()) {
				configuration.Prompts.push_back(core::portable_prompt_of(prompt, pick->BuiltIn));
			}
		}
		for (const auto* const workflow : workflows) {
			auto shared = workflow->Workflow;
			for (auto& action : shared.Actions) {
				if (action.Binding != core::workflow_binding::agent) {
					continue;
				}
				const auto* const binding = core::bound_agent(*workflow, action.Id);
				const auto        named   = binding == nullptr ? names.end() : std::ranges::find(names, binding->AgentName);
				if (named == names.end()) {
					return std::nullopt;
				}
				action.Agent = ids[static_cast<std::size_t>(named - names.begin())];
			}
			configuration.Workflows.push_back(std::move(shared));
		}
		return core::encode_agent_configuration(configuration);
	}

	auto default_workflow_agents(const agent_configuration_preview& aPreview, const std::span<const core::agent_import_choice> aAgents, const std::size_t aItem)
		-> std::vector<core::workflow_agent_binding>
	{
		auto bindings = std::vector<core::workflow_agent_binding>{ };
		if (aItem >= aPreview.File.Workflows.size()) {
			return bindings;
		}
		const auto& entries = aPreview.Snapshot.Agents.Agents;
		for (const auto& action : aPreview.File.Workflows[aItem].Workflow.Actions) {
			if (action.Binding != core::workflow_binding::agent) {
				continue;
			}
			const auto item = std::ranges::find_if(aPreview.File.Agents, [&action](const core::decoded_agent& aAgent) { return aAgent.Agent.PortableId == action.Agent; });
			if (item == aPreview.File.Agents.end()) {
				continue;
			}
			const auto index  = static_cast<std::size_t>(item - aPreview.File.Agents.begin());
			const auto choice = std::ranges::find(aAgents, index, &core::agent_import_choice::Item);
			auto       name   = std::string{ };
			if (choice != aAgents.end() && choice->Action == core::agent_import_action::add) {
				name = choice->Name.empty() ? item->Agent.Name : choice->Name;
			} else if (choice != aAgents.end() && choice->Action != core::agent_import_action::leave) {
				const auto target = std::ranges::find(entries, choice->Target, &core::agent_entry::Id);
				name              = target == entries.end() ? std::string{ } : target->Name;
			} else if (const auto* const existing = core::colliding_agent(entries, item->Agent); existing != nullptr) {
				name = existing->Name;
			}
			if (!name.empty()) {
				bindings.push_back({ .Action = action.Id, .AgentName = std::move(name) });
			}
		}
		return bindings;
	}

	auto snapshot_after_import(const agent_configuration_snapshot& aBefore, const core::agent_configuration_result& aResult, const std::span<const std::uint32_t> aReplaced,
							   const std::function<std::string()>& aMint) -> agent_configuration_snapshot
	{
		auto after          = aBefore;
		after.Agents.Agents = aResult.Agents;
		for (auto& entry : after.Agents.Agents) {
			if (entry.Identity.empty() && aMint) {
				entry.Identity = aMint();
			}
		}
		std::erase_if(after.Agents.EntryChecks, [&aReplaced](const agent_entry_check_record& aRecord) { return std::ranges::contains(aReplaced, aRecord.Id); });
		if (!after.Agents.AgentsMoved && !after.Agents.Agents.empty()) {
			after.Agents.AgentsMoved = true;
		}
		for (std::size_t index = 0U; index < core::writing_hook_count && index < aResult.Prompts.size(); ++index) {
			const auto& prompt           = aResult.Prompts[index];
			after.Hooks.Overrides[index] = prompt.Override;
			after.WrittenAgainst[index]  = prompt.Override.empty() ? std::nullopt : prompt.WrittenAgainst;
		}
		if (after.Hooks.Overrides != aBefore.Hooks.Overrides) {
			after.Hooks.Revision = aBefore.Hooks.Revision + 1U;
		}
		if (aResult.Prompts.size() > core::writing_hook_count) {
			const auto& skill = aResult.Prompts[core::writing_hook_count];
			if (skill.Override.empty()) {
				after.ReviewSkill.reset();
			} else if (!aBefore.ReviewSkill || aBefore.ReviewSkill->Text != skill.Override) {
				// The copy records the built-in it was written against, so a
				// newer default is still offered; unknown is today's.
				const auto version      = skill.WrittenAgainst ? skill_version_of(*skill.WrittenAgainst) : std::nullopt;
				auto       copy         = stored_review_skill{ .Text = skill.Override };
				copy.Record.BaseVersion = version.value_or(built_in_skill_version());
				copy.Record.BaseDigest  = copy.Record.BaseVersion == built_in_skill_version() ? digest_of(built_in_review_skill()) : std::array<std::uint8_t, 32>{ };
				after.ReviewSkill       = std::move(copy);
			}
		}
		return after;
	}

	auto stable_name(const agent_configuration_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case agent_configuration_outcome::applied:        return "applied";
		case agent_configuration_outcome::changed_since:  return "changed_since";
		case agent_configuration_outcome::storage_failed: return "storage_failed";
		}
		return "storage_failed";
	}

	struct agent_configuration_host::implementation final : std::enable_shared_from_this<implementation>
	{
		explicit implementation(agent_configuration_dependencies aDependencies)
			: Dependencies{ std::move(aDependencies) }
		{ }

		template <typename Task, typename Settle>
		void off_lane(Task aTask, Settle aSettle)
		{
			if (Closing || Dependencies.Effects == nullptr || Dependencies.Decision == nullptr) {
				return;
			}
			++Outstanding;
			try {
				Dependencies.Effects->post([self = shared_from_this(), task = std::move(aTask), settle = std::move(aSettle)]() mutable {
					auto result = std::optional<decltype(task())>{ };
					if (!self->Closing) {
						try {
							result.emplace(task());
						} catch (...) { }
					}
					try {
						self->Dependencies.Decision->post([owner = self, reply = std::move(settle), answer = std::move(result)]() mutable {
							--owner->Outstanding;
							if (!owner->Closing && answer) {
								reply(std::move(*answer));
							}
						});
					} catch (...) {
						--self->Outstanding;
					}
				});
			} catch (...) {
				--Outstanding;
			}
		}

		agent_configuration_dependencies Dependencies;
		std::atomic<bool>                Closing{ false };
		std::atomic<std::size_t>         Outstanding{ 0U };
	};

	agent_configuration_host::agent_configuration_host(agent_configuration_dependencies aDependencies)
		: Implementation{ std::make_shared<implementation>(std::move(aDependencies)) }
	{ }

	agent_configuration_host::~agent_configuration_host() { shutdown(); }

	void agent_configuration_host::snapshot(std::function<void(std::optional<agent_configuration_snapshot>)> aAnswered)
	{
		auto* const store = Implementation->Dependencies.Store;
		Implementation->off_lane([store] { return store == nullptr ? std::optional<agent_configuration_snapshot>{ } : store->load(); },
								 [answered = std::move(aAnswered)](std::optional<agent_configuration_snapshot> aSnapshot) {
									 if (answered) {
										 answered(std::move(aSnapshot));
									 }
								 });
	}

	void agent_configuration_host::write(std::string aPath, std::string aBytes, std::function<void(bool)> aAnswered)
	{
		auto writer = Implementation->Dependencies.WriteFile;
		Implementation->off_lane([writer, path = std::move(aPath), bytes = std::move(aBytes)] { return writer && writer(path, bytes); },
								 [answered = std::move(aAnswered)](const bool aWritten) {
									 if (answered) {
										 answered(aWritten);
									 }
								 });
	}

	void agent_configuration_host::read(std::string aPath, std::function<void(std::expected<agent_configuration_preview, agent_configuration_read_error>)> aAnswered)
	{
		auto* const store  = Implementation->Dependencies.Store;
		auto        reader = Implementation->Dependencies.ReadFile;
		Implementation->off_lane(
			[store, reader, path = std::move(aPath)]() -> std::expected<agent_configuration_preview, agent_configuration_read_error> {
				const auto bytes = reader ? reader(path, core::maximum_agent_configuration_bytes) : std::nullopt;
				if (!bytes) {
					return std::unexpected(agent_configuration_read_error{ });
				}
				auto snapshot = store == nullptr ? std::nullopt : store->load();
				if (!snapshot) {
					return std::unexpected(agent_configuration_read_error{ });
				}
				auto preview    = agent_configuration_preview{ .Snapshot = std::move(*snapshot) };
				preview.Prompts = local_prompts(preview.Snapshot);
				auto decoded    = core::decode_agent_configuration(*bytes, preview.Prompts);
				if (!decoded) {
					return std::unexpected(agent_configuration_read_error{ .Refusal = decoded.error() });
				}
				preview.File = std::move(*decoded);
				return preview;
			},
			[answered = std::move(aAnswered)](std::expected<agent_configuration_preview, agent_configuration_read_error> aPreview) {
				if (answered) {
					answered(std::move(aPreview));
				}
			});
	}

	void agent_configuration_host::apply(const agent_configuration_preview& aPreview, std::vector<core::agent_import_choice> aAgents, std::vector<std::size_t> aPrompts,
										 std::function<void(applied)> aAnswered, std::vector<core::workflow_import_choice> aWorkflows)
	{
		auto result = core::apply_agent_configuration(aPreview.Snapshot.Agents.Agents, aPreview.Prompts, aPreview.File, aAgents, aPrompts);
		if (!result) {
			if (aAnswered) {
				aAnswered(std::unexpected(result.error()));
			}
			return;
		}
		// [[BPR-2032]]. Workflows apply with the same choice, all or none.
		for (auto& choice : aWorkflows) {
			if (choice.Agents.empty()) {
				choice.Agents = default_workflow_agents(aPreview, aAgents, choice.Item);
			}
		}
		auto workflows = core::apply_workflow_choices(aPreview.Snapshot.Workflows, aPreview.File.Workflows, aWorkflows);
		if (!workflows) {
			if (aAnswered) {
				aAnswered(std::unexpected(core::configuration_apply_error{ .Refusal = core::configuration_apply_refusal::unusable_item, .Item = workflows.error().Item, .Workflow = workflows.error() }));
			}
			return;
		}
		auto after        = snapshot_after_import(aPreview.Snapshot, *result, result->Replaced, Implementation->Dependencies.MintIdentity);
		after.Workflows   = std::move(*workflows);
		auto* const store = Implementation->Dependencies.Store;
		Implementation->off_lane(
			[store, before = aPreview.Snapshot, after]() { return store == nullptr ? agent_configuration_outcome::storage_failed : store->apply(before, after); },
			[answered = std::move(aAnswered), after](const agent_configuration_outcome aOutcome) {
				if (answered) {
					answered(std::pair{ aOutcome, after });
				}
			});
	}

	void agent_configuration_host::record_written_against(const core::writing_hook aHook, std::string aText)
	{
		auto* const store = Implementation->Dependencies.Store;
		Implementation->off_lane([store, aHook, text = std::move(aText)] { return store != nullptr && store->record_written_against(aHook, text, writing_hook_version_of(aHook)); },
								 [](bool) { });
	}

	void agent_configuration_host::shutdown() noexcept { Implementation->Closing = true; }

	auto agent_configuration_host::is_quiescent() const noexcept -> bool { return Implementation->Outstanding == 0U; }
} // namespace bpr::application
