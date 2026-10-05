// [[BPR-1082]]. Conventional Comments header edits bound to the existing
// revisioned comment drafts and local tour stops (RFC-BPR-0048 R09 to R11,
// R42; RFC-BPR-0051 R41). Every plan is applied through the draft's own
// reducer: `apply` for a comment draft, `apply` for a tour.

#include "../support/review_tour_fixture.hpp"
#include "bpr/core/feedback_draft_edit.hpp"
#include "private_review_fixture.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(bool aValue, std::string_view aWhy)
	{
		if (!aValue) {
			std::cerr << "feedback draft edit failure: " << aWhy << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	auto label_at(std::size_t aIndex) -> feedback_label
	{
		return static_cast<feedback_label>(aIndex);
	}

	auto set(feedback_label aLabel, std::optional<feedback_expectation> aExpectation = std::nullopt) -> feedback_header_edit
	{
		return { .Change = feedback_label_change::set, .Label = aLabel, .Expectation = aExpectation };
	}
	auto remove() -> feedback_header_edit
	{
		return { .Change = feedback_label_change::remove };
	}
	auto expect_only(feedback_expectation aExpectation) -> feedback_header_edit
	{
		return { .Expectation = aExpectation };
	}

	auto draft(std::string aBody, std::uint64_t aRevision = 4) -> revisioned_review_draft
	{
		return { .Draft = { .Id = review_draft_id{ 7 }, .Anchor = { }, .Body = std::move(aBody) }, .Revision = aRevision, .Scope = review_draft_scope_id{ 3 } };
	}

	/// Plan against what the author was shown and apply through the draft's
	/// reducer, as the shared draft owner does.
	auto edit(const revisioned_review_draft& aDraft, const feedback_header_edit& aEdit) -> revisioned_review_draft
	{
		const auto plan = plan_feedback_draft_edit(aDraft, feedback_draft_basis_of(aDraft), aEdit);
		expect(plan.Refusal == feedback_draft_edit_refusal::none && plan.Command.has_value(), "edit planned");
		const auto outcome = apply(aDraft, *plan.Command);
		expect(outcome.Refusal == revisioned_draft_refusal::none && outcome.Changed, "the draft owner accepts the planned edit");
		expect(outcome.Accepted.Revision == aDraft.Revision + 1U && outcome.Accepted.Draft.Body == plan.Decision.Body, "one revision, the decided body");
		expect(plan.Command->Command.Body.substr(0U, plan.Decision.Replaced.Begin) == aDraft.Draft.Body.substr(0U, plan.Decision.Replaced.Begin)
				   && plan.Command->Command.Body.substr(plan.Decision.Replaced.Begin + plan.Decision.Replacement.size()) == aDraft.Draft.Body.substr(plan.Decision.Replaced.End),
			   "only the replaced span changes");
		return outcome.Accepted;
	}

	/// A comment classified before it is written, then written.
	void label_first()
	{
		auto value = draft("");
		value      = edit(value, set(feedback_label::issue));
		expect(value.Draft.Body == "issue: ", "a blank comment takes the header alone");
		expect(recognize_feedback_prefix(value.Draft.Body).Status == feedback_prefix_status::malformed, "a header with no subject yet is incomplete, not a label");
		value = edit(value, expect_only(feedback_expectation::blocking));
		expect(value.Draft.Body == "issue (blocking): ", "the decoration joins the incomplete header");
		value = edit(value, set(feedback_label::question));
		expect(value.Draft.Body == "question (blocking): ", "the incomplete header is relabelled");
		value = edit(value, expect_only(feedback_expectation::unspecified));
		expect(value.Draft.Body == "question: ", "the decoration leaves it");
		value = edit(value, remove());
		expect(value.Draft.Body.empty(), "Plain empties the incomplete header");

		value            = edit(value, set(feedback_label::todo, feedback_expectation::if_minor));
		const auto typed = apply(value, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = value.Draft.Body + "Rename it." }, .ExpectedRevision = value.Revision, .Scope = value.Scope });
		expect(typed.Changed, "the author types the subject");
		const auto annotation = recognize_feedback_prefix(typed.Accepted.Draft.Body);
		expect(annotation.Status == feedback_prefix_status::recognized && annotation.Label == feedback_label::todo && annotation.Expectation == feedback_expectation::if_minor, "with a subject the header is the label chosen first");

		for (const auto& blank : { "  ", "\r\n\r\n", "\xEF\xBB\xBF", "\t\n" }) {
			const auto labelled = edit(draft(blank), set(feedback_label::note));
			expect(recognize_feedback_prefix(labelled.Draft.Body + "x").Label == feedback_label::note, "a blank body's header reads back once a subject follows");
			expect(edit(labelled, remove()).Draft.Body == blank, "Plain restores the blank bytes");
		}

		std::size_t cases = 0;
		for (std::size_t first = 0; first < feedback_label_count; ++first) {
			for (const auto& expectation : { feedback_expectation::unspecified, feedback_expectation::blocking, feedback_expectation::non_blocking, feedback_expectation::if_minor }) {
				const auto chosen = edit(draft(""), set(label_at(first), expectation));
				for (std::size_t second = 0; second < feedback_label_count; ++second) {
					if (second == first) {
						continue;
					}
					const auto relabelled = edit(chosen, set(label_at(second)));
					expect(recognize_feedback_prefix(relabelled.Draft.Body + "x").Label == label_at(second), "every label replaces every other before the subject");
					expect(edit(relabelled, remove()).Draft.Body.empty(), "and Plain empties it");
					++cases;
				}
			}
		}
		std::cout << "label-first relabels: " << cases << '\n';

		for (const auto& written : { "issue:", "issue: \nmore", "issue (blocking: ", "**issue:**" }) {
			const auto current = draft(written);
			const auto plan    = plan_feedback_draft_edit(current, feedback_draft_basis_of(current), remove());
			expect(plan.Refusal == feedback_draft_edit_refusal::header && !plan.Command && plan.Decision.Refusal == feedback_edit_refusal::unsupported_header, "malformed leading syntax is never deleted");
		}
		const auto current = draft("> quoted");
		const auto plan    = plan_feedback_draft_edit(current, feedback_draft_basis_of(current), set(feedback_label::issue));
		expect(plan.Refusal == feedback_draft_edit_refusal::header && plan.Decision.Refusal == feedback_edit_refusal::restructuring_required, "a body that needs restructuring is still refused");
	}

	/// Repeated, undone and conflicting edits of a written comment.
	void written_comment()
	{
		const auto original = std::string{ "Rename this.\r\n\r\n```suggestion\r\n\tvalue = 1;\r\n```\r\nα" };
		auto       value    = edit(draft(original), set(feedback_label::issue, feedback_expectation::blocking));
		expect(value.Draft.Body == "issue (blocking): " + original, "the header is written before the subject; the suggestion is untouched");
		for (std::size_t index = 0; index < feedback_label_count; ++index) {
			value = edit(value, set(label_at(index) == feedback_label::issue ? feedback_label::praise : label_at(index)));
		}
		value = edit(value, set(feedback_label::issue));
		expect(value.Draft.Body == "issue (blocking): " + original, "twelve relabels return to the same bytes");
		expect(edit(value, remove()).Draft.Body == original, "Plain restores the original comment byte for byte");

		const auto same = plan_feedback_draft_edit(value, feedback_draft_basis_of(value), set(feedback_label::issue));
		expect(same.Refusal == feedback_draft_edit_refusal::none && !same.Command && !same.Decision.Changed, "a no-op plans no command and keeps the revision");

		// The author typed after the edit was planned: the owner's revision
		// check refuses the old command and keeps the newer draft.
		const auto planned = plan_feedback_draft_edit(value, feedback_draft_basis_of(value), set(feedback_label::nitpick));
		const auto typed   = apply(value, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = value.Draft.Body + " typed" }, .ExpectedRevision = value.Revision, .Scope = value.Scope });
		expect(typed.Changed && planned.Command.has_value(), "another edit lands first");
		const auto late = apply(typed.Accepted, *planned.Command);
		expect(late.Refusal == revisioned_draft_refusal::conflict && late.Accepted == typed.Accepted, "the old label command is a conflict and the typed draft is kept byte for byte");
		const auto replan = plan_feedback_draft_edit(typed.Accepted, feedback_draft_basis_of(value), set(feedback_label::nitpick));
		expect(replan.Refusal == feedback_draft_edit_refusal::stale_draft && !replan.Command && replan.Decision.Annotation.BodyDigest == recognize_feedback_prefix(typed.Accepted.Draft.Body).BodyDigest, "planning from the old basis is stale and reports the current body");
		auto otherBody   = feedback_draft_basis_of(value);
		otherBody.Digest = recognize_feedback_prefix("something else").BodyDigest;
		expect(plan_feedback_draft_edit(value, otherBody, set(feedback_label::nitpick)).Refusal == feedback_draft_edit_refusal::stale_draft, "the shown body must be the current body");
		auto otherParser           = feedback_draft_basis_of(value);
		otherParser.ParserRevision = feedback_parser_revision + 1U;
		expect(plan_feedback_draft_edit(value, otherParser, set(feedback_label::nitpick)).Refusal == feedback_draft_edit_refusal::stale_draft, "another parser revision is stale");
		expect(edit(typed.Accepted, set(feedback_label::nitpick)).Draft.Body.starts_with("nitpick (blocking): Rename this."), "planning again from the current draft applies");

		const auto retargeted = apply(typed.Accepted, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = typed.Accepted.Draft.Body }, .ExpectedRevision = typed.Accepted.Revision, .ChangeReplyTarget = true, .ReplyToThread = "thread-1", .Scope = value.Scope });
		expect(retargeted.Changed && retargeted.Accepted.Draft.Body == typed.Accepted.Draft.Body, "another window moves the reply target; the body is unchanged");
		expect(plan_feedback_draft_edit(retargeted.Accepted, feedback_draft_basis_of(typed.Accepted), set(feedback_label::nitpick)).Refusal == feedback_draft_edit_refusal::stale_draft, "a revision change with the same body is still stale");

		auto foreign  = *planned.Command;
		foreign.Scope = review_draft_scope_id{ 99 };
		expect(apply(value, foreign).Refusal == revisioned_draft_refusal::invalid_identity, "a command for another draft scope changes nothing");
	}

	/// Only a drafting comment is edited: a prepared publication keeps its
	/// exact body, and a linked composition goes through its durable owner.
	void prepared_and_linked()
	{
		auto       value    = edit(draft("Please check this."), set(feedback_label::question));
		auto       publish  = apply(value, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::begin_publish }, .ExpectedRevision = value.Revision, .Scope = value.Scope });
		auto       proposal = propose_publication(publish.Accepted.Draft, review_disposition::comment);
		const auto old      = proposal;
		expect(publish.Changed && proposal && proposal->Body == "question: Please check this.", "the prepared proposal carries the exact labelled body");
		for (const auto& state : { review_draft_state::publishing, review_draft_state::published, review_draft_state::discarded }) {
			auto other        = publish.Accepted;
			other.Draft.State = state;
			const auto plan   = plan_feedback_draft_edit(other, feedback_draft_basis_of(other), set(feedback_label::issue));
			expect(plan.Refusal == feedback_draft_edit_refusal::not_editable && !plan.Command, "a draft that is not drafting is not relabelled");
		}
		const auto bypass = apply(publish.Accepted, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::edit, .Body = "issue: Please check this." }, .ExpectedRevision = publish.Accepted.Revision, .Scope = value.Scope });
		expect(bypass.Refusal == revisioned_draft_refusal::policy && bypass.Policy == review_draft_rejection::already_publishing && bypass.Accepted == publish.Accepted, "a hand-made edit of a publishing draft is refused by its owner");

		const auto failed = apply(publish.Accepted, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::fail_publish }, .ExpectedRevision = publish.Accepted.Revision, .Scope = value.Scope });
		expect(failed.Changed && failed.Accepted.Draft.State == review_draft_state::drafting, "a definite failure returns the draft to drafting");
		const auto relabelled = edit(failed.Accepted, set(feedback_label::issue));
		publish               = apply(relabelled, revisioned_review_draft_command{ .Command = { .Id = value.Draft.Id, .Kind = review_draft_command_kind::begin_publish }, .ExpectedRevision = relabelled.Revision, .Scope = value.Scope });
		proposal              = propose_publication(publish.Accepted.Draft, review_disposition::comment);
		expect(proposal && proposal->Body == "issue: Please check this." && proposal != old && proposal->Draft == old->Draft, "the next prepared proposal is a new action on the same draft");

		auto linked        = edit(draft("Linked prose."), set(feedback_label::note));
		linked.Composition = review_composition_link{ private_review_fixture::binding().Scope, review_composition_id{ 1 }, review_composition_revision_id{ 2 }, private_digest(linked.Draft.Body) };
		const auto plan    = plan_feedback_draft_edit(linked, feedback_draft_basis_of(linked), set(feedback_label::issue));
		expect(plan.Command && !plan.Command->Composition, "a linked draft's plan carries no composition link");
		expect(apply(linked, *plan.Command).Refusal == revisioned_draft_refusal::invalid_command, "the volatile reducer cannot rewrite linked prose without its durable owner");
	}

	void tour_stops()
	{
		auto       tour = bpr::tests::review_tour_fixture();
		const auto stop = tour.Stops.front().Id;
		const auto raw  = tour.Stops.front().Body;
		auto       plan = plan_feedback_tour_stop_edit(tour, stop, *feedback_tour_stop_basis(tour, stop), set(feedback_label::note));
		expect(plan.Refusal == feedback_draft_edit_refusal::none && plan.Command && plan.Command->Kind == tour_command_kind::edit_stop && plan.Command->ExpectedRevision == tour.Revision, "a stop edit is the existing edit_stop command");
		auto outcome = apply(tour, *plan.Command);
		expect(outcome.Refusal == tour_refusal::none && outcome.Accepted.Revision == tour.Revision + 1U, "the tour owner accepts it");
		const auto& edited = outcome.Accepted.Stops.front();
		expect(edited.Body == "note: " + raw && edited.Title == tour.Stops.front().Title && edited.Original == tour.Stops.front().Original && edited.Related == tour.Stops.front().Related, "only the narrative's header changes");
		expect(outcome.Accepted.Problem == tour.Problem && outcome.Accepted.Solution == tour.Solution, "the tour's other fields are untouched");

		const auto stale = plan_feedback_tour_stop_edit(outcome.Accepted, stop, *feedback_tour_stop_basis(tour, stop), remove());
		expect(stale.Refusal == feedback_draft_edit_refusal::stale_draft && !stale.Command, "an older tour revision is stale");
		expect(apply(outcome.Accepted, *plan.Command).Refusal == tour_refusal::conflict, "the old command is a conflict for the tour owner");
		expect(plan_feedback_tour_stop_edit(tour, tour_stop_id{ 99 }, *feedback_tour_stop_basis(tour, stop), remove()).Refusal == feedback_draft_edit_refusal::stale_draft && !feedback_tour_stop_basis(tour, tour_stop_id{ 99 }), "an unknown stop is stale");
		plan    = plan_feedback_tour_stop_edit(outcome.Accepted, stop, *feedback_tour_stop_basis(outcome.Accepted, stop), remove());
		outcome = apply(outcome.Accepted, *plan.Command);
		expect(outcome.Refusal == tour_refusal::none && outcome.Accepted.Stops.front().Body == raw, "Plain restores the stop byte for byte");
		auto readOnly     = outcome.Accepted;
		readOnly.ReadOnly = true;
		expect(plan_feedback_tour_stop_edit(readOnly, stop, *feedback_tour_stop_basis(readOnly, stop), set(feedback_label::note)).Refusal == feedback_draft_edit_refusal::not_editable, "a read-only tour is not relabelled");
	}
}

int main()
{
	label_first();
	written_comment();
	prepared_and_linked();
	tour_stops();
	std::cout << "FEEDBACK_DRAFT_EDIT_OK\n";
	return EXIT_SUCCESS;
}
