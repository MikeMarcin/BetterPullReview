#include "bpr/core/review_tour_markdown.hpp"
#include <array>
#include <cstdlib>
#include <iostream>

namespace {
	using namespace bpr::core;
	void expect(bool aValue, const char* aWhy)
	{
		if (!aValue) {
			std::cerr << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}
	auto slice(std::string_view aRaw, tour_text_span aSpan) -> std::string
	{
		return std::string{ aRaw.substr(aSpan.Begin, aSpan.End - aSpan.Begin) };
	}
	void golden()
	{
		for (const std::string nl : { "\n", "\r\n" }) {
			const auto  prefix = "JIRA-42\n- [x] disclosure\n```md\n## Problem\n```\n> ## Problem\n<details>\n\n## Problem\n1. [fake](https://example.com/fake)\n</details>\n<script>\n## Solution\n</script>\n";
			std::string raw;
			for (char byte : std::string{ prefix }) {
				raw += byte == '\n' ? nl : std::string{ byte };
			}
			raw.append("## Problem").append(nl).append("A & B").append(nl).append(nl).append("## Solution").append(nl).append("Use snapshots").append(nl).append("## Checks").append(nl).append("- [ ] later").append(nl);
			const auto mapped = map_tour_markdown(raw);
			expect(mapped.Bounded && mapped.Sections.size() == 2 && mapped.Lists.empty(), "code/quote/disclosure cannot map fields or lists");
			expect(edit_tour_markdown(raw, { }).value() == raw, "unchanged source byte round trip");
			const auto       span = mapped.Sections[0].Content;
			const auto       text = slice(raw, span);
			const std::array edits{ tour_text_edit{ span, text, std::string{ "Changed" }.append(nl).append(nl) } };
			const auto       changed = edit_tour_markdown(raw, edits);
			expect(changed && *changed == raw.substr(0, span.Begin) + edits[0].Replacement + raw.substr(span.End), "only chosen source bytes replaced");
			auto stale        = edits;
			stale[0].Expected = "wrong";
			expect(!edit_tour_markdown(raw, stale), "stale mapping refused");
			const std::array duplicate{ edits[0], edits[0] };
			expect(!edit_tour_markdown(raw, duplicate), "overlapping mapping refused");
		}
		expect(insert_tour_brief("JIRA\r\n", 6, "broken", "fix", "\r\n") == "JIRA\r\n## Problem\r\nbroken\r\n\r\n## Solution\r\nfix\r\n\r\n", "explicit insert preview preserves template bytes");
		const auto duplicate = map_tour_markdown("## Problem\na\n## Problem\nb\n## Solution\nc\n");
		expect(duplicate.Sections.size() == 3, "duplicate fields remain choice, not newest wins");
		const auto nested = map_tour_markdown("1. [first](https://example.com/1)\n   1. [nested](https://example.com/2)\n1. [second](https://example.com/3)\n\nAnother route\n1) [last](https://example.com/4)\n");
		expect(nested.Lists.size() == 2 && nested.Lists[0].Entries.size() == 2 && nested.Lists[0].MappingNeeded, "nested entries never become stops; lists remain separately selectable");
		expect(!edit_tour_markdown("é", std::array{ tour_text_edit{ { 1, 2 }, "\xa9", "x" } }), "manual mapping cannot split UTF8");
		const std::string custom = "custom\r\n<!-- all bytes retained -->\n";
		expect(!map_tour_stop(custom).Narrative && edit_tour_markdown(custom, { }).value() == custom, "custom body remains raw mapping needed");
		expect(!map_tour_markdown(std::string(maximum_tour_field_bytes + 1, 'x')).Bounded, "scanner bounds explicit");
	}
	void generated()
	{
		const std::array entries{ tour_markdown_link{ "same [title] (x) <script> &", "https://github.com/o/r/pull/7#discussion_r18446744073709551615" }, tour_markdown_link{ "same [title] (x) <script> &", "https://github.com/o/r/pull/7#issuecomment-8" } };
		const auto       index = render_tour_index(entries, "\r\n");
		expect(index.has_value() && index->find("<!--") == std::string::npos, "ordinary index without hidden markers");
		const auto mapped = map_tour_markdown(*index);
		expect(mapped.Lists.size() == 1 && mapped.Lists[0].Entries == std::vector<tour_markdown_link>(entries.begin(), entries.end()), "escaped repeated titles round trip in textual order");
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" }, tour_markdown_link{ "Next", entries[1].Destination } };
		const auto       stop = render_tour_stop("Why boundary", 2, "Local explanation", navigation, "https://github.com/o/r/blob/1111111111111111111111111111111111111111/a.md?plain=1#L2-L4");
		expect(stop && stop->starts_with("https://") && stop->find("\n\n###") != std::string::npos, "fallback bare permalink stays first");
		const auto prefixed = map_tour_stop("Website prefix.\n\n" + *stop);
		expect(prefixed.Heading && prefixed.Narrative && prefixed.Footer && prefixed.Heading->Begin > std::string{ "Website prefix.\n\n" }.size(), "unique managed spans preserve leading website prose and permalink");
		expect(!map_tour_stop(*stop + "\n" + *stop).Heading, "multiple managed headings require explicit mapping");
		const auto split = map_tour_stop(*stop);
		expect(split.Heading && split.Narrative && split.Footer && split.Title == "Why boundary", "separate managed spans");
		auto prose = *stop + "\nThis website paragraph is new.\n";
		expect(!map_tour_stop(prose).Narrative, "website layout edit does not erase unfamiliar text");
		const auto quoted = "### Author's tour · 1 — Why\n\n```md\n[Tour overview](https://example.com)\n```\n";
		expect(!map_tour_stop(quoted).Footer, "footer in code is not managed");
		expect(!render_tour_index(std::array{ tour_markdown_link{ "bad", "https://example.com/)\n<script>" } }), "destination injection refused");
	}
	/// [[BPR-1222]]. The owner asked for stops on the Files changed tab. GitHub's
	/// `/pull/50/files` page carries each review comment as `id="r4235770230"`
	/// (read 2026-10-09 on MikeMarcin/BPR-Test#50).
	void files_changed_links()
	{
		const std::string comment = "https://github.com/o/r/pull/7#discussion_r4235770230";
		const std::string files   = "https://github.com/o/r/pull/7/files#r4235770230";
		expect(tour_stop_link(comment) == files, "a review comment links to its Files changed anchor");
		expect(tour_link_identity(files) == comment && tour_link_identity(comment) == comment, "either form names the comment GitHub returned");
		for (const auto* other : { "https://github.com/o/r/pull/7#pullrequestreview-9", "https://github.com/o/r/pull/7#issuecomment-8", "https://github.com/o/r/pull/7", "https://github.com/o/r/pull/7#discussion_r", "https://github.com/o/r/pull/7#discussion_r01", "https://github.com/o/r/pull/7#discussion_r1x",
								   "https://github.com/o/r/issues/7#discussion_r1", "https://github.com/o/r/pull/x#discussion_r1" }) {
			expect(tour_stop_link(other) == other, "anything but a review comment's URL is kept");
		}
		for (const auto* other : { "https://github.com/o/r/pull/7/files#r", "https://github.com/o/r/pull/7/files#r0", "https://github.com/o/r/pull/7/files#diff-abc", "https://github.com/o/r/blob/x/files#r1", "https://github.com/o/r/pull/7/files#r1x" }) {
			expect(tour_link_identity(other) == other, "anything but a Files changed comment anchor is kept");
		}
	}
	/// [[BPR-1785]]. GitHub does not jump to an anchor inside a diff it hides
	/// as too large, so the line under Previous and Next says to use the
	/// overview. Stops published before the note still map.
	void navigation_note()
	{
		const std::array  three{ tour_markdown_link{ "Previous", "https://github.com/o/r/pull/7#discussion_r1" }, tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" },
								 tour_markdown_link{ "Next", "https://github.com/o/r/pull/7#discussion_r3" } };
		const std::string links = "[Previous](https://github.com/o/r/pull/7#discussion_r1) · [Tour overview](https://github.com/o/r/pull/7#pullrequestreview-9) · [Next](https://github.com/o/r/pull/7#discussion_r3)";
		const std::string note  = std::string{ tour_navigation_note };
		expect(note == "_If Next or Previous doesn't jump (GitHub hides large diffs), use the Tour overview._", "the owner's wording");
		for (const std::string_view nl : { std::string_view{ "\n" }, std::string_view{ "\r\n" } }) {
			const auto stop = render_tour_stop("Why", 2, "Body", three, { }, nl);
			const auto line = std::string{ nl };
			expect(stop && stop->ends_with(line + line + links + line + note + line), "the note sits directly under the links");
			const auto mapped = map_tour_stop(*stop);
			expect(mapped.Footer && slice(*stop, *mapped.Footer) == links + line + note + line, "the footer holds the links and the note");
			expect(mapped.Narrative && slice(*stop, *mapped.Narrative) == line + "Body" + line + line, "the narrative ends before the links");
			expect(with_tour_navigation_note(*stop) == *stop, "a noted stop is left alone");
			const auto old    = "### Author's tour · 2 — Why" + line + line + "Body" + line + line + links + line;
			const auto before = map_tour_stop(old);
			expect(before.Footer && slice(old, *before.Footer) == links + line && before.Narrative && slice(old, *before.Narrative) == line + "Body" + line + line, "a stop published before the note still maps");
			expect(with_tour_navigation_note(old) == old + note + line, "an old stop gains the note under its links and nothing else changes");
		}
		const std::array alone{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       single = render_tour_stop("Only", 1, "Body", alone);
		expect(single && single->find(note) == std::string::npos, "a stop with no Previous or Next has no note");
		expect(with_tour_navigation_note(*single) == *single, "nor does it gain one");
		const auto unterminated = "### Author's tour · 2 — Why\n\nBody\n\n" + links;
		expect(with_tour_navigation_note(unterminated) == unterminated + "\n" + note + "\n", "a footer with no final newline gains one before the note");
		expect(!map_tour_stop("### Author's tour · 2 — Why\n\nBody\n\n" + note + "\n").Footer, "the note alone is not a footer");
	}
}
int main()
{
	golden();
	generated();
	files_changed_links();
	navigation_note();
	std::cout << "TOUR_MARKDOWN_OK\n";
}
