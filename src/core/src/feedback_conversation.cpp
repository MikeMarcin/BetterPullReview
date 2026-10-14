#include "bpr/core/feedback_conversation.hpp"

#include "bpr/core/review_tour_markdown.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <tuple>
#include <utility>

namespace bpr::core {
	namespace {
		using id_index = std::pair<std::string_view, std::size_t>;

		/// Identities paired with their positions, sorted for lookup.
		template <typename TRange, typename TProjection>
		auto index_by(const TRange& aItems, TProjection aId) -> std::vector<id_index>
		{
			std::vector<id_index> index;
			index.reserve(std::ranges::size(aItems));
			for (std::size_t at = 0U; at < std::ranges::size(aItems); ++at) {
				index.emplace_back(aId(aItems[at]), at);
			}
			std::ranges::sort(index);
			return index;
		}
		auto find_in(const std::vector<id_index>& aIndex, std::string_view aId) -> std::optional<std::size_t>
		{
			const auto found = std::ranges::lower_bound(aIndex, aId, { }, &id_index::first);
			return found != aIndex.end() && found->first == aId ? std::optional{ found->second } : std::nullopt;
		}
		auto has_duplicate(const std::vector<id_index>& aIndex) -> bool
		{
			return std::ranges::adjacent_find(aIndex, { }, &id_index::first) != aIndex.end();
		}

		/// Whether every reply's chain reaches the one root, without cycles.
		/// Each post is walked once: a settled post ends any later walk.
		auto every_reply_reaches_root(std::span<const feedback_post> aPosts, const std::vector<id_index>& aIndex) -> bool
		{
			enum class mark : std::uint8_t {
				open,
				walking,
				reaches,
			};
			std::vector<mark>        marks(aPosts.size(), mark::open);
			std::vector<std::size_t> path;
			for (std::size_t start = 0U; start < aPosts.size(); ++start) {
				auto at = start;
				while (marks[at] == mark::open && aPosts[at].ReplyTo) {
					marks[at] = mark::walking;
					path.push_back(at);
					const auto parent = find_in(aIndex, *aPosts[at].ReplyTo);
					if (!parent) {
						return false;
					}
					at = *parent;
				}
				if (marks[at] == mark::walking) {
					return false;
				}
				for (const auto walked : path) {
					marks[walked] = mark::reaches;
				}
				marks[at] = mark::reaches;
				path.clear();
			}
			return true;
		}

		auto status_of(const std::optional<feedback_annotation>& aAnnotation) -> feedback_root_status
		{
			if (!aAnnotation) {
				return feedback_root_status::unclassified;
			}
			switch (aAnnotation->Status) {
			case feedback_prefix_status::recognized:
				return feedback_root_status::labelled;
			case feedback_prefix_status::unknown_label:
				return feedback_root_status::unknown_label;
			case feedback_prefix_status::plain:
				return std::ranges::find(aAnnotation->Diagnostics, feedback_diagnostic::classification_unavailable) != aAnnotation->Diagnostics.end() ? feedback_root_status::unclassified : feedback_root_status::plain;
			case feedback_prefix_status::malformed:
			case feedback_prefix_status::limited:
				break;
			}
			return feedback_root_status::unclassified;
		}

		/// [[BPR-1349]]. A kept annotation holds while the observed bytes are
		/// exactly the kept ones: a comparison, where a digest would hash every
		/// unchanged body on every observation.
		auto reusable(const feedback_post& aPrevious, const feedback_post_observation& aObserved) -> bool
		{
			return !aPrevious.Deleted && aPrevious.Annotation && aPrevious.Body && aPrevious.Segment == aObserved.Segment
				&& aPrevious.Annotation->ParserRevision == feedback_parser_revision && *aPrevious.Body == aObserved.Body;
		}
		auto valid_identities(const feedback_post_observation& aPost) noexcept -> bool
		{
			return !aPost.Conversation.empty() && !aPost.Id.empty() && (!aPost.ReplyTo || !aPost.ReplyTo->empty());
		}
		auto same_observation(const feedback_post_observation& aLeft, const feedback_post_observation& aRight) -> bool
		{
			return std::tie(aLeft.Conversation, aLeft.Id, aLeft.ReplyTo, aLeft.Body, aLeft.Segment, aLeft.Deleted)
				== std::tie(aRight.Conversation, aRight.Id, aRight.ReplyTo, aRight.Body, aRight.Segment, aRight.Deleted);
		}
		/// Mark every later copy of a post that overlapping pages repeated
		/// with identical facts. A copy that differs in any fact is a
		/// contradiction, and nothing is marked.
		auto repeated_pages(std::span<const feedback_post_observation> aPosts, const std::vector<id_index>& aIndex) -> std::optional<std::vector<bool>>
		{
			std::vector<bool> repeated(aPosts.size(), false);
			for (std::size_t at = 1U; at < aIndex.size(); ++at) {
				if (aIndex[at].first != aIndex[at - 1U].first) {
					continue;
				}
				if (!same_observation(aPosts[aIndex[at - 1U].second], aPosts[aIndex[at].second])) {
					return std::nullopt;
				}
				repeated[aIndex[at].second] = true;
			}
			return repeated;
		}
		auto by_conversation(const feedback_post& aLeft, const feedback_post& aRight) -> bool
		{
			return std::tie(aLeft.Conversation, aLeft.Id) < std::tie(aRight.Conversation, aRight.Id);
		}

