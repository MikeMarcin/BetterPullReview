#include "bpr/core/conversation_timeline.hpp"

#include <algorithm>
#include <optional>
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
		// [[BPR-2171]]. Each unit becomes pieces: a head row (a bundle's or a
		// verdict's line, or a bots row) or a comment shown after the filters.
		// A thread's context says which threads share a file row: those of one
		// bundle, or lone threads in a row.
		struct piece
		{
			const timeline_item*        Item = nullptr;
			std::string                 Review;
			std::string                 Context;
			std::optional<timeline_row> Head;
		};
		struct shown_unit
		{
			std::vector<piece> Pieces;
			/// Its author is a bot and nothing in it is kept.
			bool Bot = false;
			/// What the unit carried that is a comment, in order.
			std::vector<const timeline_item*> Comments;
			std::string_view                  Author;
		};
		auto shownUnits = std::vector<shown_unit>{ };
		shownUnits.reserve(units.size());
		for (auto& entry : units) {
			auto unit = shown_unit{ };
			if (entry.Review == nullptr) {
				const auto& item = *entry.Members.front();
				if (!shown(item, aFilters)) {
					continue;
				}
				unit.Pieces.push_back(piece{ .Item = &item, .Context = item.Kind == timeline_item_kind::thread ? std::string{ "lone" } : std::string{ } });
				unit.Comments.push_back(&item);
				unit.Bot    = item.Bot && !item.Kept;
				unit.Author = item.Author;
				shownUnits.push_back(std::move(unit));
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
					unit.Pieces.push_back(piece{ .Head = std::move(head) });
					shownUnits.push_back(std::move(unit));
				}
				continue;
			}
			auto kept = false;
			for (const auto* member : members) {
				if (!shown(*member, aFilters)) {
					continue;
				}
				if (unit.Pieces.empty()) {
					unit.Pieces.push_back(piece{ .Head = head });
				}
				unit.Pieces.push_back(piece{ .Item = member, .Review = review.Id, .Context = member->Kind == timeline_item_kind::thread ? "bundle:" + review.Id : std::string{ } });
				unit.Comments.push_back(member);
				kept = kept || member->Kept;
			}
			if (!unit.Pieces.empty()) {
				unit.Bot    = review.Bot && !kept;
				unit.Author = review.Author;
				shownUnits.push_back(std::move(unit));
			}
		}
		// Runs of bot units take one bots row.
		auto pieces = std::vector<piece>{ };
		for (std::size_t at = 0U; at < shownUnits.size();) {
			const auto& first = shownUnits[at];
			if (!aFilters.FoldBots || !first.Bot || first.Comments.empty()) {
				pieces.insert(pieces.end(), first.Pieces.begin(), first.Pieces.end());
				++at;
				continue;
			}
			auto end = at;
			while (end < shownUnits.size() && shownUnits[end].Bot && !shownUnits[end].Comments.empty()) {
				++end;
			}
			auto row = timeline_row{ .Kind = timeline_row_kind::bots, .Id = "bots:" + first.Comments.front()->Id };
			row.Fold = row.Id;
			row.Open = aFilters.Unfolded.contains(row.Fold);
			for (auto unit = at; unit < end; ++unit) {
				for (const auto* item : shownUnits[unit].Comments) {
					row.Members.push_back(item->Id);
				}
				if (std::ranges::find(row.Authors, shownUnits[unit].Author) == row.Authors.end()) {
					row.Authors.emplace_back(shownUnits[unit].Author);
				}
			}
			const auto open = row.Open;
			if (!open) {
				result.Folded += row.Members.size();
			}
			pieces.push_back(piece{ .Head = std::move(row) });
			if (open) {
				for (auto unit = at; unit < end; ++unit) {
					pieces.insert(pieces.end(), shownUnits[unit].Pieces.begin(), shownUnits[unit].Pieces.end());
				}
			}
			at = end;
		}
		for (std::size_t at = 0U; at < pieces.size();) {
			auto& current = pieces[at];
			if (current.Head.has_value()) {
				result.Verdicts += current.Head->Kind == timeline_row_kind::verdict ? 1U : 0U;
				result.Rows.push_back(std::move(*current.Head));
				++at;
				continue;
			}
			const auto& item = *current.Item;
			if (!aFilters.Files || item.Kind != timeline_item_kind::thread) {
				comment(item, current.Review);
				++at;
				continue;
			}
			// One file row over the threads that follow one another on it.
			auto end = at + 1U;
			while (end < pieces.size() && !pieces[end].Head.has_value() && pieces[end].Item->Kind == timeline_item_kind::thread
				   && pieces[end].Context == current.Context && pieces[end].Item->Path == item.Path) {
				++end;
			}
			const auto folds = [&aFilters](const timeline_item& aThread) { return aFilters.FoldResolved && aThread.Resolved && !aThread.Unread && !aThread.Kept; };
			auto       row   = timeline_row{ .Kind = timeline_row_kind::file, .Id = item.Id, .Review = current.Review, .Path = item.Path };
			for (auto member = at; member < end; ++member) {
				if (folds(*pieces[member].Item)) {
					row.Members.push_back(pieces[member].Item->Id);
				}
			}
			if (!row.Members.empty()) {
				row.Fold = "file:" + item.Id;
				row.Open = aFilters.Unfolded.contains(row.Fold);
			}
			const auto open = row.Open;
			result.Rows.push_back(std::move(row));
			for (auto member = at; member < end; ++member) {
				if (!open && folds(*pieces[member].Item)) {
					++result.Folded;
					continue;
				}
				comment(*pieces[member].Item, pieces[member].Review);
			}
			at = end;
		}
		return result;
	}

	auto summarize_conversation_timeline(const std::span<const timeline_item> aItems, const timeline_filters& aFilters)
		-> timeline_filter_summary
	{
		auto summary      = timeline_filter_summary{ };
		auto resolvedById = std::unordered_map<std::string_view, bool>{ };
		resolvedById.reserve(aItems.size());
		// The unfiltered timeline lists every comment once, in any bundle.
		// Verdict-only reviews add no comment. Resolution uses the first item
		// of each identity, as when matching the arranged rows back to items.
		for (const auto& item : aItems) {
			const auto resolved = resolvedById.try_emplace(item.Id, item.Resolved).first;
			if (item.Kind == timeline_item_kind::review && !item.HasBody) {
				continue;
			}
			++summary.Comments;
			summary.Unread += item.Unread ? 1U : 0U;
			summary.Resolved += resolved->second ? 1U : 0U;
		}
		const auto filtering = aFilters.UnreadOnly || aFilters.UnresolvedOnly || aFilters.Listed.has_value();
		summary.Shown        = filtering ? arrange_conversation_timeline(aItems, aFilters).Comments : summary.Comments;
		if (summary.Shown > 0U || summary.Comments == 0U || !filtering) {
			return summary;
		}
		if (aFilters.Listed.has_value()) {
			summary.Empty = timeline_empty::no_match;
		} else if (aFilters.UnreadOnly && aFilters.UnresolvedOnly) {
			summary.Empty = summary.Unread == 0U ? timeline_empty::no_new : timeline_empty::no_new_unresolved;
		} else if (aFilters.UnreadOnly) {
			summary.Empty = timeline_empty::no_new;
		} else {
			summary.Empty = timeline_empty::no_unresolved;
		}
		return summary;
	}
}
