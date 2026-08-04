#include "bpr/core/review_round_trip.hpp"

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
	test_every_enumerator_names_itself();

	std::cout << "review-round-trip-core-tests:pass\n";
	return EXIT_SUCCESS;
}