		/// Classify each run of posts sharing a conversation, and name every
		/// conversation whose classification differs from the old set's.
		void classify_all(feedback_annotation_refresh& aRefresh, const feedback_annotation_set& aOld)
		{
			const auto& posts = aRefresh.Next.Posts;
			for (auto begin = posts.begin(); begin != posts.end();) {
				const auto end = std::ranges::find_if(begin, posts.end(), [&](const feedback_post& aPost) { return aPost.Conversation != begin->Conversation; });
				aRefresh.Next.Conversations.push_back(classify_feedback_conversation(begin->Conversation, std::span{ begin, end }));
				begin = end;
			}
			const auto& next     = aRefresh.Next.Conversations;
			const auto  oldIndex = index_by(aOld.Conversations, [](const feedback_conversation& aItem) -> std::string_view { return aItem.Id; });
			const auto  newIndex = index_by(next, [](const feedback_conversation& aItem) -> std::string_view { return aItem.Id; });
			for (const auto& conversation : next) {
				const auto previous = aRefresh.ScopeReplaced ? std::nullopt : find_in(oldIndex, conversation.Id);
				if (!previous || aOld.Conversations[*previous] != conversation) {
					aRefresh.ChangedConversations.push_back(conversation.Id);
				}
			}
			for (const auto& conversation : aOld.Conversations) {
				if (aRefresh.ScopeReplaced || !find_in(newIndex, conversation.Id)) {
					aRefresh.ChangedConversations.push_back(conversation.Id);
				}
			}
			std::ranges::sort(aRefresh.ChangedConversations);
			const auto [first, last] = std::ranges::unique(aRefresh.ChangedConversations);
			aRefresh.ChangedConversations.erase(first, last);
		}
		/// [[BPR-1349]]. Where each kept post's observation is, when the
		/// observation names exactly the kept posts, each in its kept
		/// conversation with its kept reply link, and no page repeated one.
		/// Both indexes are sorted by identity, so they pair up in one walk.
		auto carried_order(const feedback_annotation_set& aCurrent, std::span<const feedback_post_observation> aPosts, const std::vector<id_index>& aObserved,
						   const std::vector<id_index>& aPrevious, const std::vector<bool>& aRepeated) -> std::optional<std::vector<std::size_t>>
		{
			if (aPosts.size() != aCurrent.Posts.size() || std::ranges::find(aRepeated, true) != aRepeated.end()
				|| !std::ranges::is_sorted(aCurrent.Posts, by_conversation)) {
				return std::nullopt;
			}
			std::vector<std::size_t> order(aPosts.size());
			for (std::size_t at = 0U; at < aObserved.size(); ++at) {
				const auto& [id, observedAt] = aObserved[at];
				const auto& [keptId, keptAt] = aPrevious[at];
				const auto& kept             = aCurrent.Posts[keptAt];
				const auto& observation      = aPosts[observedAt];
				if (id != keptId || kept.Conversation != observation.Conversation || kept.ReplyTo.has_value() != observation.ReplyTo.has_value()
					|| (kept.ReplyTo && *kept.ReplyTo != *observation.ReplyTo)) {
					return std::nullopt;
				}
				order[keptAt] = observedAt;
			}
			return order;
		}

