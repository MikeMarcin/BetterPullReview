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

	/// \brief Counts one exact token in the frame this process was handed.
	///
	/// \details Counted over the whole received input rather than over the
	/// parsed fields, so a question placed twice - once in the supplement and
	/// once beside it in the framing - is two here and not one.
	[[nodiscard]] auto occurrences(
		const std::string_view aInput, const std::string_view aToken)
		-> std::size_t
	{
		if (aToken.empty()) {
			return 0U;
		}
		auto count = std::size_t{ 0U };
		auto at    = aInput.find(aToken);
		while (at != std::string_view::npos) {
			++count;
			at = aInput.find(aToken, at + aToken.size());
		}
		return count;
	}

	/// \brief Escapes the echoed message so a reply stays parseable JSON.
	[[nodiscard]] auto json_escaped(const std::string_view aValue) -> std::string
	{
		auto output = std::string{ };
		for (const auto character : aValue) {
			const auto byte = static_cast<unsigned char>(character);
			if (character == '"' || character == '\\') {
				output.push_back('\\');
				output.push_back(character);
			} else if (byte < 0x20U) {
				output += ' ';
			} else {
				output.push_back(character);
			}
		}
		return output;
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
	// What the reviewer typed, read back out of the frame this process was
	// handed, with the number of times it appeared in it. Reported rather than
	// asserted: this fixture says what it received, and the course that sent it
	// says what that had to be.
	const auto message = field(fields, "chat_message", "");
	const auto echoed  = std::to_string(occurrences(payload, message));
	// [[BPR-1245]]. Which private sources the turn was handed, and the
	// mutable draft and note fields beside them, reported the same way: the
	// workshop course says a press's only source is the revision that
	// recorded the draft. The body is the wire's escaped spelling, and its
	// first 256 bytes, so the claim stays inside the finding's bound.
	const auto sources = std::string{ " | BPR_PRIVATE_SOURCES_1245 count=" }
		+ field(fields, "private.count", "absent")
		+ " kind=" + field(fields, "private.0.kind", "absent")
		+ " drafts=" + field(fields, "draft.count", "absent")
		+ " notes=" + field(fields, "note.count", "absent")
		+ " body=" + field(fields, "private.0.body", "absent").substr(0U, 256U) + " end";
	// [[BPR-1091]]. The review-feedback guidance the turn was handed, if
	// any: where it came from, its version, its length on the wire and its
	// first 48 escaped bytes, reported the same way. A course with the label
	// helper off reads `source=absent`.
	const auto guidanceText = field(fields, "feedback_guidance.text", "");
	const auto guidance     = std::string{ " | BPR_FEEDBACK_GUIDANCE_1091 source=" }
		+ field(fields, "feedback_guidance.source", "absent")
		+ " version=" + field(fields, "feedback_guidance.version", "absent")
		+ " bytes=" + std::to_string(guidanceText.size())
		+ " head=" + guidanceText.substr(0U, 48U) + " end";
	// BPR-0538. One of the six is this fixture's silent operation: asked to
	// shorten, it completes its turn and proposes nothing. A real agent does
	// this whenever it has nothing to say about the comment, and it is the
	// only way a course can drive a turn that ends without a revision through
	// the shipped path rather than by cutting the path.
	if (field(fields, "workshop_operation", "") == "shorten") {
		std::cout
			<< "{\"type\":\"thread.started\",\"thread_id\":\"finding-canary\"}\n"
			   "{\"type\":\"turn.started\"}\n"
			   "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\","
			   "\"type\":\"agent_message\",\"text\":\"finding-canary-silent\"}}\n"
			   "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,"
			   "\"cached_input_tokens\":0,\"output_tokens\":1}}\n";
		std::cout.flush();
		return EXIT_SUCCESS;
	}
	auto endLine = std::string{ "1" };
	try {
		endLine = std::to_string(std::stoull(lineValue) + 1U);
	} catch (...) {
		return EXIT_FAILURE;
	}

	// The one thing this fixture proposes, and the exact bytes the composed
	// promotion must carry through to the workshop's proposed side.
	constexpr std::string_view unguided
		= "BPR_FINDING_SUGGESTED_0504 name the exhausted retry in the message";
	// [[BPR-1091]]. Handed the shipped guidance, this fixture does what it
	// asks: it labels the comment and keeps the reviewer's question. Handed a
	// profile's own guidance, it labels it a blocking issue and states it, so
	// a course can see the workshop name a semantic change. Neither is model
	// output; both are fixed bytes a course compares.
	constexpr std::string_view guidedShipped
		= "question: BPR_FINDING_SUGGESTED_1091 is the exhausted retry budget reported to the caller?";
	constexpr std::string_view guidedOverride
		= "issue (blocking): BPR_FINDING_SUGGESTED_1091 report the exhausted retry budget to the caller.";
	const auto source    = field(fields, "feedback_guidance.source", "absent");
	const auto suggested = source == "shipped" ? guidedShipped : source == "profile_override" ? guidedOverride
																							  : unguided;

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
				"reported to the caller"
				" | BPR_CHAT_ECHO_0517 count="
		+ echoed + " text=" + json_escaped(message) + json_escaped(sources) + json_escaped(guidance) + "\"";
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
