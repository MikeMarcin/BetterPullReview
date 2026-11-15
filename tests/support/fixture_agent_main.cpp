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

	/// [[BPR-2151]] look prompts-v001 (owner: "we need a message that the
	/// fake agent takes enough time on for me to queue stuff"): a question
	/// holding "Take your time" runs about twenty seconds, showing progress
	/// each second (`BPR_FIXTURE_AGENT_SLOW_STEP_MS`, default 1000).
	constexpr int slow_steps = 20;

	auto slow(const std::string& aRequest) -> bool { return holds(aRequest, "Take your time") || holds(aRequest, "take your time"); }

	void slow_step()
	{
		const auto configured = environment("BPR_FIXTURE_AGENT_SLOW_STEP_MS");
		const auto millis     = configured.empty() ? 1000 : std::atoi(configured.c_str());
		if (millis > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ millis });
		}
	}

	/// The canned answer for what the request asks.
	auto answer_for(const std::string& aRequest) -> std::string
	{
		if (slow(aRequest)) {
			return "Took my time: twenty slow steps, about twenty seconds, so you could queue follow-ups while I worked. This is the fixture agent; nothing real ran.";
		}
		if (holds(aRequest, "returning.operation")) {
			// RFC-BPR-0128 as amended (look ask-v005): several threads in one
			// ask, a claim a thread in the bpr-claims block.
			if (holds(aRequest, "addressed_threads")) {
				// Look ask-v007: a report, a section a thread (its file and
				// line, its claim, why), then the claims block.
				static constexpr auto claims   = std::array{ "Partly", "Addressed", "Not addressed", "Cannot tell" };
				static constexpr auto headings = std::array{ "Clamp the size to at least one pixel", "Name the magic number", "Should this clamp to the maximum?", "Is the threshold still right after the move?",
															 "Cover the zero-size resize with a test", "Say who owns the target" };
				static constexpr auto reasons  = std::array{ "The width is clamped. The height still goes to `Target.resize` as it arrives.", "The change the thread asked for is in the push.",
															 "The lines are as they were when you commented, and no follow-up is linked.", "The file moved, so its lines cannot be compared with the ones you commented on." };
				const auto            value    = [&aRequest](const std::size_t aFrom, const std::string_view aField) {
					const auto found = aRequest.find(aField, aFrom);
					if (found == std::string::npos) {
						return std::string{ };
					}
					auto start = found + aField.size();
					start += aRequest.compare(start, 2U, "\\t") == 0 ? 2U : 1U;
					const auto end = aRequest.find_first_of("\\\n\"", start);
					return aRequest.substr(start, end - start);
				};
				// docs/design/report-templates-v1: a section a verdict, the
				// threads that still need work first, an item a thread.
				auto items = std::array<std::string, 4>{ };
				auto block = std::string{ "```bpr-claims\n" };
				auto index = std::size_t{ 0U };
				for (auto found = aRequest.find(".identity"); found != std::string::npos; found = aRequest.find(".identity", found + 1U)) {
					const auto identity = value(found, ".identity");
					const auto path     = value(found, ".path");
					const auto line     = value(found, ".line");
					const auto claim    = index % claims.size();
					items[claim] += std::string{ "### " } + headings[index % headings.size()] + "\n\n`" + path + ":" + line + "`\n\n" + reasons[claim] + "\n\n";
					if (claim == 2U) {
						items[claim] += "```cpp\nValue = aValue;\nif (Value > Maximum) { notify_overflow(); }\n```\n\n";
					}
					block += identity + ": " + claims[claim] + "\n";
					++index;
				}
				auto report = std::string{ "# Was this addressed?\n\nOf " } + std::to_string(index) + " threads, some are addressed and some are not; what is left is listed first.\n\n";
				for (const auto claim : { 2U, 0U, 3U, 1U }) {
					if (!items[claim].empty()) {
						report += std::string{ "## " } + claims[claim] + "\n\n" + items[claim];
					}
				}
				return report + block + "```\n\n_Fixture agent: a canned answer, nothing was spent._";
			}
			if (holds(aRequest, "proposed_fix")) {
				return "**Partly**\n\nThe resize is clamped to one pixel at `src/engine/renderer.cpp:12`, as the thread asked; no test covers the zero-width case yet.\n\n"
					   "_Fixture agent: a canned answer, nothing was spent._";
			}
			return "# What changed since your review\n\n"
				   "Two commits and one merge of main since you reviewed at `b20cb44`. The resize is now clamped to one pixel, as you asked, and a tonemap pass is new. Nothing else you read has changed.\n\n"
				   "## Changed\n\n### Rendering\n\n"
				   "- `src/engine/renderer.cpp:12` The width is clamped to at least one pixel before the resize. The height is not.\n"
				   "- `src/engine/renderer.h:4` The comment on `resize()` now states the one-pixel floor.\n\n"
				   "```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n"
				   "### Shaders\n\n- `src/engine/shaders/post/tonemap.frag:1-5` A new pass that samples `uScene` and writes it straight through.\n\n"
				   "### Tests\n\n- `tests/engine/renderer_tests.cpp:6` A zero-width resize is covered. A zero height is not.\n\n"
				   "## Merged main\n\n- Merged main (9 commits). None touched this pull request's files.\n\n"
				   "## Your conversations\n\n"
				   "- **Lines changed** `src/engine/renderer.cpp:12` Clamp the size to at least one pixel.\n"
				   "- **Replied** `src/ui/widgets/slider.cpp:3` Should this clamp to the maximum? The lines are as they were.\n"
				   "- **Unchanged** `src/ui/widgets/button.cpp:5` nitpick: name the magic number.\n\n"
				   "_Fixture agent: a canned answer, nothing was spent._";
		}
		// Look ask-v002: an explanation and a what-could-break answer are long
		// enough to be reports (two headings or more), so a look sees the
		// report card and its tab.
		if (holds(aRequest, "explain.path")) {
			return "# What renderer.cpp does\n\n"
				   "`src/engine/renderer.cpp` draws one frame: it sizes the swap chain, then paints each layer between `begin_frame()` and `present()`. This pull request moves it into `src/engine/` and adds a one-pixel clamp to the resize.\n\n"
				   "## How it works\n\n### resize()\n\n`src/engine/renderer.cpp:9-14`\n\nClamps the width to one pixel and resizes the target. It runs whenever the window's extent changes, including when the window is minimised.\n\n"
				   "```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n"
				   "### draw()\n\n`src/engine/renderer.cpp:3-7`\n\nBegins the frame, draws the scene and presents it. Nothing is drawn outside that bracket.\n\n"
				   "## What this pull request changes\n\n- `src/engine/renderer.cpp:1` The include path follows the move to `src/engine/`.\n- `src/engine/renderer.cpp:12` The width clamp is new.\n\n"
				   "## How it fits\n\n- **Declared in** `src/engine/renderer.h:3`\n- **Called by** `src/main.cpp:6` Once a frame.\n- **Tested by** `tests/engine/renderer_tests.cpp:6`\n\n"
				   "## Worth knowing\n\n- `src/engine/renderer.cpp:13` A zero height reaches `Target.resize` unclamped.\n\n"
				   "_Fixture agent: a canned answer, nothing was spent._";
		}
		if (holds(aRequest, "What could this change break")) {
			return "# What this change could break\n\n"
				   "The move itself is safe: every include resolves. One thing is likely to break where no test looks: the packaging script still collects shaders from the old folder. Two more are possible.\n\n"
				   "## Likely\n\n### A packaged build starts with no shaders\n\n`tools/build/scripts/package.py:2`\n\nThe script collects `src/shaders/*.frag`. After the move that matches nothing, and nothing fails: the archive is built without them.\n\n"
				   "```python\nSHADERS = glob(\"src/shaders/*.frag\")\n```\n\n**Check:** Run the packaging script and list the archive's `shaders/` folder.\n\n"
				   "## Possible\n\n### A minimised window asserts\n\n`src/engine/renderer.cpp:13`\n\nThe width is clamped to one pixel but the height is not, and the swap chain asserts on a zero extent.\n\n"
				   "**Check:** Add a zero-height resize beside the zero-width one in `tests/engine/renderer_tests.cpp:6`.\n\n"
				   "### Code outside this repository that includes the old paths\n\n`src/engine/renderer.h:1`\n\nAnything that includes `renderer.h` by its old path stops compiling.\n\n**Check:** Search the dependants for `#include \"renderer.h\"`.\n\n"
				   "## Unlikely\n\n### The bloom pass samples black\n\n`src/engine/shaders/post/bloom.frag:3`\n\nIt trusts `uScene` to be bound; the move did not change the binding.\n\n**Check:** Open a scene with bloom on.\n\n"
				   "## Not checked\n\n- Windows and Linux builds: only the macOS paths were read.\n\n"
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
			// docs/design/report-templates-v1: a review is a report, its
			// findings block after it.
			return "# Review of #21\n\n"
				   "The move is clean: the files moved without a change in behaviour. One thing must be fixed before this merges: a zero height still asserts. The new tonemap pass raises a question.\n\n"
				   "## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\n"
				   "The width is clamped, but the height goes to `Target.resize` as it arrives. A minimised window reports a height of zero, and the swap chain asserts on it.\n\n"
				   "```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n"
				   "```suggestion\nconst auto width = std::max(aSize.Width, 1);\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```\n\n"
				   "## Worth a look\n\n### question: Is the tonemap pass meant to ignore exposure?\n\n`src/engine/shaders/post/tonemap.frag:5`\n\nIt writes the scene straight through. Either it is a placeholder, or `uExposure` was lost in the move.\n\n"
				   "### nitpick: The comment says what, not why\n\n`src/engine/renderer.cpp:11`\n\n\"A minimised window reports zero\" would be more use as \"the swap chain asserts on a zero extent\".\n\n"
				   "## Good\n\n### praise: The frame bracket is easy to read\n\n`src/engine/renderer.cpp:3-7`\n\nBegin, draw, present, and nothing else. It is hard to get the order wrong.\n\n"
				   "## Questions for the author\n\n- `src/engine/shaders/post/tonemap.frag:5` Is this pass a placeholder for now?\n\n"
				   "## Not reviewed\n\n- `docs/api/v2/endpoints.md` and `docs/api/reference.md`. Prose only.\n\n"
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
		// The slow ask: one read a second, so the footer and the pending
		// list have a running turn to show.
		if (slow(aRequest)) {
			const auto reads = std::ranges::find(aTools, std::string{ "Read" }) != aTools.end();
			for (int step = 1; step <= slow_steps; ++step) {
				slow_step();
				if (reads) {
					const auto id = "slow-" + std::to_string(step);
					std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-)" << id << R"(","role":"assistant","content":[{"type":"tool_use","id":")" << id
							  << R"(","name":"Read","input":{"file_path":"src/engine/step-)" << step << R"(.cpp"}}]}})" << '\n'
							  << R"({"type":"user","session_id":"fixture-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" << id
							  << R"(","content":"fixture file"}]}})" << '\n'
							  << std::flush;
				}
			}
		} else if (std::ranges::find(aTools, std::string{ "Read" }) != aTools.end()) {
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
		if (slow(aRequest)) {
			for (int step = 1; step <= slow_steps; ++step) {
				slow_step();
				std::cout << R"({"type":"item.completed","item":{"id":"slow-)" << step << R"(","type":"reasoning","text":"Taking my time: step )" << step << R"( of 20."}})" << '\n'
						  << std::flush;
			}
		}
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
