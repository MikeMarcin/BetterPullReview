#include "tour_publication_plan.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "review_comment_permalink.hpp"
#include <algorithm>
namespace bpr::app {
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
																																																															: "Publication is unavailable for these destinations on this host and transport. Keep preparing locally; operation qualification is still required.";
			return result;
		}
		// Filled member by member: GCC 15 at -O3 reads the nested designated
		// aggregate as leaving the proposal's body uninitialized.
		application::review_submission_request submission{ };
		submission.Proposal.Draft         = core::review_submission_draft_id{ draft.Id.value() };
		submission.Proposal.Attempt       = core::review_submission_attempt_id{ draft.Revision };
		submission.Proposal.Target        = aBinding.Target.Semantic;
		submission.Proposal.ActingAccount = aBinding.ActingIdentity.Account;
		submission.Proposal.Body          = "## Problem\n\n" + draft.Problem + "\n\n## Solution\n\n" + draft.Solution;
		submission.Proposal.Disposition   = core::review_disposition::comment;
		submission.IdempotencyKey         = "tour-" + std::to_string(draft.Owner.Profile.value()) + "-" + std::to_string(draft.Id.value()) + "-" + std::to_string(draft.Revision);
		submission.LiveBinding            = aBinding;
		application::staged_review_plan value{ .Submission = std::move(submission), .Owner = draft.Owner, .Generation = draft.Revision, .Stops = std::move(stops), .Compatibility = compatibility };
		if (draft.Remote) {
			value.Update = draft;
			std::erase_if(value.Stops, [&](const auto& aStop) {const auto found=std::ranges::find(draft.Stops,aStop.Id,&core::tour_stop::Id);return found!=draft.Stops.end() && !core::needs_tour_destination(*found); });
		}
		if (aOptions.Description) {
			const auto desired = core::propose_tour_description(draft, aDescription);
			if (!desired) {
				result.Status = "Map Problem and Solution in the current website description before updating it. Unrelated template text is retained.";
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
		result += aPlan.Description ? "\n\n### Separate optional description edit\n\nAfter the tour succeeds, replace the description with:\n\n" + aPlan.Description->Desired + (aPlan.Description->AppendIndexLink ? "\n\nThe returned tour index URL will be appended." : "") : "\n\nThe PR description will not be edited.";
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
