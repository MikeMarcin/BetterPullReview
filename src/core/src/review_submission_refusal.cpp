#include "bpr/core/review_submission_refusal.hpp"

#include <array>

namespace bpr::core {
	namespace {
		using reason = review_submission_refusal_reason;
		using part   = review_submission_refusal_part;

		constexpr auto check_names  = std::array<std::string_view, 3>{ "workflow", "host", "session" };
		constexpr auto reason_names = std::array<std::string_view, review_submission_refusal_reason_count>{
			"request_stopped",
			"grant_inactive",
			"provider_unavailable",
			"identity_mismatch_account",
			"identity_mismatch_pull_request",
			"invalid_request",
			"recovery_record_unavailable",
			"connection_lease_inactive",
			"connection_unavailable",
			"target_moved",
			"stale_head",
			"review_list_unavailable",
			"binding_changed",
			"comments_not_held",
			"admission_refused",
		};
		constexpr auto part_names = std::array<std::string_view, review_submission_refusal_part_count>{
			"none",
			"summary_missing",
			"summary_too_long",
			"summary_not_text",
			"comment_body_missing",
			"comment_body_too_long",
			"comment_not_text",
			"comment_path",
			"comment_place",
			"too_many_comments",
			"review_too_large",
			"request",
		};
		static_assert(reason_names.size() == static_cast<std::size_t>(reason::admission_refused) + 1U);
		static_assert(part_names.size() == static_cast<std::size_t>(part::request) + 1U);

		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto named(const std::array<std::string_view, N>& aNames, const TEnum aValue) noexcept -> std::string_view
		{
			const auto index = static_cast<std::size_t>(aValue);
			return index < N ? aNames[index] : std::string_view{ };
		}
		template <typename TEnum, std::size_t N>
		[[nodiscard]] auto lookup(const std::array<std::string_view, N>& aNames, const std::string_view aName) noexcept -> std::optional<TEnum>
		{
			for (auto index = std::size_t{ 0U }; index < N; ++index) {
				if (aNames[index] == aName) {
					return static_cast<TEnum>(index);
				}
			}
			return std::nullopt;
		}

		[[nodiscard]] auto comment_part(const part aPart) noexcept -> bool
		{
			switch (aPart) {
			case part::comment_body_missing:
			case part::comment_body_too_long:
			case part::comment_not_text:
			case part::comment_path:
			case part::comment_place:         return true;
			default:                          return false;
			}
		}

		/// Where the comment an invalid part names is, as the reviewer sees
		/// it: `path:line`, GitHub's one-based line.
		[[nodiscard]] auto place(const review_submission_refusal& aRefusal, const review_submission_refusal_context& aContext) -> std::string
		{
			if (aRefusal.Comment == 0U || aRefusal.Comment > aContext.Comments.size()) {
				return "a pending comment";
			}
			const auto& comment = aContext.Comments[aRefusal.Comment - 1U];
			return "the comment on " + comment.Path + ":" + std::to_string(std::uint64_t{ comment.Line } + 1U);
		}

		[[nodiscard]] auto invalid(const review_submission_refusal& aRefusal, const review_submission_refusal_context& aContext) -> std::string
		{
			switch (aRefusal.Part) {
			case part::summary_missing:       return "Not published: this verdict needs a summary. Write one, then submit again.";
			case part::summary_too_long:      return "Not published: the summary is longer than GitHub accepts. Shorten it, then submit again.";
			case part::summary_not_text:      return "Not published: the summary holds characters GitHub can't take. Edit it, then submit again.";
			case part::comment_body_missing:  return "Not published: " + place(aRefusal, aContext) + " has no text. Write it or delete it, then submit again.";
			case part::comment_body_too_long: return "Not published: " + place(aRefusal, aContext) + " is longer than GitHub accepts. Shorten it, then submit again.";
			case part::comment_not_text:      return "Not published: " + place(aRefusal, aContext) + " holds characters GitHub can't take. Edit it, then submit again.";
			case part::comment_path:          return "Not published: " + place(aRefusal, aContext) + " names a file path GitHub can't take. Delete it, then submit again.";
			case part::comment_place:         return "Not published: " + place(aRefusal, aContext) + " names lines that don't hold together. Delete it and write it again, then submit.";
			case part::too_many_comments:     return "Not published: this review holds more than " + std::to_string(maximum_review_submission_comments) + " pending comments. Add some on their own with Add single comment, then submit again.";
			case part::review_too_large:      return "Not published: this review is too long to send at once. Shorten the summary or add some comments on their own with Add single comment.";
			case part::none:
			case part::request:               break;
			}
			return "Not published: BetterPullReview couldn't write this review as GitHub's request, and nothing was sent. Your review is kept. Copy diagnostics says which check refused it.";
		}
	}

