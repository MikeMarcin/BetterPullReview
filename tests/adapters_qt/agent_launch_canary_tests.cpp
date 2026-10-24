/// \file
/// \brief Hostile real-process coverage for agent protocol and teardown policy.

#include "../core/private_review_fixture.hpp"
#include "../sqlite_contract/scratch_root.hpp"
#include "../support/scratch_directory.hpp"
#include "../support/test_expectation.hpp"
#include "bpr/adapters/agent/agent_process_launcher.hpp"
#include "bpr/adapters/sqlite/private_review_store.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/private_review_application_host.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#if defined(Q_OS_WIN)
	#include "bpr/adapters/agent/windows_workspace_guard.hpp"

	#include <aclapi.h>
#endif

#if defined(Q_OS_UNIX)
	#include <fcntl.h>
	#include <sys/file.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif

namespace {
	using namespace bpr;

	using bpr::tests::expect;

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

	[[nodiscard]] auto canary_profile(const core::agent_protocol aProtocol)
		-> core::agent_profile
	{
		return {
			.Id          = core::agent_profile_id{ 1U },
			.DisplayName = "Canary",
			.Executable  = BPR_AGENT_CANARY_EXECUTABLE,
			.Protocol    = aProtocol,
			.Workspace
			= core::agent_workspace_policy::disposable_review_workspace,
			.AllowedEnvironment = {
				"BPR_CANARY_AGENT_IDENTITY",
				"BPR_CANARY_ALLOWED",
			},
		};
	}

