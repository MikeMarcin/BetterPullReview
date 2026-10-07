#include "../support/test_expectation.hpp"
#include "bpr/application/feedback_counters.hpp"
#include "bpr/application/feedback_sources.hpp"
#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1087]]. Accepted label buckets for one pull request from the
/// accepted read model: the shared feedback adapter, the kept annotations, one
/// tally per pull request, file and folder scope, and the bounded set of scopes
/// a new snapshot re-tallies (RFC-BPR-0049 R25, R26, R27, R29).

namespace {
	using namespace bpr;
	using application::feedback_counter_model;
	using application::feedback_counter_update;
	using application::github_pull_request_snapshot;
	using core::feedback_counter_scope;
	using core::feedback_counter_scope_kind;
	using core::feedback_label;
	using core::feedback_resolution;
	using tests::expect;

	auto review_key(std::uint32_t aNumber = 7U) -> core::cached_review_key
	{
		core::cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = core::host_profile_id{ 1U };
		key.Host         = core::git_host_id{ 1U };
		key.Account      = core::github_account_id{ 1U };
		key.Locator      = { "octo", "repo" };
		key.Number       = core::pull_request_number{ aNumber };
		return key;
	}

	auto actor(std::string aLogin) -> application::github_actor_record
	{
		return { .Actor = application::github_actor_id{ aLogin }, .Login = aLogin };
	}

	/// One native thread: its root, then any replies.
	auto thread(std::string aId, std::string aPath, bool aResolved, std::string aRoot, std::vector<std::string> aReplies = { })
		-> application::github_review_thread_record
	{
		auto record = application::github_review_thread_record{ .Thread = application::github_thread_id{ aId }, .Path = aPath, .Line = 3U, .Resolved = aResolved };
		record.Comments.push_back({ .Comment = application::github_comment_id{ aId + "-root" }, .Author = actor("reviewer"), .Body = std::move(aRoot), .Path = aPath });
		for (std::size_t index = 0U; index < aReplies.size(); ++index) {
			record.Comments.push_back({ .Comment = application::github_comment_id{ aId + "-reply-" + std::to_string(index) },
										.ReplyTo = application::github_comment_id{ aId + "-root" },
										.Author  = actor("author"),
										.Body    = std::move(aReplies[index]),
										.Path    = aPath });
		}
		return record;
	}

	auto issue_comment(std::string aId, std::string aBody) -> application::github_issue_comment_record
	{
		return { .Comment = application::github_comment_id{ std::move(aId) }, .Author = actor("reviewer"), .Body = std::move(aBody) };
	}

	auto review(std::string aId, std::string aBody, application::github_review_state aState = application::github_review_state::commented) -> application::github_review_record
	{
		return { .Review = application::github_review_id{ std::move(aId) }, .Author = actor("reviewer"), .State = aState, .Body = std::move(aBody) };
	}

	auto tour_index() -> std::string
	{
		const auto links = std::vector<core::tour_markdown_link>{ { "Start with the parser", "https://github.com/octo/repo/pull/7#discussion_r1" },
																  { "Then the cache", "https://github.com/octo/repo/pull/7#discussion_r2" } };
		return *core::render_tour_index(links);
	}

	/// Five native threads in three files and one unanchored, two top-level
	/// comments (one of them the generated tour index), one published review
	/// summary and my own pending review.
	auto base_snapshot() -> github_pull_request_snapshot
	{
		auto snapshot                      = github_pull_request_snapshot{ };
		snapshot.Viewer                    = actor("reviewer");
		snapshot.ReviewThreadsAvailability = application::github_collection_availability::available;
		snapshot.ReviewThreads             = {
			thread("t1", "src/a/x.cpp", false, "issue (blocking): this leaks the handle", { "Fixed." }),
			thread("t2", "src/a/y.cpp", true, "suggestion: call it reader"),
			thread("t3", "src/b/z.cpp", false, "praise: clear and small"),
			thread("t4", "", false, "question: why a second cache?"),
			thread("t5", "src/a/x.cpp", false, "This could be shorter."),
		};
		snapshot.IssueComments = { issue_comment("c1", "nitpick: typo in the title"), issue_comment("c2", tour_index()) };
		snapshot.Reviews       = { review("r1", "thought: the shape is right"), review("r2", "todo: tests", application::github_review_state::pending) };
		return snapshot;
	}

