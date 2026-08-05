#include "bpr/core/defect_workflow.hpp"

namespace bpr::core {
	auto name(const defect_severity aSeverity) noexcept -> std::string_view
	{
		switch (aSeverity) {
		case defect_severity::trivial:
			return "trivial";
		case defect_severity::minor:
			return "minor";
		case defect_severity::major:
			return "major";
		case defect_severity::critical:
			return "critical";
		}
		return "minor";
	}

	auto name(const defect_category aCategory) noexcept -> std::string_view
	{
		switch (aCategory) {
		case defect_category::correctness:
			return "correctness";
		case defect_category::security:
			return "security";
		case defect_category::performance:
			return "performance";
		case defect_category::maintainability:
			return "maintainability";
		case defect_category::style:
			return "style";
		case defect_category::documentation:
			return "documentation";
		case defect_category::other:
			return "other";
		}
		return "other";
	}

	auto name(const defect_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case defect_state::open:
			return "open";
		case defect_state::fix_claimed:
			return "fix_claimed";
		case defect_state::needs_verification:
			return "needs_verification";
		case defect_state::verified:
			return "verified";
		case defect_state::reopened:
			return "reopened";
		case defect_state::withdrawn:
			return "withdrawn";
		case defect_state::accepted_as_is:
			return "accepted_as_is";
		}
		return "open";
	}

	auto name(const defect_command_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case defect_command_kind::claim_fix:
			return "claim_fix";
		case defect_command_kind::verify:
			return "verify";
		case defect_command_kind::reopen:
			return "reopen";
		case defect_command_kind::withdraw:
			return "withdraw";
		case defect_command_kind::accept_as_is:
			return "accept_as_is";
		case defect_command_kind::reclassify:
			return "reclassify";
		}
		return "claim_fix";
	}

	auto name(const defect_rejection aRejection) noexcept -> std::string_view
	{
		switch (aRejection) {
		case defect_rejection::none:
			return "none";
		case defect_rejection::unknown_defect:
			return "unknown_defect";
		case defect_rejection::nothing_to_verify:
			return "nothing_to_verify";
		case defect_rejection::not_verified:
			return "not_verified";
		case defect_rejection::already_settled:
			return "already_settled";
		case defect_rejection::nothing_to_reclassify:
			return "nothing_to_reclassify";
		}
		return "none";
	}

	auto name(const defect_anchor_availability aAvailability) noexcept
		-> std::string_view
	{
		switch (aAvailability) {
		case defect_anchor_availability::available:
			return "available";
		case defect_anchor_availability::outdated:
			return "outdated";
		case defect_anchor_availability::thread_deleted:
			return "thread_deleted";
		}
		return "available";
	}

	auto lose_anchor(
		const defect_history&            aHistory,
		const defect_anchor_availability aAvailability) noexcept -> defect_history
	{
		auto next         = aHistory;
		next.Availability = aAvailability;
		// OriginalAnchor and Defect.State are both left exactly as they were.
		// The anchor is the record of what was complained about, and the state
		// is a judgement nobody has revisited.
		return next;
	}

	auto name(const defect_evidence_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case defect_evidence_kind::author_reply:
			return "author_reply";
		case defect_evidence_kind::nearby_code_change:
			return "nearby_code_change";
		case defect_evidence_kind::thread_resolved_externally:
			return "thread_resolved_externally";
		}
		return "author_reply";
	}

	auto observe(
		const defect_record&   aDefect,
		const defect_evidence& aEvidence) noexcept -> defect_outcome
	{
		const auto unchanged = [&aDefect]() {
			return defect_outcome{
				.Defect    = aDefect,
				.Rejection = defect_rejection::none,
				.Changed   = false,
			};
		};

		if (aEvidence.Defect != aDefect.Id) {
			return defect_outcome{
				.Defect    = aDefect,
				.Rejection = defect_rejection::unknown_defect,
				.Changed   = false,
			};
		}

		switch (aDefect.State) {
		case defect_state::open:
		case defect_state::fix_claimed:
		case defect_state::reopened:
			break;
		case defect_state::needs_verification:
		case defect_state::verified:
		case defect_state::withdrawn:
		case defect_state::accepted_as_is:
			// Nothing to do, for two different reasons that happen to share an
			// answer. A defect already awaiting the reviewer is already asking:
			// more evidence does not ask harder. A verified, withdrawn, or
			// accepted defect has had a decision taken, and evidence does not
			// undo one — reopening is a separate explicit action, and a late
			// reply overruling a reviewer would be evidence overriding the
			// person it exists to inform.
			return unchanged();
		}

		auto next  = aDefect;
		next.State = defect_state::needs_verification;
		return defect_outcome{
			.Defect    = next,
			.Rejection = defect_rejection::none,
			.Changed   = true,
		};
	}

	auto apply(
		const defect_record&  aDefect,
		const defect_command& aCommand) noexcept -> defect_outcome
	{
		const auto refuse = [&aDefect](const defect_rejection aRejection) {
			return defect_outcome{
				.Defect    = aDefect,
				.Rejection = aRejection,
				.Changed   = false,
			};
		};

		if (aCommand.Id != aDefect.Id) {
			return refuse(defect_rejection::unknown_defect);
		}

		// Withdrawn and accepted-as-is are decisions already taken. Reopening
		// one is not a lifecycle transition but a new defect: the earlier
		// decision remains part of the record rather than being overwritten.
		const auto settled = aDefect.State == defect_state::withdrawn
			|| aDefect.State == defect_state::accepted_as_is;
		if (settled) {
			return refuse(defect_rejection::already_settled);
		}

		auto next = aDefect;
		switch (aCommand.Kind) {
		case defect_command_kind::claim_fix:
			// A claim is a claim whoever makes it and however often. It never
			// reaches verified.
			next.State = defect_state::fix_claimed;
			break;
		case defect_command_kind::verify:
			// Only from a state where something has actually been claimed or
			// suggested. Verifying an open defect nobody has addressed would
			// be recording a judgement about work that has not happened.
			if (aDefect.State != defect_state::fix_claimed
				&& aDefect.State != defect_state::needs_verification) {
				return refuse(defect_rejection::nothing_to_verify);
			}
			next.State = defect_state::verified;
			break;
		case defect_command_kind::reopen:
			if (aDefect.State != defect_state::verified) {
				return refuse(defect_rejection::not_verified);
			}
			next.State = defect_state::reopened;
			break;
		case defect_command_kind::withdraw:
			next.State = defect_state::withdrawn;
			break;
		case defect_command_kind::accept_as_is:
			next.State = defect_state::accepted_as_is;
			break;
		case defect_command_kind::reclassify:
			if (!aCommand.Severity && !aCommand.Category) {
				return refuse(defect_rejection::nothing_to_reclassify);
			}
			next.Severity = aCommand.Severity.value_or(aDefect.Severity);
			next.Category = aCommand.Category.value_or(aDefect.Category);
			break;
		}

		return defect_outcome{
			.Defect    = next,
			.Rejection = defect_rejection::none,
			.Changed   = next != aDefect,
		};
	}
} // namespace bpr::core
