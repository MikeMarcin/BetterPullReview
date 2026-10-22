#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto anchored_draft(std::string aBody) -> core::review_comment_draft
	{
		return core::review_comment_draft{
			.Id     = core::review_draft_id{ 1U },
			.Anchor = core::source_line_ref{
				.Comparison = opaque<core::file_comparison_digest>(3U),
				.Side       = core::diff_side::right,
				.Line       = 42U },
			.Body = std::move(aBody),
		};
	}

	[[nodiscard]] auto command(
		const core::review_draft_command_kind aKind,
		std::string                           aBody = { })
		-> core::review_draft_command
	{
		return core::review_draft_command{
			.Id   = core::review_draft_id{ 1U },
			.Kind = aKind,
			.Body = std::move(aBody),
		};
	}

	using kind = core::review_draft_command_kind;

	void test_a_draft_composes_publishes_and_becomes_shared()
	{
		const auto start = anchored_draft("");
		expect(
			!start.shared() && start.State == core::review_draft_state::drafting,
			"a new draft is private and being composed");

		const auto written
			= core::apply(start, command(kind::edit, "This needs a test."));
		expect(
			written.Changed && written.Draft.Body == "This needs a test."
				&& !written.Draft.shared(),
			"editing sets the body and leaves the draft private");

		const auto begun
			= core::apply(written.Draft, command(kind::begin_publish));
		expect(
			begun.Changed
				&& begun.Draft.State == core::review_draft_state::publishing
				&& !begun.Draft.shared(),
			"a draft in flight is not yet shared");

		const auto done
			= core::apply(begun.Draft, command(kind::complete_publish));
		expect(
			done.Changed && done.Draft.State == core::review_draft_state::published
				&& done.Draft.shared(),
			"a published comment is shared");
	}

	// The interval between issuing a publish and hearing back is observable,
	// and a second publish issued during it must not look legal.
	void test_a_second_publish_while_one_is_in_flight_is_refused()
	{
		const auto begun = core::apply(
			anchored_draft("Ready."),
			command(kind::begin_publish));
		expect(begun.Changed, "the first publish begins");

		const auto again
			= core::apply(begun.Draft, command(kind::begin_publish));
		expect(
			again.Rejection == core::review_draft_rejection::already_publishing
				&& !again.Changed,
			"a second publish during the first is refused");

		const auto edited
			= core::apply(begun.Draft, command(kind::edit, "Changed my mind."));
		expect(
			edited.Rejection == core::review_draft_rejection::already_publishing
				&& edited.Draft.Body == "Ready.",
			"the body cannot change while it is being published");

		const auto discarded
			= core::apply(begun.Draft, command(kind::discard));
		expect(
			discarded.Rejection == core::review_draft_rejection::already_publishing,
			"a draft in flight cannot be discarded out from under the request");
	}

	// A network failure must not cost the reviewer the text they wrote.
	void test_a_failed_publish_returns_the_text_to_the_author()
	{
		const auto begun = core::apply(
			anchored_draft("Worth saying."),
			command(kind::begin_publish));
		const auto failed
			= core::apply(begun.Draft, command(kind::fail_publish));

		expect(
			failed.Rejection == core::review_draft_rejection::none && failed.Changed,
			"a failed publication is a legal transition");
		expect(
			failed.Draft.State == core::review_draft_state::drafting
				&& failed.Draft.Body == "Worth saying." && !failed.Draft.shared(),
			"the draft returns to composition with its text intact");

		const auto retried
			= core::apply(failed.Draft, command(kind::begin_publish));
		expect(retried.Changed, "the reviewer can try again");
	}

	void test_whitespace_is_not_a_comment()
	{
		for (const auto* const body : { "", " ", "\t\n  \r\n" }) {
			const auto refused = core::apply(
				anchored_draft(body),
				command(kind::begin_publish));
			expect(
				refused.Rejection == core::review_draft_rejection::empty_body
					&& !refused.Changed,
				"a body with nothing to read is refused");
		}

		const auto accepted = core::apply(
			anchored_draft("  x  "),
			command(kind::begin_publish));
		expect(
			accepted.Rejection == core::review_draft_rejection::none,
			"a body with one readable character is accepted");
	}

	void test_a_published_comment_is_no_longer_a_draft()
	{
		const auto published = core::apply(
			core::apply(anchored_draft("Said."), command(kind::begin_publish)).Draft,
			command(kind::complete_publish));

		for (const auto attempt :
			 { kind::edit, kind::discard, kind::begin_publish }) {
			const auto refused
				= core::apply(published.Draft, command(attempt, "rewrite"));
			expect(
				refused.Rejection == core::review_draft_rejection::already_published
					&& !refused.Changed,
				"a published comment is GitHub's and no draft command touches it");
		}
	}

	void test_illegal_commands_carry_their_own_reason()
	{
		auto elsewhere        = command(kind::edit, "x");
		elsewhere.Id          = core::review_draft_id{ 99U };
		const auto wrongDraft = core::apply(anchored_draft("a"), elsewhere);
		expect(
			wrongDraft.Rejection == core::review_draft_rejection::unknown_draft,
			"a command naming another draft is refused as unknown");

		const auto notInFlight = core::apply(
			anchored_draft("a"),
			command(kind::complete_publish));
		expect(
			notInFlight.Rejection == core::review_draft_rejection::not_publishing,
			"completing a publication that never began is refused");

		const auto discarded
			= core::apply(anchored_draft("a"), command(kind::discard));
		const auto afterDiscard
			= core::apply(discarded.Draft, command(kind::edit, "b"));
		expect(
			afterDiscard.Rejection == core::review_draft_rejection::discarded_draft,
			"a discarded draft refuses further commands with its own reason");

		// Six distinct reasons, none standing in for another.
		expect(
			wrongDraft.Rejection != notInFlight.Rejection
				&& notInFlight.Rejection != afterDiscard.Rejection
				&& wrongDraft.Rejection != afterDiscard.Rejection,
			"the reasons are distinct rather than one generic refusal");
	}

	// The core proposes; it does not perform. The proposal carries what to say
	// and where, and no host, endpoint, or credential.
	void test_a_proposal_exists_only_while_publishing()
	{
		const auto composing = anchored_draft("Text.");
		expect(
			!core::propose_publication(composing, core::review_disposition::comment)
				 .has_value(),
			"a draft still being composed proposes nothing");

		const auto begun
			= core::apply(composing, command(kind::begin_publish)).Draft;
		const auto proposal = core::propose_publication(
			begun,
			core::review_disposition::request_changes);
		expect(proposal.has_value(), "a draft in flight proposes a publication");
		expect(
			proposal->Body == "Text." && proposal->Anchor == composing.Anchor
				&& proposal->Draft == composing.Id
				&& proposal->Disposition
					== core::review_disposition::request_changes,
			"the proposal carries the text, the semantic anchor, and the "
			"disposition");

		const auto published
			= core::apply(begun, command(kind::complete_publish)).Draft;
		expect(
			!core::propose_publication(published, core::review_disposition::comment)
				 .has_value(),
			"a published comment is not proposed a second time");

		// [[BPR-0657]]. A draft that answers a thread proposes a reply to it;
		// a root draft proposes no thread.
		expect(!proposal->ReplyToThread.has_value(), "a root draft proposes no thread");
		auto answering          = composing;
		answering.ReplyToThread = "PRRT_thread_1";
		const auto reply        = core::propose_publication(
			core::apply(answering, command(kind::begin_publish)).Draft,
			core::review_disposition::comment);
		expect(reply.has_value() && reply->ReplyToThread == std::optional<std::string>{ "PRRT_thread_1" }
				   && reply->Anchor == composing.Anchor && reply->Body == "Text.",
			   "a draft answering a thread proposes the reply with the thread it answers");
	}

	// [[BPR-0657]]. A thread resolution is refused, in this order, for an
	// anonymous read, an absent capability, and a thread already in the
	// state asked for; otherwise it is allowed.
	void test_a_thread_resolution_is_refused_for_exactly_three_reasons()
	{
		using core::decide_thread_resolution;
		using core::thread_resolution_refusal;
		const auto allowed = core::thread_resolution_input{
			.Capability = core::github_capability_state::supported, .Resolved = false, .Resolve = true
		};
		expect(decide_thread_resolution(allowed) == thread_resolution_refusal::none,
			   "a supported, signed-in resolution of an open thread is allowed");
		auto anonymous          = allowed;
		anonymous.AnonymousRead = true;
		anonymous.Capability    = core::github_capability_state::unsupported;
		expect(decide_thread_resolution(anonymous) == thread_resolution_refusal::anonymous_read,
			   "an anonymous read is refused first, whatever else is wrong");
		auto unknown           = allowed;
		unknown.Capability     = core::github_capability_state::unknown;
		auto unsupported       = allowed;
		unsupported.Capability = core::github_capability_state::unsupported;
		expect(decide_thread_resolution(unknown) == thread_resolution_refusal::capability_absent
				   && decide_thread_resolution(unsupported) == thread_resolution_refusal::capability_absent,
			   "a capability the server never claimed, or denied, refuses");
		auto already     = allowed;
		already.Resolved = true;
		expect(decide_thread_resolution(already) == thread_resolution_refusal::already_in_state,
			   "resolving a resolved thread is refused");
		auto unresolveOpen    = allowed;
		unresolveOpen.Resolve = false;
		expect(decide_thread_resolution(unresolveOpen) == thread_resolution_refusal::already_in_state,
			   "unresolving an open thread is refused");
		auto unresolveResolved     = unresolveOpen;
		unresolveResolved.Resolved = true;
		expect(decide_thread_resolution(unresolveResolved) == thread_resolution_refusal::none,
			   "unresolving a resolved thread is allowed");
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		// A sha1 oid occupies the first twenty bytes and requires the rest to
		// be zero, so filling all thirty-two produces an *invalid* oid rather
		// than the distinct commit this helper is meant to supply.
		auto bytes = std::array<std::uint8_t, 32>{ };
		std::fill_n(bytes.begin(), 20U, aSeed);
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto target(const std::uint8_t aHead) -> core::review_mutation_target
	{
		return core::review_mutation_target{
			.Host         = core::git_host_id{ 1U },
			.Repository   = core::repository_id{ 2U },
			.PullRequest  = core::pull_request_id{ 3U },
			.ExpectedHead = commit(aHead),
		};
	}

	void test_an_unchanged_target_publishes()
	{
		const auto prepared = target(0x11U);
		expect(
			core::revalidate(prepared, prepared) == core::target_revalidation::accepted,
			"an identical target revalidates");
		expect(
			core::publication_permitted(core::target_revalidation::accepted, false),
			"an accepted target needs no acknowledgement");
	}

	// The whole point of this slice: a head that moved between drafting and
	// publishing must not silently submit against the wrong revision.
	void test_a_moved_head_stops_the_publication_until_acknowledged()
	{
		const auto prepared   = target(0x11U);
		auto       observed   = prepared;
		observed.ExpectedHead = commit(0x22U);

		const auto outcome = core::revalidate(prepared, observed);
		expect(
			outcome == core::target_revalidation::head_moved,
			"a moved head is reported as such");
		expect(
			!core::publication_permitted(outcome, false),
			"a moved head does not publish on its own");
		expect(
			core::publication_permitted(outcome, true),
			"an explicit acknowledgement lets the reviewer proceed");
	}

	// A comment aimed at the wrong pull request is a different and worse
	// mistake than one aimed at the right pull request's newer revision. No
	// acknowledgement can authorise it, because the acknowledgement the
	// reviewer gave was about a different pull request.
	void test_a_changed_target_can_never_be_acknowledged_away()
	{
		const auto prepared = target(0x11U);

		auto elsewhereHost             = prepared;
		elsewhereHost.Host             = core::git_host_id{ 9U };
		auto elsewhereRepository       = prepared;
		elsewhereRepository.Repository = core::repository_id{ 9U };
		auto elsewhereRequest          = prepared;
		elsewhereRequest.PullRequest   = core::pull_request_id{ 9U };

		for (const auto& observed :
			 { elsewhereHost, elsewhereRepository, elsewhereRequest }) {
			const auto outcome = core::revalidate(prepared, observed);
			expect(
				outcome == core::target_revalidation::target_changed,
				"a changed host, repository, or pull request is reported as a "
				"changed target");
			expect(
				!core::publication_permitted(outcome, false)
					&& !core::publication_permitted(outcome, true),
				"no acknowledgement authorises publishing to a different target");
		}
	}

	// Reporting the head first would hide the worse mistake.
	void test_a_changed_target_outranks_a_moved_head()
	{
		const auto prepared   = target(0x11U);
		auto       observed   = prepared;
		observed.Repository   = core::repository_id{ 9U };
		observed.ExpectedHead = commit(0x22U);

		expect(
			core::revalidate(prepared, observed)
				== core::target_revalidation::target_changed,
			"both wrong reports the changed target, not the moved head");
	}

	void test_an_unnameable_head_is_refused_outright()
	{
		const auto prepared = target(0x11U);

		auto unknownObserved         = prepared;
		unknownObserved.ExpectedHead = core::git_commit_oid{ };
		expect(
			core::revalidate(prepared, unknownObserved)
				== core::target_revalidation::head_unknown,
			"an observed head nobody can name is reported unknown");

		auto unknownPrepared         = prepared;
		unknownPrepared.ExpectedHead = core::git_commit_oid{ };
		expect(
			core::revalidate(unknownPrepared, prepared)
				== core::target_revalidation::head_unknown,
			"a prepared head nobody can name is reported unknown too");

		expect(
			!core::publication_permitted(core::target_revalidation::head_unknown, true),
			"a revision nobody can name is not one a reviewer can have agreed to");
	}

	void test_comment_target_shape_preserves_left_replies()
	{
		expect(core::supported_comment_target(core::diff_side::right), "right-side lines admit new comments and replies");
		// [[BPR-1212]]. GitHub takes a new comment on a base line as `side: LEFT`;
		// whether this comparison's base is numbered as GitHub's is the host's
		// evidence, which `decide_comment_range` and the host check.
		expect(core::supported_comment_target(core::diff_side::left), "left-side lines admit new comments and replies in shape");
		expect(!core::supported_comment_target(static_cast<core::diff_side>(255)), "an unknown side cannot admit a comment or reply");
	}

	void test_every_enumerator_names_itself()
	{
		expect(
			core::name(core::review_disposition::request_changes)
					== "request_changes"
				&& core::name(core::review_disposition::approve) == "approve",
			"every disposition names itself");
		expect(
			core::name(core::review_draft_state::publishing) == "publishing"
				&& core::name(core::review_draft_state::discarded) == "discarded",
			"every draft state names itself");
		expect(
			core::name(kind::fail_publish) == "fail_publish"
				&& core::name(core::review_draft_rejection::not_publishing)
					== "not_publishing",
			"every command and rejection names itself");
	}
} // namespace

