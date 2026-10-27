#include "bpr/core/conversation_timeline.hpp"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace bpr::core {
	namespace {
		/// One unit of the timeline: a review's bundle or a lone comment.
		struct unit
		{
			std::string_view CreatedAt;
			/// A lone thread's place, which breaks a tie of times; empty for a
			/// bundle or a pull request comment, which come first.
			std::string_view Path;
			std::uint32_t    Line  = 0U;
			std::size_t      Order = 0U;
			/// The review item, for a bundle.
			const timeline_item* Review = nullptr;
			/// The bundle's threads, or the lone comment.
			std::vector<const timeline_item*> Members;
		};

		[[nodiscard]] auto shown(const timeline_item& aItem, const timeline_filters& aFilters) -> bool
		{
			if (aItem.Kept) {
				return true;
			}
			if (aFilters.Listed.has_value() && !aFilters.Listed->contains(aItem.Id)) {
				return false;
			}
			return !(aFilters.UnreadOnly && !aItem.Unread) && !(aFilters.UnresolvedOnly && aItem.Resolved);
		}
	}

	auto arrange_conversation_timeline(const std::span<const timeline_item> aItems, const timeline_filters& aFilters)
		-> conversation_timeline
	{
		auto reviews = std::unordered_map<std::string_view, std::size_t>{ };
		for (std::size_t index = 0U; index < aItems.size(); ++index) {
			if (aItems[index].Kind == timeline_item_kind::review) {
				reviews.try_emplace(aItems[index].Id, index);
			}
		}
		auto units  = std::vector<unit>{ };
		auto bundle = std::unordered_map<std::size_t, std::size_t>{ };
		for (std::size_t index = 0U; index < aItems.size(); ++index) {
			const auto& item = aItems[index];
			if (item.Kind == timeline_item_kind::review) {
				if (!bundle.contains(index)) {
					bundle.emplace(index, units.size());
					units.push_back(unit{ .CreatedAt = item.CreatedAt, .Order = index, .Review = &item });
				}
				continue;
			}
			const auto review = item.Kind == timeline_item_kind::thread && !item.Review.empty() ? reviews.find(item.Review) : reviews.end();
			if (review == reviews.end()) {
				units.push_back(unit{ .CreatedAt = item.CreatedAt, .Path = item.Path, .Line = item.Line.value_or(0U), .Order = index, .Members = { &item } });
				continue;
			}
			auto found = bundle.find(review->second);
			if (found == bundle.end()) {
				const auto& owner = aItems[review->second];
				found             = bundle.emplace(review->second, units.size()).first;
				units.push_back(unit{ .CreatedAt = owner.CreatedAt, .Order = review->second, .Review = &owner });
			}
			units[found->second].Members.push_back(&item);
		}
		std::ranges::stable_sort(units, [](const unit& aLeft, const unit& aRight) {
			return std::tuple{ aLeft.CreatedAt, aLeft.Path, aLeft.Line, aLeft.Order } < std::tuple{ aRight.CreatedAt, aRight.Path, aRight.Line, aRight.Order };
		});
		const auto filtering = aFilters.UnreadOnly || aFilters.UnresolvedOnly || aFilters.Listed.has_value();
		auto       result    = conversation_timeline{ };
		const auto comment   = [&result](const timeline_item& aItem, const std::string& aReview) {
			result.Rows.push_back(timeline_row{ .Kind = timeline_row_kind::comment, .Id = aItem.Id, .Review = aReview, .Item = aItem.Kind });
			++result.Comments;
			result.Threads += aItem.Kind == timeline_item_kind::thread ? 1U : 0U;
			result.Unread += aItem.Unread ? 1U : 0U;
		};
		for (auto& entry : units) {
			if (entry.Review == nullptr) {
				if (shown(*entry.Members.front(), aFilters)) {
					comment(*entry.Members.front(), std::string{ });
				}
				continue;
			}
			const auto& review = *entry.Review;
			// The bundle's threads in file and line order; equal places keep
			// the items' order.
			std::ranges::stable_sort(entry.Members, [](const timeline_item* aLeft, const timeline_item* aRight) {
				return std::tuple{ std::string_view{ aLeft->Path }, aLeft->Line.value_or(0U) }
				< std::tuple{ std::string_view{ aRight->Path }, aRight->Line.value_or(0U) };
			});
			auto members = std::vector<const timeline_item*>{ };
			if (review.HasBody) {
				members.push_back(&review);
			}
			members.insert(members.end(), entry.Members.begin(), entry.Members.end());
			const auto verdict = timeline_verdict_named(review.Verdict) ? review.Verdict : timeline_verdict::none;
			auto       head    = timeline_row{ .Kind = timeline_row_kind::bundle, .Id = review.Id, .Review = review.Id, .Author = review.Author, .CreatedAt = review.CreatedAt, .Verdict = verdict };
			if (members.empty()) {
				// Carried nothing: only a verdict that matters for merging shows.
				if (verdict != timeline_verdict::none && !filtering) {
					head.Kind = timeline_row_kind::verdict;
					result.Rows.push_back(std::move(head));
					++result.Verdicts;
				}
				continue;
			}
			auto headed = false;
			for (const auto* member : members) {
				if (!shown(*member, aFilters)) {
					continue;
				}
				if (!headed) {
					result.Rows.push_back(head);
					headed = true;
				}
				comment(*member, review.Id);
			}
		}
		return result;
	}
}
