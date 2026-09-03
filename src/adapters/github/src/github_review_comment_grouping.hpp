#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_REVIEW_COMMENT_GROUPING_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_REVIEW_COMMENT_GROUPING_HPP

#include "bpr/application/github_read_model.hpp"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail {
	/// \brief Reconstructs conversation grouping from reply linkage alone.
	///
	/// \details The REST inline review-comment listing is flat and carries
	/// exactly one structural fact, `in_reply_to_id`. Every rule below is
	/// about that fact and none is about position: a reply must name a
	/// comment present in the same read, must not name itself, and must name
	/// a comment that replies to nothing. The last rule refuses a
	/// reply-to-reply chain and, with it, every cycle, because a cycle needs
	/// some member to reply to a comment that is itself a reply. Server order
	/// is preserved within a group and between groups, but order decides no
	/// membership and no resolution is inferred from any of it.
	///
	/// One function serves both the paged provider read and the recorded
	/// contract normalizer so the two cannot disagree about what a
	/// conversation is.
	[[nodiscard]] inline auto group_review_comments(
		std::vector<application::github_review_comment_record>        aFlat,
		std::vector<application::github_review_comment_group_record>& aGroups)
		-> bool
	{
		std::unordered_map<std::string_view, std::size_t> byIdentity;
		byIdentity.reserve(aFlat.size());
		for (std::size_t index = 0U; index < aFlat.size(); ++index) {
			if (!byIdentity.emplace(aFlat[index].Comment.value(), index).second) {
				return false;
			}
		}
		for (const auto& record : aFlat) {
			if (!record.ReplyTo.has_value()) {
				continue;
			}
			const auto target = byIdentity.find(record.ReplyTo->value());
			if (
				target == byIdentity.end() || *record.ReplyTo == record.Comment
				|| aFlat[target->second].ReplyTo.has_value()) {
				return false;
			}
		}

		std::unordered_map<std::string_view, std::size_t> groupOfRoot;
		groupOfRoot.reserve(aFlat.size());
		aGroups.reserve(aGroups.size() + aFlat.size());
		for (const auto& record : aFlat) {
			if (record.ReplyTo.has_value()) {
				continue;
			}
			groupOfRoot.emplace(record.Comment.value(), aGroups.size());
			aGroups.push_back(
				application::github_review_comment_group_record{
					.Root    = record,
					.Replies = { },
				});
		}
		for (auto& record : aFlat) {
			if (!record.ReplyTo.has_value()) {
				continue;
			}
			const auto group = groupOfRoot.find(record.ReplyTo->value());
			if (group == groupOfRoot.end()) {
				return false;
			}
			aGroups[group->second].Replies.push_back(std::move(record));
		}
		return true;
	}
} // namespace bpr::adapters::github::detail

#endif // BPR_SRC_ADAPTERS_GITHUB_SRC_GITHUB_REVIEW_COMMENT_GROUPING_HPP