namespace {
	void test_publication_knowledge_survives_a_refused_retry()
	{
		using outcome = core::review_publication_outcome;
		auto known    = core::retain_review_publication_outcome(outcome::not_published, outcome::not_published);
		expect(known == outcome::not_published, "a fresh refusal makes no publication claim");
		known = core::retain_review_publication_outcome(known, outcome::indeterminate);
		for (unsigned retry = 0; retry < 4; ++retry) {
			known = core::retain_review_publication_outcome(known, outcome::not_published);
			expect(known == outcome::indeterminate, "refused recovery cannot unlock a draft after an unknown attempt");
		}
		known = core::retain_review_publication_outcome(known, outcome::published);
		expect(known == outcome::published, "a confirmed exact receipt resolves the previous unknown publication");
		expect(core::retain_review_publication_outcome(known, outcome::not_published) == outcome::published,
			   "a later unavailable credential cannot erase a confirmed receipt");
	}

	/// [[BPR-1706]]. A commit-filtered diff's line the core placed where
	/// GitHub resolves no line comment goes straight to RFC-BPR-0022's file
	/// comment: no native attempt, its own durable admission, the same
	/// fallback child and reconciliation after it.
	void test_unresolvable_line_policy()
	{
		using namespace core;
		using action = review_publication_action;
		using state  = review_publication_state;
		using event  = review_publication_event;
		for (const auto capability : { off_hunk_creation_capability::supported, off_hunk_creation_capability::unsupported, off_hunk_creation_capability::unknown }) {
			for (const auto current : { true, false }) {
				const auto value = review_publication_observation{ .Placement = review_placement::unresolvable_line, .Capability = capability, .Eligible = true, .CapabilityCurrent = current };
				const auto plan  = plan_review_publication(value);
				expect(plan.Action == action::admit_unresolvable_line && plan.Representation == review_publication_representation::file_comment_with_permalink,
					   "an unresolvable line is admitted straight to the file comment, whatever the capability cache says");
				expect(!advance_review_publication(value, event::prepare_native), "no native attempt is prepared for it");
				expect(!advance_review_publication(value, event::admit_unsupported_evidence), "nor is capability evidence admitted for it");
				expect(!advance_review_publication(value, event::prepare_fallback), "its file comment needs the durable admission first");
				expect(advance_review_publication(value, event::admit_unresolvable_line) == std::optional{ state::unresolvable_line_admitted }, "the admission is durable");
			}
		}
		const auto suggestion = review_publication_observation{ .Placement = review_placement::unresolvable_line, .Body = review_body_compatibility::requires_line_subject, .Eligible = true };
		expect(plan_review_publication(suggestion).Diagnostic == review_publication_diagnostic::incompatible_body && !advance_review_publication(suggestion, event::admit_unresolvable_line),
			   "a suggestion needs a line, so an unresolvable line refuses it before any write");
		const auto prepared = plan_review_publication({ .Placement = review_placement::unresolvable_line, .State = state::native_prepared, .Eligible = true });
		expect(prepared.Action == action::refuse && prepared.Diagnostic == review_publication_diagnostic::invalid_observation, "a prepared native child never posts for an unresolvable line");
		expect(!advance_review_publication({ .Placement = review_placement::legacy_line, .Eligible = true }, event::admit_unresolvable_line)
				   && !advance_review_publication({ .Placement = review_placement::off_hunk, .Capability = off_hunk_creation_capability::unsupported, .Eligible = true, .CapabilityCurrent = true }, event::admit_unresolvable_line),
			   "no other placement is admitted as unresolvable");

		review_publication_observation value{ .Placement = review_placement::unresolvable_line, .Eligible = true };
		const auto                     step = [&value](event aEvent) { auto next=advance_review_publication(value,aEvent); expect(next.has_value(), "legal durable transition"); value.State=*next; };
		step(event::admit_unresolvable_line);
		// The parent's admission, not the placement, carries it on: a resumed
		// publication reloads only the state.
		expect(plan_review_publication({ .State = state::unresolvable_line_admitted, .Eligible = true }).Action == action::fallback_attempt, "the admitted parent prepares its file comment");
		expect(!advance_review_publication(value, event::native_transmission_witness) && !advance_review_publication(value, event::native_created)
				   && !advance_review_publication(value, event::native_definitively_unsupported),
			   "an admitted unresolvable line has no native child to witness");
		step(event::prepare_fallback);
		step(event::fallback_transmission_witness);
		expect(plan_review_publication(value).Action == action::reconcile_fallback, "a transmitted file comment only reconciles");
		step(event::fallback_created);
		expect(plan_review_publication(value).Action == action::already_succeeded && plan_review_publication(value).Representation == review_publication_representation::file_comment_with_permalink,
			   "a replay returns the file comment's receipt");
	}

