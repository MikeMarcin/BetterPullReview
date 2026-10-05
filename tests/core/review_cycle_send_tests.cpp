#include "bpr/core/review_cycle_send.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1180]]. Send to Agent decided by the round's channel and the
/// profile's own host policy, sessions bound exactly, and what one send may
/// record about its round.

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle send failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	constexpr auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	constexpr auto subject = local_cycle_subject{
		.Authority  = { .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) },
		.Cycle      = review_cycle_id{ 3 },
		.Generation = 1,
	};
	constexpr auto round = cycle_round{
		.Id            = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_id{ 2 } },
		.Draft         = { .Cycle = review_cycle_id{ 3 }, .Id = cycle_round_draft_id{ 1 } },
		.DraftRevision = 4,
		.Digest        = identity<cycle_round_digest>(9),
	};
	constexpr auto output = cycle_delivery_output{ .Format = cycle_round_file_kind::markdown, .Digest = identity<cycle_material_digest>(4), .Bytes = 120 };

	auto acknowledged(const std::uint64_t aOrdinal, std::string aDestination) -> cycle_delivery_record
	{
		return {
			.Ordinal     = aOrdinal,
			.Observation = { .Round = round.Id, .Digest = round.Digest, .State = cycle_delivery_state::recipient_acknowledged, .Channel = cycle_disclosure_channel::agent_adapter, .Destination = std::move(aDestination) },
		};
	}

	auto offer_of(const std::vector<cycle_send_offer>& aOffers, const agent_profile_id aProfile) -> cycle_send_offer
	{
		for (const auto& offer : aOffers) {
			if (offer.Profile == aProfile) {
				return offer;
			}
		}
		expect(false, "every installed profile is listed");
		return { };
	}

	void names_sessions_exactly()
	{
		expect(send_session_name("a") && send_session_name("0f8e-41b2_c.9") && send_session_name(std::string(maximum_send_session_bytes, 'x')), "ordinary session names");
		for (const auto* name : { "", "a b", "a/b", "a:b", "a\nb", "é", "--resume", ".hidden" }) {
			expect(!send_session_name(name), "only letters, digits, dot, underscore and dash, never read as an option");
		}
		expect(!send_session_name(std::string(maximum_send_session_bytes + 1U, 'x')), "within its bound");
		expect(continues_sessions(agent_protocol::claude_stream_json), "Claude stream JSON resumes a session");
		for (const auto protocol : { agent_protocol::codex_app_server, agent_protocol::codex_jsonl_exec, agent_protocol::external_terminal }) {
			expect(!continues_sessions(protocol), "no other harness is told to continue one");
		}
		const auto record = send_session_record(agent_protocol::claude_stream_json, "s-1");
		expect(record == "claude_stream_json/s-1", "protocol and session");
		expect(parse_send_session_record(record) == std::pair{ agent_protocol::claude_stream_json, std::string{ "s-1" } }, "and back");
		expect(send_session_record(agent_protocol::claude_stream_json, "a/b").empty(), "no record for a name that is not one");
		for (const auto* text : { "", "s-1", "claude/s-1", "claude_stream_json/", "claude_stream_json/a b", "RC-3-F2.md" }) {
			expect(!parse_send_session_record(text), "nothing else parses as a session");
		}
	}

	/// RC60-08: a corporate account or known enterprise source keeps the
	/// corporate policy; nothing is inferred from a folder alone.
	void answers_to_the_profiles_own_host_policy()
	{
		const auto folder = cycle_workspace_origin{ };
		expect(cycle_send_policy(folder, std::nullopt).Host == send_github_host, "a local folder, no account");
		expect(cycle_send_policy(folder, std::string_view{ "github.com" }).Host == send_github_host, "a GitHub.com account");
		expect(cycle_send_policy(folder, std::string_view{ "GitHub.com" }).Host == send_github_host, "host names compare without case");
		expect(cycle_send_policy(folder, std::string_view{ "ghe.example.com" }).Host == send_enterprise_host, "an account on a corporate host");
		const auto enterprise = cycle_workspace_origin{ .Classification = cycle_data_classification::enterprise_restricted };
		expect(cycle_send_policy(enterprise, std::nullopt).Host == send_enterprise_host, "known enterprise source");
		expect(cycle_send_policy(enterprise, std::string_view{ "github.com" }).Host == send_enterprise_host, "a GitHub.com account does not wash it away");

		const auto profiles   = built_in_agent_profiles();
		const auto deliveries = std::vector{
			acknowledged(1, send_session_record(agent_protocol::claude_stream_json, "old")),
			acknowledged(2, send_session_record(agent_protocol::codex_jsonl_exec, "thread")),
			acknowledged(3, send_session_record(agent_protocol::claude_stream_json, "new")),
			acknowledged(4, send_session_record(agent_protocol::claude_stream_json, "old")),
		};
		const auto corporate = send_offers(profiles, cycle_send_policy(folder, std::string_view{ "ghe.example.com" }), deliveries);
		expect(corporate.size() == profiles.size(), "refused profiles stay listed");
		for (const auto codex : { codex_app_server_profile_id, codex_jsonl_profile_id }) {
			const auto offer = offer_of(corporate, codex);
			expect(!offer.offered() && offer.Verdict.Refusal == agent_invocation_refusal::profile_forbidden_on_host && offer.Verdict.RefusedBy == send_enterprise_host,
				   "the corporate host refuses Codex, by name");
		}
		const auto claude = offer_of(corporate, claude_stream_json_profile_id);
		expect(claude.offered() && claude.Continues && claude.Sessions == std::vector<std::string>{ "old", "new" }, "Claude continues its own sessions, newest first, each once");
		const auto terminal = offer_of(corporate, external_terminal_profile_id);
		expect(terminal.offered() && !terminal.Continues && terminal.Sessions.empty(), "the external terminal starts a new session every time");
		const auto open = send_offers(profiles, cycle_send_policy(folder, std::nullopt), deliveries);
		expect(offer_of(open, codex_jsonl_profile_id).offered(), "GitHub.com permits Codex");
		// [[BPR-1282]]. Each offer names its harness, never the built-in label.
		expect(claude.Harness == "Claude" && claude.Protocol == agent_protocol::claude_stream_json, "Claude's stream protocol is Claude");
		expect(offer_of(open, codex_app_server_profile_id).Harness == "Codex" && offer_of(open, codex_jsonl_profile_id).Harness == "Codex", "either Codex protocol is Codex");
		expect(terminal.Harness == terminal.DisplayName && terminal.Harness.find("Claude") == std::string::npos, "a custom harness by its own name, not Claude's");
		auto custom        = profiles.back();
		custom.DisplayName = "Team review bot";
		expect(harness_name(custom) == "Team review bot", "whatever the custom profile is called");
	}

	void decides_before_anything_is_frozen()
	{
		const auto profiles   = built_in_agent_profiles();
		const auto github     = cycle_send_policy({ }, std::nullopt);
		const auto corporate  = cycle_send_policy({ }, std::string_view{ "ghe.example.com" });
		const auto deliveries = std::vector{ acknowledged(1, send_session_record(agent_protocol::claude_stream_json, "s-1")) };
		const auto claude     = cycle_send_request{ .Profile = claude_stream_json_profile_id };
		expect(!decide_send(cycle_authorization::allowed, profiles, github, deliveries, claude), "a new session");
		expect(!decide_send(cycle_authorization::allowed, profiles, corporate, deliveries, claude), "Claude on the corporate host");

		const auto unconfigured = decide_send(cycle_authorization::channel_unconfigured, profiles, github, deliveries, { .Profile = agent_profile_id{ 99 } });
		expect(unconfigured && unconfigured->Problem == cycle_send_problem::channel_refused && unconfigured->Channel == cycle_authorization::channel_unconfigured,
			   "the round's channel decides first");
		const auto denied = decide_send(cycle_authorization::channel_denied, profiles, github, deliveries, claude);
		expect(denied && denied->Problem == cycle_send_problem::channel_refused && denied->Channel == cycle_authorization::channel_denied, "a denied channel stays denied");
		const auto unknown = decide_send(cycle_authorization::allowed, profiles, github, deliveries, { .Profile = agent_profile_id{ 99 } });
		expect(unknown && unknown->Problem == cycle_send_problem::profile_unknown, "only an installed profile");
		const auto codex = decide_send(cycle_authorization::allowed, profiles, corporate, deliveries, { .Profile = codex_jsonl_profile_id });
		expect(codex && codex->Problem == cycle_send_problem::profile_refused && codex->Verdict.Refusal == agent_invocation_refusal::profile_forbidden_on_host
				   && codex->Verdict.RefusedBy == send_enterprise_host,
			   "the denied GHES profile is refused, by its host");
		expect(stable_name(codex->Problem) == "profile_refused", "a stable name");

		const auto terminal = decide_send(cycle_authorization::allowed, profiles, github, deliveries, { .Profile = external_terminal_profile_id, .Session = { .Continue = "s-1" } });
		expect(terminal && terminal->Problem == cycle_send_problem::session_not_continuable, "a harness that cannot continue is never told to");
		const auto stranger = decide_send(cycle_authorization::allowed, profiles, github, deliveries, { .Profile = claude_stream_json_profile_id, .Session = { .Continue = "s-2" } });
		expect(stranger && stranger->Problem == cycle_send_problem::session_unknown, "only a session this cycle's sends were told");
		expect(!decide_send(cycle_authorization::allowed, profiles, github, deliveries, { .Profile = claude_stream_json_profile_id, .Session = { .Continue = "s-1" } }), "that exact one");
	}

	/// RC60-09: delivered, acknowledged and finished are said apart; nothing
	/// claims more than was seen.
	void records_only_what_was_seen()
	{
		const auto claude = agent_protocol::claude_stream_json;
		const auto states = [](const std::vector<cycle_delivery_observation>& aSeen) {
			auto result = std::vector<cycle_delivery_state>{ };
			for (const auto& seen : aSeen) {
				result.push_back(seen.State);
				expect(seen.Channel == cycle_disclosure_channel::agent_adapter && seen.Round == round.Id && seen.Digest == round.Digest, "the adapter channel, this round");
				expect(!admit_delivery(subject, round, seen), "each is admitted as it stands");
			}
			return result;
		};
		using enum cycle_delivery_state;
		const auto never = send_observations(round, output, claude, { .End = cycle_send_end::not_started });
		expect(states(never) == std::vector{ failed } && !never.front().Output, "nothing started: failed, and no bytes left");
		const auto finished = send_observations(round, output, claude, { .End = cycle_send_end::completed, .Session = "s-1", .Reply = "done" });
		expect(states(finished) == std::vector{ accepted_by_adapter, recipient_acknowledged }, "finished: accepted and acknowledged, nothing more");
		expect(finished[0].Output == output && finished[1].Destination == "claude_stream_json/s-1", "the bytes handed over, and the session reported");
		const auto quiet = send_observations(round, output, agent_protocol::external_terminal, { .End = cycle_send_end::completed });
		expect(states(quiet) == std::vector{ accepted_by_adapter }, "no session reported, none claimed");
		for (const auto end : { cycle_send_end::stopped, cycle_send_end::timed_out, cycle_send_end::cancelled }) {
			expect(states(send_observations(round, output, claude, { .End = end, .Session = "s-1" })) == std::vector{ accepted_by_adapter, recipient_acknowledged, uncertain },
				   "an unfinished turn leaves delivery uncertain");
			expect(states(send_observations(round, output, claude, { .End = end })) == std::vector{ accepted_by_adapter, uncertain }, "with or without a session");
		}
		const auto odd = send_observations(round, output, claude, { .End = cycle_send_end::completed, .Session = "not a session" });
		expect(states(odd) == std::vector{ accepted_by_adapter }, "a session that is not a name is not recorded");
		expect(stable_name(cycle_send_end::timed_out) == "timed_out", "a stable name");

		auto stray        = acknowledged(1, "RC-3-F2.md").Observation;
		auto elsewhere    = acknowledged(1, send_session_record(claude, "s-1")).Observation;
		elsewhere.Channel = cycle_disclosure_channel::external_retrieval;
		expect(admit_delivery(subject, round, stray) == cycle_delivery_problem::unexpected_destination, "an acknowledgment names a session or nothing");
		expect(admit_delivery(subject, round, elsewhere) == cycle_delivery_problem::unexpected_destination, "and only the adapter's");
	}

	void reads_a_response_out_of_a_reply()
	{
		expect(reply_response("  {\"schema\":\"x\"}\n") == std::string_view{ "{\"schema\":\"x\"}" }, "the whole reply");
		expect(reply_response("Done.\n```json\n{\"a\":1}\n```\nThanks.") == std::string_view{ "{\"a\":1}" }, "its one json block");
		expect(!reply_response("```json\n{}\n```\n```json\n{}\n```"), "two blocks are not one response");
		expect(!reply_response("I changed the name and ran the tests."), "prose is a reply, not a response");
		expect(!reply_response(" \n") && !reply_response("```json\n{}"), "nothing, or an unclosed block");
	}
}

int main()
{
	names_sessions_exactly();
	answers_to_the_profiles_own_host_policy();
	decides_before_anything_is_frozen();
	records_only_what_was_seen();
	reads_a_response_out_of_a_reply();
	return EXIT_SUCCESS;
}
