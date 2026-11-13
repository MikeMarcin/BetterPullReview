// [[BPR-2061]], [[BPR-2059]]. A fixture agent whose review answers with
// several suggested comments, for the owner's look at Draft all and at how a
// finding card reads.
//
// The text is synthetic, written for this fixture; it reproduces the shape
// the owner reported on 2026-11-10 (a suggestion led by a Conventional
// Comments label, restating its reasoning, with inline code and a fenced
// block) beside a short, postable one, so one reply shows both. One finding
// names a line past its file's end, so Draft all has one to skip, and one
// carries no suggested comment. It speaks Codex JSONL, as
// `agent_finding_canary_main.cpp` does, and answers as the review skill asks
// a real agent to: prose, then one block fenced `bpr-findings`, which the host
// decodes, anchoring the findings on the file the turn froze.

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
	struct suggestion
	{
		std::string_view Path;
		unsigned         Begin = 0U;
		unsigned         End   = 1U;
		std::string_view Severity;
		std::string_view Confidence;
		std::string_view Category;
		std::string_view Claim;
		std::string_view Evidence;
		/// JSON-escaped already; empty for none.
		std::string_view Comment;
	};
} // namespace

auto main(int argc, char** argv) -> int
{
	// The agent setup's check asks `codex login status` and `codex --version`
	// (and the model menu `codex app-server`) of the program it found: this
	// fixture is built as `codex` in a folder of its own for that search.
	const auto argument = [argc, argv](const std::string_view aWord) {
		for (auto index = 1; index < argc; ++index) {
			if (std::string_view{ argv[index] } == aWord) {
				return true;
			}
		}
		return false;
	};
	if (argument("login")) {
		std::cout << "Logged in using a fixture account\n";
		return EXIT_SUCCESS;
	}
	if (argument("--version")) {
		std::cout << "codex-cli 0.0.0-fixture\n";
		return EXIT_SUCCESS;
	}
	if (!argument("exec")) {
		return EXIT_SUCCESS;
	}
	// The context is read and dropped: the answer does not depend on it.
	auto buffer = std::ostringstream{ };
	buffer << std::cin.rdbuf();

	// One-based, inclusive lines, as the review skill asks.
	constexpr suggestion findings[] = {
		{ .Path       = "src/engine/renderer.cpp",
		  .Begin      = 1U,
		  .End        = 2U,
		  .Severity   = "major",
		  .Confidence = "medium",
		  .Category   = "correctness",
		  .Claim      = "`Renderer::begin_frame` resets `frame_budget_` on every call, so the retry loop never sees an exhausted budget.",
		  .Evidence   = "`draw_pass` checks `frame_budget_.exhausted()` after `begin_frame` has already restored it.",
		  .Comment    = "suggestion (non-blocking): `Renderer::begin_frame` resets `frame_budget_` on every call, which means the retry loop in `draw_pass` "
						"can never observe an exhausted budget because the counter is restored before the check runs, so the budget is effectively "
						"unbounded. Consider moving the reset into `begin_request` instead:\\n\\n```cpp\\nvoid Renderer::begin_request() {\\n    "
						"frame_budget_.reset();\\n}\\n```\\n\\nThis keeps per-frame work bounded while still giving each request a fresh budget." },
		{ .Path       = "tests/ui/widgets/button_tests.cpp",
		  .Begin      = 1U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "high",
		  .Category   = "maintainability",
		  .Claim      = "The test name does not say what it checks.",
		  .Evidence   = "`test_button` asserts that one click emits `clicked` exactly once.",
		  .Comment    = "nitpick: The test name `test_button` does not say what it checks; because the test asserts a single emission of `clicked`, "
						"naming it after that behaviour, for example `test_click_emits_once`, makes a failure easier to read." },
		{ .Path       = "src/engine/shaders/post/tonemap.frag",
		  .Begin      = 1U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "medium",
		  .Category   = "correctness",
		  .Claim      = "Exposure is applied after the clamp, so bright values clip early.",
		  .Evidence   = "`clamp(color, 0.0, 1.0)` runs before `color *= exposure`.",
		  .Comment    = "Bright values clip before `exposure` is applied. Multiply by `exposure` first, then clamp." },
		{ .Path       = "src/engine/renderer.cpp",
		  .Begin      = 4000U,
		  .End        = 4000U,
		  .Severity   = "trivial",
		  .Confidence = "high",
		  .Category   = "style",
		  .Claim      = "An unused member is left behind.",
		  .Evidence   = "Nothing reads `legacy_path_`.",
		  .Comment    = "Remove the unused `legacy_path_` member." },
		// RFC-BPR-0127 ([[BPR-2160]]). A file the pull request does not
		// change: no line takes a draft, so its card offers Keep local.
		{ .Path       = "docs/not-in-this-pull-request.md",
		  .Begin      = 1U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "medium",
		  .Category   = "documentation",
		  .Claim      = "The guide still names the renderer's old path.",
		  .Evidence   = "It links `src/renderer.cpp`, which this change moves.",
		  .Comment    = "The guide still links the old path; update it with this move." },
		{ .Path       = "src/engine/renderer.h",
		  .Begin      = 1U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "low",
		  .Category   = "documentation",
		  .Claim      = "The header does not say which thread may call `begin_frame`.",
		  .Evidence   = "No comment or assertion names the render thread.",
		  .Comment    = "" },
	};
	// The block's JSON, escaped once more to ride inside the message text.
	auto block = std::string{ "{\\\"findings\\\": [" };
	auto first = true;
	for (const auto& finding : findings) {
		block += first ? "{" : ",{";
		first = false;
		block += "\\\"path\\\": \\\"" + std::string{ finding.Path } + "\\\"";
		block += ", \\\"startLine\\\": " + std::to_string(finding.Begin) + ", \\\"endLine\\\": " + std::to_string(finding.End);
		block += ", \\\"severity\\\": \\\"" + std::string{ finding.Severity } + "\\\", \\\"category\\\": \\\"" + std::string{ finding.Category } + "\\\"";
		block += ", \\\"confidence\\\": \\\"" + std::string{ finding.Confidence } + "\\\"";
		block += ", \\\"claim\\\": \\\"" + std::string{ finding.Claim } + "\\\"";
		block += ", \\\"evidence\\\": \\\"" + std::string{ finding.Evidence } + "\\\"";
		if (!finding.Comment.empty()) {
			// A suggestion's own escapes (\n) are escaped again for the frame.
			auto comment = std::string{ };
			for (const auto character : finding.Comment) {
				if (character == '\\') {
					comment += "\\\\";
				} else {
					comment.push_back(character);
				}
			}
			block += ", \\\"suggestedComment\\\": \\\"" + comment + "\\\"";
		}
		block += "}";
	}
	block += "]}";
	std::cout << "{\"type\":\"thread.started\",\"thread_id\":\"suggestions-fixture\"}\n"
				 "{\"type\":\"turn.started\"}\n"
				 "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\",\"type\":\"agent_message\",\"text\":\""
				 "I read the renderer, the tone-mapping shader and the button tests. Six findings below; five come with a comment you could post. "
				 "I could not run the shaders.\\n\\n```bpr-findings\\n"
			  << block
			  << "\\n```\\n\"}}\n"
				 "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,\"cached_input_tokens\":0,\"output_tokens\":1}}\n";
	std::cout.flush();
	return EXIT_SUCCESS;
}