	/// [[BPR-1735]]. A fully received refusal retires the child as rejected;
	/// GitHub holds no comment of it, so publishing again is a new attempt,
	/// and a line the core now places outside GitHub's diff goes to its file
	/// comment. Nothing else rejects.
	void test_rejected_publication_policy()
	{
		using namespace core;
		using action = review_publication_action;
		using state  = review_publication_state;
		using event  = review_publication_event;
		review_publication_observation value{ .Eligible = true };
		const auto                     step = [&value](event aEvent) { auto next=advance_review_publication(value,aEvent); expect(next.has_value(), "legal durable transition"); value.State=*next; };
		step(event::prepare_native);
		step(event::native_transmission_witness);
		expect(plan_review_publication(value).Action == action::reconcile_native, "an unanswered native child only reconciles");
		step(event::definitively_rejected);
		expect(value.State == state::rejected, "a fully received refusal rejects the native child");
		const auto again = plan_review_publication(value);
		expect(again.Action == action::native_attempt && again.Representation == review_publication_representation::native_line, "a rejected comment can be published again");
		expect(advance_review_publication(value, event::prepare_native) == std::optional{ state::native_prepared }, "as a new native attempt");
		expect(!advance_review_publication(value, event::prepare_fallback) && !advance_review_publication(value, event::native_created)
				   && !advance_review_publication(value, event::fallback_created) && !advance_review_publication(value, event::native_transmission_witness),
			   "a rejection authorizes no fallback and no receipt");
		value.Placement = review_placement::unresolvable_line;
		expect(plan_review_publication(value).Action == action::admit_unresolvable_line, "a refused line now placed outside GitHub's diff goes to its file comment");
		expect(advance_review_publication(value, event::admit_unresolvable_line) == std::optional{ state::unresolvable_line_admitted } && !advance_review_publication(value, event::prepare_native),
			   "admitted, with no native attempt");
		value.Placement         = review_placement::off_hunk;
		value.Capability        = off_hunk_creation_capability::unsupported;
		value.CapabilityCurrent = true;
		expect(plan_review_publication(value).Action == action::admit_unsupported_evidence, "fresh scoped evidence is admitted as from the start");

		// A file comment GitHub refused (subject_type file on a host that has
		// none) is rejected too, and its draft kept.
		value = { .Placement = review_placement::unresolvable_line, .Eligible = true };
		step(event::admit_unresolvable_line);
		step(event::prepare_fallback);
		step(event::fallback_transmission_witness);
		step(event::definitively_rejected);
		expect(value.State == state::rejected, "a refused file comment is rejected");
		expect(plan_review_publication(value).Action == action::admit_unresolvable_line, "and can be published again");
		for (const auto unanswered : { state::initial, state::native_definitively_unsupported, state::unsupported_evidence_admitted, state::unresolvable_line_admitted, state::succeeded_native, state::succeeded_fallback, state::rejected }) {
			expect(!advance_review_publication({ .State = unanswered, .Eligible = true }, event::definitively_rejected), "only a sent child is rejected");
		}

		// What settles a child that may have been sent, when GitHub's complete
		// list holds no comment of it.
		using answer       = review_publication_answer;
		constexpr auto at  = std::int64_t{ 1'000'000 };
		const auto     now = at + review_publication_settle_delay_ms;
		expect(publication_definitely_unpublished(review_publication_post{ .Answer = answer::error_status, .StatusClass = 4U, .RecordedUtcMs = at }, at), "a fully received 4xx settles at once");
		expect(!publication_definitely_unpublished(review_publication_post{ .Answer = answer::error_status, .StatusClass = 5U, .RecordedUtcMs = at }, now * 9), "a server error never does");
		expect(!publication_definitely_unpublished(review_publication_post{ .Answer = answer::error_status, .StatusClass = 3U, .RecordedUtcMs = at }, now * 9), "nor a redirect");
		expect(!publication_definitely_unpublished(review_publication_post{ .Answer = answer::no_answer, .StatusClass = 0U, .RecordedUtcMs = at }, now * 9), "nor a POST with no definite answer: its outcome is unknown");
		expect(!publication_definitely_unpublished(review_publication_post{ .Answer = answer::success, .StatusClass = 2U, .RecordedUtcMs = at }, now * 9), "nor a success");
		expect(!publication_definitely_unpublished(std::nullopt, now * 9), "nor a child with no answer kept");
		const auto migrated = review_publication_post{ .Answer = answer::kept_before_answers, .StatusClass = 0U, .RecordedUtcMs = at };
		expect(!publication_definitely_unpublished(migrated, now - 1) && publication_definitely_unpublished(migrated, now), "a migrated record settles ten minutes after it was migrated");
		expect(!publication_definitely_unpublished(migrated, at - 1), "a clock earlier than the record waits");
		expect(!valid(review_publication_post{ .Answer = answer::success, .StatusClass = 4U }) && !valid(review_publication_post{ .Answer = answer::no_answer, .StatusClass = 4U })
				   && !valid(review_publication_post{ .Answer = static_cast<answer>(9), .StatusClass = 0U }) && !valid(review_publication_post{ .Answer = answer::no_answer, .RecordedUtcMs = -1 }),
			   "answers out of range are invalid");
		expect(!publication_definitely_unpublished(review_publication_post{ .Answer = answer::error_status, .StatusClass = 6U }, now), "and settle nothing");

		using outcome = review_publication_outcome;
		expect(retain_review_publication_outcome(outcome::indeterminate, outcome::rejected) == outcome::not_published, "the journal's rejection unlocks a draft whose outcome was unknown");
		expect(retain_review_publication_outcome(outcome::indeterminate, outcome::not_published) == outcome::indeterminate, "a refused check does not");
		expect(retain_review_publication_outcome(outcome::published, outcome::rejected) == outcome::published, "nor does anything undo a confirmed receipt");
	}

