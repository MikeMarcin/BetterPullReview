/// \file
/// \brief Proves exact context adaptation, framing, and typed host outcomes.

#include "../core/private_review_fixture.hpp"
#include "bpr/application/agent_launch.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 40U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = {
				.Host        = core::git_host_id{ 10U },
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1U },
				.RightEpoch  = core::epoch_ordinal{ 2U },
			},
		};
	}

	[[nodiscard]] auto profile(const core::agent_protocol aProtocol)
		-> core::agent_profile
	{
		return {
			.Id          = core::agent_profile_id{ 1U },
			.DisplayName = "Canary",
			.Executable  = "/opt/agents/canary",
			.Protocol    = aProtocol,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = { "HOME", "PATH" },
		};
	}

	[[nodiscard]] auto observation() -> application::agent_context_observation
	{
		const auto comparison = identity<core::file_comparison_digest>(5U);
		const auto anchor     = core::source_line_ref{
			.Comparison = comparison,
			.Side       = core::diff_side::right,
			.Line       = 17U,
		};
		return {
			.ApplicationSession = core::application_session_id{ 1U },
			.Binding            = session().Binding,
			.PullRequestUrl     = "https://github.example/octo/app/pull/30",
			.PreviousUpload     = commit(3U),
			.Files              = {
				{
					.Path    = "src/retry.cpp",
					.Receipt = {
						.File     = identity<core::file_identity>(3U),
						.Material = identity<core::file_material_id>(4U),
						.Read     = core::file_read_state::read,
						.Accepted = true,
					},
					.ChangedSinceReview = true,
				},
			},
			.Private = {
				.Notes = {
					{
						.Id   = application::local_note_id{ 1U },
						.Note = { .Text = "not selected" },
					},
					{
						.Id         = application::local_note_id{ 2U },
						.Attachment = application::local_note_attachment{
							.File       = identity<core::file_identity>(3U),
							.Comparison = comparison,
						},
						.Note = { .Text = "check retry exhaustion" },
					},
				},
				.LastReviewedHead = commit(4U),
				.Defects          = {
					{
						.File         = identity<core::file_identity>(3U),
						.Conversation = core::review_conversation_id{ 70U },
						.History      = {
							.Defect = {
								.Id       = core::defect_id{ 80U },
								.Anchor   = anchor,
								.Severity = core::defect_severity::critical,
								.Category = core::defect_category::security,
								.State    = core::defect_state::verified,
							},
							.OriginalAnchor = anchor,
						},
					},
				},
			},
			.Selection = core::supplement_semantic_context{
				.File      = identity<core::file_identity>(3U),
				.Selection = {
					.Comparison    = comparison,
					.PreferredLine = anchor,
				},
				.Confidence         = core::supplement_mapping_confidence::exact,
				.SelectedSourceText = std::string{ "return retry();" },
			},
			.CurrentDraft = application::agent_draft_observation{
				.Choice = { .Id = core::review_draft_id{ 3U }, .Revision = 7U },
				.Draft  = {
					.Id     = core::review_draft_id{ 3U },
					.Anchor = anchor,
					.Body   = "Would a bounded retry be safer?",
				},
			},
			.IncludedNotes = { application::local_note_id{ 2U } },
			.IncludedDraft = application::agent_draft_choice{ .Id = core::review_draft_id{ 3U }, .Revision = 7U },
		};
	}

	[[nodiscard]] auto built_supplement()
		-> core::agent_context_supplement
	{
		const auto built
			= application::build_agent_context_supplement(observation(), session());
		expect(built.has_value(), "the complete context observation builds");
		return *built;
	}

	[[nodiscard]] auto parent_environment() -> std::map<std::string, std::string>
	{
		return {
			{ "HOME", "/Users/reviewer" },
			{ "PATH", "/usr/bin:/bin" },
			{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
			{ "AWS_SECRET_ACCESS_KEY", "aws_secret_value" },
			{ "ANTHROPIC_API_KEY", "sk-ant-secret" },
		};
	}

	[[nodiscard]] auto reader() -> application::environment_reader
	{
		return [](const std::string& aName) -> std::optional<std::string> {
			const auto parent = parent_environment();
			const auto found  = parent.find(aName);
			return found == parent.end() ? std::nullopt
										 : std::optional{ found->second };
		};
	}

	void test_private_ledger_observation_is_an_input_not_an_implicit_store()
	{
		const auto built
			= application::build_agent_context_supplement(observation(), session());
		expect(built.has_value(), "the bounded supplied observation builds");
		expect(
			built->LastReviewedHead == commit(4U) && built->Defects.size() == 1U
				&& built->Defects.front().History.Defect.Id == core::defect_id{ 80U },
			"last-reviewed head and exact immutable defect history come from the supplied BPR-0238 observation");
		expect(
			built->Notes.size() == 1U
				&& built->Notes.front().Text == "check retry exhaustion",
			"only the explicitly named private note crosses");

		auto missing          = observation();
		missing.IncludedNotes = { application::local_note_id{ 99U } };
		const auto missingResult
			= application::build_agent_context_supplement(missing, session());
		expect(
			!missingResult
				&& missingResult.error()
					== application::agent_context_failure::selected_note_missing,
			"an unknown selected note is refused rather than silently omitted");

		auto duplicate = observation();
		duplicate.IncludedNotes.emplace_back(2U);
		const auto duplicateResult
			= application::build_agent_context_supplement(duplicate, session());
		expect(
			!duplicateResult
				&& duplicateResult.error()
					== application::agent_context_failure::duplicate_selected_note,
			"a duplicated selection is refused rather than duplicated on the wire");

		auto duplicateSource = observation();
		duplicateSource.Private.Notes.push_back(
			duplicateSource.Private.Notes.back());
		const auto duplicateSourceResult
			= application::build_agent_context_supplement(duplicateSource, session());
		expect(
			!duplicateSourceResult
				&& duplicateSourceResult.error()
					== application::agent_context_failure::invalid_observation,
			"a supplied private observation with ambiguous note identity is refused");

		auto stale = observation();
		++stale.IncludedDraft->Revision;
		const auto staleResult
			= application::build_agent_context_supplement(stale, session());
		expect(
			!staleResult
				&& staleResult.error()
					== application::agent_context_failure::selected_draft_stale,
			"a draft edited after its choice is refused rather than retargeted");

		auto invalid = observation();
		invalid.CurrentDraft->Draft.Body.clear();
		const auto invalidResult
			= application::build_agent_context_supplement(invalid, session());
		expect(
			!invalidResult
				&& invalidResult.error()
					== application::agent_context_failure::selected_draft_invalid,
			"an empty selected draft is refused before session persistence");
	}

	/// \brief The asked-for operation crosses only under the workshop press.
	///
	/// \details BPR-0533. The entry point is what admits the operation, so this
	/// is the one seam where "a workshop request" and "a launch that happens to
	/// carry an operation" are told apart. A press carries its exact operation
	/// into the supplement the agent is framed from; the same operation named
	/// under any other gesture is refused whole rather than dropped, because a
	/// supplement built with the field cleared would ask the agent to revise a
	/// comment without saying how.
	void test_the_workshop_operation_crosses_only_under_its_entry_point()
	{
		auto asked              = observation();
		asked.EntryPoint        = core::agent_entry_point::workshop_comment;
		asked.WorkshopOperation = core::workshop_operation::phrase_as_a_question;
		const auto pressed
			= application::build_agent_context_supplement(asked, session());
		expect(
			pressed.has_value()
				&& pressed->Operation
					== core::workshop_operation::phrase_as_a_question,
			"a workshop press carries its exact operation into the supplement");

		const auto plain
			= application::build_agent_context_supplement(observation(), session());
		expect(
			plain.has_value() && !plain->Operation.has_value(),
			"every launch that is not a workshop press carries no operation");

		auto smuggled              = observation();
		smuggled.WorkshopOperation = core::workshop_operation::shorten;
		const auto smuggledResult
			= application::build_agent_context_supplement(smuggled, session());
		expect(
			!smuggledResult
				&& smuggledResult.error()
					== application::agent_context_failure::invalid_observation,
			"an operation riding on a gesture that is not a workshop press is refused whole");

		auto undeclared              = observation();
		undeclared.EntryPoint        = core::agent_entry_point::workshop_comment;
		undeclared.WorkshopOperation = static_cast<core::workshop_operation>(
			core::workshop_operation_count);
		const auto undeclaredResult
			= application::build_agent_context_supplement(undeclared, session());
		expect(
			!undeclaredResult
				&& undeclaredResult.error()
					== application::agent_context_failure::invalid_observation,
			"an operation nobody declared is refused before it reaches the wire");
	}

	void test_the_complete_encoding_preserves_presence_and_exact_identity()
	{
		const auto encoded = application::encode_agent_supplement(built_supplement());
		for (const auto required : {
				 "schema\t2\n",
				 "session\t40\n",
				 "profile\t1\n",
				 "host\t10\n",
				 "repository\t20\n",
				 "pull_request\t30\n",
				 "pull_request_url\thttps://github.example/octo/app/pull/30\n",
				 "left_epoch\t1\n",
				 "right_epoch\t2\n",
				 "previous_upload.present\t1\n",
				 "last_reviewed_head.present\t1\n",
				 "file.count\t1\n",
				 "file.0.path\tsrc/retry.cpp\n",
				 "file.0.read\t2\n",
				 "file.0.changed_since_review\t1\n",
				 "file.0.accepted\t1\n",
				 "defect.count\t1\n",
				 "defect.0.conversation\t70\n",
				 "defect.0.id\t80\n",
				 "selection.present\t1\n",
				 "selection.mapping_confidence\t0\n",
				 "selection.source_text.present\t1\n",
				 "selection.source_text.value\treturn retry();\n",
				 "note.count\t1\n",
				 "note.0.text\tcheck retry exhaustion\n",
				 "draft.count\t1\n",
				 "draft.0.text\tWould a bounded retry be safer?\n",
			 }) {
			expect(
				encoded.find(required) != std::string::npos,
				"the exact required context field is encoded");
		}

		auto absent = built_supplement();
		absent.PreviousUpload.reset();
		absent.Selection->SelectedSourceText.reset();
		const auto absentEncoded = application::encode_agent_supplement(absent);
		expect(
			absentEncoded.find("previous_upload.present\t0\n")
					!= std::string::npos
				&& absentEncoded.find("selection.source_text.present\t0\n")
					!= std::string::npos,
			"absent revisions and source text are explicit on the wire");
		absent.Selection->SelectedSourceText = std::string{ };
		const auto emptyEncoded              = application::encode_agent_supplement(absent);
		expect(
			emptyEncoded.find("selection.source_text.present\t1\n")
					!= std::string::npos
				&& emptyEncoded.find("selection.source_text.value\t\n")
					!= std::string::npos,
			"present empty source text does not collapse into absence");
	}

	void test_each_protocol_has_exact_argv_and_distinct_input_framing()
	{
		const auto protocols = std::array{
			core::agent_protocol::codex_app_server,
			core::agent_protocol::codex_jsonl_exec,
			core::agent_protocol::claude_stream_json,
			core::agent_protocol::external_terminal,
		};
		const auto expectedArguments = std::array{
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
			std::vector<std::string>{ },
		};
		const auto inputPrefixes = std::array{
			std::string_view{ "{\"method\":\"initialize\"" },
			std::string_view{ "BPR-CONTEXT/2 CODEX-EXEC\n" },
			std::string_view{ "{\"type\":\"user\"" },
			std::string_view{ "BPR-CONTEXT/2 EXTERNAL-TERMINAL\n" },
		};
		for (auto index = std::size_t{ 0U }; index < protocols.size(); ++index) {
			const auto launched = application::build_agent_launch(
				profile(protocols[index]), built_supplement(), reader());
			expect(launched.has_value(), "the matching profile and session launch");
			expect(
				launched->Arguments == expectedArguments[index]
					&& launched->Input.starts_with(inputPrefixes[index])
					&& (launched->DeferredContext.empty()
						== (protocols[index]
							!= core::agent_protocol::codex_app_server)),
				"each protocol has its exact argv and distinct stdin framing");
			for (const auto& argument : launched->Arguments) {
				expect(
					argument.find("check retry exhaustion") == std::string::npos
						&& argument.find("github.example") == std::string::npos,
					"no context field reaches process-list-visible argv");
			}
		}

		const auto appServer = application::build_agent_launch(
			profile(core::agent_protocol::codex_app_server),
			built_supplement(),
			reader());
		expect(appServer.has_value(), "the App Server request builds");
		const auto initialized = application::build_agent_protocol_followup(
			*appServer,
			{
				.Kind
				= application::agent_protocol_event_kind::initialization_complete,
			});
		const auto turn = application::build_agent_protocol_followup(
			*appServer,
			{
				.Kind = application::agent_protocol_event_kind::context_channel_ready,
				.Text = "thread-7",
			});
		expect(
			initialized
				&& *initialized
					== "{\"method\":\"initialized\"}\n"
					   "{\"method\":\"thread/start\",\"id\":1,\"params\":{}}\n"
				&& turn && turn->find("\"threadId\":\"thread-7\"") != std::string::npos
				&& turn->find("schema\\t2\\n") != std::string::npos,
			"App Server context waits for its initialized thread and then travels in a turn input");
	}

	void test_no_credential_crosses_the_environment_boundary()
	{
		const auto launched = application::build_agent_launch(
			profile(core::agent_protocol::codex_jsonl_exec),
			built_supplement(),
			reader());
		expect(launched.has_value(), "the launch request builds");
		auto names = std::vector<std::string>{ };
		for (const auto& [name, value] : launched->Environment) {
			static_cast<void>(value);
			names.push_back(name);
		}
		std::ranges::sort(names);
		expect(
			names == std::vector<std::string>{ "HOME", "PATH" },
			"the complete child environment equals the allowlist");
		const auto parent = parent_environment();
		for (const auto secret :
			 { "GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY" }) {
			expect(
				parent.contains(secret)
					&& std::ranges::find(names, secret) == names.end(),
				"a planted parent credential is absent from the child request");
		}

		auto hostile               = profile(core::agent_protocol::codex_jsonl_exec);
		hostile.AllowedEnvironment = {
			"HOME",
			"github_token",
			"ANTHROPIC_API_KEY",
			"aws_secret_access_key",
			"GIT_ASKPASS",
			"git_config_value_0",
		};
		const auto filtered = application::materialize_agent_environment(
			hostile,
			[](const std::string& aName) -> std::optional<std::string> {
				return aName == "HOME" ? std::optional{ std::string{ "/safe" } }
									   : std::optional{ std::string{ "secret" } };
			});
		expect(
			filtered.empty(),
			"direct environment materialization refuses an invalid profile whole");
		const auto refused = application::build_agent_launch(
			hostile, built_supplement(), reader());
		expect(
			!refused
				&& refused.error()
					== application::agent_launch_failure::invalid_profile,
			"launch construction refuses a profile that names any credential alias");
	}

	void test_launch_refuses_inexact_profile_session_and_schema()
	{
		auto invalidProfile     = profile(core::agent_protocol::codex_jsonl_exec);
		invalidProfile.Protocol = static_cast<core::agent_protocol>(255U);
		const auto invalid      = application::build_agent_launch(
			invalidProfile, built_supplement(), reader());
		expect(
			!invalid
				&& invalid.error()
					== application::agent_launch_failure::invalid_profile,
			"an unknown process protocol is refused before launch framing");

		auto wrongProfile     = profile(core::agent_protocol::codex_jsonl_exec);
		wrongProfile.Id       = core::agent_profile_id{ 9U };
		const auto mismatched = application::build_agent_launch(
			wrongProfile, built_supplement(), reader());
		expect(
			!mismatched
				&& mismatched.error()
					== application::agent_launch_failure::profile_session_mismatch,
			"a profile cannot launch a supplement pinned to another profile");

		auto wrongSchema          = built_supplement();
		wrongSchema.SchemaVersion = 99U;
		const auto unsupported    = application::build_agent_launch(
			profile(core::agent_protocol::codex_jsonl_exec),
			wrongSchema,
			reader());
		expect(
			!unsupported
				&& unsupported.error()
					== application::agent_launch_failure::unsupported_supplement_schema,
			"an unsupported context schema is refused rather than guessed");
	}

#if defined(__GNUC__) && !defined(__clang__)
	// GCC 15's -Wdangling-pointer reports the braced event lists below as
	// dangling at the function's end. Every event owns its strings; nothing
	// outlives its list.
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
	void test_every_protocol_decoder_handles_partial_frames()
	{
		auto escapedText = std::string{ "one/two \" " };
		escapedText += "\xE2\x98\xBA \xF0\x9F\x98\x80";
		const auto protocols = std::array{
			core::agent_protocol::codex_app_server,
			core::agent_protocol::codex_jsonl_exec,
			core::agent_protocol::claude_stream_json,
			core::agent_protocol::external_terminal,
		};
		const auto frames = std::array{
			std::string{
				"{\"id\":0,\"result\":{}}\n"
				"{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread-7\"}}}\n"
				"{\"id\":2,\"result\":{\"turn\":{\"id\":\"turn-8\","
				"\"items\":[],\"status\":\"inProgress\"}}}\n"
				"{\"method\":\"item/agentMessage/delta\",\"params\":{"
				"\"threadId\":\"thread-7\",\"turnId\":\"turn-8\","
				"\"itemId\":\"item-9\","
				"\"delta\":\"one\\/two \\\" \\u263A \\uD83D\\uDE00\"}}\n"
				"{\"method\":\"turn/completed\",\"params\":{"
				"\"threadId\":\"thread-7\",\"turn\":{\"id\":\"turn-8\","
				"\"items\":[],\"status\":\"completed\"}}}\n" },
			std::string{
				"{\"type\":\"thread.started\",\"thread_id\":\"thread-7\"}\n"
				"{\"type\":\"turn.started\"}\n"
				"{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\","
				"\"type\":\"agent_message\","
				"\"text\":\"one\\/two \\\" \\u263A \\uD83D\\uDE00\"}}\n"
				"{\"type\":\"turn.completed\",\"usage\":{}}\n" },
			std::string{
				"{\"type\":\"system\",\"subtype\":\"init\","
				"\"session_id\":\"session-6\"}\n"
				"{\"type\":\"assistant\",\"session_id\":\"session-6\","
				"\"message\":{\"role\":\"assistant\",\"content\":[{"
				"\"type\":\"text\","
				"\"text\":\"one\\/two \\\" \\u263A \\uD83D\\uDE00\"}]}}\n"
				"{\"type\":\"result\",\"subtype\":\"success\","
				"\"is_error\":false,\"result\":\"done\","
				"\"session_id\":\"session-6\"}\n" },
			std::string{ "one\ntwo" },
		};
		for (auto index = std::size_t{ 0U }; index < protocols.size(); ++index) {
			auto decoder  = application::agent_protocol_decoder{ protocols[index] };
			auto messages = std::vector<application::agent_protocol_event>{ };
			for (const auto character : frames[index]) {
				const auto decoded = decoder.push(std::string_view{ &character, 1U });
				expect(decoded.has_value(), "each one-byte partial frame is accepted");
				if (protocols[index] == core::agent_protocol::codex_app_server) {
					for (const auto& event : *decoded) {
						if (event.Kind != application::agent_protocol_event_kind::output) {
							expect(
								decoder.acknowledge_followup(event.Kind).has_value(),
								"the next App Server response is enabled only after its follow-up write");
						}
					}
				}
				messages.insert(messages.end(), decoded->begin(), decoded->end());
			}
			const auto trailing = decoder.finish();
			expect(trailing.has_value(), "the complete partial stream finishes");
			messages.insert(messages.end(), trailing->begin(), trailing->end());
			if (protocols[index] == core::agent_protocol::codex_app_server) {
				expect(
					messages
						== std::vector<application::agent_protocol_event>{
							{
								.Kind = application::agent_protocol_event_kind::initialization_complete,
							},
							{
								.Kind = application::agent_protocol_event_kind::context_channel_ready,
								.Text = "thread-7",
							},
							{
								.Kind = application::agent_protocol_event_kind::output,
								.Text = escapedText,
							},
						},
					"App Server control and output frames remain ordered");
				continue;
			}
			const auto expected = protocols[index]
					== core::agent_protocol::external_terminal
				? std::vector<application::agent_protocol_event>{
					  { .Text = "one" },
					  { .Text = "two" },
				  }
				: std::vector<application::agent_protocol_event>{
					  { .Text = escapedText },
				  };
			expect(
				messages == expected,
				"messages are emitted only after complete protocol-specific frames");
		}

		auto preemptive = application::agent_protocol_decoder{
			core::agent_protocol::codex_app_server
		};
		const auto preemptiveResult = preemptive.push(
			"{\"id\":0,\"result\":{}}\n"
			"{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread\"}}}\n"
			"{\"id\":2,\"result\":{\"turn\":{\"id\":\"turn\","
			"\"items\":[],\"status\":\"inProgress\"}}}\n");
		expect(
			!preemptiveResult
				&& preemptiveResult.error()
					== application::agent_protocol_failure::causal_violation,
			"batched App Server responses cannot outrun host follow-up writes");

		auto malformed
			= application::agent_protocol_decoder{ core::agent_protocol::codex_jsonl_exec };
		expect(
			malformed.push(
						 "{\"type\":\"thread.started\",\"thread_id\":\"thread\"}\n"
						 "{\"type\":\"turn.started\"}\n")
					.has_value()
				&& !malformed.push(
					"{\"type\":\"item.completed\" \"item\":{}}\n"),
			"malformed JSON is refused rather than accepted by substring shape");

		auto duplicateKey
			= application::agent_protocol_decoder{ core::agent_protocol::codex_jsonl_exec };
		expect(
			!duplicateKey.push(
				"{\"type\":\"thread.started\",\"type\":\"turn.started\","
				"\"thread_id\":\"thread\"}\n"),
			"duplicate JSON keys are refused rather than resolved ambiguously");

		auto invalidUnicode
			= application::agent_protocol_decoder{ core::agent_protocol::codex_jsonl_exec };
		expect(
			!invalidUnicode.push(
				"{\"type\":\"thread.started\",\"thread_id\":\"\\uD800\"}\n"),
			"an unpaired Unicode surrogate is malformed JSON");

		for (const auto& invalid : {
				 std::string{ "\x80", 1U },
				 std::string{ "\xC0\xAF", 2U },
				 std::string{ "\xED\xA0\x80", 3U },
				 std::string{ "\xF4\x90\x80\x80", 4U },
				 std::string{ "\xE2\x82", 2U },
			 }) {
			auto rawFrame
				= std::string{ "{\"type\":\"thread.started\",\"thread_id\":\"" };
			rawFrame += invalid;
			rawFrame += "\"}\n";
			auto rawDecoder = application::agent_protocol_decoder{
				core::agent_protocol::codex_jsonl_exec
			};
			expect(
				!rawDecoder.push(rawFrame),
				"lone, overlong, surrogate, out-of-range, and truncated raw UTF-8 are refused");
		}

		auto rawUnicode = application::agent_protocol_decoder{
			core::agent_protocol::codex_jsonl_exec
		};
		expect(
			rawUnicode
				.push(
					"{\"type\":\"thread.started\",\"thread_id\":\"thread\"}\n"
					"{\"type\":\"turn.started\"}\n")
				.has_value(),
			"the raw-Unicode probe enters the Codex output phase");
		auto rawText = std::string{ "raw " };
		rawText += "\xE2\x98\xBA \xF0\x9F\x98\x80";
		auto rawMessage
			= std::string{ "{\"type\":\"item.completed\",\"item\":{"
						   "\"id\":\"item\",\"type\":\"agent_message\",\"text\":\"" };
		rawMessage += rawText;
		rawMessage += "\"}}\n";
		const auto rawDecoded = rawUnicode.push(rawMessage);
		expect(
			rawDecoded
				&& *rawDecoded
					== std::vector<application::agent_protocol_event>{ {
						.Text = rawText,
					} },
			"valid raw BMP and supplementary Unicode survive decoding exactly");
		expect(
			rawUnicode
					.push("{\"type\":\"turn.completed\",\"usage\":{}}\n")
					.has_value()
				&& rawUnicode.finish().has_value(),
			"valid raw Unicode does not disturb terminal validation");

		auto incomplete = application::agent_protocol_decoder{
			core::agent_protocol::claude_stream_json
		};
		expect(
			incomplete.push(
						  "{\"type\":\"system\",\"subtype\":\"init\","
						  "\"session_id\":\"session\"}\n"
						  "{\"type\":\"assistant\",\"session_id\":\"session\","
						  "\"message\":{\"role\":\"assistant\",\"content\":[]}}\n")
					.has_value()
				&& !incomplete.finish(),
			"Claude output without its terminal result is a protocol failure");

		auto claudeFailure = application::agent_protocol_decoder{
			core::agent_protocol::claude_stream_json
		};
		expect(
			claudeFailure
				.push(
					"{\"type\":\"system\",\"subtype\":\"init\","
					"\"session_id\":\"session\"}\n"
					"{\"type\":\"assistant\",\"session_id\":\"session\","
					"\"message\":{\"role\":\"assistant\",\"content\":[]}}\n")
				.has_value(),
			"Claude accepts its documented initialization and assistant sequence");
		const auto claudeError = claudeFailure.push(
			"{\"type\":\"result\",\"subtype\":\"error_during_execution\","
			"\"is_error\":true,\"session_id\":\"session\"}\n");
		expect(
			!claudeError
				&& claudeError.error()
					== application::agent_protocol_failure::remote_failed,
			"Claude's documented error result remains an exact typed failure");

		const auto advanceAppToTurn = [](application::agent_protocol_decoder& aApp) {
			const auto initialized = aApp.push("{\"id\":0,\"result\":{}}\n");
			expect(
				initialized && initialized->size() == 1U
					&& aApp
						   .acknowledge_followup(initialized->front().Kind)
						   .has_value(),
				"App Server initialization is followed before thread response");
			const auto thread = aApp.push(
				"{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread\"}}}\n");
			expect(
				thread && thread->size() == 1U
					&& aApp.acknowledge_followup(thread->front().Kind).has_value(),
				"App Server thread start is followed before turn response");
			expect(
				aApp.push(
						"{\"id\":2,\"result\":{\"turn\":{\"id\":\"turn\","
						"\"items\":[],\"status\":\"inProgress\"}}}\n")
					.has_value(),
				"App Server consumes id-2 only after the turn-start write");
		};

		for (const auto& [status, failure] : {
				 std::pair{
					 std::string_view{ "failed" },
					 application::agent_protocol_failure::remote_failed },
				 std::pair{
					 std::string_view{ "interrupted" },
					 application::agent_protocol_failure::remote_interrupted },
			 }) {
			auto app = application::agent_protocol_decoder{
				core::agent_protocol::codex_app_server
			};
			advanceAppToTurn(app);
			const auto terminal = app.push(
				"{\"method\":\"turn/completed\",\"params\":{"
				"\"threadId\":\"thread\",\"turn\":{\"id\":\"turn\","
				"\"items\":[],\"status\":\""
				+ std::string{ status } + "\"}}}\n");
			expect(
				!terminal && terminal.error() == failure,
				"App Server failed and interrupted statuses stay distinct");
		}

		auto missingTurnResponse = application::agent_protocol_decoder{
			core::agent_protocol::codex_app_server
		};
		const auto initialized
			= missingTurnResponse.push("{\"id\":0,\"result\":{}}\n");
		expect(
			initialized && initialized->size() == 1U
				&& missingTurnResponse
					   .acknowledge_followup(initialized->front().Kind)
					   .has_value(),
			"the missing-turn-response probe completes initialization causally");
		const auto thread = missingTurnResponse.push(
			"{\"id\":1,\"result\":{\"thread\":{\"id\":\"thread\"}}}\n");
		expect(
			thread && thread->size() == 1U
				&& missingTurnResponse
					   .acknowledge_followup(thread->front().Kind)
					   .has_value()
				&& !missingTurnResponse.push(
					"{\"method\":\"turn/completed\",\"params\":{"
					"\"threadId\":\"thread\",\"turn\":{\"id\":\"turn\","
					"\"items\":[],\"status\":\"completed\"}}}\n"),
			"App Server cannot skip the response to BetterPullReview's id-2 request");

		auto oversized = application::agent_protocol_decoder{
			core::agent_protocol::external_terminal
		};
		const auto refusedFrame = oversized.push(
			std::string(application::maximum_agent_frame_bytes + 1U, 'x'));
		expect(
			!refusedFrame
				&& refusedFrame.error()
					== application::agent_protocol_failure::frame_too_large,
			"a frame beyond the fixed bound is refused before further buffering");

		// The total ceiling and the per-frame ceiling are separate limits, so
		// this flood keeps every frame inside the frame bound and can only be
		// refused by the total.
		auto flooded = application::agent_protocol_decoder{
			core::agent_protocol::external_terminal
		};
		const auto line = std::string(
							  application::maximum_agent_frame_bytes / 2U, 'o')
			+ "\n";
		auto flooding = flooded.push(line);
		for (auto written = line.size();
			 flooding
			 && written < application::maximum_agent_stdout_bytes + line.size();
			 written += line.size()) {
			flooding = flooded.push(line);
		}
		expect(
			!flooding
				&& flooding.error()
					== application::agent_protocol_failure::output_too_large,
			"output past the total ceiling is refused as its own exact cause");

		// A refused decoder answers every later call with the generic cause,
		// including `finish`. The host calls `finish` after its last drain, so
		// a host that treated this answer as new information would restate an
		// exact cause it already held. This is the premise that requires the
		// launcher to keep the first cause it recorded.
		auto restating = application::agent_protocol_decoder{
			core::agent_protocol::external_terminal
		};
		const auto exactCause = restating.push(
			std::string(application::maximum_agent_frame_bytes + 1U, 'x'));
		const auto restatedCause = restating.finish();
		expect(
			!exactCause
				&& exactCause.error()
					== application::agent_protocol_failure::frame_too_large
				&& !restatedCause
				&& restatedCause.error()
					== application::agent_protocol_failure::malformed_frame,
			"a refused decoder restates the generic cause rather than the exact one");
	}

#if defined(__GNUC__) && !defined(__clang__)
	#pragma GCC diagnostic pop
#endif

	void test_process_and_staleness_outcomes_remain_distinct()
	{
		const auto outcomes = std::array{
			application::agent_run_outcome::success,
			application::agent_run_outcome::failed_to_start,
			application::agent_run_outcome::workspace_unavailable,
			application::agent_run_outcome::nonzero_exit,
			application::agent_run_outcome::protocol_failure,
			application::agent_run_outcome::cancelled,
			application::agent_run_outcome::timeout,
			application::agent_run_outcome::stale_result,
		};
		auto names = std::vector<std::string_view>{ };
		for (const auto outcome : outcomes) {
			names.push_back(application::name(outcome));
		}
		std::ranges::sort(names);
		expect(
			std::ranges::adjacent_find(names) == names.end(),
			"every terminal condition has a distinct stable typed name");

		auto success = application::agent_run_result{
			.Session = session().Id,
			.Outcome = application::agent_run_outcome::success,
			.Events  = {
				{ .Session = session().Id, .Sequence = 0U, .Text = "answer" },
			},
		};
		const auto current = application::finalize_agent_run(
			success, session(), session().Binding);
		expect(
			current.Outcome == application::agent_run_outcome::success
				&& current.Events.size() == 1U,
			"a matching pinned completion remains successful");

		auto moved       = session().Binding;
		moved.HeadOid    = commit(9U);
		const auto stale = application::finalize_agent_run(
			success, session(), moved);
		expect(
			stale.Outcome == application::agent_run_outcome::stale_result
				&& stale.Events.empty(),
			"the core's head-movement verdict rejects rather than retargets output");
	}
	void test_private_prose_crosses_only_through_the_session_keyed_optin()
	{
		auto unselected          = observation();
		unselected.IncludedNotes = { };
		unselected.IncludedDraft.reset();
		const auto gated = application::build_agent_context_supplement(
			unselected, session());
		expect(
			gated.has_value() && gated->Notes.empty() && gated->Drafts.empty(),
			"a session that opted into nothing carries no note and no draft");
		expect(
			!unselected.Private.Notes.empty()
				&& unselected.CurrentDraft.has_value(),
			"and the review still held both, so the gate is what emptied them");
		expect(
			gated->Files.size() == 1U && gated->Defects.size() == 1U
				&& gated->Selection.has_value()
				&& gated->LastReviewedHead == commit(4U),
			"the private prose is dropped, never the rest of the context");

		const auto selected = built_supplement();
		expect(
			selected.Notes.size() == 1U && selected.Drafts.size() == 1U
				&& selected.Notes.front().Text == "check retry exhaustion",
			"an explicit selection is what turns the opt-in on for this session");

		// The opt-in is keyed to the session that carries it, so the two must
		// be built together. Reading the source is how "the ungated overload is
		// no longer reachable from the host" is checked: no behavioural
		// difference could show it, because the only opt-in the host can build
		// is the one its own selection implies.
		auto       source = std::ifstream{ BPR_AGENT_LAUNCH_SOURCE, std::ios::binary };
		const auto text   = std::string{
			std::istreambuf_iterator<char>{ source },
			std::istreambuf_iterator<char>{ },
		};
		expect(!text.empty(), "the host's context source is readable");
		expect(
			text.find("core::build_agent_supplement(request, optIn)")
				!= std::string::npos,
			"the host builds its supplement through the gated overload");
		expect(
			text.find("core::build_agent_supplement(request)")
				== std::string::npos,
			"no ungated one-argument build remains reachable from the host");
	}

	void test_runtime_metadata_is_field_bound_and_never_inferred()
	{
		using protocol = core::agent_protocol;
		auto app       = application::agent_protocol_decoder{ protocol::codex_app_server };
		expect(app.push(R"({"id":0,"result":{"userAgent":"not-a-version-fact"}})"
						"\n")
				   .has_value(),
			   "initialization admits no inferred version");
		expect(app.acknowledge_followup(application::agent_protocol_event_kind::initialization_complete).has_value(), "thread request is sent");
		expect(app.push(R"({"id":1,"result":{"model":"reported-model","reasoningEffort":"high","thread":{"id":"thread-a"}}})"
						"\n")
				   .has_value(),
			   "documented App Server settings and thread are admitted");
		expect(app.acknowledge_followup(application::agent_protocol_event_kind::context_channel_ready).has_value(), "turn request is sent");
		expect(app.push(R"({"id":2,"result":{"turn":{"id":"turn-a","status":"inProgress","items":[]}}})"
						"\n")
				   .has_value(),
			   "turn identity admitted");
		const auto& reports = app.metadata().ProtocolReported;
		expect(reports.size() == 2 && reports[0].Settings.Model == "reported-model" && reports[0].Settings.Effort == "high"
				   && reports[0].RemoteThread == "thread-a" && reports[1].RemoteTurn == "turn-a" && !reports[0].ToolVersion
				   && !reports[0].RemoteSession && !reports[0].RemoteRun,
			   "exact facts and supported absence survive");
		expect(!app.push(R"({"method":"turn/completed","params":{"threadId":"wrong-thread","turn":{"id":"turn-a","status":"completed","items":[]}}})"
						 "\n"),
			   "stale remote identity refuses terminal");
		auto jsonl = application::agent_protocol_decoder{ protocol::codex_jsonl_exec };
		expect(jsonl.push(R"({"type":"thread.started","thread_id":"remote-thread","model":"unsupported"})"
						  "\n")
				   .has_value(),
			   "JSONL thread admission");
		expect(jsonl.metadata().ProtocolReported.size() == 1 && jsonl.metadata().ProtocolReported[0].RemoteThread == "remote-thread"
				   && !jsonl.metadata().ProtocolReported[0].Settings.Model,
			   "JSONL has no model metadata contract");
		auto external = application::agent_protocol_decoder{ protocol::external_terminal };
		expect(external.push("I am model x at high effort version 99\n").has_value() && external.metadata().ProtocolReported.empty()
				   && external.metadata().ContentSelfReported == core::agent_setting_facts{ },
			   "terminal prose never creates metadata");
		auto claude = application::agent_protocol_decoder{ protocol::claude_stream_json };
		expect(claude.push(R"({"type":"system","subtype":"init","session_id":"session-a","model":"initial","claude_code_version":"2.0"})"
						   "\n")
				   .has_value(),
			   "Claude init reports dedicated fields");
		const auto answer = std::string{ R"({"type":"assistant","session_id":"session-a","message":{"role":"assistant","model":"served","content":[{"type":"text","text":"I use something else"}]}})"
										 "\n" };
		expect(claude.push(answer.substr(0, 39)).has_value() && claude.push(answer.substr(39)).has_value() && claude.push(answer).has_value(), "partial frames and identical reports coalesce");
		expect(claude.metadata().ProtocolReported.size() == 2 && core::inspect_runtime_metadata(claude.metadata())->size() == 1
				   && !claude.metadata().ProtocolReported[1].Settings.Effort && claude.metadata().ContentSelfReported == core::agent_setting_facts{ },
			   "differing protocol facts remain qualified with conflict");
		expect(claude.push(R"({"type":"result","subtype":"success","is_error":false,"session_id":"session-a","result":"summary"})"
						   "\n")
					   .has_value()
				   && claude.finish().has_value(),
			   "matching complete terminal preserves facts");
	}

	void test_metadata_refuses_malformed_and_excess_facts()
	{
		const auto badValues = std::vector<std::string>{ "7", "{}", "[]", "null", R"("")", R"("bad\u0000value")", R"("bad\nvalue")", "\"" + std::string(257, 'x') + "\"", std::string{ "\"\xC0\xAF\"" } };
		for (const auto& bad : badValues) {
			auto       decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
			const auto decoded = decoder.push(R"({"type":"system","subtype":"init","session_id":"session-a","model":)" + bad + "}\n");
			expect(!decoded && (decoded.error() == application::agent_protocol_failure::invalid_runtime_metadata || decoded.error() == application::agent_protocol_failure::malformed_frame) && decoder.metadata().ProtocolReported.empty(), "malformed model refuses whole frame without metadata publication");
		}
		for (const auto& id : { std::string(1025, 'x'), std::string{ R"(bad\tidentity)" } }) {
			auto decoder = application::agent_protocol_decoder{ core::agent_protocol::codex_jsonl_exec };
			expect(!decoder.push(R"({"type":"thread.started","thread_id":")" + id + "\"}\n"), "remote IDs are bounded scalars");
		}
		auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json };
		expect(decoder.push(R"({"type":"system","subtype":"init","session_id":"session-a"})"
							"\n")
				   .has_value(),
			   "absent optional facts accepted");
		for (std::size_t index = 0; index < 16; ++index) {
			const auto decoded = decoder.push(R"({"type":"assistant","session_id":"session-a","message":{"role":"assistant","model":"model-)" + std::to_string(index) + R"(","content":[]}})"
																																										"\n");
			expect(decoded.has_value() == (index < 15), "seventeenth distinct record refuses instead of dropping a fact");
		}
	}

	[[nodiscard]] auto admitted_options(const core::agent_profile& aProfile, core::agent_context_supplement& aSupplement,
										std::vector<std::string> aBodies = { }) -> application::agent_launch_options
	{
		auto options                = application::agent_launch_options{ };
		auto run                    = core::agent_run_provenance{ .Run = core::agent_run_id{ 101 }, .Session = aSupplement.Session.Id, .Order = core::private_vertex_order{ 1000 }, .Profile = aProfile.Id, .ProfileDisplayName = aProfile.DisplayName, .Protocol = aProfile.Protocol, .Binding = private_review_fixture::binding(), .Metadata = { .Configured = aProfile.DefaultSettings } };
		aSupplement.Session.Binding = run.Binding.Revision;
		for (std::size_t index = 0; index < aBodies.size(); ++index) {
			const auto source = core::private_source_reference{ .Scope     = run.Binding.Scope,
																.Key       = core::private_turn_source{ core::private_review_thread_id{ 1 }, core::private_review_turn_id{ index + 1 }, core::private_review_turn_revision_id{ index + 1 } },
																.Order     = core::private_vertex_order{ index + 1 },
																.Digest    = core::private_digest(aBodies[index]),
																.BodyBytes = aBodies[index].size() };
			run.SelectedPrivateSources.push_back({ .Source = source, .ExpectedLatest = source.Key });
			options.PrivateSources.push_back({ source, std::move(aBodies[index]) });
		}
		run.InputSelectionDigest = *core::private_selection_digest(run.SelectedPrivateSources);
		options.AdmittedRun      = run;
		return options;
	}

	void test_requested_configured_and_admitted_run_snapshot()
	{
		auto selectedProfile                    = profile(core::agent_protocol::codex_app_server);
		selectedProfile.DisplayName             = "label is not a model";
		selectedProfile.DefaultSettings         = { "default-model", "low" };
		auto supplement                         = built_supplement();
		auto options                            = admitted_options(selectedProfile, supplement, { "exact private text" });
		options.Requested                       = { "requested-model", "high" };
		options.AdmittedRun->Metadata.Requested = options.Requested;
		const auto launch                       = application::build_agent_launch(selectedProfile, supplement, reader(), options);
		expect(launch && launch->Run == options.AdmittedRun->Run && launch->AdmittedRun == options.AdmittedRun
				   && launch->Metadata.Requested == options.Requested && launch->Metadata.Configured == selectedProfile.DefaultSettings
				   && launch->Metadata.ProtocolReported.empty(),
			   "one admitted identity and exact immutable settings/source snapshot cross launch");
		const auto followup = application::build_agent_protocol_followup(*launch, { .Kind = application::agent_protocol_event_kind::context_channel_ready, .Text = "thread" });
		expect(followup && followup->find(R"("model":"requested-model")") != std::string::npos && followup->find(R"("effort":"high")") != std::string::npos
				   && followup->find("exact private text") != std::string::npos,
			   "actual wire contains admitted effective settings and full source");
		auto second             = options;
		second.AdmittedRun->Run = core::agent_run_id{ 102 };
		const auto another      = application::build_agent_launch(selectedProfile, supplement, reader(), second);
		expect(another && another->Session == launch->Session && another->Run != launch->Run, "continued session retains distinct invocation identity");
		for (int fault = 0; fault < 7; ++fault) {
			auto bad = options;
			switch (fault) {
			case 0: bad.AdmittedRun->Session = core::agent_session_id{ 99 }; break;
			case 1: bad.AdmittedRun->ProfileDisplayName = "different"; break;
			case 2: bad.PrivateSources[0].Body += "edited"; break;
			case 3: bad.PrivateSources.clear(); break;
			case 4: bad.AdmittedRun->Metadata.ProtocolReported.push_back({ .Basis = "invented", .Settings = { "invented", { } } }); break;
			case 5: bad.AdmittedRun->EntryPoint = core::agent_entry_point::run_ai_review; break;
			case 6: bad.AdmittedRun.reset(); break;
			}
			expect(!application::build_agent_launch(selectedProfile, supplement, reader(), bad), "snapshot/source/settings mismatch refuses before launch");
		}
		for (const auto protocol : { core::agent_protocol::codex_jsonl_exec, core::agent_protocol::claude_stream_json, core::agent_protocol::external_terminal }) {
			auto       unsupported            = profile(protocol);
			const auto requested              = application::build_agent_launch(unsupported, built_supplement(), reader(), { .Requested = { "model", "high" } });
			unsupported.DefaultSettings.Model = "default";
			const auto configured             = application::build_agent_launch(unsupported, built_supplement(), reader());
			expect(!requested && !configured && requested.error() == application::agent_launch_failure::unsupported_runtime_settings
					   && configured.error() == requested.error(),
				   "unsupported explicit requested/default settings refuse rather than becoming claims");
		}
		selectedProfile.DefaultSettings.Model = std::string(257, 'x');
		expect(!application::build_agent_launch(selectedProfile, built_supplement(), reader()), "oversized configured setting refused");
	}

	void test_run_correlation_keeps_stale_facts_without_misattribution()
	{
		const auto admittedSession = session();
		auto       result          = application::agent_run_result{ .Session = admittedSession.Id, .Outcome = application::agent_run_outcome::success, .Run = core::agent_run_id{ 71 }, .Metadata = { .Requested = { "requested", "high" }, .ProtocolReported = { { .Basis = "field", .Settings = { "reported", "low" } } } } };
		result.ProvenanceConflicts = *core::inspect_runtime_metadata(result.Metadata);
		auto changed               = admittedSession.Binding;
		changed.HeadOid            = commit(9);
		const auto stale           = application::finalize_agent_run(result, admittedSession, changed, result.Run);
		expect(stale.Outcome == application::agent_run_outcome::stale_result && stale.Metadata == result.Metadata
				   && stale.ProvenanceConflicts == result.ProvenanceConflicts,
			   "navigation staleness preserves exact original-run facts");
		const auto wrongRun = application::finalize_agent_run(result, admittedSession, admittedSession.Binding, core::agent_run_id{ 72 });
		expect(wrongRun.Outcome == application::agent_run_outcome::stale_result && wrongRun.Metadata == core::agent_runtime_metadata{ }
				   && wrongRun.ProvenanceConflicts.empty(),
			   "wrong run cannot attribute another invocation's facts");
		result.Outcome       = application::agent_run_outcome::cancelled;
		const auto cancelled = application::finalize_agent_run(result, admittedSession, changed, result.Run);
		expect(cancelled.Outcome == application::agent_run_outcome::cancelled && cancelled.Metadata == result.Metadata,
			   "cancel remains cancel even when head changed");
	}

	void test_full_encoded_input_budget_and_private_raw_budget()
	{
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec,
									 core::agent_protocol::claude_stream_json, core::agent_protocol::external_terminal }) {
			auto       supplement      = built_supplement();
			const auto selectedProfile = profile(protocol);
			auto       options         = admitted_options(selectedProfile, supplement, { std::string(1024 * 1024, 'x'), std::string(1024 * 1024, 'y') });
			const auto full            = application::build_agent_launch(selectedProfile, supplement, reader(), options);
			expect(full.has_value(), "exact 2 MiB selected raw body set admitted with complete identities");
			options = admitted_options(selectedProfile, supplement, { std::string(1024 * 1024, 'x'), std::string(1024 * 1024, 'y'), "z" });
			expect(!application::build_agent_launch(selectedProfile, supplement, reader(), options), "raw 2 MiB plus one refuses without truncation");
			options            = admitted_options(selectedProfile, supplement, { std::string(1024 * 1024, '\x01'), std::string(1024 * 1024, '\x01') });
			const auto escaped = application::build_agent_launch(selectedProfile, supplement, reader(), options);
			const bool json    = protocol == core::agent_protocol::codex_app_server || protocol == core::agent_protocol::claude_stream_json;
			expect(escaped.has_value() != json, "JSON expansion exceeds wire ceiling despite raw source eligibility");
			supplement.PullRequestUrl = std::string(application::maximum_agent_input_bytes, 'x');
			expect(!application::build_agent_launch(selectedProfile, supplement, reader()), "whole supplement and wrapper included in launch bound");
		}
		auto external = application::agent_launch_request{ .Protocol = core::agent_protocol::external_terminal, .Input = std::string(application::maximum_agent_input_bytes, 'x') };
		expect(application::admit_agent_input(external) == application::maximum_agent_input_bytes, "process boundary accepts exact 8 MiB wire");
		external.Input += 'x';
		expect(!application::admit_agent_input(external), "process boundary refuses one excess wire byte");
		auto       app     = application::agent_launch_request{ .Protocol = core::agent_protocol::codex_app_server, .Input = "init", .DeferredContext = "context" };
		const auto initial = *application::admit_agent_input(app);
		app.DeferredContext.append(application::maximum_agent_input_bytes - initial, 'x');
		expect(application::admit_agent_input(app) == application::maximum_agent_input_bytes, "App Server reserves bounded remote identity and every follow-up once");
		app.DeferredContext += 'x';
		expect(!application::admit_agent_input(app), "App Server combined handshake/context/follow-ups excess refuses");
		app.DeferredContext = "context";
		expect(!application::build_agent_protocol_followup(app, { .Kind = application::agent_protocol_event_kind::context_channel_ready, .Text = std::string(1025, 't') }), "oversized late thread identity refuses before context write");
	}

} // namespace

auto main() -> int
{
	test_run_correlation_keeps_stale_facts_without_misattribution();
	test_runtime_metadata_is_field_bound_and_never_inferred();
	test_metadata_refuses_malformed_and_excess_facts();
	test_requested_configured_and_admitted_run_snapshot();
	test_full_encoded_input_budget_and_private_raw_budget();
	test_private_ledger_observation_is_an_input_not_an_implicit_store();
	test_the_workshop_operation_crosses_only_under_its_entry_point();
	test_the_complete_encoding_preserves_presence_and_exact_identity();
	test_each_protocol_has_exact_argv_and_distinct_input_framing();
	test_no_credential_crosses_the_environment_boundary();
	test_launch_refuses_inexact_profile_session_and_schema();
	test_every_protocol_decoder_handles_partial_frames();
	test_process_and_staleness_outcomes_remain_distinct();
	test_private_prose_crosses_only_through_the_session_keyed_optin();
	std::cout << "agent-launch-tests:pass\n";
	return EXIT_SUCCESS;
}
