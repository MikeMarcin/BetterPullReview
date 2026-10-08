#include "../support/test_expectation.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/overview_feedback.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1089]]. The Overview's label chips from the kept label buckets:
/// every row's numbers for no chips, a typical subset and all twelve, Open and
/// All; the remainder row; a drill-down that lists exactly what a number
/// counted; the numeric sort; and unknown resolution that never reads as a
/// clean zero (RFC-BPR-0049 R21, R23, R24, R27).

namespace {
	using namespace bpr;
	using application::feedback_counter_model;
	using application::github_pull_request_snapshot;
	using application::overview_drill_down;
	using application::overview_feedback_projection;
	using application::overview_feedback_request;
	using application::overview_feedback_row;
	using application::overview_feedback_row_kind;
	using core::feedback_count_kind;
	using core::feedback_count_target;
	using core::feedback_label;
	using core::feedback_resolution_mode;
	using tests::expect;

	auto review_key() -> core::cached_review_key
	{
		core::cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = core::host_profile_id{ 1U };
		key.Host         = core::git_host_id{ 1U };
		key.Account      = core::github_account_id{ 1U };
		key.Locator      = { "octo", "repo" };
		key.Number       = core::pull_request_number{ 7U };
		return key;
	}

	auto actor(std::string aLogin) -> application::github_actor_record
	{
		return { .Actor = application::github_actor_id{ aLogin }, .Login = aLogin };
	}

	// Named locals assigned member by member: GCC 15 at -O3 misreads a
	// designated-initializer temporary with defaulted strings (tests/AGENTS.md).
	auto comment(std::string aId, std::string aLogin, std::string aBody, std::string aPath) -> application::github_review_comment_record
	{
		auto record    = application::github_review_comment_record{ };
		record.Comment = application::github_comment_id{ std::move(aId) };
		record.Author  = actor(std::move(aLogin));
		record.Body    = std::move(aBody);
		record.Path    = std::move(aPath);
		return record;
	}

	auto thread(std::string aId, std::string aPath, bool aResolved, std::string aRoot, std::vector<std::string> aReplies = { }) -> application::github_review_thread_record
	{
		auto record     = application::github_review_thread_record{ };
		record.Thread   = application::github_thread_id{ aId };
		record.Path     = aPath;
		record.Line     = 3U;
		record.Resolved = aResolved;
		record.Comments.push_back(comment(aId + "-root", "reviewer", std::move(aRoot), aPath));
		for (std::size_t index = 0U; index < aReplies.size(); ++index) {
			auto reply    = comment(aId + "-reply-" + std::to_string(index), "author", std::move(aReplies[index]), aPath);
			reply.ReplyTo = application::github_comment_id{ aId + "-root" };
			record.Comments.push_back(std::move(reply));
		}
		return record;
	}

	auto group(std::string aId, std::string aBody, std::string aPath) -> application::github_review_comment_group_record
	{
		auto record = application::github_review_comment_group_record{ };
		record.Root = comment(std::move(aId), "reviewer", std::move(aBody), std::move(aPath));
		return record;
	}

	/// Native threads in four files of three folders, one on a file the pull
	/// request no longer lists, one unanchored; a pull request comment and a
	/// review summary. README.md is listed with no conversation.
	auto snapshot() -> github_pull_request_snapshot
	{
		auto result                      = github_pull_request_snapshot{ };
		result.Viewer                    = actor("reviewer");
		result.ReviewThreadsAvailability = application::github_collection_availability::available;
		result.ReviewThreads             = {
			thread("t1", "src/a/x.cpp", false, "issue (blocking): this leaks the handle", { "question: really?", "Fixed." }),
			thread("t2", "src/a/x.cpp", true, "issue: off by one"),
			thread("t3", "src/a/x.cpp", false, "This could be shorter."),
			thread("t4", "src/a/y.cpp", false, "issue: wrong lock"),
			thread("t5", "src/a/y.cpp", false, "nitpick: spacing"),
			thread("t6", "src/b/z.cpp", false, "question: why a second cache?"),
			thread("t7", "src/b/z.cpp", true, "praise: clear and small"),
			thread("t8", "old/gone.cpp", false, "issue: this file left the pull request"),
			thread("t9", "", false, "question: is this on purpose?"),
			thread("t10", "lib/one.cpp", false, "todo: add the test"),
			thread("t11", "lib/one.cpp", false, "chore: rename the helper"),
			thread("t12", "lib/one.cpp", false, "polish: shorter name"),
		};
		auto issue    = application::github_issue_comment_record{ };
		issue.Comment = application::github_comment_id{ "c1" };
		issue.Author  = actor("reviewer");
		issue.Body    = "note: see the design doc";
		result.IssueComments.push_back(std::move(issue));
		auto summary   = application::github_review_record{ };
		summary.Review = application::github_review_id{ "r1" };
		summary.Author = actor("reviewer");
		summary.State  = application::github_review_state::commented;
		summary.Body   = "thought: the shape is right";
		result.Reviews.push_back(std::move(summary));
		return result;
	}

