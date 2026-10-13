#include "bpr/core/pull_request_location.hpp"

#include "bpr/core/hex.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto lowercase_hex(const std::string_view aText, const std::size_t aMinimum, const std::size_t aMaximum) noexcept -> bool
		{
			return aText.size() >= aMinimum && aText.size() <= aMaximum
				&& std::ranges::all_of(aText, [](const char aValue) {
					   return (aValue >= '0' && aValue <= '9') || (aValue >= 'a' && aValue <= 'f');
				   });
		}

		[[nodiscard]] auto unavailable(const pull_request_location_refusal aRefusal) -> pull_request_location_target
		{
			return { .Action = pull_request_location_action::unavailable, .Refusal = aRefusal };
		}

		[[nodiscard]] auto conversation_for(
			const std::span<const pull_request_location_conversation> aKnown,
			const std::uint64_t                                       aIdentity) -> const pull_request_location_conversation*
		{
			const auto found = std::ranges::find(aKnown, aIdentity, &pull_request_location_conversation::Identity);
			return found == aKnown.end() ? nullptr : &*found;
		}

		/// A digest names a file only when exactly one changed path hashes to
		/// it; two would mean the facts are not what they claim to be.
		[[nodiscard]] auto path_for(const std::span<const std::string> aPaths, const std::string_view aDigest) -> const std::string*
		{
			const std::string* named = nullptr;
			for (const auto& path : aPaths) {
				if (pull_request_file_digest(path) != aDigest) {
					continue;
				}
				if (named != nullptr) {
					return nullptr;
				}
				named = &path;
			}
			return named;
		}

		[[nodiscard]] auto resolve_file(const pull_request_location& aLocation, const pull_request_location_facts& aFacts) -> pull_request_location_target
		{
			const auto* const path = path_for(aFacts.ChangedPaths, aLocation.FileDigest);
			if (path == nullptr) {
				return unavailable(pull_request_location_refusal::file_not_in_pull_request);
			}
			auto target = pull_request_location_target{ .Action = pull_request_location_action::select_file, .Path = *path };
			if (!aLocation.Side.has_value()) {
				return target;
			}
			if (*aLocation.Side == diff_side::left) {
				target.Refusal = pull_request_location_refusal::base_side_line;
			} else if (!aFacts.RightIsHead) {
				target.Refusal = pull_request_location_refusal::line_needs_head;
			} else {
				target.RightLine = aLocation.Line - 1U;
			}
			return target;
		}

		[[nodiscard]] auto resolve_conversation(
			const std::span<const pull_request_location_conversation> aKnown,
			const std::uint64_t                                       aIdentity,
			const pull_request_location_refusal                       aAbsent) -> pull_request_location_target
		{
			const auto* const known = conversation_for(aKnown, aIdentity);
			if (known == nullptr) {
				return unavailable(aAbsent);
			}
			return { .Action = pull_request_location_action::reveal_conversation, .Conversation = known->Conversation };
		}
	} // namespace

	auto valid(const pull_request_location& aLocation) noexcept -> bool
	{
		const auto noCommit   = aLocation.Commit.empty();
		const auto noFile     = aLocation.FileDigest.empty() && !aLocation.Side.has_value() && aLocation.Line == 0U;
		const auto noIdentity = aLocation.Identity == 0U;
		switch (aLocation.Kind) {
		case pull_request_location_kind::pull_request:
		case pull_request_location_kind::unrecognized:
			return noCommit && noFile && noIdentity;
		case pull_request_location_kind::commit:
			return lowercase_hex(aLocation.Commit, 4U, 64U) && noFile && noIdentity;
		case pull_request_location_kind::file:
			return noCommit && noIdentity && lowercase_hex(aLocation.FileDigest, 64U, 64U)
				&& aLocation.Side.has_value() == (aLocation.Line != 0U)
				&& (!aLocation.Side.has_value() || *aLocation.Side == diff_side::left || *aLocation.Side == diff_side::right);
		case pull_request_location_kind::review:
		case pull_request_location_kind::review_comment:
		case pull_request_location_kind::issue_comment:
			return noCommit && noFile && !noIdentity;
		}
		return false;
	}

	auto resolve_pull_request_location(
		const pull_request_location&       aLocation,
		const pull_request_location_facts& aFacts) -> pull_request_location_target
	{
		if (!valid(aLocation)) {
			return unavailable(pull_request_location_refusal::unrecognized);
		}
		switch (aLocation.Kind) {
		case pull_request_location_kind::pull_request:   return { };
		case pull_request_location_kind::commit:         return unavailable(pull_request_location_refusal::commit_page);
		case pull_request_location_kind::file:           return resolve_file(aLocation, aFacts);
		case pull_request_location_kind::review:         return resolve_conversation(aFacts.Reviews, aLocation.Identity, pull_request_location_refusal::review_not_read);
		case pull_request_location_kind::review_comment: return resolve_conversation(aFacts.ReviewComments, aLocation.Identity, pull_request_location_refusal::comment_not_identified);
		case pull_request_location_kind::issue_comment:  return unavailable(pull_request_location_refusal::issue_comment);
		case pull_request_location_kind::unrecognized:   return unavailable(pull_request_location_refusal::unrecognized);
		}
		return unavailable(pull_request_location_refusal::unrecognized);
	}

	auto explanation(const pull_request_location_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case pull_request_location_refusal::none:                     return { };
		case pull_request_location_refusal::commit_page:              return "A single commit is not shown; the comparison compares uploads.";
		case pull_request_location_refusal::file_not_in_pull_request: return "No changed file of this pull request matches the link.";
		case pull_request_location_refusal::base_side_line:           return "A line on the base side is not shown; GitHub numbers it against a merge base this comparison does not use.";
		case pull_request_location_refusal::line_needs_head:          return "The line is not shown; the comparison on screen does not end at the pull request's head.";
		case pull_request_location_refusal::line_not_in_file:         return "The file has no such line in the comparison on screen.";
		case pull_request_location_refusal::review_not_read:          return "That review is not in what was read of this pull request.";
		case pull_request_location_refusal::comment_not_identified:   return "That comment is not identified in what was read of this pull request.";
		case pull_request_location_refusal::issue_comment:            return "A single discussion comment is not shown from a link.";
		case pull_request_location_refusal::unrecognized:             return "That part of the link is not a location this product reads.";
		}
		return { };
	}

	auto pull_request_file_digest(const std::string_view aPath) -> std::string
	{
		auto bytes = std::vector<std::uint8_t>{ };
		bytes.reserve(aPath.size());
		std::ranges::transform(aPath, std::back_inserter(bytes), [](const char aValue) { return static_cast<std::uint8_t>(aValue); });
		return lower_hex(sha256(bytes));
	}
} // namespace bpr::core
