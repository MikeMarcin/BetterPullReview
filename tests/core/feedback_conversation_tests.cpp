// [[BPR-1077]]. Root-post classification and the scoped derived annotation set
// (RFC-BPR-0047 R07, R08, R41): roots from reply linkage alone, reply-local
// annotations, website edits that invalidate only matching posts, and stale or
// cross-scope refreshes that are refused. Driven partly by the pack's
// counter-cases.json units.

#include "bpr/core/feedback_conversation.hpp"
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
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback conversation failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto read_file(const std::filesystem::path& aPath) -> std::string
	{
		auto stream = std::ifstream{ aPath, std::ios::binary };
		expect(static_cast<bool>(stream), "pack file readable");
		return { std::istreambuf_iterator<char>{ stream }, std::istreambuf_iterator<char>{ } };
	}

	auto scope(std::uint64_t aAccount, std::uint32_t aNumber = 7U) -> cached_review_key
	{
		cached_review_key key;
		key.Authority[0] = 1U;
		key.Profile      = host_profile_id{ 1U };
		key.Host         = git_host_id{ 1U };
		key.Account      = github_account_id{ aAccount };
		key.Locator      = { "octo", "repo" };
		key.Number       = pull_request_number{ aNumber };
		return key;
	}

	/// An owned post, so tests can edit bodies between observations.
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

	auto observe(const std::vector<post_text>& aPosts) -> std::vector<feedback_post_observation>
	{
		std::vector<feedback_post_observation> result;
		std::ranges::transform(aPosts, std::back_inserter(result), [](const post_text& aPost) {
			return feedback_post_observation{ .Conversation = aPost.Conversation,
											  .Id           = aPost.Id,
											  .ReplyTo      = aPost.ReplyTo ? std::optional<std::string_view>{ *aPost.ReplyTo } : std::nullopt,
											  .Body         = aPost.Body,
											  .Segment      = aPost.Segment,
											  .Deleted      = aPost.Deleted };
		});
		return result;
	}

	auto posts_of(const std::vector<post_text>& aPosts) -> std::vector<feedback_post>
	{
		std::vector<feedback_post> result;
		result.reserve(aPosts.size());
		for (const auto& post : aPosts) {
			result.push_back({ .Conversation = post.Conversation, .Id = post.Id, .ReplyTo = post.ReplyTo, .Deleted = post.Deleted, .Segment = post.Segment, .Annotation = post.Deleted ? std::nullopt : std::optional{ recognize_feedback_body(post.Body, post.Segment) } });
		}
		return result;
	}

	auto classify(const std::vector<post_text>& aPosts) -> feedback_conversation
	{
		const auto posts = posts_of(aPosts);
		return classify_feedback_conversation("T", posts);
	}

	auto conversation(const feedback_annotation_set& aSet, std::string_view aId) -> const feedback_conversation&
	{
		const auto found = std::ranges::find(aSet.Conversations, aId, &feedback_conversation::Id);
		expect(found != aSet.Conversations.end(), aId);
		return *found;
	}
	auto post(const feedback_annotation_set& aSet, std::string_view aId) -> const feedback_post&
	{
		const auto found = std::ranges::find(aSet.Posts, aId, &feedback_post::Id);
		expect(found != aSet.Posts.end(), aId);
		return *found;
	}

	auto refresh(const feedback_annotation_set& aCurrent, const cached_review_key& aScope, const std::vector<post_text>& aPosts) -> feedback_annotation_refresh
	{
		const auto observations = observe(aPosts);
		return refresh_feedback_annotations(aCurrent, aScope, observations);
	}
	auto adopted(const feedback_annotation_set& aCurrent, feedback_annotation_refresh aRefresh) -> feedback_annotation_set
	{
		expect(aRefresh.Refusal == feedback_refresh_refusal::none, "refresh accepted");
		auto next = adopt_feedback_refresh(aCurrent, std::move(aRefresh));
		expect(next.has_value(), "refresh adopted onto its base");
		return *next;
	}

	using names = std::vector<std::string>;

	/// The comparable content of a set: identities, classification, and each
	/// post's annotation facts.
	auto summary(const feedback_annotation_set& aSet) -> std::string
	{
		std::string result;
		for (const auto& item : aSet.Conversations) {
			result += item.Id + '=' + std::string{ stable_name(item.Status) } + ':' + item.Root.value_or("-") + ':' + std::string{ item.Label ? stable_name(*item.Label) : "-" } + ';';
		}
		for (const auto& item : aSet.Posts) {
			result += item.Conversation + '/' + item.Id + (item.Annotation ? std::string{ stable_name(item.Annotation->Status) } : "deleted") + (item.Annotation && item.Annotation->Label ? std::string{ stable_name(*item.Annotation->Label) } : "") + ';';
		}
		return result;
	}

	// The root decides; position, recency, identity order and replies do not.
	void root_identity()
	{
		// The root has the largest identifier and comes last; replies are
		// labelled differently and one replies to another reply.
		std::vector<post_text> thread{
			{ "T", "c9", std::nullopt, "issue (blocking): The root." },
			{ "T", "c1", "c9", "question: A reply." },
			{ "T", "c2", "c1", "praise: A reply to a reply." },
			{ "T", "c3", "c9", "Plain reply." },
		};
		const auto expected = feedback_conversation{ .Id = "T", .Status = feedback_root_status::labelled, .Root = "c9", .Label = feedback_label::issue };
		std::ranges::sort(thread, { }, &post_text::Id);
		std::size_t orders = 0U;
		do {
			expect(classify(thread) == expected, "every input order finds the same root");
			++orders;
		} while (std::ranges::next_permutation(thread, { }, &post_text::Id).found);
		expect(orders == 24U, "all orders tried");

		// Without the root, no reply is promoted, whatever it says.
		const std::vector<post_text> missing{ { "T", "c1", "c9", "issue: A labelled reply." }, { "T", "c2", "c9", "question: Another." } };
		expect(classify(missing) == feedback_conversation{ .Id = "T", .Status = feedback_root_status::root_missing }, "missing root stays unknown");

		auto       deleted = thread;
		const auto rootAt  = std::ranges::find(deleted, "c9", &post_text::Id);
		rootAt->Deleted    = true;
		const auto removed = classify(deleted);
		expect(removed == feedback_conversation{ .Id = "T", .Status = feedback_root_status::root_deleted, .Root = "c9" }, "deleted root stays unknown, reply not promoted");
		expect(!classification_known(removed.Status), "deleted root has no bucket");

		const auto ambiguous = [](std::vector<post_text> aPosts, std::string_view aWhy) {
			expect(classify(aPosts).Status == feedback_root_status::root_ambiguous && !classify(aPosts).Root && !classify(aPosts).Label, aWhy);
		};
		ambiguous({ { "T", "a", std::nullopt, "issue: One." }, { "T", "b", std::nullopt, "note: Two." } }, "two roots");
		ambiguous({ { "T", "a", std::nullopt, "issue: One." }, { "T", "a", "a", "note: Same id." } }, "duplicate identity");
		ambiguous({ { "T", "a", std::nullopt, "issue: One." }, { "T", "b", "x", "note: Orphan." } }, "reply to an unobserved post");
		ambiguous({ { "T", "a", std::nullopt, "issue: One." }, { "T", "b", "c", "x" }, { "T", "c", "b", "y" } }, "reply cycle");
		ambiguous({ { "T", "a", std::nullopt, "issue: One." }, { "T", "b", "b", "x" } }, "self reply");
	}

	// The root's own annotation sets its status, including a managed root.
	void root_status()
	{
		const auto status = [](std::string_view aBody, feedback_body_segment aSegment = { }) {
			return classify({ { "T", "r", std::nullopt, std::string{ aBody }, false, aSegment }, { "T", "x", "r", "issue: A reply." } });
		};
		expect(status("Plain.").Status == feedback_root_status::plain && classification_known(feedback_root_status::plain), "plain root is known catch-all");
		expect(status("observation: Custom.").Status == feedback_root_status::unknown_label && classification_known(feedback_root_status::unknown_label), "custom root is known catch-all");
		expect(status("issue (blocking : x").Status == feedback_root_status::unclassified, "malformed root is partial");
		expect(status("    issue: Indented code.").Status == feedback_root_status::plain, "indented root is plain");
		expect(status(std::string(1100U, '\n') + "issue: x").Status == feedback_root_status::unclassified, "limited root is partial");
		expect(!classification_known(feedback_root_status::unclassified), "partial is not known");
		expect(status("issue: x", { .Managed = true }).Status == feedback_root_status::unclassified, "envelope without a segment is partial");

		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       stop   = render_tour_stop("Why", 1, "todo: Close it.", navigation);
		const auto       mapped = map_tour_stop(*stop);
		const auto       tour   = status(*stop, { .Managed = true, .UserBody = feedback_span{ mapped.Narrative->Begin, mapped.Narrative->End } });
		expect(tour.Status == feedback_root_status::labelled && tour.Label == feedback_label::todo, "tour stop root labelled from its narrative");
		expect(status(*stop).Status == feedback_root_status::plain, "the envelope read whole is never stripped");
	}

	// Website edits, deletions and disappearance across refreshes.
	void refresh_cycle()
	{
		std::vector<post_text> posts{
			{ "T1", "r1", std::nullopt, "issue: Root one.\r\n\r\n```suggestion\r\nx();\r\n```\r\n" },
			{ "T1", "p1", "r1", "question: Why?" },
			{ "T1", "p2", "r1", "praise: Nice." },
			{ "T2", "r2", std::nullopt, "note: Root two." },
			{ "G1", "g1", std::nullopt, "Plain general comment." },
		};
		const auto key   = scope(1U);
		auto       first = refresh({ }, key, posts);
		expect(first.ScopeReplaced && first.Reused == 0U && first.Recognized == names{ "g1", "p1", "p2", "r1", "r2" } && first.ChangedConversations == names{ "G1", "T1", "T2" }, "first observation parses everything");
		auto set = adopted({ }, std::move(first));
		expect(set.Epoch == 1U && conversation(set, "T1").Label == feedback_label::issue && conversation(set, "T2").Label == feedback_label::note && conversation(set, "G1").Status == feedback_root_status::plain, "initial classification");

		auto same = refresh(set, key, posts);
		expect(!same.ScopeReplaced && same.Reused == 5U && same.Recognized.empty() && same.Removed.empty() && same.ChangedConversations.empty(), "an unchanged observation reuses every annotation");
		set = adopted(set, std::move(same));

		// A reply edited on the website: only that post is parsed again, and
		// the conversation keeps its root label.
		posts[1].Body = "thought: Maybe not.";
		auto reply    = refresh(set, key, posts);
		expect(reply.Recognized == names{ "p1" } && reply.Reused == 4U && reply.ChangedConversations.empty(), "reply edit touches only the reply");
		set = adopted(set, std::move(reply));
		expect(post(set, "p1").Annotation->Label == feedback_label::thought && conversation(set, "T1").Label == feedback_label::issue, "reply badge moves, root bucket stays");

		// The root edited on the website: its conversation moves, the replies
		// keep their own annotations, and the other conversations are quiet.
		posts[0].Body = "suggestion: Root one.\r\n\r\n```suggestion\r\nx();\r\n```\r\n";
		auto root     = refresh(set, key, posts);
		expect(root.Recognized == names{ "r1" } && root.Reused == 4U && root.ChangedConversations == names{ "T1" }, "root edit touches its own conversation only");
		set = adopted(set, std::move(root));
		expect(conversation(set, "T1").Label == feedback_label::suggestion && post(set, "p1").Annotation->Label == feedback_label::thought && post(set, "p2").Annotation->Label == feedback_label::praise, "root moved, replies kept");

		posts[0].Body = "Root one, now plain.";
		set           = adopted(set, refresh(set, key, posts));
		expect(conversation(set, "T1").Status == feedback_root_status::plain && !conversation(set, "T1").Label, "label removed on the website");

		// Deleted root: unknown, and its text no longer current.
		posts[0].Deleted = true;
		auto deletion    = refresh(set, key, posts);
		expect(deletion.Removed == names{ "r1" } && deletion.ChangedConversations == names{ "T1" }, "deletion removes the root's annotation");
		set = adopted(set, std::move(deletion));
		expect(conversation(set, "T1").Status == feedback_root_status::root_deleted && !post(set, "r1").Annotation && post(set, "p2").Annotation->Label == feedback_label::praise, "deleted root unknown, replies kept");

		// Root no longer observed at all: missing, never a promoted reply.
		posts.erase(posts.begin());
		auto vanished = refresh(set, key, posts);
		expect(vanished.Removed.empty() && vanished.ChangedConversations == names{ "T1" }, "already-removed root is not removed twice");
		set = adopted(set, std::move(vanished));
		expect(conversation(set, "T1") == feedback_conversation{ .Id = "T1", .Status = feedback_root_status::root_missing }, "missing root unknown");

		// A whole conversation gone.
		posts.erase(std::ranges::find(posts, "r2", &post_text::Id));
		auto gone = refresh(set, key, posts);
		expect(gone.Removed == names{ "r2" } && gone.ChangedConversations == names{ "T2" }, "vanished conversation reported");
		set = adopted(set, std::move(gone));
		expect(std::ranges::find(set.Conversations, "T2", &feedback_conversation::Id) == set.Conversations.end(), "vanished conversation dropped");

		// The owner now validates a segment for the same bytes: parsed again.
		const std::array navigation{ tour_markdown_link{ "Tour overview", "https://github.com/o/r/pull/7#pullrequestreview-9" } };
		const auto       stop = render_tour_stop("Why", 1, "chore: Rebase.", navigation);
		posts.push_back({ "T3", "s1", std::nullopt, *stop, false, { .Managed = true } });
		set = adopted(set, refresh(set, key, posts));
		expect(conversation(set, "T3").Status == feedback_root_status::unclassified, "unmapped stop is partial");
		const auto mapped    = map_tour_stop(*stop);
		posts.back().Segment = { .Managed = true, .UserBody = feedback_span{ mapped.Narrative->Begin, mapped.Narrative->End } };
		auto segment         = refresh(set, key, posts);
		expect(segment.Recognized == names{ "s1" } && segment.ChangedConversations == names{ "T3" }, "a new segment is a new key");
		set = adopted(set, std::move(segment));
		expect(conversation(set, "T3").Label == feedback_label::chore, "mapped stop labelled");

		// Observation order never matters.
		auto reversed = posts;
		std::ranges::reverse(reversed);
		expect(summary(refresh(set, key, reversed).Next) == summary(refresh(set, key, posts).Next), "order-independent result");
	}

	// Stale and cross-scope refreshes are refused without touching the set.
	void scope_and_staleness()
	{
		const std::vector<post_text> posts{ { "T", "r", std::nullopt, "issue: Root." }, { "T", "p", "r", "question: Reply." } };
		const auto                   accountA = scope(1U);
		const auto                   accountB = scope(2U);
		auto                         set      = adopted({ }, refresh({ }, accountA, posts));

		// Two refreshes computed from the same base: only the first lands.
		auto       earlier = refresh(set, accountA, posts);
		auto       later   = refresh(set, accountA, posts);
		const auto base    = set;
		set                = adopted(set, std::move(later));
		expect(!adopt_feedback_refresh(set, std::move(earlier)).has_value() && set.Epoch == base.Epoch + 1U, "a refresh computed before another was adopted is stale");

		// Account replacement: the same bodies are parsed again in the new
		// scope, and a refresh for the old account arriving late is refused.
		auto lateA   = refresh(set, accountA, posts);
		auto swapped = refresh(set, accountB, posts);
		expect(swapped.ScopeReplaced && swapped.Reused == 0U && swapped.Removed == names{ "p", "r" } && swapped.ChangedConversations == names{ "T" }, "nothing carried across accounts");
		set = adopted(set, std::move(swapped));
		expect(set.Scope == accountB, "new scope current");
		const auto beforeLate = summary(set);
		expect(!adopt_feedback_refresh(set, std::move(lateA)).has_value() && summary(set) == beforeLate && set.Scope == accountB, "late refresh for the old account refused, current set kept");

		// Another pull request in the same account is another scope too.
		const auto otherPull = refresh(set, scope(2U, 8U), posts);
		expect(otherPull.ScopeReplaced && otherPull.Reused == 0U, "another pull request is another scope");

		auto invalid       = scope(1U);
		invalid.Authority  = { };
		const auto refused = refresh(set, invalid, posts);
		expect(refused.Refusal == feedback_refresh_refusal::invalid_scope && !adopt_feedback_refresh(set, refused).has_value(), "invalid scope refused");
		expect(refresh(set, accountB, { { "T", "", std::nullopt, "x" } }).Refusal == feedback_refresh_refusal::invalid_identity, "empty post identity refused");
		expect(refresh(set, accountB, { { "", "a", std::nullopt, "x" } }).Refusal == feedback_refresh_refusal::invalid_identity, "empty conversation refused");
		expect(refresh(set, accountB, { { "T", "a", "", "x" } }).Refusal == feedback_refresh_refusal::invalid_identity, "empty reply target refused");
		expect(refresh(set, accountB, { { "T", "a", std::nullopt, "x" }, { "U", "a", std::nullopt, "y" } }).Refusal == feedback_refresh_refusal::duplicate_post, "one post in two conversations refused");
	}

	// Overlapping provider pages repeat a post: identical copies count once,
	// and a copy that differs in any fact refuses the refresh.
	void repeated_pages()
	{
		const auto                   key = scope(1U);
		const std::vector<post_text> once{ { "T", "r", std::nullopt, "issue: Root." }, { "T", "p", "r", "question: Reply." } };
		auto                         twice = once;
		twice.insert(twice.begin(), once.begin(), once.end());
		twice.push_back(once[1]);
		const auto single   = refresh({ }, key, once);
		const auto repeated = refresh({ }, key, twice);
		expect(repeated.Refusal == feedback_refresh_refusal::none && repeated.Collapsed == 3U && single.Collapsed == 0U, "identical repeated pages collapse");
		expect(summary(repeated.Next) == summary(single.Next) && repeated.Recognized == single.Recognized, "a collapsed copy changes nothing");

		const auto differs = [&](post_text aCopy, std::string_view aWhy) {
			auto posts = once;
			posts.push_back(std::move(aCopy));
			expect(refresh({ }, key, posts).Refusal == feedback_refresh_refusal::duplicate_post, aWhy);
		};
		differs({ "T", "p", "r", "question: Edited reply." }, "another body refused");
		differs({ "T", "p", std::nullopt, "question: Reply." }, "another reply target refused");
		differs({ "T", "p", "r", "question: Reply.", true }, "another deletion state refused");
		differs({ "T", "p", "r", "question: Reply.", false, { .Managed = true } }, "another segment refused");
	}

	// [[BPR-1349]]. A kept annotation is reused when the observed bytes are
	// exactly its kept bytes, proved by comparing them, never by hashing the
	// observed body; only a recognized body is hashed.
	void reuse_by_kept_bytes()
	{
		const auto             key = scope(1U);
		std::vector<post_text> posts{
			{ "T1", "r1", std::nullopt, "issue: Root one." },
			{ "T1", "p1", "r1", "question: Why?" },
			{ "T2", "r2", std::nullopt, "note: Root two." },
		};
		auto first = refresh({ }, key, posts);
		expect(first.Hashed == 3U && first.Recognized.size() == 3U, "the first observation hashes each body it parses, once");
		auto set = adopted({ }, std::move(first));
		expect(post(set, "r1").Body && *post(set, "r1").Body == "issue: Root one.", "a post keeps the exact bytes it was read from");

		auto same = refresh(set, key, posts);
		expect(same.Reused == 3U && same.Hashed == 0U, "an unchanged observation hashes no body");
		expect(post(same.Next, "r1").Body == post(set, "r1").Body && post(same.Next, "p1").Body == post(set, "p1").Body,
			   "a reused post shares its kept bytes, not a copy");
		set = adopted(set, std::move(same));

		// One byte changed, the length kept: parsed again, and only it hashed.
		posts[0].Body = "issue: Root One.";
		auto edited   = refresh(set, key, posts);
		expect(edited.Recognized == names{ "r1" } && edited.Reused == 2U && edited.Hashed == 1U, "one changed byte parses that body again and hashes it alone");
		set = adopted(set, std::move(edited));

		// The kept bytes decide, not the kept digest. A kept digest that no
		// longer matches its bytes does not stop a reuse of the same bytes...
		auto forged = set;
		for (auto& kept : forged.Posts) {
			if (kept.Id == "r2") {
				kept.Annotation->BodyDigest = { };
			}
		}
		const auto byBytes = refresh(forged, key, posts);
		expect(std::ranges::find(byBytes.Recognized, "r2") == byBytes.Recognized.end() && byBytes.Hashed == 0U, "the same bytes are reused whatever the kept digest says");
		// ...and a kept digest of the observed body does not reuse other bytes.
		auto other = set;
		for (auto& kept : other.Posts) {
			if (kept.Id == "p1") {
				kept.Annotation->BodyDigest = recognize_feedback_body("question: Why?", { }).BodyDigest;
				kept.Body                   = std::make_shared<const std::string>("question: How?");
			}
		}
		const auto byDigest = refresh(other, key, posts);
		expect(byDigest.Recognized == names{ "p1" } && byDigest.Hashed == 1U, "other kept bytes are parsed again, though the kept digest matches");

		// A deleted post keeps no bytes; brought back, it is parsed again.
		posts[2].Deleted = true;
		set              = adopted(set, refresh(set, key, posts));
		expect(!post(set, "r2").Body && !post(set, "r2").Annotation, "a deleted post keeps no bytes");
		posts[2].Deleted = false;
		const auto back  = refresh(set, key, posts);
		expect(back.Recognized == names{ "r2" } && back.Hashed == 1U, "a post that comes back is parsed again");
	}

	// The pack's normalized counter units, each built as a real conversation.
	auto counter_units(const std::filesystem::path& aPack) -> std::size_t
	{
		const auto parsed = parse_cycle_json(read_file(aPack / "fixtures" / "counter-cases.json"), 1U << 20U);
		expect(parsed.has_value(), "counter-cases.json parses");
		const auto&                                                          root  = std::get<cycle_json_value::object>(parsed->Value);
		const auto&                                                          units = std::get<cycle_json_value::array>(find_member(root, "units")->Value);
		std::vector<post_text>                                               posts;
		std::vector<std::pair<std::string, const cycle_json_value::object*>> seen;
		for (const auto& entry : units) {
			const auto& unit = std::get<cycle_json_value::object>(entry.Value);
			const auto  id   = std::get<std::string>(find_member(unit, "id")->Value);
			// A repeated page of the same unit is observed again, as a
			// provider would deliver it, and collapses in the refresh.
			if (std::ranges::find(seen, id, &std::pair<std::string, const cycle_json_value::object*>::first) == seen.end()) {
				seen.emplace_back(id, &unit);
			}
			const auto* label  = find_member(unit, "root_label");
			const auto* raw    = find_member(unit, "raw_label");
			const auto* status = find_member(unit, "parse_status");
			const bool  known  = std::get<bool>(find_member(unit, "classification_known")->Value);
			std::string body   = "Plain root.";
			if (const auto* name = std::get_if<std::string>(&label->Value)) {
				body = *name + ": Root.";
			} else if (raw != nullptr) {
				body = std::get<std::string>(raw->Value) + ": Root.";
			} else if (status != nullptr && std::get<std::string>(status->Value) == "limited") {
				body = std::string(1100U, '\n') + "issue: Root.";
			}
			if (known || status != nullptr) {
				posts.emplace_back(id, id + "/root", std::nullopt, body, std::get<bool>(find_member(unit, "deleted")->Value));
			}
			if (const auto* replies = find_member(unit, "replies_labels")) {
				std::size_t index = 0U;
				for (const auto& reply : std::get<cycle_json_value::array>(replies->Value)) {
					posts.emplace_back(id, id + "/reply" + std::to_string(index++), id + "/root", std::get<std::string>(reply.Value) + ": Reply.");
				}
			}
			if (!known && status == nullptr) {
				posts.emplace_back(id, id + "/orphan", id + "/root", "issue: A reply whose root was not observed.");
			}
		}
		auto refreshed = refresh({ }, scope(1U), posts);
		expect(refreshed.Collapsed == 3U, "the repeated issue-open page collapses: root and two replies");
		const auto set = adopted({ }, std::move(refreshed));
		for (const auto& [id, unit] : seen) {
			const auto& result  = conversation(set, id);
			const auto* label   = std::get_if<std::string>(&find_member(*unit, "root_label")->Value);
			const bool  known   = std::get<bool>(find_member(*unit, "classification_known")->Value);
			const bool  deleted = std::get<bool>(find_member(*unit, "deleted")->Value);
			if (deleted) {
				expect(result.Status == feedback_root_status::root_deleted, id + ": deleted root has no bucket");
				continue;
			}
			expect(classification_known(result.Status) == known, id + ": classification known");
			expect(result.Label == (label != nullptr ? find_feedback_label(*label) : std::nullopt), id + ": root label");
			if (find_member(*unit, "raw_label") != nullptr) {
				expect(result.Status == feedback_root_status::unknown_label, id + ": custom label is the catch-all");
			}
		}
		return seen.size();
	}
}

auto main(int aArgc, char** aArgv) -> int
{
	expect(aArgc == 2, "usage: test_feedback_conversation <pack directory>");
	root_identity();
	root_status();
	refresh_cycle();
	scope_and_staleness();
	repeated_pages();
	reuse_by_kept_bytes();
	const auto units = counter_units(aArgv[1]);
	std::cout << "pack counter units: " << units << '\n';
	std::cout << "FEEDBACK_CONVERSATION_OK\n";
	return EXIT_SUCCESS;
}
