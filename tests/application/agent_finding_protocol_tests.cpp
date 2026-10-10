/// \file
/// \brief Proves the bounded structured-finding wire contract and its causes.

#include "agent_protocol_loop.hpp"

#include "bpr/application/agent_finding_delivery.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using namespace bpr::adapters::agent::detail;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	// The decode path takes values and returns values. A signature that grew a
	// store, a snapshot, a projection, or any other handle would be a decode
	// path holding something it could write through, and this stops compiling.
	static_assert(
		std::is_same_v<
			decltype(&application::decode_agent_finding_payload),
			std::expected<
				application::agent_finding_payload,
				application::agent_protocol_failure> (*)(
				core::agent_protocol, std::string_view, core::agent_session_id)>,
		"the finding decode reads only a protocol, bytes, and a session");

	void field_shape_guard()
	{
		const auto payload              = application::agent_finding_payload{ };
		const auto& [session, findings] = payload;
		static_assert(
			!std::is_pointer_v<std::remove_cvref_t<decltype(session)>>
				&& !std::is_pointer_v<std::remove_cvref_t<decltype(findings)>>,
			"a decoded payload holds values, never a handle into anything");
		static_cast<void>(session);
		static_cast<void>(findings);
	}

	constexpr auto run_session = core::agent_session_id{ 41U };

	using json_members = std::vector<std::pair<std::string, std::string>>;

	[[nodiscard]] auto render(const json_members& aMembers) -> std::string
	{
		auto result = std::string{ "{" };
		for (const auto& [name, value] : aMembers) {
			if (result.size() > 1U) {
				result += ',';
			}
			result += '"';
			result += name;
			result += "\":";
			result += value;
		}
		return result + "}";
	}

	void put(
		json_members&      aMembers,
		const std::string& aName,
		const std::string& aValue)
	{
		const auto found = std::ranges::find(
			aMembers, aName, &std::pair<std::string, std::string>::first);
		if (found == aMembers.end()) {
			aMembers.emplace_back(aName, aValue);
			return;
		}
		found->second = aValue;
	}

	void drop(json_members& aMembers, const std::string& aName)
	{
		std::erase_if(aMembers, [&aName](const auto& aMember) {
			return aMember.first == aName;
		});
	}

	[[nodiscard]] auto quoted(const std::string& aText) -> std::string
	{
		return '"' + aText + '"';
	}

	[[nodiscard]] auto repeated(const std::size_t aCount, const char aDigit)
		-> std::string
	{
		return std::string(aCount, aDigit);
	}

	/// \brief The one well-formed finding every case starts from.
	[[nodiscard]] auto base_finding() -> json_members
	{
		return {
			{ "provider", "1" },
			{ "severity", quoted("major") },
			{ "confidence", quoted("high") },
			{ "category", quoted("correctness") },
			{ "path", quoted("src/canary.cpp") },
			{ "lines", "{\"begin\":10,\"end\":12}" },
			{ "anchor",
			  "{\"line\":{\"comparison\":\"" + repeated(64U, '5')
				  + "\",\"side\":\"right\",\"line\":11}}" },
			{ "claim", quoted("canary-claim") },
			{ "evidence", quoted("canary-evidence") },
			{ "consequence", quoted("canary-consequence") },
			{ "suggestedComment", quoted("canary-suggested-comment") },
			{ "requiredContext", quoted("canary-required-context") },
			{ "analyzedBase", quoted(repeated(40U, '1')) },
			{ "analyzedHead", quoted(repeated(40U, '2')) },
			{ "analyzedCommits", "[\"" + repeated(40U, '3') + "\"]" },
		};
	}

	[[nodiscard]] auto payload_of(
		const std::string& aSession, const std::vector<std::string>& aFindings)
		-> std::string
	{
		auto findings = std::string{ "[" };
		for (const auto& finding : aFindings) {
			if (findings.size() > 1U) {
				findings += ',';
			}
			findings += finding;
		}
		return "{\"session\":" + aSession + ",\"findings\":" + findings + "]}";
	}

	[[nodiscard]] auto one_finding_payload(const json_members& aFinding)
		-> std::string
	{
		return payload_of("41", { render(aFinding) });
	}

	/// \brief The `core::agent_finding` the base wire object must decode into.
	[[nodiscard]] auto expected_finding() -> core::agent_finding
	{
		auto comparison = std::array<std::uint8_t, 32U>{ };
		comparison.fill(0x55U);
		auto base = std::array<std::uint8_t, 32U>{ };
		auto head = std::array<std::uint8_t, 32U>{ };
		auto only = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			base[index] = 0x11U;
			head[index] = 0x22U;
			only[index] = 0x33U;
		}
		return {
			.Provider         = core::agent_profile_id{ 1U },
			.Severity         = core::defect_severity::major,
			.Confidence       = core::agent_finding_confidence::high,
			.Category         = core::defect_category::correctness,
			.Path             = "src/canary.cpp",
			.Lines            = { .Begin = 10U, .End = 12U },
			.Anchor           = { .Line = core::source_line_ref{
									  .Comparison = core::file_comparison_digest{ comparison },
									  .Side       = core::diff_side::right,
									  .Line       = 11U,
								  } },
			.Claim            = "canary-claim",
			.Evidence         = "canary-evidence",
			.Consequence      = "canary-consequence",
			.SuggestedComment = "canary-suggested-comment",
			.RequiredContext  = "canary-required-context",
			.AnalyzedBaseOid  = core::git_commit_oid{ core::git_hash_algorithm::sha1, base },
			.AnalyzedHeadOid  = core::git_commit_oid{ core::git_hash_algorithm::sha1, head },
			.AnalyzedCommits  = { core::git_commit_oid{ core::git_hash_algorithm::sha1, only } },
		};
	}

	struct read_step
	{
		agent_protocol_pipe_read_status Status
			= agent_protocol_pipe_read_status::no_data;
		std::string Bytes;
	};

	class replay_transport final : public agent_protocol_transport
	{
	public:
		[[nodiscard]] auto write(const std::string_view)
			-> agent_protocol_write_status override
		{
			return agent_protocol_write_status::success;
		}

		void close_input() noexcept override
		{
		}

		[[nodiscard]] auto read_standard_output(const std::span<char> aBuffer)
			-> agent_protocol_pipe_read_result override
		{
			if (Output.empty()) {
				return { .Status = agent_protocol_pipe_read_status::closed };
			}
			const auto step = std::move(Output.front());
			Output.pop_front();
			if (step.Status != agent_protocol_pipe_read_status::data) {
				return { .Status = step.Status };
			}
			expect(
				step.Bytes.size() <= aBuffer.size(),
				"each replayed frame fits the caller-owned transport buffer");
			std::ranges::copy(step.Bytes, aBuffer.begin());
			return {
				.Status = agent_protocol_pipe_read_status::data,
				.Count  = step.Bytes.size(),
			};
		}

		[[nodiscard]] auto read_standard_error(const std::span<char>)
			-> agent_protocol_pipe_read_result override
		{
			return { .Status = agent_protocol_pipe_read_status::closed };
		}

		[[nodiscard]] auto wait() -> agent_protocol_wait_result override
		{
			return { .Status = agent_protocol_wait_status::exited };
		}

		[[nodiscard]] auto terminate() noexcept -> bool override
		{
			return true;
		}

		std::deque<read_step> Output;
	};

	/// \brief Replays one transcript a frame at a time, as a pipe would.
	[[nodiscard]] auto replay(
		const core::agent_protocol aProtocol, const std::string& aTranscript)
		-> application::agent_run_result
	{
		// One frame at a time, and never more than a pipe read can carry. Frame
		// boundaries are preserved deliberately: delivering a completed frame
		// together with the next one is the causal violation the App Server
		// course already refuses, and this replay is not the place to provoke
		// it.
		constexpr auto chunk     = std::size_t{ 8U * 1024U };
		auto           transport = replay_transport{ };
		auto           begin     = std::size_t{ 0U };
		while (begin < aTranscript.size()) {
			const auto newline = aTranscript.find('\n', begin);
			const auto end     = newline == std::string::npos ? aTranscript.size()
															  : newline + 1U;
			for (auto offset = begin; offset < end; offset += chunk) {
				transport.Output.push_back({
					.Status = agent_protocol_pipe_read_status::data,
					.Bytes  = aTranscript.substr(
						offset, std::min(chunk, end - offset)),
				});
			}
			begin = end;
		}
		transport.Output.push_back(
			{ .Status = agent_protocol_pipe_read_status::closed });
		const auto request = application::agent_launch_request{
			.Protocol        = aProtocol,
			.Session         = run_session,
			.Input           = "initial-input",
			.DeferredContext = aProtocol == core::agent_protocol::codex_app_server ? "selected-context" : "",
		};
		return run_agent_protocol_loop(request, { }, transport);
	}

	/// \brief The complete transcript each protocol carries one payload in.
	[[nodiscard]] auto transcript(
		const core::agent_protocol aProtocol, const std::string& aPayload)
		-> std::string
	{
		const auto member = aPayload.empty()
			? std::string{ }
			: ",\"bprFindings\":" + aPayload;
		switch (aProtocol) {
		case core::agent_protocol::codex_app_server:
			return "{\"id\":0,\"result\":{}}\n"
				   "{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread-7\"}}}\n"
				   "{\"id\":2,\"result\":{\"turn\":{\"id\":\"turn-8\","
				   "\"items\":[],\"status\":\"inProgress\"}}}\n"
				   "{\"method\":\"item/agentMessage/delta\",\"params\":{"
				   "\"threadId\":\"thread-7\",\"turnId\":\"turn-8\","
				   "\"itemId\":\"item-9\",\"delta\":\"reviewed\"}}\n"
				   "{\"method\":\"turn/completed\",\"params\":{"
				   "\"threadId\":\"thread-7\",\"turn\":{\"id\":\"turn-8\","
				   "\"items\":[],\"status\":\"completed\""
				+ member + "}}}\n";
		case core::agent_protocol::codex_jsonl_exec:
			return "{\"type\":\"thread.started\",\"thread_id\":\"thread-7\"}\n"
				   "{\"type\":\"turn.started\"}\n"
				   "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-9\","
				   "\"type\":\"agent_message\",\"text\":\"reviewed\"}}\n"
				   "{\"type\":\"turn.completed\",\"usage\":{"
				   "\"input_tokens\":1,\"cached_input_tokens\":0,"
				   "\"output_tokens\":1}"
				+ member + "}\n";
		case core::agent_protocol::claude_stream_json:
			return "{\"type\":\"system\",\"subtype\":\"init\","
				   "\"session_id\":\"session-7\"}\n"
				   "{\"type\":\"assistant\",\"session_id\":\"session-7\","
				   "\"message\":{\"role\":\"assistant\",\"content\":[{"
				   "\"type\":\"text\",\"text\":\"reviewed\"}]}}\n"
				   "{\"type\":\"result\",\"subtype\":\"success\","
				   "\"is_error\":false,\"result\":\"done\","
				   "\"session_id\":\"session-7\""
				+ member + "}\n";
		case core::agent_protocol::external_terminal:
			break;
		}
		return "BPR-FINDINGS/1 " + aPayload + "\n";
	}

	[[nodiscard]] auto supported_protocols()
		-> std::array<core::agent_protocol, 3U>
	{
		return {
			core::agent_protocol::codex_app_server,
			core::agent_protocol::codex_jsonl_exec,
			core::agent_protocol::claude_stream_json,
		};
	}

	void test_each_protocol_carries_the_payload_in_its_own_result_framing()
	{
		const auto payload = one_finding_payload(base_finding());
		for (const auto protocol : supported_protocols()) {
			const auto result = replay(protocol, transcript(protocol, payload));
			expect(
				result.Outcome == application::agent_run_outcome::success,
				"a payload-bearing terminal frame still completes its turn");
			expect(
				result.Findings.has_value()
					&& result.Findings->Session == run_session
					&& result.Findings->Findings.size() == 1U,
				"each supported protocol delivers exactly one payload");
			expect(
				result.Findings->Findings.front() == expected_finding(),
				"the decoded finding equals the wire object field for field");
		}

		const auto absent = replay(
			core::agent_protocol::codex_jsonl_exec,
			transcript(core::agent_protocol::codex_jsonl_exec, { }));
		expect(
			absent.Outcome == application::agent_run_outcome::success
				&& !absent.Findings.has_value(),
			"a turn that returned no payload returns no findings");

		const auto refused = replay(
			core::agent_protocol::external_terminal,
			transcript(core::agent_protocol::external_terminal, payload));
		expect(
			refused.Outcome == application::agent_run_outcome::protocol_failure
				&& refused.ProtocolFailure
					== application::agent_protocol_failure::
						unsupported_finding_transport
				&& !refused.Findings.has_value(),
			"external terminal has no carrier, so a payload is refused by name");
	}

	void test_one_completed_turn_returns_one_payload()
	{
		const auto payload = one_finding_payload(base_finding());
		const auto doubled
			= "{\"type\":\"thread.started\",\"thread_id\":\"thread-7\"}\n"
			  "{\"type\":\"turn.started\"}\n"
			  "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-9\","
			  "\"type\":\"agent_message\",\"text\":\"reviewed\"}}\n"
			  "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1},"
			  "\"bprFindings\":"
			+ payload
			+ "}\n"
			  "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1},"
			  "\"bprFindings\":"
			+ payload + "}\n";
		const auto result = replay(core::agent_protocol::codex_jsonl_exec, doubled);
		expect(
			result.Outcome == application::agent_run_outcome::protocol_failure
				&& !result.Findings.has_value(),
			"a second payload in one turn is refused rather than merged");
	}

	struct sweep_row
	{
		std::string_view     Name;
		std::string          Payload;
		core::agent_protocol Protocol
			= core::agent_protocol::codex_jsonl_exec;
		std::optional<application::agent_protocol_failure> Cause;
	};

	[[nodiscard]] auto finding_with(
		const std::string& aName, const std::string& aValue) -> std::string
	{
		auto finding = base_finding();
		put(finding, aName, aValue);
		return one_finding_payload(finding);
	}

	[[nodiscard]] auto finding_without(const std::string& aName) -> std::string
	{
		auto finding = base_finding();
		drop(finding, aName);
		return one_finding_payload(finding);
	}

	[[nodiscard]] auto text_of(const std::size_t aBytes, const char aFill)
		-> std::string
	{
		return quoted(std::string(aBytes, aFill));
	}

	[[nodiscard]] auto commits_of(const std::size_t aCount) -> std::string
	{
		auto result = std::string{ "[" };
		for (auto index = std::size_t{ 0U }; index < aCount; ++index) {
			if (index > 0U) {
				result += ',';
			}
			result += quoted(repeated(40U, '3'));
		}
		return result + "]";
	}

	/// \brief A payload of exactly the requested size, padded across findings.
	[[nodiscard]] auto payload_sized(const std::size_t aBytes) -> std::string
	{
		constexpr auto findings = std::size_t{ 4U };
		auto           empty    = base_finding();
		put(empty, "evidence", quoted(""));
		auto rendered = std::vector<std::string>{ };
		for (auto index = std::size_t{ 0U }; index < findings; ++index) {
			rendered.push_back(render(empty));
		}
		const auto bare = payload_of("41", rendered).size();
		expect(bare < aBytes, "the padding target must exceed the bare payload");
		const auto padding = aBytes - bare;
		rendered.clear();
		for (auto index = std::size_t{ 0U }; index < findings; ++index) {
			const auto share = padding / findings
				+ (index + 1U == findings ? padding % findings : 0U);
			auto padded = base_finding();
			put(padded, "evidence", text_of(share, 'e'));
			rendered.push_back(render(padded));
		}
		return payload_of("41", rendered);
	}

	[[nodiscard]] auto count_payload(const std::size_t aCount) -> std::string
	{
		auto rendered = std::vector<std::string>{ };
		for (auto index = std::size_t{ 0U }; index < aCount; ++index) {
			rendered.push_back(render(base_finding()));
		}
		return payload_of("41", rendered);
	}

	[[nodiscard]] auto bound_rows() -> std::vector<sweep_row>
	{
		using failure = application::agent_protocol_failure;
		auto rows     = std::vector<sweep_row>{
			{ .Name    = "payload at the byte bound",
			  .Payload = payload_sized(
				  application::maximum_agent_finding_payload_bytes) },
			{ .Name    = "payload one byte past the byte bound",
			  .Payload = payload_sized(
				  application::maximum_agent_finding_payload_bytes + 1U),
			  .Cause = failure::finding_payload_too_large },
			{ .Name    = "findings at the per-turn bound",
			  .Payload = count_payload(
				  application::maximum_agent_findings_per_turn) },
			{ .Name    = "one finding past the per-turn bound",
			  .Payload = count_payload(
				  application::maximum_agent_findings_per_turn + 1U),
			  .Cause = failure::too_many_findings },
		};
		const auto textBounds = std::array{
			std::pair{ std::string{ "path" }, core::maximum_identified_path_bytes },
			std::pair{ std::string{ "claim" }, core::maximum_finding_claim_bytes },
			std::pair{ std::string{ "evidence" },
					   core::maximum_finding_evidence_bytes },
			std::pair{ std::string{ "consequence" },
					   core::maximum_finding_consequence_bytes },
			std::pair{ std::string{ "suggestedComment" },
					   core::maximum_finding_comment_bytes },
			std::pair{ std::string{ "requiredContext" },
					   core::maximum_finding_required_context_bytes },
		};
		for (const auto& [name, bound] : textBounds) {
			rows.push_back({
				.Name    = "text member at its bound",
				.Payload = finding_with(name, text_of(bound, 'x')),
			});
			rows.push_back({
				.Name    = "text member one byte past its bound",
				.Payload = finding_with(name, text_of(bound + 1U, 'x')),
				.Cause   = failure::malformed_finding_payload,
			});
		}
		rows.push_back({
			.Name    = "analyzed commits at their bound",
			.Payload = finding_with(
				"analyzedCommits",
				commits_of(core::maximum_finding_analyzed_oids)),
		});
		rows.push_back({
			.Name    = "one analyzed commit past the bound",
			.Payload = finding_with(
				"analyzedCommits",
				commits_of(core::maximum_finding_analyzed_oids + 1U)),
			.Cause = failure::malformed_finding_payload,
		});
		return rows;
	}

	[[nodiscard]] auto malformation_rows() -> std::vector<sweep_row>
	{
		using failure     = application::agent_protocol_failure;
		const auto member = [](const std::string& aName, const std::string& aValue) {
			return finding_with(aName, aValue);
		};
		auto rows = std::vector<sweep_row>{
			{ .Name    = "payload is not an object",
			  .Payload = "[]",
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "payload session is absent",
			  .Payload = "{\"findings\":[]}",
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "payload session is not a number",
			  .Payload = "{\"session\":\"41\",\"findings\":[]}",
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "payload findings are absent",
			  .Payload = "{\"session\":41}",
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "payload findings are not an array",
			  .Payload = "{\"session\":41,\"findings\":{}}",
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "a finding is not an object",
			  .Payload = "{\"session\":41,\"findings\":[7]}",
			  .Cause   = failure::malformed_finding_payload },
			// These two break the frame that carries them, so the frame reader
			// refuses them before a payload exists to refuse. Kept in the sweep
			// with their real cause rather than dropped: where the boundary
			// falls is part of what the sweep is evidence for.
			{ .Name    = "a repeated member breaks the carrying frame",
			  .Payload = "{\"session\":41,\"session\":41,\"findings\":[]}",
			  .Cause   = failure::malformed_frame },
			{ .Name    = "a truncated payload breaks the carrying frame",
			  .Payload = "{\"session\":41,\"findings\":[",
			  .Cause   = failure::malformed_frame },
			{ .Name    = "claim is absent",
			  .Payload = finding_without("claim"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "anchor is absent",
			  .Payload = finding_without("anchor"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "lines are absent",
			  .Payload = finding_without("lines"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "analyzed commits are absent",
			  .Payload = finding_without("analyzedCommits"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "claim is not a string",
			  .Payload = member("claim", "12"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "a line ordinate is not a number",
			  .Payload = member("lines", "{\"begin\":\"10\",\"end\":12}"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "a line ordinate is fractional",
			  .Payload = member("lines", "{\"begin\":10.5,\"end\":12}"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "provider is negative",
			  .Payload = member("provider", "-1"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "severity is an enumerator nobody declared",
			  .Payload = member("severity", quoted("catastrophic")),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "confidence is an enumerator nobody declared",
			  .Payload = member("confidence", quoted("certain")),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "category is an enumerator nobody declared",
			  .Payload = member("category", quoted("vibes")),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "a side is neither left nor right",
			  .Payload = member(
				  "anchor",
				  "{\"line\":{\"comparison\":\"" + repeated(64U, '5')
					  + "\",\"side\":\"middle\",\"line\":11}}"),
			  .Cause = failure::malformed_finding_payload },
			{ .Name    = "an identity is one digit short",
			  .Payload = member(
				  "anchor",
				  "{\"line\":{\"comparison\":\"" + repeated(63U, '5')
					  + "\",\"side\":\"right\",\"line\":11}}"),
			  .Cause = failure::malformed_finding_payload },
			{ .Name    = "an identity is uppercase",
			  .Payload = member(
				  "anchor",
				  "{\"line\":{\"comparison\":\"" + repeated(64U, 'A')
					  + "\",\"side\":\"right\",\"line\":11}}"),
			  .Cause = failure::malformed_finding_payload },
			{ .Name    = "a refinement identity is malformed",
			  .Payload = member(
				  "anchor",
				  "{\"line\":{\"comparison\":\"" + repeated(64U, '5')
					  + "\",\"side\":\"right\",\"line\":11},\"segment\":\""
					  + repeated(10U, '5') + "\"}"),
			  .Cause = failure::malformed_finding_payload },
			{ .Name    = "a commit identifier is the wrong width",
			  .Payload = member("analyzedHead", quoted(repeated(39U, '2'))),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "a commit identifier is not hex",
			  .Payload = member("analyzedBase", quoted(repeated(40U, 'z'))),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "an analyzed commit is not a string",
			  .Payload = member("analyzedCommits", "[7]"),
			  .Cause   = failure::malformed_finding_payload },
			{ .Name    = "the payload names another session",
			  .Payload = payload_of("999", { render(base_finding()) }),
			  .Cause   = failure::unknown_finding_session },
			{ .Name     = "the protocol has no carrier",
			  .Payload  = one_finding_payload(base_finding()),
			  .Protocol = core::agent_protocol::external_terminal,
			  .Cause    = failure::unsupported_finding_transport },
		};
		return rows;
	}

	void test_every_bound_and_malformation_has_its_own_typed_cause()
	{
		auto rows = bound_rows();
		auto more = malformation_rows();
		rows.insert(
			rows.end(),
			std::make_move_iterator(more.begin()),
			std::make_move_iterator(more.end()));
		auto observed = std::vector<application::agent_protocol_failure>{ };
		for (const auto& row : rows) {
			const auto result = replay(
				row.Protocol, transcript(row.Protocol, row.Payload));
			if (!row.Cause.has_value()) {
				expect(
					result.Outcome == application::agent_run_outcome::success
						&& result.Findings.has_value(),
					std::string{ "an admitted payload is admitted: " }
						+ std::string{ row.Name });
				continue;
			}
			expect(
				result.Outcome == application::agent_run_outcome::protocol_failure
					&& result.ProtocolFailure == row.Cause
					&& !result.Findings.has_value(),
				std::string{ "the exact typed cause is reported: " }
					+ std::string{ row.Name });
			observed.push_back(*row.Cause);
		}

		using failure         = application::agent_protocol_failure;
		const auto structured = std::array{
			failure::finding_payload_too_large,
			failure::too_many_findings,
			failure::malformed_finding_payload,
			failure::unknown_finding_session,
			failure::unsupported_finding_transport,
		};
		for (const auto cause : structured) {
			expect(
				std::ranges::find(observed, cause) != observed.end(),
				"every structured-finding cause is exercised by the sweep");
		}
		auto values = std::vector<std::uint8_t>{ };
		for (const auto cause : structured) {
			values.push_back(static_cast<std::uint8_t>(cause));
		}
		for (const auto existing : {
				 failure::malformed_frame,
				 failure::incomplete_frame,
				 failure::frame_too_large,
				 failure::output_too_large,
				 failure::remote_failed,
				 failure::remote_interrupted,
				 failure::causal_violation,
				 failure::invalid_runtime_metadata,
				 failure::input_too_large,
				 failure::not_signed_in,
			 }) {
			values.push_back(static_cast<std::uint8_t>(existing));
		}
		std::ranges::sort(values);
		expect(
			std::ranges::adjacent_find(values) == values.end()
				&& values.size() == application::agent_protocol_failure_count,
			"collapsing any two protocol causes onto one value fails here");
	}

#if defined(BPR_AGENT_CANARY_EXECUTABLE)
	[[nodiscard]] auto canary_output(const std::vector<std::string>& aArguments)
		-> std::string
	{
		auto command = std::string{ "\"" } + BPR_AGENT_CANARY_EXECUTABLE
			+ "\" --findings-transcript --findings-session 41";
		for (const auto& argument : aArguments) {
			command += ' ' + argument;
		}
	#if defined(_WIN32)
		auto* const pipe = _popen(command.c_str(), "r");
	#else
		// The test's whole job is to drive the built canary through a pipe;
		// the command is the quoted path it computed above, never input.
		// NOLINTNEXTLINE(bugprone-command-processor)
		auto* const pipe = ::popen(command.c_str(), "r");
	#endif
		expect(pipe != nullptr, "the canary fixture starts");
		auto output = std::string{ };
		auto buffer = std::array<char, 4096U>{ };
		while (
			std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)
			!= nullptr) {
			output += buffer.data();
		}
	#if defined(_WIN32)
		const auto status = _pclose(pipe);
	#else
		const auto status = ::pclose(pipe);
	#endif
		expect(status == 0, "the canary fixture exits cleanly");
		return output;
	}
#endif

	void test_the_canary_returns_a_payload_on_each_supported_protocol()
	{
#if !defined(BPR_AGENT_CANARY_EXECUTABLE)
		std::cout << "agent-finding-protocol-tests:canary-unavailable\n";
		return;
#else
		const auto arguments = std::array{
			std::vector<std::string>{ "app-server" },
			std::vector<std::string>{ "exec", "--json", "-" },
			std::vector<std::string>{
				"-p",
				"--input-format",
				"stream-json",
				"--output-format",
				"stream-json",
				"--verbose",
			},
		};
		const auto protocols = supported_protocols();
		for (auto index = std::size_t{ 0U }; index < protocols.size(); ++index) {
			const auto emitted = canary_output(arguments[index]);
			expect(
				emitted.find("bprFindings") != std::string::npos,
				"the canary writes the payload inside its own result framing");
			const auto result = replay(protocols[index], emitted);
			expect(
				result.Outcome == application::agent_run_outcome::success
					&& result.Findings.has_value()
					&& result.Findings->Findings.size() == 1U,
				"the host decodes the real child's payload exactly once");
			expect(
				result.Findings->Session == run_session
					&& result.Findings->Findings.front() == expected_finding(),
				"the real child's finding decodes field for field");
		}
		const auto external = canary_output({ });
		expect(
			external.starts_with("BPR-FINDINGS/1"),
			"the canary still announces a payload where none can ride");
		const auto refused = replay(
			core::agent_protocol::external_terminal, external);
		expect(
			refused.Outcome == application::agent_run_outcome::protocol_failure
				&& refused.ProtocolFailure
					== application::agent_protocol_failure::
						unsupported_finding_transport,
			"the unsupported transport refuses the real child by name");
#endif
	}
} // namespace

auto main() -> int
{
	field_shape_guard();
	test_each_protocol_carries_the_payload_in_its_own_result_framing();
	test_one_completed_turn_returns_one_payload();
	test_every_bound_and_malformation_has_its_own_typed_cause();
	test_the_canary_returns_a_payload_on_each_supported_protocol();
	std::cout << "agent-finding-protocol-tests:pass\n";
	return EXIT_SUCCESS;
}
