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
// `BPR_FIXTURE_AGENT_PAUSE_MS` (default 350), 0 for none. As `claude` it keeps
// its connection and can be steered ([[BPR-2153]]);
// `BPR_FIXTURE_AGENT_STEERABLE=0` makes it the one-shot agent with no version,
// and `FIXTURE_UNVERSIONED` in a question makes that turn name no version
// (an agent BPR can't steer, reachable through the app). `FIXTURE_HELPER` in a
// question makes that turn run a helper for about twenty seconds and report a
// scripted cost mid-turn ([[BPR-2252]]).
//
// RFC-BPR-0120 ([[BPR-2189]]). A local draft's check: a request that asks
// for a `bpr-check-plan` block is answered with a plan (the author's words
// choose it: "fail" plans a failing test, "touch" a command that changes a
// file Git tracks, "scratch" a script in the scratch folder); one that asks
// for a `bpr-check-report` runs that plan in its working folder, the check
// copy, as Bash tool calls with their output. "touch" really rewrites the
// copy's first file, so the host sees a tracked file changed; nothing else is
// run or written.

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
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

	auto tour_question(const std::string& aRequest) -> std::string;

	/// A tour chat's request carries its earlier turns: only this turn's
	/// question decides ([[BPR-2154]]).
	auto slow(const std::string& aRequest) -> bool
	{
		const auto& asked = aRequest.find("this_turn") != std::string::npos ? tour_question(aRequest) : aRequest;
		return holds(asked, "Take your time") || holds(asked, "take your time");
	}

	void slow_step()
	{
		const auto configured = environment("BPR_FIXTURE_AGENT_SLOW_STEP_MS");
		const auto millis     = configured.empty() ? 1000 : std::atoi(configured.c_str());
		if (millis > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds{ millis });
		}
	}

	/// The text after `aMarker` up to the end of its line, in a request
	/// whose message is JSON-escaped or not.
	auto line_after(const std::string& aText, const std::string_view aMarker) -> std::string
	{
		const auto found = aText.find(aMarker);
		if (found == std::string::npos) {
			return { };
		}
		const auto from = found + aMarker.size();
		auto       end  = std::min(aText.find("\\n", from), aText.find('\n', from));
		end             = std::min(end, aText.find('"', from));
		return aText.substr(from, end == std::string::npos ? std::string::npos : end - from);
	}

	auto check_plan_answer(const std::string& aRequest) -> std::string
	{
		const auto words   = line_after(aRequest, "The author asks: ");
		auto       steps   = std::vector<std::pair<std::string, std::string>>{ { "cmake --build build --target parser_tests", "the change still builds" } };
		const auto failing = holds(words, "fail");
		steps.emplace_back(failing ? "ctest --test-dir build -R parser_fails" : "ctest --test-dir build -R parser", failing ? "the parser's edge cases pass" : "the parser tests pass");
		if (holds(words, "touch")) {
			steps.emplace_back("tools/stamp-version", "the version stamp still applies; it rewrites a file Git tracks");
		}
		const auto scratch = holds(words, "scratch");
		if (scratch) {
			steps.emplace_back("python3 scratch/summarize.py", "a summary of the test log, from a script in the scratch folder");
		}
		auto json = std::string{ R"({"commands":[)" };
		for (std::size_t index = 0U; index < steps.size(); ++index) {
			json += (index == 0U ? "" : ",") + std::string{ R"({"command":")" } + steps[index].first + R"(","shows":")" + steps[index].second + R"("})";
		}
		json += std::string{ R"(],"needs":["builds")" } + (scratch ? R"(,"scratch")" : "") + R"(],"not_covered":"The GUI and the slow integration tests."})";
		const auto lead = words.empty() ? std::string{ "You gave no words, so I chose the checks that matter most here: build the parser and run its tests." }
										: "Here is a check for \"" + words + "\".";
		return lead + " This is the fixture agent; nothing runs until you press Run.\n\n```bpr-check-plan\n" + json + "\n```";
	}

	struct check_step
	{
		std::string Command;
		std::string Output;
		int         Exit = 0;
	};

	/// The plan's commands, as the run request numbers them ("1. `cmd`: ...").
	auto check_run_steps(const std::string& aRequest) -> std::vector<check_step>
	{
		auto steps = std::vector<check_step>{ };
		for (auto at = aRequest.find(". `"); at != std::string::npos; at = aRequest.find(". `", at + 3U)) {
			const auto close = aRequest.find('`', at + 3U);
			if (close == std::string::npos || at == 0U || aRequest[at - 1U] < '0' || aRequest[at - 1U] > '9') {
				continue;
			}
			auto command = aRequest.substr(at + 3U, close - at - 3U);
			if (std::ranges::any_of(steps, [&command](const check_step& aStep) { return aStep.Command == command; })) {
				continue;
			}
			const auto fails = holds(command, "parser_fails");
			steps.push_back({ .Command = command, .Output = fails ? "parser_tests: expected 3, got 2" : "ok", .Exit = fails ? 8 : 0 });
		}
		if (std::ranges::any_of(steps, [](const check_step& aStep) { return aStep.Exit != 0; })) {
			steps.push_back({ .Command = "ls build", .Output = "parser_tests" });
		}
		return steps;
	}

	/// The copy's first file, in path order, skipping hidden ones.
	auto first_file() -> std::string
	{
		auto files = std::vector<std::string>{ };
		auto error = std::error_code{ };
		for (auto entry = std::filesystem::recursive_directory_iterator{ std::filesystem::current_path(), error }; !error && entry != std::filesystem::recursive_directory_iterator{ }; entry.increment(error)) {
			const auto relative = std::filesystem::relative(entry->path(), std::filesystem::current_path(), error).generic_string();
			if (entry->is_regular_file() && !relative.starts_with('.') && relative.find("/.") == std::string::npos) {
				files.push_back(relative);
			}
		}
		std::ranges::sort(files);
		return files.empty() ? std::string{ } : files.front();
	}

	/// Runs the plan as tool calls: each command and its output.
	void emit_check_run(const std::vector<check_step>& aSteps)
	{
		auto index = 0;
		for (const auto& step : aSteps) {
			pause();
			const auto id = "check-" + std::to_string(++index);
			std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-)" << id << R"(","role":"assistant","content":[{"type":"tool_use","id":")" << id
					  << R"(","name":"Bash","input":{"command":")" << escape(step.Command) << R"("}}]}})" << '\n'
					  << std::flush;
			if (holds(step.Command, "stamp-version")) {
				if (const auto file = first_file(); !file.empty()) {
					auto out = std::ofstream{ file, std::ios::app };
					out << "\nstamped by the fixture agent's check\n";
				}
			}
			pause();
			std::cout << R"({"type":"user","session_id":"fixture-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" << id << R"(","content":")"
					  << escape(step.Output) << R"(","is_error":)" << (step.Exit == 0 ? "false" : "true") << "}]}}\n"
					  << std::flush;
		}
	}

	auto check_run_answer(const std::string& aRequest, const std::vector<check_step>& aSteps) -> std::string
	{
		const auto failed = std::ranges::any_of(aSteps, [](const check_step& aStep) { return aStep.Exit != 0; });
		auto       json   = std::string{ R"({"outcome":")" } + (aSteps.empty() ? "didnt_run" : failed ? "failed"
																									  : "passed")
			+ R"(","summary":")"
			+ (aSteps.empty() ? "The plan named no command I could run."
				   : failed   ? "The parser built, but its edge-case test failed: an empty token is counted."
							  : "The parser built and its tests passed.")
			+ R"(","commands":[)";
		for (std::size_t index = 0U; index < aSteps.size(); ++index) {
			json += (index == 0U ? "" : ",") + std::string{ R"({"command":")" } + escape(aSteps[index].Command) + R"(","exit":)" + std::to_string(aSteps[index].Exit) + "}";
		}
		json += "]";
		if (failed) {
			auto file = line_after(aRequest, "Changed in this candidate:\\n- ");
			if (file.empty()) {
				file = line_after(aRequest, "Changed in this candidate:\n- ");
			}
			if (file.empty()) {
				file = first_file();
			}
			json += R"(,"output":["parser_tests: expected 3, got 2","  at the empty token"])";
			if (!file.empty()) {
				json += R"(,"references":[{"path":")" + escape(file) + R"(","line":2}])";
			}
		}
		json += R"(,"not_covered":"The GUI and the slow integration tests."})";
		return "I ran the plan in the check copy (" + std::filesystem::current_path().generic_string() + "). This is the fixture agent's report.\n\n```bpr-check-report\n" + json + "\n```";
	}

	/// The canned answer for what the request asks.
	/// [[BPR-2154]]. The question of a tour chat's turn, from the request's
	/// `this_turn`, its quotes escaped once or not at all.
	auto tour_question(const std::string& aRequest) -> std::string
	{
		// The member, not the instructions' mention of it.
		auto turn = aRequest.rfind(R"(this_turn\":)");
		if (turn == std::string::npos) {
			turn = aRequest.rfind(R"(this_turn":)");
		}
		for (const auto& [open, close] : { std::pair<std::string_view, std::string_view>{ R"(\"question\":\")", R"(\")" }, { R"("question":")", R"(")" } }) {
			const auto found = aRequest.find(open, turn == std::string::npos ? 0U : turn);
			if (found != std::string::npos) {
				const auto from = found + open.size();
				const auto end  = aRequest.find(close, from);
				return aRequest.substr(from, end == std::string::npos ? std::string::npos : std::min<std::size_t>(end - from, 120U));
			}
		}
		return { };
	}

	auto tour_reply(const std::string& aRequest) -> std::string
	{
		auto question = tour_question(aRequest);
		std::erase_if(question, [](const char aChar) { return aChar == '\\' || aChar == '"'; });
		return "Fixture agent: read your message (" + question + "). Nothing in the tour changes; nothing was spent.";
	}

	/// A review: the review skill asks every turn for findings it finds; only
	/// a review is answered with them.
	auto reviews(const std::string& aRequest) -> bool
	{
		return holds(aRequest, "bpr-findings") && (holds(aRequest, "Review this pull request") || holds(aRequest, "Review src/") || holds(aRequest, "Review commits") || holds(aRequest, "run_ai_review"));
	}

	/// [[BPR-2219]]. What a real agent says while it reviews, between its
	/// tool calls: each is a text block of the turn, so the application's
	/// answer holds them before the report, as a real Claude Code turn's does.
	constexpr auto review_notes = std::array<std::string_view, 3>{
		"I'll start with the brief and the diff, then read the files the change touches.",
		"The resize is the change that matters. Let me check what calls it and what the swap chain does with a zero extent.",
		"I have what I need. Now I'll write up the review.",
	};

	/// Writes the notes of a review as Claude Code does: a text block and a
	/// tool call in one assistant message, then the tool's result.
	void emit_review_notes(const bool aReads, const int aTurn)
	{
		for (auto index = std::size_t{ 0U }; index < review_notes.size(); ++index) {
			// A kept connection answers several turns: each message's
			// identity is its turn's own.
			const auto id = "review-" + std::to_string(aTurn) + "-" + std::to_string(index);
			std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-)" << id << R"(","role":"assistant","content":[{"type":"text","text":")" << escape(std::string{ review_notes[index] }) << "\"}";
			if (aReads && index + 1U < review_notes.size()) {
				std::cout << R"(,{"type":"tool_use","id":")" << id << R"(","name":"Read","input":{"file_path":"src/engine/renderer.cpp"}}]}})" << '\n'
						  << R"({"type":"user","session_id":"fixture-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" << id << R"(","content":"fixture file"}]}})" << '\n';
			} else {
				std::cout << "]}}\n";
			}
			std::cout << std::flush;
		}
	}

	auto answer_for(const std::string& aRequest) -> std::string
	{
		// [[BPR-2286]]. A follow-up round: a proposal is only its text; a
		// question's answer says how many earlier rounds its message carried,
		// so a course sees the conversation travel with each round.
		if (holds(aRequest, "bpr.follow-up.v1")) {
			if (holds(aRequest, "bpr.follow-up.update-suggestion")) {
				return "issue (blocking): clamp the height as well as the width. A minimised window reports a height of zero, and the swap chain asserts on it.";
			}
			if (holds(aRequest, "bpr.follow-up.update-draft")) {
				return "question: does a zero height still reach the swap chain here? A minimised window reports one.";
			}
			if (holds(aRequest, "bpr.follow-up.draft-reply")) {
				return "Thanks for checking. A minimised window reports a height of zero, so the clamp needs the height too; I'll add it.";
			}
			auto rounds = std::size_t{ 0U };
			for (auto at = aRequest.find("Reviewer: "); at != std::string::npos; at = aRequest.find("Reviewer: ", at + 1U)) {
				++rounds;
			}
			auto answer = "Fixture follow-up: I read the item and " + std::to_string(rounds) + " earlier rounds. A minimised window reports a height of zero, which this does not clamp.";
			if (holds(aRequest, "skill.0.name\\tfixture-item-style") && holds(aRequest, "FIXTURE_SELECTED_ITEM_CHAT_GUIDANCE")
				&& !holds(aRequest, "FIXTURE_UNUSED_ITEM_CHAT_GUIDANCE") && !holds(aRequest, "skill.1.name")) {
				answer += " Item-chat skill guidance received.";
			}
			return answer;
		}
		if (holds(aRequest, "bpr-check-report")) {
			return check_run_answer(aRequest, check_run_steps(aRequest));
		}
		if (holds(aRequest, "bpr-check-plan")) {
			return check_plan_answer(aRequest);
		}
		// [[BPR-2154]]. A turn of a tour part's chat: the reply says which
		// message it read, so a look and a course see each queued message go
		// as its own turn, in order; it changes nothing in the tour.
		if (holds(aRequest, "bpr.tour-revise.v2")) {
			return R"({"schema":"bpr.tour-revise.v2","reply":")" + tour_reply(aRequest)
				+ R"(","stops":[],"stop_texts":[],"remove_stops":[],"merge_stops":[],"split_stops":[],"order":[],"focus":"","replace_waiting_stops":false})";
		}
		if (holds(aRequest, "bpr.tour-ask.v2")) {
			return R"({"schema":"bpr.tour-ask.v2","reply":")" + tour_reply(aRequest) + R"(","stops":[],"stop_texts":[]})";
		}
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
		// [[BPR-2248]]. Explain these commits: a section a commit, by the
		// headlines the request carries, a merge of the base named and not
		// explained, and one commit whose change does more than its message.
		if (holds(aRequest, "commits.scope") && !reviews(aRequest)) {
			const auto value = [&aRequest](const std::size_t aFrom, const std::string& aField) {
				const auto found = aRequest.find(aField, aFrom);
				if (found == std::string::npos) {
					return std::string{ };
				}
				auto start = found + aField.size();
				start += aRequest.compare(start, 2U, "\\t") == 0 ? 2U : 1U;
				const auto end = aRequest.find_first_of("\\\n\"", start);
				return aRequest.substr(start, end - start);
			};
			auto own    = std::string{ };
			auto merges = std::string{ };
			auto count  = 0;
			for (auto index = 0; index < 50; ++index) {
				const auto prefix = "commits." + std::to_string(index);
				const auto found  = aRequest.find(prefix + ".oid");
				if (found == std::string::npos) {
					break;
				}
				const auto oid      = value(found, prefix + ".oid").substr(0U, 7U);
				const auto headline = value(found, prefix + ".headline");
				if (value(found, prefix + ".merges_base") == "1") {
					merges.append("- `").append(oid).append("` ").append(headline).append(". It brings in the base branch; its changes are not this pull request's.\n");
					continue;
				}
				++count;
				own.append("### `").append(oid).append("` ").append(headline).append("\n\n");
				own += count == 2 ? std::string{ "**Does more than its message says.** Besides what the message names, it also edits `src/app.cpp:40`, which the message does not mention.\n\n" }
								  : std::string{ "The change matches its message: it does what the message says and nothing else.\n\n" };
			}
			// A course checks the base branch's own change never rode along.
			const auto based = holds(aRequest, "release-notes") ? std::string{ "present" } : std::string{ "none" };
			return "# What these commits change\n\n" + std::to_string(count) + " commits of the pull request's own were read against their messages. One does more than its message says.\n\n## By commit\n\n" + own
				+ (merges.empty() ? std::string{ } : "## Merges of the base branch\n\n" + merges + "\n") + "_Fixture agent: a canned answer, nothing was spent. Base branch changes in the request: " + based + "._";
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
		// [[BPR-2281]]. Draft Description: the description itself, title-free
		// Markdown in the Description writing prompt's three parts, with no
		// canned-answer line, since it goes into the pull request's body.
		if (holds(aRequest, "bpr.description-draft.v1")) {
			return "## Summary\n\n"
				   "Each view now clears and draws its own scene between `begin_frame()` and `present()`, so two views in one frame no longer draw over each other.\n\n"
				   "## What changed\n\n"
				   "- `draw()` brackets the scene with `clear()` and `present()` for each view.\n"
				   "- `resize()` keeps the size it is given for the next frame instead of stalling at zero width.\n\n"
				   "## How to test\n\n"
				   "1. Open two views side by side and resize the window: neither draws over the other.\n"
				   "2. Drag the window to zero width and back: drawing resumes at the new size.";
		}
		if (holds(aRequest, "bpr.tour-draft.v2")) {
			// Two stops on the fixture's renderer change, so each stop has a
			// chat of its own ([[BPR-2154]]).
			const auto stops = holds(aRequest, "src/engine/renderer.cpp")
				? std::string{ R"({"path":"src/engine/renderer.cpp","first_line":3,"last_line":8,"title":"Where the change starts","explanation":"draw() now draws the scene between clear() and present()."},)"
							   R"({"path":"src/engine/renderer.cpp","first_line":10,"last_line":14,"title":"The new resize","explanation":"resize() keeps the new size for the next frame."})" }
				: std::string{ };
			return R"({"schema":"bpr.tour-draft.v2","stops":[)" + stops + "]}";
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
		// [[BPR-2248]]. Review these commits: a review of the selected
		// commits' own changes, its findings on the commit-selection course's
		// files.
		if (reviews(aRequest) && holds(aRequest, "commits.scope")) {
			return "# Review of the selected commits\n\n"
				   "**Verdict:** Ready with comments\n\n"
				   "The input change and the regeneration agree. The follow-up commit edits two generated files by hand, which the next regeneration will undo.\n\n"
				   "## Worth a look\n\n### issue (non-blocking): A generated file is edited by hand\n\n`gen/item_007.txt:2`\n\n"
				   "The follow-up adjusts this file directly. The generator does not know about the change, so the next run puts the old line back.\n\n"
				   "### question: Is line 40 part of the follow-up's purpose?\n\n`src/app.cpp:40`\n\nThe message says it tidies the app after the regeneration; this line changes what it prints.\n\n"
				   "## Not reviewed\n\n- The merge of main: it is the base branch's own work.\n\n"
				   "```bpr-findings\n"
				   R"({"findings": [{"path": "gen/item_007.txt", "startLine": 2, "endLine": 2, "severity": "minor", "category": "maintainability", "confidence": "high", )"
				   R"("claim": "A generated file is edited by hand.", "evidence": "The follow-up commit changes gen/item_007.txt, which the generator writes.", )"
				   R"("suggestedComment": "issue (non-blocking): this file is generated, so the next regeneration undoes this edit. Could the change go in the generator's input?"}, )"
				   R"({"path": "src/app.cpp", "startLine": 40, "endLine": 40, "severity": "minor", "category": "correctness", "confidence": "medium", )"
				   R"("claim": "Line 40 changes behaviour in a tidy-up commit.", "evidence": "The message says tidy; the line changes what the app prints.", )"
				   R"("suggestedComment": "question: is this change to line 40 meant to be part of the follow-up?"}]})"
				   "\n```\n";
		}
		if (reviews(aRequest)) {
			// docs/design/report-templates-v1 and [[BPR-2219]]: a review is a
			// report, its verdict first, and its findings block lists the
			// report's own findings in its order, each with the comment a
			// draft starts from.
			return "# Review of #21\n\n"
				   "**Verdict:** Needs changes\n\n"
				   "The move is clean: the files moved without a change in behaviour. One thing must be fixed before this merges: a zero height still asserts. The new tonemap pass raises a question.\n\n"
				   "## Must fix\n\n### issue (blocking): A zero height still asserts\n\n`src/engine/renderer.cpp:12-13`\n\n"
				   "The width is clamped, but the height goes to `Target.resize` as it arrives. A minimised window reports a height of zero, and the swap chain asserts on it.\n\n"
				   "```cpp\nconst auto width = std::max(aSize.Width, 1);\nTarget.resize(extent{ width, aSize.Height });\n```\n\n"
				   "```suggestion\nconst auto width = std::max(aSize.Width, 1);\nconst auto height = std::max(aSize.Height, 1);\nTarget.resize(extent{ width, height });\n```\n\n"
				   "## Worth a look\n\n### question: Is the tonemap pass meant to ignore exposure?\n\n`src/engine/shaders/post/tonemap.frag:5`\n\nIt writes the scene straight through. Either it is a placeholder, or `uExposure` was lost in the move.\n\n"
				   "### nitpick: The comment says what, not why\n\n`src/engine/renderer.cpp:11`\n\n\"A minimised window reports zero\" would be more use as \"the swap chain asserts on a zero extent\".\n\n"
				   "### suggestion (non-blocking): Say in the README where the engine moved\n\n`README.md`\n\nIt still points at `render/`. A reader following it finds nothing.\n\n"
				   "## Good\n\n### praise: The frame bracket is easy to read\n\n`src/engine/renderer.cpp:3-7`\n\nBegin, draw, present, and nothing else. It is hard to get the order wrong.\n\n"
				   "## Questions for the author\n\n- `src/engine/shaders/post/tonemap.frag:5` Is this pass a placeholder for now?\n\n"
				   "## Not reviewed\n\n- `docs/api/v2/endpoints.md` and `docs/api/reference.md`. Prose only.\n\n"
				   "```bpr-findings\n"
				   R"({"findings": [{"path": "src/engine/renderer.cpp", "startLine": 12, "endLine": 13, "severity": "major", "category": "correctness", "confidence": "high", )"
				   R"("claim": "A zero height still reaches the swap chain.", "evidence": "resize clamps the width only; Target.resize takes aSize.Height as it arrives.", )"
				   R"("suggestedComment": "issue (blocking): the width is clamped but the height is not, so a minimised window still asserts in the swap chain. Clamp both."}, )"
				   R"({"path": "src/engine/shaders/post/tonemap.frag", "startLine": 5, "endLine": 5, "severity": "minor", "category": "correctness", "confidence": "low", )"
				   R"("claim": "The tonemap pass ignores exposure.", "evidence": "It writes the scene colour straight through; uExposure is not read."}]})"
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
		if (holds(aRequest, "bpr-check-report")) {
			emit_check_run(check_run_steps(aRequest));
		} else if (slow(aRequest)) {
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
		if (reviews(aRequest)) {
			emit_review_notes(std::ranges::find(aTools, std::string{ "Read" }) != aTools.end(), 0);
		}
		const auto encoded = escape(answer_for(aRequest));
		std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-answer","role":"assistant","content":[{"type":"text","text":")" << encoded << "\"}]}}\n";
		std::cout << R"({"type":"result","subtype":"success","session_id":"fixture-agent","is_error":false,"total_cost_usd":0,"usage":{"input_tokens":1,"output_tokens":1},"result":")" << encoded << "\"}\n"
				  << std::flush;
		return 0;
	}

	/// [[BPR-2153]]. Standard input, read on a thread of its own, so a turn
	/// can look for a correction between its tool calls without blocking.
	class input_lines final
	{
	public:
		input_lines()
			: Reader{ [this] { read(); } }
		{ }
		~input_lines() { Reader.join(); }
		input_lines(const input_lines&)                    = delete;
		auto operator=(const input_lines&) -> input_lines& = delete;

		/// The next line, waiting for it; empty once input has ended.
		auto wait() -> std::optional<std::string>
		{
			auto lock = std::unique_lock{ Mutex };
			Changed.wait(lock, [this] { return !Lines.empty() || Closed; });
			return take();
		}
		/// The next line already read, without waiting.
		auto poll() -> std::optional<std::string>
		{
			const auto lock = std::scoped_lock{ Mutex };
			return take();
		}

	private:
		auto take() -> std::optional<std::string>
		{
			if (Lines.empty()) {
				return std::nullopt;
			}
			auto line = std::move(Lines.front());
			Lines.pop_front();
			return line;
		}
		void read()
		{
			for (auto line = std::string{ }; std::getline(std::cin, line);) {
				const auto lock = std::scoped_lock{ Mutex };
				Lines.push_back(line);
				Changed.notify_all();
			}
			const auto lock = std::scoped_lock{ Mutex };
			Closed          = true;
			Changed.notify_all();
		}
		std::mutex              Mutex;
		std::condition_variable Changed;
		std::deque<std::string> Lines;
		bool                    Closed = false;
		std::thread             Reader;
	};

	/// A string field of one stream-JSON line, unescaped; empty when absent.
	auto field(const std::string& aLine, const std::string_view aName) -> std::string
	{
		const auto key   = "\"" + std::string{ aName } + "\":\"";
		const auto found = aLine.find(key);
		if (found == std::string::npos) {
			return { };
		}
		auto value = std::string{ };
		for (auto index = found + key.size(); index < aLine.size() && aLine[index] != '"'; ++index) {
			if (aLine[index] == '\\' && index + 1U < aLine.size()) {
				++index;
				value += aLine[index] == 'n' ? '\n' : aLine[index] == 't' ? '\t'
																		  : aLine[index];
				continue;
			}
			value += aLine[index];
		}
		return value;
	}

	/// A stream-JSON user message, written compactly or not.
	auto user_line(const std::string& aLine) -> bool { return holds(aLine, "\"type\":\"user\"") || holds(aLine, "\"type\": \"user\""); }

	/// [[BPR-2252]]. A question holding `FIXTURE_HELPER` is a turn with one
	/// helper, as Claude Code 2.1.319 was recorded running one
	/// (`tests/fixtures/agent/traces/claude-2.1.319-helpers-stdin-open.jsonl`):
	/// the message starts it and ends with a result while it is out, which
	/// reports a figure mid-turn; the helper works for `slow_steps` steps and
	/// reports back; a further part answers. The figures are scripted
	/// ($0.0412, then $0.0587); the fixture still costs nothing.
	auto helper_turn(const std::string& aRequest) -> bool
	{
		const auto& asked = aRequest.find("this_turn") != std::string::npos ? tour_question(aRequest) : aRequest;
		return holds(asked, "FIXTURE_HELPER");
	}

	void emit_helper_part(const int aResults)
	{
		const auto turn = std::to_string(aResults);
		pause();
		std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-helper-)" << turn << R"(","role":"assistant","content":[{"type":"tool_use","id":"helper-use-)" << turn
				  << R"(","name":"Agent","input":{"description":"Read the renderer","prompt":"Read src/engine/renderer.cpp","run_in_background":true}}]}})" << '\n'
				  << R"({"type":"system","subtype":"task_started","session_id":"fixture-agent","task_id":"fixture-helper-)" << turn << R"(","tool_use_id":"helper-use-)" << turn << R"(","description":"Read the renderer"})" << '\n'
				  << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-helper-said-)" << turn << R"(","role":"assistant","content":[{"type":"text","text":"I asked a helper to read the renderer."}]}})" << '\n'
				  << R"({"type":"result","subtype":"success","session_id":"fixture-agent","is_error":false,"total_cost_usd":0.0412,"usage":{"input_tokens":1,"output_tokens":1},"uuid":"fixture-helper-result-)" << turn
				  << R"(","user_message_uuids":[],"queued_turn_count":0,"result":"I asked a helper to read the renderer."})" << '\n'
				  << std::flush;
		for (int step = 1; step <= slow_steps; ++step) {
			slow_step();
			std::cout << R"({"type":"system","subtype":"task_progress","session_id":"fixture-agent","task_id":"fixture-helper-)" << turn << R"(","description":"Reading step )" << step << R"( of 20","last_tool_name":"Read"})" << '\n'
					  << std::flush;
		}
		std::cout << R"({"type":"system","subtype":"task_notification","session_id":"fixture-agent","task_id":"fixture-helper-)" << turn << R"(","tool_use_id":"helper-use-)" << turn
				  << R"(","status":"completed","summary":"renderer.cpp read"})" << '\n'
				  << R"({"type":"system","subtype":"init","session_id":"fixture-agent","tools":[],"mcp_servers":[],"plugins":[],"model":"fixture","claude_code_version":"2.1.319"})" << '\n'
				  << std::flush;
	}

	/// [[BPR-2153]], RFC-BPR-0126 Q14. Claude Code 2.1.319 as a kept
	/// stream-JSON connection was observed to behave (the default as
	/// `claude`; `BPR_FIXTURE_AGENT_STEERABLE=0` restores the one-shot agent
	/// that reports no version): it reports its version, answers each user
	/// message as a turn, takes a correction written while it works (a
	/// UUID-bearing user message, priority `next`) after its current tool
	/// call, and names every input a result answered in `user_message_uuids`.
	/// Its answer says what it was steered with. A scripted failure ends the
	/// process at once, as a crashed or limited Claude does.
	auto serve_steerable_claude(const std::string& aArguments, const std::vector<std::string>& aTools) -> int
	{
		auto       input   = input_lines{ };
		auto       results = 0;
		const auto reads   = std::ranges::find(aTools, std::string{ "Read" }) != aTools.end();
		while (auto line = input.wait()) {
			if (!user_line(*line)) {
				continue;
			}
			const auto request = aArguments + *line + '\n';
			auto       inputs  = std::vector<std::string>{ };
			if (const auto uuid = field(*line, "uuid"); !uuid.empty()) {
				inputs.push_back(uuid);
			}
			auto listed = std::string{ };
			for (const auto& tool : aTools) {
				listed += (listed.empty() ? "\"" : ",\"") + tool + "\"";
			}
			std::cout << R"({"type":"system","subtype":"init","session_id":"fixture-agent","tools":[)" << listed << R"(],"mcp_servers":[],"plugins":[],"model":"fixture")" << (holds(request, "FIXTURE_UNVERSIONED") ? "" : R"(,"claude_code_version":"2.1.319")") << "}" << '\n'
					  << std::flush;
			// The reader thread still waits on input, so a scripted failure
			// leaves without unwinding.
			if (const auto failure = failure_for(request); failure == "crash") {
				pause();
				std::cerr << "fixture agent: stopped before answering (scripted)\n"
						  << std::flush;
				std::_Exit(1);
			} else if (failure == "usage-limit") {
				pause();
				std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"limit","model":"<synthetic>","role":"assistant","content":[{"type":"text","text":"Claude AI usage limit reached|1893456000"}]},"error":"rate_limit","is_api_error_message":true})" << '\n'
						  << R"({"type":"result","subtype":"success","is_error":true,"session_id":"fixture-agent","result":"Claude AI usage limit reached|1893456000","total_cost_usd":0})" << '\n'
						  << std::flush;
				std::_Exit(1);
			}
			auto steered = std::string{ };
			if (holds(request, "bpr-check-report")) {
				emit_check_run(check_run_steps(request));
			}
			const auto helped = helper_turn(request);
			if (helped) {
				emit_helper_part(results);
			}
			const auto steps = slow(request) ? slow_steps : 1;
			for (int step = 1; step <= steps; ++step) {
				slow(request) ? slow_step() : pause();
				if (reads) {
					const auto id = "steer-" + std::to_string(results) + "-" + std::to_string(step);
					std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-)" << id << R"(","role":"assistant","content":[{"type":"tool_use","id":")" << id
							  << R"(","name":"Read","input":{"file_path":"src/engine/step-)" << step << R"(.cpp"}}]}})" << '\n'
							  << R"({"type":"user","session_id":"fixture-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":")" << id << R"(","content":"fixture file"}]}})" << '\n'
							  << std::flush;
				}
				// After the tool call: the corrections written meanwhile.
				while (auto correction = input.poll()) {
					if (!user_line(*correction)) {
						continue;
					}
					inputs.push_back(field(*correction, "uuid"));
					// Said without repeating the words: the transcript shows
					// them as the reviewer's own message ([[BPR-2153]]).
					steered += "\n\nTook your correction after step " + std::to_string(step) + " (" + std::to_string(field(*correction, "text").size()) + " characters).";
				}
			}
			if (reviews(request)) {
				emit_review_notes(reads, results);
			}
			auto answered = answer_for(request);
			// A tour chat's answer is one JSON object: what it took goes in
			// its reply.
			if (const auto reply = answered.find(R"("reply":")"); answered.starts_with(R"({"schema":"bpr.tour-)") && reply != std::string::npos && !steered.empty()) {
				answered.insert(answered.find(R"(",")", reply + 9U), escape(steered));
				steered.clear();
			}
			const auto encoded = escape(answered + steered);
			auto       named   = std::string{ };
			for (const auto& identity : inputs) {
				named += (named.empty() ? "\"" : ",\"") + identity + "\"";
			}
			++results;
			std::cout << R"({"type":"assistant","session_id":"fixture-agent","message":{"id":"msg-answer-)" << results << R"(","role":"assistant","content":[{"type":"text","text":")" << encoded << "\"}]}}\n";
			std::cout << R"({"type":"result","subtype":"success","session_id":"fixture-agent","is_error":false,"total_cost_usd":)" << (helped ? "0.0587" : "0") << R"(,"usage":{"input_tokens":1,"output_tokens":1},"uuid":"fixture-result-)" << results
					  << R"(","user_message_uuids":[)" << named << R"(],"queued_turn_count":0,"result":")" << encoded << "\"}\n"
					  << std::flush;
		}
		return EXIT_SUCCESS;
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
	if (!codex && environment("BPR_FIXTURE_AGENT_STEERABLE") != "0") {
		return serve_steerable_claude(request, tools_of(arguments));
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
