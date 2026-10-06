// [[BPR-1086]]. Tour compatibility (RFC-BPR-0048, "Tour compatibility"; R19;
// the pack's native-integration case `tour-coexistence`). A tour with plain
// and deliberately labelled stops, its index links and an ordinary native
// thread is read through the one segment owner, `published_feedback_segment`,
// and counted through the shared population. Plain stops stay plain, a
// labelled stop counts in its bucket, the index's links count nothing, the
// order, links and envelopes survive byte for byte, and a label edited into a
// published stop changes only the narrative's header.

#include "bpr/core/feedback_header_edit.hpp"
#include "bpr/core/feedback_partition.hpp"
#include "bpr/core/review_tour_markdown.hpp"

#include <array>
#include <cstdint>
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
			std::cerr << "FAILED: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto review_key() -> cached_review_key
	{
		cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = host_profile_id{ 1U };
		key.Host         = git_host_id{ 1U };
		key.Account      = github_account_id{ 1U };
		key.Locator      = { "octo", "repo" };
		key.Number       = pull_request_number{ 7U };
		return key;
	}

	struct post_text
	{
		std::string                Conversation;
		std::string                Id;
		std::optional<std::string> ReplyTo;
		std::string                Body;
	};

	auto post(std::string aConversation, std::string aId, std::optional<std::string> aReplyTo, std::string aBody) -> post_text
	{
		auto value         = post_text{ };
		value.Conversation = std::move(aConversation);
		value.Id           = std::move(aId);
		value.ReplyTo      = std::move(aReplyTo);
		value.Body         = std::move(aBody);
		return value;
	}

	/// Every post read through the one segment owner, as the application's
	/// adapters read accepted bodies.
	auto annotate(const std::vector<post_text>& aPosts) -> feedback_annotation_set
	{
		auto observations = std::vector<feedback_post_observation>{ };
		observations.reserve(aPosts.size());
		for (const auto& item : aPosts) {
			auto observation         = feedback_post_observation{ };
			observation.Conversation = item.Conversation;
			observation.Id           = item.Id;
			if (item.ReplyTo) {
				observation.ReplyTo = *item.ReplyTo;
			}
			observation.Body    = item.Body;
			observation.Segment = published_feedback_segment(item.Body);
			observations.push_back(observation);
		}
		auto refresh = refresh_feedback_annotations({ }, review_key(), observations);
		expect(refresh.Refusal == feedback_refresh_refusal::none, "the tour's posts are annotated");
		auto adopted = adopt_feedback_refresh({ }, std::move(refresh));
		expect(adopted.has_value(), "and the annotations adopted");
		return std::move(*adopted);
	}

	auto conversation_of(const feedback_annotation_set& aSet, const std::string_view aId) -> const feedback_conversation*
	{
		for (const auto& item : aSet.Conversations) {
			if (item.Id == aId) {
				return &item;
			}
		}
		return nullptr;
	}

	auto tally(const feedback_annotation_set& aSet) -> feedback_counter_tally
	{
		auto facts = std::vector<feedback_conversation_facts>{ };
		for (const auto& id : { "stop-1", "stop-2", "stop-3", "thread-1" }) {
			auto fact          = feedback_conversation_facts{ };
			fact.Id            = id;
			fact.Resolution    = feedback_resolution::unresolved;
			fact.Anchor.Status = feedback_anchor_status::file;
			fact.Anchor.Path   = "src/light.cpp";
			facts.push_back(std::move(fact));
		}
		// The index is a review summary: no resolution and no file.
		auto index       = feedback_conversation_facts{ };
		index.Id         = "index";
		index.Resolution = feedback_resolution::not_applicable;
		facts.push_back(std::move(index));
		const auto population = build_feedback_population(aSet, facts, feedback_collection::complete);
		expect(population.Refusal == feedback_population_refusal::none, "the shared population admits the tour");
		return tally_feedback_counters(population.Population, { });
	}

	auto total(const feedback_state_counts& aCounts) -> std::size_t
	{
		auto sum = std::size_t{ 0U };
		for (const auto count : aCounts) {
			sum += count;
		}
		return sum;
	}

	auto labelled(const feedback_counter_tally& aTally, const feedback_label aLabel) -> std::size_t
	{
		return total(aTally.Labelled[static_cast<std::size_t>(aLabel)]);
	}

	auto all_labelled(const feedback_counter_tally& aTally) -> std::size_t
	{
		auto sum = std::size_t{ 0U };
		for (const auto& counts : aTally.Labelled) {
			sum += total(counts);
		}
		return sum;
	}

	auto footer_of(const std::string& aStop) -> std::string
	{
		const auto mapped = map_tour_stop(aStop);
		expect(mapped.Footer.has_value(), "the tour owner maps the stop's navigation");
		return aStop.substr(mapped.Footer->Begin, mapped.Footer->End - mapped.Footer->Begin);
	}

	auto before_narrative(const std::string& aStop) -> std::string
	{
		const auto mapped = map_tour_stop(aStop);
		expect(mapped.Narrative.has_value(), "the tour owner maps the stop's narrative");
		return aStop.substr(0U, mapped.Narrative->Begin);
	}

	/// The segment owner itself: a stop is the author's only inside its
	/// mapped narrative; everything else is ordinary text.
	void the_segment_owner()
	{
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       plain        = *render_tour_stop("Why", 1, "Read the cache first.", navigation);
		const auto       asked        = *render_tour_stop("How", 2, "question: Does the cache outlive the view?", navigation,
														  "https://github.com/o/r/blob/1111111111111111111111111111111111111111/src/a.cpp?plain=1#L2");
		const auto       plainSegment = published_feedback_segment(plain);
		const auto       askedSegment = published_feedback_segment(asked);
		expect(plainSegment.Managed && plainSegment.UserBody.has_value(), "a published stop is managed, its narrative the author's");
		expect(recognize_feedback_body(plain, plainSegment).Status == feedback_prefix_status::plain, "a narrative without a header is plain");
		const auto annotation = recognize_feedback_body(asked, askedSegment);
		expect(annotation.Status == feedback_prefix_status::recognized && annotation.Label == feedback_label::question
				   && annotation.Header.has_value() && annotation.Header->Begin >= askedSegment.UserBody->Begin,
			   "a labelled narrative is recognized after the pinned source and the heading");
		expect(recognize_feedback_prefix(asked).Status != feedback_prefix_status::recognized,
			   "negative control: read from its first byte, the stop's label is not found");

		const auto broken        = std::string{ "### Author's tour · 3 — Teardown\n\nissue: The navigation paragraph is missing." };
		const auto brokenSegment = published_feedback_segment(broken);
		const auto unread        = recognize_feedback_body(broken, brokenSegment);
		expect(brokenSegment.Managed && !brokenSegment.UserBody.has_value() && unread.Status == feedback_prefix_status::plain && !unread.Label.has_value(),
			   "a heading whose envelope does not map yields no author text, and nothing is stripped by pattern");

		const auto ordinary = std::string{ "issue: An ordinary comment." };
		expect(published_feedback_segment(ordinary) == feedback_body_segment{ }, "an ordinary body is the author's from its first byte");
	}

	/// The pack's `tour-coexistence` case.
	void a_tour_beside_ordinary_threads()
	{
		const auto link = [](const char* const aLabel, const char* const aDestination) {
			auto value        = tour_markdown_link{ };
			value.Label       = aLabel;
			value.Destination = aDestination;
			return value;
		};
		const std::array entries{ link("1. Where the frame starts", "https://github.com/o/r/pull/7#discussion_r1"),
								  link("2. Why the width is clamped", "https://github.com/o/r/pull/7#discussion_r2"),
								  link("3. Teardown", "https://github.com/o/r/pull/7#discussion_r3") };
		const auto       index = render_tour_index(entries);
		expect(index.has_value(), "the tour owner renders the index");
		const auto navigation = [&](const std::size_t aStop) {
			auto value = std::vector<tour_markdown_link>{ };
			if (aStop > 0U) {
				value.push_back(link("Previous", entries[aStop - 1U].Destination.c_str()));
			}
			value.push_back(link("Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9"));
			if (aStop + 1U < entries.size()) {
				value.push_back(link("Next", entries[aStop + 1U].Destination.c_str()));
			}
			return value;
		};
		const auto first  = render_tour_stop("Where the frame starts", 1, "The renderer owns the frame from here.", navigation(0U),
											 "https://github.com/o/r/blob/1111111111111111111111111111111111111111/src/light.cpp?plain=1#L4");
		const auto second = render_tour_stop("Why the width is clamped", 2, "question: Should the clamp live in the caller?\n\nThe renderer trusts it today.", navigation(1U));
		const auto third  = render_tour_stop("Teardown", 3, "Handles close here.\n\n```suggestion\n\tclose(handle);\n```\n", navigation(2U));
		expect(first && second && third, "the tour owner renders three stops");

		const auto posts = std::vector<post_text>{
			post("stop-1", "s1", std::nullopt, *first),
			post("stop-2", "s2", std::nullopt, *second),
			post("stop-3", "s3", std::nullopt, *third),
			post("thread-1", "t1", std::nullopt, "issue: The clamp hides a zero width."),
			post("thread-1", "t2", std::string{ "t1" }, "question: Should it assert instead?"),
			post("index", "i1", std::nullopt, *index),
		};
		const auto set = annotate(posts);
		for (const auto* const plain : { "stop-1", "stop-3", "index" }) {
			const auto* const found = conversation_of(set, plain);
			expect(found != nullptr && found->Status == feedback_root_status::plain && !found->Label.has_value(),
				   "an explanatory stop and the index are plain: no label is forced on them");
		}
		const auto* const asked = conversation_of(set, "stop-2");
		expect(asked != nullptr && asked->Status == feedback_root_status::labelled && asked->Label == feedback_label::question,
			   "the stop its author labelled counts as its label");
		const auto* const thread = conversation_of(set, "thread-1");
		expect(thread != nullptr && thread->Label == feedback_label::issue, "the ordinary thread counts as its root, not its reply");

		const auto counts = tally(set);
		expect(counts.Valid && all_labelled(counts) + total(counts.Plain) + total(counts.Custom) + total(counts.Unclassified) == 5U,
			   "five conversations: three stops, one thread and the index; its links and the stops' navigation add none");
		expect(labelled(counts, feedback_label::question) == 1U && labelled(counts, feedback_label::issue) == 1U && total(counts.Plain) == 3U
				   && total(counts.Unclassified) == 0U,
			   "one question (the labelled stop), one issue (the thread), three plain");

		// Order and links: the index lists the stops in order, each stop keeps
		// its ordinal, and the navigation reaches its neighbours.
		const auto mapped = map_tour_markdown(*index);
		expect(mapped.Links.size() == entries.size(), "the index keeps one link per stop");
		for (std::size_t at = 0U; at < entries.size(); ++at) {
			expect(mapped.Links[at] == entries[at], "the index keeps the stops' order and destinations");
		}
		const auto stops = std::array{ &*first, &*second, &*third };
		for (std::size_t at = 0U; at < stops.size(); ++at) {
			const auto stop    = map_tour_stop(*stops[at]);
			const auto heading = stops[at]->substr(stop.Heading->Begin, stop.Heading->End - stop.Heading->Begin);
			expect(heading.find("· " + std::to_string(at + 1U) + " — ") != std::string::npos, "each stop keeps its ordinal");
			const auto footer = footer_of(*stops[at]);
			expect((at == 0U || footer.find(entries[at - 1U].Destination) != std::string::npos)
					   && (at + 1U == stops.size() || footer.find(entries[at + 1U].Destination) != std::string::npos),
				   "each stop's navigation links its neighbours");
		}

		// A label edited into the first published stop changes only its
		// narrative's header: pinned source, heading, navigation, the other
		// stops and the index are untouched, and the stop moves bucket.
		auto request           = feedback_header_edit_request{ };
		request.Body           = *first;
		request.Segment        = published_feedback_segment(*first);
		request.ExpectedDigest = recognize_feedback_body(*first, request.Segment).BodyDigest;
		request.Edit.Change    = feedback_label_change::set;
		request.Edit.Label     = feedback_label::note;
		const auto edit        = decide_feedback_header_edit(request);
		expect(edit.Refusal == feedback_edit_refusal::none && edit.Changed, "a label can be written into a published stop's narrative");
		expect(before_narrative(edit.Body) == before_narrative(*first) && footer_of(edit.Body) == footer_of(*first)
				   && edit.Replaced.Begin >= map_tour_stop(*first).Narrative->Begin,
			   "the generated envelope stays the tour engine's, byte for byte");
		auto edited  = posts;
		edited[0]    = post("stop-1", "s1", std::nullopt, edit.Body);
		const auto r = tally(annotate(edited));
		expect(labelled(r, feedback_label::note) == 1U && total(r.Plain) == 2U && all_labelled(r) + total(r.Plain) == 5U,
			   "the relabelled stop counts once, as its new label");
	}
}

auto main() -> int
{
	the_segment_owner();
	a_tour_beside_ordinary_threads();
	std::cout << "FEEDBACK_TOUR_COEXISTENCE_OK\n";
	return EXIT_SUCCESS;
}
