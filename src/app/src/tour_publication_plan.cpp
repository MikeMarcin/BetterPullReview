#include "tour_publication_plan.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "review_comment_permalink.hpp"
#include <algorithm>
namespace bpr::app {
	auto tour_publication_failure_text(const application::staged_publication_result& aResult) -> std::string
	{
		using failure = application::review_mutation_failure;
		using step    = application::staged_publication_step;
		if (!aResult.Failure || !aResult.Diagnosis || aResult.State == application::staged_publication_state::outcome_unknown || (aResult.Record && aResult.Record->Published)) {
			return { };
		}
		std::string result;
		switch (aResult.Diagnosis->Step) {
		case step::authority:           result = "Publication could not start"; break;
		case step::plan_validation:     result = "The captured publication plan is invalid"; break;
		case step::recovery_open:       result = "Publication recovery is unavailable in this profile"; break;
		case step::lease:               result = "The publishing connection is no longer active"; break;
		case step::configuration:       result = "The publishing connection configuration is invalid"; break;
		case step::binding:             result = "The publication does not match this account and pull request"; break;
		case step::journal_read:        result = "Could not read publication recovery in this profile"; break;
		case step::journal_save:        result = "Could not save publication recovery in this profile"; break;
		case step::credential_scope:    result = "Could not open the publishing credential operation"; break;
		case step::identity_check:      result = "Could not confirm the publishing account and pull request revision"; break;
		case step::request_preparation: result = "Could not prepare this publication operation"; break;
		case step::baseline_read:
			result = aResult.Diagnosis->Operation == application::staged_review_operation::list_issue_comments ? "Could not read existing pull request comments" : "Could not read existing pull request reviews";
			break;
		}
		if (const auto status = aResult.Diagnosis->HttpStatus; status && *status >= 100U && *status <= 599U) {
			result += " (HTTP " + std::to_string(*status) + ")";
		}
		result += ". ";
		switch (*aResult.Failure) {
		case failure::refused_by_server:   result += "The server refused the request. Check account access and server limits, then Preview again. "; break;
		case failure::target_unavailable:  result += "The server did not expose this target. Check account access, refresh the pull request, then Preview again. "; break;
		case failure::malformed_response:  result += "The response could not be validated. Check the connection and server compatibility, then Preview again. "; break;
		case failure::undesignated_target:
		case failure::target_moved:
		case failure::stale_head:          result += "Refresh the pull request and confirm the current account before a new preview. "; break;
		case failure::unsupported:         result += "This connection does not support the operation. Keep the draft locally or use a supported connection. "; break;
		case failure::cancelled:           result += "The check was cancelled. Preview again to retry. "; break;
		default:                           result += (aResult.Diagnosis->Step == step::journal_read || aResult.Diagnosis->Step == step::journal_save || aResult.Diagnosis->Step == step::recovery_open) ? "Check local profile storage, then Preview again. " : "Check the connection, then Preview again. "; break;
		}
		return result + "This attempt sent nothing; your private draft is kept.";
	}
	auto prepare_tour_plan(const core::review_tour& aDraft, const application::review_submission_live_binding& aBinding, std::string_view aDescription, tour_publication_options aOptions, bool aQualified) -> tour_plan_preparation
	{
		tour_plan_preparation                      result;
		const auto&                                draft         = aDraft;
		const bool                                 compatibility = aOptions.Compatibility;
		std::vector<core::tour_destination_plan>   decisions;
		std::vector<application::staged_tour_stop> stops;
		for (const auto& stop : draft.Stops) {
			const auto source = core::publication_source(stop);
			if (!source) {
				decisions.push_back({ });
				continue;
			}
			decisions.push_back(core::plan_tour_destination(core::publication_observation(*source, draft.ObservedHead.value_or(draft.Head), draft.Base, stop.Body, compatibility, aQualified)));
			const auto link = adapters::github::detail::compile_tour_permalink({ std::string{ aBinding.Target.Hostname.value() }, aBinding.Target.Repository }, *source);
			stops.push_back({ stop.Id, *source, stop.Title, stop.Body, link.value_or(std::string{ }) });
		}
		const auto readiness = draft.Remote ? core::assess_tour_update(draft, decisions, compatibility, aQualified ? core::tour_operation_support::supported : core::tour_operation_support::unknown) : core::assess_tour_publication(draft, decisions, compatibility);
		if (readiness != core::tour_publication_readiness::single_review && readiness != core::tour_publication_readiness::compatibility_comments) {
			result.Status = readiness == core::tour_publication_readiness::incomplete ? "Complete Problem, Solution and every stop with an exact available source before publishing." : readiness == core::tour_publication_readiness::compatibility_choice ? "This route needs visible compatibility comments. Keep one review by relocating destinations, or explicitly choose compatibility comments."
																																																															: "Publication is unavailable for these destinations. Move each stop to a line GitHub can comment on, or keep preparing locally.";
			return result;
		}
		// Filled member by member: GCC 15 at -O3 reads the nested designated
		// aggregate as leaving the proposal's body uninitialized.
		application::review_submission_request submission{ };
		submission.Proposal.Draft         = core::review_submission_draft_id{ draft.Id.value() };
		submission.Proposal.Attempt       = core::review_submission_attempt_id{ draft.Revision };
		submission.Proposal.Target        = aBinding.Target.Semantic;
		submission.Proposal.ActingAccount = aBinding.ActingIdentity.Account;
		// [[BPR-1787]]. The brief is published in one place. A new tour puts it
		// where the author chose; a published tour keeps it where it is.
		const bool briefInDescription   = draft.Remote ? core::tour_brief_in_description(draft) : aOptions.Description;
		submission.Proposal.Body        = core::tour_comment_lead(draft, draft.Remote ? !core::tour_brief_in_comment(draft) : briefInDescription);
		submission.Proposal.Disposition = core::review_disposition::comment;
		submission.IdempotencyKey       = "tour-" + std::to_string(draft.Owner.Profile.value()) + "-" + std::to_string(draft.Id.value()) + "-" + std::to_string(draft.Revision);
		submission.LiveBinding          = aBinding;
		application::staged_review_plan value{ .Submission = std::move(submission), .Owner = draft.Owner, .Generation = draft.Revision, .Stops = std::move(stops), .Compatibility = compatibility };
		if (draft.Remote) {
			value.Update = draft;
			std::erase_if(value.Stops, [&](const auto& aStop) {const auto found=std::ranges::find(draft.Stops,aStop.Id,&core::tour_stop::Id);return found!=draft.Stops.end() && !core::needs_tour_destination(*found); });
		}
		if (briefInDescription) {
			const auto desired = core::propose_tour_description(draft, aDescription);
			if (!desired) {
				result.Status = draft.Remote ? "Map Problem and Solution in the current website description before updating it. Unrelated template text is retained."
											 : "The description has a Problem or Solution heading that cannot be matched. Give it exactly one of each, or turn off Update PR description to keep the brief in the tour comment.";
				return result;
			}
			if (*desired != aDescription || !draft.Remote) {
				value.Description = application::staged_description_edit{ std::string{ aDescription }, *desired, !draft.Remote };
			}
		}
		if (!application::valid(value)) {
			result.Status = "The publication exceeds an operation limit or has an invalid destination. Edit the draft and preview again.";
			return result;
		}
		if (value.Update && value.Stops.empty()) {
			const auto bodies = application::staged_update_bodies({ .Plan = value });
			if (!bodies) {
				result.Status = "The website layout needs explicit mapping before this update.";
				return result;
			}
			if (bodies->Edits.empty() && !value.Description) {
				result.Status = "No published changes. Existing comments and discussions are unchanged.";
				return result;
			}
		}
		result.Plan   = std::move(value);
		result.Status = draft.Remote ? "Preview ready: non-atomic update; new destinations first, affected comments next, original index last. Concurrent website changes may conflict." : compatibility ? "Preview ready: visible compatibility comments, then index and navigation; non-atomic."
																																																		 : "Preview ready: one pending review, verified navigation, then one COMMENT submission.";
		return result;
	}
	auto tour_plan_markdown(const application::staged_review_plan& aPlan) -> std::string
	{
		std::string result = "### " + std::string{ aPlan.Update ? "Update existing tour — non-atomic" : aPlan.Compatibility ? "Compatibility comments — non-atomic"
																															: "One COMMENT review" }
			+ "\n\nHost: " + std::string{ aPlan.Submission.LiveBinding->Target.Hostname.value() } + " · Repository: " + aPlan.Submission.LiveBinding->Target.Repository.Owner + "/" + aPlan.Submission.LiveBinding->Target.Repository.Name + " · Actor: " + aPlan.Submission.LiveBinding->ActingIdentity.Login + " · PR #" + std::to_string(aPlan.Submission.LiveBinding->Target.Number.value()) + "\n\nHead: `" + std::string{ core::canonical_hex(aPlan.Submission.LiveBinding->Target.Semantic.ExpectedHead).view() } + "`\n\n" + aPlan.Submission.Proposal.Body;
		for (const auto& stop : aPlan.Stops) {
			result += "\n\n### " + stop.Title + "\n\n" + stop.SourceLink + "\n\n" + stop.Narrative;
		}
		// [[BPR-1787]]. The preview says where the brief goes.
		if (!aPlan.Update) {
			result += aPlan.Submission.Proposal.Body == core::tour_brief_in_description_note ? "\n\nProblem and Solution go in the PR description, not the tour comment." : "\n\nProblem and Solution go in the tour comment.";
		} else {
			const bool comment = core::tour_brief_in_comment(*aPlan.Update), description = core::tour_brief_in_description(*aPlan.Update);
			result += comment && description ? "\n\nProblem and Solution stay in the tour comment and the PR description, where they were published." : description ? "\n\nProblem and Solution stay in the PR description, where they were published."
																																									: "\n\nProblem and Solution stay in the tour comment, where they were published.";
		}
		result += aPlan.Description ? "\n\n### Separate optional description edit\n\nAfter the tour succeeds, replace the description with:\n\n" + aPlan.Description->Desired + (aPlan.Description->AppendIndexLink ? "\n\nThe returned tour link will be placed at the top." : "") : "\n\nThe PR description will not be edited.";
		if (aPlan.Update) {
			result += "\n\n### Desired route (original discussion identities retained)";
			for (const auto& stop : aPlan.Update->Stops) {
				result += "\n\n### " + stop.Title + "\n\n" + (stop.Remote ? stop.Remote->WebUrl : "New returned destination") + "\n\n" + stop.Body;
			}
			for (const auto& stop : aPlan.Update->Retired) {
				result += "\n\nRemove from route, preserve discussion: " + stop.Title + " " + (stop.Remote ? stop.Remote->WebUrl : "");
			}
			result += "\n\nNavigation uses the returned destination URLs; the original index is updated last.";
			const auto bodies = application::staged_update_bodies({ .Plan = aPlan });
			if (bodies) {
				for (const auto& edit : bodies->Edits) {
					result += "\n\n### Update " + edit.Remote.WebUrl + "\n\n" + edit.Desired;
				}
			}
		}
		return result;
	}
}
