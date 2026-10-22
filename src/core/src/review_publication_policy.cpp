#include "bpr/core/review_publication_policy.hpp"

namespace bpr::core {
	auto retain_review_publication_outcome(const review_publication_outcome aPrior, const review_publication_outcome aCurrent) noexcept -> review_publication_outcome
	{
		if (aCurrent == review_publication_outcome::published || aPrior == review_publication_outcome::published) {
			return review_publication_outcome::published;
		}
		// [[BPR-1735]]. The journal's rejection, not a refused check, says no
		// comment exists, so it unlocks a draft whose outcome was unknown.
		if (aCurrent == review_publication_outcome::rejected) {
			return review_publication_outcome::not_published;
		}
		if (aPrior == review_publication_outcome::indeterminate || aCurrent == review_publication_outcome::indeterminate) {
			return review_publication_outcome::indeterminate;
		}
		return review_publication_outcome::not_published;
	}

	namespace {
		[[nodiscard]] auto valid_observation(const review_publication_observation& aObservation) noexcept -> bool
		{
			return aObservation.Placement <= review_placement::unresolvable_line
				&& aObservation.Capability <= off_hunk_creation_capability::unsupported
				&& aObservation.Body <= review_body_compatibility::requires_line_subject
				&& aObservation.State <= review_publication_state::unresolvable_line_admitted;
		}

		[[nodiscard]] auto fallback(const review_publication_observation& aObservation, const review_publication_action aAction) noexcept -> review_publication_decision
		{
			if (aObservation.Body == review_body_compatibility::requires_line_subject) {
				return { .Diagnostic = review_publication_diagnostic::incompatible_body };
			}
			return { .Action = aAction, .Representation = review_publication_representation::file_comment_with_permalink };
		}
	}

	auto plan_review_publication(const review_publication_observation& aObservation) noexcept -> review_publication_decision
	{
		if (!valid_observation(aObservation)) {
			return { .Diagnostic = review_publication_diagnostic::invalid_observation };
		}
		if (!aObservation.Eligible) {
			return { .Diagnostic = review_publication_diagnostic::ineligible_anchor };
		}
		using state          = review_publication_state;
		using action         = review_publication_action;
		using representation = review_publication_representation;
		// [[BPR-1706]]. GitHub resolves an unresolvable line nowhere, so it
		// never has a native child to prepare or post: it is admitted
		// straight to the file comment, and only from the initial state.
		// [[BPR-1735]]. A rejected publication's every child was answered
		// with a fully received refusal, so GitHub holds no comment of it: it
		// starts again as a new attempt, planned as from the initial state.
		const auto fresh        = aObservation.State == state::initial || aObservation.State == state::rejected;
		const auto unresolvable = aObservation.Placement == review_placement::unresolvable_line;
		if (unresolvable && fresh) {
			return fallback(aObservation, action::admit_unresolvable_line);
		}
		if (unresolvable && aObservation.State == state::native_prepared) {
			return { .Diagnostic = review_publication_diagnostic::invalid_observation };
		}
		switch (aObservation.State) {
		case state::initial:
		case state::rejected:
			if (aObservation.Placement == review_placement::off_hunk
				&& aObservation.Capability == off_hunk_creation_capability::unsupported
				&& aObservation.CapabilityCurrent) {
				return fallback(aObservation, action::admit_unsupported_evidence);
			}
			return { .Action = action::native_attempt, .Representation = representation::native_line };
		case state::native_prepared:
			return { .Action = action::native_attempt, .Representation = representation::native_line };
		case state::native_possibly_transmitted:
			return { .Action = action::reconcile_native, .Representation = representation::native_line };
		case state::native_definitively_unsupported:
		case state::unsupported_evidence_admitted:
		case state::unresolvable_line_admitted:
		case state::fallback_prepared:
			return fallback(aObservation, action::fallback_attempt);
		case state::fallback_possibly_transmitted:
			return { .Action = action::reconcile_fallback, .Representation = representation::file_comment_with_permalink };
		case state::succeeded_native:
			return { .Action = action::already_succeeded, .Representation = representation::native_line };
		case state::succeeded_fallback:
			return { .Action = action::already_succeeded, .Representation = representation::file_comment_with_permalink };
		}
		return { .Diagnostic = review_publication_diagnostic::invalid_observation };
	}

