// [[BPR-1779]]. The owner, 2026-10-27: "to me the review is just a bundle of
// comments posted together and what really matters is the comments themselves
// on the files and lines". The decisions only: which reviews take a row, how a
// review's comments are grouped under it, the reading order, what the filters
// act on and what the counts count.
#include "bpr/core/conversation_timeline.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	int  Failures = 0;
	auto Scenario = std::string_view{ };

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: [" << Scenario << "] " << aMessage << '\n';
			++Failures;
		}
	}

	[[nodiscard]] auto review(std::string aId, std::string aAuthor, std::string aAt, const timeline_verdict aVerdict, const bool aBody) -> timeline_item
	{
		return { .Id = aId, .Kind = timeline_item_kind::review, .Review = aId, .Author = std::move(aAuthor), .CreatedAt = std::move(aAt), .Verdict = aVerdict, .HasBody = aBody };
	}

	[[nodiscard]] auto thread(std::string aId, std::string aReview, std::string aAt, std::string aPath, const std::uint32_t aLine) -> timeline_item
	{
		return { .Id = std::move(aId), .Kind = timeline_item_kind::thread, .Review = std::move(aReview), .CreatedAt = std::move(aAt), .Path = std::move(aPath), .Line = aLine };
	}

	[[nodiscard]] auto comment(std::string aId, std::string aAt) -> timeline_item
	{
		return { .Id = std::move(aId), .Kind = timeline_item_kind::pull_request_comment, .CreatedAt = std::move(aAt) };
	}

	[[nodiscard]] auto ids(const conversation_timeline& aTimeline) -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (const auto& row : aTimeline.Rows) {
			const auto prefix = row.Kind == timeline_row_kind::bundle ? "bundle:" : row.Kind == timeline_row_kind::verdict ? "verdict:"
																														   : "";
			result.push_back(prefix + row.Id);
		}
		return result;
	}

	/// The owner's #186: a no-body review with two file comments, two
	/// body-less Commented reviews, two approvals.
	[[nodiscard]] auto owner_186() -> std::vector<timeline_item>
	{
		return {
			review("r-dos", "dosullivan", "2026-10-20T10:00:00Z", timeline_verdict::commented, false),
			thread("t-cpp", "r-dos", "2026-10-20T10:00:00Z", "Client/AppCore/src/Instance.cpp", 40U),
			thread("t-h", "r-dos", "2026-10-20T10:00:00Z", "Client/AppCore/include/AppCore/Instance.h", 12U),
			review("r-fzhu", "fzhu", "2026-10-21T09:00:00Z", timeline_verdict::commented, false),
			review("r-dos2", "dosullivan", "2026-10-21T11:00:00Z", timeline_verdict::commented, false),
			review("r-csw", "cswiedler", "2026-10-22T08:00:00Z", timeline_verdict::approved, false),
			review("r-sau", "saustin", "2026-10-23T08:00:00Z", timeline_verdict::approved, false),
		};
	}

	void bundles()
	{
		Scenario        = "bundles";
		const auto rows = arrange_conversation_timeline(owner_186());
		expect(ids(rows) == std::vector<std::string>{ "bundle:r-dos", "t-h", "t-cpp", "verdict:r-csw", "verdict:r-sau" },
			   "the no-body review's comments sit under its line, in file order; the Commented reviews with nothing add no row; the approvals stay");
		expect(rows.Rows[0].Verdict == timeline_verdict::none && rows.Rows[0].Author == "dosullivan" && rows.Rows[0].CreatedAt == "2026-10-20T10:00:00Z",
			   "a Commented bundle's line names who and when, not the verdict");
		expect(rows.Rows[1].Review == "r-dos" && rows.Rows[1].Item == timeline_item_kind::thread, "a thread names the review it was posted in");
		expect(rows.Rows[3].Verdict == timeline_verdict::approved && rows.Rows[3].Author == "cswiedler", "a verdict line names who gave it");
		expect(rows.Comments == 2U && rows.Threads == 2U && rows.Verdicts == 2U, "the counts count the two comments, not the six reviews");
	}

	void bodies()
	{
		Scenario   = "bodies";
		auto items = std::vector<timeline_item>{
			comment("c-late", "2026-10-25T00:00:00Z"),
			review("r-body", "jiayin", "2026-10-24T00:00:00Z", timeline_verdict::changes_requested, true),
			thread("t-b", "r-body", "2026-10-24T00:00:00Z", "b.cpp", 3U),
			thread("t-a", "r-body", "2026-10-24T00:00:00Z", "a.cpp", 9U),
			comment("c-early", "2026-10-23T00:00:00Z"),
			review("r-said", "tbutler", "2026-10-26T00:00:00Z", timeline_verdict::commented, true),
		};
		const auto rows = arrange_conversation_timeline(items);
		expect(ids(rows) == std::vector<std::string>{ "c-early", "bundle:r-body", "r-body", "t-a", "t-b", "c-late", "bundle:r-said", "r-said" },
			   "time order; a review's body is the first comment of its bundle, its threads follow in file order");
		expect(rows.Rows[1].Verdict == timeline_verdict::changes_requested, "Changes requested is named on its bundle's line");
		expect(rows.Rows[6].Verdict == timeline_verdict::none, "a Commented review with a body names no verdict");
		expect(rows.Comments == 6U && rows.Threads == 2U && rows.Verdicts == 0U, "the body counts as a comment");
	}

	void lone()
	{
		Scenario   = "lone";
		auto items = std::vector<timeline_item>{
			thread("t-orphan", "r-absent", "2026-10-02T00:00:00Z", "z.cpp", 1U),
			thread("t-none", "", "", "y.cpp", 1U),
			comment("c", "2026-10-01T00:00:00Z"),
			review("r-dismissed", "x", "2026-10-03T00:00:00Z", timeline_verdict::dismissed, false),
			review("r-changes", "y", "2026-10-04T00:00:00Z", timeline_verdict::changes_requested, false),
		};
		const auto rows = arrange_conversation_timeline(items);
		expect(ids(rows) == std::vector<std::string>{ "t-none", "c", "t-orphan", "verdict:r-changes" },
			   "a thread whose review is absent stands alone at its own time; a legacy one without a time comes first; a dismissed review with nothing shows nothing");
		expect(rows.Rows[0].Review.empty(), "a lone thread belongs to no bundle");
		expect(arrange_conversation_timeline(items) == rows, "equal items arrange equally");
		auto legacy = std::vector<timeline_item>{
			thread("t-z", "", "", "z.cpp", 9U),
			thread("t-a3", "", "", "a.cpp", 3U),
			comment("c", ""),
			thread("t-a1", "", "", "a.cpp", 1U),
		};
		expect(ids(arrange_conversation_timeline(legacy)) == std::vector<std::string>{ "c", "t-a1", "t-a3", "t-z" },
			   "without times, what is not on a file comes first, then file and line order");
	}

	void filters()
	{
		Scenario          = "filters";
		auto items        = owner_186();
		items[1].Unread   = true;
		items[2].Resolved = true;
		items.push_back(review("r-body", "jiayin", "2026-10-24T00:00:00Z", timeline_verdict::approved, true));
		const auto unread = arrange_conversation_timeline(items, { .UnreadOnly = true });
		expect(ids(unread) == std::vector<std::string>{ "bundle:r-dos", "t-cpp" },
			   "Unread keeps the unread comment under its review's line; verdict lines and read comments go");
		expect(unread.Comments == 1U && unread.Unread == 1U && unread.Verdicts == 0U, "the filtered counts count comments");
		const auto unresolved = arrange_conversation_timeline(items, { .UnresolvedOnly = true });
		expect(ids(unresolved) == std::vector<std::string>{ "bundle:r-dos", "t-cpp", "bundle:r-body", "r-body" },
			   "Hide resolved hides the resolved thread, keeps a review body (it is never resolved) and drops the verdict lines");
		items[2].Kept   = true;
		const auto kept = arrange_conversation_timeline(items, { .UnreadOnly = true });
		expect(ids(kept) == std::vector<std::string>{ "bundle:r-dos", "t-h", "t-cpp" }, "a kept thread (the open one) is never filtered out");
		const auto listed = arrange_conversation_timeline(owner_186(), { .Listed = std::set<std::string>{ "t-h" } });
		expect(ids(listed) == std::vector<std::string>{ "bundle:r-dos", "t-h" }, "a drill-down lists exactly its comments, under their review's line");
		items[1].Unread = false;
		items[2].Kept   = false;
		const auto none = arrange_conversation_timeline(items, { .UnreadOnly = true });
		expect(none.Rows.empty() && none.Comments == 0U, "with nothing unread, no review line is left standing alone");
	}
}

auto main() -> int
{
	bundles();
	bodies();
	lone();
	filters();
	if (Failures != 0) {
		std::cerr << Failures << " expectation(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "CONVERSATION_TIMELINE_OK\n";
	return EXIT_SUCCESS;
}
