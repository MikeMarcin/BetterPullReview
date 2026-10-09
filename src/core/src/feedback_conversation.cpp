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
		for (std::size_t at = 0U; at < aPosts.size(); ++at) {
			const auto& observation = aPosts[at];
			if ((*repeated)[at]) {
				++refresh.Collapsed;
				continue;
			}
			feedback_post post{ .Conversation = std::string{ observation.Conversation },
								.Id           = std::string{ observation.Id },
								.ReplyTo      = observation.ReplyTo ? std::optional<std::string>{ *observation.ReplyTo } : std::nullopt,
								.Deleted      = observation.Deleted,
								.Segment      = observation.Segment };
			const auto    prior = refresh.ScopeReplaced ? std::nullopt : find_in(previous, observation.Id);
			if (observation.Deleted) {
				if (prior && !aCurrent.Posts[*prior].Deleted) {
					refresh.Removed.push_back(post.Id);
				}
			} else if (prior && reusable(aCurrent.Posts[*prior], observation)) {
				post.Annotation = aCurrent.Posts[*prior].Annotation;
				post.Body       = aCurrent.Posts[*prior].Body;
				++refresh.Reused;
			} else {
				post.Annotation = recognize_feedback_body(observation.Body, observation.Segment);
				post.Body       = std::make_shared<const std::string>(observation.Body);
				++refresh.Hashed;
				refresh.Recognized.push_back(post.Id);
			}
			next.Posts.push_back(std::move(post));
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
