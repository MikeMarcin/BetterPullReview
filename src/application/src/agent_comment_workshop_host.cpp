#include "bpr/application/agent_comment_workshop_host.hpp"

namespace bpr::application {
	auto name(const agent_comment_workshop_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_comment_workshop_refusal::none:
			return "none";
		case agent_comment_workshop_refusal::different_draft:
			return "different_draft";
		case agent_comment_workshop_refusal::not_an_edit:
			return "not_an_edit";
		case agent_comment_workshop_refusal::empty_revision:
			return "empty_revision";
		case agent_comment_workshop_refusal::unchanged:
			return "unchanged";
		}
		return { };
	}

	agent_comment_workshop_host::agent_comment_workshop_host(
		agent_comment_workshop_projection_port& aProjection)
		: Projection{ aProjection }
	{
	}

	auto agent_comment_workshop_host::propose(
		const core::review_comment_draft&             aDraft,
		const core::review_draft_command&             aCommand,
		const std::optional<core::workshop_operation> aOperation)
		-> agent_comment_workshop_refusal
	{
		// Identity before content. A revision meant for another draft is not
		// made to fit this one, and the reason is reported rather than the
		// nearest legal thing being done instead.
		const auto refusal = aCommand.Id != aDraft.Id
			? agent_comment_workshop_refusal::different_draft
			: aCommand.Kind != core::review_draft_command_kind::edit
			? agent_comment_workshop_refusal::not_an_edit
			: aCommand.Body.empty()
			? agent_comment_workshop_refusal::empty_revision
			: aCommand.Body == aDraft.Body
			? agent_comment_workshop_refusal::unchanged
			: agent_comment_workshop_refusal::none;
		if (refusal != agent_comment_workshop_refusal::none) {
			++Statistics.Refusals;
			Projection.refuse(refusal);
			return refusal;
		}
		++Statistics.Proposals;
		Projection.present(agent_comment_workshop_proposal{
			.Draft    = aDraft.Id,
			.Original = aDraft.Body,
			.Proposed = aCommand.Body,
			// The operation is carried, not consulted: it says why this after
			// differs from this before, and an undeclared one is dropped here
			// rather than presented as a name nobody can read.
			.Operation = aOperation && core::declared(*aOperation)
				? aOperation
				: std::nullopt,
			// [[BPR-1091]]. Read, not enforced: a candidate that changes the
			// label, drops the question or rewrites the patch is still offered,
			// and the change is said beside it so it is never accepted unseen.
			.Change = core::compare_workshop_candidate(aDraft.Body, aCommand.Body),
		});
		return agent_comment_workshop_refusal::none;
	}

	auto agent_comment_workshop_host::statistics() const noexcept
		-> agent_comment_workshop_statistics
	{
		return Statistics;
	}
} // namespace bpr::application