	auto advance_review_publication(const review_publication_observation& aObservation, const review_publication_event aEvent) noexcept -> std::optional<review_publication_state>
	{
		if (!valid_observation(aObservation) || !aObservation.Eligible) {
			return std::nullopt;
		}
		using state        = review_publication_state;
		using event        = review_publication_event;
		const auto current = aObservation.State;
		const auto native  = current == state::native_prepared || current == state::native_possibly_transmitted;
		const auto file    = current == state::fallback_prepared || current == state::fallback_possibly_transmitted;
		// [[BPR-1735]]. A rejected publication starts again as from the initial state.
		const auto fresh = current == state::initial || current == state::rejected;
		switch (aEvent) {
		case event::prepare_native:
			if (fresh && plan_review_publication(aObservation).Action == review_publication_action::native_attempt) {
				return state::native_prepared;
			}
			break;
		case event::admit_unsupported_evidence:
			if (fresh && plan_review_publication(aObservation).Action == review_publication_action::admit_unsupported_evidence) {
				return state::unsupported_evidence_admitted;
			}
			break;
		case event::native_transmission_witness:
			if (current == state::native_prepared) {
				return state::native_possibly_transmitted;
			}
			break;
		case event::native_created:
			if (native) {
				return state::succeeded_native;
			}
			break;
		case event::native_definitively_unsupported:
			if (native) {
				return state::native_definitively_unsupported;
			}
			break;
		case event::admit_unresolvable_line:
			if (fresh && plan_review_publication(aObservation).Action == review_publication_action::admit_unresolvable_line) {
				return state::unresolvable_line_admitted;
			}
			break;
		case event::prepare_fallback:
			if ((current == state::native_definitively_unsupported || current == state::unsupported_evidence_admitted || current == state::unresolvable_line_admitted)
				&& plan_review_publication(aObservation).Action == review_publication_action::fallback_attempt) {
				return state::fallback_prepared;
			}
			break;
		case event::fallback_transmission_witness:
			if (current == state::fallback_prepared) {
				return state::fallback_possibly_transmitted;
			}
			break;
		case event::fallback_created:
			if (file) {
				return state::succeeded_fallback;
			}
			break;
		case event::definitively_rejected:
			if (native || file) {
				return state::rejected;
			}
			break;
		}
		return std::nullopt;
	}

	auto classify_review_body(const std::string_view aBody) noexcept -> review_body_compatibility
	{
		// Conservative recognition includes longer fences and whitespace in the
		// info string. False positives refuse degradation without changing native writes.
		for (auto offset = std::size_t{ 0 }; offset < aBody.size(); ++offset) {
			const auto marker = aBody[offset];
			if (marker != '`' && marker != '~') {
				continue;
			}
			auto end = offset;
			while (end < aBody.size() && aBody[end] == marker) {
				++end;
			}
			if (end - offset >= 3U) {
				auto info = end;
				while (info < aBody.size() && (aBody[info] == ' ' || aBody[info] == '\t')) {
					++info;
				}
				if (aBody.substr(info).starts_with("suggestion")) {
					return review_body_compatibility::requires_line_subject;
				}
			}
			offset = end - 1U;
		}
		return review_body_compatibility::portable;
	}

	auto valid(const review_publication_post& aPost) noexcept -> bool
	{
		if (aPost.RecordedUtcMs < 0) {
			return false;
		}
		switch (aPost.Answer) {
		case review_publication_answer::success:
			return aPost.StatusClass == 2U;
		case review_publication_answer::error_status:
			return aPost.StatusClass >= 3U && aPost.StatusClass <= 5U;
		case review_publication_answer::kept_before_answers:
		case review_publication_answer::no_answer:
			return aPost.StatusClass == 0U;
		}
		return false;
	}

	auto publication_definitely_unpublished(const std::optional<review_publication_post>& aPost, const std::int64_t aNowUtcMs) noexcept -> bool
	{
		if (!aPost || !valid(*aPost)) {
			return false;
		}
		if (aPost->Answer == review_publication_answer::error_status) {
			return aPost->StatusClass == 4U;
		}
		return aPost->Answer == review_publication_answer::kept_before_answers && aNowUtcMs >= aPost->RecordedUtcMs
			&& aNowUtcMs - aPost->RecordedUtcMs >= review_publication_settle_delay_ms;
	}
}
