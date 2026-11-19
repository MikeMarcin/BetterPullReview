// [[BPR-1861]]. The Conventional Comments label an agent wrote at the start of
// a suggested comment, read into the editor's label model and written again by
// the label control's own serializer.

#include "bpr/core/agent_comment_suggestion.hpp"
#include "bpr/core/feedback_convention.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "agent comment suggestion failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	struct labelled_case
	{
		std::string_view              Text;
		feedback_label                Label;
		std::string_view              Body;
		bool                          Blocking    = false;
		bool                          NonBlocking = false;
		bool                          IfMinor     = false;
		std::vector<std::string_view> Custom      = { };
	};

	/// The result reads back in the editor as the label it names, and reading
	/// it again changes nothing: the canonical form is a fixed point.
	void expect_round_trip(const agent_comment_suggestion& aRead, const std::string_view aWhat)
	{
		const auto annotation = recognize_feedback_prefix(aRead.Body);
		const auto raw        = annotation.RawLabel ? feedback_text(aRead.Body, *annotation.RawLabel) : std::string_view{ };
		expect(find_feedback_label(raw) == aRead.Label && raw == stable_name(*aRead.Label), std::string{ aWhat } + ": the body reads back as its label in catalog spelling");
		expect(read_agent_comment_suggestion(aRead.Body) == aRead, std::string{ aWhat } + ": reading the written body again changes nothing");
	}

	void expect_labelled(const labelled_case& aCase)
	{
		const auto what = std::string{ aCase.Text };
		const auto read = read_agent_comment_suggestion(aCase.Text);
		expect(read.Reading == agent_comment_reading::labelled, what + ": labelled");
		expect(read.Label == aCase.Label, what + ": label");
		expect(read.Body == aCase.Body, what + ": body is '" + read.Body + "'");
		expect(read.Blocking == aCase.Blocking && read.NonBlocking == aCase.NonBlocking && read.IfMinor == aCase.IfMinor, what + ": standard decorations");
		expect(read.Custom.size() == aCase.Custom.size(), what + ": custom decoration count");
		for (std::size_t index = 0U; index < aCase.Custom.size(); ++index) {
			expect(read.Custom[index] == aCase.Custom[index], what + ": custom decoration kept as written");
		}
		expect_round_trip(read, what);
		expect(read_agent_comment_suggestion(aCase.Text) == read, what + ": equal text gives an equal reading");
	}

	void expect_as_written(const std::string_view aText, const agent_comment_reading aReading)
	{
		const auto what = std::string{ aText };
		const auto read = read_agent_comment_suggestion(aText);
		expect(read.Reading == aReading, what + ": reading is " + std::string{ stable_name(read.Reading) });
		expect(!read.Label && !read.Blocking && !read.NonBlocking && !read.IfMinor && read.Custom.empty(), what + ": no label or decoration is invented");
		expect(read.Body == aText, what + ": the text is kept byte for byte");
	}

	void test_every_catalog_label()
	{
		for (const auto& definition : feedback_label_catalog()) {
			const auto id    = std::string{ definition.Id };
			auto       title = id;
			title.front()    = static_cast<char>(title.front() - 'a' + 'A');
			auto upper       = id;
			for (auto& byte : upper) {
				byte = static_cast<char>(byte - 'a' + 'A');
			}
			const auto canonical = id + ": check the retry budget";
			for (const auto& written : { id + ": check the retry budget", title + ": check the retry budget", upper + ": check the retry budget", "**" + title + ":** check the retry budget",
										 "**" + id + "**: check the retry budget", "  \n\n\t" + title + " :  check the retry budget" }) {
				expect_labelled({ .Text = written, .Label = definition.Label, .Body = canonical });
			}
		}
	}

	void test_decorations()
	{
		// [[BPR-2264]] A weight the label implies is left out of the draft.
		expect_labelled({ .Text = "suggestion (non-blocking): use a constant", .Label = feedback_label::suggestion, .Body = "suggestion: use a constant" });
		expect_labelled({ .Text = "issue (blocking): the loop never ends", .Label = feedback_label::issue, .Body = "issue: the loop never ends" });
		// One that differs from the label's adds information and stays.
		expect_labelled({ .Text = "suggestion (blocking): rename it before release", .Label = feedback_label::suggestion, .Body = "suggestion (blocking): rename it before release", .Blocking = true });
		expect_labelled({ .Text = "nitpick (blocking): x", .Label = feedback_label::nitpick, .Body = "nitpick (blocking): x", .Blocking = true });
		expect_labelled({ .Text = "nitpick (if-minor): spacing", .Label = feedback_label::nitpick, .Body = "nitpick (if-minor): spacing", .IfMinor = true });
		expect_labelled({ .Text = "Issue (Non-Blocking, If-Minor): x", .Label = feedback_label::issue, .Body = "issue (non-blocking, if-minor): x", .NonBlocking = true, .IfMinor = true });
		expect_labelled({ .Text = "**Suggestion (NON_BLOCKING):** use a constant", .Label = feedback_label::suggestion, .Body = "suggestion: use a constant" });
		expect_labelled({ .Text = "**issue (blocking)**: x", .Label = feedback_label::issue, .Body = "issue: x" });
		expect_labelled({ .Text = "**Issue** (Blocking): x", .Label = feedback_label::issue, .Body = "issue: x" });
		expect_labelled({ .Text = "question (non-blocking): x", .Label = feedback_label::question, .Body = "question (non-blocking): x", .NonBlocking = true });
		// [[BPR-2276]] A todo does not block on its own: `(non-blocking)` adds
		// nothing to it, and `(blocking)` does.
		expect_labelled({ .Text = "todo( non blocking ):x", .Label = feedback_label::todo, .Body = "todo: x" });
		expect_labelled({ .Text = "todo (blocking): add the clamp", .Label = feedback_label::todo, .Body = "todo (blocking): add the clamp", .Blocking = true });
		expect_labelled({ .Text = "typo (non-blocking): recieve", .Label = feedback_label::typo, .Body = "typo: recieve" });
		expect_labelled({ .Text = "chore (if_minor) : x", .Label = feedback_label::chore, .Body = "chore (if-minor): x", .IfMinor = true });
		// A custom decoration is kept as written, in the order written.
		expect_labelled({ .Text = "issue (Security, blocking): x", .Label = feedback_label::issue, .Body = "issue (Security): x", .Custom = { "Security" } });
		// One weight: of blocking and non-blocking the later one stays.
		expect_labelled({ .Text = "issue (blocking, non-blocking): x", .Label = feedback_label::issue, .Body = "issue (non-blocking): x", .NonBlocking = true });
		// A repeated decoration is written once.
		expect_labelled({ .Text = "question (non-blocking, Non-Blocking): x", .Label = feedback_label::question, .Body = "question (non-blocking): x", .NonBlocking = true });
		// [[BPR-2264]] And a repeated weight the label implies is left out.
		expect_labelled({ .Text = "thought (non-blocking, Non-Blocking): x", .Label = feedback_label::thought, .Body = "thought: x" });
	}

	void test_emphasis_and_layout()
	{
		expect_labelled({ .Text = "*question:* is this reached?", .Label = feedback_label::question, .Body = "question: is this reached?" });
		expect_labelled({ .Text = "__praise__: tidy", .Label = feedback_label::praise, .Body = "praise: tidy" });
		expect_labelled({ .Text = "_note_ (non-blocking): x", .Label = feedback_label::note, .Body = "note: x" });
		// The header alone on its first line takes its subject from the next
		// line that is not blank; the discussion and its line endings stay.
		expect_labelled({ .Text = "**Issue (blocking):**\n\nThe loop never stops.\n\nIt resets each frame.", .Label = feedback_label::issue, .Body = "issue: The loop never stops.\n\nIt resets each frame." });
		expect_labelled({ .Text = "Suggestion:\r\n\r\nUse a constant.\r\nIt reads better.\r\n", .Label = feedback_label::suggestion, .Body = "suggestion: Use a constant.\r\nIt reads better.\r\n" });
		// A header with nothing after it is the header alone, as the chip
		// writes it before a subject is typed.
		expect_labelled({ .Text = "**Question:**", .Label = feedback_label::question, .Body = "question: " });
		// A suggested-change fence in the discussion is copied unchanged.
		expect_labelled({ .Text = "suggestion: rename it\n\n```suggestion\nauto attempts = 0;\n```\n", .Label = feedback_label::suggestion, .Body = "suggestion: rename it\n\n```suggestion\nauto attempts = 0;\n```\n" });
		// UTF-8 in the subject survives.
		expect_labelled({ .Text = "Praise: très bien — 很好", .Label = feedback_label::praise, .Body = "praise: très bien — 很好" });
	}

	void test_missing_or_unknown_label()
	{
		expect_as_written("", agent_comment_reading::unlabelled);
		expect_as_written("  \n\t", agent_comment_reading::unlabelled);
		expect_as_written("Name the exhausted retry in the message.", agent_comment_reading::unlabelled);
		expect_as_written("  Should this increment `attempts` before sleeping?\n\n", agent_comment_reading::unlabelled);
		expect_as_written("Note that this resets each frame.", agent_comment_reading::unlabelled);
		expect_as_written("Suggestion - use a constant.", agent_comment_reading::unlabelled);
		expect_as_written("**Suggestion: use a constant.**", agent_comment_reading::unlabelled);
		expect_as_written("- issue: in a list", agent_comment_reading::unlabelled);
		expect_as_written("`issue`: in code", agent_comment_reading::unlabelled);
		expect_as_written("> issue: quoted", agent_comment_reading::unlabelled);
		// No alias, abbreviation or plural is a label.
		expect_as_written("nit: spacing", agent_comment_reading::unknown_label);
		expect_as_written("Nit (non-blocking): spacing", agent_comment_reading::unknown_label);
		expect_as_written("bug (blocking): crashes", agent_comment_reading::unknown_label);
		expect_as_written("suggestions: two of them", agent_comment_reading::unknown_label);
		expect_as_written("**Summary:** the change is fine", agent_comment_reading::unknown_label);
		expect_as_written(std::string(40U, 'a') + ": too long a word", agent_comment_reading::unlabelled);
	}

	void test_headers_kept_as_written()
	{
		// A catalog label whose header cannot be written without changing
		// what the agent wrote is left as the text it is.
		expect_as_written("issue (): empty list", agent_comment_reading::kept_as_written);
		expect_as_written("issue (blocking, ): empty decoration", agent_comment_reading::kept_as_written);
		expect_as_written("issue (blocking: unclosed", agent_comment_reading::kept_as_written);
		expect_as_written("issue ((blocking)): nested", agent_comment_reading::kept_as_written);
		expect_as_written("issue (sec/urity): not a token", agent_comment_reading::kept_as_written);
		expect_as_written("issue: - a list item first", agent_comment_reading::kept_as_written);
		expect_as_written("suggestion:\n\n```suggestion\nauto attempts = 0;\n```", agent_comment_reading::kept_as_written);
	}

	void test_canonical_text_is_a_fixed_point()
	{
		for (const std::string_view text : { "issue (non-blocking): x", "suggestion (blocking, if-minor): y\n\nmore", "issue: w", "question: ", "praise: z" }) {
			const auto read = read_agent_comment_suggestion(text);
			expect(read.Reading == agent_comment_reading::labelled && read.Body == text, std::string{ text } + ": canonical text is unchanged");
		}
		expect(stable_name(agent_comment_reading::unlabelled) == "unlabelled" && stable_name(agent_comment_reading::unknown_label) == "unknown_label"
				   && stable_name(agent_comment_reading::labelled) == "labelled" && stable_name(agent_comment_reading::kept_as_written) == "kept_as_written",
			   "stable names");
	}

	/// [[BPR-2061]]. The draft keeps the suggestion's Markdown: code spans, a
	/// fenced block and its line breaks pass from reply to draft unchanged,
	/// with or without a label in front.
	void test_markdown_survives_into_the_draft()
	{
		const auto plain = std::string_view{ "Guard `cache_.at(key)` with a lookup first:\n\n```cpp\nif (auto found = cache_.find(key); found != cache_.end()) {\n\treturn found->second;\n}\n```\nThen the miss path stays cheap.\n" };
		const auto read  = read_agent_comment_suggestion(plain);
		expect(read.Reading == agent_comment_reading::unlabelled && read.Body == plain, "an unlabelled suggestion with code drafts byte for byte");
		const auto labelled  = std::string{ "suggestion: Guard `cache_.at(key)` with a lookup.\n\n```cpp\nauto found = cache_.find(key);\n```\n" };
		const auto withLabel = read_agent_comment_suggestion(labelled);
		expect(withLabel.Reading == agent_comment_reading::labelled, "the label is read");
		expect(withLabel.Body.find("`cache_.at(key)`") != std::string::npos, "the code span keeps its backticks");
		expect(withLabel.Body.find("\n\n```cpp\nauto found = cache_.find(key);\n```\n") != std::string::npos, "the fence and its line breaks are kept");
	}

	/// [[BPR-2270]] (owner, 2026-11-18: "I don't think we need that safety
	/// net"). A drafted comment is the agent's text: the label Comment labels
	/// asked for is kept, a plain comment (a blanked convention) stays plain,
	/// and a label from the person's own convention is neither replaced nor
	/// given a second one.
	void test_drafts_follow_the_comment_labels_feature()
	{
		const auto labelled = read_agent_comment_suggestion("**Issue:** `begin_frame` resets `Retry_` every frame, so the retry loop never ends.");
		expect(labelled.Reading == agent_comment_reading::labelled && labelled.Label == feedback_label::issue
				   && labelled.Body == "issue: `begin_frame` resets `Retry_` every frame, so the retry loop never ends.",
			   "the agent's label is kept, in the label control's spelling");
		const auto weighted = read_agent_comment_suggestion("suggestion (blocking): Reset it once per request.");
		expect(weighted.Label == feedback_label::suggestion && weighted.Blocking && weighted.Body == "suggestion (blocking): Reset it once per request.",
			   "a weight that differs from the label's is kept");
		const auto plain = std::string_view{ "`begin_frame` resets `Retry_` every frame. Reset it once per request instead." };
		const auto none  = read_agent_comment_suggestion(plain);
		expect(none.Reading == agent_comment_reading::unlabelled && !none.Label && none.Body == plain, "a plain comment stays plain: the app adds no label of its own");
		const auto custom = std::string_view{ "blocker: `begin_frame` resets `Retry_` every frame." };
		const auto own    = read_agent_comment_suggestion(custom);
		expect(own.Reading == agent_comment_reading::unknown_label && !own.Label && own.Body == custom, "a label outside the catalog stays as written, with no second label");
	}
} // namespace

auto main() -> int
{
	test_every_catalog_label();
	test_decorations();
	test_emphasis_and_layout();
	test_missing_or_unknown_label();
	test_headers_kept_as_written();
	test_canonical_text_is_a_fixed_point();
	test_markdown_survives_into_the_draft();
	test_drafts_follow_the_comment_labels_feature();
	std::cout << "agent-comment-suggestion-tests:pass\n";
	return EXIT_SUCCESS;
}
