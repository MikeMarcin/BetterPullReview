#include "bpr/core/agent_setup.hpp"

#include <algorithm>
#include <array>

namespace bpr::core {
	namespace {
		constexpr auto setup_order = std::array{ setup_agent::claude_code, setup_agent::codex, setup_agent::plain_text_harness };

		[[nodiscard]] auto find_profile(const std::span<const agent_profile> aProfiles, const agent_profile_id aId) noexcept -> const agent_profile*
		{
			const auto found = std::ranges::find(aProfiles, aId, &agent_profile::Id);
			return found == aProfiles.end() ? nullptr : &*found;
		}
	} // namespace

	auto stable_name(const setup_agent aAgent) noexcept -> std::string_view
	{
		switch (aAgent) {
		case setup_agent::claude_code:        return "claude_code";
		case setup_agent::codex:              return "codex";
		case setup_agent::plain_text_harness: return "plain_text_harness";
		}
		return "claude_code";
	}

	auto plain_name(const setup_agent aAgent) noexcept -> std::string_view
	{
		switch (aAgent) {
		case setup_agent::claude_code:        return "Claude Code";
		case setup_agent::codex:              return "Codex";
		case setup_agent::plain_text_harness: return "A harness that reads plain text";
		}
		return "Claude Code";
	}

	auto setup_agent_of(const agent_protocol aProtocol) noexcept -> setup_agent
	{
		switch (aProtocol) {
		case agent_protocol::claude_stream_json: return setup_agent::claude_code;
		case agent_protocol::codex_app_server:
		case agent_protocol::codex_jsonl_exec:   return setup_agent::codex;
		case agent_protocol::external_terminal:  return setup_agent::plain_text_harness;
		}
		return setup_agent::plain_text_harness;
	}

	auto setup_profile(const setup_agent aAgent, const codex_protocol_choice aCodex) noexcept -> agent_profile_id
	{
		switch (aAgent) {
		case setup_agent::claude_code:        return claude_stream_json_profile_id;
		case setup_agent::codex:              return aCodex == codex_protocol_choice::jsonl ? codex_jsonl_profile_id : codex_app_server_profile_id;
		case setup_agent::plain_text_harness: return external_terminal_profile_id;
		}
		return claude_stream_json_profile_id;
	}

	auto stable_name(const agent_found_how aHow) noexcept -> std::string_view
	{
		switch (aHow) {
		case agent_found_how::on_path:       return "on_path";
		case agent_found_how::usual_folder:  return "usual_folder";
		case agent_found_how::saved_program: return "saved_program";
		}
		return "on_path";
	}

	auto agent_setup_rows(const std::span<const agent_detection> aFound, const std::span<const agent_profile> aProfiles,
						  const std::optional<agent_host_policy>& aPolicy, const codex_protocol_choice aCodex) -> std::vector<agent_setup_row>
	{
		auto rows = std::vector<agent_setup_row>{ };
		for (const auto agent : setup_order) {
			const auto found = std::ranges::find_if(aFound, [agent](const agent_detection& aDetection) {
				return aDetection.Agent == agent && !aDetection.Path.empty() && aDetection.Path.size() <= maximum_agent_text_bytes;
			});
			if (found == aFound.end()) {
				continue;
			}
			// Never searched for: only the program the reviewer chose.
			if (agent == setup_agent::plain_text_harness && found->How != agent_found_how::saved_program) {
				continue;
			}
			const auto  id      = setup_profile(agent, aCodex);
			const auto* profile = find_profile(aProfiles, id);
			if (profile == nullptr) {
				continue;
			}
			rows.push_back({
				.Agent   = agent,
				.Profile = id,
				.Path    = found->Path,
				.How     = found->How,
				.Verdict = decide_agent_invocation(*profile, aPolicy),
			});
		}
		return rows;
	}

