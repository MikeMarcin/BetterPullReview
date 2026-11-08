#include "bpr/application/review_mutation_coordinator.hpp"
#include "github_mutation_contract.hpp"
#include "github_mutation_contract_support.hpp"
#include "github_paged_read.hpp"
#include "review_comment_permalink.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::github::detail {
	namespace {
		[[nodiscard]] auto same_attempt(
			const application::review_mutation_recovery_record& aLeft,
			const application::review_mutation_recovery_record& aRight) noexcept
			-> bool
		{
			return aLeft.IdempotencyKey == aRight.IdempotencyKey
				&& aLeft.RequestFingerprint == aRight.RequestFingerprint
				&& aLeft.AuthorityFingerprint == aRight.AuthorityFingerprint
				&& aLeft.Body == aRight.Body && aLeft.Anchor == aRight.Anchor;
		}

		[[nodiscard]] auto exact_comment_ids(
			const review_mutation_server_reconciliation& aObservation)
			-> std::vector<std::string>
		{
			auto result = std::vector<std::string>{ };
			result.reserve(aObservation.ExactReceipts.size());
			std::ranges::transform(
				aObservation.ExactReceipts,
				std::back_inserter(result),
				&application::review_mutation_receipt::ServerCommentId);
			return result;
		}

		[[nodiscard]] auto new_exact_receipt(
			const review_mutation_server_reconciliation& aObservation,
			const std::vector<std::string>&              aBaseline)
			-> std::optional<application::review_mutation_receipt>
		{
			auto result = std::optional<application::review_mutation_receipt>{ };
			for (const auto& receipt : aObservation.ExactReceipts) {
				if (std::ranges::binary_search(aBaseline, receipt.ServerCommentId)) {
					continue;
				}
				if (result.has_value()) {
					return std::nullopt;
				}
				result = receipt;
			}
			return result;
		}

		/// [[BPR-0879]]. A reply is addressed by a thread identity exactly as a
		/// resolution is, so it is observed by exactly the same read. The read
		/// document names no state, so `Resolve` is carried unread.
		[[nodiscard]] auto thread_observation_subject(
			const application::review_mutation_request& aRequest)
			-> application::review_thread_resolution_request
		{
			return {
				.Target         = aRequest.Target,
				.ActingIdentity = aRequest.ActingIdentity,
				.Thread         = aRequest.Proposal.ReplyToThread.value_or(std::string{ }),
				.Resolve        = true,
				.LiveBinding    = aRequest.LiveBinding,
			};
		}

		/// \brief [[BPR-1560]]. Whether the comment sits on a LEFT line.
		/// [[BPR-1706]]. A left line published as a file comment names no LEFT line.
		[[nodiscard]] auto publishes_left(const application::review_mutation_request& aRequest) noexcept -> bool
		{
			return !aRequest.Proposal.ReplyToThread && aRequest.Proposal.Anchor.Side == core::diff_side::left
				&& !(aRequest.Source && aRequest.Source->Placement == core::review_placement::unresolvable_line);
		}

		[[nodiscard]] auto publication_failure(const application::review_mutation_failure aFailure = application::review_mutation_failure::unavailable) -> application::review_mutation_submission
		{
			return { .Result = std::unexpected(aFailure) };
		}

		[[nodiscard]] auto publication_now() -> std::uint64_t
		{
			return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		}

		[[nodiscard]] auto capability_key(const live_review_mutation_configuration& aConfig, const application::review_mutation_request& aRequest) -> application::review_publication_capability_key
		{
			const auto& authority = aConfig.CredentialContext.Authority;
			auto        anchor    = std::string{ "bpr-capability-anchor-v1" };
			append_fingerprint_text(anchor, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
			append_fingerprint_text(anchor, aRequest.LiveBinding->Anchor.Path);
			append_fingerprint_integer(anchor, application::remote_line(aRequest.LiveBinding->Anchor));
			append_fingerprint_oid(anchor, aRequest.LiveBinding->Anchor.Commit);
			return { .Profile = authority.Profile, .ProfileRevision = authority.ProfileRevision, .Host = authority.Host, .Hostname = authority.Hostname, .ServerContract = authority.ServerContract, .Actor = aConfig.ActingIdentity, .RepositoryDatabaseId = aConfig.Target.RepositoryDatabaseId, .RepositoryNodeId = aConfig.Target.RepositoryNodeId, .ClassifierRevision = off_hunk_classifier_revision, .FreshnessGeneration = aConfig.CredentialContext.IdentityGeneration.value(), .AnchorDigest = text_digest(anchor) };
		}

		[[nodiscard]] auto capability_scope_digest(const application::review_mutation_request& aRequest, const live_review_mutation_configuration& aConfig) -> std::array<std::uint8_t, 32>
		{
			auto material = std::string{ "bpr-off-hunk-evidence-v1" };
			append_fingerprint_text(material, digest_hex(canonical_authority_fingerprint(*aRequest.LiveBinding)));
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.Profile.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.Authority.ProfileRevision.value());
			append_fingerprint_integer(material, aConfig.CredentialContext.IdentityGeneration.value());
			append_fingerprint_integer(material, 20221128U);
			append_fingerprint_integer(material, off_hunk_classifier_revision);
			append_fingerprint_text(material, digest_hex(capability_key(aConfig, aRequest).AnchorDigest));
			return text_digest(material);
		}

		[[nodiscard]] auto prepared_child(const application::review_mutation_request& aRequest, const std::string& aParent, const review_mutation_server_reconciliation& aBaseline) -> application::review_mutation_recovery_record
		{
			const auto wire = compile_review_comment_request(aRequest, aRequest.LiveBinding->Target.ServerContract);
			return { .IdempotencyKey          = application::review_publication_child_key(aParent, aRequest.Representation),
					 .RequestFingerprint      = canonical_write_fingerprint(*wire),
					 .AuthorityFingerprint    = canonical_authority_fingerprint(*aRequest.LiveBinding),
					 .Body                    = *publication_body(aRequest),
					 .Anchor                  = aRequest.LiveBinding->Anchor,
					 .BaselineExactCommentIds = exact_comment_ids(aBaseline),
					 .Transmission            = application::review_mutation_recovery_transmission::prepared };
		}

		[[nodiscard]] auto file_child_state(const publication_state aState) -> bool
		{
			return aState == publication_state::fallback_prepared || aState == publication_state::fallback_possibly_transmitted || aState == publication_state::succeeded_fallback;
		}

		[[nodiscard]] auto selected_request(application::review_mutation_request aRequest, const application::review_publication_recovery_record& aRecord) -> application::review_mutation_request
		{
			aRequest.Representation = file_child_state(aRecord.State) ? representation::file_comment_with_permalink : representation::native_line;
			return aRequest;
		}

		[[nodiscard]] auto child_matches(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord) -> bool
		{
			const auto  wire  = compile_review_comment_request(aRequest, aRequest.LiveBinding->Target.ServerContract);
			const auto& child = file_child_state(aRecord.State) ? aRecord.Fallback : aRecord.Native;
			return wire && child && child->RequestFingerprint == canonical_write_fingerprint(*wire)
				&& child->AuthorityFingerprint == canonical_authority_fingerprint(*aRequest.LiveBinding)
				&& child->Body == publication_body(aRequest) && child->Anchor == aRequest.LiveBinding->Anchor;
		}

		[[nodiscard]] auto resolved_publication(application::review_publication_recovery_record aRecord, application::review_mutation_receipt aReceipt) -> application::review_publication_recovery_record
		{
			const bool file         = file_child_state(aRecord.State);
			auto&      child        = file ? aRecord.Fallback : aRecord.Native;
			aReceipt.Representation = file ? representation::file_comment_with_permalink : representation::native_line;
			child->State            = application::review_mutation_recovery_state::succeeded;
			child->Receipt          = std::move(aReceipt);
			aRecord.State           = file ? publication_state::succeeded_fallback : publication_state::succeeded_native;
			return aRecord;
		}

		/// \brief [[BPR-1735]]. The record with its last sent child rejected.
		[[nodiscard]] auto rejected_publication(application::review_publication_recovery_record aRecord) -> application::review_publication_recovery_record
		{
			aRecord.State = publication_state::rejected;
			return aRecord;
		}

		/// \brief [[BPR-1735]]. Whether GitHub's complete comment list holds
		/// no exact comment of this publication outside the child's baseline:
		/// not one, where `new_exact_receipt` also answers none for several.
		[[nodiscard]] auto no_new_exact_comment(const review_mutation_server_reconciliation& aObserved, const std::vector<std::string>& aBaseline) -> bool
		{
			return std::ranges::all_of(aObserved.ExactReceipts, [&aBaseline](const application::review_mutation_receipt& aReceipt) {
				return std::ranges::binary_search(aBaseline, aReceipt.ServerCommentId);
			});
		}

		/// \brief [[BPR-1735]]. What a child's POST answered, by class only:
		/// an answer's status, or the status an error carried; no answer
		/// otherwise.
		[[nodiscard]] auto kept_answer(const std::expected<review_mutation_wire_response, application::review_mutation_failure>& aResponse, const std::uint16_t aErrorStatus,
									   const std::int64_t aNow) -> core::review_publication_post
		{
			const auto status = aResponse ? aResponse->Status : static_cast<int>(aErrorStatus);
			const auto klass  = status >= 200 && status <= 599 ? static_cast<std::uint8_t>(status / 100) : std::uint8_t{ 0U };
			if (aResponse && klass == 2U) {
				return { .Answer = core::review_publication_answer::success, .StatusClass = klass, .RecordedUtcMs = aNow };
			}
			if (klass >= 3U) {
				return { .Answer = core::review_publication_answer::error_status, .StatusClass = klass, .RecordedUtcMs = aNow };
			}
			return { .Answer = core::review_publication_answer::no_answer, .StatusClass = 0U, .RecordedUtcMs = aNow };
		}
	}

	auto review_mutation_session::recover_publication(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, const std::stop_token aStop) -> application::review_mutation_submission
	{
		const auto request = selected_request(aRequest, aRecord);
		if (!child_matches(request, aRecord)) {
			return publication_failure();
		}
		const auto& child = file_child_state(aRecord.State) ? aRecord.Fallback : aRecord.Native;
		if (child->Receipt) {
			auto receipt      = *child->Receipt;
			receipt.Duplicate = true;
			return { .Result = receipt };
		}
		if (child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
			return publication_failure();
		}
		if (const auto exact = observe_exact(aStop); !exact) {
			return publication_failure(exact.error());
		}
		const auto observed = reconcile(request, aStop);
		if (!observed) {
			return publication_failure(observed.error());
		}
		auto result  = application::review_mutation_submission{ .Reconciliation = observed->Projection };
		auto receipt = new_exact_receipt(*observed, *child->BaselineExactCommentIds);
		if (!receipt) {
			// [[BPR-1735]]. GitHub's complete list holds no comment of it, and
			// the kept answer settles it: a 4xx fully received, or a record an
			// earlier version left, ten minutes after it was migrated. Any
			// other answer stays unknown and only reconciles.
			if (no_new_exact_comment(*observed, *child->BaselineExactCommentIds)) {
				const auto post = Recovery->load_publication_post(aRecord.Key);
				if (post && core::publication_definitely_unpublished(*post, PublicationClock ? PublicationClock() : utc_now_ms())
					&& Recovery->advance_publication(aRecord, rejected_publication(aRecord))) {
					result.Result = std::unexpected((**post).Answer == core::review_publication_answer::kept_before_answers ? application::review_mutation_failure::settled_not_published
																															: application::review_mutation_failure::refused_not_published);
				}
			}
			return result;
		}
		receipt->Duplicate  = true;
		const auto resolved = resolved_publication(aRecord, *receipt);
		if (Recovery->advance_publication(aRecord, resolved)) {
			result.Result = *(file_child_state(resolved.State) ? resolved.Fallback : resolved.Native)->Receipt;
		}
		return result;
	}

	auto review_mutation_session::execute_publication_child(const application::review_mutation_request& aRequest, const application::review_publication_recovery_record& aRecord, const std::stop_token aStop) -> application::review_mutation_submission
	{
		const auto request = selected_request(aRequest, aRecord);
		if (!child_matches(request, aRecord)) {
			return publication_failure();
		}
		if (const auto exact = observe_exact(aStop, publishes_left(request)); !exact) {
			return publication_failure(exact.error());
		}
		if (aStop.stop_requested()) {
			return publication_failure(application::review_mutation_failure::cancelled);
		}
		const auto wire        = compile_review_comment_request(request, Configuration.Target.ServerContract);
		auto       result      = application::review_mutation_submission{ .MutationAttempted = true };
		auto       witnessed   = std::make_shared<std::atomic_bool>(false);
		const auto response    = perform(*wire, aStop, [this, aRecord, witnessed] {
			if (witnessed->exchange(true, std::memory_order_acq_rel)) {
				return false;
			}
			auto       next                                    = aRecord;
			const bool file                                    = file_child_state(next.State);
			(file ? next.Fallback : next.Native)->Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
			next.State                                         = file ? publication_state::fallback_possibly_transmitted : publication_state::native_possibly_transmitted;
			if (!Recovery->advance_publication(aRecord, next)) {
				return false;
			}
			// [[BPR-1735]]. No answer yet, and since when. Best effort: a
			// child with no answer kept only reconciles, as before.
			static_cast<void>(Recovery->note_publication_post(aRecord.Key, { .Answer = core::review_publication_answer::no_answer, .StatusClass = 0U, .RecordedUtcMs = PublicationClock ? PublicationClock() : utc_now_ms() }));
			return true;
		});
		const auto errorStatus = LastErrorStatus;
		const auto loaded      = Recovery->load_publication(aRecord.Key);
		if (!loaded || !*loaded) {
			return result;
		}
		const auto& current = **loaded;
		const bool  file    = file_child_state(current.State);
		const auto& child   = file ? current.Fallback : current.Native;
		if (!child || child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted) {
			// [[BPR-1572]]. Never sent: the transport's refusal (the GitHub CLI
			// account wrapper finding the pull request moved) is the answer.
			result.MutationAttempted = witnessed->load();
			if (!response) {
				result.Result = std::unexpected(response.error());
			}
			return result;
		}
		// [[BPR-1736]]. A fully received refusal naming the line of a comment
		// the core placed outside the diff teaches at once that this server
		// refuses such a line, whatever the comment list shows next.
		const auto coreOffHunk = !file && aRequest.Source->CorePlacedOffHunk && aRequest.Source->Placement == core::review_placement::off_hunk;
		const auto namesLine   = coreOffHunk && response && refusal_names_comment_line(*response);
		if (namesLine) {
			learn_off_hunk(aRequest, *response, core::off_hunk_answer::refused_naming_line);
		}
		// [[BPR-1736]] (D4). Such a line's refusal in GitHub.com's measured
		// shape is classified for any repository, as the designated PR's is.
		const auto contract = UnsupportedContract ? UnsupportedContract : recorded_off_hunk_contract(Configuration);
		const auto general  = coreOffHunk ? general_off_hunk_contract(Configuration.Target.ServerContract) : std::nullopt;
		if (!file && response && aRequest.Source->Placement != core::review_placement::legacy_line
			&& ((contract && definitive_off_hunk_unsupported(*response, *wire, *contract)) || (general && definitive_off_hunk_unsupported(*response, *wire, *general)))) {
			auto rejected          = current;
			rejected.State         = publication_state::native_definitively_unsupported;
			rejected.Authorization = { .Source         = application::review_fallback_authorization_source::native_rejection,
									   .ContractDigest = text_digest(response->Body),
									   .ScopeDigest    = capability_scope_digest(aRequest, Configuration) };
			if (!Recovery->advance_publication(current, rejected)) {
				return result;
			}
			const auto now = publication_now();
			static_cast<void>(PublicationCapabilities.record(capability_key(Configuration, aRequest), { .State = core::off_hunk_creation_capability::unsupported, .Source = application::review_publication_evidence_source::observed_definitive_unsupported, .ContractDigest = rejected.Authorization.ContractDigest, .ObservedAt = now, .ExpiresAt = now + application::review_publication_capability_cache::maximum_lifetime_seconds }, now));
			return result;
		}
		// [[BPR-1735]]. What GitHub answered, kept with the child, so a later
		// check or restart can settle a refusal it could not confirm now. An
		// exact unsupported rejection above is the classifier's, not this.
		const auto answer  = kept_answer(response, errorStatus, PublicationClock ? PublicationClock() : utc_now_ms());
		const auto refused = answer.Answer == core::review_publication_answer::error_status && answer.StatusClass == 4U;
		static_cast<void>(Recovery->note_publication_post(current.Key, answer));
		std::optional<application::review_mutation_receipt> receipt;
		auto                                                parsedReceipt = false;
		if (response) {
			const auto parsed = parse_review_comment_response(*response, request);
			if (parsed) {
				receipt       = parsed->Receipt;
				parsedReceipt = true;
			} else {
				result.Result = std::unexpected(map_contract_failure(parsed.error()));
			}
		} else {
			result.Result = std::unexpected(response.error());
		}
		// A witnessed write reconciles once even when the gesture was cancelled.
		const auto observed = reconcile(request, { });
		if (observed) {
			result.Reconciliation = observed->Projection;
		}
		if (receipt && std::ranges::binary_search(*child->BaselineExactCommentIds, receipt->ServerCommentId)) {
			receipt.reset();
		}
		if (!receipt && observed) {
			receipt = new_exact_receipt(*observed, *child->BaselineExactCommentIds);
		}
		if (!receipt) {
			// [[BPR-1735]]. A fully received 4xx, and GitHub's complete list
			// holds no comment of it: GitHub validates a comment before it
			// creates one, so this child is rejected and its draft kept, to be
			// published again. A refusal not yet confirmed by the list stays
			// unknown and settles on a later check.
			if (refused && observed && no_new_exact_comment(*observed, *child->BaselineExactCommentIds)
				&& Recovery->advance_publication(current, rejected_publication(current))) {
				// [[BPR-1736]]. A refusal of a line the core placed outside the
				// diff that no classifier recognized: the fact now says such a
				// line is refused, so publishing again posts its file comment.
				const auto unplaceable = response && refusal_names_unplaceable_line(response->Body);
				result.Result          = std::unexpected(namesLine         ? application::review_mutation_failure::off_hunk_refused
															 : unplaceable ? application::review_mutation_failure::unplaceable_line
																		   : application::review_mutation_failure::refused_not_published);
			}
			return result;
		}
		const auto resolved = resolved_publication(current, *receipt);
		if (!Recovery->advance_publication(current, resolved)) {
			return result;
		}
		result.Result = *(file ? resolved.Fallback : resolved.Native)->Receipt;
		// [[BPR-1736]]. GitHub created the comment the core placed outside the
		// diff, at that line, and answered it exactly: this server accepts such
		// a line. A receipt found only by the list names no server version.
		if (coreOffHunk && response && response->Status == 201 && parsedReceipt) {
			learn_off_hunk(aRequest, *response, core::off_hunk_answer::created);
		}
		if (!file && aRequest.Source->Placement == core::review_placement::off_hunk) {
			const auto now = publication_now();
			static_cast<void>(PublicationCapabilities.record(capability_key(Configuration, aRequest), { .State = core::off_hunk_creation_capability::supported, .Source = application::review_publication_evidence_source::observed_native_success, .ContractDigest = child->RequestFingerprint, .ObservedAt = now, .ExpiresAt = now + application::review_publication_capability_cache::maximum_lifetime_seconds }, now));
		}
		return result;
	}

	auto review_mutation_session::submit_publication(const application::review_mutation_request& aRequest, const std::stop_token aStop) -> application::review_mutation_submission
	{
		// [[BPR-1736]] (RFC-BPR-0022 invariant 2). A line the core placed
		// outside the diff, on a server this profile learned refuses such a
		// line, is the file comment from the start: no native attempt.
		if (auto file = learned_file_comment(aRequest, aStop)) {
			return submit_publication(*file, aStop);
		}
		const auto& source    = *aRequest.Source;
		const auto& authority = Configuration.CredentialContext.Authority;
		// [[BPR-1212]]. A new comment on the base side is GitHub's `side: LEFT`
		// on the head's path. It has no permalink form, so its source is held
		// to the same comparison, head and line count directly, and it never
		// falls back to a file comment.
		// [[BPR-1706]]. A commit-filtered diff's line GitHub resolves nowhere is
		// a file comment from the start, at the head's path with commit_id =
		// head, its permalink naming the line at the diff's commit on its side.
		const auto unresolvable = source.Placement == core::review_placement::unresolvable_line;
		const auto onBase       = aRequest.Proposal.Anchor.Side == core::diff_side::left && !aRequest.Proposal.ReplyToThread;
		// [[BPR-1736]]. A base line the core placed outside the diff has a file
		// comment linking to it in the merge base, should GitHub refuse it.
		const auto leftFileable = source.CorePlacedOffHunk && source.Placement == core::review_placement::off_hunk && source.LeftEndpoint.has_value();
		const auto sourceFound  = onBase && !unresolvable
			? aRequest.Representation == core::review_publication_representation::native_line && source.Endpoint
				&& aRequest.Proposal.Anchor.Comparison == core::comparison_digest(source.Comparison) && aRequest.Proposal.Anchor.Line < source.LineCount
				&& source.Endpoint->request().Diff.Comparison.Right.Commit == Configuration.Target.Semantic.ExpectedHead
			: compile_review_permalink_body(permalink_source(source, Configuration.Target, aRequest.Proposal.Anchor.Side), aRequest.Proposal.Anchor, "Source validation").has_value();
		// RFC-BPR-0089 M4. A line the host placed is a commit-filtered diff's,
		// published natively as one line: on the head, or on that diff's own
		// right commit, which the epoch host authorized as a commit of the
		// pull request's first-parent chain.
		const auto& anchor   = aRequest.LiveBinding->Anchor;
		const auto  placedOk = !anchor.RemoteLine
			|| (source.Comparison.Filter && source.Placement == core::review_placement::legacy_line
				&& aRequest.Representation == core::review_publication_representation::native_line && !aRequest.Proposal.StartLine
				&& !aRequest.Proposal.ReplyToThread
				&& (anchor.Commit == Configuration.Target.Semantic.ExpectedHead
					|| (anchor.Semantic.Side == core::diff_side::right && source.Comparison.Filter->Right == anchor.Commit)));
		// [[BPR-1735]]. On the Whole PR diff too: the core placed the line
		// outside every hunk of the merge base to the head.
		const auto unresolvableOk = !unresolvable
			|| (!anchor.RemoteLine && anchor.Commit == Configuration.Target.Semantic.ExpectedHead
				&& aRequest.Representation == core::review_publication_representation::native_line && !aRequest.Proposal.StartLine && !aRequest.Proposal.ReplyToThread);
		if (!source.Endpoint || source.Placement > core::review_placement::unresolvable_line || !placedOk || !unresolvableOk
			|| source.Comparison.Authority.Profile != authority.Profile || source.Comparison.Authority.ProfileRevision.value() != authority.ProfileRevision.value()
			|| source.Comparison.Authority.Account != authority.Account || source.Endpoint->path().generic_string() != aRequest.LiveBinding->Anchor.Path
			|| !sourceFound) {
			return publication_failure(application::review_mutation_failure::undesignated_target);
		}
		const auto fingerprint = publication_fingerprint(aRequest, Configuration);
		const auto key         = digest_hex(fingerprint);
		auto       loaded      = Recovery->load_publication(key);
		if (!loaded) {
			return publication_failure();
		}
		auto current = *loaded;
		if (current && (current->Fingerprint != fingerprint || current->LegacyKey != aRequest.IdempotencyKey)) {
			return publication_failure();
		}
		auto result       = publication_failure();
		bool ownsPrepared = false;
		// At most one native child and one fallback child. Every resume reloads durable authority.
		for (unsigned step = 0U; step < 4U; ++step) {
			if (aStop.stop_requested()) {
				result.Result = std::unexpected(application::review_mutation_failure::cancelled);
				return result;
			}
			const auto evidence    = PublicationCapabilities.lookup(capability_key(Configuration, aRequest), publication_now());
			const auto observation = core::review_publication_observation{ .Placement         = source.Placement,
																		   .Capability        = evidence ? evidence->State : core::off_hunk_creation_capability::unknown,
																		   .Body              = aRequest.Proposal.StartLine || (onBase && !unresolvable && !leftFileable) ? core::review_body_compatibility::requires_line_subject : core::classify_review_body(aRequest.Proposal.Body),
																		   .State             = current ? current->State : publication_state::initial,
																		   .Eligible          = true,
																		   .CapabilityCurrent = evidence.has_value() };
			const auto plan        = core::plan_review_publication(observation);
			using action           = core::review_publication_action;
			if (plan.Action == action::refuse) {
				result.Result = std::unexpected(plan.Diagnostic == core::review_publication_diagnostic::incompatible_body ? application::review_mutation_failure::incompatible_body : application::review_mutation_failure::unsupported);
				return result;
			}
			if (plan.Action == action::already_succeeded || plan.Action == action::reconcile_native || plan.Action == action::reconcile_fallback) {
				return recover_publication(aRequest, *current, aStop);
			}
			// [[BPR-1735]]. A rejected publication starts again as a new attempt
			// under its own key: GitHub holds no comment of it.
			if (!current || current->State == publication_state::rejected) {
				if (const auto exact = observe_exact(aStop, publishes_left(aRequest)); !exact) {
					return publication_failure(exact.error());
				}
				auto next = application::review_publication_recovery_record{ .Key = key, .LegacyKey = aRequest.IdempotencyKey, .Fingerprint = fingerprint };
				if (plan.Action == action::admit_unsupported_evidence) {
					next.State         = publication_state::unsupported_evidence_admitted;
					next.Authorization = { .Source = application::review_fallback_authorization_source::scoped_evidence, .ContractDigest = evidence->ContractDigest, .ScopeDigest = capability_scope_digest(aRequest, Configuration), .Generation = capability_key(Configuration, aRequest).FreshnessGeneration, .ObservedAt = evidence->ObservedAt, .ExpiresAt = evidence->ExpiresAt };
				} else if (plan.Action == action::admit_unresolvable_line) {
					// [[BPR-1706]]. The admission binds the exact file comment it
					// authorizes and the authority it is written under.
					auto file           = aRequest;
					file.Representation = representation::file_comment_with_permalink;
					const auto wire     = compile_review_comment_request(file, Configuration.Target.ServerContract);
					if (!wire) {
						return publication_failure(application::review_mutation_failure::incompatible_body);
					}
					next.State         = publication_state::unresolvable_line_admitted;
					next.Authorization = { .Source = application::review_fallback_authorization_source::unresolvable_line, .ContractDigest = canonical_write_fingerprint(*wire), .ScopeDigest = canonical_authority_fingerprint(*aRequest.LiveBinding) };
				} else {
					const auto baseline = reconcile(aRequest, aStop);
					if (!baseline) {
						return publication_failure(baseline.error());
					}
					next.State  = publication_state::native_prepared;
					next.Native = prepared_child(aRequest, key, *baseline);
				}
				const auto localBody = publication_body(aRequest);
				if (aRequest.Proposal.Durable && (!Lineage || !localBody || !Lineage->prepare(aRequest, key, *localBody))) {
					return publication_failure();
				}
				const auto saved = Recovery->advance_publication(current, next);
				if (!saved) {
					return publication_failure();
				}
				current      = *saved;
				ownsPrepared = next.State == publication_state::native_prepared;
				if (!ownsPrepared) {
					continue;
				}
			} else if (plan.Action == action::fallback_attempt
					   && (current->State == publication_state::native_definitively_unsupported || current->State == publication_state::unsupported_evidence_admitted
						   || current->State == publication_state::unresolvable_line_admitted)) {
				if (const auto exact = observe_exact(aStop, publishes_left(aRequest)); !exact) {
					result.Result = std::unexpected(exact.error());
					return result;
				}
				auto request           = aRequest;
				request.Representation = representation::file_comment_with_permalink;
				if (!compile_review_comment_request(request, Configuration.Target.ServerContract)) {
					result.Result = std::unexpected(application::review_mutation_failure::incompatible_body);
					return result;
				}
				const auto baseline = reconcile(request, aStop);
				if (!baseline) {
					result.Result = std::unexpected(baseline.error());
					return result;
				}
				auto next            = *current;
				next.State           = publication_state::fallback_prepared;
				next.Fallback        = prepared_child(request, key, *baseline);
				const auto localBody = publication_body(request);
				if (request.Proposal.Durable && (!Lineage || !localBody || !Lineage->prepare(request, key, *localBody))) {
					return publication_failure();
				}
				const auto saved = Recovery->advance_publication(current, next);
				if (!saved) {
					return result;
				}
				current      = *saved;
				ownsPrepared = true;
			}
			if (!ownsPrepared) {
				return result;
			}
			const auto attempted       = execute_publication_child(aRequest, *current, aStop);
			const bool previousAttempt = result.MutationAttempted;
			result                     = attempted;
			result.MutationAttempted   = result.MutationAttempted || previousAttempt;
			loaded                     = Recovery->load_publication(key);
			if (!loaded || !*loaded) {
				return result;
			}
			current      = **loaded;
			ownsPrepared = false;
			if (current->State != publication_state::native_definitively_unsupported) {
				return result;
			}
		}
		return result;
	}

	auto review_mutation_session::finish_lineage(const application::review_mutation_request& aRequest, application::review_mutation_submission aResult, std::stop_token aStop) -> application::review_mutation_submission
	{
		if (!aRequest.Proposal.Durable) {
			return aResult;
		}
		aResult.Lineage = application::review_lineage_state::pending;
		if (!aResult.Result || !Lineage) {
			return aResult;
		}
		auto request           = aRequest;
		request.Representation = aResult.Result->Representation;
		const auto observed    = reconcile(request, aStop);
		if (!observed) {
			return aResult;
		}
		aResult.Reconciliation = observed->Projection;
		const auto exact       = std::ranges::any_of(observed->ExactReceipts, [&](const auto& aReceipt) { return aReceipt.ServerCommentId == aResult.Result->ServerCommentId; });
		if (!exact) {
			return aResult;
		}
		const auto logical   = digest_hex(publication_fingerprint(aRequest, Configuration));
		const auto localBody = publication_body(request);
		if (localBody && Lineage->complete(request, logical, *localBody, *aResult.Result)) {
			aResult.Lineage = application::review_lineage_state::recorded;
		}
		return aResult;
	}

	auto review_mutation_session::reconcile_publication(const application::review_mutation_request& aRequest, std::stop_token aStop) -> application::review_mutation_submission
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return publication_failure(operation.failure());
			}
			if (!Recovery || !Lease || !Lease->active() || aRequest.Representation != core::review_publication_representation::native_line || !aRequest.Source || !aRequest.LiveBinding || !application::valid(*aRequest.LiveBinding) || aRequest.LiveBinding->Target != Configuration.Target || aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity || aRequest.Proposal.ReplyToThread || !compile_review_comment_request(aRequest, Configuration.Target.ServerContract)) {
				return publication_failure();
			}
			const auto fingerprint = publication_fingerprint(aRequest, Configuration);
			auto       loaded      = Recovery->load_publication(digest_hex(fingerprint));
			if (!loaded || !*loaded || (**loaded).Fingerprint != fingerprint || (**loaded).LegacyKey != aRequest.IdempotencyKey) {
				return publication_failure();
			}
			return finish_lineage(aRequest, recover_publication(aRequest, **loaded, aStop), aStop);
		} catch (...) {
			return publication_failure();
		}
	}

	void review_mutation_session::repair_pending_publications(const std::stop_token aStop)
	{
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return;
			}
			if (!Lineage || !Recovery || !Lease || !Lease->active() || !valid(Configuration) || aStop.stop_requested()) {
				return;
			}
			auto rows = Lineage->pending(Configuration.Target.Semantic, Configuration.ActingIdentity.Account);
			if (rows.empty() || !observe_exact(aStop)) {
				return;
			}
			auto wire = compile_review_comment_collection_request(Configuration.Target, Configuration.Target.ServerContract);
			if (!wire) {
				return;
			}
			auto response = perform(*wire, aStop);
			if (!response || response->Status != 200 || !response_headers_valid(*response) || has_header(*response, "Link")) {
				return;
			}
			auto comments = document<std::vector<comment_response_json>>(*response);
			if (!comments || comments->size() > 100U) {
				return;
			}
			for (const auto& row : rows) {
				const auto& scope = row.Target.Binding.Scope;
				if (scope.Host != Configuration.Target.Semantic.Host || scope.Repository != Configuration.Target.Semantic.Repository
					|| scope.PullRequest != Configuration.Target.Semantic.PullRequest || row.Actor != Configuration.ActingIdentity.Account
					|| row.ActorNode != Configuration.ActingIdentity.NodeId || row.ActorDatabaseId != Configuration.ActingIdentity.DatabaseId
					|| row.ActorLogin != Configuration.ActingIdentity.Login || row.ServerComment) {
					continue;
				}
				auto loaded = Recovery->load_publication(row.LogicalPublication);
				if (!loaded || !*loaded || (**loaded).Key != row.LogicalPublication || (**loaded).LegacyKey != row.IdempotencyKey) {
					continue;
				}
				const auto& journal = **loaded;
				const bool  file    = file_child_state(journal.State);
				const auto& child   = file ? journal.Fallback : journal.Native;
				if (!child || row.WireBodyDigest != core::private_digest(child->Body)
					|| (file ? row.Representation != "file_comment_with_permalink" : row.Representation != "native_line")
					|| (child->Transmission != application::review_mutation_recovery_transmission::possibly_transmitted && !child->Receipt)
					|| (!child->Receipt && !child->BaselineExactCommentIds)) {
					continue;
				}
				const auto                                          commit       = core::canonical_hex(row.AcceptedHead);
				const auto                                          expectedSide = row.Target.Line.Side == core::diff_side::left ? "LEFT" : "RIGHT";
				std::optional<application::review_mutation_receipt> observed;
				bool                                                ambiguous = false;
				for (const auto& comment : *comments) {
					const bool fileComment = comment.SubjectType == "file";
					const auto currentLine = comment.CommitId == commit.view() && comment.Line
						&& *comment.Line == static_cast<std::uint64_t>(row.Target.Line.Line) + 1U;
					const auto originalLine = comment.OriginalCommitId == commit.view()
						&& comment.OriginalLine && *comment.OriginalLine == static_cast<std::uint64_t>(row.Target.Line.Line) + 1U;
					if (comment.NodeId.empty() || comment.Id == 0 || comment.Path != row.Path
						|| comment.Body != child->Body || comment.User.Id != row.ActorDatabaseId
						|| comment.User.NodeId != row.ActorNode || comment.User.Login != row.ActorLogin || fileComment != file
						|| comment.InReplyToId
						|| (!file && (!(currentLine || originalLine) || comment.Side != expectedSide || comment.StartLine || comment.StartSide))
						|| (file && comment.CommitId != commit.view())
						|| (file && !valid_file_subject_metadata(comment))
						|| (child->BaselineExactCommentIds && std::ranges::binary_search(*child->BaselineExactCommentIds, comment.NodeId))) {
						continue;
					}
					if (observed) {
						ambiguous = true;
						break;
					}
					observed = application::review_mutation_receipt{ .ServerCommentId = comment.NodeId, .Duplicate = true, .Representation = file ? representation::file_comment_with_permalink : representation::native_line };
				}
				if (ambiguous || !observed || (child->Receipt && child->Receipt->ServerCommentId != observed->ServerCommentId)) {
					continue;
				}
				if (!child->Receipt) {
					auto resolved = resolved_publication(journal, *observed);
					if (!Recovery->advance_publication(journal, resolved)) {
						continue;
					}
				}
				static_cast<void>(Lineage->complete_observed(row, child->Body, *observed));
			}
		} catch (...) {
			// A later read can repair the same pending row; this path never writes remotely.
		}
	}

	auto review_mutation_session::submit_and_reconcile(
		const application::review_mutation_request& aRequest,
		const std::stop_token                       aStop)
		-> application::review_mutation_submission
	{
		const auto failure = [](const application::review_mutation_failure aFailure) {
			return application::review_mutation_submission{
				.Result = std::unexpected(aFailure),
			};
		};
		try {
			std::scoped_lock                lock{ Mutex };
			github_mutation_operation_scope operation{ Transport, aStop };
			if (!operation.valid()) {
				return publication_failure(operation.failure());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			if (Lease == nullptr || !Lease->active() || Recovery == nullptr) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!valid(Configuration) || !aRequest.LiveBinding.has_value()
				|| !application::valid(*aRequest.LiveBinding)
				|| aRequest.Representation != core::review_publication_representation::native_line
				|| aRequest.LiveBinding->Target != Configuration.Target
				|| aRequest.LiveBinding->ActingIdentity != Configuration.ActingIdentity
				// RFC-BPR-0089 M4. A placed line is published only from its
				// filtered diff's source, through the logical journal.
				|| (aRequest.LiveBinding->Anchor.RemoteLine && (!aRequest.Source || aRequest.Proposal.ReplyToThread))) {
				return failure(application::review_mutation_failure::undesignated_target);
			}
			const auto wire = compile_review_comment_request(
				aRequest,
				Configuration.CredentialContext.Authority.ServerContract);
			if (!wire.has_value()) {
				return failure(map_contract_failure(wire.error()));
			}
			const auto fingerprint = canonical_write_fingerprint(*wire);
			const auto expected    = application::review_mutation_recovery_record{
				.IdempotencyKey       = aRequest.IdempotencyKey,
				.RequestFingerprint   = fingerprint,
				.AuthorityFingerprint = canonical_authority_fingerprint(
					*aRequest.LiveBinding),
				.Body   = aRequest.Proposal.Body,
				.Anchor = aRequest.LiveBinding->Anchor,
			};
			if (!application::valid(expected)) {
				return failure(application::review_mutation_failure::unavailable);
			}

			const auto finishKnown = [&](const application::review_mutation_recovery_record& aKnown)
				-> application::review_mutation_submission {
				if (!application::valid(aKnown) || !same_attempt(aKnown, expected)) {
					return failure(application::review_mutation_failure::unavailable);
				}
				if (aKnown.State == application::review_mutation_recovery_state::succeeded
					&& aKnown.Receipt.has_value()) {
					auto duplicate      = *aKnown.Receipt;
					duplicate.Duplicate = true;
					return { .Result = std::move(duplicate) };
				}
				if (aKnown.State != application::review_mutation_recovery_state::unresolved
					|| aKnown.Receipt.has_value()
					|| !aKnown.BaselineExactCommentIds.has_value()
					|| aKnown.Transmission
						!= application::review_mutation_recovery_transmission::possibly_transmitted) {
					return failure(application::review_mutation_failure::unavailable);
				}
				const auto current = observe_exact(aStop);
				if (!current.has_value()) {
					return failure(current.error());
				}
				auto observed = reconcile(aRequest, aStop);
				if (!observed.has_value()) {
					return failure(observed.error());
				}
				auto submission = application::review_mutation_submission{
					.Reconciliation = observed->Projection,
				};
				auto receipt = new_exact_receipt(
					*observed, *aKnown.BaselineExactCommentIds);
				if (!receipt.has_value()) {
					return submission;
				}
				receipt->Duplicate  = true;
				const auto resolved = Recovery->resolve(
					aRequest.IdempotencyKey, fingerprint, *receipt);
				if (!resolved.has_value()) {
					return submission;
				}
				submission.Result = std::move(*receipt);
				return submission;
			};

			if (aRequest.Proposal.Durable && (!Lineage || !aRequest.Source || aRequest.Proposal.ReplyToThread || aRequest.Proposal.Durable->DraftRevision == 0 || std::ranges::all_of(aRequest.Proposal.Durable->Incarnation, [](auto aByte) { return aByte == 0; }))) {
				return failure(application::review_mutation_failure::unavailable);
			}
			const auto loaded = Recovery->load(aRequest.IdempotencyKey);
			if (!loaded.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (loaded->has_value()) {
				return finishKnown(**loaded);
			}
			if (aRequest.Source && !aRequest.Proposal.ReplyToThread) {
				return finish_lineage(aRequest, submit_publication(aRequest, aStop), aStop);
			}

			// [[BPR-0879]]. A reply names a thread, and a thread identity says
			// nothing about which pull request it belongs to. Ask before the
			// attempt is admitted, so a refusal leaves no prepared record
			// behind and no write has happened.
			if (aRequest.Proposal.ReplyToThread.has_value()) {
				const auto bound = observe_thread_target(
					thread_observation_subject(aRequest), aStop);
				if (!bound.has_value()) {
					return failure(bound.error());
				}
			}

			auto baseline = reconcile(aRequest, aStop);
			if (!baseline.has_value()) {
				return failure(baseline.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}
			auto admittedRecord                    = expected;
			admittedRecord.BaselineExactCommentIds = exact_comment_ids(*baseline);
			admittedRecord.Transmission
				= application::review_mutation_recovery_transmission::prepared;
			if (!application::valid(admittedRecord)) {
				return failure(application::review_mutation_failure::unavailable);
			}
			const auto admitted = Recovery->admit(admittedRecord);
			if (!admitted.has_value()) {
				return failure(application::review_mutation_failure::unavailable);
			}
			if (!admitted->Inserted) {
				return finishKnown(admitted->Record);
			}
			if (const auto current = observe_exact(aStop, publishes_left(aRequest)); !current.has_value()) {
				return failure(current.error());
			}
			if (aStop.stop_requested()) {
				return failure(application::review_mutation_failure::cancelled);
			}

			auto submission = application::review_mutation_submission{
				.MutationAttempted = true,
			};
			auto witnessed = std::make_shared<std::atomic_bool>(false);
			auto response  = perform(
				*wire,
				aStop,
				[this, witnessed, key = aRequest.IdempotencyKey, fingerprint] {
					if (witnessed->exchange(true, std::memory_order_acq_rel)) {
						return false;
					}
					const auto marked = Recovery->mark_possibly_transmitted(
						key, fingerprint);
					return marked.has_value()
						&& marked->Transmission
						== application::review_mutation_recovery_transmission::possibly_transmitted;
				});
			auto postReceipt = std::optional<application::review_mutation_receipt>{ };
			if (response.has_value()) {
				const auto parsed = parse_review_comment_response(*response, aRequest);
				if (parsed.has_value()) {
					postReceipt = parsed->Receipt;
				} else {
					submission.Result = std::unexpected(map_contract_failure(parsed.error()));
				}
			} else {
				submission.Result = std::unexpected(response.error());
			}
			const auto transmission = Recovery->load(aRequest.IdempotencyKey);
			if (!transmission.has_value() || !transmission->has_value()
				|| (**transmission).Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
				// [[BPR-1572]]. Never witnessed, never sent: a transport's
				// refusal (the pull request moved) is not an attempt.
				submission.MutationAttempted = witnessed->load();
				return submission;
			}

			// Once POST has been invoked its transmission is ambiguous even if the
			// caller cancels. The bounded transport therefore performs this one
			// credentialed GET with an independent token before returning.
			auto observed = reconcile(aRequest, { });
			if (observed.has_value()) {
				submission.Reconciliation = observed->Projection;
			}
			auto confirmed = postReceipt.has_value()
					&& !std::ranges::binary_search(
								 *admittedRecord.BaselineExactCommentIds,
								 postReceipt->ServerCommentId)
				? postReceipt
				: std::nullopt;
			if (!confirmed.has_value() && observed.has_value()) {
				confirmed = new_exact_receipt(
					*observed, *admittedRecord.BaselineExactCommentIds);
			}
			if (!confirmed.has_value()) {
				return submission;
			}
			confirmed->Duplicate = false;
			const auto resolved  = Recovery->resolve(
				aRequest.IdempotencyKey, fingerprint, *confirmed);
			if (!resolved.has_value()) {
				return submission;
			}
			submission.Result = std::move(*confirmed);
			return submission;
		} catch (...) {
			return failure(application::review_mutation_failure::unavailable);
		}
	}
}
