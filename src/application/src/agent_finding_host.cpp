#include "bpr/application/agent_finding_host.hpp"

#include <utility>

namespace bpr::application {
	namespace {
		/// \brief The promotion choice one action asks the core for.
		///
		/// \details Only the three promoting actions have one. The other three
		/// never reach here, because `act` refuses them as `not_a_promotion`
		/// before a request is built: a default choice for `dismiss` would be a
		/// promotion nobody asked for.
		[[nodiscard]] constexpr auto promotion_choice(
			const agent_finding_action aAction) noexcept
			-> core::agent_finding_promotion_choice
		{
			switch (aAction) {
			case agent_finding_action::convert_to_draft_comment:
				return core::agent_finding_promotion_choice::draft_comment;
			case agent_finding_action::convert_to_draft_defect:
				return core::agent_finding_promotion_choice::defect;
			case agent_finding_action::add_to_private_notes:
			case agent_finding_action::open_evidence:
			case agent_finding_action::ask_follow_up:
			case agent_finding_action::dismiss:
				break;
			}
			return core::agent_finding_promotion_choice::private_note;
		}

		[[nodiscard]] constexpr auto promotes(
			const agent_finding_action aAction) noexcept -> bool
		{
			return aAction == agent_finding_action::convert_to_draft_comment
				|| aAction == agent_finding_action::convert_to_draft_defect
				|| aAction == agent_finding_action::add_to_private_notes;
		}
	} // namespace

	auto name(const agent_finding_action aAction) noexcept -> std::string_view
	{
		switch (aAction) {
		case agent_finding_action::open_evidence:
			return "open_evidence";
		case agent_finding_action::ask_follow_up:
			return "ask_follow_up";
		case agent_finding_action::convert_to_draft_comment:
			return "convert_to_draft_comment";
		case agent_finding_action::convert_to_draft_defect:
			return "convert_to_draft_defect";
		case agent_finding_action::add_to_private_notes:
			return "add_to_private_notes";
		case agent_finding_action::dismiss:
			return "dismiss";
		}
		return { };
	}

	auto name(const agent_finding_action_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_finding_action_refusal::none:
			return "none";
		case agent_finding_action_refusal::nothing_delivered:
			return "nothing_delivered";
		case agent_finding_action_refusal::other_session:
			return "other_session";
		case agent_finding_action_refusal::unknown_finding:
			return "unknown_finding";
		case agent_finding_action_refusal::not_a_promotion:
			return "not_a_promotion";
		case agent_finding_action_refusal::note_not_stored:
			return "note_not_stored";
		}
		return { };
	}

	agent_finding_host::agent_finding_host(
		promoted_note_store            aStore,
		agent_finding_projection_port& aProjection)
		: Store{ std::move(aStore) }
		, Projection{ aProjection }
	{
	}

	void agent_finding_host::deliver(agent_finding_delivery aDelivery)
	{
		++Statistics.Deliveries;
		Statistics.DeliveredFindings += aDelivery.Findings.size();
		Delivered = std::move(aDelivery);
		Projection.present(*Delivered);
	}

	void agent_finding_host::clear()
	{
		Delivered.reset();
		Projection.clear();
	}

	auto agent_finding_host::locate(
		const agent_finding_action_request& aRequest) const
		-> std::optional<agent_delivered_finding>
	{
		if (!Delivered.has_value() || Delivered->Session != aRequest.Session
			|| aRequest.Ordinal >= Delivered->Findings.size()) {
			return std::nullopt;
		}
		return Delivered->Findings[aRequest.Ordinal];
	}

	void agent_finding_host::act(const agent_finding_action_request& aRequest)
	{
		++Statistics.Actions;
		auto outcome = agent_finding_outcome{
			.Session          = aRequest.Session,
			.Ordinal          = aRequest.Ordinal,
			.Action           = aRequest.Action,
			.ActionGeneration = Statistics.Actions,
			.Refusal          = agent_finding_action_refusal::nothing_delivered,
		};
		if (!Delivered.has_value()) {
			outcome.Refusal = agent_finding_action_refusal::nothing_delivered;
		} else if (Delivered->Session != aRequest.Session) {
			outcome.Refusal = agent_finding_action_refusal::other_session;
		} else if (aRequest.Ordinal >= Delivered->Findings.size()) {
			outcome.Refusal = agent_finding_action_refusal::unknown_finding;
		} else {
			outcome.Subject = locate(aRequest);
			if (!promotes(aRequest.Action)) {
				// Three of the six produce no artifact, and saying so is not
				// the same as saying nothing happened. `dismiss` is counted
				// separately because it is the reviewer's decision rather than
				// an inaction.
				outcome.Refusal = agent_finding_action_refusal::not_a_promotion;
				if (aRequest.Action == agent_finding_action::dismiss) {
					++Statistics.Dismissals;
				}
			} else {
				const auto request = core::agent_finding_promotion_request{
					.Finding  = outcome.Subject->Finding,
					.Verdict  = outcome.Subject->Verdict,
					.Choice   = promotion_choice(aRequest.Action),
					.Standing = aRequest.Standing,
					.Draft    = aRequest.Draft,
					.Defect   = aRequest.Defect,
				};
				outcome.Refusal = agent_finding_action_refusal::none;
				if (aRequest.Action
					== agent_finding_action::add_to_private_notes) {
					const auto note = promoted_note(aRequest.Note, aRequest.Attachment, request);
					if (!note) {
						outcome.NoteFailure = note.error();
						outcome.Promotion   = note.error().Refusal;
						if (note.error().Store.has_value()) {
							outcome.Refusal
								= agent_finding_action_refusal::note_not_stored;
						}
					} else {
						// [[BPR-1211]]. The note is written off the decision
						// lane and presented once the write answers.
						const auto admitted = Store && Store(*note, [this, outcome](std::expected<local_note_record, local_note_failure> aStored) mutable {
												  if (aStored.has_value()) {
													  outcome.Note = std::move(*aStored);
													  ++Statistics.StoredNotes;
												  } else {
													  outcome.NoteFailure = agent_note_promotion_failure{ .Store = aStored.error().Reason };
													  outcome.Refusal     = agent_finding_action_refusal::note_not_stored;
												  }
												  finish(outcome);
											  });
						if (admitted) {
							return;
						}
						outcome.Refusal = agent_finding_action_refusal::note_not_stored;
					}
				} else {
					const auto promotion = core::promote_agent_finding(request);
					outcome.Promotion    = promotion.Refusal;
					if (promotion.promoted()) {
						outcome.Draft  = promotion.Draft;
						outcome.Defect = promotion.Defect;
						if (promotion.Draft.has_value()) {
							++Statistics.DraftCommands;
						}
						if (promotion.Defect.has_value()) {
							++Statistics.DefectCommands;
						}
					}
				}
			}
		}
		finish(outcome);
	}

	void agent_finding_host::finish(const agent_finding_outcome& aOutcome)
	{
		if (!aOutcome.promoted()) {
			++Statistics.Refusals;
		}
		// Presented unconditionally. A refused conversion the reviewer is
		// never shown is indistinguishable from a queue that silently swallowed
		// their gesture, and RFC-BPR-0012's whole point is that the reviewer
		// stays the one who decides.
		Projection.present(aOutcome);
		++Statistics.PresentedOutcomes;
	}

	auto agent_finding_host::delivered() const noexcept
		-> const std::optional<agent_finding_delivery>&
	{
		return Delivered;
	}

	auto agent_finding_host::statistics() const noexcept
		-> agent_finding_host_statistics
	{
		return Statistics;
	}
} // namespace bpr::application
