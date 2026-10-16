#include "review_submission_session.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace bpr::adapters::github::detail {
	namespace {
		using reason = core::review_submission_unknown_reason;
		/// [[BPR-1540]]. The hundreds digit of an HTTP status, 0 outside 200 to 599.
		[[nodiscard]] auto status_class(const int aStatus) noexcept -> std::uint8_t
		{
			return aStatus >= 200 && aStatus <= 599 ? static_cast<std::uint8_t>(aStatus / 100) : std::uint8_t{ 0U };
		}
		auto wire_input(const application::review_submission_request& aRequest) -> review_submission_request
		{
			return { .Target = aRequest.LiveBinding->Target, .ActingIdentity = aRequest.LiveBinding->ActingIdentity, .Disposition = aRequest.Proposal.Disposition, .Body = aRequest.Proposal.Body, .Comments = aRequest.Proposal.Comments };
		}
		auto result(const application::review_submission_request& aRequest, application::review_submission_publication aPublication = application::review_submission_publication::indeterminate, application::review_mutation_failure aFailure = application::review_mutation_failure::unavailable) -> application::review_submission_result { return { .Request = aRequest, .Publication = aPublication, .Failure = aFailure }; }
		/// [[BPR-1538]]. Where a listed comment was written at `aHead`, or
		/// none when it was written at another commit.
		struct written_place
		{
			std::uint32_t                Line = 0U;
			std::optional<std::uint32_t> StartLine;
		};
		auto written_at(const review_comment_collection_entry& aComment, const std::string_view aHead) -> std::optional<written_place>
		{
			if (aComment.OriginalLine) {
				if (aComment.OriginalCommitId != aHead) {
					return std::nullopt;
				}
				return written_place{ .Line = *aComment.OriginalLine, .StartLine = aComment.OriginalStartLine };
			}
			if (aComment.CommitId != aHead || !aComment.Line) {
				return std::nullopt;
			}
			return written_place{ .Line = *aComment.Line, .StartLine = aComment.StartLine };
		}
		auto receipt(const review_submission_observation& aReview) -> application::review_submission_receipt { return { .ReviewDatabaseId = application::review_database_id{ aReview.ReviewDatabaseId }, .ReviewNodeId = aReview.ReviewNodeId, .Target = aReview.Target.Semantic, .ActingAccount = aReview.Author.Account, .Disposition = aReview.Disposition }; }
	}
	review_submission_session::review_submission_session(github_mutation_wire_transport& aTransport, live_review_mutation_configuration aConfiguration, application::review_mutation_transport aLease, std::unique_ptr<application::review_submission_recovery_port> aRecovery,
														 std::shared_ptr<application::diagnostic_log_port> aLog)
		: Transport{ &aTransport }
		, Configuration{ std::move(aConfiguration) }
		, Lease{ std::move(aLease) }
		, Recovery{ std::move(aRecovery) }
		, Log{ std::move(aLog) }
	{ }
	void review_submission_session::note(const core::review_submission_unknown_reason aReason, const std::uint8_t aHttpClass)
	{
		if (!Why) {
			Why = core::review_submission_unknown_diagnosis{ .Reason = aReason, .HttpStatusClass = aHttpClass };
		}
	}
	void review_submission_session::note_completion(const github_mutation_wire_completion& aCompletion)
	{
		if (std::holds_alternative<github_mutation_wire_cancelled>(aCompletion)) {
			note(reason::cancelled);
			return;
		}
		if (const auto* success = std::get_if<github_mutation_wire_success>(&aCompletion)) {
			const auto status = success->Response.Status;
			note(status == 429 ? reason::rate_limited : reason::http_status, status_class(status));
			return;
		}
		const auto& error = std::get<github_mutation_wire_error>(aCompletion);
		if (error.ProviderFailure) {
			switch (*error.ProviderFailure) {
			case application::github_provider_failure::rate_limit:      note(reason::rate_limited, status_class(error.HttpStatus)); return;
			case application::github_provider_failure::timeout:         note(reason::timeout); return;
			case application::github_provider_failure::network:
			case application::github_provider_failure::certificate:
			case application::github_provider_failure::redirect:        note(reason::network, status_class(error.HttpStatus)); return;
			case application::github_provider_failure::authentication:
			case application::github_provider_failure::permission:
			case application::github_provider_failure::vault:
			case application::github_provider_failure::cli_unavailable:
			case application::github_provider_failure::process_startup: note(reason::connection_unavailable, status_class(error.HttpStatus)); return;
			default:                                                    break;
			}
		}
		if (error.HttpStatus != 0U) {
			note(error.HttpStatus == 429U ? reason::rate_limited : reason::http_status, status_class(error.HttpStatus));
			return;
		}
		switch (error.Failure) {
		case application::review_mutation_failure::cancelled:              note(reason::cancelled); return;
		case application::review_mutation_failure::credential_unavailable:
		case application::review_mutation_failure::credential_tool_failed: note(reason::connection_unavailable); return;
		default:                                                           note(reason::network); return;
		}
	}
	void review_submission_session::explain(application::review_submission_result& aResult)
	{
		if (aResult.Publication != application::review_submission_publication::indeterminate) {
			aResult.Unknown.reset();
			return;
		}
		auto diagnosis             = Why.value_or(core::review_submission_unknown_diagnosis{ .Reason = reason::connection_unavailable });
		diagnosis.PostAnswer       = PostAnswer;
		diagnosis.PostStatusClass  = PostStatusClass;
		diagnosis.CommentsExpected = static_cast<std::uint16_t>(std::min<std::size_t>(aResult.Request.Proposal.Comments.size(), core::maximum_review_submission_comments));
		diagnosis.PagesRead        = Pages;
		diagnosis.CandidateReviews = Candidates;
		diagnosis.CommentsMatched  = std::min({ diagnosis.CommentsMatched, diagnosis.CommentsExpected, diagnosis.CommentsFound });
		if (!core::valid(diagnosis)) {
			return;
		}
		aResult.Unknown = diagnosis;
		// Kept with the attempt when there is one; nothing else is written.
		if (Recovery && application::valid(aResult.Request)) {
			static_cast<void>(Recovery->note_unknown(aResult.Request.IdempotencyKey, application::review_submission_request_fingerprint(aResult.Request), diagnosis));
		}
		if (Log) {
			auto       event = application::diagnostic_event{ .Operation = application::diagnostic_operation::review_submission, .Outcome = application::diagnostic_outcome::failed };
			const auto text  = core::describe(diagnosis);
			if (text.size() <= event.Summary.Text.size()) {
				std::ranges::copy(text, event.Summary.Text.begin());
				event.Summary.Size  = static_cast<std::uint16_t>(text.size());
				event.Summary.State = application::diagnostic_summary_state::sanitized;
			}
			event.ReviewSubmission = diagnosis;
			static_cast<void>(Log->record(event));
		}
	}
	auto review_submission_session::perform(review_mutation_wire_request aWire, std::stop_token aStop, github_mutation_transmission_witness aWitness) -> github_mutation_wire_completion
	{
		if (aStop.stop_requested()) {
			return github_mutation_wire_cancelled{ };
		}
		if (!Lease || !Lease->active() || !valid(Configuration) || !validate_outbound_review_mutation_request(aWire)) {
			return github_mutation_wire_error{ };
		}
		return Transport->perform_review_mutation({ .Context = Configuration.CredentialContext, .Wire = std::move(aWire) }, aStop, std::move(aWitness));
	}
	auto review_submission_session::observe_exact(std::stop_token aStop, bool aHistorical) -> std::expected<void, application::review_mutation_failure>
	{
		auto viewer = compile_viewer_observation_request(Configuration.ActingIdentity, Configuration.Target.ServerContract);
		if (!viewer) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto  observedViewer = perform(*viewer, aStop);
		auto* viewerSuccess  = std::get_if<github_mutation_wire_success>(&observedViewer);
		if (!viewerSuccess || viewerSuccess->Response.Status != 200) {
			note_completion(observedViewer);
			return std::unexpected(application::review_mutation_failure::undesignated_target);
		}
		if (!parse_viewer_observation(viewerSuccess->Response, Configuration.ActingIdentity)) {
			note(reason::account_mismatch);
			return std::unexpected(application::review_mutation_failure::undesignated_target);
		}
		auto target = compile_target_observation_request(Configuration.Target, Configuration.Target.ServerContract);
		if (!target) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		auto  observedTarget = perform(*target, aStop);
		auto* targetSuccess  = std::get_if<github_mutation_wire_success>(&observedTarget);
		if (!targetSuccess || targetSuccess->Response.Status != 200) {
			note_completion(observedTarget);
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
		if (aHistorical) {
			if (!parse_review_submission_recovery_target(targetSuccess->Response, Configuration.Target)) {
				note(reason::pull_request_mismatch);
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			return { };
		}
		auto parsed = parse_target_observation(targetSuccess->Response, Configuration.Target);
		if (!parsed) {
			note(reason::pull_request_mismatch);
			return std::unexpected(application::review_mutation_failure::undesignated_target);
		}
		if (parsed->Target.Semantic.ExpectedHead != Configuration.Target.Semantic.ExpectedHead) {
			return std::unexpected(application::review_mutation_failure::stale_head);
		}
		return { };
	}
	auto review_submission_session::observe_reviews(const application::review_submission_request& aRequest, std::stop_token aStop) -> review_mutation_parse_result<review_submission_reconciliation>
	{
		auto wire = compile_review_submission_observation_request(wire_input(aRequest), Configuration.Target.ServerContract);
		if (!wire) {
			note(reason::review_list_unreadable);
			return std::unexpected(wire.error());
		}
		auto  completion = perform(*wire, aStop);
		auto* success    = std::get_if<github_mutation_wire_success>(&completion);
		if (success == nullptr || success->Response.Status != 200) {
			note_completion(completion);
			return std::unexpected(review_mutation_contract_failure::invalid_status);
		}
		auto parsed = parse_review_submission_observations(success->Response, wire_input(aRequest));
		if (!parsed) {
			// [[BPR-1540]]. The list is refused whole; say how.
			const auto paged = std::ranges::any_of(success->Response.Headers, [](const http_header& aHeader) {
				return aHeader.Name.size() == 4U && std::ranges::equal(aHeader.Name, std::string_view{ "link" }, [](const unsigned char aLeft, const unsigned char aRight) { return std::tolower(aLeft) == aRight; });
			});
			note(paged ? reason::review_list_paged : parsed.error() == review_mutation_contract_failure::response_limit ? reason::review_list_too_large
																														: reason::review_list_unreadable);
		}
		return parsed;
	}
	auto review_submission_session::finish_known(const application::review_submission_recovery_record& aRecord, std::stop_token aStop, bool aDuplicate) -> application::review_submission_result
	{
		auto outcome = result(aRecord.Request);
		if (!application::valid(aRecord)) {
			note(reason::local_record_unavailable);
			return outcome;
		}
		if (aRecord.Receipt) {
			outcome.Publication = application::review_submission_publication::published;
			outcome.Receipt     = aRecord.Receipt;
			outcome.Duplicate   = true;
			return outcome;
		}
		if (aRecord.Transmission == application::review_mutation_recovery_transmission::prepared && aRecord.State == application::review_mutation_recovery_state::unresolved) {
			// [[BPR-1345]]. Its witness never admitted the POST, so nothing
			// left this computer: it was definitely not published, and is
			// withdrawn rather than restored as unfinished at every start.
			if (Recovery->withdraw(aRecord.Request.IdempotencyKey, aRecord.RequestFingerprint)) {
				outcome.Publication = application::review_submission_publication::definitely_not_published;
			} else {
				note(reason::local_record_unavailable);
			}
			return outcome;
		}
		if (aRecord.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted || !aRecord.BaselineReviewIds) {
			note(reason::local_record_unavailable);
			return outcome;
		}
		auto observed = observe_reviews(aRecord.Request, aStop);
		if (!observed) {
			return outcome;
		}
		return resolve_observed(aRecord, *observed, aDuplicate, aStop);
	}
	auto review_submission_session::observe_comment_collection(std::stop_token aStop) -> review_mutation_parse_result<std::vector<review_comment_collection_entry>>
	{
		auto        result = std::vector<review_comment_collection_entry>{ };
		std::size_t bytes  = 0U;
		for (std::uint32_t page = 1U; page <= maximum_review_comment_collection_pages; ++page) {
			const auto wire = compile_review_comment_collection_request(Configuration.Target, Configuration.Target.ServerContract, page);
			if (!wire) {
				return std::unexpected(wire.error());
			}
			const auto  completion = perform(*wire, aStop);
			const auto* success    = std::get_if<github_mutation_wire_success>(&completion);
			if (success == nullptr || success->Response.Status != 200) {
				note_completion(completion);
				return std::unexpected(review_mutation_contract_failure::invalid_status);
			}
			Pages = static_cast<std::uint16_t>(Pages + 1U);
			bytes += success->Response.Body.size();
			if (bytes > maximum_review_mutation_list_response_bytes * maximum_review_comment_collection_pages) {
				note(reason::comment_page_too_large);
				return std::unexpected(review_mutation_contract_failure::response_limit);
			}
			auto parsed = parse_review_comment_collection(success->Response, Configuration.Target, page);
			if (!parsed) {
				// [[BPR-1540]]. A page refused, by size, by its next link, or
				// as not GitHub's list.
				note(parsed.error() == review_mutation_contract_failure::response_limit           ? reason::comment_page_too_large
						 : parsed.error() == review_mutation_contract_failure::collection_limit   ? reason::comment_collection_too_long
						 : parsed.error() == review_mutation_contract_failure::unexpected_headers ? reason::comment_page_link_refused
																								  : reason::comment_page_unreadable);
				return std::unexpected(parsed.error());
			}
			for (auto& comment : parsed->Comments) {
				if (std::ranges::any_of(result, [&comment](const review_comment_collection_entry& aSeen) { return aSeen.Id == comment.Id || aSeen.NodeId == comment.NodeId; })) {
					note(reason::comment_page_unreadable);
					return std::unexpected(review_mutation_contract_failure::identity_mismatch);
				}
				result.push_back(std::move(comment));
			}
			if (!parsed->HasNext) {
				return result;
			}
		}
		note(reason::comment_collection_too_long);
		return std::unexpected(review_mutation_contract_failure::response_limit);
	}
	auto review_submission_session::carries_exactly(const application::review_submission_request& aRequest, const application::review_database_id aReview, std::stop_token aStop) -> bool
	{
		// GitHub's list of one review's comments has no line or side (seen on
		// MikeMarcin/BPR-Test#44 and #45, 2026-10-09); the pull request's
		// comment collection names each comment's review, path, lines and side.
		const auto listed = observe_comment_collection(aStop);
		if (!listed) {
			return false;
		}
		// [[BPR-1538]]. Held to the head the review was written on, not the
		// head this session observes now: after a push GitHub moves a
		// comment's `commit_id` and `line`, or leaves the comment outdated
		// with no line, so Check status could never match it. Where the
		// comment was written (`original_commit_id`, `original_line`,
		// `original_start_line`) does not move. A server that names no
		// original line is held to the current position at that head.
		const auto head      = core::canonical_hex(aRequest.Proposal.Target.ExpectedHead);
		const auto held      = std::ranges::count_if(*listed, [aReview](const review_comment_collection_entry& aComment) { return aComment.Review == aReview.value(); });
		auto       unmatched = std::vector<const review_comment_collection_entry*>{ };
		// [[BPR-1540]]. Says how many comments the review holds and how many
		// matched when the match stops.
		const auto mismatch = [this, held](const core::review_submission_unknown_reason aReason, const std::size_t aMatched) {
			if (!Why) {
				note(aReason);
				Why->CommentsFound   = static_cast<std::uint16_t>(std::min<std::ptrdiff_t>(held, std::numeric_limits<std::uint16_t>::max()));
				Why->CommentsMatched = static_cast<std::uint16_t>(std::min<std::size_t>(aMatched, Why->CommentsFound));
			}
			return false;
		};
		for (const auto& comment : *listed) {
			if (comment.Review != aReview.value()) {
				continue;
			}
			if (!written_at(comment, head.view())) {
				// Moved off the review's head with no original position named,
				// or written at another commit.
				return mismatch(!comment.OriginalLine && comment.CommitId != head.view() ? core::review_submission_unknown_reason::comment_moved_without_original_position
																						 : core::review_submission_unknown_reason::comments_mismatch,
								0U);
			}
			if (comment.Reply || !comment.OnLine || comment.Author != Configuration.ActingIdentity.DatabaseId || comment.Login != Configuration.ActingIdentity.Login) {
				return mismatch(core::review_submission_unknown_reason::comments_mismatch, 0U);
			}
			unmatched.push_back(&comment);
		}
		if (unmatched.size() != aRequest.Proposal.Comments.size()) {
			return mismatch(core::review_submission_unknown_reason::comments_mismatch, 0U);
		}
		auto matched = std::size_t{ 0U };
		for (const auto& sent : aRequest.Proposal.Comments) {
			const auto found = std::ranges::find_if(unmatched, [&sent, &head](const review_comment_collection_entry* aComment) {
				const auto written = written_at(*aComment, head.view());
				return written && aComment->Path == sent.Path && aComment->Side == sent.Side && written->Line == sent.Line && written->StartLine == sent.StartLine
					&& (!aComment->StartSide || aComment->StartSide == std::optional{ sent.Side }) && aComment->Body == sent.Body;
			});
			if (found == unmatched.end()) {
				return mismatch(core::review_submission_unknown_reason::comments_mismatch, matched);
			}
			unmatched.erase(found);
			++matched;
		}
		return unmatched.empty() || mismatch(core::review_submission_unknown_reason::comments_mismatch, matched);
	}
	auto review_submission_session::resolve_observed(const application::review_submission_recovery_record& aRecord, const review_submission_reconciliation& aObserved, bool aDuplicate, std::stop_token aStop) -> application::review_submission_result
	{
		auto                                                  outcome = result(aRecord.Request);
		std::optional<application::review_submission_receipt> candidate;
		// [[BPR-1540]]. How many new reviews of mine match.
		Candidates = static_cast<std::uint16_t>(std::ranges::count_if(aObserved.ExactReviews, [&aRecord](const review_submission_observation& aReview) {
			return !std::ranges::binary_search(*aRecord.BaselineReviewIds, application::review_database_id{ aReview.ReviewDatabaseId });
		}));
		for (const auto& review : aObserved.ExactReviews) {
			const auto current = receipt(review);
			if (std::ranges::binary_search(*aRecord.BaselineReviewIds, current.ReviewDatabaseId)) {
				continue;
			}
			if (candidate) {
				note(core::review_submission_unknown_reason::several_reviews_listed);
				return outcome;
			}
			candidate = current;
		}
		if (!candidate) {
			note(core::review_submission_unknown_reason::review_not_listed);
			return outcome;
		}
		// [[BPR-1345]]. A review that carried line comments is this one only
		// if GitHub holds exactly those comments in it: a single comment
		// published meanwhile is also a new review with no summary.
		if (!aRecord.Request.Proposal.Comments.empty() && !carries_exactly(aRecord.Request, candidate->ReviewDatabaseId, aStop)) {
			return outcome;
		}
		auto resolved = Recovery->resolve(aRecord.Request.IdempotencyKey, aRecord.RequestFingerprint, *candidate);
		if (!resolved || !application::valid(*resolved) || resolved->Request != aRecord.Request || resolved->RequestFingerprint != aRecord.RequestFingerprint || resolved->AuthorityFingerprint != aRecord.AuthorityFingerprint || resolved->BaselineReviewIds != aRecord.BaselineReviewIds || resolved->Receipt != candidate) {
			note(core::review_submission_unknown_reason::local_record_unavailable);
			return outcome;
		}
		outcome.Publication = application::review_submission_publication::published;
		outcome.Receipt     = std::move(candidate);
		outcome.Duplicate   = aDuplicate;
		return outcome;
	}
	auto review_submission_session::execute(const application::review_submission_request& aRequest, std::stop_token aStop, bool aAllowNew) -> application::review_submission_result
	{
		try {
			std::scoped_lock lock{ Mutex };
			Why.reset();
			PostAnswer      = core::review_submission_post_answer::not_sent;
			PostStatusClass = 0U;
			Pages           = 0U;
			Candidates      = 0U;
			auto outcome    = application::review_submission_result{ .Request = aRequest };
			try {
				outcome = run(aRequest, aStop, aAllowNew);
			} catch (...) {
				outcome = result(aRequest);
			}
			explain(outcome);
			return outcome;
		} catch (...) {
			return result(aRequest);
		}
	}
	auto review_submission_session::run(const application::review_submission_request& aRequest, std::stop_token aStop, bool aAllowNew) -> application::review_submission_result
	{
		using publication = application::review_submission_publication;
		{
			if (!application::valid(aRequest) || !aRequest.LiveBinding || !application::same_review_submission_identity(*aRequest.LiveBinding, { .Target = Configuration.Target, .ActingIdentity = Configuration.ActingIdentity })) {
				note(aRequest.LiveBinding && aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity ? reason::account_mismatch : reason::pull_request_mismatch);
				return result(aRequest, aAllowNew ? publication::definitely_not_published : publication::indeterminate, application::review_mutation_failure::undesignated_target);
			}
			if (!Recovery) {
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			if (!Lease || !Lease->active() || !valid(Configuration)) {
				note(reason::connection_unavailable);
				return result(aRequest);
			}
			auto wire = compile_review_submission_request(wire_input(aRequest), Configuration.Target.ServerContract);
			if (!wire) {
				return result(aRequest, publication::definitely_not_published);
			}
			const auto fingerprint = application::review_submission_request_fingerprint(aRequest);
			const auto authority   = application::review_submission_authority_fingerprint(*aRequest.LiveBinding);
			auto       loaded      = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded) {
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			// A later credential refusal cannot erase an earlier durable write.
			// Confirmed and unwitnessed records need no provider operation at all.
			if (*loaded) {
				const auto& known = **loaded;
				if (!application::valid(known) || known.Request != aRequest || known.RequestFingerprint != fingerprint || known.AuthorityFingerprint != authority) {
					note(reason::local_record_unavailable);
					return result(aRequest);
				}
				if (known.Receipt || known.Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
					return finish_known(known, aStop, true);
				}
			} else if (!aAllowNew) {
				// A missing local row is not proof that the original review was never sent.
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				note(operation.failure() == application::review_mutation_failure::cancelled ? reason::cancelled : reason::connection_unavailable);
				return result(aRequest, *loaded ? publication::indeterminate : publication::definitely_not_published, operation.failure());
			}
			if (*loaded) {
				const auto& known = **loaded;
				if (aStop.stop_requested()) {
					note(reason::cancelled);
					return result(aRequest);
				}
				if (!observe_exact(aStop, true)) {
					return result(aRequest);
				}
				return finish_known(known, aStop, true);
			}
			if (aRequest.LiveBinding->Target != Configuration.Target || aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity) {
				return result(aRequest, publication::definitely_not_published, application::review_mutation_failure::stale_head);
			}
			if (aStop.stop_requested()) {
				return result(aRequest, publication::definitely_not_published, application::review_mutation_failure::cancelled);
			}
			const auto baseline = observe_reviews(aRequest, aStop);
			if (!baseline) {
				return result(aRequest, publication::definitely_not_published);
			}
			auto expected = application::review_submission_recovery_record{ .Request = aRequest, .RequestFingerprint = fingerprint, .AuthorityFingerprint = authority, .BaselineReviewIds = baseline->ReviewIds, .Transmission = application::review_mutation_recovery_transmission::prepared };
			auto admitted = Recovery->admit(expected);
			if (!admitted || !application::valid(admitted->Record) || admitted->Record.Request != aRequest || admitted->Record.RequestFingerprint != fingerprint || admitted->Record.AuthorityFingerprint != authority) {
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			if (!admitted->Inserted) {
				if (!admitted->Record.Receipt
					&& admitted->Record.Transmission == application::review_mutation_recovery_transmission::possibly_transmitted
					&& !observe_exact(aStop, true)) {
					return result(aRequest);
				}
				return finish_known(admitted->Record, aStop, true);
			}
			if (admitted->Record != expected) {
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			// [[BPR-1345]]. A request admitted but refused before its witness
			// was never sent; it is withdrawn, not left to be restored.
			const auto withdrawn = [this, &expected](application::review_submission_result aResult) {
				static_cast<void>(Recovery->withdraw(expected.Request.IdempotencyKey, expected.RequestFingerprint));
				return aResult;
			};
			const auto current = observe_exact(aStop);
			if (!current) {
				return withdrawn(result(aRequest, publication::definitely_not_published, current.error()));
			}
			if (aStop.stop_requested() || !Lease->active()) {
				return withdrawn(result(aRequest, publication::definitely_not_published, application::review_mutation_failure::cancelled));
			}
			auto witnessed  = std::make_shared<std::atomic_bool>(false);
			auto completion = github_mutation_wire_completion{ github_mutation_wire_error{ .Transmission = github_mutation_transmission::indeterminate } };
			try {
				completion = perform(*wire, aStop, [this, witnessed, expected] {
					if (witnessed->exchange(true)) {
						return false;
					}
					const auto marked  = Recovery->mark_possibly_transmitted(expected.Request.IdempotencyKey, expected.RequestFingerprint);
					auto       exact   = expected;
					exact.Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
					return marked && *marked == exact && application::valid(*marked);
				});
			} catch (...) { /* The durable witness and server observation decide the outcome. */
			}
			// [[BPR-1540]]. What GitHub answered the POST, by class only.
			if (const auto* answered = std::get_if<github_mutation_wire_success>(&completion)) {
				PostAnswer      = answered->Response.Status >= 200 && answered->Response.Status < 300 ? core::review_submission_post_answer::success : core::review_submission_post_answer::error_status;
				PostStatusClass = status_class(answered->Response.Status);
			} else if (const auto* failed = std::get_if<github_mutation_wire_error>(&completion); failed != nullptr && status_class(failed->HttpStatus) != 0U) {
				PostAnswer      = core::review_submission_post_answer::error_status;
				PostStatusClass = status_class(failed->HttpStatus);
			} else {
				PostAnswer = core::review_submission_post_answer::no_answer;
			}
			// A POST receipt alone never establishes completion: server truth must contain one new exact review.
			const auto observed         = observe_reviews(aRequest, { });
			const auto definitelyUnsent = std::visit([](const auto& aCompletion) {
				using completion_type = std::remove_cvref_t<decltype(aCompletion)>;
				if constexpr (std::same_as<completion_type, github_mutation_wire_success>) {
					return false;
				} else {
					return aCompletion.Transmission == github_mutation_transmission::definitely_not_sent;
				}
			},
													 completion);
			if (!witnessed->load() && definitelyUnsent) {
				return withdrawn(result(aRequest, publication::definitely_not_published));
			}
			const auto persisted = Recovery->load(aRequest.IdempotencyKey);
			if (!persisted || !*persisted || !application::valid(**persisted) || (**persisted).Request != aRequest || (**persisted).BaselineReviewIds != expected.BaselineReviewIds || (**persisted).Transmission != application::review_mutation_recovery_transmission::possibly_transmitted || !observed) {
				note(reason::local_record_unavailable);
				return result(aRequest);
			}
			// [[BPR-1345]]. GitHub answered 422: it refused the review, which
			// it creates whole or not at all. With no new exact review of mine
			// observed after the answer, it was definitely not published, and
			// its record is withdrawn so it is not restored as unfinished. The
			// server refused it: the connection supports the operation, so the
			// refusal is not reported as `unsupported`, whose text says it does
			// not (BPR-1429).
			const auto* refusal = std::get_if<github_mutation_wire_error>(&completion);
			if (refusal != nullptr && refusal->HttpStatus == 422U
				&& std::ranges::all_of(observed->ExactReviews, [&expected](const review_submission_observation& aReview) {
					   return std::ranges::binary_search(*expected.BaselineReviewIds, application::review_database_id{ aReview.ReviewDatabaseId });
				   })) {
				const auto unpublished = result(aRequest, publication::definitely_not_published, application::review_mutation_failure::refused_by_server);
				static_cast<void>(Recovery->withdraw(aRequest.IdempotencyKey, (**persisted).RequestFingerprint));
				return unpublished;
			}
			return resolve_observed(**persisted, *observed, false, aStop);
		}
	}
	auto review_submission_session::pending(std::stop_token aStop) -> std::expected<std::vector<application::review_submission_request>, application::review_mutation_failure>
	{
		try {
			std::scoped_lock lock{ Mutex };
			if (aStop.stop_requested() || !Recovery || !Lease || !Lease->active() || !valid(Configuration)) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			auto records = Recovery->pending();
			if (!records || records->size() > application::maximum_review_mutation_recovery_records) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			std::vector<application::review_submission_request> requests;
			for (const auto& record : *records) {
				if (aStop.stop_requested() || !application::valid(record) || record.State != application::review_mutation_recovery_state::unresolved || !application::same_review_submission_identity(*record.Request.LiveBinding, { .Target = Configuration.Target, .ActingIdentity = Configuration.ActingIdentity })) {
					return std::unexpected(application::review_mutation_failure::unavailable);
				}
				requests.push_back(record.Request);
			}
			return requests;
		} catch (...) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}
	auto review_submission_session::admitted(const std::string& aKey, std::stop_token aStop) -> std::expected<std::optional<application::review_submission_request>, application::review_mutation_failure>
	{
		try {
			std::scoped_lock lock{ Mutex };
			if (aStop.stop_requested() || !Recovery || !Lease || !Lease->active() || !valid(Configuration)) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			auto loaded = Recovery->load(aKey);
			if (!loaded) {
				return std::unexpected(application::review_mutation_failure::unavailable);
			}
			if (!*loaded) {
				return std::optional<application::review_submission_request>{ };
			}
			const auto& record = **loaded;
			if (!application::valid(record) || record.Request.IdempotencyKey != aKey
				|| !application::same_review_submission_identity(*record.Request.LiveBinding, { .Target = Configuration.Target, .ActingIdentity = Configuration.ActingIdentity })) {
				return std::unexpected(application::review_mutation_failure::undesignated_target);
			}
			return std::optional{ record.Request };
		} catch (...) {
			return std::unexpected(application::review_mutation_failure::unavailable);
		}
	}
	auto review_submission_session::last_unknown(const std::string& aKey, std::stop_token aStop) -> std::optional<core::review_submission_unknown_diagnosis>
	{
		try {
			std::scoped_lock lock{ Mutex };
			if (aStop.stop_requested() || !Recovery) {
				return std::nullopt;
			}
			const auto loaded = Recovery->load(aKey);
			if (!loaded || !*loaded || !application::valid(**loaded) || (**loaded).Request.IdempotencyKey != aKey || !(**loaded).Request.LiveBinding
				|| !application::same_review_submission_identity(*(**loaded).Request.LiveBinding, { .Target = Configuration.Target, .ActingIdentity = Configuration.ActingIdentity })) {
				return std::nullopt;
			}
			return (**loaded).LastUnknown;
		} catch (...) {
			return std::nullopt;
		}
	}
	auto review_submission_session::submit_and_reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return execute(aRequest, aStop, true); }
	auto review_submission_session::reconcile(const application::review_submission_request& aRequest, std::stop_token aStop) -> application::review_submission_result { return execute(aRequest, aStop, false); }
}
