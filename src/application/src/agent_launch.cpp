#include "bpr/application/agent_launch.hpp"

#include <utility>

namespace bpr::application {
	namespace {
		void append_field(
			std::string&           aOutput,
			const std::string_view aName,
			const std::string_view aValue)
		{
			aOutput += aName;
			aOutput += '\t';
			// Newlines and tabs are the record and field separators, so a value
			// carrying one would forge a record. Escaped rather than rejected:
			// a reviewer's note may legitimately contain both.
			for (const auto character : aValue) {
				switch (character) {
				case '\n':
					aOutput += "\\n";
					break;
				case '\t':
					aOutput += "\\t";
					break;
				case '\\':
					aOutput += "\\\\";
					break;
				default:
					aOutput += character;
					break;
				}
			}
			aOutput += '\n';
		}
	} // namespace

	auto materialize_agent_environment(
		const core::agent_profile& aProfile,
		const environment_reader&  aParent)
		-> std::vector<std::pair<std::string, std::string>>
	{
		auto result = std::vector<std::pair<std::string, std::string>>{ };
		if (!aParent) {
			return result;
		}
		result.reserve(aProfile.AllowedEnvironment.size());
		// Iterating the allowlist, never the parent. There is deliberately no
		// way to enumerate the parent's environment from here: a variable the
		// profile did not name cannot be reached, so it cannot be copied.
		for (const auto& name : aProfile.AllowedEnvironment) {
			if (auto value = aParent(name); value.has_value()) {
				result.emplace_back(name, std::move(*value));
			}
		}
		return result;
	}

	auto encode_agent_supplement(
		const core::agent_context_supplement& aSupplement) -> std::string
	{
		auto result = std::string{ };
		append_field(
			result, "schema", std::to_string(aSupplement.SchemaVersion));
		append_field(
			result,
			"host",
			std::to_string(aSupplement.Binding.Host.value()));
		append_field(
			result,
			"repository",
			std::to_string(aSupplement.Binding.Repository.value()));
		append_field(
			result,
			"pull_request",
			std::to_string(aSupplement.Binding.PullRequest.value()));
		append_field(
			result, "base", core::canonical_hex(aSupplement.Binding.BaseOid).view());
		append_field(
			result, "head", core::canonical_hex(aSupplement.Binding.HeadOid).view());
		append_field(
			result,
			"left_epoch",
			std::to_string(aSupplement.Binding.LeftEpoch.value()));
		append_field(
			result,
			"right_epoch",
			std::to_string(aSupplement.Binding.RightEpoch.value()));
		for (const auto& note : aSupplement.Notes) {
			append_field(result, "note", note.Text);
		}
		return result;
	}

	auto build_agent_launch(
		const core::agent_profile&            aProfile,
		const core::agent_context_supplement& aSupplement,
		const environment_reader&             aParent) -> agent_launch_request
	{
		// `Arguments` carries the protocol switch and nothing derived from the
		// supplement. There is no branch below that reads `aSupplement` into
		// it, which is what makes the argv rule a property of this function
		// rather than a convention its callers keep.
		auto arguments = std::vector<std::string>{ };
		switch (aProfile.Protocol) {
		case core::agent_protocol::codex_app_server:
			arguments.emplace_back("app-server");
			break;
		case core::agent_protocol::codex_jsonl_exec:
			arguments.emplace_back("exec");
			arguments.emplace_back("--json");
			break;
		case core::agent_protocol::claude_stream_json:
			arguments.emplace_back("--output-format");
			arguments.emplace_back("stream-json");
			break;
		case core::agent_protocol::external_terminal:
			break;
		}
		return {
			.Executable  = aProfile.Executable,
			.Arguments   = std::move(arguments),
			.Environment = materialize_agent_environment(aProfile, aParent),
			.Input       = encode_agent_supplement(aSupplement),
		};
	}
} // namespace bpr::application