	auto pr() -> feedback_counter_scope { return { .Kind = feedback_counter_scope_kind::pull_request }; }
	auto file(std::string aPath) -> feedback_counter_scope { return { .Kind = feedback_counter_scope_kind::file, .Path = std::move(aPath) }; }
	auto folder(std::string aPath) -> feedback_counter_scope { return { .Kind = feedback_counter_scope_kind::folder, .Path = std::move(aPath) }; }
	auto unattributed() -> feedback_counter_scope { return { .Kind = feedback_counter_scope_kind::unattributed }; }

	auto count(const core::feedback_counter_tally& aTally, feedback_label aLabel, feedback_resolution aState) -> std::size_t
	{
		return aTally.Labelled[static_cast<std::size_t>(aLabel)][static_cast<std::size_t>(aState)];
	}

	auto same_counts(const core::feedback_counter_tally& aLeft, const core::feedback_counter_tally& aRight) -> bool
	{
		return aLeft.Valid == aRight.Valid && aLeft.Collection == aRight.Collection && aLeft.Labelled == aRight.Labelled && aLeft.Plain == aRight.Plain
			&& aLeft.Custom == aRight.Custom && aLeft.Unclassified == aRight.Unclassified && aLeft.Unattributed == aRight.Unattributed
			&& aLeft.AttributionUnknown == aRight.AttributionUnknown;
	}

	auto key_of(const feedback_counter_scope& aScope) -> std::pair<int, std::string>
	{
		return { static_cast<int>(aScope.Kind), aScope.Path };
	}

	auto keys(const std::vector<feedback_counter_scope>& aScopes) -> std::set<std::pair<int, std::string>>
	{
		auto result = std::set<std::pair<int, std::string>>{ };
		for (const auto& scope : aScopes) {
			result.insert(key_of(scope));
		}
		return result;
	}

	/// The core's own reading of the whole population, scope by scope: the
	/// pull request, every anchored file and every folder above one, and,
	/// [[BPR-1089]], the unattributed remainder when a unit has no exact file.
	auto oracle(const core::feedback_population& aPopulation) -> std::map<std::pair<int, std::string>, core::feedback_counter_tally>
	{
		auto scopes = std::vector<feedback_counter_scope>{ pr() };
		for (const auto& unit : aPopulation.Units) {
			if (unit.Anchor.Status != core::feedback_anchor_status::file || !unit.Anchor.Path) {
				scopes.push_back(unattributed());
				continue;
			}
			scopes.push_back(file(*unit.Anchor.Path));
			for (auto slash = unit.Anchor.Path->find('/'); slash != std::string::npos; slash = unit.Anchor.Path->find('/', slash + 1U)) {
				if (slash != 0U) {
					scopes.push_back(folder(unit.Anchor.Path->substr(0U, slash)));
				}
			}
		}
		auto result = std::map<std::pair<int, std::string>, core::feedback_counter_tally>{ };
		for (const auto& scope : scopes) {
			result.insert_or_assign(key_of(scope), core::tally_feedback_counters(aPopulation, scope));
		}
		return result;
	}

	/// Every kept tally is the core's full-population tally for its scope, and
	/// the model keeps exactly the scopes the population has.
	void expect_matches_oracle(const feedback_counter_model& aModel, const std::string& aWhat)
	{
		const auto expected = oracle(aModel.population());
		expect(keys(aModel.scopes()).size() == expected.size(), aWhat + ": the model keeps every scope the population has, and no other");
		for (const auto& [key, tally] : expected) {
			const auto* kept = aModel.tally({ .Kind = static_cast<feedback_counter_scope_kind>(key.first), .Path = key.second });
			expect(kept != nullptr && same_counts(*kept, tally) && kept->Epoch == aModel.population().Epoch, aWhat + ": a kept tally equals the core's own for " + key.second);
		}
	}

