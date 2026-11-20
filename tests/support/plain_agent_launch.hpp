#ifndef BPR_TESTS_SUPPORT_PLAIN_AGENT_LAUNCH_HPP
#define BPR_TESTS_SUPPORT_PLAIN_AGENT_LAUNCH_HPP

// [[BPR-2308]]. A launch of one plain prompt for launcher and fixture-agent
// tests. The retired Review Cycles' round launch used to be the shortest way
// to give a profile one message; this builds the Agent tab's launch through
// the shipped `build_agent_launch` instead, in the given folder, and replaces
// its input with the prompt framed as each protocol frames one message.

#include "bpr/application/agent_launch.hpp"
#include "bpr/core/agent_context_supplement.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::tests {
	[[nodiscard]] inline auto plain_prompt_supplement(const core::agent_profile_id aProfile) -> std::optional<core::agent_context_supplement>
	{
		auto oid = [](const std::uint8_t aSeed) {
			auto bytes = std::array<std::uint8_t, 32U>{ };
			for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
				bytes[index] = static_cast<std::uint8_t>(aSeed + index);
			}
			return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
		};
		auto digest = std::array<std::uint8_t, 32U>{ };
		digest.fill(5U);
		const auto anchor = core::source_line_ref{ .Comparison = core::file_comparison_digest{ digest }, .Side = core::diff_side::right, .Line = 17U };
		return core::build_agent_supplement({
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = {
				.Id      = core::agent_session_id{ 40U },
				.Profile = aProfile,
				.Binding = {
					.Host        = core::git_host_id{ 10U },
					.Repository  = core::repository_id{ 20U },
					.PullRequest = core::pull_request_id{ 30U },
					.BaseOid     = oid(1U),
					.HeadOid     = oid(2U),
					.LeftEpoch   = core::epoch_ordinal{ 1U },
					.RightEpoch  = core::epoch_ordinal{ 2U },
				},
			},
			.PullRequestUrl = "https://github.example/octo/app/pull/30",
			.SelectedDrafts = { { .Anchor = anchor, .Text = "Would a bounded retry be safer?" } },
		});
	}

	[[nodiscard]] inline auto plain_prompt_json(const std::string_view aText) -> std::string
	{
		auto escaped = std::string{ };
		for (const auto character : aText) {
			switch (character) {
			case '"':  escaped += "\\\""; break;
			case '\\': escaped += "\\\\"; break;
			case '\n': escaped += "\\n"; break;
			default:   escaped += character; break;
			}
		}
		return escaped;
	}

	[[nodiscard]] inline auto plain_agent_launch(const core::agent_profile& aProfile, const std::string_view aPrompt, std::string aWorkingDirectory,
												 const application::environment_reader& aParent)
		-> std::expected<application::agent_launch_request, application::agent_launch_failure>
	{
		const auto supplement = plain_prompt_supplement(aProfile.Id);
		if (!supplement) {
			return std::unexpected(application::agent_launch_failure::invalid_profile);
		}
		auto built = application::build_agent_launch(aProfile, *supplement, aParent);
		if (!built) {
			return built;
		}
		built->WorkingDirectory = std::move(aWorkingDirectory);
		switch (aProfile.Protocol) {
		case core::agent_protocol::claude_stream_json:
			built->Input = "{\"type\":\"user\",\"message\":{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"" + plain_prompt_json(aPrompt) + "\"}]}}\n";
			break;
		case core::agent_protocol::codex_jsonl_exec:
		case core::agent_protocol::external_terminal:
			built->Input = std::string{ aPrompt };
			break;
		case core::agent_protocol::codex_app_server:
			built->DeferredContext = std::string{ aPrompt };
			break;
		}
		return built;
	}
}

#endif
