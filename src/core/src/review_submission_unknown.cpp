#include "bpr/core/review_submission_unknown.hpp"

#include "bpr/core/review_submission.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>

namespace bpr::core {
	namespace {
		constexpr std::array<std::string_view, review_submission_unknown_reason_count> reason_names{
			"review_not_listed",
			"several_reviews_listed",
			"review_list_unreadable",
			"review_list_too_large",
			"review_list_paged",
			"comment_page_unreadable",
			"comment_page_too_large",
			"comment_page_link_refused",
			"comment_collection_too_long",
			"comments_mismatch",
			"comment_moved_without_original_position",
			"http_status",
			"rate_limited",
			"timeout",
			"network",
			"account_mismatch",
			"pull_request_mismatch",
			"connection_unavailable",
			"local_record_unavailable",
			"cancelled",
		};
		constexpr std::array<std::string_view, 4> answer_names{ "not_sent", "success", "error_status", "no_answer" };

		[[nodiscard]] auto status_class(const std::uint8_t aClass) noexcept -> bool
		{
			return aClass == 0U || (aClass >= 2U && aClass <= 5U);
		}

		/// One `key=value` word, the value a decimal with no leading zero.
		[[nodiscard]] auto number(std::string_view& aText, const std::string_view aKey, const std::uint64_t aMaximum) -> std::optional<std::uint64_t>
		{
			if (!aText.starts_with(aKey) || aText.substr(aKey.size()).empty() || aText[aKey.size()] != '=') {
				return std::nullopt;
			}
			aText.remove_prefix(aKey.size() + 1U);
			const auto end    = aText.find(' ');
			const auto digits = aText.substr(0U, end);
			auto       value  = std::uint64_t{ 0U };
			const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
			if (digits.empty() || (digits.size() > 1U && digits.front() == '0') || parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || value > aMaximum) {
				return std::nullopt;
			}
			aText.remove_prefix(end == std::string_view::npos ? aText.size() : end + 1U);
			if (end != std::string_view::npos && aText.empty()) {
				return std::nullopt;
			}
			return value;
		}

		[[nodiscard]] auto word(std::string_view& aText, const std::string_view aKey) -> std::optional<std::string_view>
		{
			if (!aText.starts_with(aKey) || aText.substr(aKey.size()).empty() || aText[aKey.size()] != '=') {
				return std::nullopt;
			}
			aText.remove_prefix(aKey.size() + 1U);
			const auto end   = aText.find(' ');
			const auto value = aText.substr(0U, end);
			aText.remove_prefix(end == std::string_view::npos ? aText.size() : end + 1U);
			if (end != std::string_view::npos && aText.empty()) {
				return std::nullopt;
			}
			return value;
		}
	} // namespace

	auto name(const review_submission_unknown_reason aReason) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aReason);
		return index < reason_names.size() ? reason_names[index] : std::string_view{ };
	}

	auto name(const review_submission_post_answer aAnswer) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aAnswer);
		return index < answer_names.size() ? answer_names[index] : std::string_view{ };
	}

	auto review_submission_unknown_reason_named(const std::string_view aName) noexcept -> std::optional<review_submission_unknown_reason>
	{
		for (std::size_t index = 0U; index < reason_names.size(); ++index) {
			if (reason_names[index] == aName) {
				return static_cast<review_submission_unknown_reason>(index);
			}
		}
		return std::nullopt;
	}

	auto valid(const review_submission_unknown_diagnosis& aDiagnosis) noexcept -> bool
	{
		const auto answered = aDiagnosis.PostAnswer == review_submission_post_answer::success || aDiagnosis.PostAnswer == review_submission_post_answer::error_status;
		return !name(aDiagnosis.Reason).empty() && !name(aDiagnosis.PostAnswer).empty() && status_class(aDiagnosis.PostStatusClass)
			&& status_class(aDiagnosis.HttpStatusClass) && (aDiagnosis.PostStatusClass == 0U || answered)
			&& aDiagnosis.CommentsExpected <= maximum_review_submission_comments && aDiagnosis.CommentsMatched <= aDiagnosis.CommentsExpected
			&& aDiagnosis.CommentsMatched <= aDiagnosis.CommentsFound;
	}

	auto describe(const review_submission_unknown_diagnosis& aDiagnosis) -> std::string
	{
		if (!valid(aDiagnosis)) {
			return { };
		}
		auto text = std::string{ "review_submission=" };
		text += name(aDiagnosis.Reason);
		text += " post=";
		text += name(aDiagnosis.PostAnswer);
		for (const auto& [key, value] : std::array<std::pair<std::string_view, unsigned>, 7>{ { { " post_class=", aDiagnosis.PostStatusClass },
																								{ " http_class=", aDiagnosis.HttpStatusClass },
																								{ " expected=", aDiagnosis.CommentsExpected },
																								{ " found=", aDiagnosis.CommentsFound },
																								{ " matched=", aDiagnosis.CommentsMatched },
																								{ " candidates=", aDiagnosis.CandidateReviews },
																								{ " pages=", aDiagnosis.PagesRead } } }) {
			text += key;
			text += std::to_string(value);
		}
		return text;
	}

	auto parse_review_submission_unknown(std::string_view aText) -> std::optional<review_submission_unknown_diagnosis>
	{
		auto       result = review_submission_unknown_diagnosis{ };
		const auto reason = word(aText, "review_submission");
		const auto answer = reason ? word(aText, "post") : std::nullopt;
		if (!reason || !answer) {
			return std::nullopt;
		}
		const auto named = review_submission_unknown_reason_named(*reason);
		const auto found = std::ranges::find(answer_names, *answer);
		if (!named || found == answer_names.end()) {
			return std::nullopt;
		}
		result.Reason        = *named;
		result.PostAnswer    = static_cast<review_submission_post_answer>(found - answer_names.begin());
		constexpr auto small = std::uint64_t{ std::numeric_limits<std::uint8_t>::max() };
		constexpr auto large = std::uint64_t{ std::numeric_limits<std::uint16_t>::max() };
		const auto     post  = number(aText, "post_class", small);
		const auto     http  = post ? number(aText, "http_class", small) : std::nullopt;
		const auto     want  = http ? number(aText, "expected", large) : std::nullopt;
		const auto     held  = want ? number(aText, "found", large) : std::nullopt;
		const auto     same  = held ? number(aText, "matched", large) : std::nullopt;
		const auto     count = same ? number(aText, "candidates", large) : std::nullopt;
		const auto     pages = count ? number(aText, "pages", large) : std::nullopt;
		if (!pages || !aText.empty()) {
			return std::nullopt;
		}
		result.PostStatusClass  = static_cast<std::uint8_t>(*post);
		result.HttpStatusClass  = static_cast<std::uint8_t>(*http);
		result.CommentsExpected = static_cast<std::uint16_t>(*want);
		result.CommentsFound    = static_cast<std::uint16_t>(*held);
		result.CommentsMatched  = static_cast<std::uint16_t>(*same);
		result.CandidateReviews = static_cast<std::uint16_t>(*count);
		result.PagesRead        = static_cast<std::uint16_t>(*pages);
		if (!valid(result)) {
			return std::nullopt;
		}
		return result;
	}
} // namespace bpr::core
