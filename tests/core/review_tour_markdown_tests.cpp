#include "bpr/core/review_tour_markdown.hpp"
#include <array>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

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
		expect(stop && stop->starts_with("https://") && stop->find("\n\n### PR Tour · 2 — Why boundary\n") != std::string::npos, "fallback bare permalink stays first and the published heading says PR Tour");
		const auto prefixed = map_tour_stop("Website prefix.\n\n" + *stop);
		expect(prefixed.Heading && prefixed.Narrative && prefixed.Footer && prefixed.Heading->Begin > std::string{ "Website prefix.\n\n" }.size(), "unique managed spans preserve leading website prose and permalink");
		expect(!map_tour_stop(*stop + "\n" + *stop).Heading, "multiple managed headings require explicit mapping");
		const auto split = map_tour_stop(*stop);
		expect(split.Heading && split.Narrative && split.Footer && split.Title == "Why boundary", "separate managed spans");
		auto prose = *stop + "\nThis website paragraph is new.\n";
		expect(!map_tour_stop(prose).Narrative, "website layout edit does not erase unfamiliar text");
		const auto quoted = "### Author's tour · 1 — Why\n\n```md\n[Tour overview](https://example.com)\n```\n";
		expect(!map_tour_stop(quoted).Footer, "footer in code is not managed");
		expect(!map_tour_stop(*stop + "\n" + quoted).Heading, "mixed new and legacy headings still require explicit mapping");
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
			const auto stop      = render_tour_stop("Why", 2, "Body", three, { }, nl);
			const auto line      = std::string{ nl };
			const auto footer    = std::string{ links }.append(line).append(note).append(line);
			const auto narrative = std::string{ line }.append("Body").append(line).append(line);
			expect(stop && stop->ends_with(std::string{ line }.append(line).append(footer)), "the note sits directly under the links");
			const auto mapped = map_tour_stop(*stop);
			expect(mapped.Footer && slice(*stop, *mapped.Footer) == footer, "the footer holds the links and the note");
			expect(mapped.Narrative && slice(*stop, *mapped.Narrative) == narrative, "the narrative ends before the links");
			expect(with_tour_navigation_note(*stop) == *stop, "a noted stop is left alone");
			const auto old    = std::string{ "### Author's tour · 2 — Why" }.append(line).append(narrative).append(links).append(line);
			const auto before = map_tour_stop(old);
			expect(before.Footer && slice(old, *before.Footer) == std::string{ links }.append(line) && before.Narrative && slice(old, *before.Narrative) == narrative, "a stop published before the note still maps");
			expect(with_tour_navigation_note(old) == std::string{ old }.append(note).append(line), "an old stop gains the note under its links and nothing else changes");
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
namespace {
	/// \brief [[BPR-1806]]. A published stop is read back exactly as
	/// `render_tour_stop` wrote it, and nothing looser is a stop.
	void published_stops()
	{
		constexpr auto overview = std::string_view{ "https://github.com/o/r/pull/7#pullrequestreview-9" };
		constexpr auto source   = std::string_view{ "https://github.com/o/r/blob/1111111111111111111111111111111111111111/src/light.cpp?plain=1#L12" };
		const auto     links    = [&](const bool aPrevious, const bool aNext) {
			auto result = std::vector<tour_markdown_link>{ };
			if (aPrevious) {
				result.push_back({ "Previous", "https://github.com/o/r/pull/7/files#r11" });
			}
			result.push_back({ "Tour overview", std::string{ overview } });
			if (aNext) {
				result.push_back({ "Next", "https://github.com/o/r/pull/7/files#r13" });
			}
			return result;
		};
		for (const std::string nl : { "\n", "\r\n" }) {
			// Each body written line by line in this newline style.
			const auto lined = [&nl](const std::initializer_list<std::string_view> aLines) {
				auto result = std::string{ };
				for (const auto line : aLines) {
					result.append(line).append(nl);
				}
				return result;
			};
			auto narrative = lined({ "question: Should the clamp *live* here?", "" });
			narrative.append("The renderer trusts it.");
			const auto middle = render_tour_stop("Why `width` is [clamped]", 6, narrative, links(true, true), source, nl);
			expect(middle.has_value(), "the publisher renders a middle stop");
			const auto stop = recognize_published_tour_stop(*middle);
			expect(stop.has_value() && stop->Ordinal == 6U && stop->Title == "Why `width` is [clamped]", "the number and the unescaped title are read back");
			expect(stop && slice(*middle, stop->Narrative) == narrative, "the narrative is exactly the author's words");
			expect(stop && stop->PinnedSource && slice(*middle, *stop->PinnedSource) == source, "the pinned source is named");
			expect(stop && stop->Overview == overview && stop->Previous == "https://github.com/o/r/pull/7/files#r11" && stop->Next == "https://github.com/o/r/pull/7/files#r13",
				   "the navigation's destinations are read");
			const auto first = render_tour_stop("Start", 1, "Plain words.", links(false, true), { }, nl);
			const auto read  = first ? recognize_published_tour_stop(*first) : std::nullopt;
			expect(read && read->Ordinal == 1U && !read->PinnedSource && !read->Previous && read->Next && slice(*first, read->Narrative) == "Plain words.",
				   "a first stop without a pinned source is recognized");
			// A stop published before its note, and one only partly published.
			const auto noteless = lined({ "### PR Tour · 2 — Old", "", "Words.", "", "[Previous](https://x/1) · [Tour overview](https://x/o)" });
			expect(recognize_published_tour_stop(noteless).has_value(), "a stop published before BPR-1785's note is still a stop");
			const auto pending = render_tour_stop("Pending", 3, "Words.", links(false, false), { }, nl);
			expect(pending && recognize_published_tour_stop(*pending).has_value(), "a stop with only Tour overview is a stop");
			const auto legacy = lined({ "### Author's tour · 4 — Legacy", "", "Words.", "", "[Tour overview](https://x/o)", "" });
			expect(recognize_published_tour_stop(legacy).has_value(), "the legacy heading and trailing blank lines are accepted");

			// Not stops: anything other than the exact envelope.
			constexpr auto overviewLink = std::string_view{ "[Tour overview](https://x/o)" };
			const auto     plain        = [&lined, overviewLink](const std::string_view aHeading) { return lined({ aHeading, "", "Words.", "", overviewLink }); };
			const auto     refused      = std::array{
				std::string{ "Ordinary comment." },
				std::string{ },
				lined({ "### PR Tour · 2 — Missing navigation", "", "Words." }),
				lined({ "### PR Tour · 2 — No narrative", "", overviewLink }),
				lined({ "### PR Tour · 2 — Blank narrative", "", "   ", "", overviewLink }),
				lined({ "Intro words", "", "### PR Tour · 2 — Prose before", "", "Words.", "", overviewLink }),
				lined({ "https://x/a https://x/b", "", "### PR Tour · 2 — Two sources", "", "Words.", "", overviewLink }),
				plain("### PR Tour · 02 — Leading zero"),
				plain("### PR Tour · 0 — Zero"),
				plain("### PR Tour · 257 — Too many"),
				plain("### PR Tour · six — Words"),
				plain("### PR Tour · 2 —   "),
				plain("### PR Tour · 2 - Hyphen"),
				plain("## PR Tour · 2 — Level two"),
				lined({ "### PR Tour · 2 — No gap", "Words.", "", overviewLink }),
				lined({ "### PR Tour · 2 — Glued links", "", "Words.", overviewLink }),
				lined({ "### PR Tour · 2 — After", "", "Words.", "", overviewLink, "", "Postscript." }),
				lined({ "### PR Tour · 2 — Order", "", "Words.", "", "[Next](https://x/n) · [Tour overview](https://x/o)" }),
				lined({ "### PR Tour · 2 — Other", "", "Words.", "", "[Home](https://x/o)" }),
				lined({ "### PR Tour · 1 — First with previous", "", "Words.", "", "[Previous](https://x/p) · [Tour overview](https://x/o)" }),
				lined({ "```", "### PR Tour · 2 — Fenced", "", "Words.", "", overviewLink, "```" }),
				lined({ "> ### PR Tour · 2 — Quoted", ">", "> Words.", ">", "> [Tour overview](https://x/o)" }),
				lined({ "### PR Tour · 2 — Fence open", "", "```", "Words.", "", overviewLink }),
			};
			for (const auto& body : refused) {
				if (recognize_published_tour_stop(body)) {
					std::cerr << "recognized: " << body << '\n';
				}
				expect(!recognize_published_tour_stop(body), "a body other than the published envelope is not a stop");
			}
		}
		expect(!recognize_published_tour_stop(std::string(maximum_tour_field_bytes + 1U, 'a')), "an oversized body is not read");

		// How many stops a tour has, only when the stops prove it.
		const auto stop = [](const std::string& aOverview, const std::size_t aOrdinal, const bool aPrevious, const bool aNext) {
			auto result     = published_tour_stop{ };
			result.Ordinal  = aOrdinal;
			result.Title    = "t";
			result.Overview = aOverview;
			if (aPrevious) {
				result.Previous = "p";
			}
			if (aNext) {
				result.Next = "n";
			}
			return result;
		};
		const auto whole  = std::array{ stop("a", 2, true, true), stop("b", 1, false, false), stop("a", 1, false, true), stop("a", 3, true, false) };
		const auto counts = published_tour_stop_counts(whole);
		expect(counts == std::vector<std::optional<std::size_t>>{ 3U, 1U, 3U, 3U }, "a whole tour of three and a tour of one are counted, in the given order");
		const auto missing = std::array{ stop("a", 1, false, true), stop("a", 3, true, false) };
		expect(published_tour_stop_counts(missing) == std::vector<std::optional<std::size_t>>{ std::nullopt, std::nullopt }, "a missing stop leaves the count unknown");
		const auto twice = std::array{ stop("a", 1, false, true), stop("a", 1, false, true) };
		expect(published_tour_stop_counts(twice) == std::vector<std::optional<std::size_t>>{ std::nullopt, std::nullopt }, "a number given twice leaves the count unknown");
		const auto publishing = std::array{ stop("a", 1, false, false), stop("a", 2, false, false) };
		expect(published_tour_stop_counts(publishing) == std::vector<std::optional<std::size_t>>{ std::nullopt, std::nullopt }, "a tour still being published has no count yet");

		// [[BPR-1810]]. Where each stop's Previous and Next lead among the stops at hand.
		using hops            = published_tour_stop_links;
		const auto neighbours = published_tour_stop_neighbours(whole);
		expect(neighbours == std::vector<hops>{ hops{ 2U, 3U }, hops{ }, hops{ std::nullopt, 0U }, hops{ 0U, std::nullopt } },
			   "a whole tour's stops lead to their neighbours by index, in the given order; a tour of one leads nowhere");
		expect(published_tour_stop_neighbours(missing) == std::vector<hops>{ hops{ }, hops{ } }, "a neighbour not at hand is reached by nothing");
		const auto doubled = std::array{ stop("a", 1, false, true), stop("a", 2, true, true), stop("a", 2, true, true), stop("a", 3, true, false) };
		expect(published_tour_stop_neighbours(doubled) == std::vector<hops>{ hops{ }, hops{ 0U, 3U }, hops{ 0U, 3U }, hops{ } },
			   "a number two stops share is reached by nothing, though they reach their own neighbours");
		expect(published_tour_stop_neighbours(publishing) == std::vector<hops>{ hops{ }, hops{ } }, "a stop without its links reaches nothing, though its neighbour is at hand");
		const auto tours = std::array{ stop("a", 1, false, true), stop("b", 2, true, false), stop("b", 1, false, true) };
		expect(published_tour_stop_neighbours(tours) == std::vector<hops>{ hops{ }, hops{ 2U, std::nullopt }, hops{ std::nullopt, 1U } },
			   "a stop of another tour at the next number is not a neighbour");
		expect(published_tour_stop_neighbours(std::span<const published_tour_stop>{ }).empty(), "no stops, no links");

		// The tour's own stop for a comment: by its binding, then by number and title.
		auto tour = review_tour{ };
		for (const auto& [id, title, entity] : std::array{ std::tuple{ 1U, "Start", "PRRC_1" }, std::tuple{ 2U, "Middle", "PRRC_2" }, std::tuple{ 3U, "End", "" } }) {
			auto added    = tour_stop{ };
			added.Id      = tour_stop_id{ id };
			added.Title   = title;
			auto remote   = tour_remote_binding{ };
			remote.Kind   = tour_remote_kind::line_comment;
			remote.Entity = entity;
			added.Remote  = std::move(remote);
			tour.Stops.push_back(std::move(added));
		}
		auto read    = published_tour_stop{ };
		read.Ordinal = 3U;
		read.Title   = "Middle";
		const auto a = find_published_tour_stop(tour, "review-comment:PRRC_2", read);
		expect(a && *a == tour_stop_id{ 2U }, "the stop bound to that very comment is found, whatever its number");
		read.Title   = "End";
		const auto b = find_published_tour_stop(tour, "review-comment:PRRC_9", read);
		expect(b && *b == tour_stop_id{ 3U }, "otherwise the bound stop at its number with its title");
		read.Title = "Renamed";
		expect(!find_published_tour_stop(tour, "review-comment:PRRC_9", read), "a different title at that number is not the stop");
		read.Ordinal = 9U;
		read.Title   = "Start";
		expect(!find_published_tour_stop(tour, "issue-comment:PRRC_1", read), "a conversation comment does not match a line comment's binding");
	}
}

int main()
{
	golden();
	generated();
	files_changed_links();
	navigation_note();
	published_stops();
	std::cout << "TOUR_MARKDOWN_OK\n";
}
