// A fixture agent that answers with one structured finding anchored at the
// semantic identity the composed context disclosed to it.
//
// The existing `tests/fixtures/agent/agent_canary_main.cpp` emits a finding
// whose anchor is a fixed repeated hex digit, which is exactly right for
// proving that an unanchored answer still reaches the reviewer. It cannot
// prove the other half: that a finding the agent placed correctly validates
// against the frozen snapshot the host composed and promotes into a local
// draft. This one reads the context it was handed - `name<TAB>value` lines,
// the same payload `encode_agent_supplement` wrote - and anchors its finding at
// `selection.preferred`, which is the exact `core::source_line_ref` the
// reviewer selected.
//
// It speaks Codex JSONL because that protocol has a `turn.completed` frame a
// payload can ride on. The external terminal has none, and refusing one there
// is a property BPR-0500 already proves.

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace {
	using context_fields = std::map<std::string, std::string, std::less<>>;

	[[nodiscard]] auto read_all_input() -> std::string
	{
		auto buffer = std::ostringstream{ };
		buffer << std::cin.rdbuf();
		return buffer.str();
	}

	/// \brief Parses every `name<TAB>value` line the payload carries.
	///
	/// \details Lines before the first tab-bearing one - the protocol's own
	/// `BPR-CONTEXT/2 CODEX-EXEC` header - are skipped rather than refused, so
	/// this stays a reader of the supplement rather than a second decoder of
	/// the framing around it.
	[[nodiscard]] auto parse_context(const std::string_view aPayload)
		-> context_fields
	{
		auto fields = context_fields{ };
		auto begin  = std::size_t{ 0U };
		while (begin < aPayload.size()) {
			auto end = aPayload.find('\n', begin);
			if (end == std::string_view::npos) {
				end = aPayload.size();
			}
			const auto line = aPayload.substr(begin, end - begin);
			const auto tab  = line.find('\t');
			if (tab != std::string_view::npos && tab != 0U) {
				fields.emplace(
					std::string{ line.substr(0U, tab) },
					std::string{ line.substr(tab + 1U) });
			}
			begin = end + 1U;
		}
		return fields;
	}

	[[nodiscard]] auto field(
		const context_fields&  aFields,
		const std::string_view aName,
		const std::string_view aFallback) -> std::string
	{
		const auto found = aFields.find(aName);
		return found == aFields.end() ? std::string{ aFallback } : found->second;
	}
} // namespace

auto main() -> int
{
	const auto payload  = read_all_input();
	const auto fields   = parse_context(payload);
	const auto session  = field(fields, "session", "1");
	const auto provider = field(fields, "profile", "2");
	const auto base     = field(fields, "base", std::string(40U, '1'));
	const auto head     = field(fields, "head", std::string(40U, '2'));
	const auto path     = field(fields, "file.0.path", "unknown");
	const auto comparison
		= field(fields, "selection.preferred.comparison", std::string(64U, '5'));
	const auto sideValue = field(fields, "selection.preferred.side", "1");
	const auto side      = sideValue == "0" ? "left" : "right";
	const auto lineValue = field(fields, "selection.preferred.line", "1");
	auto       endLine   = std::string{ "1" };
	try {
		endLine = std::to_string(std::stoull(lineValue) + 1U);
	} catch (...) {
		return EXIT_FAILURE;
	}

	// The one thing this fixture proposes, and the exact bytes the composed
	// promotion must carry through to the workshop's proposed side.
	constexpr std::string_view suggested
		= "BPR_FINDING_SUGGESTED_0504 name the exhausted retry in the message";

	auto findings = std::string{ };
	findings += "{\"session\":" + session + ",\"findings\":[{";
	findings += "\"provider\":" + provider;
	findings += ",\"severity\":\"major\",\"confidence\":\"high\"";
	findings += ",\"category\":\"correctness\"";
	findings += ",\"path\":\"" + path + "\"";
	findings += ",\"lines\":{\"begin\":" + lineValue + ",\"end\":" + endLine + "}";
	findings += ",\"anchor\":{\"line\":{\"comparison\":\"" + comparison
		+ "\",\"side\":\"" + side + "\",\"line\":" + lineValue + "}}";
	findings += ",\"claim\":\"BPR_FINDING_CLAIM_0504 the retry budget is not "
				"reported to the caller\"";
	findings += ",\"evidence\":\"BPR_FINDING_EVIDENCE_0504\"";
	findings += ",\"consequence\":\"BPR_FINDING_CONSEQUENCE_0504\"";
	findings += ",\"suggestedComment\":\"" + std::string{ suggested } + "\"";
	findings += ",\"requiredContext\":\"BPR_FINDING_REQUIRED_CONTEXT_0504\"";
	findings += ",\"analyzedBase\":\"" + base + "\"";
	findings += ",\"analyzedHead\":\"" + head + "\"";
	findings += ",\"analyzedCommits\":[\"" + head + "\"]}]}";

	std::cout << "{\"type\":\"thread.started\",\"thread_id\":\"finding-canary\"}\n"
				 "{\"type\":\"turn.started\"}\n"
				 "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\","
				 "\"type\":\"agent_message\",\"text\":\"finding-canary-complete\"}}\n"
				 "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,"
				 "\"cached_input_tokens\":0,\"output_tokens\":1},\"bprFindings\":"
			  << findings << "}\n";
	std::cout.flush();
	return EXIT_SUCCESS;
}