	[[nodiscard]] auto supplement(const std::string_view aLargeDraft = { }) -> core::agent_context_supplement
	{
		const auto comparison = identity<core::file_comparison_digest>(5U);
		const auto anchor     = core::source_line_ref{
			.Comparison = comparison,
			.Side       = core::diff_side::right,
			.Line       = 17U,
		};
		const auto segment = identity<core::alignment_segment_id>(6U);
		const auto group   = identity<core::change_group_id>(7U);
		const auto built   = core::build_agent_supplement({
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = session(),
			.PullRequestUrl     = "https://github.example/octo/app/pull/30",
			.PreviousUpload     = commit(3U),
			.LastReviewedHead   = commit(4U),
			.Files              = {
				{
					.File               = identity<core::file_identity>(3U),
					.Material           = identity<core::file_material_id>(4U),
					.Path               = "src/retry.cpp",
					.Read               = core::file_read_state::read,
					.ChangedSinceReview = true,
					.Accepted           = true,
				},
			},
			.Defects = {
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
			.Selection = core::supplement_semantic_context{
				.File      = identity<core::file_identity>(3U),
				.Selection = {
					.Comparison    = comparison,
					.Kind          = core::semantic_selection_kind::group,
					.PreferredLine = anchor,
					.Segment       = segment,
					.Group         = group,
				},
				.Confidence = core::supplement_mapping_confidence::similar,
				.Segments   = {
					{
						.Id    = segment,
						.Kind  = core::alignment_kind::replacement,
						.Left  = { 16U, 18U },
						.Right = { 16U, 19U },
						.Group = group,
					},
				},
				.Groups = {
					{
						.Id      = group,
						.Members = { 0U, 1U },
						.Left    = { 16U, 18U },
						.Right   = { 16U, 19U },
						.First   = { 16U, 16U },
						.Last    = { 18U, 19U },
					},
				},
				.SelectedSourceText = std::string{ "return retry();" },
			},
			.SelectedNotes = {
				{
					.File       = identity<core::file_identity>(3U),
					.Comparison = comparison,
					.Text       = "check retry exhaustion",
				},
			},
			.SelectedDrafts = {
				{ .Anchor = anchor, .Text = "Would a bounded retry be safer?" },
			},
		});
		expect(built.has_value(), "the hostile fixture supplement builds");
		auto result = *built;
		if (!aLargeDraft.empty()) {
			// [[BPR-0886]]. A draft is the one part of the request a reviewer
			// can make large, so it is what makes this request larger than a
			// pipe holds.
			auto large = core::build_agent_supplement({
				.ApplicationSession = core::application_session_id{ 11U },
				.Session            = session(),
				.PullRequestUrl     = "https://github.example/octo/app/pull/30",
				.PreviousUpload     = commit(3U),
				.LastReviewedHead   = commit(4U),
				.SelectedDrafts     = {
					{ .Anchor = anchor, .Text = std::string{ aLargeDraft } },
				},
			});
			expect(large.has_value(), "the supplement with a large draft builds");
			result = *large;
		}
		return result;
	}

	[[nodiscard]] auto request(const core::agent_protocol aProtocol, const std::string_view aLargeDraft = { })
		-> application::agent_launch_request
	{
		auto built = application::build_agent_launch(
			canary_profile(aProtocol),
			supplement(aLargeDraft),
			[](const std::string& aName) -> std::optional<std::string> {
				return qEnvironmentVariableIsSet(aName.c_str())
					? std::optional{
						  qEnvironmentVariable(aName.c_str()).toStdString()
					  }
					: std::nullopt;
			});
		expect(built.has_value(), "the matching canary request builds");
		built->WorkingDirectory = std::filesystem::current_path().string();
		return *built;
	}

	[[nodiscard]] auto event_values(
		const application::agent_run_result& aResult, const std::string_view aTag)
		-> std::vector<std::string>
	{
		auto values = std::vector<std::string>{ };
		for (const auto& event : aResult.Events) {
			if (event.Text.starts_with(std::string{ aTag } + "\t")) {
				values.push_back(event.Text.substr(aTag.size() + 1U));
			}
		}
		return values;
	}

	/// \brief FNV-1a, 64-bit, as sixteen lower-case hex digits: the canary's
	/// own digest of what it read.
	[[nodiscard]] auto fnv1a_hex(const std::string_view aValue) -> std::string
	{
		constexpr std::string_view digits = "0123456789abcdef";
		auto                       hash   = std::uint64_t{ 14'695'981'039'346'656'037ULL };
		for (const auto character : aValue) {
			hash ^= static_cast<unsigned char>(character);
			hash *= 1'099'511'628'211ULL;
		}
		auto output = std::string(16U, '0');
		for (auto index = std::size_t{ 0U }; index < output.size(); ++index) {
			output[output.size() - 1U - index] = digits[(hash >> (index * 4U)) & 0x0FU];
		}
		return output;
	}

	void plant_parent_secrets()
	{
		for (const auto& [name, value] : {
				 std::pair{ "GITHUB_TOKEN", "ghp_the_reviewers_own_authority" },
				 std::pair{ "AWS_SECRET_ACCESS_KEY", "aws_secret_value" },
				 std::pair{ "ANTHROPIC_API_KEY", "sk-ant-secret" },
			 }) {
			qputenv(name, QByteArray{ value });
			expect(
				qEnvironmentVariableIsSet(name),
				"the parent really holds each planted credential");
		}
	}

	void test_durable_private_input_crosses_only_after_admission()
	{
		using namespace application;
		using namespace core;
		tests::sqlite_contract::scratch_root scratch{ "private-input-child" };
		const auto                           profile = canary_profile(agent_protocol::codex_jsonl_exec);
		const auto                           current = session();
		auto                                 binding = private_review_fixture::binding();
		binding.Revision                             = current.Binding;
		adapters::sqlite::private_review_configuration config{ { scratch.path(), "github.com", "synthetic", "example", 30 }, bpr_profile_id{ 1 }, current.Binding.Host, github_account_id{ 3 }, current.Binding.Repository, current.Binding.PullRequest };
		auto                                           opened = adapters::sqlite::make_private_review_store(config);
		expect(opened.has_value(), "open durable child course");
		auto& store   = **opened;
		binding.Scope = store.frontier()->Scope;
		auto seed     = [&](std::string aBody) {
			auto next     = store.frontier()->NextIdentity;
			auto revision = *create_private_thread(private_review_thread_id{ next }, { binding, private_pr_subject{ } }, { private_review_turn_id{ next + 1 }, private_review_turn_revision_id{ next + 2 }, private_vertex_order{ next + 2 }, private_reviewer_actor{ }, std::move(aBody) });
			expect(store.commit({ .ExpectedNextIdentity = next, .Threads = { { revision.Thread, 0 } }, .Revisions = { revision.Revision } }).has_value(), "commit child input fixture source");
			return source_of(revision.Revision);
		};
		auto selected = seed("EXPLICIT_PRIVATE_CHILD_CANARY");
		(void)seed("UNSELECTED_PRIVATE_CHILD_CANARY");
		private_review_application_host host{ store };
		agent_context_observation       context{ .ApplicationSession = application_session_id{ 11 }, .Binding = current.Binding, .PullRequestUrl = "https://github.example/octo/app/pull/30", .ChatMessage = "Synthetic private conversation" };
		auto                            admitted = host.admit({ .Binding = binding, .Sources = { { selected, selected.Key, false } } }, profile, current, context, { });
		expect(admitted.has_value(), "atomic private run admission");
		auto supplementValue = build_agent_context_supplement(context, current);
		expect(supplementValue.has_value(), "build admitted context");
		auto launched = build_agent_launch(profile, *supplementValue, [](const std::string& aName) -> std::optional<std::string> { return aName == "BPR_CANARY_AGENT_IDENTITY" ? std::optional<std::string>{ "allowed" } : std::nullopt; }, admitted->Launch);
		expect(launched.has_value(), "exact admitted private launch");
		launched->WorkingDirectory = scratch.path().string();
		auto claim                 = host.claim(*admitted, { });
		expect(claim.has_value(), "claim current source before child input");
		launched->PrivateInputClaim = *claim;
		adapters::agent::agent_process_launcher launcher;
		auto                                    result = launcher.run(*launched, 20'000);
		expect(result.Outcome == agent_run_outcome::success && result.Run == admitted->Delta.Run, "real child produces exact durable run response");
		const auto input = event_values(result, "stdin");
		expect(input.size() == 1 && input.front().find("EXPLICIT_PRIVATE_CHILD_CANARY") != std::string::npos && input.front().find("UNSELECTED_PRIVATE_CHILD_CANARY") == std::string::npos, "real child reads selected private body and never unchecked body");
		expect(!(*claim)->permitted(), "real transport releases source claim after write");
		expect(host.finish(*admitted, result, { .Binding = current.Binding }).has_value(), "real correlated full child output commits as agent turn");
		auto transcript = store.list_revisions(*admitted->Delta.Thread);
		expect(transcript && transcript->Revisions.size() == 2 && transcript->Revisions.back().Revision.Body.find("canary-complete") != std::string::npos, "durable turn contains validated complete child output");
	}

	/// [[BPR-PENDING-AQ6]], owner look v006 ("Codex is totally missing its
	/// model settings"). A catalog run through the real launcher: the App
	/// Server handshake, `model/list`, the list read past a notification,
	/// and the input closed so the child exits.
	void test_a_catalog_run_lists_models_through_a_real_child()
	{
		const auto launch = [] {
			auto built             = application::build_agent_model_catalog_launch(canary_profile(core::agent_protocol::codex_app_server), BPR_AGENT_CANARY_EXECUTABLE, { "--profile=work" },
																				   [](const std::string&) -> std::optional<std::string> { return std::nullopt; });
			built.WorkingDirectory = std::filesystem::current_path().string();
			return built;
		}();
		expect(launch.Arguments == std::vector<std::string>{ "--profile=work", "app-server" } && launch.Purpose == application::agent_launch_purpose::model_catalog,
			   "a catalog run is the agent's fixed arguments, then app-server");
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launch, 20'000);
		const auto models   = application::decode_agent_model_catalog(result);
		expect(result.Outcome == application::agent_run_outcome::success && models.size() == 1U && models.front().Model == "canary-model" && models.front().DisplayName == "Canary Model"
				   && models.front().Efforts == std::vector<std::string>{ "low", "max" },
			   "the real child's own list comes back, its efforts with max");
	}

	void test_all_four_protocols_stream_partial_frames_under_stderr_pressure()
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto protocols = std::array{
			core::agent_protocol::codex_app_server,
			core::agent_protocol::codex_jsonl_exec,
			core::agent_protocol::claude_stream_json,
			core::agent_protocol::external_terminal,
		};
		for (const auto protocol : protocols) {
			auto launched = request(protocol);
			launched.Run  = core::agent_run_id{ 71U };
			if (protocol == core::agent_protocol::codex_app_server) {
				launched.Metadata.Requested = { "canary-requested", "high" };
			}
			launched.Arguments.emplace_back("--stderr-pressure");
			auto streamed     = std::vector<application::agent_stream_event>{ };
			auto firstEventAt = std::optional<qint64>{ };
			auto timer        = QElapsedTimer{ };
			timer.start();
			auto       launcher = adapters::agent::agent_process_launcher{ };
			const auto result   = launcher.run(
				launched,
				20'000,
				[&](const application::agent_stream_event& aEvent) {
					if (!firstEventAt) {
						firstEventAt = timer.elapsed();
					}
					streamed.push_back(aEvent);
				});
			const auto finishedAt = timer.elapsed();
			expect(result.Run == launched.Run, "real process result preserves exact invocation identity");
			if (protocol == core::agent_protocol::codex_app_server) {
				expect(result.Metadata.Requested.Model == "canary-requested" && result.Metadata.ProtocolReported.size() == 2
						   && result.Metadata.ProtocolReported[0].Settings.Model == "canary-reported" && result.ProvenanceConflicts.size() == 2,
					   "real App Server IPC retains requested/reported disagreement");
			} else if (protocol == core::agent_protocol::claude_stream_json) {
				expect(result.Metadata.ProtocolReported.size() == 2 && result.Metadata.ProtocolReported[0].ToolVersion == "fixture-1"
						   && result.Metadata.ProtocolReported[1].Settings.Model == "canary-served" && result.ProvenanceConflicts.size() == 1,
					   "real Claude IPC retains version, session and conflicting reported models");
			} else if (protocol == core::agent_protocol::codex_jsonl_exec) {
				expect(result.Metadata.ProtocolReported.size() == 1 && !result.Metadata.ProtocolReported[0].Settings.Model,
					   "real JSONL reports its thread and leaves model absent");
			} else {
				expect(result.Metadata.ProtocolReported.empty(), "real terminal leaves unsupported metadata unknown");
			}
			expect(result.Metadata.ContentSelfReported == core::agent_setting_facts{ }, "output prose never supplies runtime facts");

			expect(
				result.Outcome == application::agent_run_outcome::success
					&& result.ExitCode == 0,
				"each selected protocol reaches a typed successful terminal");
			expect(
				firstEventAt && *firstEventAt < finishedAt && streamed == result.Events
					&& result.Events.size() > 3U,
				"partial stdout frames publish incrementally before process completion");
			expect(
				result.StandardError.size() == 256U * 1024U,
				"stderr pressure is drained concurrently and retained within its bound");
			const auto complete = std::ranges::find_if(
				result.Events,
				[](const application::agent_stream_event& aEvent) {
					return aEvent.Text == "canary-complete";
				});
			expect(
				complete != result.Events.end(),
				"each protocol preserves its framing through hostile partial reads");
			for (auto index = std::size_t{ 0U }; index < result.Events.size(); ++index) {
				expect(
					result.Events[index].Session == session().Id
						&& result.Events[index].Run == launched.Run
						&& result.Events[index].Sequence == index,
					"every streamed frame carries exact ordered session identity");
			}
		}
	}

	// [[BPR-0886]]. A child that writes more than a pipe holds to stderr
	// before it reads its request, given a request larger than a pipe holds.
	// A parent that writes the whole request before draining anything waits
	// on the child while the child waits on it, and the run ends at its
	// timeout. The parent has to service the child's output while it writes.
	void test_output_before_input_does_not_stall_the_request()
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto draft = std::string(12U * 1024U, 'd');
		for (const auto protocol : {
				 core::agent_protocol::codex_jsonl_exec,
				 core::agent_protocol::claude_stream_json,
				 core::agent_protocol::external_terminal,
			 }) {
			auto launched = request(protocol, draft);
			expect(launched.Input.size() > 12U * 1024U && launched.Input.find(draft) != std::string::npos,
				   "the request carries the large draft and is larger than a pipe holds");
			launched.Arguments.emplace_back("--stderr-before-input");
			auto timer = QElapsedTimer{ };
			timer.start();
			auto       launcher = adapters::agent::agent_process_launcher{ };
			const auto result   = launcher.run(launched, 20'000, { });
			const auto elapsed  = timer.elapsed();
			std::cout << "BPR-0886 protocol " << static_cast<int>(protocol) << ": request " << launched.Input.size()
					  << " bytes, outcome " << static_cast<int>(result.Outcome) << ", " << elapsed << " ms\n";
			expect(result.Outcome == application::agent_run_outcome::success && result.ExitCode == 0,
				   "a child that writes before it reads its request completes");
			expect(elapsed < 10'000, "it completes well inside its timeout rather than at it");
			expect(result.StandardError == std::string(64U * 1024U, 'e'),
				   "everything the child wrote before reading is kept, in order");
			// The canary reports a length and a digest rather than echoing the
			// request, whose paced echo alone would outlast the timeout.
			expect(event_values(result, "stdin-bytes") == std::vector{ std::to_string(launched.Input.size()) }
					   && event_values(result, "stdin-fnv1a") == std::vector{ fnv1a_hex(launched.Input) },
				   "the child received the whole request, byte for byte");
		}
	}

	void test_private_claim_waits_for_buffered_input_delivery()
	{
		struct observed_claim final : application::agent_input_claim
		{
			std::atomic_bool   Released = false;
			[[nodiscard]] auto permitted() const noexcept -> bool override { return !Released.load(); }
			void               release() noexcept override { Released.store(true); }
		};
		// [[BPR-1229]]. A root the contained child can write its marker in.
		const auto                      directory = bpr::tests::make_scratch_directory("bpr-agent-claim");
		const bpr::tests::scratch_guard ownedDirectory{ directory };
		const auto                      marker   = (directory / "child-started").string();
		auto                            launched = request(core::agent_protocol::codex_jsonl_exec, std::string(12U * 1024U, 'p'));
		launched.Input.append(512U * 1024U, 'x');
		launched.Arguments.insert(launched.Arguments.end(), { "--pre-assignment-side-effect", marker, "--stderr-before-input", "--delay-input-ms", "400" });
		auto claim                 = std::make_shared<observed_claim>();
		launched.PrivateInputClaim = claim;
		adapters::agent::agent_process_launcher      launcher;
		std::optional<application::agent_run_result> result;
		auto                                         child = std::jthread{ [&] { result = launcher.run(launched, 20'000); } };
		for (int attempt = 0; attempt < 100 && !std::filesystem::exists(marker); ++attempt) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 5 });
		}
		expect(std::filesystem::exists(marker), "child reaches delayed-input checkpoint");
		std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
		expect(claim->permitted(), "source claim remains held while Qt has queued private input for a backpressured child");
		child.join();
		expect(result && result->Outcome == application::agent_run_outcome::success && !claim->permitted(), "child receives all input and claim releases after pipe drain");
		expect(event_values(*result, "stdin-bytes") == std::vector{ std::to_string(launched.Input.size()) }, "backpressured child receives complete private input");
	}

	void test_real_child_receives_no_parent_credential_or_context_in_argv()
	{
		plant_parent_secrets();
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto launched = request(core::agent_protocol::codex_jsonl_exec);
		// [[BPR-1229]]. The shared helper's owner-private root, which on Windows
		// also grants the AppContainer the row runs in.
		const auto                      workingDirectory = bpr::tests::make_scratch_directory("bpr-agent-canary");
		const bpr::tests::scratch_guard ownedWorkingDirectory{ workingDirectory };

		launched.WorkingDirectory = workingDirectory.string();
		auto       launcher       = adapters::agent::agent_process_launcher{ };
		const auto result         = launcher.run(launched, 20'000);
		expect(
			result.Outcome == application::agent_run_outcome::success,
			"the boundary canary succeeds");

		auto environment = event_values(result, "env");
		auto names       = std::vector<std::string>{ };
		for (const auto& entry : environment) {
			names.push_back(entry.substr(0U, entry.find('=')));
		}
		std::ranges::sort(names);
		expect(
			names
				== std::vector<std::string>{
					"BPR_CANARY_AGENT_IDENTITY", "BPR_CANARY_ALLOWED" },
			"the child's complete environment equals the profile allowlist");
		for (const auto secret :
			 { "GITHUB_TOKEN", "AWS_SECRET_ACCESS_KEY", "ANTHROPIC_API_KEY" }) {
			expect(
				qEnvironmentVariableIsSet(secret)
					&& std::ranges::find(names, secret) == names.end(),
				"each credential exists only in the parent");
		}

		const auto arguments = event_values(result, "argv");
		expect(
			arguments.size() == 4U && arguments.at(1) == "exec"
				&& arguments.at(2) == "--json" && arguments.at(3) == "-",
			"the child reports only its executable and exact protocol switches");
		for (const auto& argument : arguments) {
			expect(
				argument.find("github.example") == std::string::npos
					&& argument.find("retry exhaustion") == std::string::npos,
				"PR identity and private context never reach argv");
		}
		const auto childWorkingDirectories = event_values(result, "cwd");
		expect(
			childWorkingDirectories.size() == 1U
				&& std::filesystem::path{ childWorkingDirectories.front() }
						.lexically_normal()
					== workingDirectory.lexically_normal(),
			"the child reports the exact request working directory, not the parent cwd");
		const auto input = event_values(result, "stdin");
		expect(
			std::ranges::any_of(input, [](const std::string& aInput) {
				return aInput.find("schema\t3\n") != std::string::npos
					&& aInput.find("pull_request_url\t") != std::string::npos
					&& aInput.find("last_reviewed_head.present\t1")
					!= std::string::npos
					&& aInput.find("file.0.changed_since_review\t1")
					!= std::string::npos
					&& aInput.find("defect.0.id\t80") != std::string::npos
					&& aInput.find("selection.source_text.value\treturn retry();")
					!= std::string::npos
					&& aInput.find("note.0.text\tcheck retry exhaustion")
					!= std::string::npos
					&& aInput.find("draft.0.text\tWould a bounded retry be safer?")
					!= std::string::npos;
			}),
			"the child reads every required local context class only from stdin");
	}

	void test_input_budget_refuses_before_real_child_effect()
	{
		// [[BPR-1229]]. The shared helper's root, which on Windows also grants
		// the AppContainer the row runs in, so the contained child can write
		// its marker; a bare QTemporaryDir does not.
		const auto                      root = bpr::tests::make_scratch_directory("bpr-agent-budget");
		const bpr::tests::scratch_guard ownedRoot{ root };
		const auto                      marker   = root / "child-started";
		auto                            launched = request(core::agent_protocol::external_terminal);
		launched.WorkingDirectory                = root.string();
		launched.Arguments                       = { "--pre-assignment-side-effect", marker.string() };
		launched.Input.assign(application::maximum_agent_input_bytes + 1, 'x');
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto refused  = launcher.run(launched, 20'000);
		expect(refused.ProtocolFailure == application::agent_protocol_failure::input_too_large && !std::filesystem::exists(marker),
			   "over-limit input cannot reach even the child's first side effect");
		launched.Input     = "small context\n";
		const auto control = launcher.run(launched, 20'000);
		expect(control.Outcome == application::agent_run_outcome::success && std::filesystem::exists(marker),
			   "negative control proves the same child creates the marker when admitted");
	}

	void test_independent_agent_identity_alone_controls_canary_authority()
	{
		qputenv("BPR_CANARY_ALLOWED", QByteArray{ "allowed-value" });
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto allowedRequest = request(core::agent_protocol::claude_stream_json);
		auto       launcher       = adapters::agent::agent_process_launcher{ };
		const auto allowed        = launcher.run(allowedRequest, 20'000);
		expect(
			allowed.Outcome == application::agent_run_outcome::success,
			"the independently configured allowed identity succeeds");

		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "denied" });
		const auto deniedRequest = request(core::agent_protocol::claude_stream_json);
		const auto denied        = launcher.run(deniedRequest, 20'000);
		expect(
			denied.Outcome == application::agent_run_outcome::nonzero_exit
				&& denied.ExitCode == 23,
			"changing only the agent's own synthetic identity changes its authority outcome");
		expect(
			allowedRequest.Arguments == deniedRequest.Arguments
				&& allowedRequest.Input == deniedRequest.Input,
			"BetterPullReview context and argv do not carry or substitute that authority");
	}

	void test_every_terminal_condition_is_typed_and_failed_runs_publish_no_result()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto launcher = adapters::agent::agent_process_launcher{ };

		auto unmaterialized = request(core::agent_protocol::external_terminal);
		unmaterialized.WorkingDirectory.clear();
		const auto refusedDirectory = launcher.run(unmaterialized, 2'000);
		expect(
			refusedDirectory.Outcome
					== application::agent_run_outcome::failed_to_start
				&& !refusedDirectory.StandardError.empty()
				&& refusedDirectory.Events.empty(),
			"the launcher refuses before start when no materialized working directory was accepted");

		auto failedStart       = request(core::agent_protocol::external_terminal);
		failedStart.Executable = "/definitely/not/a/betterpullreview-agent";
		const auto failed      = launcher.run(failedStart, 2'000);
		expect(
			failed.Outcome == application::agent_run_outcome::failed_to_start,
			"a missing executable is failed-to-start, not cancellation");

		auto nonzero = request(core::agent_protocol::codex_jsonl_exec);
		nonzero.Arguments.emplace_back("--nonzero-exit");
		const auto nonzeroResult = launcher.run(nonzero, 20'000);
		expect(
			nonzeroResult.Outcome == application::agent_run_outcome::nonzero_exit
				&& nonzeroResult.ExitCode == 17 && nonzeroResult.Events.empty(),
			"a nonzero child exit is typed and does not become a successful answer");

		auto malformed = request(core::agent_protocol::codex_jsonl_exec);
		malformed.Arguments.emplace_back("--malformed-frame");
		const auto malformedResult = launcher.run(malformed, 20'000);
		expect(
			malformedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& malformedResult.Events.empty()
				&& malformedResult.ProtocolFailure
					== application::agent_protocol_failure::malformed_frame,
			"a malformed structured frame is a typed protocol failure");

		auto malformedJson = request(core::agent_protocol::codex_jsonl_exec);
		malformedJson.Arguments.emplace_back("--malformed-json");
		const auto malformedJsonResult = launcher.run(malformedJson, 20'000);
		expect(
			malformedJsonResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& malformedJsonResult.ProtocolFailure
					== application::agent_protocol_failure::malformed_frame,
			"a real child cannot disguise malformed JSON as a known event");

		auto preemptive = request(core::agent_protocol::codex_app_server);
		preemptive.Arguments.emplace_back("--preemptive-app-server");
		const auto preemptiveResult = launcher.run(preemptive, 20'000);
		expect(
			preemptiveResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& preemptiveResult.ProtocolFailure
					== application::agent_protocol_failure::causal_violation
				&& preemptiveResult.Events.empty(),
			"a child that emits the whole App Server exchange before reading follow-ups is refused");

		for (const auto& [statusSwitch, expectedFailure] : {
				 std::pair{
					 std::string_view{ "--remote-failed" },
					 application::agent_protocol_failure::remote_failed },
				 std::pair{
					 std::string_view{ "--remote-interrupted" },
					 application::agent_protocol_failure::remote_interrupted },
			 }) {
			auto remote = request(core::agent_protocol::codex_app_server);
			remote.Arguments.emplace_back(statusSwitch);
			const auto remoteResult = launcher.run(remote, 20'000);
			expect(
				remoteResult.Outcome
						== application::agent_run_outcome::protocol_failure
					&& remoteResult.ProtocolFailure == expectedFailure
					&& remoteResult.Events.empty(),
				"App Server terminal status is retained as an exact typed failure");
		}

		// Nothing the drain reads is refusable here, so the trailing decode is
		// the first and only refusal. This is why the host must still adopt a
		// cause from that decode, and it is the half of the ordering that a
		// guard against restating an earlier cause must not break.
		auto truncated = request(core::agent_protocol::codex_jsonl_exec);
		truncated.Arguments.emplace_back("--truncated-exchange");
		const auto truncatedResult = launcher.run(truncated, 20'000);
		expect(
			truncatedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& truncatedResult.ProtocolFailure
					== application::agent_protocol_failure::incomplete_frame
				&& truncatedResult.Events.empty(),
			"an exchange that stops early is refused as incomplete by the trailing decode");

		// The switches above all outrun a pipe buffer or wait on parent input,
		// so the parent always reads their refusal while the child is still
		// running. This one writes its whole exchange at once and exits, so the
		// refusal arrives in the same drain that ends the loop â€” the one path
		// where a later trailing decode could restate the exact cause.
		auto fastFailure = request(core::agent_protocol::codex_jsonl_exec);
		fastFailure.Arguments.emplace_back("--fast-remote-failure");
		const auto fastFailureResult = launcher.run(fastFailure, 20'000);
		expect(
			fastFailureResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& fastFailureResult.ProtocolFailure
					== application::agent_protocol_failure::remote_failed
				&& fastFailureResult.Events.empty(),
			"a refusal read in the terminal drain keeps its exact cause");

		auto oversized = request(core::agent_protocol::claude_stream_json);
		oversized.Arguments.emplace_back("--oversize-frame");
		const auto oversizedResult = launcher.run(oversized, 20'000);
		expect(
			oversizedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& oversizedResult.Events.empty()
				&& oversizedResult.ProtocolFailure
					== application::agent_protocol_failure::frame_too_large,
			"an output frame beyond the bound fails without unbounded retention");

		// Both byte ceilings report the same cause, so each case is pinned to
		// the channel it flooded: the stdout course retains no standard error,
		// and the stderr course publishes no event.
		auto flooded = request(core::agent_protocol::external_terminal);
		flooded.Arguments.emplace_back("--stdout-flood");
		const auto floodedResult = launcher.run(flooded, 20'000);
		expect(
			floodedResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& floodedResult.ProtocolFailure
					== application::agent_protocol_failure::output_too_large
				&& floodedResult.Events.empty()
				&& floodedResult.StandardError.empty(),
			"stdout past the total ceiling is refused as its own exact cause");

		auto floodedError = request(core::agent_protocol::external_terminal);
		floodedError.Arguments.emplace_back("--stderr-flood");
		const auto floodedErrorResult = launcher.run(floodedError, 20'000);
		expect(
			floodedErrorResult.Outcome
					== application::agent_run_outcome::protocol_failure
				&& floodedErrorResult.ProtocolFailure
					== application::agent_protocol_failure::output_too_large
				&& floodedErrorResult.Events.empty()
				&& floodedErrorResult.StandardError.size()
					<= application::maximum_agent_stderr_bytes,
			"retained stderr stops at its ceiling instead of growing to the flood");

		const auto refusedBySink = launcher.run(
			request(core::agent_protocol::external_terminal),
			20'000,
			[](const application::agent_stream_event&) {
				throw std::runtime_error{ "consumer refused output" };
			});
		expect(
			refusedBySink.Outcome
					== application::agent_run_outcome::protocol_failure
				&& refusedBySink.Events.empty(),
			"the synchronous sink applies backpressure and can refuse publication");

		auto cancelled = request(core::agent_protocol::codex_app_server);
		cancelled.Arguments.emplace_back("--wait-forever");
		auto preCancelled = std::stop_source{ };
		static_cast<void>(preCancelled.request_stop());
		const auto preCancelledResult = launcher.run(
			cancelled,
			10'000,
			{ },
			preCancelled.get_token());
		expect(
			preCancelledResult.Outcome
				== application::agent_run_outcome::cancelled,
			"an invocation-specific cancellation cannot be reset before process start");
		auto cancelThread = std::jthread{ [&launcher] {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 150 });
			launcher.cancel();
		} };
		const auto cancelledResult = launcher.run(cancelled, 10'000);
		expect(
			cancelledResult.Outcome == application::agent_run_outcome::cancelled
				&& cancelledResult.Events.empty(),
			"explicit cancellation is distinct and retains no partial final answer");
		cancelThread.join();

		auto timedOut = request(core::agent_protocol::external_terminal);
		timedOut.Arguments.emplace_back("--wait-forever");
		const auto timeoutResult = launcher.run(timedOut, 250);
		expect(
			timeoutResult.Outcome == application::agent_run_outcome::timeout
				&& timeoutResult.Events.empty(),
			"deadline expiry is timeout, not explicit cancellation");

		const auto recovered = launcher.run(
			request(core::agent_protocol::external_terminal), 20'000);
		expect(
			recovered.Outcome == application::agent_run_outcome::success,
			"the same owner launches cleanly after failures, cancellation, and timeout");

		auto moved       = session().Binding;
		moved.HeadOid    = commit(9U);
		const auto stale = application::finalize_agent_run(
			recovered, session(), moved);
		expect(
			stale.Outcome == application::agent_run_outcome::stale_result
				&& stale.Events.empty(),
			"head movement rejects a successful old-revision result as stale");
	}

	void test_timeout_terminates_the_descendant_process_tree()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		std::error_code ignored;
		// [[BPR-1128]], [[BPR-1138]]. The descendant writes its heartbeat into a
		// directory this course made at a name nothing had, owner-private.
		// Nothing is removed before it is written, no link at a known name is
		// written through, and the directory goes, marker and all, however the
		// course ends.
		const auto                      directory = bpr::tests::make_scratch_directory("bpr-agent-descendant");
		const bpr::tests::scratch_guard ownedDirectory{ directory };
		const auto                      marker = directory / "heartbeat";

		auto launched = request(core::agent_protocol::external_terminal);
		launched.Arguments.insert(
			launched.Arguments.end(),
			{ "--spawn-descendant", marker.string(), "--wait-forever" });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 750);
		expect(
			result.Outcome == application::agent_run_outcome::timeout,
			"the descendant fixture reaches the bounded timeout");
		expect(
			std::filesystem::exists(marker),
			"the descendant wrote its heartbeat, so teardown evidence is non-vacuous");
		const auto stoppedSize = std::filesystem::file_size(marker);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
		expect(
			std::filesystem::file_size(marker) == stoppedSize,
			"the descendant heartbeat stops with its process group - no child leaks");
		std::filesystem::remove(marker, ignored);
	}

#if defined(Q_OS_UNIX)
	[[nodiscard]] auto make_workspace_guard(const std::filesystem::path& aPath) -> application::agent_workspace_lifetime_guard
	{
		{
			std::ofstream file{ aPath };
			file << "fixture-owned workspace guard\n";
		}
		std::filesystem::permissions(aPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
		struct stat identity{ };
		expect(::lstat(aPath.c_str(), &identity) == 0, "capture exact fixture guard identity");
		return { .Path = aPath.string(), .FileIdentity = { 1U, static_cast<std::uint64_t>(identity.st_dev), 0U, static_cast<std::uint64_t>(identity.st_ino) } };
	}

	void test_workspace_guard(const std::filesystem::path& aRoot)
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto launched                   = request(core::agent_protocol::external_terminal);
		launched.WorkspaceLifetimeGuard = make_workspace_guard(aRoot / "guard");
		const auto marker               = aRoot / "guard-side-effect";
		launched.Arguments.insert(launched.Arguments.end(), { "--pre-assignment-side-effect", marker.string(), "--probe-inherited-fd", "4" });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 20'000);
		if (result.Outcome != application::agent_run_outcome::success) {
			std::cerr << "guarded launch outcome " << static_cast<int>(result.Outcome) << ": " << result.StandardError << '\n';
		}
		expect(result.Outcome == application::agent_run_outcome::success && std::filesystem::exists(marker)
				   && std::ranges::any_of(result.Events, [](const auto& aEvent) { return aEvent.Text == "inherited-fd-closed"; }),
			   "guarded launch succeeds without exposing the guard descriptor to the agent");
		std::filesystem::remove(marker);
		const auto lock = ::open((aRoot / "guard").c_str(), O_RDONLY | O_CLOEXEC);
		expect(lock >= 0 && ::flock(lock, LOCK_EX | LOCK_NB) == 0, "completed launch releases lifetime exclusion");
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "recovery's exclusive lock refuses launch before any child side effect");
		static_cast<void>(::close(lock));
		std::filesystem::rename(aRoot / "guard", aRoot / "original");
		static_cast<void>(make_workspace_guard(aRoot / "guard"));
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "same-path guard substitution refuses before execution");
		std::filesystem::remove(aRoot / "guard");
		std::filesystem::create_symlink(aRoot / "original", aRoot / "guard");
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "guard symlink refuses before execution");
		std::cout << "agent-workspace-guard:pass\n";
	}
#endif

#if defined(Q_OS_WIN)
	namespace win_guard = adapters::agent::detail;

	[[nodiscard]] auto utf8_path(const std::filesystem::path& aPath) -> std::string
	{
		const auto text = aPath.u8string();
		return std::string{ text.begin(), text.end() };
	}

	// [[BPR-0622]]. A fixture guard created exactly as a workspace journal is:
	// owner and a single full grant for the current user (and package when
	// contained), protected, single link.
	[[nodiscard]] auto make_workspace_guard(const std::filesystem::path& aPath) -> application::agent_workspace_lifetime_guard
	{
		const auto principals = win_guard::windows_private_principals::load();
		expect(principals.has_value(), "load the current token's principals");
		auto descriptor = win_guard::windows_private_descriptor::make(*principals, false);
		expect(descriptor.has_value(), "build the private guard descriptor");
		const auto file = win_guard::windows_handle{ ::CreateFileW(
			aPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, descriptor->attributes(), CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr) };
		expect(static_cast<bool>(file), "create the private fixture guard");
		constexpr std::string_view text{ "fixture-owned workspace guard\n" };
		auto                       written = DWORD{ 0U };
		expect(::WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) != FALSE, "write fixture guard");
		const auto identity = win_guard::windows_regular_file_identity(file.get());
		expect(identity.has_value(), "capture exact fixture guard identity");
		return { .Path = utf8_path(aPath), .FileIdentity = *identity };
	}

	[[nodiscard]] auto hold_exclusive(const application::agent_workspace_lifetime_guard& aGuard) -> win_guard::windows_handle
	{
		return win_guard::open_windows_workspace_guard(std::filesystem::path{ std::u8string{ aGuard.Path.begin(), aGuard.Path.end() } }.wstring(), aGuard.FileIdentity, win_guard::windows_guard_lock::exclusive);
	}

	[[nodiscard]] auto has_event(const application::agent_run_result& aResult, const std::string_view aText) -> bool
	{
		return std::ranges::any_of(aResult.Events, [&](const auto& aEvent) { return aEvent.Text == aText; });
	}

	[[nodiscard]] auto count_event(const application::agent_run_result& aResult, const std::string_view aPrefix) -> std::optional<unsigned long>
	{
		for (const auto& event : aResult.Events) {
			if (event.Text.starts_with(aPrefix)) {
				return std::stoul(event.Text.substr(aPrefix.size()));
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] auto share_with_everyone(const std::filesystem::path& aPath) -> bool
	{
		PACL                 current    = nullptr;
		PSECURITY_DESCRIPTOR descriptor = nullptr;
		if (::GetNamedSecurityInfoW(aPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &current, nullptr, &descriptor) != ERROR_SUCCESS) {
			return false;
		}
		SID_IDENTIFIER_AUTHORITY world    = SECURITY_WORLD_SID_AUTHORITY;
		PSID                     everyone = nullptr;
		if (::AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone) == FALSE) {
			static_cast<void>(::LocalFree(descriptor));
			return false;
		}
		auto grant                 = EXPLICIT_ACCESSW{ };
		grant.grfAccessPermissions = GENERIC_READ;
		grant.grfAccessMode        = GRANT_ACCESS;
		grant.grfInheritance       = NO_INHERITANCE;
		grant.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
		grant.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
		grant.Trustee.ptstrName    = static_cast<LPWSTR>(everyone);
		PACL widened               = nullptr;
		auto status                = ::SetEntriesInAclW(1, &grant, current, &widened);
		if (status == ERROR_SUCCESS) {
			auto path = aPath.wstring();
			status    = ::SetNamedSecurityInfoW(path.data(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, widened, nullptr);
		}
		static_cast<void>(::LocalFree(widened));
		static_cast<void>(::FreeSid(everyone));
		static_cast<void>(::LocalFree(descriptor));
		return status == ERROR_SUCCESS;
	}

	void test_workspace_guard(const std::filesystem::path& aRoot)
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		const auto guardPath            = aRoot / "guard";
		const auto original             = make_workspace_guard(guardPath);
		auto       launched             = request(core::agent_protocol::external_terminal);
		launched.WorkspaceLifetimeGuard = original;
		const auto marker               = aRoot / "guard-side-effect";
		launched.Arguments.insert(launched.Arguments.end(), { "--pre-assignment-side-effect", utf8_path(marker), "--probe-inherited-handles", utf8_path(guardPath) });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 20'000);
		if (result.Outcome != application::agent_run_outcome::success) {
			std::cerr << "guarded launch outcome " << static_cast<int>(result.Outcome) << ": " << result.StandardError << '\n';
		}
		expect(result.Outcome == application::agent_run_outcome::success && std::filesystem::exists(marker),
			   "guarded launch succeeds through the Windows supervisor");
		// The same fixture launched unsupervised is the baseline: whatever job,
		// port or extra pipe handles a process owns for itself appear there too.
		auto baselineRequest = launched;
		baselineRequest.WorkspaceLifetimeGuard.reset();
		const auto baseline = launcher.run(baselineRequest, 20'000);
		expect(baseline.Outcome == application::agent_run_outcome::success, "unsupervised census baseline launch succeeds");
		for (const auto* const name : { "handle-count-job\t", "handle-count-port\t", "handle-count-extra-pipe\t" }) {
			const auto supervised   = count_event(result, name);
			const auto unsupervised = count_event(baseline, name);
			std::cout << "agent-workspace-guard:census\t" << name << (supervised ? std::to_string(*supervised) : std::string{ "absent" }) << " unsupervised " << (unsupervised ? std::to_string(*unsupervised) : std::string{ "absent" }) << '\n';
			expect(supervised.has_value() && supervised == unsupervised, "the supervised agent inherits no job, port or owner pipe handle beyond its own");
		}
		expect(has_event(result, "inherited-guard-closed"), "the supervised agent holds no workspace guard handle");
		std::filesystem::remove(marker);
		std::filesystem::remove(marker);
		{
			const auto exclusive = hold_exclusive(original);
			expect(static_cast<bool>(exclusive), "completed launch releases lifetime exclusion");
			expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
					   && !std::filesystem::exists(marker),
				   "recovery's exclusive lock refuses launch before any child side effect");
		}
		std::filesystem::rename(guardPath, aRoot / "original");
		static_cast<void>(make_workspace_guard(guardPath));
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "same-path guard substitution refuses before execution");
		std::filesystem::remove(guardPath);
		expect(::CreateHardLinkW(guardPath.c_str(), (aRoot / "original").c_str(), nullptr) != FALSE, "create a second link to the guard");
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "a guard with two links refuses before execution");
		std::filesystem::remove(guardPath);
		if (::CreateSymbolicLinkW(guardPath.c_str(), (aRoot / "original").c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE) != FALSE) {
			expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
					   && !std::filesystem::exists(marker),
				   "guard symlink refuses before execution");
			std::filesystem::remove(guardPath);
			std::cout << "agent-workspace-guard:symlink-refused\n";
		} else {
			// Symbolic-link creation needs Developer Mode or the privilege; the
			// reparse refusal is still covered by the ownership journal course.
			std::cout << "agent-workspace-guard:symlink-unavailable\t" << ::GetLastError() << '\n';
		}
		std::filesystem::rename(aRoot / "original", guardPath);
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::success, "the restored exact guard launches again");
		std::filesystem::remove(marker);
		expect(share_with_everyone(guardPath), "widen the guard's DACL");
		expect(launcher.run(launched, 20'000).Outcome == application::agent_run_outcome::failed_to_start
				   && !std::filesystem::exists(marker),
			   "a guard whose DACL is not private refuses before execution");
		std::cout << "agent-workspace-guard:pass\n";
	}

	// [[BPR-0622]]. The supervisor's job allows no breakaway: a descendant that
	// asks for CREATE_BREAKAWAY_FROM_JOB is refused, lives inside the job, and
	// dies at teardown. This course runs inside whatever job the test launcher
	// already placed this process in, so the supervisor's job is nested.
	void test_breakaway_descendant_dies_with_supervised_job()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		// An owner already inside a job (a test runner, a terminal, a launcher)
		// must still get a working supervised job nested under it. This process
		// joins a plain outer job first; the course is the last in the run.
		auto alreadyInJob = BOOL{ FALSE };
		static_cast<void>(::IsProcessInJob(::GetCurrentProcess(), nullptr, &alreadyInJob));
		static const auto outer = win_guard::windows_handle{ ::CreateJobObjectW(nullptr, nullptr) };
		expect(static_cast<bool>(outer) && ::AssignProcessToJobObject(outer.get(), ::GetCurrentProcess()) != FALSE,
			   "the owner joins an outer job before launching");
		std::cout << "agent-breakaway:owner-already-in-job\t" << (alreadyInJob != FALSE ? 1 : 0) << '\n';
		// [[BPR-1138]]. Made at a name nothing had; [[BPR-1121]], owned from
		// here, so a failure below removes it.
		const auto                      root = bpr::tests::make_scratch_directory("bpr-agent-breakaway");
		const bpr::tests::scratch_guard ownedRoot{ root };

		const auto marker               = root / "heartbeat";
		auto       launched             = request(core::agent_protocol::external_terminal);
		launched.WorkspaceLifetimeGuard = make_workspace_guard(root / "guard");
		launched.Arguments.insert(launched.Arguments.end(), { "--spawn-breakaway-descendant", utf8_path(marker), "--wait-forever" });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 1'500);
		expect(result.Outcome == application::agent_run_outcome::timeout, "the breakaway fixture reaches the bounded timeout");
		auto nested = BOOL{ FALSE };
		static_cast<void>(::IsProcessInJob(::GetCurrentProcess(), nullptr, &nested));
		std::cout << "agent-breakaway:owner-in-job\t" << (nested != FALSE ? 1 : 0) << '\n';
		expect(nested != FALSE, "the owner ran the supervised launch from inside a job");
		QElapsedTimer started;
		started.start();
		while (!std::filesystem::exists(marker) && started.elapsed() < 2'000) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
		}
		expect(std::filesystem::exists(marker), "the breakaway descendant wrote its heartbeat before teardown");
		const auto answer = [&] {
			auto stream = std::ifstream{ marker.string() + ".breakaway", std::ios::binary };
			return std::string{ std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
		}();
		std::cout << "agent-breakaway:answer\t" << answer << '\n';
		expect(answer.starts_with("refused "), "the supervised job refuses a descendant's breakaway request");
		const auto stopped = std::filesystem::file_size(marker);
		std::this_thread::sleep_for(std::chrono::milliseconds{ 250 });
		expect(std::filesystem::file_size(marker) == stopped, "the breakaway descendant stops with the supervised job");
		{
			const auto exclusive = hold_exclusive(*launched.WorkspaceLifetimeGuard);
			expect(static_cast<bool>(exclusive), "teardown releases lifetime exclusion after the job is empty");
		}
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
		std::cout << "agent-breakaway:pass\n";
	}
#endif

	void run_parent_death_course(const std::string_view aPhase, const std::filesystem::path& aRoot)
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		auto launched             = request(core::agent_protocol::external_terminal);
		launched.WorkingDirectory = aRoot.string();
		auto launcher             = adapters::agent::agent_process_launcher{ };
		if (aPhase == "--recover-agent-parent") {
			const auto recovered = launcher.run(launched, 20'000);
			if (recovered.Outcome != application::agent_run_outcome::success) {
				std::cerr << "recovery launch outcome " << static_cast<int>(recovered.Outcome) << ": " << recovered.StandardError << '\n';
			}
			expect(recovered.Outcome == application::agent_run_outcome::success, "fresh launcher succeeds after abrupt owner death");
			std::cout << "agent-parent-recovery:pass\n";
			return;
		}
		expect(aPhase == "--kill-agent-parent", "known parent-death course");
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		launched.WorkspaceLifetimeGuard = make_workspace_guard(aRoot / "workspace.guard");
#endif

		launched.Arguments.insert(launched.Arguments.end(), { "--spawn-descendant", (aRoot / "heartbeat").string(), "--report-process-id", "--wait-forever" });
		static_cast<void>(launcher.run(launched, 60'000, [&](const application::agent_stream_event& aEvent) {
			// [[BPR-1121]]. The launcher calls this and catches what it throws, so a
			// failure here still ends the process rather than a launch.
			const bpr::tests::exit_on_failure insideTheLauncher;
			if (aEvent.Text.starts_with("supervisor-id\t")) {
				std::ofstream supervisor{ aRoot / "supervisor.pid" };
				supervisor << aEvent.Text.substr(std::string_view{ "supervisor-id\t" }.size());
				supervisor.close();
				expect(supervisor.good(), "publish supervisor PID");
			}
			if (aEvent.Text.starts_with("descendant-id\t")) {
				std::ofstream descendant{ aRoot / "descendant.pid" };
				descendant << aEvent.Text.substr(std::string_view{ "descendant-id\t" }.size());
				descendant.close();
				expect(descendant.good(), "publish descendant PID");
			}
			if (aEvent.Text.starts_with("process-id\t")) {
				std::ofstream pid{ aRoot / "agent.pid" };
				pid << aEvent.Text.substr(std::string_view{ "process-id\t" }.size());
				pid.close();
				expect(pid.good(), "publish fixture agent PID");
				std::ofstream ready{ aRoot / "kill-ready" };
				ready << aPhase;
				ready.close();
				expect(ready.good(), "publish live agent kill checkpoint");
			}
		}));
		expect(false, "owner must be forcibly killed while its agent is running");
	}

#if defined(Q_OS_UNIX)
	void test_supervisor_closes_unrelated_descriptors()
	{
		int channel[2]{ -1, -1 };
		expect(::pipe(channel) == 0, "create unrelated inherited descriptor fixture");
		const auto descriptor = ::fcntl(channel[0], F_DUPFD, 128);
		expect(descriptor >= 128, "create a non-close-on-exec high descriptor");
		auto launched = request(core::agent_protocol::external_terminal);
		launched.Arguments.insert(launched.Arguments.end(), { "--probe-inherited-fd", std::to_string(descriptor), "--probe-owner-channel" });
		auto       launcher = adapters::agent::agent_process_launcher{ };
		const auto result   = launcher.run(launched, 20'000);
		static_cast<void>(::close(descriptor));
		static_cast<void>(::close(channel[0]));
		static_cast<void>(::close(channel[1]));
		expect(result.Outcome == application::agent_run_outcome::success
				   && std::ranges::any_of(result.Events, [](const auto& aEvent) { return aEvent.Text == "inherited-fd-closed"; })
				   && std::ranges::none_of(result.Events, [](const auto& aEvent) { return aEvent.Text == "inherited-fd-open" || aEvent.Text == "owner-channel-open"; })
				   && std::ranges::any_of(result.Events, [](const auto& aEvent) { return aEvent.Text == "owner-channel-closed"; }),
			   "supervised agent cannot inherit an unrelated application descriptor");
	}
#endif

#if defined(Q_OS_WIN) && defined(BPR_BUILD_TESTING)
	void test_windows_assignment_failure_has_no_pre_assignment_side_effect()
	{
		qputenv("BPR_CANARY_AGENT_IDENTITY", QByteArray{ "allowed" });
		// [[BPR-1138]]. The marker the child must not write goes inside a
		// directory made at a name nothing had, so nothing is removed before
		// the check; [[BPR-1121]], owned from here, so a failure below removes
		// it.
		const auto                      directory = bpr::tests::make_scratch_directory("bpr-agent-pre-assignment");
		const bpr::tests::scratch_guard ownedDirectory{ directory };
		const auto                      marker = directory / "side-effect";
		std::error_code                 ignored;

		auto launched = request(core::agent_protocol::external_terminal);
		launched.Arguments.insert(
			launched.Arguments.end(),
			{ "--pre-assignment-side-effect", marker.string() });

		auto launcher = adapters::agent::agent_process_launcher{ };
		adapters::agent::agent_process_launcher::
			force_job_assignment_failure_for_testing(true);
		const auto result = launcher.run(launched, 20'000);
		adapters::agent::agent_process_launcher::
			force_job_assignment_failure_for_testing(false);
		expect(
			result.Outcome == application::agent_run_outcome::failed_to_start
				&& !result.StandardError.empty() && !result.ExitCode.has_value()
				&& result.Events.empty() && !std::filesystem::exists(marker),
			"forced Job Object assignment failure leaves the suspended child without a side effect");

		const auto recovered = launcher.run(
			request(core::agent_protocol::external_terminal), 20'000);
		expect(
			recovered.Outcome == application::agent_run_outcome::success,
			"the native launcher recovers after a pre-resume assignment refusal");
		std::filesystem::remove(marker, ignored);
	}
#endif
} // namespace

auto main(int argc, char** argv) -> int
try {
	QCoreApplication application{ argc, argv };
	// [[BPR-1229]]. The durable course opens a private SQLite store under a
	// scratch root, which the Windows AppContainer refuses, so it is its own
	// row (qt_agent_launch_canary_durable_course) on every platform and the
	// launch courses below keep their network boundary.
	if (argc == 2 && std::string_view{ argv[1] } == "--durable-course") {
		test_durable_private_input_crosses_only_after_admission();
		std::cout << "agent-launch-durable-course-tests:pass\n";
		return EXIT_SUCCESS;
	}
	if (argc == 3) {
#if defined(Q_OS_UNIX) || defined(Q_OS_WIN)
		if (std::string_view{ argv[1] } == "--workspace-guard") {
			test_workspace_guard(std::filesystem::canonical(argv[2]));
			return EXIT_SUCCESS;
		}
#endif
		run_parent_death_course(argv[1], std::filesystem::canonical(argv[2]));
		return EXIT_SUCCESS;
	}
	test_input_budget_refuses_before_real_child_effect();
	test_all_four_protocols_stream_partial_frames_under_stderr_pressure();
	test_a_catalog_run_lists_models_through_a_real_child();
	test_output_before_input_does_not_stall_the_request();
	test_private_claim_waits_for_buffered_input_delivery();
	test_real_child_receives_no_parent_credential_or_context_in_argv();
	test_independent_agent_identity_alone_controls_canary_authority();
	test_every_terminal_condition_is_typed_and_failed_runs_publish_no_result();
	test_timeout_terminates_the_descendant_process_tree();
#if defined(Q_OS_UNIX)
	test_supervisor_closes_unrelated_descriptors();
#endif
#if defined(Q_OS_WIN) && defined(BPR_BUILD_TESTING)
	test_windows_assignment_failure_has_no_pre_assignment_side_effect();
#endif
#if defined(Q_OS_WIN)
	test_breakaway_descendant_dies_with_supervised_job();
#endif
	std::cout << "agent-launch-canary-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
