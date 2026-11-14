// [[BPR-2177]] (owner, 2026-11-14: "we need a fixture agent for testing why am
// I wasting a ton of money just seeing how buttons and UI flow works"). The
// fixture agent every tests-on attended build runs in place of a real one.
//
// Built once and copied as `claude` and as `codex` into the build's
// `fixture-agent` folder. Run as `claude` it speaks Claude Code's stream-JSON;
// as `codex` (or with `exec`), Codex's JSONL. It answers each built-in prompt
// and quick ask with a canned, fixture-shaped answer chosen by what the
// request holds, streams it in steps with short pauses so a look sees the run,
// Stop and the queue, and fails when asked to. It reads nothing but its own
// input, opens no connection, and costs nothing.
//
// Scripting: `BPR_FIXTURE_AGENT_FAIL=crash|usage-limit` in its environment,
// or `FIXTURE_CRASH` / `FIXTURE_USAGE_LIMIT` in the question. Pauses:
// `BPR_FIXTURE_AGENT_PAUSE_MS` (default 350), 0 for none.

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
	auto environment(const char* aName) -> std::string
	{
#if defined(_MSC_VER)
		char*       value = nullptr;
		std::size_t size  = 0;
		if (_dupenv_s(&value, &size, aName) != 0 || value == nullptr) {
			return { };
		}
		auto result = std::string{ value };
		std::free(value);
		return result;
#else
		const auto* const value = std::getenv(aName);
		return value == nullptr ? std::string{ } : std::string{ value };
#endif
	}

	auto escape(const std::string_view aText) -> std::string
	{
		auto result = std::string{ };
		for (const auto character : aText) {
			switch (character) {
			case '"':  result += "\\\""; break;
			case '\\': result += "\\\\"; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default:   result += character; break;
			}
		}
		return result;
	}

	void pause()
	{
		const auto configured = environment("BPR_FIXTURE_AGENT_PAUSE_MS");
		const auto millis     = configured.empty() ? 350 : std::atoi(configured.c_str());
		if (millis > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ millis });
		}
	}

	auto holds(const std::string& aText, const std::string_view aWord) -> bool { return aText.find(aWord) != std::string::npos; }

	/// The canned answer for what the request asks.
	auto answer_for(const std::string& aRequest) -> std::string
	{
		if (holds(aRequest, "returning.operation")) {
			// RFC-BPR-0128 as amended (look ask-v005): several threads in one
			// ask, a claim a thread in the bpr-claims block.
			if (holds(aRequest, "addressed_threads")) {
				static constexpr auto claims = std::array{ "Partly, the clamp is in and no test covers it", "Addressed", "Not addressed, nothing changed there", "Cannot tell, the file moved" };
				auto                  answer = std::string{ "Of these threads, some are done and some are not; each claim is below.\n\n```bpr-claims\n" };
				auto                  index  = std::size_t{ 0U };
				for (auto found = aRequest.find(".identity"); found != std::string::npos; found = aRequest.find(".identity", found + 1U)) {
					auto start = found + 9U;
					start += aRequest.compare(start, 2U, "\\t") == 0 ? 2U : 1U;
					const auto end = aRequest.find_first_of("\\\n\"", start);
					answer += aRequest.substr(start, end - start) + ": " + claims[index++ % claims.size()] + "\n";
				}
				return answer + "```\n\n_Fixture agent: a canned answer, nothing was spent._";
			}
			if (holds(aRequest, "proposed_fix")) {
				return "**Partly**\n\nThe resize is clamped to one pixel at `src/engine/renderer.cpp:12`, as the thread asked; no test covers the zero-width case yet.\n\n"
					   "_Fixture agent: a canned answer, nothing was spent._";
			}
			return "# What changed since your review\n\nTwo commits since your review, and one merge of main.\n\n"
				   "## Rendering\n\n`src/engine/renderer.cpp:12` now clamps the width and height to at least one pixel before resizing.\n\n"
				   "## Merged main\n\nThe merge touched none of this pull request's files.\n\n"
				   "## Your conversations\n\nThe thread at `src/engine/renderer.cpp:12` is touched by this change.\n\n"
				   "_Fixture agent: a canned answer, nothing was spent._";
		}
		// Look ask-v002: an explanation and a what-could-break answer are long
		// enough to be reports (two headings or more), so a look sees the
		// report card and its tab.
		if (holds(aRequest, "explain.path")) {
			return "# What this file does\n\n`src/engine/renderer.cpp` draws each view between `begin_frame()` and `present()`.\n\n"
				   "## What changed\n\nThe resize at `src/engine/renderer.cpp:12` is the one behavioural change: the width and height are clamped to one pixel.\n\n"
				   "## How it fits\n\n`src/engine/renderer.h` declares what it defines; `src/main.cpp` calls `draw()` once a frame.\n\n"
				   "_Fixture agent: a canned answer, nothing was spent._";
		}
		if (holds(aRequest, "What could this change break")) {
			return "# What this change could break\n\n"
				   "## Rendering\n\n- A zero-size window now draws one pixel instead of asserting (`src/engine/renderer.cpp:12`).\n- `draw_scene()` runs before `present()`, so a scene that read the old frame sees the new one.\n\n"
				   "## Shaders\n\n- `src/engine/shaders/post/bloom.frag` trusts `uScene`; a missing binding samples black.\n\n"
				   "## Widgets\n\n- `src/ui/widgets/slider.cpp` accepts a value past its maximum.\n\n"
				   "_Fixture agent: a canned answer, nothing was spent._";
		}
		if (holds(aRequest, "bpr.tour-draft.v1")) {
			return R"({"schema":"bpr.tour-draft.v1","problem":"Two views that cleared in the same frame drew over each other.","solution":"Each view now clears and draws its own scene between begin and present.","stops":[]})";
		}
		if (holds(aRequest, "bpr.review-preparation")) {
			return R"({"schema":"bpr.review-preparation.result.v1","kind":"rewrite","status":"ok","replacement_text":"Fixture rewrite: the loop does the same work twice. Could it run once?","notes":[]})";
		}
		if (holds(aRequest, "suggest.")) {
			return "These lines need no change for that comment.\n\n_Fixture agent: a canned answer, nothing was spent._";
		}
		if (holds(aRequest, "commit message") || holds(aRequest, "Commit message")) {
			return "Clamp the resize to one pixel\n\nA zero-width frame asserted in the swap chain; the renderer now resizes to at least one pixel.";
		}
		// A review (the review skill asks every turn for findings it finds;
		// only a review is answered with them).
		if (holds(aRequest, "bpr-findings") && (holds(aRequest, "Review this pull request") || holds(aRequest, "Review src/") || holds(aRequest, "run_ai_review"))) {
			return "I read the renderer and its header. Two findings below; one comes with a comment you could post.\n\n"
				   "```bpr-findings\n"
				   R"({"findings": [{"path": "src/engine/renderer.cpp", "startLine": 1, "endLine": 2, "severity": "major", "category": "correctness", "confidence": "medium", )"
				   R"("claim": "The frame budget resets on every call.", "evidence": "draw_pass checks it after begin_frame restored it.", "suggestedComment": "suggestion: reset the budget once per request, not per frame."}, )"
				   R"({"path": "src/engine/renderer.h", "startLine": 1, "endLine": 1, "severity": "minor", "category": "documentation", "confidence": "low", )"
				   R"("claim": "The header does not say which thread calls begin_frame.", "evidence": "No comment or assertion names the render thread."}]})"
				   "\n```\n";
		}
		return "Fixture answer: this is a canned reply from the fixture agent, so no real agent ran and nothing was spent.\n\n"
			   "- The change at `src/engine/renderer.cpp:12` clamps the resize.\n- Nothing else in the question was read.";
	}

	auto failure_for(const std::string& aRequest) -> std::string
	{
		const auto scripted = environment("BPR_FIXTURE_AGENT_FAIL");
		if (scripted == "crash" || holds(aRequest, "FIXTURE_CRASH")) {
			return "crash";
		}
		if (scripted == "usage-limit" || holds(aRequest, "FIXTURE_USAGE_LIMIT")) {
			return "usage-limit";
		}
		return { };
	}

	/// \brief The tools a launch gave Claude (`--tools`), as Claude's init
	/// frame lists them: none for a tool-free writing run (What changed since
	/// my review, a rewrite), the read-only set for a read-only run, and Read
	/// and Grep for an ordinary chat. Look ask-v004 (owner, 2026-11-14:
	/// "still doesnt work"): the init frame always listed Read and Grep, which
	/// the app rightly refuses from a run it gave no tools.
	auto tools_of(const std::vector<std::string>& aArguments) -> std::vector<std::string>
	{
		const auto given = std::ranges::find(aArguments, std::string{ "--tools" });
		if (given == aArguments.end() || std::next(given) == aArguments.end()) {
			return { "Read", "Grep" };
		}
		auto tools = std::vector<std::string>{ };
		auto list  = std::string_view{ *std::next(given) };
		while (!list.empty()) {
			const auto comma = list.find(',');
			tools.emplace_back(list.substr(0U, comma));
			list = comma == std::string_view::npos ? std::string_view{ } : list.substr(comma + 1U);
		}
		return tools;
	}

	auto run_claude(const std::string& aRequest, const std::vector<std::string>& aTools) -> int
	{
		auto listed = std::string{ };
		for (const auto& tool : aTools) {
			listed += (listed.empty() ? "\"" : ",\"") + tool + "\"";
		}
		std::cout << R"({"type":"system","subtype":"init","session_id":"fixture-agent","tools":[)" << listed << R"(],"mcp_servers":[],"plugins":[],"model":"fixture"})" << '\n'
				  << std::flush;
		const auto failure = failure_for(aRequest);
		if (failure == "crash") {
			pause();
			std::cerr << "fixture agent: stopped before answering (scripted)\n";
			return 1;
		}
		if (failure == "usage-limit") {
			pause();
			std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"limit","model":"<synthetic>","role":"assistant","content":[{"type":"text","text":"Claude AI usage limit reached|1893456000"}]},"error":"rate_limit","is_api_error_message":true})" << '\n'
					  << R"({"type":"result","subtype":"success","is_error":true,"session_id":"fixture-agent","result":"Claude AI usage limit reached|1893456000","total_cost_usd":0})" << '\n'
					  << std::flush;
			return 1;
		}
		// A run with Read reads a file first, in steps; a tool-free run
		// answers straight away.
		if (std::ranges::find(aTools, std::string{ "Read" }) != aTools.end()) {
			pause();
			std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-read","role":"assistant","content":[{"type":"tool_use","id":"read-1","name":"Read","input":{"file_path":"src/engine/renderer.cpp"}}]}})" << '\n'
					  << std::flush;
			pause();
			std::cout << R"({"type":"user","session_id":"fixture-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"read-1","content":"fixture file"}]}})" << '\n'
					  << std::flush;
		}
		pause();
		const auto encoded = escape(answer_for(aRequest));
		std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-answer","role":"assistant","content":[{"type":"text","text":")" << encoded << "\"}]}}\n";
		std::cout << R"({"type":"result","subtype":"success","session_id":"fixture-agent","is_error":false,"total_cost_usd":0,"usage":{"input_tokens":1,"output_tokens":1},"result":")" << encoded << "\"}\n"
				  << std::flush;
		return 0;
	}

	auto run_codex(const std::string& aRequest) -> int
	{
		std::cout << R"({"type":"thread.started","thread_id":"fixture-agent"})" << '\n'
				  << R"({"type":"turn.started"})" << '\n'
				  << std::flush;
		const auto failure = failure_for(aRequest);
		if (failure == "crash") {
			pause();
			std::cerr << "fixture agent: stopped before answering (scripted)\n";
			return 1;
		}
		if (failure == "usage-limit") {
			pause();
			std::cout << R"({"type":"turn.failed","error":{"message":"You've hit your usage limit. Try again later."}})" << '\n'
					  << std::flush;
			return 1;
		}
		pause();
		std::cout << R"({"type":"item.completed","item":{"id":"item-0","type":"reasoning","text":"Reading the fixture file."}})" << '\n'
				  << std::flush;
		pause();
		std::cout << R"({"type":"item.completed","item":{"id":"item-1","type":"agent_message","text":")" << escape(answer_for(aRequest)) << "\"}}\n"
				  << R"({"type":"turn.completed","usage":{"input_tokens":1,"cached_input_tokens":0,"output_tokens":1}})" << '\n'
				  << std::flush;
		return 0;
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	auto arguments = std::vector<std::string>{ };
	for (auto index = 1; index < argc; ++index) {
		arguments.emplace_back(argv[index]);
	}
	const auto has   = [&arguments](const std::string_view aWord) { return std::ranges::find(arguments, aWord) != arguments.end(); };
	const auto name  = argc > 0 ? std::filesystem::path{ argv[0] }.stem().string() : std::string{ };
	const auto codex = name == "codex" || has("exec");
	if (has("--version")) {
		std::cout << (codex ? "codex-cli 0.0.0-fixture\n" : "2.1.314 (Claude Code)\n");
		return EXIT_SUCCESS;
	}
	if (!codex && has("auth") && has("status")) {
		std::cout << R"({"loggedIn":true,"authMethod":"fixture","apiProvider":"firstParty"})" << '\n';
		return EXIT_SUCCESS;
	}
	if (codex && has("login")) {
		std::cout << "Logged in using the fixture agent\n";
		return EXIT_SUCCESS;
	}
	if (codex && !has("exec")) {
		return EXIT_SUCCESS;
	}
	// The request: every argument (a system prompt rides there) and the input,
	// read to its end or, for Claude's stream-JSON, to the first user message.
	auto request = std::string{ };
	for (const auto& argument : arguments) {
		request += argument;
		request += '\n';
	}
	for (auto line = std::string{ }; std::getline(std::cin, line);) {
		request += line;
		request += '\n';
		if (!codex && line.find("\"type\":\"user\"") != std::string::npos) {
			break;
		}
	}
	return codex ? run_codex(request) : run_claude(request, tools_of(arguments));
}
