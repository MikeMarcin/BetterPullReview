#include "bpr/core/suggestion_applicability.hpp"

namespace bpr::core {
	auto stable_name(const suggestion_applicability aApplicability) noexcept -> std::string_view
	{
		switch (aApplicability) {
		case suggestion_applicability::no_patch:
			return "no_patch";
		case suggestion_applicability::native:
			return "native";
		case suggestion_applicability::textual_only:
			return "textual_only";
		}
		return "unknown";
	}

	auto stable_name(const suggestion_text_reason aReason) noexcept -> std::string_view
	{
		switch (aReason) {
		case suggestion_text_reason::none:
			return "none";
		case suggestion_text_reason::base_side:
			return "base_side";
		case suggestion_text_reason::file_subject:
			return "file_subject";
		case suggestion_text_reason::outdated_line:
			return "outdated_line";
		case suggestion_text_reason::unavailable_anchor:
			return "unavailable_anchor";
		case suggestion_text_reason::conversation:
			return "conversation";
		case suggestion_text_reason::inline_comment_unsupported:
			return "inline_comment_unsupported";
		case suggestion_text_reason::invalid_observation:
			return "invalid_observation";
		}
		return "unknown";
	}

	auto decide_suggestion_applicability(const std::string_view aBody, const suggestion_placement_observation& aPlacement) noexcept -> suggestion_applicability_decision
	{
		const auto body = classify_review_body(aBody);
		if (body != review_body_compatibility::requires_line_subject) {
			return { };
		}
		const auto textual = [](const suggestion_text_reason aReason) {
			return suggestion_applicability_decision{ .Applicability = suggestion_applicability::textual_only, .Reason = aReason };
		};
		if (aPlacement.Subject > suggestion_subject::conversation || aPlacement.Side > diff_side::right
			|| aPlacement.Placement > review_placement::off_hunk || aPlacement.Capability > off_hunk_creation_capability::unsupported) {
			return textual(suggestion_text_reason::invalid_observation);
		}
		switch (aPlacement.Subject) {
		case suggestion_subject::current_line:
			break;
		case suggestion_subject::file:
			return textual(suggestion_text_reason::file_subject);
		case suggestion_subject::outdated_line:
			return textual(suggestion_text_reason::outdated_line);
		case suggestion_subject::unavailable:
			return textual(suggestion_text_reason::unavailable_anchor);
		case suggestion_subject::conversation:
			return textual(suggestion_text_reason::conversation);
		}
		if (aPlacement.Side == diff_side::left) {
			return textual(suggestion_text_reason::base_side);
		}
		// The line's own publication plan, from its first step: the same
		// policy that will decide the real attempt.
		const auto plan = plan_review_publication({ .Placement         = aPlacement.Placement,
													.Capability        = aPlacement.Capability,
													.Body              = body,
													.State             = review_publication_state::initial,
													.Eligible          = true,
													.CapabilityCurrent = aPlacement.CapabilityCurrent });
		if (plan.Action == review_publication_action::native_attempt && plan.Representation == review_publication_representation::native_line) {
			return { .Applicability = suggestion_applicability::native };
		}
		if (plan.Diagnostic == review_publication_diagnostic::incompatible_body) {
			return textual(suggestion_text_reason::inline_comment_unsupported);
		}
		return textual(suggestion_text_reason::invalid_observation);
	}
}
