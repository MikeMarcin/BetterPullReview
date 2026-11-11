#include "tour_publication_plan.hpp"
#include "bpr/core/review_tour_destination.hpp"
#include "bpr/core/review_tour_reconciliation.hpp"
#include "bpr/core/tour_references.hpp"
#include "review_comment_permalink.hpp"
#include <algorithm>
#include <array>
namespace bpr::app {
	auto tour_publication_failure_text(const application::staged_publication_result& aResult) -> std::string
	{
		using failure = application::review_mutation_failure;
		using step    = application::staged_publication_step;
		if (!aResult.Failure || !aResult.Diagnosis) {
			return { };
		}
		const bool  unknown   = aResult.State == application::staged_publication_state::outcome_unknown || (aResult.Record && aResult.Record->PossiblyTransmitted);
		const bool  published = aResult.Record && aResult.Record->Published;
		std::string result;
		if (published) {
			result = aResult.Record->Plan.Description && !aResult.Record->DescriptionDone ? "Tour published; description update needs attention. " : "Tour published; the status check could not finish. ";
		}
		if (unknown) {
			result += published ? "The remaining operation's result is still unknown. " : "Publication result is still unknown. ";
		}
		std::string reason;
		switch (aResult.Diagnosis->Step) {
		case step::authority:           reason = "Publication could not start"; break;
		case step::plan_validation:     reason = "The captured publication plan is invalid"; break;
		case step::recovery_open:       reason = "Publication recovery is unavailable in this profile"; break;
		case step::lease:               reason = "The publishing connection is no longer active"; break;
		case step::configuration:       reason = "The publishing connection configuration is invalid"; break;
		case step::binding:             reason = "The publication does not match this account and pull request"; break;
		case step::journal_read:        reason = "Could not read publication recovery in this profile"; break;
		case step::journal_save:        reason = "Could not save publication recovery in this profile"; break;
		case step::credential_scope:    reason = "Could not open the publishing credential operation"; break;
		case step::identity_check:      reason = "Could not confirm the publishing account and pull request revision"; break;
		case step::request_preparation: reason = "Could not prepare this publication operation"; break;
		case step::verification:
		case step::recovery:
			switch (aResult.Diagnosis->Operation.value_or(application::staged_review_operation::discard_pending)) {
			case application::staged_review_operation::list_file_comments:  reason = "Could not check file comments"; break;
			case application::staged_review_operation::list_issue_comments: reason = "Could not check pull request comments"; break;
			case application::staged_review_operation::list_reviews:        reason = "Could not check pull request reviews"; break;
			case application::staged_review_operation::read_review:         reason = "Could not check the captured review"; break;
			case application::staged_review_operation::list_comments:       reason = "Could not check review comments"; break;
			case application::staged_review_operation::update_description:  reason = "Could not check the pull request description"; break;
			default:                                                        reason = "Could not confirm the publishing account and pull request revision"; break;
			}
			break;
		case step::baseline_read:
			reason = aResult.Diagnosis->Operation == application::staged_review_operation::list_issue_comments ? "Could not read existing pull request comments" : "Could not read existing pull request reviews";
			break;
		}
		result += reason;
		if (const auto status = aResult.Diagnosis->HttpStatus; status && *status >= 100U && *status <= 599U) {
			result += " (HTTP " + std::to_string(*status) + ")";
		}
		result += ". ";
		const auto retry = unknown || published ? std::string{ "Check status" } : aResult.Record ? std::string{ "Preview the captured plan before Resume" }
																								 : std::string{ "Preview again" };
		switch (*aResult.Failure) {
		case failure::refused_by_server:   result += "The server refused the request. Check account access and server limits, then " + retry + ". "; break;
		case failure::target_unavailable:  result += "The server did not expose this target. Check account access, refresh the pull request, then " + retry + ". "; break;
		case failure::malformed_response:  result += "The response could not be validated. Check the connection and server compatibility, then " + retry + ". "; break;
		case failure::undesignated_target:
		case failure::target_moved:
		case failure::stale_head:          result += "Refresh the pull request and confirm the current account before a new preview. "; break;
		case failure::unsupported:         result += "GitHub would not take this operation from this connection. Keep the draft locally or use another connection. "; break;
		case failure::cancelled:           result += "The check was cancelled. " + retry + " to retry. "; break;
		default:                           result += (aResult.Diagnosis->Step == step::journal_read || aResult.Diagnosis->Step == step::journal_save || aResult.Diagnosis->Step == step::recovery_open) ? "Check local profile storage, then " + retry + ". " : "Check the connection, then " + retry + ". "; break;
		}
		if (unknown) {
			return result + "No write will be repeated blindly. Your private draft is kept.";
		}
		if (aResult.Record) {
			return result + "No further writes were sent after this failed check. Confirmed staged or published work is retained.";
		}
		return result + "This attempt sent nothing; your private draft is kept.";
	}
	auto tour_operation_observation(const core::review_tour& aTour, const core::tour_stop& aStop, bool aCompatibility, bool aQualified, std::span<const core::tour_destination_observation> aFacts) -> core::tour_destination_observation
	{
		const auto source = core::publication_source(aStop);
		if (!source) {
			return { };
		}
		auto       observed = core::publication_observation(*source, aTour.ObservedHead.value_or(aTour.Head), aTour.Base, aStop.Body, aCompatibility, aQualified);
		const auto fact     = std::ranges::find(aFacts, *source, &core::tour_destination_observation::Source);
		if (fact != aFacts.end()) {
			observed.PendingOffHunkBatch = fact->PendingOffHunkBatch;
			observed.PendingFileThread   = fact->PendingFileThread;
			observed.StandaloneFile      = fact->StandaloneFile;
		}
		return observed;
	}
	auto prepare_tour_plan(const core::review_tour& aDraft, const application::review_submission_live_binding& aBinding, std::string_view aDescription, tour_publication_options aOptions, bool aQualified, std::span<const core::tour_anchor_file> aFiles, std::span<const core::tour_destination_observation> aFacts) -> tour_plan_preparation
	{
		tour_plan_preparation                      result;
		const auto&                                draft         = aDraft;
		const bool                                 compatibility = aOptions.Compatibility;
		std::vector<core::tour_destination_plan>   decisions;
		std::vector<application::staged_tour_stop> stops;
		// [[BPR-1996]], RFC-BPR-0100 AC06. A stop text's references are
		// published as commit-pinned permalinks at the tour's pinned head (the
		// base for "before the change"): inline as a Markdown link, alone on
		// its line as the bare permalink, which GitHub shows as a snippet. A
		// reference that names no file of the pull request is left as written
		// and said before publishing.
		auto files = core::agent_reference_files{ };
		for (const auto& file : aFiles) {
			files.Changed.push_back(file.Path);
		}
		const auto scope     = adapters::github::detail::tour_permalink_scope{ std::string{ aBinding.Target.Hostname.value() }, aBinding.Target.Repository };
		const auto published = [&](const core::tour_text_reference& aReference) -> std::optional<std::string> {
			auto destination      = core::tour_code_destination{ };
			destination.Commit    = aReference.Before ? draft.Base : draft.Head;
			destination.Path      = aReference.Path;
			destination.Side      = aReference.Before ? core::diff_side::left : core::diff_side::right;
			destination.FirstLine = aReference.First - 1U;
			destination.LastLine  = aReference.Last - 1U;
			const auto link       = aReference.First == 0U ? std::nullopt : adapters::github::detail::compile_tour_permalink(scope, destination);
			if (!link) {
				return std::nullopt;
			}
			return aReference.Alone ? *link : "[" + aReference.Written + "](" + *link + ")";
		};
		auto unlinked = std::vector<std::string>{ };
		for (const auto& stop : draft.Stops) {
			const auto source = core::publication_source(stop);
			if (!source) {
				decisions.push_back({ });
				result.Problems.push_back(core::tour_destination_problem::source_unavailable);
				continue;
			}
			const auto observed = tour_operation_observation(draft, stop, compatibility, aQualified, aFacts);
			const auto repair   = core::plan_tour_stop_repair(draft, stop, observed, aFiles);
			decisions.push_back(repair.Destination);
			result.Problems.push_back(repair.Problem);
			if (decisions.back().Representation == core::tour_representation::file_permalink && ((!stop.FileCommentRequested && !aOptions.FileComments) || decisions.back().JoinsPendingReview)) {
				decisions.back() = { };
			}
			const auto link      = adapters::github::detail::compile_tour_permalink({ std::string{ aBinding.Target.Hostname.value() }, aBinding.Target.Repository }, *source);
			auto       narrative = core::rewrite_tour_references(stop.Body, files, published);
			if (!narrative.Unresolved.empty()) {
				auto said = std::string{ };
				for (const auto& written : narrative.Unresolved) {
					said += (said.empty() ? "" : ", ") + written;
				}
				unlinked.push_back("stop " + std::to_string(stops.size() + 1U) + " (" + said + ")");
			}
			stops.push_back({ stop.Id, *source, stop.Title, std::move(narrative.Text), link.value_or(std::string{ }), decisions.back().Representation, stop.Body });
		}
		if (!unlinked.empty()) {
			result.Unlinked = (unlinked.size() == 1U ? "A reference in " : "References in ") + unlinked.front();
			for (std::size_t i = 1U; i < unlinked.size(); ++i) {
				result.Unlinked += (i + 1U == unlinked.size() ? " and " : ", ") + unlinked[i];
			}
			result.Unlinked += " will not link: they name no file of this pull request. They are published as written.";
		}
		result.Decisions     = decisions;
		const auto readiness = draft.Remote ? core::assess_tour_update(draft, decisions, compatibility, aQualified ? core::tour_operation_support::supported : core::tour_operation_support::unknown, aOptions.Mixed) : core::assess_tour_publication(draft, decisions, compatibility, aOptions.Mixed);
		result.Readiness     = readiness;
		if (readiness != core::tour_publication_readiness::single_review && readiness != core::tour_publication_readiness::compatibility_comments && readiness != core::tour_publication_readiness::mixed_comments) {
			result.Status = readiness == core::tour_publication_readiness::incomplete ? "Complete Problem, Solution and every stop with an exact available source before publishing." : readiness == core::tour_publication_readiness::compatibility_choice ? "Repair incompatible destinations to keep one review, or explicitly choose a mixed tour. Compatibility comments remain an explicit last choice."
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
		application::staged_review_plan value{ .Submission = std::move(submission), .Owner = draft.Owner, .Generation = draft.Revision, .Stops = std::move(stops), .Compatibility = compatibility, .Mixed = aOptions.Mixed };
		if (draft.Remote) {
			// [[BPR-2116]], RFC-BPR-0100 AC06. The frozen tour is rendered as it
			// is sent: a published stop whose text was edited is written anew,
			// so its references are published as a first publication's are.
			// The live draft keeps the author's text, which becomes the
			// published baseline's `DraftText` (`published_tour`).
			auto frozen = draft;
			for (auto& stop : frozen.Stops) {
				if (!core::needs_tour_destination(stop) && core::tour_stop_text_edited(stop)) {
					stop.Body = core::rewrite_tour_references(stop.Body, files, published).Text;
				}
			}
			value.Update = std::move(frozen);
			std::erase_if(value.Stops, [&](const auto& aStop) {const auto found=std::ranges::find(draft.Stops,aStop.Id,&core::tour_stop::Id);return found!=draft.Stops.end() && !core::needs_tour_destination(*found); });
		}
		if (value.Update && value.Stops.empty()) {
			value.Mixed = false;
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
		result.Status = draft.Remote ? "Preview ready: non-atomic update; new destinations first, affected comments next, original index last. Concurrent website changes may conflict." : aOptions.Mixed ? "Preview ready: one pending review, then visible conversation comments for the remaining stops, cross-kind navigation, and one COMMENT submission; non-atomic."
			: compatibility                                                                                                                                                                               ? "Preview ready: visible compatibility comments, then index and navigation; non-atomic."
																																																		  : "Preview ready: one pending review, verified navigation, then one COMMENT submission.";
		return result;
	}
	auto tour_plan_markdown(const application::staged_review_plan& aPlan) -> std::string
	{
		std::string result = "### " + std::string{ aPlan.Update ? "Update existing tour — non-atomic" : aPlan.Mixed ? "Mixed tour — non-atomic"
													   : aPlan.Compatibility                                        ? "Compatibility comments — non-atomic"
																													: "One COMMENT review" }
			+ "\n\nHost: " + std::string{ aPlan.Submission.LiveBinding->Target.Hostname.value() } + " · Repository: " + aPlan.Submission.LiveBinding->Target.Repository.Owner + "/" + aPlan.Submission.LiveBinding->Target.Repository.Name + " · Actor: " + aPlan.Submission.LiveBinding->ActingIdentity.Login + " · PR #" + std::to_string(aPlan.Submission.LiveBinding->Target.Number.value()) + "\n\nHead: `" + std::string{ core::canonical_hex(aPlan.Submission.LiveBinding->Target.Semantic.ExpectedHead).view() } + "`\n\n" + aPlan.Submission.Proposal.Body;
		if (aPlan.Mixed) {
			for (std::size_t i = 0; i < aPlan.Stops.size(); ++i) {
				if (aPlan.Stops[i].Representation == core::tour_representation::file_permalink) {
					result += "\n\nStop " + std::to_string(i + 1) + ": " + aPlan.Stops[i].Title + " publishes as a standalone file comment, a separate visible write outside the pending review.";
				}
			}
			result += "\n\nOne pending review is created first. Conversation comments become visible one at a time, then navigation is patched, followed by one COMMENT submission. Interrupted publication can remain partial.\n\nStops outside Files changed:";
			for (std::size_t i = 0; i < aPlan.Stops.size(); ++i) {
				if (aPlan.Stops[i].Representation == core::tour_representation::conversation_permalink) {
					result += "\n- Stop " + std::to_string(i + 1) + ": " + aPlan.Stops[i].Title;
				}
			}
		}
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

	namespace {
		[[nodiscard]] auto count_words(const std::size_t aCount, const bool aCapital) -> std::string
		{
			static constexpr std::array<std::string_view, 11> words{ "no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };
			auto                                              result = aCount < words.size() ? std::string{ words[aCount] } : std::to_string(aCount);
			if (aCapital && !result.empty() && result[0] >= 'a' && result[0] <= 'z') {
				result[0] = static_cast<char>(result[0] - 'a' + 'A');
			}
			return result;
		}
		[[nodiscard]] auto capital(std::string aText) -> std::string
		{
			if (!aText.empty() && aText[0] >= 'a' && aText[0] <= 'z') {
				aText[0] = static_cast<char>(aText[0] - 'a' + 'A');
			}
			return aText;
		}
		/// Why a stop cannot sit in one review, from its one-review attempt.
		[[nodiscard]] auto forced_reason(const core::tour_destination_problem aProblem, const bool aOne) -> std::string
		{
			using problem = core::tour_destination_problem;
			switch (aProblem) {
			case problem::off_hunk:               return aOne ? "sits outside the lines GitHub shows in this diff" : "sit outside the lines GitHub shows in this diff";
			case problem::unchanged_file:         return aOne ? "is in a file this pull request does not change" : "are in a file this pull request does not change";
			case problem::historical_exact:
			case problem::historical_changed:
			case problem::historical_ambiguous:
			case problem::historical_missing:
			case problem::historical_unavailable: return aOne ? "points at an earlier version of the code" : "point at an earlier version of the code";
			default:                              return "cannot be placed on this diff's lines";
			}
		}
		/// What a stop no form carries needs from the connection.
		[[nodiscard]] auto refused_need(const core::tour_destination_problem aProblem) -> std::string
		{
			using problem = core::tour_destination_problem;
			switch (aProblem) {
			case problem::off_hunk:               return "file comments";
			case problem::unchanged_file:
			case problem::historical_exact:
			case problem::historical_changed:
			case problem::historical_ambiguous:
			case problem::historical_missing:
			case problem::historical_unavailable: return "conversation comments";
			case problem::source_unavailable:     return "a source GitHub can show";
			case problem::incompatible_body:      return "a line comment on this diff";
			default:                              return "review comments on their lines";
			}
		}
		[[nodiscard]] auto in_one_review(const core::tour_destination_plan& aPlan) -> bool
		{
			return aPlan.JoinsPendingReview && aPlan.Representation == core::tour_representation::native_line;
		}
		[[nodiscard]] auto cascades(const core::tour_publication_readiness aReadiness) -> bool
		{
			return aReadiness == core::tour_publication_readiness::compatibility_choice || aReadiness == core::tour_publication_readiness::unavailable;
		}
	}
	auto choose_tour_plan(const core::review_tour& aDraft, const application::review_submission_live_binding& aBinding, std::string_view aDescription, bool aDescribe, bool aQualified, std::span<const core::tour_anchor_file> aFiles, std::span<const core::tour_destination_observation> aFacts) -> tour_plan_choice
	{
		auto       result = tour_plan_choice{ };
		const auto one    = prepare_tour_plan(aDraft, aBinding, aDescription, { .Description = aDescribe, .FileComments = true }, aQualified, aFiles, aFacts);
		auto       chosen = one;
		auto       mixed  = std::optional<tour_plan_preparation>{ };
		auto       compat = std::optional<tour_plan_preparation>{ };
		if (!one.Plan && cascades(one.Readiness)) {
			mixed = prepare_tour_plan(aDraft, aBinding, aDescription, { .Description = aDescribe, .Mixed = true, .FileComments = true }, aQualified, aFiles, aFacts);
			if (mixed->Plan || !cascades(mixed->Readiness)) {
				chosen = *mixed;
			} else {
				compat = prepare_tour_plan(aDraft, aBinding, aDescription, { .Compatibility = true, .Description = aDescribe, .FileComments = true }, aQualified, aFiles, aFacts);
				chosen = compat->Plan || !cascades(compat->Readiness) ? *compat : one;
			}
		}
		result.Plan   = chosen.Plan;
		result.Status = chosen.Status;
		// The stops one review cannot carry, in route order.
		auto forced = std::vector<std::size_t>{ };
		for (std::size_t i = 0U; i < aDraft.Stops.size() && i < one.Decisions.size(); ++i) {
			if (!in_one_review(one.Decisions[i]) && (!aDraft.Remote || core::needs_tour_destination(aDraft.Stops[i]))) {
				forced.push_back(i);
			}
		}
		const auto title = [&](const std::size_t aIndex) {
			return "Stop " + std::to_string(aIndex + 1U) + ", \xE2\x80\x9C" + aDraft.Stops[aIndex].Title + "\xE2\x80\x9D";
		};
		if (!result.Plan) {
			if (!compat || !cascades(compat->Readiness)) {
				return result;
			}
			// No form carries every stop: refused before any write, naming the
			// stops no form carries, what they need, and both ways out.
			auto blocked = std::vector<std::size_t>{ };
			for (const auto i : forced) {
				const auto viaMixed  = i < mixed->Decisions.size() && mixed->Decisions[i].Representation != core::tour_representation::unresolved && !mixed->Decisions[i].NeedsQualification;
				const auto viaCompat = i < compat->Decisions.size() && compat->Decisions[i].Representation == core::tour_representation::conversation_permalink && !compat->Decisions[i].NeedsQualification;
				if (!viaMixed && !viaCompat) {
					blocked.push_back(i);
				}
			}
			if (blocked.empty()) {
				blocked = forced;
			}
			auto banner = application::tour_publication_banner{ .Title = "This connection cannot publish this tour", .Actions = { application::tour_publication_remedy::move_stops, application::tour_publication_remedy::other_connection } };
			auto needs  = std::vector<std::pair<std::string, std::vector<std::size_t>>>{ };
			for (const auto i : blocked) {
				const auto need  = refused_need(i < one.Problems.size() ? one.Problems[i] : core::tour_destination_problem::none);
				auto       found = std::ranges::find(needs, need, &std::pair<std::string, std::vector<std::size_t>>::first);
				if (found == needs.end()) {
					needs.push_back({ need, { } });
					found = needs.end() - 1;
				}
				found->second.push_back(i + 1U);
			}
			for (const auto& [need, numbers] : needs) {
				banner.Why += (banner.Why.empty() ? "" : " ") + capital(application::staged_stop_list(numbers)) + (numbers.size() == 1U ? " needs " : " need ") + need + ", and this connection cannot " + (need.starts_with("a ") ? "provide it." : "create them.");
			}
			if (banner.Why.empty()) {
				banner.Why = "This connection cannot create the comments this tour needs.";
			}
			const auto carried = aDraft.Stops.size() - blocked.size();
			banner.OnGitHub    = "Nothing was sent. " + capital(count_words(carried, false)) + " of " + count_words(aDraft.Stops.size(), false) + (aDraft.Stops.size() == 1U ? " stop" : " stops") + " can publish as " + (carried == 1U ? "it is." : "they are.");
			if (!blocked.empty()) {
				banner.MoveTo = aDraft.Stops[blocked.front()].Id;
			}
			result.Refusal = std::move(banner);
			result.Status  = "Not published. " + result.Refusal->Why;
			return result;
		}
		const auto& plan  = *result.Plan;
		const auto  steps = application::staged_publication_steps(plan).size();
		result.Form       = (plan.Update ? "Updates the published tour in " : "Publishes in ") + std::to_string(steps) + (steps == 1U ? " step." : " steps.");
		if (!plan.Mixed && !plan.Compatibility) {
			const auto count = plan.Stops.size();
			if (count > 0U) {
				result.Form += count == 1U ? (plan.Update ? " The added stop goes in one review." : " The stop goes in one review.") : (plan.Update ? " The " + count_words(count, false) + " added stops go in one review." : " All " + count_words(count, false) + " stops go in one review.");
			}
			if (!chosen.Unlinked.empty()) {
				result.Form += " " + chosen.Unlinked;
			}
			return result;
		}
		// The stops that forced the form, grouped by why and how they go.
		struct forced_group
		{
			core::tour_destination_problem Problem = core::tour_destination_problem::none;
			std::string                    Why, How;
			std::vector<std::size_t>       Numbers;
		};
		auto groups = std::vector<forced_group>{ };
		for (const auto i : forced) {
			const auto found   = std::ranges::find(plan.Stops, aDraft.Stops[i].Id, &application::staged_tour_stop::Id);
			const auto how     = plan.Compatibility || found == plan.Stops.end() || found->Representation != core::tour_representation::file_permalink ? std::string{ "conversation comment" } : std::string{ "file comment" };
			const auto problem = i < one.Problems.size() ? one.Problems[i] : core::tour_destination_problem::none;
			const auto why     = forced_reason(problem, false);
			auto       group   = std::ranges::find_if(groups, [&](const auto& aGroup) { return aGroup.Why == why && aGroup.How == how; });
			if (group == groups.end()) {
				groups.push_back({ .Problem = problem, .Why = why, .How = how });
				group = groups.end() - 1;
			}
			group->Numbers.push_back(i + 1U);
			result.KeepOneReview.emplace_back(aDraft.Stops[i].Id, title(i));
		}
		if (plan.Mixed) {
			const auto inReview = static_cast<std::size_t>(std::ranges::count(plan.Stops, core::tour_representation::native_line, &application::staged_tour_stop::Representation));
			result.Form += " " + count_words(inReview, true) + (inReview == 1U ? " stop goes" : " stops go") + " in one review.";
		} else {
			result.Form += " Every stop goes as its own conversation comment.";
		}
		for (const auto& group : groups) {
			const auto single = group.Numbers.size() == 1U;
			result.Form += " " + capital(application::staged_stop_list(group.Numbers)) + " " + forced_reason(group.Problem, single);
			result.Form += plan.Mixed ? (single ? ", so it is sent as a " + group.How + "." : ", so they are sent as " + group.How + "s.") : ", so no single review can carry the tour.";
		}
		if (!chosen.Unlinked.empty()) {
			result.Form += " " + chosen.Unlinked;
		}
		return result;
	}

	namespace {
		struct step_failure_words
		{
			std::string_view Reason, Clause;
		};
		[[nodiscard]] auto step_failure(const application::staged_publication_result& aResult) -> step_failure_words
		{
			using failure = application::review_mutation_failure;
			switch (aResult.Failure.value_or(failure::unavailable)) {
			case failure::refused_by_server:
			case failure::refused_not_published:  return { "Refused by GitHub", "was refused by GitHub" };
			case failure::unplaceable_line:       return { "Line outside the diff", "was refused: GitHub could not place it on this diff" };
			case failure::target_unavailable:     return { "Target not found", "could not find its target on GitHub" };
			case failure::malformed_response:     return { "Answer not understood", "got an answer that could not be checked" };
			case failure::undesignated_target:
			case failure::target_moved:
			case failure::stale_head:             return { "Pull request changed", "found that the pull request had changed" };
			case failure::unsupported:            return { "Not accepted on this connection", "was not accepted from this connection" };
			case failure::cancelled:              return { "Stopped", "was stopped" };
			case failure::credential_unavailable:
			case failure::credential_tool_failed: return { "Credential unavailable", "could not use the GitHub credential" };
			default:                              return { "Did not complete", "did not complete" };
			}
		}
		[[nodiscard]] auto http_status(const application::staged_publication_result& aResult) -> std::optional<std::uint16_t>
		{
			if (aResult.Diagnosis && aResult.Diagnosis->HttpStatus && *aResult.Diagnosis->HttpStatus >= 100U && *aResult.Diagnosis->HttpStatus <= 599U) {
				return aResult.Diagnosis->HttpStatus;
			}
			return std::nullopt;
		}
	}
	auto tour_step_failure_reason(const application::staged_publication_result& aResult) -> std::string
	{
		auto result = std::string{ step_failure(aResult).Reason };
		if (const auto status = http_status(aResult)) {
			result += " (" + std::to_string(*status) + ")";
		}
		return result;
	}
	auto tour_step_failure_clause(const application::staged_publication_result& aResult) -> std::string
	{
		auto result = std::string{ step_failure(aResult).Clause };
		if (const auto status = http_status(aResult)) {
			result += " (HTTP " + std::to_string(*status) + ")";
		}
		return result;
	}
}