	void test_the_adapter_reads_the_accepted_model()
	{
		const auto snapshot = base_snapshot();
		const auto sources  = application::observe_feedback_sources(snapshot);
		auto       facts    = std::map<std::string, core::feedback_conversation_facts>{ };
		auto       index    = 0;
		for (const auto& conversation : sources.Conversations) {
			facts.emplace(conversation.Facts.Id, conversation.Facts);
			index += conversation.TourIndex ? 1 : 0;
		}
		expect(sources.Collection == core::feedback_collection::complete, "native threads read are a complete collection");
		expect(facts.at("review-thread:t2").Resolution == feedback_resolution::resolved && facts.at("review-thread:t1").Resolution == feedback_resolution::unresolved,
			   "a native thread carries its own resolution");
		expect(facts.at("review-thread:t4").Anchor.Status == core::feedback_anchor_status::unanchored, "a thread without a path is unanchored, not guessed into a file");
		expect(facts.at("issue-comment:c1").Resolution == feedback_resolution::not_applicable && facts.at("review:r1").Resolution == feedback_resolution::not_applicable,
			   "top-level comments and review summaries are not applicable");
		expect(facts.at("review:r2").Origin == core::feedback_unit_origin::pending_publication, "my own pending review is not published");
		expect(index == 1 && std::ranges::count_if(sources.Conversations, [](const auto& aConversation) { return aConversation.TourIndex; }) == 1
				   && std::ranges::find_if(sources.Conversations, [](const auto& aConversation) { return aConversation.TourIndex; })->Facts.Id == "issue-comment:c2",
			   "the generated tour index, and only it, is a reference");

		auto words = snapshot;
		words.IssueComments[1].Body += "\nPlease start at the parser.\n";
		const auto annotated = application::observe_feedback_sources(words);
		expect(std::ranges::none_of(annotated.Conversations, [](const auto& aConversation) { return aConversation.TourIndex; }),
			   "a person's words around the index make it an ordinary comment");
	}

	void test_projects_accepted_buckets_per_scope()
	{
		auto       model  = feedback_counter_model{ };
		const auto update = model.observe(review_key(), base_snapshot());
		expect(update.Admitted && update.Generation == 1U && update.TourIndexReferences == 1U, "the first snapshot is admitted, with the index set aside");
		expect(model.population().Units.size() == 7U, "five threads, one comment and one published review; not the index or my pending review");
		const auto* whole = model.tally(pr());
		expect(whole != nullptr && count(*whole, feedback_label::issue, feedback_resolution::unresolved) == 1U
				   && count(*whole, feedback_label::suggestion, feedback_resolution::resolved) == 1U
				   && count(*whole, feedback_label::praise, feedback_resolution::unresolved) == 1U
				   && count(*whole, feedback_label::question, feedback_resolution::unresolved) == 1U
				   && count(*whole, feedback_label::nitpick, feedback_resolution::not_applicable) == 1U
				   && count(*whole, feedback_label::thought, feedback_resolution::not_applicable) == 1U
				   && whole->Plain[static_cast<std::size_t>(feedback_resolution::unresolved)] == 1U && count(*whole, feedback_label::todo, feedback_resolution::not_applicable) == 0U,
			   "the pull request counts each accepted conversation once, in its own bucket and state");
		const auto* x = model.tally(file("src/a/x.cpp"));
		expect(x != nullptr && count(*x, feedback_label::issue, feedback_resolution::unresolved) == 1U && x->Plain[static_cast<std::size_t>(feedback_resolution::unresolved)] == 1U
				   && count(*x, feedback_label::question, feedback_resolution::unresolved) == 0U,
			   "a file counts exactly the conversations anchored to it");
		const auto* a   = model.tally(folder("src/a"));
		const auto* src = model.tally(folder("src"));
		expect(a != nullptr && src != nullptr && count(*a, feedback_label::suggestion, feedback_resolution::resolved) == 1U
				   && count(*a, feedback_label::praise, feedback_resolution::unresolved) == 0U && count(*src, feedback_label::praise, feedback_resolution::unresolved) == 1U,
			   "a folder counts its files' conversations, and its parent folder those of every child");
		expect(keys(model.scopes()) == keys({ pr(), file("src/a/x.cpp"), file("src/a/y.cpp"), file("src/b/z.cpp"), folder("src"), folder("src/a"), folder("src/b"), unattributed() }),
			   "the scopes are the pull request, the three files and their folders, and the remainder; the unanchored question has no file");
		const auto* remainder = model.tally(unattributed());
		expect(remainder != nullptr && count(*remainder, feedback_label::question, feedback_resolution::unresolved) == 1U
				   && count(*remainder, feedback_label::nitpick, feedback_resolution::not_applicable) == 1U
				   && count(*remainder, feedback_label::thought, feedback_resolution::not_applicable) == 1U && count(*remainder, feedback_label::issue, feedback_resolution::unresolved) == 0U,
			   "the remainder holds the unanchored question, the pull request comment and the review summary, and no file's conversation");
		expect_matches_oracle(model, "first snapshot");
	}

