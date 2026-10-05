#include "bpr/core/feedback_draft_edit.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace bpr::core {
	namespace {
		/// A subject the probe appends, so that a body which is only a header
		/// (or nothing) reads as one with a subject.
		constexpr char probe_subject = 'x';

		auto blank(std::string_view aBody) -> bool
		{
			if (aBody.starts_with("\xEF\xBB\xBF")) {
				aBody.remove_prefix(3U);
			}
			return std::ranges::all_of(aBody, [](char aByte) { return aByte == ' ' || aByte == '\t' || aByte == '\r' || aByte == '\n'; });
		}

		/// The body is blank, or exactly a header whose subject has not been
		/// written yet: with a subject appended it reads as a header ending
		/// where the body ends.
		auto incomplete(std::string_view aBody, const feedback_annotation& aCurrent, const feedback_annotation& aProbe) -> bool
		{
			if (aCurrent.Status == feedback_prefix_status::plain) {
				return blank(aBody);
			}
			const bool emptySubject = aCurrent.Status == feedback_prefix_status::malformed && std::ranges::find(aCurrent.Diagnostics, feedback_diagnostic::empty_subject) != aCurrent.Diagnostics.end();
			const bool header       = aProbe.Status == feedback_prefix_status::recognized || aProbe.Status == feedback_prefix_status::unknown_label;
			return emptySubject && header && aProbe.Subject && aProbe.Subject->Begin == aBody.size();
		}

		auto refused(feedback_draft_edit_refusal aRefusal, std::string_view aBody) -> feedback_draft_edit_plan
		{
			return { .Refusal = aRefusal, .Decision = { .Annotation = recognize_feedback_prefix(aBody) } };
		}

		auto refusal_of(const feedback_header_edit_decision& aDecision) noexcept -> feedback_draft_edit_refusal
		{
			switch (aDecision.Refusal) {
			case feedback_edit_refusal::none:
				return feedback_draft_edit_refusal::none;
			case feedback_edit_refusal::stale_revision:
				return feedback_draft_edit_refusal::stale_draft;
			case feedback_edit_refusal::invalid_edit:
			case feedback_edit_refusal::unsupported_header:
			case feedback_edit_refusal::restructuring_required:
			case feedback_edit_refusal::body_limit:
				break;
			}
			return feedback_draft_edit_refusal::header;
		}
	}

	auto stable_name(feedback_draft_edit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case feedback_draft_edit_refusal::none:
			return "none";
		case feedback_draft_edit_refusal::stale_draft:
			return "stale_draft";
		case feedback_draft_edit_refusal::not_editable:
			return "not_editable";
		case feedback_draft_edit_refusal::header:
			return "header";
		}
		return "unknown";
	}

	auto decide_feedback_composition_edit(const feedback_header_edit_request& aRequest) -> feedback_header_edit_decision
	{
		if (aRequest.Segment.Managed) {
			return decide_feedback_header_edit(aRequest);
		}
		const auto current = recognize_feedback_prefix(aRequest.Body);
		if (current.BodyDigest != aRequest.ExpectedDigest || aRequest.ExpectedParserRevision != feedback_parser_revision) {
			return decide_feedback_header_edit(aRequest);
		}
		auto probe = std::string{ };
		probe.reserve(aRequest.Body.size() + 1U);
		probe.append(aRequest.Body).push_back(probe_subject);
		const auto probed = recognize_feedback_prefix(probe);
		if (!incomplete(aRequest.Body, current, probed)) {
			return decide_feedback_header_edit(aRequest);
		}
		auto decision = decide_feedback_header_edit({ .Body = probe, .Segment = { }, .ExpectedDigest = probed.BodyDigest, .Edit = aRequest.Edit });
		if (!decision.Changed) {
			return { .Refusal = decision.Refusal, .Segment = aRequest.Segment, .Annotation = current };
		}
		// The probe subject is the last byte, after every replaced span.
		decision.Body.pop_back();
		decision.Annotation = recognize_feedback_prefix(decision.Body);
		return decision;
	}

	auto feedback_draft_basis_of(const revisioned_review_draft& aDraft) -> feedback_draft_basis
	{
		return { .Revision = aDraft.Revision, .Digest = recognize_feedback_prefix(aDraft.Draft.Body).BodyDigest };
	}

	auto plan_feedback_draft_edit(const revisioned_review_draft& aCurrent, const feedback_draft_basis& aShown, const feedback_header_edit& aEdit) -> feedback_draft_edit_plan
	{
		const auto& draft = aCurrent.Draft;
		if (aShown.Revision != aCurrent.Revision) {
			return refused(feedback_draft_edit_refusal::stale_draft, draft.Body);
		}
		if (draft.State != review_draft_state::drafting) {
			return refused(feedback_draft_edit_refusal::not_editable, draft.Body);
		}
		auto plan    = feedback_draft_edit_plan{ .Decision = decide_feedback_composition_edit({ .Body = draft.Body, .Segment = { }, .ExpectedDigest = aShown.Digest, .ExpectedParserRevision = aShown.ParserRevision, .Edit = aEdit }) };
		plan.Refusal = refusal_of(plan.Decision);
		if (plan.Decision.Changed) {
			plan.Command = revisioned_review_draft_command{
				.Command          = { .Id = draft.Id, .Kind = review_draft_command_kind::edit, .Body = plan.Decision.Body },
				.ExpectedRevision = aCurrent.Revision,
				.Scope            = aCurrent.Scope,
			};
		}
		return plan;
	}

	auto feedback_tour_stop_basis(const review_tour& aTour, tour_stop_id aStop) -> std::optional<feedback_draft_basis>
	{
		const auto found = std::ranges::find(aTour.Stops, aStop, &tour_stop::Id);
		if (found == aTour.Stops.end()) {
			return std::nullopt;
		}
		return feedback_draft_basis{ .Revision = aTour.Revision, .Digest = recognize_feedback_prefix(found->Body).BodyDigest };
	}

	auto plan_feedback_tour_stop_edit(const review_tour& aCurrent, tour_stop_id aStop, const feedback_draft_basis& aShown, const feedback_header_edit& aEdit) -> feedback_tour_stop_edit_plan
	{
		const auto found = std::ranges::find(aCurrent.Stops, aStop, &tour_stop::Id);
		if (found == aCurrent.Stops.end()) {
			return { .Refusal = feedback_draft_edit_refusal::stale_draft };
		}
		const auto& body = found->Body;
		if (aShown.Revision != aCurrent.Revision) {
			return { .Refusal = feedback_draft_edit_refusal::stale_draft, .Decision = { .Annotation = recognize_feedback_prefix(body) } };
		}
		if (aCurrent.ReadOnly) {
			return { .Refusal = feedback_draft_edit_refusal::not_editable, .Decision = { .Annotation = recognize_feedback_prefix(body) } };
		}
		auto plan    = feedback_tour_stop_edit_plan{ .Decision = decide_feedback_composition_edit({ .Body = body, .Segment = { }, .ExpectedDigest = aShown.Digest, .ExpectedParserRevision = aShown.ParserRevision, .Edit = aEdit }) };
		plan.Refusal = refusal_of(plan.Decision);
		if (plan.Decision.Changed) {
			auto value   = *found;
			value.Body   = plan.Decision.Body;
			plan.Command = tour_command{ .Draft = aCurrent.Id, .Owner = aCurrent.Owner, .ExpectedRevision = aCurrent.Revision, .Kind = tour_command_kind::edit_stop, .Stop = aStop, .Value = std::move(value) };
		}
		return plan;
	}
}
