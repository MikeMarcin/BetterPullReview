// [[BPR-1078]]. Conversation accounting for label counters (RFC-BPR-0049 R21-R26,
// RFC-BPR-0047 R07, pack R43): the shared population, the disjoint partition
// for every column subset, the four resolution states and the coverage limits
// that keep an incomplete count from reading as a clean zero. The pack's
// counter-cases.json units are built as real posts, classified through the
// annotation set and counted; the test's own brute force, written from the
// fixture facts alone, is the expected value.

#include "bpr/core/feedback_partition.hpp"
#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback partition failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "pack file readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto review_key(std::uint64_t aAccount = 1U) -> cached_review_key
	{
		cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = host_profile_id{ 1U };
		key.Host         = git_host_id{ 1U };
		key.Account      = github_account_id{ aAccount };
		key.Locator      = { "octo", "repo" };
		key.Number       = pull_request_number{ 7U };
		return key;
	}

	struct post_text
	{
		post_text(std::string aConversation, std::string aId, std::optional<std::string> aReplyTo, std::string aBody, bool aDeleted = false, feedback_body_segment aSegment = { })
			: Conversation{ std::move(aConversation) }
			, Id{ std::move(aId) }
			, ReplyTo{ std::move(aReplyTo) }
			, Body{ std::move(aBody) }
			, Deleted{ aDeleted }
			, Segment{ aSegment }
		{
		}

		std::string                Conversation;
		std::string                Id;
		std::optional<std::string> ReplyTo;
		std::string                Body;
		bool                       Deleted = false;
		feedback_body_segment      Segment;
	};

	auto annotate(const feedback_annotation_set& aCurrent, const std::vector<post_text>& aPosts) -> feedback_annotation_set
	{
		std::vector<feedback_post_observation> observations;
		std::ranges::transform(aPosts, std::back_inserter(observations), [](const post_text& aPost) {
			return feedback_post_observation{ .Conversation = aPost.Conversation,
											  .Id           = aPost.Id,
											  .ReplyTo      = aPost.ReplyTo ? std::optional<std::string_view>{ *aPost.ReplyTo } : std::nullopt,
											  .Body         = aPost.Body,
											  .Segment      = aPost.Segment,
											  .Deleted      = aPost.Deleted };
		});
		auto refresh = refresh_feedback_annotations(aCurrent, review_key(), observations);
		expect(refresh.Refusal == feedback_refresh_refusal::none, "annotation refresh accepted");
		auto next = adopt_feedback_refresh(aCurrent, std::move(refresh));
		expect(next.has_value(), "annotation refresh adopted");
		return *next;
	}

	auto populate(const feedback_annotation_set& aAnnotations, const std::vector<feedback_conversation_facts>& aFacts, feedback_collection aCollection = feedback_collection::complete) -> feedback_population
	{
		auto result = build_feedback_population(aAnnotations, aFacts, aCollection);
		expect(result.Refusal == feedback_population_refusal::none, "population admitted");
		return std::move(result.Population);
	}

	auto counts_for(const feedback_population& aPopulation, const feedback_counter_scope& aScope, std::vector<feedback_label> aColumns, feedback_resolution_mode aMode) -> feedback_counter_projection
	{
		const auto tally = tally_feedback_counters(aPopulation, aScope);
		auto       view  = project_feedback_counters(tally, aColumns, aMode);
		expect(view.Refusal == feedback_projection_refusal::none, "projection accepted");
		return view;
	}

	auto dedicated(const feedback_counter_projection& aView, feedback_label aLabel) -> std::size_t
	{
		const auto found = std::ranges::find(aView.Dedicated, aLabel, &feedback_dedicated_count::Label);
		expect(found != aView.Dedicated.end(), "column present");
		return found->Count;
	}
	auto state(const feedback_counter_projection& aView, feedback_resolution aState) -> std::size_t
	{
		return aView.States[static_cast<std::size_t>(aState)];
	}
	auto has_limit(const feedback_counter_projection& aView, feedback_count_limit aLimit) -> bool
	{
		return std::ranges::find(aView.Limits, aLimit) != aView.Limits.end();
	}
	auto conserved(const feedback_counter_projection& aView) -> bool
	{
		std::size_t total = aView.Catchall;
		for (const auto& column : aView.Dedicated) {
			total += column.Count;
		}
		const auto& parts = aView.CatchallParts;
		return total == aView.Included && parts.Unselected + parts.Plain + parts.Custom + parts.Unclassified == aView.Catchall;
	}

	const auto pull_request = feedback_counter_scope{ };
	auto       file(std::string aPath) -> feedback_counter_scope
	{
		return { .Kind = feedback_counter_scope_kind::file, .Path = std::move(aPath) };
	}
	auto folder(std::string aPath) -> feedback_counter_scope
	{
		return { .Kind = feedback_counter_scope_kind::folder, .Path = std::move(aPath) };
	}
	auto directory(std::string aPath) -> feedback_counter_scope
	{
		return { .Kind = feedback_counter_scope_kind::directory, .Path = std::move(aPath) };
	}
	const auto unattributed = feedback_counter_scope{ .Kind = feedback_counter_scope_kind::unattributed };
	auto       at_file(std::string aPath, bool aOutdated = false) -> feedback_anchor
	{
		return { .Status = feedback_anchor_status::file, .Path = std::move(aPath), .Outdated = aOutdated };
	}

	// --- The pack fixture ---------------------------------------------------

	/// One fixture record exactly as counter-cases.json states it.
	struct fixture_unit
	{
		std::string                Id;
		std::optional<std::string> Label;
		std::string                Resolution;
		std::optional<std::string> File;
		bool                       Public    = true;
		bool                       Published = true;
		bool                       Deleted   = false;
		bool                       Known     = true;
		bool                       Outdated  = false;
		std::optional<std::string> RawLabel;
		std::optional<std::string> ParseStatus;
		std::vector<std::string>   Replies;
	};

	auto text_member(const cycle_json_value::object& aObject, std::string_view aKey) -> std::optional<std::string>
	{
		const auto* member = find_member(aObject, aKey);
		const auto* text   = member != nullptr ? std::get_if<std::string>(&member->Value) : nullptr;
		return text != nullptr ? std::optional{ *text } : std::nullopt;
	}
	auto flag_member(const cycle_json_value::object& aObject, std::string_view aKey) -> bool
	{
		const auto* member = find_member(aObject, aKey);
		return member != nullptr && std::get<bool>(member->Value);
	}

	auto fixture_units(const cycle_json_value::object& aRoot) -> std::vector<fixture_unit>
	{
		std::vector<fixture_unit> result;
		for (const auto& entry : std::get<cycle_json_value::array>(find_member(aRoot, "units")->Value)) {
			const auto&  unit = std::get<cycle_json_value::object>(entry.Value);
			fixture_unit item{ .Id          = *text_member(unit, "id"),
							   .Label       = text_member(unit, "root_label"),
							   .Resolution  = *text_member(unit, "resolution"),
							   .File        = text_member(unit, "file"),
							   .Public      = *text_member(unit, "visibility") == "public",
							   .Published   = *text_member(unit, "publication") == "published",
							   .Deleted     = flag_member(unit, "deleted"),
							   .Known       = flag_member(unit, "classification_known"),
							   .Outdated    = flag_member(unit, "outdated"),
							   .RawLabel    = text_member(unit, "raw_label"),
							   .ParseStatus = text_member(unit, "parse_status") };
			if (const auto* replies = find_member(unit, "replies_labels")) {
				for (const auto& reply : std::get<cycle_json_value::array>(replies->Value)) {
					item.Replies.push_back(std::get<std::string>(reply.Value));
				}
			}
			result.push_back(std::move(item));
		}
		return result;
	}

	/// Real posts for each record, repeated pages included. A known root is
	/// a body with its label, custom label or no label; a limited root is
	/// pushed past the classifier budget; an unknown root is not observed,
	/// leaving only a reply that names it.
	auto fixture_posts(const std::vector<fixture_unit>& aUnits) -> std::vector<post_text>
	{
		std::vector<post_text> posts;
		for (const auto& unit : aUnits) {
			const auto root = unit.Id + "/root";
			if (unit.Known || unit.ParseStatus) {
				std::string body = "Plain root.";
				if (unit.Label || unit.RawLabel) {
					body = unit.Label.value_or(unit.RawLabel.value_or("")) + ": Root.";
				} else if (unit.ParseStatus == "limited") {
					body = std::string(1100U, '\n') + "issue: Root.";
				}
				posts.emplace_back(unit.Id, root, std::nullopt, body, unit.Deleted);
			} else {
				posts.emplace_back(unit.Id, unit.Id + "/orphan", root, "issue: A reply whose root was not observed.");
			}
			for (std::size_t at = 0U; at < unit.Replies.size(); ++at) {
				posts.emplace_back(unit.Id, unit.Id + "/reply" + std::to_string(at), root, unit.Replies[at] + ": Reply.");
			}
		}
		return posts;
	}

	auto resolution_of(std::string_view aName) -> feedback_resolution
	{
		for (const auto candidate : { feedback_resolution::unresolved, feedback_resolution::resolved, feedback_resolution::unknown, feedback_resolution::not_applicable }) {
			if (stable_name(candidate) == aName) {
				return candidate;
			}
		}
		expect(false, "fixture resolution known");
		return feedback_resolution::unknown;
	}

	auto fixture_facts(const std::vector<fixture_unit>& aUnits) -> std::vector<feedback_conversation_facts>
	{
		std::vector<feedback_conversation_facts> facts;
		std::ranges::transform(aUnits, std::back_inserter(facts), [](const fixture_unit& aUnit) {
			const auto origin = !aUnit.Public ? feedback_unit_origin::private_note : aUnit.Published ? feedback_unit_origin::published
																									 : feedback_unit_origin::draft;
			return feedback_conversation_facts{ .Id         = aUnit.Id,
												.Origin     = origin,
												.Deleted    = aUnit.Deleted,
												.Resolution = resolution_of(aUnit.Resolution),
												.Anchor     = aUnit.File ? at_file(*aUnit.File, aUnit.Outdated) : feedback_anchor{ } };
		});
		return facts;
	}

	/// The expected counts, computed from the fixture records alone.
	struct brute_force
	{
		std::map<std::string, std::size_t> Counts;
		std::size_t                        Catchall = 0U;
		std::size_t                        Included = 0U;
		std::size_t                        Total    = 0U;
		std::map<std::string, std::size_t> States;
		bool                               ClassificationComplete = true;
	};
	auto brute(const std::vector<fixture_unit>& aUnits, const feedback_counter_scope& aScope, const std::vector<std::string>& aColumns, bool aAll) -> brute_force
	{
		brute_force              result;
		std::vector<std::string> seen;
		for (const auto& unit : aUnits) {
			if (std::ranges::find(seen, unit.Id) != seen.end()) {
				continue;
			}
			seen.push_back(unit.Id);
			if (!unit.Public || !unit.Published || unit.Deleted) {
				continue;
			}
			const bool inScope = aScope.Kind == feedback_counter_scope_kind::pull_request
				|| (unit.File && (aScope.Kind == feedback_counter_scope_kind::file ? *unit.File == aScope.Path : unit.File->starts_with(aScope.Path + "/")));
			if (!inScope) {
				continue;
			}
			++result.Total;
			++result.States[unit.Resolution];
			result.ClassificationComplete = result.ClassificationComplete && unit.Known;
			if (!aAll && unit.Resolution != "unresolved") {
				continue;
			}
			++result.Included;
			if (unit.Label && std::ranges::find(aColumns, *unit.Label) != aColumns.end()) {
				++result.Counts[*unit.Label];
			} else {
				++result.Catchall;
			}
		}
		return result;
	}

	auto matches(const feedback_counter_projection& aView, const brute_force& aExpected, std::string_view aWhy) -> bool
	{
		for (const auto& column : aView.Dedicated) {
			const auto found = aExpected.Counts.find(std::string{ stable_name(column.Label) });
			if (column.Count != (found == aExpected.Counts.end() ? 0U : found->second)) {
				std::cerr << aWhy << ": " << stable_name(column.Label) << " counts " << column.Count << '\n';
				return false;
			}
		}
		for (const auto candidate : { feedback_resolution::unresolved, feedback_resolution::resolved, feedback_resolution::unknown, feedback_resolution::not_applicable }) {
			const auto found = aExpected.States.find(std::string{ stable_name(candidate) });
			if (state(aView, candidate) != (found == aExpected.States.end() ? 0U : found->second)) {
				return false;
			}
		}
		return aView.Catchall == aExpected.Catchall && aView.Included == aExpected.Included && aView.ScopeTotal == aExpected.Total
			&& (aView.ScopeUnclassified == 0U) == aExpected.ClassificationComplete && conserved(aView);
	}

	auto scope_of(const cycle_json_value::object& aScope) -> feedback_counter_scope
	{
		const auto kind = *text_member(aScope, "kind");
		if (kind == "pr") {
			return pull_request;
		}
		return kind == "file" ? file(*text_member(aScope, "path")) : folder(*text_member(aScope, "path"));
	}

	/// The fixture's declared cases, all 4,096 subsets in every declared
	/// scope and mode, and the worked examples of COUNTER-EXAMPLES.md.
	auto pack_cases(const std::filesystem::path& aPack) -> std::size_t
	{
		const auto parsed = parse_cycle_json(read_file(aPack / "fixtures" / "counter-cases.json"), 1U << 20U);
		expect(parsed.has_value(), "counter-cases.json parses");
		const auto& root  = std::get<cycle_json_value::object>(parsed->Value);
		const auto  units = fixture_units(root);
		expect(units.size() == 28U, "28 fixture records, one a repeated page");

		const auto annotations = annotate({ }, fixture_posts(units));
		const auto population  = populate(annotations, fixture_facts(units));
		expect(population.Excluded == feedback_population_exclusions{ .Private = 1U, .Unpublished = 1U, .Deleted = 1U, .Collapsed = 1U }, "private, draft, deleted and the repeated page are left out");
		expect(population.Units.size() == 24U && population.Epoch == annotations.Epoch && population.Scope == review_key(), "24 shared conversations at the set's epoch");

		for (const auto& entry : std::get<cycle_json_value::array>(find_member(root, "cases")->Value)) {
			const auto&                 item = std::get<cycle_json_value::object>(entry.Value);
			const auto                  id   = *text_member(item, "id");
			std::vector<feedback_label> columns;
			for (const auto& column : std::get<cycle_json_value::array>(find_member(item, "columns")->Value)) {
				columns.push_back(*find_feedback_label(std::get<std::string>(column.Value)));
			}
			const bool  all      = *text_member(item, "mode") == "all";
			const auto  view     = counts_for(population, scope_of(std::get<cycle_json_value::object>(find_member(item, "scope")->Value)), columns, all ? feedback_resolution_mode::all : feedback_resolution_mode::unresolved);
			const auto& expected = std::get<cycle_json_value::object>(find_member(item, "expected")->Value);
			const auto  number   = [&](const cycle_json_value::object& aObject, std::string_view aKey) {
				return static_cast<std::size_t>(std::get<std::int64_t>(find_member(aObject, aKey)->Value));
			};
			const auto& counts = std::get<cycle_json_value::object>(find_member(expected, "counts")->Value);
			const auto& states = std::get<cycle_json_value::object>(find_member(expected, "scope_states")->Value);
			for (const auto& column : view.Dedicated) {
				expect(column.Count == number(counts, stable_name(column.Label)), id + ": dedicated count");
			}
			for (const auto candidate : { feedback_resolution::unresolved, feedback_resolution::resolved, feedback_resolution::unknown, feedback_resolution::not_applicable }) {
				expect(state(view, candidate) == number(states, stable_name(candidate)), id + ": scope state");
			}
			expect(view.Included == number(expected, "included_total") && view.Catchall == number(expected, "catchall") && view.ScopeTotal == number(expected, "scope_total"), id + ": totals");
			expect((view.ScopeUnclassified == 0U) == flag_member(expected, "classification_complete") && (state(view, feedback_resolution::unknown) == 0U) == flag_member(expected, "resolution_complete"), id + ": coverage");
			expect(!view.Exact && has_limit(view, feedback_count_limit::classification_partial) && has_limit(view, feedback_count_limit::resolution_unknown) == !all, id + ": never a clean count");
		}

		std::size_t projections = 0U;
		for (const auto& scopeEntry : std::get<cycle_json_value::array>(find_member(root, "exhaustive_scopes")->Value)) {
			const auto where = scope_of(std::get<cycle_json_value::object>(scopeEntry.Value));
			const auto tally = tally_feedback_counters(population, where);
			for (const auto& modeEntry : std::get<cycle_json_value::array>(find_member(root, "exhaustive_modes")->Value)) {
				const bool all = std::get<std::string>(modeEntry.Value) == "all";
				for (std::uint32_t mask = 0U; mask < (1U << feedback_label_count); ++mask) {
					std::vector<feedback_label> columns;
					std::vector<std::string>    names;
					for (const auto& definition : feedback_label_catalog()) {
						if ((mask & (1U << static_cast<std::uint32_t>(definition.Label))) != 0U) {
							columns.push_back(definition.Label);
							names.emplace_back(definition.Id);
						}
					}
					const auto view = project_feedback_counters(tally, columns, all ? feedback_resolution_mode::all : feedback_resolution_mode::unresolved);
					expect(matches(view, brute(units, where, names, all), "subset"), "every subset partitions exactly: " + where.Path + " mask " + std::to_string(mask));
					++projections;
				}
			}
		}

		// COUNTER-EXAMPLES.md, src/cache.cpp.
		const std::vector<feedback_label> twelve{ feedback_label::praise, feedback_label::nitpick, feedback_label::suggestion, feedback_label::issue, feedback_label::todo, feedback_label::question,
												  feedback_label::thought, feedback_label::chore, feedback_label::note, feedback_label::typo, feedback_label::polish, feedback_label::quibble };
		const auto                        cache = file("src/cache.cpp");
		expect(counts_for(population, cache, { }, feedback_resolution_mode::unresolved).Catchall == 7U && counts_for(population, cache, { }, feedback_resolution_mode::all).Catchall == 10U, "no columns: catch-all is the whole population");
		const auto open = counts_for(population, cache, twelve, feedback_resolution_mode::unresolved);
		expect(dedicated(open, feedback_label::issue) == 2U && dedicated(open, feedback_label::question) == 1U && dedicated(open, feedback_label::suggestion) == 1U && open.Catchall == 3U, "all twelve, unresolved");
		expect(open.CatchallParts == feedback_catchall_parts{ .Plain = 1U, .Custom = 1U, .Unclassified = 1U }, "catch-all holds plain, custom and the missing root, each named");
		const auto every = counts_for(population, cache, twelve, feedback_resolution_mode::all);
		expect(dedicated(every, feedback_label::issue) == 4U && dedicated(every, feedback_label::note) == 1U && every.Catchall == 3U, "all twelve, all states");
		expect(state(every, feedback_resolution::unknown) == 1U && dedicated(open, feedback_label::issue) == 2U, "the unknown-resolution issue is outside unresolved but visible as unknown");
		return projections;
	}

	/// Exact attribution: file rows plus the unattributed remainder make the
	/// pull request, folders sum their files once, outdated anchors count.
	void attribution(const std::filesystem::path& aPack)
	{
		const auto parsed = parse_cycle_json(read_file(aPack / "fixtures" / "counter-cases.json"), 1U << 20U);
		const auto units  = fixture_units(std::get<cycle_json_value::object>(parsed->Value));
		const auto set    = populate(annotate({ }, fixture_posts(units)), fixture_facts(units));
		for (const auto mode : { feedback_resolution_mode::unresolved, feedback_resolution_mode::all }) {
			const auto whole  = counts_for(set, pull_request, { feedback_label::issue }, mode);
			const auto cache  = counts_for(set, file("src/cache.cpp"), { feedback_label::issue }, mode);
			const auto render = counts_for(set, file("src/render.cpp"), { feedback_label::issue }, mode);
			const auto source = counts_for(set, folder("src"), { feedback_label::issue }, mode);
			const auto top    = counts_for(set, folder(""), { feedback_label::issue }, mode);
			expect(whole.Included == cache.Included + render.Included + whole.Unattributed && whole.Unattributed == (mode == feedback_resolution_mode::all ? 3U : 1U), "files plus the unattributed remainder make the pull request");
			expect(source.Included == cache.Included + render.Included && top.Included == source.Included && source.Unattributed == 0U, "a folder counts each child conversation once");
			expect(dedicated(whole, feedback_label::issue) == dedicated(cache, feedback_label::issue) + dedicated(render, feedback_label::issue) + 1U, "the unanchored issue is only pull-request wide");
		}
		const auto withOutdated = counts_for(set, file("src/cache.cpp"), { feedback_label::issue }, feedback_resolution_mode::unresolved);
		expect(dedicated(withOutdated, feedback_label::issue) == 2U, "the outdated issue counts for its exact file");
	}

	// --- Synthetic rules ------------------------------------------------------

	/// An annotation set holding the given root classifications directly.
	auto classified(std::vector<feedback_conversation> aConversations) -> feedback_annotation_set
	{
		std::ranges::sort(aConversations, { }, &feedback_conversation::Id);
		return { .Scope = review_key(), .Epoch = 1U, .Posts = { }, .Conversations = std::move(aConversations) };
	}
	auto labelled(std::string aId, feedback_label aLabel) -> feedback_conversation
	{
		return { .Id = std::move(aId), .Status = feedback_root_status::labelled, .Root = "r", .Label = aLabel };
	}
	auto facts(std::string aId, feedback_resolution aResolution, feedback_anchor aAnchor = at_file("a.cpp")) -> feedback_conversation_facts
	{
		return { .Id = std::move(aId), .Resolution = aResolution, .Anchor = std::move(aAnchor) };
	}

	void inclusion_rules()
	{
		const auto set = classified({ labelled("a", feedback_label::issue), labelled("b", feedback_label::issue), labelled("c", feedback_label::issue), labelled("d", feedback_label::issue), labelled("e", feedback_label::issue), { .Id = "f", .Status = feedback_root_status::root_deleted, .Root = "r" } });
		auto       all = std::vector{ facts("a", feedback_resolution::unresolved), facts("b", feedback_resolution::unresolved), facts("c", feedback_resolution::unresolved),
									  facts("d", feedback_resolution::unresolved), facts("e", feedback_resolution::unresolved), facts("f", feedback_resolution::resolved) };
		all[1].Origin  = feedback_unit_origin::private_note;
		all[2].Origin  = feedback_unit_origin::draft;
		all[3].Origin  = feedback_unit_origin::pending_publication;
		all[4].Deleted = true;
		all.push_back(all[0]);
		const auto population = populate(set, all);
		expect(population.Excluded == feedback_population_exclusions{ .Private = 1U, .Unpublished = 2U, .Deleted = 1U, .Collapsed = 1U } && population.Units.size() == 2U, "only published, current conversations are shared, each once");
		const auto view = counts_for(population, pull_request, { feedback_label::issue }, feedback_resolution_mode::all);
		expect(dedicated(view, feedback_label::issue) == 1U && view.CatchallParts.Unclassified == 1U && state(view, feedback_resolution::resolved) == 1U, "a deleted root keeps its thread, in the catch-all, with its own resolution");
		expect(has_limit(view, feedback_count_limit::classification_partial) && !view.Exact, "a deleted root leaves label coverage incomplete");

		const auto refused = [&](std::vector<feedback_conversation_facts> aFacts, feedback_population_refusal aRefusal, std::string_view aWhy) {
			const auto result = build_feedback_population(set, aFacts, feedback_collection::complete);
			expect(result.Refusal == aRefusal && result.Population.Units.empty(), aWhy);
		};
		auto changed = facts("a", feedback_resolution::resolved);
		refused({ facts("a", feedback_resolution::unresolved), changed }, feedback_population_refusal::contradictory_duplicate, "a page that disagrees refuses the population");
		refused({ facts("", feedback_resolution::unresolved) }, feedback_population_refusal::invalid_identity, "empty identity refused");
		refused({ facts("a", feedback_resolution::unresolved, { .Status = feedback_anchor_status::file }) }, feedback_population_refusal::invalid_anchor, "file anchor without path refused");
		refused({ facts("a", feedback_resolution::unresolved, { .Status = feedback_anchor_status::unanchored, .Path = "a.cpp" }) }, feedback_population_refusal::invalid_anchor, "unanchored with a path refused");
		for (const auto* path : { "", "/a.cpp", "src/", "src//a.cpp" }) {
			refused({ facts("a", feedback_resolution::unresolved, at_file(path)) }, feedback_population_refusal::invalid_anchor, std::string{ "unclean path refused: " } + path);
		}
		expect(build_feedback_population({ }, { }, feedback_collection::complete).Refusal == feedback_population_refusal::invalid_scope, "a set with no scope refused");

		for (const auto& where : { file(""), file("/a.cpp"), folder("src/"), folder("/"), feedback_counter_scope{ .Path = "a.cpp" } }) {
			expect(project_feedback_counters(tally_feedback_counters(population, where), { }, feedback_resolution_mode::all).Refusal == feedback_projection_refusal::invalid_tally, "invalid scope refused: " + where.Path);
		}
		const std::array twice{ feedback_label::issue, feedback_label::note, feedback_label::issue };
		expect(project_feedback_counters(tally_feedback_counters(population, pull_request), twice, feedback_resolution_mode::all).Refusal == feedback_projection_refusal::duplicate_column, "a column selected twice refused");

		// Folder boundaries are path components, not string prefixes.
		const auto paths = classified({ labelled("1", feedback_label::note), labelled("2", feedback_label::note), labelled("3", feedback_label::note), labelled("4", feedback_label::note) });
		const auto tree  = populate(paths, { facts("1", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("2", feedback_resolution::unresolved, at_file("src/x/b.cpp")), facts("3", feedback_resolution::unresolved, at_file("srcfoo/c.cpp")), facts("4", feedback_resolution::unresolved, at_file("src")) });
		expect(counts_for(tree, folder("src"), { }, feedback_resolution_mode::all).Included == 2U && counts_for(tree, folder("src/x"), { }, feedback_resolution_mode::all).Included == 1U
				   && counts_for(tree, folder(""), { }, feedback_resolution_mode::all).Included == 4U && counts_for(tree, file("src"), { }, feedback_resolution_mode::all).Included == 1U,
			   "folders hold files below them only");

		// Column order is the caller's; the default mode is unresolved.
		const auto ordered = counts_for(population, pull_request, { feedback_label::quibble, feedback_label::issue, feedback_label::praise }, default_feedback_resolution_mode);
		expect(ordered.Mode == feedback_resolution_mode::unresolved && ordered.Dedicated.size() == 3U && ordered.Dedicated[0].Label == feedback_label::quibble && ordered.Dedicated[2].Label == feedback_label::praise, "selected order kept, unresolved by default");
	}

	/// Missing resolution, root, anchor or collection never give an exact
	/// clean result; a complete empty scope does.
	void coverage()
	{
		const auto set   = classified({ labelled("a", feedback_label::issue), labelled("b", feedback_label::issue) });
		const auto rest  = populate(set, { facts("a", feedback_resolution::unknown), facts("b", feedback_resolution::unknown) });
		const auto noRes = counts_for(rest, pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved);
		expect(noRes.Included == 0U && dedicated(noRes, feedback_label::issue) == 0U && !noRes.Exact && noRes.Limits == std::vector{ feedback_count_limit::resolution_unknown } && state(noRes, feedback_resolution::unknown) == 2U, "no resolution information is not 0 unresolved");
		const auto restAll = counts_for(rest, pull_request, { feedback_label::issue }, feedback_resolution_mode::all);
		expect(dedicated(restAll, feedback_label::issue) == 2U && restAll.Exact, "all states count them exactly, with their state visible");

		const auto missing = populate(set, { facts("a", feedback_resolution::unresolved), facts("z", feedback_resolution::unresolved) });
		const auto noRoot  = counts_for(missing, pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved);
		expect(dedicated(noRoot, feedback_label::issue) == 1U && noRoot.CatchallParts.Unclassified == 1U && noRoot.Limits == std::vector{ feedback_count_limit::classification_partial }, "a conversation with no observed root is unclassified, never plain");
		const auto plain = populate(classified({ { .Id = "p", .Status = feedback_root_status::plain, .Root = "r" } }), { facts("p", feedback_resolution::unresolved) });
		expect(counts_for(plain, pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved).Exact, "a truly plain root is a known catch-all");

		const auto lost     = populate(set, { facts("a", feedback_resolution::unresolved), facts("b", feedback_resolution::unresolved, { .Status = feedback_anchor_status::unknown }) });
		const auto lostWide = counts_for(lost, pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved);
		const auto lostFile = counts_for(lost, file("a.cpp"), { feedback_label::issue }, feedback_resolution_mode::unresolved);
		expect(dedicated(lostWide, feedback_label::issue) == 2U && lostWide.Unattributed == 1U && lostWide.Exact, "a lost anchor stays visible pull-request wide");
		expect(dedicated(lostFile, feedback_label::issue) == 1U && lostFile.Limits == std::vector{ feedback_count_limit::attribution_unknown }, "and is never guessed into a file, whose count is qualified");
		expect(counts_for(lost, folder(""), { }, feedback_resolution_mode::all).Limits == std::vector{ feedback_count_limit::attribution_unknown }, "folders too");
		expect(counts_for(lost, file("b.cpp"), { feedback_label::issue }, feedback_resolution_mode::unresolved).Limits == std::vector{ feedback_count_limit::attribution_unknown }, "even a file with no counted unit");

		const std::array partialLimits{ std::pair{ feedback_collection::partial, feedback_count_limit::collection_partial }, std::pair{ feedback_collection::stale, feedback_count_limit::collection_stale },
										std::pair{ feedback_collection::unavailable, feedback_count_limit::collection_unavailable } };
		for (const auto& [collection, limit] : partialLimits) {
			const auto empty = counts_for(populate(set, { }, collection), pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved);
			expect(empty.Included == 0U && !empty.Exact && empty.Limits == std::vector{ limit }, std::string{ "an incomplete collection is never a clean zero: " } + std::string{ stable_name(collection) });
		}
		const auto clean = counts_for(populate(set, { }), pull_request, { feedback_label::issue }, feedback_resolution_mode::unresolved);
		expect(clean.Included == 0U && clean.Exact, "a complete empty scope is an exact zero");

		const auto topLevel = populate(classified({ labelled("t", feedback_label::note) }), { facts("t", feedback_resolution::not_applicable, { }) });
		const auto general  = counts_for(topLevel, pull_request, { feedback_label::note }, feedback_resolution_mode::unresolved);
		expect(general.Included == 0U && state(general, feedback_resolution::not_applicable) == 1U && state(general, feedback_resolution::resolved) == 0U, "a top-level note is not applicable, not resolved");
	}

	/// Website edits, resolve and reopen, outdating, deletion and partial
	/// paging move only the units they concern.
	void transitions()
	{
		std::vector<post_text> posts{
			{ "T1", "r1", std::nullopt, "issue: Root one." },
			{ "T1", "p1", "r1", "praise: Reply." },
			{ "T2", "r2", std::nullopt, "note: Root two." },
			{ "G", "g", std::nullopt, "Plain top-level comment." },
		};
		std::vector<feedback_conversation_facts> conversations{ facts("T1", feedback_resolution::unresolved), facts("T2", feedback_resolution::resolved, at_file("b.cpp")),
																facts("G", feedback_resolution::not_applicable, { }) };
		const std::vector                        columns{ feedback_label::issue, feedback_label::question };
		auto                                     set   = annotate({ }, posts);
		const auto                               count = [&](feedback_resolution_mode aMode, feedback_collection aCollection = feedback_collection::complete) {
			return counts_for(populate(set, conversations, aCollection), pull_request, columns, aMode);
		};
		const auto base = count(feedback_resolution_mode::all);
		expect(base.Dedicated[0].Count == 1U && base.Included == 3U && base.Exact && base.Epoch == set.Epoch, "initial counts at the set's epoch");

		posts[0].Body    = "question: Root one.";
		set              = annotate(set, posts);
		const auto moved = count(feedback_resolution_mode::all);
		expect(moved.Dedicated[0].Count == 0U && moved.Dedicated[1].Count == 1U && moved.Included == base.Included && moved.Epoch == set.Epoch, "a root edit moves one bucket and keeps the total");

		posts[1].Body        = "issue: Reply now says issue.";
		set                  = annotate(set, posts);
		const auto replyEdit = count(feedback_resolution_mode::all);
		expect(replyEdit.Dedicated == moved.Dedicated && replyEdit.Catchall == moved.Catchall, "a reply label changes no count");

		const auto openBefore       = count(feedback_resolution_mode::unresolved);
		conversations[0].Resolution = feedback_resolution::resolved;
		const auto resolved         = count(feedback_resolution_mode::unresolved);
		const auto resolvedAll      = count(feedback_resolution_mode::all);
		expect(openBefore.Included == 1U && resolved.Included == 0U && resolvedAll.Included == base.Included && state(resolvedAll, feedback_resolution::resolved) == 2U, "a resolve leaves unresolved, all states unchanged");
		conversations[0].Resolution = feedback_resolution::unresolved;
		expect(count(feedback_resolution_mode::unresolved).Dedicated == openBefore.Dedicated, "a reopen reverses it");

		conversations[0].Anchor.Outdated = true;
		expect(count(feedback_resolution_mode::unresolved).Dedicated == openBefore.Dedicated, "outdating changes nothing");

		posts[0].Deleted       = true;
		set                    = annotate(set, posts);
		const auto rootDeleted = count(feedback_resolution_mode::unresolved);
		expect(rootDeleted.Dedicated[1].Count == 0U && rootDeleted.CatchallParts.Unclassified == 1U && rootDeleted.Included == 1U && !rootDeleted.Exact, "a deleted root moves to the qualified catch-all, still unresolved");

		conversations[0].Deleted = true;
		const auto gone          = count(feedback_resolution_mode::all);
		expect(gone.Included == base.Included - 1U && gone.Exact, "a deleted conversation leaves the population");

		const auto shortPage = std::vector{ conversations[1] };
		const auto partial   = counts_for(populate(set, shortPage, feedback_collection::partial), pull_request, columns, feedback_resolution_mode::all);
		expect(partial.Included == 1U && partial.Limits == std::vector{ feedback_count_limit::collection_partial }, "a short page is a qualified count, not an exact one");
	}

	/// Published tour stops are ordinary conversations: a plain stop is a
	/// known catch-all, a labelled stop counts in its bucket, and an
	/// envelope whose narrative was not mapped is never stripped by pattern.
	void tour_roots()
	{
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       plainStop    = *render_tour_stop("Why", 1, "Read the cache first.", navigation);
		const auto       labelledStop = *render_tour_stop("How", 2, "todo: Close the handle.", navigation);
		const auto       narrative    = [](const std::string& aStop) {
			const auto mapped = map_tour_stop(aStop);
			return feedback_body_segment{ .Managed = true, .UserBody = feedback_span{ mapped.Narrative->Begin, mapped.Narrative->End } };
		};
		const std::vector<post_text> posts{ { "S1", "s1", std::nullopt, plainStop, false, narrative(plainStop) },
											{ "S2", "s2", std::nullopt, labelledStop, false, narrative(labelledStop) },
											{ "S3", "s3", std::nullopt, labelledStop, false, { .Managed = true } } };
		const auto                   population = populate(annotate({ }, posts), { facts("S1", feedback_resolution::unresolved), facts("S2", feedback_resolution::unresolved), facts("S3", feedback_resolution::unresolved) });
		const auto                   view       = counts_for(population, pull_request, { feedback_label::todo }, feedback_resolution_mode::unresolved);
		expect(dedicated(view, feedback_label::todo) == 1U && view.CatchallParts == feedback_catchall_parts{ .Plain = 1U, .Unclassified = 1U }, "plain stop is catch-all, labelled stop is todo, unmapped stop is unclassified");
		expect(has_limit(view, feedback_count_limit::classification_partial), "the unmapped stop qualifies the count");
	}

	/// [[BPR-1089]]. The Overview's numbers: no chips, a typical subset and
	/// all twelve each split one file's conversations disjointly, the
	/// catch-all once; All keeps the breakdown; the ? count holds the unknown,
	/// the not resolvable and the unlabelled once each; every number's
	/// drill-down is exactly its conversations; the sort is numeric with the
	/// qualified first at a tie; a directory and the unattributed remainder
	/// are scopes of their own.
	void overview_numbers()
	{
		auto       conversations = std::vector{ labelled("i1", feedback_label::issue), labelled("i2", feedback_label::issue), labelled("i3", feedback_label::issue),
												labelled("q1", feedback_label::question), labelled("n1", feedback_label::nitpick), labelled("p1", feedback_label::praise),
												feedback_conversation{ .Id = "plain", .Status = feedback_root_status::plain, .Root = "r" },
												feedback_conversation{ .Id = "custom", .Status = feedback_root_status::unknown_label, .Root = "r" },
												feedback_conversation{ .Id = "bad", .Status = feedback_root_status::unclassified, .Root = "r" },
												labelled("top", feedback_label::issue), labelled("pr", feedback_label::note) };
		const auto set           = classified(conversations);
		auto       pr            = facts("pr", feedback_resolution::not_applicable, { });
		pr.Anchor                = feedback_anchor{ };
		const auto population    = populate(set, { facts("i1", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("i2", feedback_resolution::resolved, at_file("src/a.cpp")), facts("i3", feedback_resolution::unknown, at_file("src/a.cpp")), facts("q1", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("n1", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("p1", feedback_resolution::unresolved, at_file("src/b.cpp")), facts("plain", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("custom", feedback_resolution::resolved, at_file("src/a.cpp")), facts("bad", feedback_resolution::unresolved, at_file("src/a.cpp")), facts("top", feedback_resolution::unresolved, at_file("README.md")), pr });
		const auto a             = file("src/a.cpp");
		auto       allTwelve     = std::vector<feedback_label>{ };
		for (std::size_t label = 0U; label < feedback_label_count; ++label) {
			allTwelve.push_back(static_cast<feedback_label>(label));
		}
		const auto ids = [&](const feedback_counter_scope& aWhere, const std::vector<feedback_label>& aColumns, feedback_resolution_mode aMode, feedback_count_target aTarget) {
			return select_feedback_units(population, aWhere, aColumns, aMode, aTarget).Ids;
		};
		using strings = std::vector<std::string>;
		// No chips: the catch-all is everything Open counts.
		const auto none = counts_for(population, a, { }, feedback_resolution_mode::unresolved);
		expect(none.Dedicated.empty() && none.Catchall == 5U && none.Included == 5U, "no chips: the catch-all counts every open conversation");
		expect(ids(a, { }, feedback_resolution_mode::unresolved, { .Kind = feedback_count_kind::catchall }) == strings{ "bad", "i1", "n1", "plain", "q1" }, "no chips: the catch-all lists exactly the open conversations");
		// A typical subset: issue and question, the rest in Other once.
		const auto typical = counts_for(population, a, { feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved);
		expect(dedicated(typical, feedback_label::issue) == 1U && dedicated(typical, feedback_label::question) == 1U && typical.Catchall == 3U && conserved(typical),
			   "a typical subset: issue 1, question 1, Other 3 (nitpick, plain, unlabelled)");
		expect(ids(a, { feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved, { .Kind = feedback_count_kind::dedicated, .Label = feedback_label::issue }) == strings{ "i1" },
			   "the issue chip lists the open issue only");
		expect(ids(a, { feedback_label::issue, feedback_label::question }, feedback_resolution_mode::unresolved, { .Kind = feedback_count_kind::catchall }) == strings{ "bad", "n1", "plain" },
			   "Other lists the nitpick, the plain and the unlabelled conversations");
		// All twelve: Other still holds the plain, custom and unlabelled.
		const auto twelve = counts_for(population, a, allTwelve, feedback_resolution_mode::all);
		expect(dedicated(twelve, feedback_label::issue) == 3U && twelve.Catchall == 3U && twelve.Included == 8U && conserved(twelve)
				   && twelve.CatchallParts == feedback_catchall_parts{ .Plain = 1U, .Custom = 1U, .Unclassified = 1U },
			   "all twelve in All: issue 3, and Other holds the plain, custom and unlabelled once");
		// All shows the breakdown: the issue is one open, one resolved, one unknown.
		const auto issueStates = std::ranges::find(twelve.Dedicated, feedback_label::issue, &feedback_dedicated_count::Label)->States;
		expect(issueStates == feedback_state_counts{ 1U, 1U, 1U, 0U } && twelve.CatchallStates == feedback_state_counts{ 2U, 1U, 0U, 0U },
			   "All keeps each chip's state breakdown");
		expect(dedicated(counts_for(population, a, allTwelve, feedback_resolution_mode::unresolved), feedback_label::issue) == 1U, "Open counts only the positively unresolved issue");
		// The ? count: the unknown issue and the unlabelled root, once each.
		expect(typical.Uncertain == 2U && ids(a, { }, feedback_resolution_mode::all, { .Kind = feedback_count_kind::uncertain }) == strings{ "bad", "i3" },
			   "the ? count lists the unknown and the unlabelled, each once");
		expect(!typical.Exact && has_limit(typical, feedback_count_limit::resolution_unknown), "unknown resolution qualifies the Open count");
		// The unattributed remainder and a directory are scopes.
		const auto remainder = counts_for(population, unattributed, { feedback_label::note }, feedback_resolution_mode::unresolved);
		expect(remainder.Included == 0U && remainder.ScopeTotal == 1U && remainder.Uncertain == 1U && !has_limit(remainder, feedback_count_limit::attribution_unknown),
			   "the pull request comment is not on a file, not open, and counted by ?");
		expect(ids(unattributed, { }, feedback_resolution_mode::all, { .Kind = feedback_count_kind::included }) == strings{ "pr" }, "the remainder lists the pull request comment");
		const auto src = counts_for(population, directory("src"), { }, feedback_resolution_mode::all);
		expect(src.ScopeTotal == 9U && counts_for(population, directory(""), { }, feedback_resolution_mode::all).ScopeTotal == 1U, "a directory holds its own files only; the root directory holds README.md");
		// Refusals.
		expect(select_feedback_units(population, a, std::vector{ feedback_label::issue }, feedback_resolution_mode::all, { .Kind = feedback_count_kind::dedicated, .Label = feedback_label::question }).Refusal == feedback_selection_refusal::unselected_label,
			   "a label without a chip has no drill-down");
		expect(select_feedback_units(population, a, std::vector{ feedback_label::issue, feedback_label::issue }, feedback_resolution_mode::all, { }).Refusal == feedback_selection_refusal::duplicate_column, "a repeated chip is refused");
		expect(select_feedback_units(population, { .Kind = feedback_counter_scope_kind::directory, .Path = "src/" }, std::vector<feedback_label>{ }, feedback_resolution_mode::all, { }).Refusal == feedback_selection_refusal::invalid_scope, "a bad directory path is refused");
		auto other = tally_feedback_counters(population, file("src/b.cpp"));
		other.Epoch += 1U;
		expect(!merge_feedback_counter_tallies(std::vector{ tally_feedback_counters(population, a), other }, directory("src")).Valid, "tallies of different epochs do not merge");
		expect(!merge_feedback_counter_tallies(std::span<const feedback_counter_tally>{ }, directory("src")).Valid, "no parts name no pull request");
		// Sorting: numbers first, then the qualified at a tie.
		expect(feedback_sorts_before({ .Count = 3U }, { .Count = 2U, .Qualified = true }) && !feedback_sorts_before({ .Count = 2U, .Qualified = true }, { .Count = 3U }),
			   "a larger count sorts first");
		expect(feedback_sorts_before({ .Count = 2U, .Qualified = true }, { .Count = 2U }) && !feedback_sorts_before({ .Count = 2U }, { .Count = 2U, .Qualified = true })
				   && !feedback_sorts_before({ .Count = 2U }, { .Count = 2U }),
			   "at a tie the qualified count sorts first; equal keys keep the caller's order");
		expect(feedback_sort_key_of(typical, { .Kind = feedback_count_kind::dedicated, .Label = feedback_label::issue }) == feedback_sort_key{ .Count = 1U, .Qualified = true },
			   "a sort key is the projection's own number and exactness");
	}

	// --- Generated populations -----------------------------------------------

	struct splitmix64
	{
		std::uint64_t State;
		auto          next() -> std::uint64_t
		{
			std::uint64_t value = (State += 0x9E3779B97F4A7C15ULL);
			value               = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
			value               = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
			return value ^ (value >> 31U);
		}
		auto below(std::size_t aBound) -> std::size_t
		{
			return static_cast<std::size_t>(next() % aBound);
		}
	};

	/// Random populations with random repeats, origins, roots, states and
	/// anchors; each projection is checked against a count over the generated
	/// records themselves.
	auto generated(std::size_t aPopulations, std::size_t aMaximumUnits) -> std::size_t
	{
		splitmix64                       random{ 1078U };
		const std::array<const char*, 5> paths{ "src/a.cpp", "src/b.cpp", "src/x/c.cpp", "doc/d.md", "top.txt" };
		const std::array                 scopes{ pull_request, file("src/a.cpp"), file("top.txt"), folder("src"), folder("src/x"), folder(""), directory("src"), directory("src/x"), directory(""), unattributed };
		const std::array                 roots{ feedback_root_status::labelled, feedback_root_status::labelled, feedback_root_status::labelled, feedback_root_status::unknown_label, feedback_root_status::plain,
												feedback_root_status::unclassified, feedback_root_status::root_missing, feedback_root_status::root_deleted, feedback_root_status::root_ambiguous };
		std::size_t                      checked = 0U;
		for (std::size_t round = 0U; round < aPopulations; ++round) {
			std::vector<feedback_conversation>       conversations;
			std::vector<feedback_conversation_facts> observed;
			const auto                               count = random.below(aMaximumUnits + 1U);
			for (std::size_t at = 0U; at < count; ++at) {
				const auto id   = "c" + std::to_string(at);
				const auto root = roots[random.below(roots.size())];
				if (random.below(8U) != 0U) { // otherwise no post was observed: root missing
					conversations.push_back({ .Id = id, .Status = root, .Root = "r", .Label = root == feedback_root_status::labelled ? std::optional{ static_cast<feedback_label>(random.below(feedback_label_count)) } : std::nullopt });
				}
				feedback_conversation_facts item{ .Id = id, .Origin = static_cast<feedback_unit_origin>(random.below(8U) < 6U ? 0U : random.below(4U)), .Deleted = random.below(10U) == 0U, .Resolution = static_cast<feedback_resolution>(random.below(4U)) };
				const auto                  anchor = random.below(7U);
				item.Anchor                        = anchor < 5U ? at_file(paths[anchor], random.below(4U) == 0U) : feedback_anchor{ .Status = anchor == 5U ? feedback_anchor_status::unanchored : feedback_anchor_status::unknown };
				observed.push_back(item);
				if (random.below(6U) == 0U) {
					observed.push_back(item); // an overlapping page
				}
			}
			std::ranges::reverse(observed);
			const auto population = populate(classified(conversations), observed, feedback_collection::complete);
			// [[BPR-1089]]. A directory's tally is its files' tallies merged.
			for (const auto& [where, members] : { std::pair{ directory("src"), std::vector<std::string>{ "src/a.cpp", "src/b.cpp" } },
												  std::pair{ directory("src/x"), std::vector<std::string>{ "src/x/c.cpp" } },
												  std::pair{ directory(""), std::vector<std::string>{ "top.txt" } } }) {
				std::vector<feedback_counter_tally> fileTallies;
				for (const auto& member : members) {
					fileTallies.push_back(tally_feedback_counters(population, file(member)));
				}
				const auto merged = merge_feedback_counter_tallies(fileTallies, where);
				const auto direct = tally_feedback_counters(population, where);
				expect(merged.Valid && direct.Valid && merged.Labelled == direct.Labelled && merged.Plain == direct.Plain && merged.Custom == direct.Custom
						   && merged.Unclassified == direct.Unclassified && merged.Unattributed == direct.Unattributed
						   && merged.AttributionUnknown == direct.AttributionUnknown && merged.Where == where && merged.Epoch == direct.Epoch,
					   "a directory's merged file tallies equal its own tally");
			}
			for (const auto& where : scopes) {
				const auto tally = tally_feedback_counters(population, where);
				for (const auto mode : { feedback_resolution_mode::unresolved, feedback_resolution_mode::all }) {
					std::vector<feedback_label> columns;
					for (std::size_t label = 0U; label < feedback_label_count; ++label) {
						if (random.below(3U) == 0U) {
							columns.push_back(static_cast<feedback_label>(label));
						}
					}
					std::ranges::reverse(columns);
					const auto view = project_feedback_counters(tally, columns, mode);
					// The expected counts, from the generated records.
					std::map<feedback_label, std::size_t> expectedLabels;
					std::size_t                           expectedCatchall = 0U;
					std::size_t                           expectedTotal    = 0U;
					std::vector<std::string>              expectedUncertain;
					bool                                  lostAnchor = false;
					std::vector<std::string>              seen;
					for (const auto& item : observed) {
						if (std::ranges::find(seen, item.Id) != seen.end()) {
							continue;
						}
						seen.push_back(item.Id);
						if (item.Origin != feedback_unit_origin::published || item.Deleted) {
							continue;
						}
						lostAnchor           = lostAnchor || item.Anchor.Status == feedback_anchor_status::unknown;
						const auto& path     = item.Anchor.Path;
						const auto  parentOf = [](const std::string& aPath) { const auto slash = aPath.rfind('/'); return slash == std::string::npos ? std::string{ } : aPath.substr(0U, slash); };
						bool        inScope  = false;
						switch (where.Kind) {
						case feedback_counter_scope_kind::pull_request:
							inScope = true;
							break;
						case feedback_counter_scope_kind::file:
							inScope = path && *path == where.Path;
							break;
						case feedback_counter_scope_kind::folder:
							inScope = path && (where.Path.empty() || path->starts_with(where.Path + "/"));
							break;
						case feedback_counter_scope_kind::directory:
							inScope = path && parentOf(*path) == where.Path;
							break;
						case feedback_counter_scope_kind::unattributed:
							inScope = !path;
							break;
						}
						if (!inScope) {
							continue;
						}
						++expectedTotal;
						const auto known      = std::ranges::find(conversations, item.Id, &feedback_conversation::Id);
						const auto labelKnown = known != conversations.end()
							&& (known->Status == feedback_root_status::labelled || known->Status == feedback_root_status::plain || known->Status == feedback_root_status::unknown_label);
						if (item.Resolution == feedback_resolution::unknown || item.Resolution == feedback_resolution::not_applicable || !labelKnown) {
							expectedUncertain.push_back(item.Id);
						}
						if (mode == feedback_resolution_mode::unresolved && item.Resolution != feedback_resolution::unresolved) {
							continue;
						}
						const auto found = std::ranges::find(conversations, item.Id, &feedback_conversation::Id);
						if (found != conversations.end() && found->Label && std::ranges::find(columns, *found->Label) != columns.end()) {
							++expectedLabels[*found->Label];
						} else {
							++expectedCatchall;
						}
					}
					expect(view.ScopeTotal == expectedTotal && view.Catchall == expectedCatchall && conserved(view), "generated totals and conservation");
					for (const auto& column : view.Dedicated) {
						expect(column.Count == expectedLabels[column.Label], "generated dedicated count");
					}
					const auto fileBound = where.Kind != feedback_counter_scope_kind::pull_request && where.Kind != feedback_counter_scope_kind::unattributed;
					expect(has_limit(view, feedback_count_limit::attribution_unknown) == (lostAnchor && fileBound), "generated attribution coverage");
					expect(has_limit(view, feedback_count_limit::classification_partial) == (view.ScopeUnclassified != 0U), "generated classification coverage");
					// [[BPR-1089]]. Every number's drill-down lists exactly what it
					// counted, and the dedicated and catch-all lists partition the
					// included one.
					std::ranges::sort(expectedUncertain);
					const auto select = [&](const feedback_count_target& aTarget) {
						const auto chosen = select_feedback_units(population, where, columns, mode, aTarget);
						expect(chosen.Refusal == feedback_selection_refusal::none && chosen.Epoch == population.Epoch, "generated selection accepted at the population's epoch");
						expect(chosen.Ids.size() == feedback_count_of(view, aTarget), "generated selection is exactly the count");
						expect(std::ranges::is_sorted(chosen.Ids) && std::ranges::adjacent_find(chosen.Ids) == chosen.Ids.end(), "generated selection in identity order, each once");
						return chosen.Ids;
					};
					const auto included = select({ .Kind = feedback_count_kind::included });
					auto       parts    = select({ .Kind = feedback_count_kind::catchall });
					for (const auto& column : view.Dedicated) {
						const auto ids = select({ .Kind = feedback_count_kind::dedicated, .Label = column.Label });
						parts.insert(parts.end(), ids.begin(), ids.end());
					}
					std::ranges::sort(parts);
					expect(parts == included, "generated dedicated and catch-all drill-downs partition the included one");
					expect(select({ .Kind = feedback_count_kind::uncertain }) == expectedUncertain && view.Uncertain == expectedUncertain.size(), "generated uncertain drill-down and count");
					std::array<std::size_t, feedback_resolution_count> stateSum{ };
					const auto                                         addStates = [&](const feedback_state_counts& aStates) {
						for (std::size_t at = 0U; at < aStates.size(); ++at) {
							stateSum[at] += aStates[at];
						}
					};
					addStates(view.CatchallStates);
					for (const auto& column : view.Dedicated) {
						addStates(column.States);
						expect(column.Count == (mode == feedback_resolution_mode::all ? column.States[0] + column.States[1] + column.States[2] + column.States[3] : column.States[0]),
							   "generated dedicated count is its breakdown's included states");
					}
					expect(stateSum == view.States, "generated breakdowns sum to the scope's states");
					++checked;
				}
			}
		}
		return checked;
	}

	/// A 10,000-conversation population: one tally per scope answers all
	/// 4,096 subsets in both modes, conserving every time.
	auto large() -> std::size_t
	{
		splitmix64                               random{ 10078U };
		std::vector<feedback_conversation>       conversations;
		std::vector<feedback_conversation_facts> observed;
		for (std::size_t at = 0U; at < 10000U; ++at) {
			const auto id = "c" + std::to_string(at);
			conversations.push_back(labelled(id, static_cast<feedback_label>(random.below(feedback_label_count))));
			observed.push_back(facts(id, static_cast<feedback_resolution>(random.below(4U)), at_file("src/f" + std::to_string(random.below(50U)) + ".cpp")));
		}
		const auto  population = populate(classified(std::move(conversations)), observed);
		std::size_t checked    = 0U;
		for (const auto& where : { pull_request, folder("src"), file("src/f7.cpp") }) {
			const auto tally = tally_feedback_counters(population, where);
			for (const auto mode : { feedback_resolution_mode::unresolved, feedback_resolution_mode::all }) {
				const auto whole = project_feedback_counters(tally, { }, mode);
				for (std::uint32_t mask = 0U; mask < (1U << feedback_label_count); ++mask) {
					std::vector<feedback_label> columns;
					for (std::uint32_t label = 0U; label < feedback_label_count; ++label) {
						if ((mask & (1U << label)) != 0U) {
							columns.push_back(static_cast<feedback_label>(label));
						}
					}
					const auto view = project_feedback_counters(tally, columns, mode);
					expect(conserved(view) && view.Included == whole.Catchall, "large population conserves for every subset");
					++checked;
				}
			}
		}
		return checked;
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 2, "usage: test_feedback_partition <pack directory>");
	const auto projections = pack_cases(aArgv[1]);
	attribution(aArgv[1]);
	inclusion_rules();
	coverage();
	transitions();
	tour_roots();
	overview_numbers();
	const auto random = generated(2000U, 40U);
	const auto bulk   = large();
	std::cout << "pack subset projections: " << projections << '\n';
	std::cout << "generated projections: " << random << '\n';
	std::cout << "10,000-unit projections: " << bulk << '\n';
	std::cout << "FEEDBACK_PARTITION_OK\n";
	return EXIT_SUCCESS;
}
