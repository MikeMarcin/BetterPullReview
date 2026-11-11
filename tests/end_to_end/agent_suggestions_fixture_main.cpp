// [[BPR-2058]], [[BPR-2059]]. A fixture agent whose review answers with
// several suggested comments, for the owner's look at Draft all and at how a
// finding card reads.
//
// The text is synthetic, written for this fixture; it reproduces the shape
// the owner reported on 2026-11-10 (a suggestion led by a Conventional
// Comments label, restating its reasoning, with inline code and a fenced
// block) beside a short, postable one, so one reply shows both. One finding
// names a line past its file's end, so Draft all has one to skip, and one
// carries no suggested comment. It speaks Codex JSONL, as
// `agent_finding_canary_main.cpp` does, and reads only the fields it needs
// from the context it is handed.

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace {
	using context_fields = std::map<std::string, std::string, std::less<>>;

	[[nodiscard]] auto parse_context(const std::string_view aPayload) -> context_fields
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
				fields.emplace(std::string{ line.substr(0U, tab) }, std::string{ line.substr(tab + 1U) });
			}
			begin = end + 1U;
		}
		return fields;
	}

	[[nodiscard]] auto field(const context_fields& aFields, const std::string_view aName, const std::string_view aFallback) -> std::string
	{
		const auto found = aFields.find(aName);
		return found == aFields.end() ? std::string{ aFallback } : found->second;
	}

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

auto main() -> int
{
	auto buffer = std::ostringstream{ };
	buffer << std::cin.rdbuf();
	const auto fields     = parse_context(buffer.str());
	const auto session    = field(fields, "session", "1");
	const auto provider   = field(fields, "profile", "2");
	const auto base       = field(fields, "base", std::string(40U, '1'));
	const auto head       = field(fields, "head", std::string(40U, '2'));
	const auto comparison = field(fields, "selection.preferred.comparison", std::string(64U, '5'));

	constexpr suggestion findings[] = {
		{ .Path       = "src/engine/renderer.cpp",
		  .Begin      = 0U,
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
		  .Begin      = 0U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "high",
		  .Category   = "maintainability",
		  .Claim      = "The test name does not say what it checks.",
		  .Evidence   = "`test_button` asserts that one click emits `clicked` exactly once.",
		  .Comment    = "nitpick: The test name `test_button` does not say what it checks; because the test asserts a single emission of `clicked`, "
						"naming it after that behaviour, for example `test_click_emits_once`, makes a failure easier to read." },
		{ .Path       = "src/engine/shaders/post/tonemap.frag",
		  .Begin      = 0U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "medium",
		  .Category   = "correctness",
		  .Claim      = "Exposure is applied after the clamp, so bright values clip early.",
		  .Evidence   = "`clamp(color, 0.0, 1.0)` runs before `color *= exposure`.",
		  .Comment    = "Bright values clip before `exposure` is applied. Multiply by `exposure` first, then clamp." },
		{ .Path       = "src/engine/renderer.cpp",
		  .Begin      = 4000U,
		  .End        = 4001U,
		  .Severity   = "trivial",
		  .Confidence = "high",
		  .Category   = "style",
		  .Claim      = "An unused member is left behind.",
		  .Evidence   = "Nothing reads `legacy_path_`.",
		  .Comment    = "Remove the unused `legacy_path_` member." },
		{ .Path       = "src/engine/renderer.h",
		  .Begin      = 0U,
		  .End        = 1U,
		  .Severity   = "minor",
		  .Confidence = "low",
		  .Category   = "documentation",
		  .Claim      = "The header does not say which thread may call `begin_frame`.",
		  .Evidence   = "No comment or assertion names the render thread.",
		  .Comment    = "" },
	};
	auto payload = std::string{ "{\"session\":" + session + ",\"findings\":[" };
	auto first   = true;
	for (const auto& finding : findings) {
		payload += first ? "{" : ",{";
		first = false;
		payload += "\"provider\":" + provider;
		payload += ",\"severity\":\"" + std::string{ finding.Severity } + "\",\"confidence\":\"" + std::string{ finding.Confidence } + "\"";
		payload += ",\"category\":\"" + std::string{ finding.Category } + "\"";
		payload += ",\"path\":\"" + std::string{ finding.Path } + "\"";
		payload += ",\"lines\":{\"begin\":" + std::to_string(finding.Begin) + ",\"end\":" + std::to_string(finding.End) + "}";
		payload += ",\"anchor\":{\"line\":{\"comparison\":\"" + comparison + "\",\"side\":\"right\",\"line\":" + std::to_string(finding.Begin) + "}}";
		payload += ",\"claim\":\"" + std::string{ finding.Claim } + "\"";
		payload += ",\"evidence\":\"" + std::string{ finding.Evidence } + "\"";
		if (!finding.Comment.empty()) {
			payload += ",\"suggestedComment\":\"" + std::string{ finding.Comment } + "\"";
		}
		payload += ",\"analyzedBase\":\"" + base + "\",\"analyzedHead\":\"" + head + "\",\"analyzedCommits\":[\"" + head + "\"]}";
	}
	payload += "]}";
	std::cout << "{\"type\":\"thread.started\",\"thread_id\":\"suggestions-fixture\"}\n"
				 "{\"type\":\"turn.started\"}\n"
				 "{\"type\":\"item.completed\",\"item\":{\"id\":\"item-1\",\"type\":\"agent_message\",\"text\":\""
				 "I read the renderer, the tone-mapping shader and the button tests. Five findings below; four come with a comment you could post. "
				 "I could not run the shaders.\"}}\n"
				 "{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":1,\"cached_input_tokens\":0,\"output_tokens\":1},\"bprFindings\":"
			  << payload << "}\n";
	std::cout.flush();
	return EXIT_SUCCESS;
}