		/// [[BPR-1349]]. Classify the carried posts' conversations: one whose
		/// posts are all unchanged keeps its kept classification; any other
		/// is classified again and named when it differs. The kept
		/// conversations are in the same order as the carried posts' runs,
		/// which is checked; otherwise every conversation is classified.
		void carry_classification(feedback_annotation_refresh& aRefresh, const feedback_annotation_set& aOld, const std::vector<bool>& aChanged)
		{
			const auto&                                      posts = aRefresh.Next.Posts;
			std::vector<std::pair<std::size_t, std::size_t>> runs;
			for (std::size_t begin = 0U; begin < posts.size();) {
				auto end = begin + 1U;
				while (end < posts.size() && posts[end].Conversation == posts[begin].Conversation) {
					++end;
				}
				runs.emplace_back(begin, end);
				begin = end;
			}
			const auto aligned = runs.size() == aOld.Conversations.size()
				&& std::ranges::equal(runs, aOld.Conversations, [&](const auto& aRun, const feedback_conversation& aKept) { return posts[aRun.first].Conversation == aKept.Id; });
			if (!aligned) {
				classify_all(aRefresh, aOld);
				return;
			}
			auto& next = aRefresh.Next.Conversations;
			next.reserve(runs.size());
			for (std::size_t index = 0U; index < runs.size(); ++index) {
				const auto [begin, end] = runs[index];
				const auto first        = aChanged.begin() + static_cast<std::ptrdiff_t>(begin);
				if (std::none_of(first, first + static_cast<std::ptrdiff_t>(end - begin), [](const bool aChange) { return aChange; })) {
					next.push_back(aOld.Conversations[index]);
					continue;
				}
				next.push_back(classify_feedback_conversation(posts[begin].Conversation, std::span{ posts }.subspan(begin, end - begin)));
				if (next.back() != aOld.Conversations[index]) {
					aRefresh.ChangedConversations.push_back(next.back().Id);
				}
			}
		}
	}