	const auto listed = std::vector<std::string>{ "README.md", "src/a/x.cpp", "src/a/y.cpp", "src/b/z.cpp", "lib/one.cpp" };

	auto columns(std::vector<feedback_label> aLabels) -> core::feedback_label_columns
	{
		auto made = core::make_feedback_label_columns(aLabels);
		expect(made.has_value(), "a valid chip selection");
		return *made;
	}

	auto all_twelve() -> std::vector<feedback_label>
	{
		auto labels = std::vector<feedback_label>{ };
		for (std::size_t label = 0U; label < core::feedback_label_count; ++label) {
			labels.push_back(static_cast<feedback_label>(label));
		}
		return labels;
	}

	auto request(std::vector<feedback_label> aLabels, feedback_resolution_mode aMode) -> overview_feedback_request
	{
		auto result    = overview_feedback_request{ };
		result.Columns = columns(std::move(aLabels));
		result.Mode    = aMode;
		return result;
	}

	auto row_of(const overview_feedback_projection& aProjection, overview_feedback_row_kind aKind, const std::string& aPath) -> overview_feedback_row
	{
		const auto& rows  = aKind == overview_feedback_row_kind::file ? aProjection.Files : aProjection.Folders;
		const auto  found = std::ranges::find(rows, aPath, &overview_feedback_row::Path);
		expect(found != rows.end(), "the row exists: " + aPath);
		return *found;
	}

	auto drill(const feedback_counter_model& aModel, const overview_feedback_request& aRequest, overview_feedback_row_kind aRow, std::string aPath, feedback_count_target aTarget)
		-> std::vector<std::string>
	{
		auto down          = overview_drill_down{ };
		down.Row           = aRow;
		down.Path          = std::move(aPath);
		down.Target        = aTarget;
		const auto listing = application::list_overview_drill_down(aModel, listed, aRequest, down);
		expect(listing.has_value(), "the drill-down is answered");
		expect(listing->Conversations.size() == listing->Count && listing->Epoch == aModel.population().Epoch && listing->Mode == aRequest.Mode,
			   "a drill-down lists exactly its number's conversations, at the accepted epoch and mode");
		return listing->Conversations;
	}

	/// Every number of one row lists exactly the conversations it counted;
	/// the chips and Other partition the included ones.
	void expect_row_drills_exactly(const feedback_counter_model& aModel, const overview_feedback_request& aRequest, const overview_feedback_row& aRow, const std::string& aWhat)
	{
		const auto& counts   = aRow.Counts;
		const auto  included = drill(aModel, aRequest, aRow.Kind, aRow.Path, { .Kind = feedback_count_kind::included });
		expect(included.size() == counts.Included, aWhat + ": the included list is the included number");
		auto parts = drill(aModel, aRequest, aRow.Kind, aRow.Path, { .Kind = feedback_count_kind::catchall });
		expect(parts.size() == counts.Catchall, aWhat + ": Other lists its number");
		for (const auto& column : counts.Dedicated) {
			const auto ids = drill(aModel, aRequest, aRow.Kind, aRow.Path, { .Kind = feedback_count_kind::dedicated, .Label = column.Label });
			expect(ids.size() == column.Count, aWhat + ": a chip lists its number");
			parts.insert(parts.end(), ids.begin(), ids.end());
		}
		std::ranges::sort(parts);
		expect(parts == included, aWhat + ": the chips and Other are disjoint and together the included list, the remainder once");
		expect(drill(aModel, aRequest, aRow.Kind, aRow.Path, { .Kind = feedback_count_kind::uncertain }).size() == counts.Uncertain, aWhat + ": ? lists its number");
	}