	auto choose_setup_agent(const std::span<const agent_setup_row> aRows, const std::optional<setup_agent> aSaved) noexcept -> std::optional<setup_agent>
	{
		if (aSaved) {
			const auto saved = std::ranges::find(aRows, *aSaved, &agent_setup_row::Agent);
			if (saved != aRows.end() && saved->Verdict.Permitted) {
				return aSaved;
			}
		}
		auto only = std::optional<setup_agent>{ };
		for (const auto& row : aRows) {
			if (!row.Verdict.Permitted) {
				continue;
			}
			if (only) {
				return std::nullopt;
			}
			only = row.Agent;
		}
		return only;
	}

	auto searched_setup_agents(const std::span<const agent_profile> aProfiles, const std::optional<agent_host_policy>& aPolicy, const codex_protocol_choice aCodex)
		-> std::vector<setup_agent>
	{
		auto searched = std::vector<setup_agent>{ };
		for (const auto agent : { setup_agent::claude_code, setup_agent::codex }) {
			const auto* profile = find_profile(aProfiles, setup_profile(agent, aCodex));
			if (profile != nullptr && decide_agent_invocation(*profile, aPolicy).Permitted) {
				searched.push_back(agent);
			}
		}
		return searched;
	}

	auto stable_name(const agent_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case agent_status::signed_in:     return "signed_in";
		case agent_status::not_signed_in: return "not_signed_in";
		case agent_status::unreadable:    return "unreadable";
		}
		return "unreadable";
	}

	auto stable_name(const agent_setup_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case agent_setup_failure::agent_not_found:       return "agent_not_found";
		case agent_setup_failure::program_refused:       return "program_refused";
		case agent_setup_failure::no_program_chosen:     return "no_program_chosen";
		case agent_setup_failure::not_signed_in:         return "not_signed_in";
		case agent_setup_failure::failed_to_start:       return "failed_to_start";
		case agent_setup_failure::status_unreadable:     return "status_unreadable";
		case agent_setup_failure::timed_out:             return "timed_out";
		case agent_setup_failure::workspace_unavailable: return "workspace_unavailable";
		}
		return "failed_to_start";
	}

	auto apply_status_check(const agent_status_check& aCheck) -> setup_check
	{
		switch (aCheck.Status) {
		case agent_status::signed_in: {
			auto passed    = setup_check{ .State = setup_check_state::passed };
			passed.Version = aCheck.Version.size() <= maximum_agent_version_bytes ? aCheck.Version : std::string{ };
			return passed;
		}
		case agent_status::not_signed_in: return fail_check(agent_setup_failure::not_signed_in);
		case agent_status::unreadable:    return fail_check(agent_setup_failure::status_unreadable);
		}
		return fail_check(agent_setup_failure::status_unreadable);
	}

	auto fail_check(const agent_setup_failure aFailure) -> setup_check
	{
		auto failed    = setup_check{ .State = setup_check_state::failed };
		failed.Failure = aFailure;
		return failed;
	}

	auto stable_name(const setup_step aStep) noexcept -> std::string_view
	{
		switch (aStep) {
		case setup_step::agent:      return "agent";
		case setup_step::repository: return "repository";
		case setup_step::check:      return "check";
		case setup_step::ready:      return "ready";
		}
		return "agent";
	}

	auto current_setup_step(const agent_setup_state& aState) noexcept -> setup_step
	{
		const auto chosen = aState.Chosen && std::ranges::any_of(aState.Rows, [&aState](const agent_setup_row& aRow) {
								return aRow.Agent == *aState.Chosen && aRow.Verdict.Permitted;
							});
		if (!chosen) {
			return setup_step::agent;
		}
		if (!aState.RepositoryChosen) {
			return setup_step::repository;
		}
		if (aState.AgentCheck.State != setup_check_state::passed || aState.WorkspaceCheck.State != setup_check_state::passed) {
			return setup_step::check;
		}
		return setup_step::ready;
	}
} // namespace bpr::core
