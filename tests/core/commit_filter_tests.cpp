// RFC-BPR-0089 M1: commit filter decisions, under core-only (AC08). M3: the
// rematch offers after a force push (AC06) and a split file's row receipt.
#include "bpr/core/commit_filter.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aValue, const char* aMessage)
	{
		if (!aValue) {
			std::cerr << "FAIL: " << aMessage << '\n';
			std::exit(1);
		}
	}

	constexpr auto oid(const std::uint8_t aSeed) noexcept -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0x5AU;
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	auto commit(const std::uint8_t aSeed, std::vector<core::git_commit_oid> aParents, std::string aHeadline)
		-> core::filter_commit
	{
		return core::filter_commit{ .Oid = oid(aSeed), .Parents = std::move(aParents), .Headline = std::move(aHeadline) };
	}

	// The owner's shape: base B0 (unlisted), C1 source, C2 regeneration, C3 follow-ups.
	constexpr auto base = oid(0x10);
	constexpr auto c1   = oid(0x01);
	constexpr auto c2   = oid(0x02);
	constexpr auto c3   = oid(0x03);

	auto owner_list() -> core::filter_commit_list
	{
		return { .Head      = c3,
				 .Commits   = { commit(0x01, { base }, "Change the generator input"),
								commit(0x02, { c1 }, "Regenerate"),
								commit(0x03, { c2 }, "Follow-ups") },
				 .Truncated = false };
	}

	auto owner_graph() -> core::filter_commit_graph
	{
		auto graph = core::build_filter_commit_graph(owner_list());
		expect(graph.has_value(), "the owner's list builds a graph");
		return *graph;
	}

	auto whole(const core::filter_commit_graph& aGraph) -> core::filter_commit_span
	{
		auto span = core::filter_span_for(aGraph, { .WholePullRequest = true, .Left = { }, .Right = { } });
		expect(span.has_value(), "Whole PR always spans the chain");
		return *span;
	}

	auto apply(const core::commit_filter_state& aState, const core::filter_commit_graph& aGraph,
			   const core::commit_filter_operation_kind aKind, std::vector<core::git_commit_oid> aSelection)
		-> std::expected<core::commit_filter_state, core::commit_filter_refusal>
	{
		return core::apply_commit_filter(aState, aGraph, whole(aGraph), { .Kind = aKind, .Selection = std::move(aSelection) });
	}

	auto shown_chain(const core::commit_filter_state& aState, const core::filter_commit_graph& aGraph)
		-> std::vector<core::git_commit_oid>
	{
		auto shown = std::vector<core::git_commit_oid>{ };
		for (const auto& entry : aGraph.Entries) {
			if (entry.Chain && core::commit_state(aState, aGraph, whole(aGraph), entry) == core::filter_commit_state::shown) {
				shown.push_back(entry.Oid);
			}
		}
		return shown;
	}

	void test_graph_roles()
	{
		const auto graph = owner_graph();
		expect(graph.Chain == std::vector{ c1, c2, c3 }, "the chain runs oldest first to the head");
		expect(graph.Fork == base, "the fork is the oldest chain commit's first parent");
		expect(graph.Entries.size() == 3U && graph.Entries[1].Role == core::filter_commit_role::chain, "a linear commit is a chain commit");
		expect(core::chain_parent(graph, 0U) == base && core::chain_parent(graph, 2U) == c2, "a chain commit's parent is the previous one, or the fork");
		expect(!core::chain_parent(graph, 3U).valid(), "no parent off the chain");

		// A merge from the base branch, and a merged topic branch with side commits.
		const auto upstream = oid(0x20);
		const auto t1       = oid(0x31);
		const auto t2       = oid(0x32);
		const auto list     = core::filter_commit_list{
			.Head      = oid(0x05),
			.Commits   = { commit(0x01, { base }, "C1"),
						   commit(0x31, { base }, "T1"),
						   commit(0x32, { t1 }, "T2"),
						   commit(0x04, { c1, upstream }, "Merge main"),
						   commit(0x05, { oid(0x04), t2 }, "Merge topic") },
			.Truncated = false
		};
		const auto merged = core::build_filter_commit_graph(list);
		expect(merged.has_value(), "a graph with merges builds");
		expect(merged->Chain == std::vector{ c1, oid(0x04), oid(0x05) }, "merges stay on the first-parent chain");
		expect(core::find_filter_commit(*merged, oid(0x04))->Role == core::filter_commit_role::base_merge, "a merge of unlisted commits is from the base");
		expect(core::find_filter_commit(*merged, oid(0x05))->Role == core::filter_commit_role::chain_merge, "a merge of listed commits is a topic merge");
		const auto* side = core::find_filter_commit(*merged, t1);
		expect(side->Role == core::filter_commit_role::side && side->Merge == oid(0x05) && !side->Chain, "side commits sit under the merge that reaches them");
		expect(core::find_filter_commit(*merged, t2)->Merge == oid(0x05), "every side commit of the topic sits under its merge");

		// The pull request offers no filter, and says why.
		auto truncated      = owner_list();
		truncated.Truncated = true;
		expect(core::build_filter_commit_graph(truncated).error() == core::commit_filter_unavailable::truncated, "a list truncated at 250 has no filter");
		auto missing = owner_list();
		missing.Head = oid(0x7F);
		expect(core::build_filter_commit_graph(missing).error() == core::commit_filter_unavailable::head_missing, "the head must be listed");
		auto duplicate = owner_list();
		duplicate.Commits.push_back(duplicate.Commits.front());
		expect(core::build_filter_commit_graph(duplicate).error() == core::commit_filter_unavailable::duplicate_commit, "a repeated commit is refused");
		auto octopus = owner_list();
		octopus.Commits.back().Parents.assign(core::maximum_filter_commit_parents + 1U, c2);
		expect(core::build_filter_commit_graph(octopus).error() == core::commit_filter_unavailable::too_many_parents, "too many parents is refused");
		auto root                    = owner_list();
		root.Commits.front().Parents = { };
		expect(core::build_filter_commit_graph(root).error() == core::commit_filter_unavailable::unrelated_history, "a chain reaching a root commit has no filter");
		expect(core::build_filter_commit_graph({ .Head = c3, .Commits = { }, .Truncated = false }).error() == core::commit_filter_unavailable::invalid_list, "an empty list is refused");
		auto cycle                    = owner_list();
		cycle.Commits.front().Parents = { c3 };
		expect(core::build_filter_commit_graph(cycle).error() == core::commit_filter_unavailable::invalid_list, "a parent cycle is refused");

		auto longHeadline                = owner_list();
		longHeadline.Commits[1].Headline = std::string(300U, 'x') + "\xC3\xA9";
		longHeadline.Commits[1].Headline.insert(255U, "\xC3\xA9");
		const auto bounded = core::build_filter_commit_graph(longHeadline);
		expect(bounded && bounded->Headlines[1].size() <= core::maximum_filter_headline_bytes && bounded->Headlines[1].size() == 255U,
			   "a long headline is cut at a UTF-8 boundary");
	}

	void test_spans()
	{
		const auto graph = owner_graph();
		const auto span  = [&](const core::git_commit_oid& aLeft, const core::git_commit_oid& aRight) {
			return core::filter_span_for(graph, { .WholePullRequest = false, .Left = aLeft, .Right = aRight });
		};
		expect(whole(graph) == core::filter_commit_span{ 0U, 3U }, "Whole PR spans all three commits");
		expect(span(base, c3) == core::filter_commit_span{ 0U, 3U }, "the fork to the head spans the chain");
		expect(span(c1, c3) == core::filter_commit_span{ 1U, 3U }, "a previous upload spans what came after it");
		expect(span(base, c2) == core::filter_commit_span{ 0U, 2U }, "an older right endpoint ends the span there");
		expect(span(c3, c1).error() == core::commit_filter_suspension::reversed, "a reversed pair is suspended");
		expect(span(c2, c2).error() == core::commit_filter_suspension::empty_comparison, "an identical pair is suspended");
		expect(span(c2, base).error() == core::commit_filter_suspension::reversed, "the fork on the right is reversed");
		expect(span(oid(0x66), c3).error() == core::commit_filter_suspension::not_first_parent_range, "a commit a force push removed is off the chain");
		expect(span(c1, oid(0x66)).error() == core::commit_filter_suspension::not_first_parent_range, "a right side off the chain is suspended");
	}

	void test_operations()
	{
		const auto graph = owner_graph();
		using kind       = core::commit_filter_operation_kind;

		// AC01: Show only C1.
		auto onlyC1 = apply({ }, graph, kind::show_only, { c1 });
		expect(onlyC1 && shown_chain(*onlyC1, graph) == std::vector{ c1 }, "Show only selected shows exactly the selection");
		expect(onlyC1->Hidden.size() == 2U && onlyC1->Hidden[0].Headline == "Regenerate", "hidden commits keep their headlines");

		// AC02: Show only {C1, C3}, then Hide C3, then Show selected C3.
		auto step = apply({ }, graph, kind::show_only, { c3, c1 });
		expect(step && shown_chain(*step, graph) == std::vector{ c1, c3 }, "Show only {C1, C3}");
		step = apply(*step, graph, kind::hide, { c3 });
		expect(step && shown_chain(*step, graph) == std::vector{ c1 }, "then Hide C3");
		step = apply(*step, graph, kind::show, { c3 });
		expect(step && shown_chain(*step, graph) == std::vector{ c1, c3 }, "then Show selected C3");
		expect(apply(*step, graph, kind::show, { c1 }).error() == core::commit_filter_refusal::nothing_hidden, "Show selected needs a hidden commit in the selection");
		auto cleared = apply(*step, graph, kind::clear, { });
		expect(cleared && cleared->Hidden.empty() && shown_chain(*cleared, graph) == graph.Chain, "Clear all filters shows every commit");

		auto hideC2 = apply({ }, graph, kind::hide, { c2 });
		expect(hideC2 && shown_chain(*hideC2, graph) == std::vector{ c1, c3 }, "Hide selected removes the selection");
		auto none = apply(*hideC2, graph, kind::hide, { c1, c3 });
		expect(none && shown_chain(*none, graph).empty(), "hiding every commit is allowed; the tree is then empty");

		expect(apply({ }, graph, kind::show_only, { }).error() == core::commit_filter_refusal::empty_selection, "an empty selection is refused");
		expect(apply({ }, graph, kind::hide, { oid(0x7E) }).error() == core::commit_filter_refusal::unknown_commit, "an unlisted commit is refused");
		expect(core::commit_filter_operation_available({ }, graph, whole(graph), { .Kind = kind::hide, .Selection = { c2 } }), "availability follows the decision");
		auto bad = core::commit_filter_state{ .Hidden = { { .Oid = c3, .Headline = "" }, { .Oid = c1, .Headline = "" } } };
		expect(!core::valid(bad) && apply(bad, graph, kind::hide, { c2 }).error() == core::commit_filter_refusal::invalid_state, "an unsorted state is refused");

		// Under a narrower comparison, out-of-span selections are ignored and
		// entries outside the span survive a gesture.
		const auto later = *core::filter_span_for(graph, { .WholePullRequest = false, .Left = c1, .Right = c3 });
		expect(core::apply_commit_filter({ }, graph, later, { .Kind = kind::hide, .Selection = { c1 } }).error() == core::commit_filter_refusal::empty_selection,
			   "a commit the comparison does not span cannot be acted on");
		const auto hiddenC1 = apply({ }, graph, kind::hide, { c1 });
		auto       narrowed = core::apply_commit_filter(*hiddenC1, graph, later, { .Kind = kind::show_only, .Selection = { c3, c1 } });
		expect(narrowed && narrowed->Hidden.size() == 2U && narrowed->Hidden[0].Oid == c1 && narrowed->Hidden[1].Oid == c2,
			   "show only under a narrower span keeps the hidden entry outside it");
		const auto c1Entry = *core::find_filter_commit(graph, c1);
		expect(core::commit_state(*narrowed, graph, later, c1Entry) == core::filter_commit_state::outside_comparison, "commits outside the comparison are greyed");

		// A merge from the base branch is never shown alone; side commits act
		// through their merge.
		const auto upstream = oid(0x20);
		const auto t1       = oid(0x31);
		const auto merged   = *core::build_filter_commit_graph({ .Head      = oid(0x05),
																 .Commits   = { commit(0x01, { base }, "C1"), commit(0x31, { base }, "T1"),
																				commit(0x04, { c1, upstream }, "Merge main"),
																				commit(0x05, { oid(0x04), t1 }, "Merge topic") },
																 .Truncated = false });
		expect(apply({ }, merged, kind::show_only, { oid(0x04) }).error() == core::commit_filter_refusal::base_merge_alone, "a base merge cannot be shown on its own");
		expect(apply({ }, merged, kind::show_only, { oid(0x04), c1 }).has_value(), "a base merge may be shown with the pull request's commits");
		expect(apply({ }, merged, kind::hide, { oid(0x04) }).has_value(), "a base merge may be hidden");
		auto viaSide = apply({ }, merged, kind::hide, { t1 });
		expect(viaSide && viaSide->Hidden.size() == 1U && viaSide->Hidden[0].Oid == oid(0x05), "a side commit is hidden through its merge");
		expect(core::commit_state(*viaSide, merged, whole(merged), *core::find_filter_commit(merged, t1)) == core::filter_commit_state::hidden,
			   "the side commit shows as hidden with its merge");
	}

	void test_summary_and_force_push()
	{
		const auto graph  = owner_graph();
		const auto hideC2 = *apply({ }, graph, core::commit_filter_operation_kind::hide, { c2 });
		const auto active = core::summarize_commit_filter(hideC2, graph, std::optional{ core::filter_span_for(graph, { }) });
		expect(active.Status == core::commit_filter_status::active && active.Shown == 2U && active.Spanned == 3U && active.Gone.empty(),
			   "the chip reads 2 of 3 commits shown");
		const auto none = core::summarize_commit_filter({ }, graph, std::optional{ core::filter_span_for(graph, { }) });
		expect(none.Status == core::commit_filter_status::inactive && none.Shown == 3U, "no filter is inactive");

		const auto suspended = core::summarize_commit_filter(
			hideC2, graph, std::optional{ core::filter_span_for(graph, { .WholePullRequest = false, .Left = c3, .Right = c1 }) });
		expect(suspended.Status == core::commit_filter_status::suspended && suspended.Suspension == core::commit_filter_suspension::reversed,
			   "a reversed comparison suspends the filter and says why");

		auto truncated         = owner_list();
		truncated.Truncated    = true;
		const auto unavailable = core::summarize_commit_filter(hideC2, core::build_filter_commit_graph(truncated), std::nullopt);
		expect(unavailable.Status == core::commit_filter_status::unavailable && unavailable.Unavailable == core::commit_filter_unavailable::truncated,
			   "a truncated list's summary says why there is no filter");

		// AC06's M1 part: a force push rewrites C3. Hidden C3 is gone: it stops
		// applying, the chip names it, and its replacement is shown.
		const auto hideC3  = *apply({ }, graph, core::commit_filter_operation_kind::hide, { c3 });
		const auto rewrite = oid(0x13);
		const auto pushed  = *core::build_filter_commit_graph({ .Head      = rewrite,
																.Commits   = { commit(0x01, { base }, "Change the generator input"), commit(0x02, { c1 }, "Regenerate"),
																			   commit(0x13, { c2 }, "Follow-ups, amended") },
																.Truncated = false });
		const auto after   = core::summarize_commit_filter(hideC3, pushed, std::optional{ core::filter_span_for(pushed, { }) });
		expect(after.Status == core::commit_filter_status::inactive && after.Gone.size() == 1U && after.Gone[0].Oid == c3
				   && after.Gone[0].Headline == "Follow-ups" && after.Shown == 3U,
			   "a gone hidden commit stops applying and is named");
		const auto stillHidden = core::summarize_commit_filter(*apply(hideC3, graph, core::commit_filter_operation_kind::hide, { c2 }), pushed,
															   std::optional{ core::filter_span_for(pushed, { }) });
		expect(stillHidden.Status == core::commit_filter_status::active && stillHidden.Shown == 2U && stillHidden.Gone.size() == 1U,
			   "a hidden commit still present keeps applying after the push");
		auto kept = core::apply_commit_filter(hideC3, pushed, whole(pushed), { .Kind = core::commit_filter_operation_kind::hide, .Selection = { c1 } });
		expect(kept && kept->Hidden.size() == 2U && kept->Hidden[1].Oid == c3, "gone entries survive later gestures until Clear all filters");
	}

	auto touch(const core::git_commit_oid& aCommit, std::vector<std::string> aPaths) -> core::filter_commit_touch
	{
		return { .Commit = aCommit, .Paths = std::move(aPaths) };
	}

	constexpr auto blob(const std::uint8_t aSeed) noexcept -> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		bytes[19]  = 0xB1U;
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	auto change(std::string aPath, const std::uint8_t aLeft, const std::uint8_t aRight) -> core::filter_path_change
	{
		auto result  = core::filter_path_change{ };
		result.Path  = std::move(aPath);
		result.Left  = blob(aLeft);
		result.Right = blob(aRight);
		return result;
	}

	auto authored(const std::uint8_t aSeed, std::vector<core::git_commit_oid> aParents, std::string aHeadline, std::string aAuthor)
		-> core::filter_commit
	{
		auto result   = commit(aSeed, std::move(aParents), std::move(aHeadline));
		result.Author = std::move(aAuthor);
		return result;
	}

	/// RFC-BPR-0089 M3, AC06: after a force push rewrites hidden C3, the new
	/// commit carrying the same change is offered, never applied by itself.
	void test_rematch()
	{
		const auto list  = core::filter_commit_list{ .Head      = c3,
													 .Commits   = { authored(0x01, { base }, "Change the generator input", "ana"),
																	authored(0x02, { c1 }, "Regenerate", "ana"), authored(0x03, { c2 }, "Follow-ups", "ana") },
													 .Truncated = false };
		const auto graph = *core::build_filter_commit_graph(list);
		expect(graph.Authors == std::vector<std::string>{ "ana", "ana", "ana" }, "the graph keeps each commit's author");
		const auto c3Changes = std::vector{ change("gen/a.txt", 0x20, 0x21), change("src/app.cpp", 0x30, 0x31) };
		const auto changes   = std::vector<core::filter_commit_changes>{ { .Commit = c1, .Changes = { change("src/app.cpp", 0x29, 0x30) } },
																		 { .Commit = c3, .Changes = c3Changes } };

		// Hide C3 and record what it changes while it is listed.
		auto hidden = *apply({ }, graph, core::commit_filter_operation_kind::hide, { c3 });
		expect(hidden.Hidden.size() == 1U && hidden.Hidden[0].Author == "ana" && !hidden.Hidden[0].Changes, "a hidden entry keeps its author, its changes not yet read");
		hidden = core::record_hidden_changes(hidden, graph, changes);
		expect(hidden.Hidden[0].Changes == std::optional{ c3Changes }, "the trees record what the hidden commit changes");
		expect(core::rematch_offers(hidden, graph, changes).empty(), "nothing is gone, nothing is offered");

		// The force push: C3 reworded (same change) and C4, a commit with C3's old headline and author.
		const auto reworded   = oid(0x13);
		const auto lookalike  = oid(0x14);
		const auto pushed     = *core::build_filter_commit_graph({ .Head      = lookalike,
																   .Commits   = { authored(0x01, { base }, "Change the generator input", "ana"),
																				  authored(0x02, { c1 }, "Regenerate", "ana"),
																				  authored(0x13, { c2 }, "Follow-ups, reworded", "ana"),
																				  authored(0x14, { reworded }, "Follow-ups", "ana") },
																   .Truncated = false });
		const auto newChanges = std::vector<core::filter_commit_changes>{ { .Commit = reworded, .Changes = c3Changes },
																		  { .Commit = lookalike, .Changes = { change("README.md", 0x40, 0x41) } } };
		const auto recorded   = core::record_hidden_changes(hidden, pushed, newChanges);
		expect(recorded == hidden, "a gone entry keeps what it recorded");
		auto offers = core::rematch_offers(hidden, pushed, newChanges);
		expect(offers == std::vector{ core::filter_rematch_offer{ .Gone = c3, .Candidate = reworded, .Basis = core::filter_rematch_basis::same_change } },
			   "the commit with the same paths between the same blobs is offered; a headline match is not when a same change exists");
		// Nothing is applied by the offer itself: the filter still names gone C3 only.
		expect(hidden.Hidden.size() == 1U && hidden.Hidden[0].Oid == c3, "an offer changes nothing");
		const auto rematched = core::apply_rematch(hidden, pushed, newChanges, offers.front());
		expect(rematched && rematched->Hidden.size() == 1U && rematched->Hidden[0].Oid == reworded && rematched->Hidden[0].Changes == std::optional{ c3Changes }
				   && rematched->Hidden[0].Headline == "Follow-ups, reworded",
			   "choosing it hides the new commit in the gone one's place and forgets the gone entry");
		expect(core::summarize_commit_filter(*rematched, pushed, std::optional{ core::filter_span_for(pushed, { }) }).Gone.empty()
				   && core::rematch_offers(*rematched, pushed, newChanges).empty(),
			   "then nothing is gone and nothing more is offered");
		expect(core::apply_rematch(hidden, pushed, newChanges, { .Gone = c3, .Candidate = lookalike, .Basis = core::filter_rematch_basis::same_change }).error()
				   == core::commit_filter_refusal::not_offered,
			   "a rematch the core does not offer is refused");
		expect(core::apply_rematch(*rematched, pushed, newChanges, offers.front()).error() == core::commit_filter_refusal::not_offered,
			   "an offer already taken is refused");

		// Without its changes read, the same change cannot be told: the headline and author match is offered as a guess.
		const auto unread = core::rematch_offers(hidden, pushed, { });
		expect(unread == std::vector{ core::filter_rematch_offer{ .Gone = c3, .Candidate = lookalike, .Basis = core::filter_rematch_basis::same_headline_and_author } },
			   "with no trees read, only a commit with the same headline and author is offered, labelled a guess");
		// A different change is not the same change.
		auto differ                = newChanges;
		differ[0].Changes[1].Right = blob(0x39);
		expect(core::rematch_offers(hidden, pushed, differ).front().Basis == core::filter_rematch_basis::same_headline_and_author,
			   "a path between other blobs is not the same change");
		// A candidate already hidden is not offered; nor is anything once no author was recorded.
		const auto hideLookalike = *core::apply_commit_filter(hidden, pushed, whole(pushed), { .Kind = core::commit_filter_operation_kind::hide, .Selection = { lookalike } });
		expect(core::rematch_offers(hideLookalike, pushed, { }).empty(), "a hidden commit is not offered");
		auto anonymous              = hidden;
		anonymous.Hidden[0].Author  = { };
		anonymous.Hidden[0].Changes = std::nullopt;
		expect(core::rematch_offers(anonymous, pushed, newChanges).empty(), "without recorded changes or author nothing is offered");
		// Malformed changes are never recorded.
		auto unsorted = changes;
		std::swap(unsorted[1].Changes[0], unsorted[1].Changes[1]);
		expect(!core::record_hidden_changes(*apply({ }, graph, core::commit_filter_operation_kind::hide, { c3 }), graph, unsorted).Hidden[0].Changes,
			   "unsorted changes are not recorded");
	}

	/// RFC-BPR-0089 M3 (owner, Q2): a split file's row is as far read as its least read diff.
	void test_split_receipt()
	{
		const auto file    = core::file_identity_of("src/version.txt");
		const auto receipt = [&](const core::file_read_state aRead, const bool aAccepted) {
			auto result     = core::file_receipt{ };
			result.File     = file;
			result.Read     = aRead;
			result.Accepted = aAccepted;
			return std::optional{ result };
		};
		expect(!core::combined_split_receipt(file, std::vector<std::optional<core::file_receipt>>{ std::nullopt, std::nullopt }), "no diff read, no receipt");
		const auto half = core::combined_split_receipt(file, std::vector{ receipt(core::file_read_state::read, false), std::optional<core::file_receipt>{ } });
		expect(half && half->File == file && half->Read == core::file_read_state::unread && !half->Accepted, "one diff read of two leaves the file unread");
		const auto seen = core::combined_split_receipt(file, std::vector{ receipt(core::file_read_state::read, true), receipt(core::file_read_state::seen, true) });
		expect(seen && seen->Read == core::file_read_state::seen && seen->Accepted, "the least read diff says how far the file is read; accepted when both are");
		const auto read = core::combined_split_receipt(file, std::vector{ receipt(core::file_read_state::read, false), receipt(core::file_read_state::read, true) });
		expect(read && read->Read == core::file_read_state::read && !read->Accepted, "read when every diff is read");
	}

	void test_paths()
	{
		const auto graph   = owner_graph();
		const auto touches = std::vector{ touch(c1, { "gen.in", "src/a.cpp", "src/both.cpp" }),
										  touch(c2, { "gen/x.cpp", "gen/y.cpp", "src/both.cpp" }),
										  touch(c3, { "gen/x.cpp", "src/a.cpp", "src/both.cpp" }) };

		// Hide C2: files C2 never touched keep the comparison's own diff.
		const auto hideC2 = *apply({ }, graph, core::commit_filter_operation_kind::hide, { c2 });
		const auto paths  = core::filter_paths(hideC2, graph, whole(graph), touches);
		expect(paths.has_value() && paths->Paths.size() == 4U && paths->Excluded == 1U, "hiding C2 leaves four files and excludes gen/y.cpp");
		expect(paths->Paths[0].Path == "gen.in" && paths->Paths[0].Kind == core::filtered_path_kind::unchanged, "a file C2 never touched is unchanged");
		expect(paths->Paths[1].Path == "gen/x.cpp" && paths->Paths[1].Kind == core::filtered_path_kind::exact && paths->Paths[1].Left == c2
				   && paths->Paths[1].Right == c3,
			   "a file C2 and C3 changed shows only C3's change, exactly");
		expect(paths->Paths[2].Path == "src/a.cpp" && paths->Paths[2].Kind == core::filtered_path_kind::unchanged, "C1 and C3 both shown: unchanged");
		const auto& both = paths->Paths[3];
		expect(both.Path == "src/both.cpp" && both.Kind == core::filtered_path_kind::split && both.Segments.size() == 2U
				   && both.Segments[0].Left == base && both.Segments[0].Right == c1 && both.Segments[0].HiddenAfter == std::vector{ c2 }
				   && both.Segments[1].Left == c2 && both.Segments[1].Right == c3 && both.Segments[1].HiddenAfter.empty(),
			   "a file hidden C2 changed between C1 and C3 splits, naming C2");

		// AC01: Show only C1 gives exactly C1's paths, each as git diff C1^ C1.
		const auto onlyC1 = *apply({ }, graph, core::commit_filter_operation_kind::show_only, { c1 });
		const auto first  = core::filter_paths(onlyC1, graph, whole(graph), touches);
		expect(first && first->Paths.size() == 3U && first->Excluded == 2U, "Show only C1 lists exactly C1's three paths");
		expect(first->Paths[0].Path == "gen.in" && first->Paths[0].Kind == core::filtered_path_kind::unchanged, "a file only C1 changed is its Whole PR diff");
		for (std::size_t index = 1U; index < first->Paths.size(); ++index) {
			expect(first->Paths[index].Kind == core::filtered_path_kind::exact && first->Paths[index].Left == base && first->Paths[index].Right == c1,
				   "a file later commits also changed is C1^..C1");
		}

		// A contiguous shown set never splits a file.
		const auto lastTwo = *apply({ }, graph, core::commit_filter_operation_kind::hide, { c1 });
		const auto tail    = core::filter_paths(lastTwo, graph, whole(graph), touches);
		for (const auto& path : tail->Paths) {
			expect(path.Kind != core::filtered_path_kind::split, "a contiguous filter never splits");
		}
		expect(core::filter_paths(lastTwo, graph, whole(graph), touches) == tail, "the decision is deterministic");

		// Malformed touch sets and an inactive filter are refused.
		expect(core::filter_paths({ }, graph, whole(graph), touches).error() == core::filtered_path_refusal::inactive, "no hidden commit, no filtered set");
		auto reordered = touches;
		std::swap(reordered[0], reordered[1]);
		expect(core::filter_paths(hideC2, graph, whole(graph), reordered).error() == core::filtered_path_refusal::invalid_touches, "touches follow the chain");
		auto unsorted = touches;
		std::swap(unsorted[0].Paths[0], unsorted[0].Paths[1]);
		expect(core::filter_paths(hideC2, graph, whole(graph), unsorted).error() == core::filtered_path_refusal::invalid_touches, "paths are sorted");
		auto repeated        = touches;
		repeated[0].Paths[1] = repeated[0].Paths[0];
		expect(core::filter_paths(hideC2, graph, whole(graph), repeated).error() == core::filtered_path_refusal::invalid_touches, "paths are unique");
		const auto partial = std::vector{ touches[0], touches[1] };
		expect(core::filter_paths(hideC2, graph, whole(graph), partial).error() == core::filtered_path_refusal::invalid_touches, "every spanned commit is covered");

		// A narrower comparison: C1 -> C3 spans C2 and C3 only.
		const auto later    = *core::filter_span_for(graph, { .WholePullRequest = false, .Left = c1, .Right = c3 });
		const auto narrowed = core::filter_paths(hideC2, graph, later, std::vector{ touches[1], touches[2] });
		expect(narrowed && narrowed->Paths.size() == 3U && narrowed->Excluded == 1U, "the span's own commits decide the set");
		expect(narrowed->Paths[0].Path == "gen/x.cpp" && narrowed->Paths[0].Kind == core::filtered_path_kind::exact && narrowed->Paths[0].Left == c2
				   && narrowed->Paths[0].Right == c3,
			   "C2 hidden in C1->C3 leaves C3's own change to a file both changed");
		expect(narrowed->Paths[1].Path == "src/a.cpp" && narrowed->Paths[1].Kind == core::filtered_path_kind::unchanged,
			   "a file only shown C3 changed keeps the comparison's diff");
		expect(narrowed->Paths[2].Path == "src/both.cpp" && narrowed->Paths[2].Kind == core::filtered_path_kind::exact, "and so does src/both.cpp, exactly");
	}

	// RFC-BPR-0089 M2: the bulk label and "Hide files these commits changed".
	void test_bulk_label()
	{
		expect(core::bulk_commit_files == 100U, "the bulk threshold is 100 files");
		expect(!core::bulk_commit(99U) && core::bulk_commit(100U) && core::bulk_commit(321U), "a commit that changes 100 files or more is bulk");
		expect(!core::bulk_commit(0U), "an empty commit is not bulk");
	}

	void test_file_filters()
	{
		const auto graph   = owner_graph();
		const auto touches = std::vector{ touch(c1, { "gen.in", "src/a.cpp" }),
										  touch(c2, { "gen/x.cpp", "gen/y.cpp" }),
										  touch(c3, { "gen/x.cpp", "src/a.cpp", "src/b.cpp" }) };

		// Hide files C2 changed: gen/x.cpp goes from C3 as well.
		const auto hidden = core::hide_files_changed_by({ }, graph, whole(graph), touches, std::vector{ c2 });
		expect(hidden.has_value() && hidden->Hidden == std::vector<std::string>{ "gen/x.cpp", "gen/y.cpp" }, "every path C2 changed is hidden");
		expect(core::valid(*hidden), "the file filter is valid");
		expect(core::file_filter_hides(*hidden, "gen/x.cpp") && !core::file_filter_hides(*hidden, "src/a.cpp"), "the filter answers by path");

		// Combining adds paths; a repeated gesture changes nothing.
		const auto both = core::hide_files_changed_by(*hidden, graph, whole(graph), touches, std::vector{ c1 });
		expect(both && both->Hidden == std::vector<std::string>{ "gen.in", "gen/x.cpp", "gen/y.cpp", "src/a.cpp" }, "file filters combine");
		expect(core::hide_files_changed_by(*both, graph, whole(graph), touches, std::vector{ c1 }) == both, "hiding the same paths again is idempotent");

		// A narrower span ignores commits it does not cover.
		const auto later = *core::filter_span_for(graph, { .WholePullRequest = false, .Left = c2, .Right = c3 });
		const auto none  = core::hide_files_changed_by({ }, graph, later, std::vector{ touches[2] }, std::vector{ c1 });
		expect(!none && none.error() == core::commit_filter_refusal::empty_selection, "a commit outside the comparison hides nothing");

		// Refusals.
		expect(core::hide_files_changed_by({ }, graph, whole(graph), touches, std::vector<core::git_commit_oid>{ }).error()
				   == core::commit_filter_refusal::empty_selection,
			   "an empty selection is refused");
		expect(core::hide_files_changed_by({ }, graph, whole(graph), touches, std::vector{ oid(0x77) }).error() == core::commit_filter_refusal::unknown_commit,
			   "an unlisted commit is refused");
		expect(core::hide_files_changed_by({ }, graph, whole(graph), std::vector{ touches[0] }, std::vector{ c1 }).error()
				   == core::commit_filter_refusal::invalid_state,
			   "touches must cover the span");
		const auto unsorted = core::file_filter_state{ .Hidden = { "b", "a" } };
		expect(!core::valid(unsorted), "an unsorted file filter is not valid");
		expect(core::hide_files_changed_by(unsorted, graph, whole(graph), touches, std::vector{ c1 }).error() == core::commit_filter_refusal::invalid_state,
			   "a malformed file filter is refused");
		expect(!core::valid(core::file_filter_state{ .Hidden = { "" } }), "an empty path is not valid");
	}
} // namespace

auto main() -> int
{
	test_graph_roles();
	test_spans();
	test_operations();
	test_summary_and_force_push();
	test_paths();
	test_bulk_label();
	test_file_filters();
	test_rematch();
	test_split_receipt();
	std::cout << "commit filter decisions pass\n";
	return 0;
}