	void test_every_number_drills_into_exactly_its_conversations()
	{
		auto model = feedback_counter_model{ };
		expect(!application::project_overview_feedback(model, listed, request({ }, feedback_resolution_mode::unresolved)), "nothing before a snapshot is admitted");
		expect(model.observe(review_key(), snapshot()).Admitted, "the snapshot is admitted");
		for (const auto& labels : { std::vector<feedback_label>{ }, std::vector{ feedback_label::issue, feedback_label::question }, all_twelve() }) {
			for (const auto mode : { feedback_resolution_mode::unresolved, feedback_resolution_mode::all }) {
				const auto asked      = request(labels, mode);
				const auto projection = application::project_overview_feedback(model, listed, asked);
				expect(projection.has_value() && projection->Files.size() == listed.size() && projection->Folders.size() == 4U && projection->Order.empty(),
					   "one row per listed file and per folder that holds one, in file order");
				const auto what = std::to_string(labels.size()) + " chips, " + std::string{ core::stable_name(mode) };
				auto       rows = std::vector<overview_feedback_row>{ projection->Files.begin(), projection->Files.end() };
				rows.insert(rows.end(), projection->Folders.begin(), projection->Folders.end());
				rows.push_back(projection->Remainder);
				rows.push_back(projection->PullRequest);
				for (const auto& row : rows) {
					expect_row_drills_exactly(model, asked, row, what + ", " + row.Path);
				}
				// The file rows and the remainder count every conversation once.
				auto everyRow = drill(model, asked, overview_feedback_row_kind::remainder, { }, { .Kind = feedback_count_kind::included });
				for (const auto& file : listed) {
					const auto ids = drill(model, asked, overview_feedback_row_kind::file, file, { .Kind = feedback_count_kind::included });
					everyRow.insert(everyRow.end(), ids.begin(), ids.end());
				}
				std::ranges::sort(everyRow);
				expect(everyRow == drill(model, asked, overview_feedback_row_kind::pull_request, { }, { .Kind = feedback_count_kind::included }),
					   what + ": the file rows and Not on a file together are the pull request, each conversation once");
				// A folder row is the sum of the files beneath it.
				const auto folder = row_of(*projection, overview_feedback_row_kind::folder, "src/a");
				expect(folder.Counts.Included == row_of(*projection, overview_feedback_row_kind::file, "src/a/x.cpp").Counts.Included + row_of(*projection, overview_feedback_row_kind::file, "src/a/y.cpp").Counts.Included,
					   what + ": the src/a row is its two files");
			}
		}

		// The typical subset in Open: x.cpp shows issue 1 and Other 1; the
		// resolved issue and the reply's question count nowhere.
		const auto typical = *application::project_overview_feedback(model, listed, request({ feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved));
		const auto x       = row_of(typical, overview_feedback_row_kind::file, "src/a/x.cpp").Counts;
		expect(x.Dedicated[0].Count == 1U && x.Dedicated[1].Count == 0U && x.Catchall == 1U && x.Exact, "x.cpp in Open: one open issue, the plain thread in Other; a reply's label counts nothing");
		expect(drill(model, request({ feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved), overview_feedback_row_kind::file, "src/a/x.cpp",
					 { .Kind = feedback_count_kind::dedicated, .Label = feedback_label::issue })
				   == std::vector<std::string>{ "review-thread:t1" },
			   "the issue chip opens exactly the open issue thread");
		const auto readme = row_of(typical, overview_feedback_row_kind::file, "README.md").Counts;
		expect(readme.Included == 0U && readme.ScopeTotal == 0U && readme.Exact, "a listed file with no conversation is a complete zero");
		const auto& remainder = typical.Remainder.Counts;
		expect(remainder.Dedicated[0].Count == 1U && remainder.Dedicated[1].Count == 1U && remainder.ScopeTotal == 4U && remainder.Uncertain == 2U,
			   "Not on a file: the issue on the unlisted file, the unanchored question, and the comment and summary that are not resolvable");
		// All keeps the breakdown: x.cpp's issue is one open and one resolved.
		const auto all = *application::project_overview_feedback(model, listed, request({ feedback_label::issue }, feedback_resolution_mode::all));
		expect(row_of(all, overview_feedback_row_kind::file, "src/a/x.cpp").Counts.Dedicated[0].States == core::feedback_state_counts{ 1U, 1U, 0U, 0U },
			   "All shows the issue's breakdown: one open, one resolved");
		// Refusals.
		auto down   = overview_drill_down{ };
		down.Row    = overview_feedback_row_kind::file;
		down.Path   = "old/gone.cpp";
		down.Target = { .Kind = feedback_count_kind::included };
		expect(!application::list_overview_drill_down(model, listed, typical.Request, down), "an unlisted file has no row to drill into");
		down.Path   = "src/a/x.cpp";
		down.Target = { .Kind = feedback_count_kind::dedicated, .Label = feedback_label::praise };
		expect(!application::list_overview_drill_down(model, listed, typical.Request, down), "a label without a chip has no drill-down");
	}

	void test_a_new_mode_or_chip_reads_no_conversation()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), snapshot()));
		const auto generation = model.generation();
		const auto open       = application::project_overview_feedback(model, listed, request({ feedback_label::issue }, feedback_resolution_mode::unresolved));
		const auto all        = application::project_overview_feedback(model, listed, request(all_twelve(), feedback_resolution_mode::all));
		expect(open && all && open->Generation == generation && all->Generation == generation && model.generation() == generation,
			   "the projection reads the kept tallies of one generation, whatever the chips and mode");
		const auto same = model.observe(review_key(), snapshot());
		expect(same.Reparsed == 0U && same.Changed.empty(), "the same snapshot again reads no body and changes nothing");
	}

	void test_the_sort_is_numeric_with_folders_first()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), snapshot()));
		auto byIssue      = request({ feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved);
		byIssue.Sort      = feedback_count_target{ .Kind = feedback_count_kind::dedicated, .Label = feedback_label::issue };
		const auto sorted = application::project_overview_feedback(model, listed, byIssue);
		// src/a has two open issues, src/b none, the root none; within src/a,
		// x.cpp and y.cpp tie at one each and keep their order.
		expect(sorted && sorted->Order == std::vector<std::string>{ "src/a/x.cpp", "src/a/y.cpp", "README.md", "src/b/z.cpp", "lib/one.cpp" },
			   "sorted by open issues: src/a first; the tied root, src/b and lib keep their order");
		// By every open conversation, folders lead: src/a's four (two each)
		// come before lib/one.cpp's three, although that file alone has more.
		auto byAll = byIssue;
		byAll.Sort = feedback_count_target{ .Kind = feedback_count_kind::included };
		expect(application::project_overview_feedback(model, listed, byAll)->Order == std::vector<std::string>{ "src/a/x.cpp", "src/a/y.cpp", "lib/one.cpp", "src/b/z.cpp", "README.md" },
			   "sorted by all open conversations: folders by their own number, then their files");
		auto byQuestion = byIssue;
		byQuestion.Sort = feedback_count_target{ .Kind = feedback_count_kind::dedicated, .Label = feedback_label::question };
		expect(application::project_overview_feedback(model, listed, byQuestion)->Order.front() == "src/b/z.cpp", "sorted by open questions: src/b/z.cpp first");
	}

	void test_unknown_resolution_never_reads_as_a_clean_zero()
	{
		// GitHub answered with anonymous comment groups: no resolution at all.
		auto rest                       = github_pull_request_snapshot{ };
		rest.Viewer                     = actor("reviewer");
		rest.ReviewCommentsAvailability = application::github_collection_availability::available;
		rest.ReviewCommentGroups.push_back(group("g1", "issue: unsure", "src/a/x.cpp"));
		rest.ReviewCommentGroups.push_back(group("g2", "Looks fine.", "src/b/z.cpp"));
		auto model = feedback_counter_model{ };
		expect(model.observe(review_key(), rest).Admitted, "the comment groups are admitted");
		const auto  asked      = request({ feedback_label::issue }, feedback_resolution_mode::unresolved);
		const auto  projection = *application::project_overview_feedback(model, listed, asked);
		const auto& whole      = projection.PullRequest.Counts;
		expect(whole.Included == 0U && whole.Uncertain == 2U && !whole.Exact
				   && std::ranges::find(whole.Limits, core::feedback_count_limit::resolution_unknown) != whole.Limits.end(),
			   "Open counts nothing it cannot confirm, and says so: two conversations of unknown state, not exact");
		const auto x = row_of(projection, overview_feedback_row_kind::file, "src/a/x.cpp").Counts;
		expect(x.Included == 0U && x.Uncertain == 1U && !x.Exact, "the file's empty Open count is qualified, never a clean zero");
		expect(drill(model, asked, overview_feedback_row_kind::pull_request, { }, { .Kind = feedback_count_kind::uncertain })
				   == std::vector<std::string>{ "public-preview-comment:g1", "public-preview-comment:g2" },
			   "the ? chip lists both conversations of unknown state");
		// Nothing inline was read: every row, the empty README.md too, is partial.
		auto none   = github_pull_request_snapshot{ };
		none.Viewer = actor("reviewer");
		auto empty  = feedback_counter_model{ };
		static_cast<void>(empty.observe(review_key(), none));
		const auto unread = *application::project_overview_feedback(empty, listed, asked);
		expect(!row_of(unread, overview_feedback_row_kind::file, "README.md").Counts.Exact && !unread.PullRequest.Counts.Exact,
			   "with no inline collection, an empty file's zero is qualified");
	}

	/// \brief [[BPR-1293]]. Threads' label filter lists, per label, the
	/// conversations the Overview's chips count: a conversation is its root's
	/// label, in every state, across the whole pull request.
	void test_each_label_lists_the_conversations_it_counts()
	{
		auto empty = feedback_counter_model{ };
		expect(!application::project_conversation_labels(empty), "nothing is listed before a snapshot is admitted");
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), snapshot()));
		const auto labels = application::project_conversation_labels(model);
		expect(labels.has_value() && labels->Generation == model.generation(), "the lists name the generation they were read at");
		const auto listing = [&labels](const feedback_label aLabel) {
			const auto found = std::ranges::find(labels->Labels, aLabel, &application::conversation_label_listing::Label);
			return found == labels->Labels.end() ? std::vector<std::string>{ } : found->Conversations;
		};
		expect(listing(feedback_label::issue)
				   == std::vector<std::string>{ "review-thread:t1", "review-thread:t2", "review-thread:t4", "review-thread:t8" },
			   "issue lists its four threads, the resolved one and the one off the listed files included");
		expect(listing(feedback_label::question) == std::vector<std::string>{ "review-thread:t6", "review-thread:t9" },
			   "a reply's label is not the conversation's: t1's question reply does not list t1 under question");
		expect(listing(feedback_label::praise) == std::vector<std::string>{ "review-thread:t7" }, "a resolved conversation is listed");
		expect(listing(feedback_label::note) == std::vector<std::string>{ "issue-comment:c1" }
				   && listing(feedback_label::thought) == std::vector<std::string>{ "review:r1" },
			   "pull request comments and review summaries are listed too; Threads keeps only what it shows");
		expect(std::ranges::none_of(labels->Labels, [](const auto& aListing) { return aListing.Conversations.empty(); })
				   && std::ranges::find(labels->Labels, feedback_label::typo, &application::conversation_label_listing::Label) == labels->Labels.end(),
			   "only the labels the pull request uses are listed");
		expect(std::ranges::is_sorted(labels->Labels, { }, [](const auto& aListing) { return static_cast<int>(aListing.Label); }),
			   "in catalog order");
		// The same numbers the Overview's chips give, with every label a chip and All.
		const auto counted = *application::project_overview_feedback(model, listed, request(all_twelve(), feedback_resolution_mode::all));
		for (const auto& entry : labels->Labels) {
			expect(core::feedback_count_of(counted.PullRequest.Counts, { .Kind = feedback_count_kind::dedicated, .Label = entry.Label }) == entry.Conversations.size(),
				   "each list is exactly the pull request's chip count for its label");
		}
	}
}

auto main() -> int
try {
	test_every_number_drills_into_exactly_its_conversations();
	test_a_new_mode_or_chip_reads_no_conversation();
	test_the_sort_is_numeric_with_folders_first();
	test_unknown_resolution_never_reads_as_a_clean_zero();
	test_each_label_lists_the_conversations_it_counts();
	std::cout << "overview-feedback-tests:pass\n";
	return EXIT_SUCCESS;
} catch (...) {
	return bpr::tests::failing_exit();
}