	void test_a_website_edit_updates_its_own_scopes()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), base_snapshot()));
		auto edited                              = base_snapshot();
		edited.ReviewThreads[1].Comments[0].Body = "issue: call it reader";
		const auto update                        = model.observe(review_key(), edited);
		expect(update.Admitted && update.Reparsed == 1U && update.Reused >= 8U, "only the edited body is read again; every other is reused");
		expect(keys(update.Changed) == keys({ pr(), file("src/a/y.cpp"), folder("src/a"), folder("src") }) && update.Removed.empty() && update.Retallied == 4U,
			   "the edit changes its file, that file's folders and the pull request, and nothing else is tallied");
		expect(update.Generation == 2U && count(*model.tally(file("src/a/y.cpp")), feedback_label::issue, feedback_resolution::resolved) == 1U
				   && count(*model.tally(file("src/a/y.cpp")), feedback_label::suggestion, feedback_resolution::resolved) == 0U,
			   "the conversation moves from one bucket to the other, never counted in both");
		expect_matches_oracle(model, "after a website edit");
	}

	void test_resolving_and_reopening_update_without_a_new_head()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), base_snapshot()));
		auto resolved                      = base_snapshot();
		resolved.ReviewThreads[2].Resolved = true;
		const auto update                  = model.observe(review_key(), resolved);
		expect(update.Reparsed == 0U && keys(update.Changed) == keys({ pr(), file("src/b/z.cpp"), folder("src/b"), folder("src") }),
			   "resolving a thread reads no body and changes only its own scopes");
		expect(count(*model.tally(folder("src/b")), feedback_label::praise, feedback_resolution::resolved) == 1U, "the resolved count follows the thread");
		const auto reopened = model.observe(review_key(), base_snapshot());
		expect(reopened.Reparsed == 0U && keys(reopened.Changed) == keys(update.Changed)
				   && count(*model.tally(folder("src/b")), feedback_label::praise, feedback_resolution::unresolved) == 1U,
			   "reopening moves it back the same way");
		const auto same = model.observe(review_key(), base_snapshot());
		expect(same.Admitted && same.Changed.empty() && same.Removed.empty() && same.Reparsed == 0U && same.Generation == reopened.Generation,
			   "an unchanged snapshot changes nothing and advances no generation");
		expect_matches_oracle(model, "after resolve and reopen");
	}

	void test_a_conversation_leaving_removes_empty_scopes()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), base_snapshot()));
		auto without = base_snapshot();
		without.ReviewThreads.erase(without.ReviewThreads.begin() + 2);
		const auto update = model.observe(review_key(), without);
		expect(keys(update.Removed) == keys({ file("src/b/z.cpp"), folder("src/b") }) && keys(update.Changed) == keys({ pr(), folder("src") })
				   && model.tally(file("src/b/z.cpp")) == nullptr,
			   "a file and folder with no conversation left are removed; the pull request and the parent change");
		expect_matches_oracle(model, "after a thread left");
	}

	void test_columns_and_mode_read_only_kept_tallies()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), base_snapshot()));
		const auto epoch      = model.population().Epoch;
		const auto generation = model.generation();
		const auto narrow     = std::vector<feedback_label>{ feedback_label::issue };
		const auto wide       = std::vector<feedback_label>{ feedback_label::issue, feedback_label::suggestion, feedback_label::praise, feedback_label::question };
		const auto first      = model.project(pr(), narrow, core::feedback_resolution_mode::unresolved);
		const auto second     = model.project(pr(), wide, core::feedback_resolution_mode::all);
		expect(first && second && first->Dedicated.size() == 1U && second->Dedicated.size() == 4U && first->Epoch == epoch && second->Epoch == epoch
				   && first->ScopeTotal == second->ScopeTotal,
			   "a column or mode change repartitions one kept tally over the same population");
		expect(model.generation() == generation && model.population().Epoch == epoch, "choosing columns observes nothing and changes nothing");
		expect(!model.project(file("src/none.cpp"), narrow, core::feedback_resolution_mode::all), "a scope with no conversation has no counts");
		const auto duplicate = std::vector<feedback_label>{ feedback_label::issue, feedback_label::issue };
		expect(!model.project(pr(), duplicate, core::feedback_resolution_mode::all), "a refused projection is no counts, not wrong ones");
	}

	void test_coverage_is_explicit()
	{
		auto rest                       = base_snapshot();
		rest.ReviewThreadsAvailability  = application::github_collection_availability::unavailable;
		rest.ReviewThreads              = { };
		rest.ReviewCommentsAvailability = application::github_collection_availability::available;
		auto root                       = application::github_review_comment_record{ .Comment = application::github_comment_id{ "g1" }, .Author = actor("reviewer"), .Body = "issue: a REST thread", .Path = "src/a/x.cpp" };
		rest.ReviewCommentGroups        = { { .Root = root } };
		auto       model                = feedback_counter_model{ };
		const auto first                = model.observe(review_key(), rest);
		const auto projection           = model.project(file("src/a/x.cpp"), std::vector{ feedback_label::issue }, core::feedback_resolution_mode::unresolved);
		expect(first.Admitted && count(*model.tally(file("src/a/x.cpp")), feedback_label::issue, feedback_resolution::unknown) == 1U && projection
				   && std::ranges::find(projection->Limits, core::feedback_count_limit::resolution_unknown) != projection->Limits.end(),
			   "a REST group's resolution is unknown, and its unresolved counts say they cannot be exact");

		auto unread                      = base_snapshot();
		unread.ReviewThreadsAvailability = application::github_collection_availability::unavailable;
		unread.ReviewThreads             = { };
		auto       partial               = feedback_counter_model{ };
		const auto none                  = partial.observe(review_key(), unread);
		const auto whole                 = partial.project(pr(), std::vector{ feedback_label::nitpick }, core::feedback_resolution_mode::all);
		expect(none.Collection == core::feedback_collection::partial && whole
				   && std::ranges::find(whole->Limits, core::feedback_count_limit::collection_partial) != whole->Limits.end(),
			   "with no inline conversation read the counts are partial");

		auto       model2 = feedback_counter_model{ };
		const auto fresh  = model2.observe(review_key(), base_snapshot());
		const auto stale  = model2.observe(review_key(), base_snapshot(), true);
		expect(stale.Collection == core::feedback_collection::stale && keys(stale.Changed) == keys(model2.scopes()) && stale.Reparsed == 0U
				   && std::ranges::all_of(model2.scopes(), [&](const auto& aScope) { return model2.tally(aScope)->Collection == core::feedback_collection::stale; }),
			   "a kept snapshot marks every scope stale at once, reading no body");
		static_cast<void>(fresh);
	}

	void test_a_different_pull_request_replaces_everything()
	{
		auto model = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), base_snapshot()));
		const auto other = model.observe(review_key(8U), base_snapshot());
		expect(other.Admitted && other.Reparsed == 9U && other.Reused == 0U && keys(other.Changed) == keys(model.scopes()),
			   "another pull request reads every body and tallies every scope afresh");
		expect_matches_oracle(model, "after another pull request");
	}

	/// Random website edits, resolutions, moves, arrivals and departures: after
	/// each, every kept tally is the core's own for its scope, and the update
	/// names exactly the scopes whose counts changed.
	void test_random_changes_match_the_core()
	{
		auto       random   = std::mt19937{ 1087U };
		const auto labels   = std::vector<std::string>{ "issue: a", "suggestion: b", "praise: c", "nitpick: d", "question: e", "plain words", "custom(x): f" };
		const auto paths    = std::vector<std::string>{ "", "a.cpp", "src/a/x.cpp", "src/a/y.cpp", "src/b/z.cpp", "src/b/deep/w.cpp", "docs/readme.md" };
		auto       pick     = [&](std::size_t aSize) { return std::uniform_int_distribution<std::size_t>{ 0U, aSize - 1U }(random); };
		auto       snapshot = base_snapshot();
		auto       model    = feedback_counter_model{ };
		static_cast<void>(model.observe(review_key(), snapshot));
		auto next = 100;
		for (int round = 0; round < 400; ++round) {
			const auto before  = oracle(model.population());
			auto&      threads = snapshot.ReviewThreads;
			switch (pick(5U)) {
			case 0:
				if (!threads.empty()) {
					threads[pick(threads.size())].Comments[0].Body = labels[pick(labels.size())];
				}
				break;
			case 1:
				if (!threads.empty()) {
					auto& chosen    = threads[pick(threads.size())];
					chosen.Resolved = !chosen.Resolved;
				}
				break;
			case 2:
				if (!threads.empty()) {
					auto&      chosen = threads[pick(threads.size())];
					const auto path   = paths[pick(paths.size())];
					chosen.Path       = path;
					for (auto& comment : chosen.Comments) {
						comment.Path = path;
					}
				}
				break;
			case 3:
				threads.push_back(thread("n" + std::to_string(next++), paths[pick(paths.size())], pick(2U) == 0U, labels[pick(labels.size())]));
				break;
			default:
				if (!threads.empty()) {
					threads.erase(threads.begin() + static_cast<std::ptrdiff_t>(pick(threads.size())));
				}
				break;
			}
			const auto update = model.observe(review_key(), snapshot);
			expect(update.Admitted, "a random change is admitted");
			const auto after   = oracle(model.population());
			auto       changed = std::set<std::pair<int, std::string>>{ };
			auto       removed = std::set<std::pair<int, std::string>>{ };
			for (const auto& [key, tally] : after) {
				const auto found = before.find(key);
				if (found == before.end() || !same_counts(found->second, tally)) {
					changed.insert(key);
				}
			}
			for (const auto& [key, tally] : before) {
				if (!after.contains(key)) {
					removed.insert(key);
				}
			}
			expect(keys(update.Changed) == changed && keys(update.Removed) == removed, "round " + std::to_string(round) + ": the update names exactly the changed and removed scopes");
			expect_matches_oracle(model, "round " + std::to_string(round));
		}
	}
} // namespace

auto main() -> int
try {
	test_the_adapter_reads_the_accepted_model();
	test_projects_accepted_buckets_per_scope();
	test_a_website_edit_updates_its_own_scopes();
	test_resolving_and_reopening_update_without_a_new_head();
	test_a_conversation_leaving_removes_empty_scopes();
	test_columns_and_mode_read_only_kept_tallies();
	test_coverage_is_explicit();
	test_a_different_pull_request_replaces_everything();
	test_random_changes_match_the_core();
	std::cout << "feedback-counters-tests:pass\n";
	return EXIT_SUCCESS;
} catch (const std::exception& aError) {
	std::cerr << "feedback counters failure: " << aError.what() << '\n';
	return EXIT_FAILURE;
}
