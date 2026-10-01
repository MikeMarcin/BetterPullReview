// [[BPR-0970]]. AI structured output, agent half: what a launched agent
// writes back (protocol frames, structured findings, writing results), and
// what an agent asks of the MCP context helper.
//
// Input: [selector] [parameter] then the payload. The MCP cases run against a
// small frozen snapshot made once per process, with a file outside it that a
// symbolic link inside points at; no answer may ever carry that file's text.

#include "bpr/adapters/mcp/mcp_context_helper.hpp"
#include "bpr/application/agent_launch.hpp"
#include "bpr/application/writing_hooks.hpp"
#include "fuzz_support.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	constexpr std::string_view outside_sentinel = "bpr-fuzz-outside-the-snapshot";

	constexpr std::array protocols{
		core::agent_protocol::codex_app_server,
		core::agent_protocol::codex_jsonl_exec,
		core::agent_protocol::claude_stream_json,
		core::agent_protocol::external_terminal,
	};

	auto commit(const char aDigit) -> core::git_commit_oid
	{
		const auto parsed = core::parse_git_commit_oid(std::string(40U, aDigit));
		fuzz::require(parsed.has_value());
		return *parsed;
	}

	auto pinned() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit('1'),
			.HeadOid     = commit('2'),
			.LeftEpoch   = core::epoch_ordinal{ 1 },
			.RightEpoch  = core::epoch_ordinal{ 2 },
		};
	}

	auto write_file(const std::filesystem::path& aPath, const std::string_view aText) -> void
	{
		std::filesystem::create_directories(aPath.parent_path());
		std::ofstream stream{ aPath, std::ios::binary | std::ios::trunc };
		stream.write(aText.data(), static_cast<std::streamsize>(aText.size()));
		fuzz::require(stream.good());
	}

	// A private directory per process: a frozen snapshot, an alignment-only
	// snapshot the alignment case rewrites, and the outside file.
	struct snapshot_files
	{
		snapshot_files()
		{
			auto pattern = (std::filesystem::temp_directory_path() / "bpr-fuzz-mcp-XXXXXX").string();
			fuzz::require(::mkdtemp(pattern.data()) != nullptr);
			Root                = pattern;
			const auto identity = [](const char aDigit) { return std::string(64U, aDigit); };
			write_file(Root / "snapshot" / "binding", adapters::mcp::encode_revision_binding(pinned()) + "\n");
			write_file(Root / "snapshot" / "alignment",
					   "file\tsrc/retry.cpp\ncomparison\t" + identity('a') + "\nleft_line_count\t42\nright_line_count\t44\nsegment\t" + identity('b')
						   + "\ngroup\t" + identity('c') + "\nmove\t" + identity('d') + "\n");
			write_file(Root / "snapshot" / "files" / "src" / "retry.cpp", "return retry();\n");
			write_file(Root / "outside" / "secret.txt", outside_sentinel);
			std::filesystem::create_directory_symlink(Root / "outside", Root / "snapshot" / "files" / "escape");
			write_file(Root / "alignment-only" / "binding", adapters::mcp::encode_revision_binding(pinned()) + "\n");
		}
		snapshot_files(const snapshot_files&)            = delete;
		snapshot_files& operator=(const snapshot_files&) = delete;
		~snapshot_files()
		{
			std::error_code ignored;
			std::filesystem::remove_all(Root, ignored);
		}

		std::filesystem::path Root;
	};

	auto snapshots() -> const snapshot_files&
	{
		static const snapshot_files instance;
		return instance;
	}

	auto findings(const std::string_view aPayload) -> void
	{
		for (const auto protocol : protocols) {
			const auto parsed = application::decode_agent_finding_payload(protocol, aPayload, core::agent_session_id{ 1 });
			const auto replay = application::decode_agent_finding_payload(protocol, aPayload, core::agent_session_id{ 1 });
			fuzz::require(parsed == replay);
			if (parsed) {
				fuzz::require(protocol != core::agent_protocol::external_terminal);
				fuzz::require(parsed->Session == core::agent_session_id{ 1 });
				fuzz::require(parsed->Findings.size() <= application::maximum_agent_findings_per_turn);
			}
		}
	}

	auto frames(fuzz::fuzz_input& aInput, const std::uint8_t aParameter) -> void
	{
		const auto protocol = protocols[aParameter % protocols.size()];
		// Frames arrive in arbitrary chunks; each separator in the payload is a
		// read boundary.
		application::agent_protocol_decoder decoder{ protocol, (aParameter & 0x10U) != 0U };
		auto                                remaining = aInput.text();
		while (true) {
			const auto at     = remaining.find(fuzz::part_separator);
			const auto chunk  = remaining.substr(0U, at);
			const auto events = decoder.push(chunk);
			if (!events) {
				return;
			}
			for (const auto& event : *events) {
				if (event.Kind == application::agent_protocol_event_kind::initialization_complete
					|| event.Kind == application::agent_protocol_event_kind::context_channel_ready) {
					static_cast<void>(decoder.acknowledge_followup(event.Kind));
				} else if (event.Kind == application::agent_protocol_event_kind::finding_payload) {
					static_cast<void>(application::decode_agent_finding_payload(protocol, event.Text, core::agent_session_id{ 1 }));
				}
			}
			if (at == std::string_view::npos) {
				break;
			}
			remaining.remove_prefix(at + fuzz::part_separator.size());
		}
		static_cast<void>(decoder.finish());
		static_cast<void>(decoder.assistant_items());
	}

	auto writing(const std::string_view aJson, const std::uint8_t aParameter) -> void
	{
		const std::array sources{
			application::writing_source{ "S1", "The retry loop never backs off.", std::nullopt },
			application::writing_source{ "S2", "Callers expect at most three attempts.", std::nullopt },
		};
		const auto hook   = static_cast<core::writing_hook>(aParameter % core::writing_hook_count);
		const auto result = application::decode_writing_result(aJson, hook, sources, (aParameter & 0x10U) != 0U);
		if (result) {
			// Every note cites only sources the request supplied.
			for (const auto& note : result->Notes) {
				for (const auto& reference : note.SourceRefs) {
					fuzz::require(reference == "S1" || reference == "S2");
				}
			}
		}
	}

	auto mcp_lines(const std::string_view aText) -> void
	{
		adapters::mcp::mcp_context_helper helper{ snapshots().Root / "snapshot", pinned() };
		auto                              remaining = aText;
		for (int line = 0; line < 8 && !remaining.empty(); ++line) {
			const auto end      = remaining.find('\n');
			const auto request  = remaining.substr(0U, end);
			const auto response = helper.handle_line(request);
			if (response) {
				// One line per answer, and nothing from outside the snapshot.
				// An answer echoes the request's identity, so a request that
				// itself carries the outside file's text proves nothing.
				fuzz::require(response->find('\n') == std::string::npos || response->find('\n') == response->size() - 1U);
				fuzz::require(request.find(outside_sentinel) != std::string_view::npos || response->find(outside_sentinel) == std::string::npos);
			}
			if (end == std::string_view::npos) {
				break;
			}
			remaining.remove_prefix(end + 1U);
		}
	}

	auto binding(const std::string_view aText) -> void
	{
		if (const auto decoded = adapters::mcp::decode_revision_binding(aText)) {
			fuzz::require(adapters::mcp::decode_revision_binding(adapters::mcp::encode_revision_binding(*decoded)) == decoded);
		}
	}

	auto alignment(const std::string_view aText) -> void
	{
		const auto root = snapshots().Root / "alignment-only";
		write_file(root / "alignment", aText);
		static_cast<void>(adapters::mcp::read_snapshot_alignment(root));
	}
}

// libFuzzer calls the entry point by this exact name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* aData, const std::size_t aSize) -> int
{
	fuzz::fuzz_input input{ aData, aSize };
	const auto       selector  = input.byte();
	const auto       parameter = input.byte();
	if (input.rest().size() > 65536U) {
		return 0;
	}
	switch (selector % 6U) {
	case 0U:
		findings(input.text());
		break;
	case 1U:
		frames(input, parameter);
		break;
	case 2U:
		writing(input.text(), parameter);
		break;
	case 3U:
		mcp_lines(input.text());
		break;
	case 4U:
		binding(input.text());
		break;
	default:
		alignment(input.text());
		break;
	}
	return 0;
}
