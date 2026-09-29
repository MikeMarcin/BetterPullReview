#include "bpr/application/agent_finding_delivery.hpp"

#include <utility>

namespace bpr::application {
	auto deliver_agent_findings(
		const agent_finding_payload&                       aPayload,
		const core::agent_session&                         aSession,
		const core::agent_finding_snapshot&                aSnapshot,
		const std::optional<core::agent_revision_binding>& aObserved)
		-> agent_finding_delivery
	{
		auto delivery = agent_finding_delivery{
			.Session = aSession.Id,
			// The session's own binding, unconditionally. `aObserved` is read
			// only to say how far the review has moved since; it is never
			// copied here.
			.Binding   = aSession.Binding,
			.Staleness = aObserved.has_value()
				? core::observe_agent_session(aSession, *aObserved).Staleness
				: core::agent_session_staleness::different_review,
		};
		if (aPayload.Session != aSession.Id) {
			return delivery;
		}
		delivery.Findings.reserve(aPayload.Findings.size());
		for (const auto& finding : aPayload.Findings) {
			delivery.Findings.push_back({
				.Finding = finding,
				.Verdict = core::validate_agent_finding(finding, aSnapshot),
			});
		}
		return delivery;
	}

	auto promoted_note(
		const local_note_id                          aNote,
		std::optional<local_note_attachment>         aAttachment,
		const core::agent_finding_promotion_request& aRequest)
		-> std::expected<local_note_record, agent_note_promotion_failure>
	{
		if (aRequest.Choice != core::agent_finding_promotion_choice::private_note) {
			return std::unexpected(
				agent_note_promotion_failure{ .WrongArtifact = true });
		}
		const auto promotion = core::promote_agent_finding(aRequest);
		if (!promotion.promoted() || !promotion.Note.has_value()) {
			return std::unexpected(
				agent_note_promotion_failure{ .Refusal = promotion.Refusal });
		}
		auto record = local_note_record{
			.Id         = aNote,
			.Attachment = std::move(aAttachment),
			.Note       = *promotion.Note,
		};
		if (!valid(record)) {
			return std::unexpected(agent_note_promotion_failure{
				.Store = ledger_store_failure::invalid_record,
			});
		}
		return record;
	}

	auto store_promoted_finding(
		review_ledger_store_port&                    aStore,
		const local_note_id                          aNote,
		std::optional<local_note_attachment>         aAttachment,
		const core::agent_finding_promotion_request& aRequest)
		-> std::expected<local_note_record, agent_note_promotion_failure>
	{
		const auto record = promoted_note(aNote, std::move(aAttachment), aRequest);
		if (!record) {
			return std::unexpected(record.error());
		}
		const auto stored = aStore.store_note(*record);
		if (!stored) {
			return std::unexpected(
				agent_note_promotion_failure{ .Store = stored.error().Reason });
		}
		return *stored;
	}
} // namespace bpr::application
