/// \file
/// \brief [[BPR-2153]], RFC-BPR-0126 AC05 and AC07. Whether a running turn
/// can be steered, a correction's identity, and a steered run's cost.

#include "bpr/core/agent_steering.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void test_support_is_decided_from_local_observations()
	{
		using core::agent_protocol;
		using core::agent_steer_support;
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "2.1.319") == agent_steer_support::available, "the qualified Claude Code steers");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "2.2.0") == agent_steer_support::available, "a newer one too");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "2.1.320 (Claude Code)") == agent_steer_support::available, "a version said with its product name");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "2.1.314") == agent_steer_support::version, "an older one keeps Queue");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "") == agent_steer_support::version_unknown, "an unreported version is unknown, never assumed");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, true, "two.one") == agent_steer_support::version, "nor is a version that does not read");
		expect(core::agent_steer_support_of(agent_protocol::claude_stream_json, false, "2.1.319") == agent_steer_support::no_connection, "a one-shot run has no channel to add to");
		expect(core::agent_steer_support_of(agent_protocol::codex_app_server, true, "") == agent_steer_support::available, "App Server's turn/steer names the turn it expects");
		expect(core::agent_steer_support_of(agent_protocol::codex_jsonl_exec, true, "0.167.0") == agent_steer_support::protocol, "Codex exec cannot be steered");
		expect(core::agent_steer_support_of(agent_protocol::external_terminal, true, "") == agent_steer_support::protocol, "nor an external terminal");
		expect(core::agent_steer_unavailable_words(agent_steer_support::available).empty(), "available needs no words");
		for (const auto support : { agent_steer_support::not_running, agent_steer_support::no_connection, agent_steer_support::protocol, agent_steer_support::version, agent_steer_support::exhausted, agent_steer_support::version_unknown, agent_steer_support::other_run, agent_steer_support::empty_text, agent_steer_support::text_too_long }) {
			expect(!core::agent_steer_unavailable_words(support).empty(), "every refusal names why Steer is unavailable");
		}
	}

	void test_identities_are_uuid_shaped_and_distinct()
	{
		const auto identity = core::agent_steer_identity(7U, 0U);
		expect(identity.size() == 36U && identity[8] == '-' && identity[13] == '-' && identity[14] == '4' && identity[18] == '-' && identity[19] == '8' && identity[23] == '-', "a correction's identity is a version-4 shaped UUID");
		expect(identity == core::agent_steer_identity(7U, 0U), "the same turn and ordinal give the same identity");
		auto seen = std::set<std::string>{ };
		for (const auto turn : { 2ULL, 3ULL, 0xFFFFFFULL, 0x1000000ULL, 0xFFFFFFFFFFFFFFFFULL }) {
			for (std::uint32_t ordinal = 0U; ordinal < core::maximum_steers_per_run; ++ordinal) {
				expect(seen.insert(core::agent_steer_identity(turn, ordinal)).second, "no two corrections of a conversation share an identity");
			}
		}
	}

	void test_a_steered_run_is_counted_once()
	{
		const auto cost = [](const std::string_view aText) { return core::parse_agent_cost(aText); };
		// Claude's cumulative figures for one run that ended in two results.
		auto reading = core::read_run_cost_report({ }, cost("0.0050"));
		reading      = core::read_run_cost_report(reading, cost("0.0081"));
		expect(reading.Figure == cost("0.0081") && !reading.Unknown && reading.Reports == 2U, "the run's figure is its last cumulative report, never the sum");
		const auto turn = core::account_agent_turn_cost({ .Reported = cost("0.0020"), .Total = *cost("0.0020"), .Known = true }, reading.Figure, true);
		expect(turn.Turn == cost("0.0061") && turn.Ledger.Total == *cost("0.0081"), "both parts count once against the turn and the session");
		reading = core::read_run_cost_report(reading, std::nullopt);
		expect(reading.Figure == cost("0.0081") && reading.Reports == 2U, "a part that reports nothing changes nothing");
		reading = core::read_run_cost_report(reading, cost("0.0010"));
		expect(!reading.Figure && reading.Unknown, "a figure that went down mid-run makes the run's cost unknown");
		reading = core::read_run_cost_report(reading, cost("0.0200"));
		expect(!reading.Figure && reading.Unknown, "and unknown stays unknown");
		expect(!core::read_run_cost_report({ }, std::nullopt).Figure, "a run that reported nothing has no figure");
	}
} // namespace

int main()
{
	test_support_is_decided_from_local_observations();
	test_identities_are_uuid_shaped_and_distinct();
	test_a_steered_run_is_counted_once();
	std::cout << "agent-steering-tests:pass\n";
	return EXIT_SUCCESS;
}
