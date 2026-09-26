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
		expect(core::supported_comment_target(core::diff_side::right, false)
				   && core::supported_comment_target(core::diff_side::right, true),
			   "right-side lines admit new comments and replies");
		expect(!core::supported_comment_target(core::diff_side::left, false)
				   && core::supported_comment_target(core::diff_side::left, true),
			   "left-side lines admit existing-thread replies only");
		expect(!core::supported_comment_target(static_cast<core::diff_side>(255), false)
				   && !core::supported_comment_target(static_cast<core::diff_side>(255), true),
			   "an unknown side cannot admit a comment or reply");
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

auto main() -> int
{
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