	void test_publication_policy()
	{
		using namespace core;
		using action = review_publication_action;
		using state  = review_publication_state;
		using event  = review_publication_event;
		for (const auto placement : { review_placement::legacy_line, review_placement::off_hunk, review_placement::unknown }) {
			for (const auto capability : { off_hunk_creation_capability::supported, off_hunk_creation_capability::unsupported, off_hunk_creation_capability::unknown }) {
				review_publication_observation value{ .Placement = placement, .Capability = capability, .Eligible = true, .CapabilityCurrent = true };
				const bool                     needsFallback = placement == review_placement::off_hunk && capability == off_hunk_creation_capability::unsupported;
				expect(plan_review_publication(value).Action == (needsFallback ? action::admit_unsupported_evidence : action::native_attempt), "all placement/capability cells choose explicit admission or native");
				expect(!advance_review_publication(value, event::prepare_fallback), "a cache fact cannot directly prepare fallback");
				value.CapabilityCurrent = false;
				expect(plan_review_publication(value).Action == action::native_attempt, "expired or foreign capability is unknown");
				value.State             = state::native_possibly_transmitted;
				value.CapabilityCurrent = true;
				expect(plan_review_publication(value).Action == action::reconcile_native, "indeterminate native cannot fall back even with fresh unsupported evidence");
				expect(!advance_review_publication(value, event::admit_unsupported_evidence) && !advance_review_publication(value, event::prepare_fallback), "neither fallback admission can bypass a native witness");
			}
		}
		review_publication_observation value{ .Eligible = true };
		const auto                     step = [&value](event aEvent) { auto next=advance_review_publication(value,aEvent); expect(next.has_value(), "legal durable transition"); value.State=*next; };
		step(event::prepare_native);
		step(event::native_transmission_witness);
		step(event::native_definitively_unsupported);
		expect(plan_review_publication(value).Action == action::fallback_attempt, "exact committed rejection permits fallback");
		value.Body = review_body_compatibility::requires_line_subject;
		expect(plan_review_publication(value).Diagnostic == review_publication_diagnostic::incompatible_body && !advance_review_publication(value, event::prepare_fallback), "suggestion refuses fallback after native rejection");
		value.Body = review_body_compatibility::portable;
		step(event::prepare_fallback);
		step(event::fallback_transmission_witness);
		expect(plan_review_publication(value).Action == action::reconcile_fallback, "fallback witness only reconciles");
		step(event::fallback_created);
		for (unsigned i = 0; i <= static_cast<unsigned>(event::admit_unresolvable_line); ++i) {
			expect(!advance_review_publication(value, static_cast<event>(i)), "success has no further write transition");
		}
		expect(plan_review_publication(value).Action == action::already_succeeded, "duplicate replays receipt");
		value      = { .Placement = review_placement::off_hunk, .Capability = off_hunk_creation_capability::unsupported, .Eligible = true, .CapabilityCurrent = true };
		value.Body = review_body_compatibility::requires_line_subject;
		expect(plan_review_publication(value).Diagnostic == review_publication_diagnostic::incompatible_body, "known unsupported suggestion has no POST");
		value.Body = review_body_compatibility::portable;
		step(event::admit_unsupported_evidence);
		step(event::prepare_fallback);
		value.Eligible = false;
		expect(plan_review_publication(value).Diagnostic == review_publication_diagnostic::ineligible_anchor, "stale eligibility refuses even prepared state");
		value.Eligible  = true;
		value.Placement = static_cast<review_placement>(255);
		expect(plan_review_publication(value).Diagnostic == review_publication_diagnostic::invalid_observation, "hostile enum fails closed");
		for (unsigned stateIndex = 0U; stateIndex <= static_cast<unsigned>(state::unresolvable_line_admitted); ++stateIndex) {
			const auto stateValue    = static_cast<state>(stateIndex);
			const auto assertRefused = [&](const review_publication_observation& aObservation, const review_publication_diagnostic aDiagnostic) {
				const auto decision = plan_review_publication(aObservation);
				expect(decision.Action == action::refuse && decision.Diagnostic == aDiagnostic && !decision.Representation, "illegal observation has no representation or action at any durable state");
				for (unsigned eventIndex = 0U; eventIndex <= static_cast<unsigned>(event::admit_unresolvable_line); ++eventIndex) {
					expect(!advance_review_publication(aObservation, static_cast<event>(eventIndex)), "illegal observation authorizes no durable transition");
				}
			};
			assertRefused({ .State = stateValue }, review_publication_diagnostic::ineligible_anchor);
			for (const unsigned invalid : { 11U, 255U }) {
				assertRefused({ .Placement = static_cast<review_placement>(invalid), .State = stateValue, .Eligible = true }, review_publication_diagnostic::invalid_observation);
				assertRefused({ .Capability = static_cast<off_hunk_creation_capability>(invalid), .State = stateValue, .Eligible = true }, review_publication_diagnostic::invalid_observation);
				assertRefused({ .Body = static_cast<review_body_compatibility>(invalid), .State = stateValue, .Eligible = true }, review_publication_diagnostic::invalid_observation);
				assertRefused({ .State = static_cast<state>(invalid), .Eligible = true }, review_publication_diagnostic::invalid_observation);
				expect(!advance_review_publication({ .State = stateValue, .Eligible = true }, static_cast<event>(invalid)), "unknown event cannot advance any durable state");
			}
		}
		test_unresolvable_line_policy();
		test_rejected_publication_policy();
		expect(classify_review_body("text\n```suggestion:-1+2\nx\n```") == review_body_compatibility::requires_line_subject, "suggestion ranges keep line semantics");
		expect(classify_review_body("~~~  suggestion\ncode") == review_body_compatibility::requires_line_subject, "whitespace in a suggestion info string cannot degrade");
		expect(classify_review_body("ordinary prose") == review_body_compatibility::portable, "plain prose is portable");
	}
}

auto main() -> int
{
	test_publication_policy();
	test_publication_knowledge_survives_a_refused_retry();
	test_a_draft_composes_publishes_and_becomes_shared();
	test_a_second_publish_while_one_is_in_flight_is_refused();
	test_a_failed_publish_returns_the_text_to_the_author();
	test_whitespace_is_not_a_comment();
	test_a_published_comment_is_no_longer_a_draft();
	test_illegal_commands_carry_their_own_reason();
	test_a_proposal_exists_only_while_publishing();
	test_a_thread_resolution_is_refused_for_exactly_three_reasons();
	test_an_unchanged_target_publishes();
	test_a_moved_head_stops_the_publication_until_acknowledged();
	test_a_changed_target_can_never_be_acknowledged_away();
	test_a_changed_target_outranks_a_moved_head();
	test_an_unnameable_head_is_refused_outright();
	test_comment_target_shape_preserves_left_replies();
	test_every_enumerator_names_itself();

	std::cout << "review-round-trip-core-tests:pass\n";
	return EXIT_SUCCESS;
}