	auto stable_name(feedback_root_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case feedback_root_status::labelled:
			return "labelled";
		case feedback_root_status::unknown_label:
			return "unknown_label";
		case feedback_root_status::plain:
			return "plain";
		case feedback_root_status::unclassified:
			return "unclassified";
		case feedback_root_status::root_missing:
			return "root_missing";
		case feedback_root_status::root_deleted:
			return "root_deleted";
		case feedback_root_status::root_ambiguous:
			return "root_ambiguous";
		}
		return "unknown";
	}
	auto classification_known(feedback_root_status aStatus) noexcept -> bool
	{
		return aStatus == feedback_root_status::labelled || aStatus == feedback_root_status::unknown_label || aStatus == feedback_root_status::plain;
	}
	auto stable_name(feedback_refresh_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_refresh_refusal::none:
			return "none";
		case feedback_refresh_refusal::invalid_scope:
			return "invalid_scope";
		case feedback_refresh_refusal::invalid_identity:
			return "invalid_identity";
		case feedback_refresh_refusal::duplicate_post:
			return "duplicate_post";
		}
		return "unknown";
	}

	auto published_feedback_segment(const std::string_view aBody) -> feedback_body_segment
	{
		const auto stop = map_tour_stop(aBody);
		if (stop.Narrative) {
			return { .Managed = true, .UserBody = feedback_span{ stop.Narrative->Begin, stop.Narrative->End } };
		}
		if (stop.Heading) {
			return { .Managed = true };
		}
		return { };
	}

	auto classify_feedback_conversation(std::string_view aConversation, std::span<const feedback_post> aPosts) -> feedback_conversation
	{
		feedback_conversation result{ .Id = std::string{ aConversation } };
		const auto            index = index_by(aPosts, [](const feedback_post& aPost) -> std::string_view { return aPost.Id; });
		const auto            roots = std::ranges::count_if(aPosts, [](const feedback_post& aPost) { return !aPost.ReplyTo; });
		if (roots == 0) {
			result.Status = feedback_root_status::root_missing;
			return result;
		}
		if (roots > 1 || has_duplicate(index) || !every_reply_reaches_root(aPosts, index)) {
			result.Status = feedback_root_status::root_ambiguous;
			return result;
		}
		const auto& root = *std::ranges::find_if(aPosts, [](const feedback_post& aPost) { return !aPost.ReplyTo; });
		result.Root      = root.Id;
		if (root.Deleted) {
			result.Status = feedback_root_status::root_deleted;
			return result;
		}
		result.Status = status_of(root.Annotation);
		if (result.Status == feedback_root_status::labelled) {
			result.Label = root.Annotation->Label;
		}
		return result;
	}

	auto refresh_feedback_annotations(const feedback_annotation_set& aCurrent, const cached_review_key& aScope,
									  std::span<const feedback_post_observation> aPosts) -> feedback_annotation_refresh
	{
		feedback_annotation_refresh refresh{ .BaseScope = aCurrent.Scope, .BaseEpoch = aCurrent.Epoch, .ScopeReplaced = aCurrent.Scope != aScope };
		if (!valid(aScope)) {
			refresh.Refusal = feedback_refresh_refusal::invalid_scope;
			return refresh;
		}
		if (!std::ranges::all_of(aPosts, valid_identities)) {
			refresh.Refusal = feedback_refresh_refusal::invalid_identity;
			return refresh;
		}
		const auto observed = index_by(aPosts, [](const feedback_post_observation& aPost) { return aPost.Id; });
		const auto repeated = repeated_pages(aPosts, observed);
		if (!repeated) {
			refresh.Refusal = feedback_refresh_refusal::duplicate_post;
			return refresh;
		}
		const auto previous = index_by(aCurrent.Posts, [](const feedback_post& aPost) -> std::string_view { return aPost.Id; });
		auto&      next     = refresh.Next;
		next.Scope          = aScope;
		next.Epoch          = aCurrent.Epoch + 1U;
		next.Posts.reserve(aPosts.size());
		// One post from its observation and the kept post it continues, if
		// any; true when its classification inputs may have changed.
		const auto admit = [&](const feedback_post_observation& aObservation, const feedback_post* aPrior) -> bool {
			feedback_post post{ .Conversation = std::string{ aObservation.Conversation },
								.Id           = std::string{ aObservation.Id },
								.ReplyTo      = aObservation.ReplyTo ? std::optional<std::string>{ *aObservation.ReplyTo } : std::nullopt,
								.Deleted      = aObservation.Deleted,
								.Segment      = aObservation.Segment };
			auto          changed = true;
			if (aObservation.Deleted) {
				if (aPrior != nullptr && !aPrior->Deleted) {
					refresh.Removed.push_back(post.Id);
				}
				changed = aPrior == nullptr || !aPrior->Deleted;
			} else if (aPrior != nullptr && reusable(*aPrior, aObservation)) {
				post.Annotation = aPrior->Annotation;
				post.Body       = aPrior->Body;
				++refresh.Reused;
				changed = false;
			} else {
				if (aObservation.BodyDigest) {
					post.Annotation = recognize_feedback_body(aObservation.Body, aObservation.Segment, *aObservation.BodyDigest);
					++refresh.Supplied;
				} else {
					post.Annotation = recognize_feedback_body(aObservation.Body, aObservation.Segment);
					++refresh.Hashed;
					refresh.HashedBytes += aObservation.Body.size();
				}
				post.Body = std::make_shared<const std::string>(aObservation.Body);
				refresh.Recognized.push_back(post.Id);
			}
			next.Posts.push_back(std::move(post));
			return changed;
		};

		// [[BPR-1349]]. The kept posts observed again, as they usually are:
		// the kept order is the next order, so nothing is sorted or looked up,
		// and only conversations with a changed post are classified again.
		if (const auto carried = refresh.ScopeReplaced ? std::nullopt : carried_order(aCurrent, aPosts, observed, previous, *repeated)) {
			refresh.Carried = true;
			std::vector<bool> changed(aCurrent.Posts.size());
			for (std::size_t kept = 0U; kept < aCurrent.Posts.size(); ++kept) {
				changed[kept] = admit(aPosts[(*carried)[kept]], &aCurrent.Posts[kept]);
			}
			std::ranges::sort(refresh.Recognized);
			std::ranges::sort(refresh.Removed);
			carry_classification(refresh, aCurrent, changed);
			return refresh;
		}

		for (std::size_t at = 0U; at < aPosts.size(); ++at) {
			const auto& observation = aPosts[at];
			if ((*repeated)[at]) {
				++refresh.Collapsed;
				continue;
			}
			const auto prior = refresh.ScopeReplaced ? std::nullopt : find_in(previous, observation.Id);
			static_cast<void>(admit(observation, prior ? &aCurrent.Posts[*prior] : nullptr));
		}
		for (const auto& post : aCurrent.Posts) {
			if (refresh.ScopeReplaced || (!post.Deleted && !find_in(observed, post.Id))) {
				refresh.Removed.push_back(post.Id);
			}
		}
		std::ranges::sort(next.Posts, by_conversation);
		std::ranges::sort(refresh.Recognized);
		std::ranges::sort(refresh.Removed);
		classify_all(refresh, aCurrent);
		return refresh;
	}

	auto adopt_feedback_refresh(const feedback_annotation_set& aCurrent, feedback_annotation_refresh aRefresh) -> std::optional<feedback_annotation_set>
	{
		if (aRefresh.Refusal != feedback_refresh_refusal::none || aRefresh.BaseEpoch != aCurrent.Epoch || aRefresh.BaseScope != aCurrent.Scope) {
			return std::nullopt;
		}
		return std::move(aRefresh.Next);
	}
}