	auto name(const review_submission_refusal_check aCheck) noexcept -> std::string_view { return named(check_names, aCheck); }
	auto name(const review_submission_refusal_reason aReason) noexcept -> std::string_view { return named(reason_names, aReason); }
	auto name(const review_submission_refusal_part aPart) noexcept -> std::string_view { return named(part_names, aPart); }
	auto review_submission_refusal_check_named(const std::string_view aName) noexcept -> std::optional<review_submission_refusal_check> { return lookup<review_submission_refusal_check>(check_names, aName); }
	auto review_submission_refusal_reason_named(const std::string_view aName) noexcept -> std::optional<review_submission_refusal_reason> { return lookup<review_submission_refusal_reason>(reason_names, aName); }
	auto review_submission_refusal_part_named(const std::string_view aName) noexcept -> std::optional<review_submission_refusal_part> { return lookup<review_submission_refusal_part>(part_names, aName); }

	auto valid(const review_submission_refusal& aRefusal) noexcept -> bool
	{
		if (name(aRefusal.Check).empty() || name(aRefusal.Reason).empty() || name(aRefusal.Part).empty()) {
			return false;
		}
		// A part only with an invalid request.
		if (aRefusal.Reason != reason::invalid_request && aRefusal.Part != part::none) {
			return false;
		}
		return aRefusal.Comment == 0U || (comment_part(aRefusal.Part) && aRefusal.Comment <= maximum_review_submission_comments);
	}

	auto decide_review_submission_repair(const review_submission_refusal& aRefusal, const bool aRepaired, const bool aSameAuthority) noexcept -> review_submission_repair
	{
		switch (aRefusal.Reason) {
		case reason::stale_head:
		case reason::target_moved:
			// The commit under the review changed: whatever was repaired
			// before, it is read again and never sent unconfirmed.
			return review_submission_repair::reread_and_confirm;
		case reason::grant_inactive:
		case reason::provider_unavailable:
		case reason::connection_lease_inactive:
		case reason::connection_unavailable:
		case reason::identity_mismatch_account:
		case reason::identity_mismatch_pull_request:
			// The application's own connection, for the account and pull
			// request the review was written for: rebuilt once.
			return aRepaired || !aSameAuthority ? review_submission_repair::none : review_submission_repair::reconnect_and_retry;
		default: return review_submission_repair::none;
		}
	}

	auto sentence(const review_submission_refusal& aRefusal, const review_submission_refusal_context& aContext) -> std::string
	{
		const auto comment = aContext.Subject == review_submission_refusal_subject::comment;
		const auto noun    = std::string{ comment ? "comment" : "review" };
		const auto kept    = comment ? std::string{ "Your draft is kept." } : std::string{ "Your review is kept." };
		const auto reread  = aContext.Rereading ? std::string{ " BetterPullReview is reading the pull request again." } : std::string{ };
		switch (aRefusal.Reason) {
		case reason::request_stopped:                return "Not published: sending was stopped before anything reached GitHub. " + kept + " Submit it again when you're ready.";
		case reason::grant_inactive:
		case reason::provider_unavailable:
		case reason::connection_lease_inactive:
		case reason::connection_unavailable:
		case reason::admission_refused:              return "Not published: the connection to GitHub wasn't ready, and reconnecting didn't fix it. Nothing was sent. " + kept + " Refresh the pull request to reconnect, then submit again.";
		case reason::identity_mismatch_account:      return "Not published: the account signed in now isn't the one this " + noun + " was written as. Nothing was sent. " + kept + " Switch back to that account to submit it.";
		case reason::identity_mismatch_pull_request: return "Not published: the pull request open now isn't the one this " + noun + " was written for. Nothing was sent. " + kept;
		case reason::binding_changed:                return "Not published: the account or pull request changed after you chose Submit. Nothing was sent. " + kept;
		case reason::invalid_request:                return invalid(aRefusal, aContext);
		case reason::recovery_record_unavailable:    return "Not published: BetterPullReview couldn't use the local record it keeps so that a review is never sent twice, and nothing was sent. " + kept + " Copy diagnostics says why.";
		case reason::review_list_unavailable:        return "Not published: BetterPullReview couldn't ask GitHub for this pull request's reviews, which it reads before sending, and nothing was sent. " + kept + " Copy diagnostics says why.";
		case reason::comments_not_held:              return "Not published: a pending comment changed or couldn't be held for this review. Nothing was sent. Check the comments, then submit again.";
		case reason::stale_head:                     return "Not published: the pull request has new commits since you wrote this " + noun + ", so nothing was sent." + reread + " Look at the new commits before you submit, so your " + noun + " covers code you've seen.";
		case reason::target_moved:                   return "Not published: the pull request's base changed since you wrote this " + noun + ", so nothing was sent." + reread + " Check the changes before you submit again.";
		}
		return "Not published: nothing was sent. " + kept + " Copy diagnostics says which check refused it.";
	}
} // namespace bpr::core
