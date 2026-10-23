/// \file
/// \brief Proves exact context adaptation, framing, and typed host outcomes.

#include "../core/private_review_fixture.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/review_skill_host.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "bpr/core/agent_wrapper.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <expected>
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

	[[nodiscard]] auto hex_digest(const std::string_view aText) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       output = std::string{ };
		for (const auto byte : core::sha256({ reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() })) {
			output.push_back(digits[byte >> 4U]);
			output.push_back(digits[byte & 0x0FU]);
		}
		return output;
	}

	[[nodiscard]] auto workshop_press(const core::workshop_operation aOperation) -> application::agent_context_observation
	{
		auto pressed              = observation();
		pressed.EntryPoint        = core::agent_entry_point::workshop_comment;
		pressed.WorkshopOperation = aOperation;
		return pressed;
	}

	/// \brief [[BPR-1091]]. The SHA-256 of a clarify press's encoded
	/// supplement as the encoder wrote it before the review-feedback hook was
	/// sent (captured at `3c7c56620`). A press with the label helper off must
	/// still produce exactly these bytes.
	constexpr std::string_view workshop_press_wire_before_guidance = "281d8a49f244269639814ad1c567b0acf5f776be8c4012bfbf49bd1412cb6c4f";

	/// \brief [[BPR-1091]]. Helper off: the press's wire is byte for byte
	/// what it was before the guidance existed.
	/// \details [[BPR-PENDING-AQ5]]. Version three changed the `schema` line
	/// and nothing else a press writes: with that one line read as version
	/// two, the bytes are still the pinned ones.
	void test_a_press_without_guidance_keeps_its_wire_bytes()
	{
		const auto built = application::build_agent_context_supplement(workshop_press(core::workshop_operation::clarify), session());
		expect(built.has_value() && !built->FeedbackGuidance.has_value() && !built->Brief.has_value(), "a press with no guidance builds and carries none, and no brief");
		auto       encoded = application::encode_agent_supplement(*built);
		const auto version = encoded.find("schema\t3\n");
		expect(version == 0U, "the press is version three");
		encoded.replace(version, 9U, "schema\t2\n");
		const auto digest = hex_digest(encoded);
		if (digest != workshop_press_wire_before_guidance) {
			std::cerr << "workshop-press-wire-digest:" << digest << '\n';
		}
		expect(digest == workshop_press_wire_before_guidance, "a press without guidance writes exactly the bytes it wrote before the hook was sent");
		expect(encoded.find("feedback_guidance") == std::string::npos, "and names no guidance field at all");
	}

	/// \brief [[BPR-1091]]. Helper on: the press carries the effective
	/// review-feedback hook as three lines beside its operation, and nothing
	/// else about the launch moves.
	void test_a_press_carries_the_effective_feedback_guidance_and_nothing_more()
	{
		auto hooks = core::writing_hook_settings{ };
		expect(!application::workshop_feedback_supplement(hooks, application::review_label_helper::off).has_value(), "the label helper off sends no guidance");
		const auto shipped = application::workshop_feedback_supplement(hooks, application::review_label_helper::on);
		expect(shipped && shipped->Source == core::feedback_guidance_source::shipped && shipped->Version == application::review_feedback_hook_version
				   && shipped->Text == application::default_writing_hook(core::writing_hook::feedback),
			   "the label helper on sends the shipped default, named by its version");
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)] = "Team rule:\tlabel every comment.\nPublish nothing; you cannot.";
		const auto custom                                                       = application::workshop_feedback_supplement(hooks, application::review_label_helper::on);
		expect(custom && custom->Source == core::feedback_guidance_source::profile_override && custom->Version == application::review_feedback_hook_version
				   && custom->Text == hooks.Overrides[static_cast<std::size_t>(core::writing_hook::feedback)],
			   "a customized hook is sent as written, never merged with or appended to the default");
		hooks.Overrides[static_cast<std::size_t>(core::writing_hook::problem)] = "Problem override must not travel.";
		expect(application::workshop_feedback_supplement(hooks, application::review_label_helper::on) == custom, "a tour hook's override is not review-feedback guidance");

		auto pressed             = workshop_press(core::workshop_operation::clarify);
		pressed.FeedbackGuidance = custom;
		const auto built         = application::build_agent_context_supplement(pressed, session());
		expect(built && built->FeedbackGuidance == custom, "the composed press carries the guidance into the supplement");
		const auto encoded  = application::encode_agent_supplement(*built);
		const auto expected = std::string{ "workshop_operation\tclarify\n"
										   "feedback_guidance.source\tprofile_override\n"
										   "feedback_guidance.version\treview-feedback-v1\n"
										   "feedback_guidance.text\tTeam rule:\\tlabel every comment.\\nPublish nothing; you cannot.\n" };
		const auto at       = encoded.find(expected);
		expect(at != std::string::npos && encoded.find("feedback_guidance", at + expected.size()) == std::string::npos, "three escaped lines, once, directly after the operation they qualify");
		auto stripped = encoded;
		stripped.erase(at + std::string_view{ "workshop_operation\tclarify\n" }.size(), expected.size() - std::string_view{ "workshop_operation\tclarify\n" }.size());
		// [[BPR-PENDING-AQ5]]. Read as version two, as the press above is.
		stripped.replace(0U, 9U, "schema\t2\n");
		expect(hex_digest(stripped) == workshop_press_wire_before_guidance, "and every other byte of the press is the pre-guidance wire");

		auto defaulted             = workshop_press(core::workshop_operation::shorten);
		defaulted.FeedbackGuidance = shipped;
		const auto defaultWire     = application::encode_agent_supplement(*application::build_agent_context_supplement(defaulted, session()));
		expect(defaultWire.find("feedback_guidance.source\tshipped\nfeedback_guidance.version\treview-feedback-v1\nfeedback_guidance.text\t") != std::string::npos, "the shipped default names itself as shipped");

		auto onReview             = observation();
		onReview.FeedbackGuidance = shipped;
		const auto onReviewResult = application::build_agent_context_supplement(onReview, session());
		expect(!onReviewResult && onReviewResult.error() == application::agent_context_failure::invalid_observation, "guidance on a launch-panel review or chat turn is refused whole");
		auto onChat             = observation();
		onChat.ChatMessage      = "is this safe?";
		onChat.FeedbackGuidance = shipped;
		expect(!application::build_agent_context_supplement(onChat, session()), "and on a chat send");

		// No new authority: the same profile, environment, arguments and
		// framing with and without the guidance; only the payload grows.
		for (const auto protocol : { core::agent_protocol::codex_jsonl_exec, core::agent_protocol::claude_stream_json, core::agent_protocol::external_terminal }) {
			auto       plain     = *application::build_agent_context_supplement(workshop_press(core::workshop_operation::clarify), session());
			const auto with      = application::build_agent_launch(profile(protocol), *built, reader());
			const auto without   = application::build_agent_launch(profile(protocol), plain, reader());
			const auto sameShape = with && without && with->Arguments == without->Arguments && with->Environment == without->Environment && with->Executable == without->Executable
				&& with->WritingOnly == without->WritingOnly && with->WorkingDirectory == without->WorkingDirectory && with->Input.size() > without->Input.size();
			expect(sameShape, "guidance adds no argument, environment variable, tool or executable to any protocol's launch");
		}
	}

	/// [[BPR-PENDING-AQ5]], RFC-BPR-0092. A review request's brief crosses
	/// whole after every version-two field, counted and with its cuts stated;
	/// a launch without one writes no brief line.
	void test_the_brief_crosses_after_the_version_two_fields()
	{
		auto observed  = observation();
		observed.Brief = core::bound_review_brief({
			.Title       = "Bound the retry",
			.Description = "Line one\nLine two",
			.BaseBranch  = "main",
			.Files       = { { .Path = "src/retry.cpp" }, { .Path = "src/new.cpp", .PreviousPath = "src/old.cpp", .Status = core::brief_file_status::renamed } },
			.Threads     = { { .Path = "src/retry.cpp", .Line = 12U, .Comments = { { .Author = "octo", .Body = "Is this bounded?" } } } },
			.Workspace   = core::brief_workspace::linked_clone_history,
			.Skill       = core::brief_review_skill{ .Source = core::review_skill_source::built_in, .Version = 1U, .Text = std::string{ application::built_in_review_skill() } },
			// [[BPR-PENDING-AQ6]], owner decision (b): the conversation so far.
			.Conversation = { { .Asked = "Does this work?", .Answered = "Yes.\nThe loop ends." } },
		});
		expect(observed.Brief.has_value(), "the fixture brief is within its bounds");
		const auto built = application::build_agent_context_supplement(observed, session());
		expect(built.has_value() && built->Brief == observed.Brief, "the brief rides the supplement unchanged");
		const auto encoded = application::encode_agent_supplement(*built);
		for (const auto required : {
				 "brief.title\tBound the retry\n",
				 "brief.description\tLine one\\nLine two\n",
				 "brief.description.cut_bytes\t0\n",
				 "brief.base_branch\tmain\n",
				 "brief.workspace\tlinked_clone_history\n",
				 "brief.file.count\t2\n",
				 "brief.file.1.status\trenamed\n",
				 "brief.file.1.previous_path\tsrc/old.cpp\n",
				 "brief.file.omitted\t0\n",
				 "brief.thread.count\t1\n",
				 "brief.thread.0.line\t12\n",
				 "brief.thread.0.resolved\t0\n",
				 "brief.thread.0.comment.0.author\tocto\n",
				 "brief.thread.0.comment.0.body\tIs this bounded?\n",
				 "brief.thread.omitted\t0\n",
				 "brief.conversation.count\t1\n",
				 "brief.conversation.0.asked\tDoes this work?\n",
				 "brief.conversation.0.answered\tYes.\\nThe loop ends.\n",
				 "brief.conversation.omitted\t0\n",
				 "brief.skill.present\t1\n",
				 "brief.skill.source\tbuilt_in\n",
				 "brief.skill.version\t1\n",
			 }) {
			expect(encoded.find(required) != std::string::npos, "the exact brief field is encoded");
		}
		expect(encoded.find("brief.diff\tgit diff ") != std::string::npos && encoded.find("brief.skill.text\t---\\nname: bpr-pr-review") != std::string::npos,
			   "the diff is named and the skill's text is carried whole");
		expect(encoded.find("brief.title") > encoded.find("draft.count"), "the brief follows every version-two field");
		auto snapshot              = *built;
		snapshot.Brief->Workspace  = core::brief_workspace::private_snapshot;
		const auto snapshotEncoded = application::encode_agent_supplement(snapshot);
		expect(snapshotEncoded.find("brief.diff\t\n") != std::string::npos, "a snapshot names no diff: it has no base");
		const auto without = application::encode_agent_supplement(*application::build_agent_context_supplement(observation(), session()));
		expect(without.find("brief.") == std::string::npos, "a launch without a brief writes no brief line");
		auto oversized         = observed;
		oversized.Brief->Title = std::string(core::maximum_brief_title_bytes + 1U, 't');
		expect(!application::build_agent_context_supplement(oversized, session()), "a brief outside its bounds refuses the build");
	}

	void test_the_complete_encoding_preserves_presence_and_exact_identity()
	{
		const auto encoded = application::encode_agent_supplement(built_supplement());
		for (const auto required : {
				 "schema\t3\n",
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
				&& turn->find("schema\\t3\\n") != std::string::npos,
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
			case 0:  bad.AdmittedRun->Session = core::agent_session_id{ 99 }; break;
			case 1:  bad.AdmittedRun->ProfileDisplayName = "different"; break;
			case 2:  bad.PrivateSources[0].Body += "edited"; break;
			case 3:  bad.PrivateSources.clear(); break;
			case 4:  bad.AdmittedRun->Metadata.ProtocolReported.push_back({ .Basis = "invented", .Settings = { "invented", { } } }); break;
			case 5:  bad.AdmittedRun->EntryPoint = core::agent_entry_point::run_ai_review; break;
			case 6:  bad.AdmittedRun.reset(); break;
			default: expect(false, "the fixture selects a known fault"); break;
			}
			expect(!application::build_agent_launch(selectedProfile, supplement, reader(), bad), "snapshot/source/settings mismatch refuses before launch");
		}
		{
			auto       unsupported            = profile(core::agent_protocol::external_terminal);
			const auto requested              = application::build_agent_launch(unsupported, built_supplement(), reader(), { .Requested = { "model", "high" } });
			unsupported.DefaultSettings.Model = "default";
			const auto configured             = application::build_agent_launch(unsupported, built_supplement(), reader());
			expect(!requested && !configured && requested.error() == application::agent_launch_failure::unsupported_runtime_settings
					   && configured.error() == requested.error(),
				   "unsupported explicit requested/default settings refuse rather than becoming claims");
		}
		// [[BPR-PENDING-AQ6]], owner look v002. Codex's exec takes a model and
		// a reasoning effort, and Claude an effort, each as its own argument;
		// a level outside the agent's own vocabulary refuses.
		{
			const auto exec = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), built_supplement(), reader(), { .Requested = { "gpt-5", "high" } });
			expect(exec && exec->Arguments == std::vector<std::string>{ "exec", "--json", "-m", "gpt-5", "-c", "model_reasoning_effort=\"high\"", "-" }, "Codex exec runs the chosen model at the chosen effort");
			const auto badExec = application::build_agent_launch(profile(core::agent_protocol::codex_jsonl_exec), built_supplement(), reader(), { .Requested = { std::nullopt, "max" } });
			expect(!badExec && badExec.error() == application::agent_launch_failure::unsupported_runtime_settings, "an effort Codex does not take refuses");
			const auto claude = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), built_supplement(), reader(), { .Requested = { "opus", "xhigh" } });
			expect(claude && std::ranges::search(claude->Arguments, std::vector<std::string>{ "--model", "opus", "--effort", "xhigh" }).begin() != claude->Arguments.end(),
				   "Claude runs the chosen model at the chosen effort");
			const auto badClaude = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), built_supplement(), reader(), { .Requested = { std::nullopt, "minimal" } });
			expect(!badClaude && badClaude.error() == application::agent_launch_failure::unsupported_runtime_settings, "an effort Claude does not take refuses");
		}
		selectedProfile.DefaultSettings.Model = std::string(257, 'x');
		expect(!application::build_agent_launch(selectedProfile, built_supplement(), reader()), "oversized configured setting refused");
	}

	/// [[BPR-1375]]. Claude takes a model as `--model`, and
	/// ([[BPR-PENDING-AQ6]]) an effort of its own levels as `--effort`: a
	/// name or level that could read as an option refuses.
	void test_claude_takes_only_a_model_as_one_argument()
	{
		const auto claude   = profile(core::agent_protocol::claude_stream_json);
		const auto launched = application::build_agent_launch(claude, built_supplement(), reader(), { .Requested = { "claude-sonnet-5-5", { } } });
		expect(launched && launched->Arguments == std::vector<std::string>{ "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose", "--model", "claude-sonnet-5-5" }
				   && launched->Metadata.Requested.Model == "claude-sonnet-5-5",
			   "a requested Claude model is one argument after --model and is recorded as requested");
		auto configured                  = claude;
		configured.DefaultSettings.Model = "sonnet";
		const auto fromProfile           = application::build_agent_launch(configured, built_supplement(), reader());
		expect(fromProfile && std::ranges::find(fromProfile->Arguments, "sonnet") - std::ranges::find(fromProfile->Arguments, "--model") == 1,
			   "a configured Claude model is passed the same way");
		// [[BPR-PENDING-AQ6]]: Claude Code 2.1 takes `--effort` too, one of its
		// own five levels; anything else still refuses.
		const auto withEffort = application::build_agent_launch(claude, built_supplement(), reader(), { .Requested = { "claude-sonnet-5-5", "high" } });
		expect(withEffort && withEffort->Arguments.back() == "high" && withEffort->Arguments[withEffort->Arguments.size() - 2U] == "--effort", "Claude takes its effort as one argument after --effort");
		const auto badEffort = application::build_agent_launch(claude, built_supplement(), reader(), { .Requested = { "claude-sonnet-5-5", "--verbose" } });
		expect(!badEffort && badEffort.error() == application::agent_launch_failure::unsupported_runtime_settings, "an effort that is not one of Claude's levels refuses");
		for (const auto* hostile : { "--dangerously-skip-permissions", "-x", "claude sonnet", "model\nnext", "\"quoted\"", "" }) {
			const auto refused = application::build_agent_launch(claude, built_supplement(), reader(), { .Requested = { std::string{ hostile }, { } } });
			expect(!refused, "a model name that is not one plain argument refuses before launch");
		}
		expect(core::valid_claude_model("claude-sonnet-5-5") && core::valid_claude_model("claude-opus-5-5[1m]") && !core::valid_claude_model(std::string(129, 'a'))
				   && core::storable_claude_model("") && !core::valid_claude_model(""),
			   "the model rule admits real names and refuses empty or oversized ones");
	}

	/// [[BPR-1375]]. Tool-free writing runs `claude -p` on the person's own
	/// sign-in: the exact restriction set, no `--bare`, in that order.
	void test_claude_writing_argv_reads_the_account_sign_in()
	{
		const auto writing  = application::build_agent_launch(profile(core::agent_protocol::claude_stream_json), built_supplement(), reader(), { .Requested = { "claude-sonnet-5-5", { } }, .WritingOnly = true });
		const auto expected = std::vector<std::string>{ "-p", "--input-format", "stream-json", "--output-format", "stream-json", "--verbose", "--model", "claude-sonnet-5-5",
														"--tools", "", "--disallowedTools", "*", "--strict-mcp-config", "--mcp-config", "{\"mcpServers\":{}}", "--setting-sources", "",
														"--disable-slash-commands", "--no-session-persistence", "--settings",
														R"({"disableAllHooks":true,"autoMemoryEnabled":false,"enabledPlugins":{"cc-plugin-agents-md@builtin":false,"cc-plugin-telemetry@builtin":false,"cc-plugin-plugin-authoring@builtin":false}})" };
		expect(writing && writing->WritingOnly && writing->Arguments == expected, "writing argv is the exact restricted print-mode set");
		expect(writing && std::ranges::find(writing->Arguments, "--bare") == writing->Arguments.end(), "writing never passes --bare, which would ignore the account sign-in");
		for (const auto protocol : { core::agent_protocol::codex_app_server, core::agent_protocol::codex_jsonl_exec, core::agent_protocol::external_terminal }) {
			expect(!application::build_agent_launch(profile(protocol), built_supplement(), reader(), { .WritingOnly = true }), "no other protocol has a tool-free writing launch");
		}
		const auto profiles = core::built_in_agent_profiles();
		const auto builtIn  = std::ranges::find(profiles, core::claude_stream_json_profile_id, &core::agent_profile::Id);
		expect(std::ranges::find(builtIn->AllowedEnvironment, "USER") != builtIn->AllowedEnvironment.end(), "the shipped Claude profile passes USER, which its keychain sign-in is filed under");
	}

	/// [[BPR-1375]]. What Claude 2.1.296 writes around a turn, and what a
	/// signed-out Claude answers.
	void test_claude_decoder_reads_sign_in_and_benign_frames()
	{
		const auto decode = [](const bool aWriting, const std::string& aFrames) {
			auto decoder = application::agent_protocol_decoder{ core::agent_protocol::claude_stream_json, aWriting };
			auto pushed  = decoder.push(aFrames);
			if (!pushed) {
				return std::expected<std::size_t, application::agent_protocol_failure>{ std::unexpect, pushed.error() };
			}
			auto finished = decoder.finish();
			if (!finished) {
				return std::expected<std::size_t, application::agent_protocol_failure>{ std::unexpect, finished.error() };
			}
			return std::expected<std::size_t, application::agent_protocol_failure>{ decoder.assistant_items().size() };
		};
		const std::string init = R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[],"plugins":[]})"
								 "\n";
		const std::string turn = R"({"type":"assistant","session_id":"s","message":{"id":"m","role":"assistant","content":[{"type":"text","text":"ok"}]}})"
								 "\n"
								 R"({"type":"rate_limit_event","session_id":"s","rate_limit_info":{}})"
								 "\n"
								 R"({"type":"result","subtype":"success","session_id":"s","is_error":false,"result":"ok"})"
								 "\n";
		const auto        ok   = decode(true, std::string{ R"({"type":"system","subtype":"ui_invalidate","session_id":"s"})" } + "\n" + init + turn);
		expect(ok && *ok == 1U, "a ui_invalidate before init and a rate_limit_event after the turn are read past");
		const auto signedOut = decode(true, init + R"({"type":"assistant","session_id":"s","error":"authentication_failed","message":{"id":"x","model":"<synthetic>","role":"assistant","content":[{"type":"text","text":"Not logged in · Please run /login"}]}})" + "\n" + R"({"type":"result","subtype":"success","session_id":"s","is_error":true,"result":"Not logged in · Please run /login"})" + "\n");
		expect(!signedOut && signedOut.error() == application::agent_protocol_failure::not_signed_in, "a signed-out Claude is not_signed_in, not a malformed frame");
		const auto plugin = decode(true, std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[],"plugins":[{"name":"p"}]})" } + "\n" + turn);
		const auto memory = decode(true, std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":[],"mcp_servers":[],"memory_paths":{"auto":"/m"}})" } + "\n" + turn);
		const auto tools  = decode(true, std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":["Read"],"mcp_servers":[]})" } + "\n" + turn);
		expect(!plugin && !memory && !tools, "a writing init frame with a plugin, an auto-memory folder or a tool refuses");
		const auto review = decode(false, std::string{ R"({"type":"system","subtype":"init","session_id":"s","tools":["Read"],"mcp_servers":[],"plugins":[{"name":"p"}]})" } + "\n" + turn);
		expect(review && *review == 1U, "an ordinary review run keeps its tools and plugins");
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
	test_claude_takes_only_a_model_as_one_argument();
	test_claude_writing_argv_reads_the_account_sign_in();
	test_claude_decoder_reads_sign_in_and_benign_frames();
	test_full_encoded_input_budget_and_private_raw_budget();
	test_private_ledger_observation_is_an_input_not_an_implicit_store();
	test_the_workshop_operation_crosses_only_under_its_entry_point();
	test_a_press_without_guidance_keeps_its_wire_bytes();
	test_a_press_carries_the_effective_feedback_guidance_and_nothing_more();
	test_the_complete_encoding_preserves_presence_and_exact_identity();
	test_the_brief_crosses_after_the_version_two_fields();
	test_each_protocol_has_exact_argv_and_distinct_input_framing();
	test_no_credential_crosses_the_environment_boundary();
	test_launch_refuses_inexact_profile_session_and_schema();
	test_every_protocol_decoder_handles_partial_frames();
	test_process_and_staleness_outcomes_remain_distinct();
	test_private_prose_crosses_only_through_the_session_keyed_optin();
	std::cout << "agent-launch-tests:pass\n";
	return EXIT_SUCCESS;
}
