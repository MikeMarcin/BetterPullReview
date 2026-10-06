// [[BPR-1086]]. Private promotion into the shared draft (RFC-BPR-0048,
// "Defects and private notes"; R17, R18; the pack's native-integration case
// `private-promotion`). An AI finding with private severity, category and
// runtime provenance, and private notes, reach the one draft through each
// promotion path the product has: RFC-BPR-0023's Squash to Draft (exact copy
// and reviewer-authored), BPR-1210's note copy and RFC-BPR-0012's finding
// conversion. The draft and the publication proposal carry exactly the text
// the reviewer accepted; no private metadata is appended and no `(blocking)`
// is derived from severity; the label is the one written in the accepted
// text; several labelled sources never become one multi-label header; and
// the squash path keeps its lineage privately, from the draft's composition
// link through the candidate's derivation to the finding and its run.

#include "bpr/core/agent_finding_promotion.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/review_composition.hpp"
#include "private_review_fixture.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
	using namespace private_review_fixture;

	constexpr auto model_sentinel  = std::string_view{ "model-sentinel-7f3a" };
	constexpr auto effort_sentinel = std::string_view{ "effort-sentinel-c2" };
	constexpr auto suggested       = std::string_view{ "issue: Guard the null callback before the first frame." };

	/// The private facts that must stay private: severity, category and the
	/// runtime's model and effort.
	void expect_no_private_metadata(const std::string_view aBody, const std::string_view aWhy)
	{
		for (const auto secret : { name(defect_severity::critical), name(defect_category::security), model_sentinel, effort_sentinel }) {
			if (aBody.find(secret) != std::string_view::npos) {
				std::cerr << aWhy << ": public text carries '" << secret << "'\n";
			}
			expect(aBody.find(secret) == std::string_view::npos, aWhy);
		}
	}

	struct sources
	{
		diff_semantic_index        Index   = index();
		private_review_binding     Binding = binding();
		private_composition_target Target;
		agent_run_provenance       Run;
		retained_agent_finding     Finding;
		private_review_revision    Note;
		std::string                NoteBody = "nitpick: Rename the callback.";

		sources()
		{
			Binding.Comparison = Index.diff().Alignment.Digest;
			const auto target  = make_composition_target(Binding, { *Binding.Comparison, diff_side::right, 1 }, Index);
			expect(target.has_value(), "the semantic index admits the exact right-side target");
			Target = *target;

			// A labelled private revision, the reviewer's own.
			auto input   = private_review_fixture::input();
			input.Body   = NoteBody;
			auto created = create_private_thread(private_review_thread_id{ 1 }, { private_review_fixture::binding(), private_pr_subject{ } }, input);
			expect(created.has_value(), "the reviewer's labelled private revision is created");
			Note = created->Revision;

			// A findings run that reported its model and effort.
			Run.Run                      = agent_run_id{ 1 };
			Run.Session                  = agent_session_id{ 9 };
			Run.Order                    = private_vertex_order{ 10 };
			Run.Profile                  = agent_profile_id{ 1 };
			Run.ProfileDisplayName       = "Review agent";
			Run.Protocol                 = agent_protocol::codex_app_server;
			Run.EntryPoint               = agent_entry_point::run_ai_review;
			Run.Purpose                  = private_run_purpose::findings;
			Run.Binding                  = Binding;
			Run.Metadata.Requested.Model = std::string{ model_sentinel };
			auto report                  = agent_reported_facts{ };
			report.Basis                 = "turn/model";
			report.Settings.Model        = std::string{ model_sentinel };
			report.Settings.Effort       = std::string{ effort_sentinel };
			Run.Metadata.ProtocolReported.push_back(report);
			const auto selected        = std::vector<private_source_selection>{ { source_of(Note), source_of(Note).Key, false } };
			Run.SelectedPrivateSources = selected;
			Run.InputSelectionDigest   = *private_selection_digest(selected);
			Run.OutputDigest           = private_digest("findings");
			Run.Outcome                = private_run_outcome::succeeded;
			Run.Completion             = private_revision_completion::complete;
			expect(valid_run_provenance(Run), "the findings run's provenance is valid");

			// Its finding, critical and about security, with a suggested comment.
			auto finding             = agent_finding{ };
			finding.Provider         = Run.Profile;
			finding.Severity         = defect_severity::critical;
			finding.Category         = defect_category::security;
			finding.Path             = "file.cpp";
			finding.Lines            = { 1, 2 };
			finding.Anchor.Line      = Target.Line;
			finding.Claim            = "The callback may be null on the first frame.";
			finding.Evidence         = "evidence";
			finding.Consequence      = "consequence";
			finding.SuggestedComment = std::string{ suggested };
			finding.RequiredContext  = "context";
			finding.AnalyzedBaseOid  = Binding.Revision.BaseOid;
			finding.AnalyzedHeadOid  = Binding.Revision.HeadOid;
			const auto& alignment    = Index.diff().Alignment;
			const auto  snapshot     = agent_finding_snapshot{ Binding.Revision, { { "file.cpp", alignment.Digest, 3, 3, { }, { }, { } } } };
			auto        retained     = retain_agent_finding(Run, 0, private_vertex_order{ 20 }, finding, snapshot);
			expect(retained.has_value() && retained->Verdict.anchored(), "the finding is retained, anchored, with its severity and category");
			Finding = *retained;
		}

		[[nodiscard]] auto note_observation() const -> private_source_observation
		{
			auto observed         = private_source_observation{ };
			observed.Source       = source_of(Note);
			observed.Latest       = observed.Source.Key;
			observed.Actor        = Note.Actor;
			observed.Completion   = Note.Completion;
			observed.Availability = private_subject_availability::current;
			observed.Target       = Target;
			observed.CopyBody     = private_copy_body{ Note.Identity.Digest, NoteBody.size() };
			return observed;
		}

		[[nodiscard]] auto finding_observation() const -> private_source_observation
		{
			auto observed           = private_source_observation{ };
			observed.Source         = Finding.Source;
			observed.Latest         = Finding.Source.Key;
			observed.Actor          = private_agent_actor{ Run.Run };
			observed.Completion     = private_revision_completion::complete;
			observed.Availability   = private_subject_availability::current;
			observed.Target         = Target;
			observed.CopyBody       = private_copy_body{ private_digest(suggested), suggested.size() };
			observed.FindingVerdict = Finding.Verdict;
			return observed;
		}

		[[nodiscard]] auto draft() const -> revisioned_review_draft
		{
			auto value         = revisioned_review_draft{ };
			value.Draft.Id     = review_draft_id{ 1 };
			value.Draft.Anchor = Target.Line;
			value.Draft.State  = review_draft_state::drafting;
			value.Revision     = 1;
			value.Scope        = review_draft_scope_id{ 1 };
			return value;
		}
	};

	/// Accepts a candidate into the draft the way the durable owner does:
	/// the exact body, with the composition link, then the existing explicit
	/// publish command. Returns the publication proposal's body.
	struct accepted_draft
	{
		revisioned_review_draft     Draft;
		review_publication_proposal Proposal;
	};
	auto accept(const sources& aSources, const composition_revision_proposal& aCandidate, const review_composition_revision_id aRevision,
				const std::vector<private_source_selection>& aSelected, const std::vector<private_source_observation>& aObserved, const std::string_view aWhy) -> accepted_draft
	{
		const auto target   = aSources.draft();
		const auto accepted = accept_composition_as_draft(aCandidate.State, aCandidate.Revision, aSelected, aObserved, aSources.Index, target);
		expect(accepted.has_value() && accepted->Body == aCandidate.Revision.Body, aWhy);
		auto command             = revisioned_review_draft_command{ };
		command.Command.Id       = target.Draft.Id;
		command.Command.Kind     = review_draft_command_kind::edit;
		command.Command.Body     = accepted->Body;
		command.ExpectedRevision = target.Revision;
		command.Scope            = target.Scope;
		command.Composition      = review_composition_link{ aSources.Binding.Scope, aCandidate.State.Id, aRevision, private_digest(accepted->Body) };
		const auto linked        = apply(target, command);
		expect(linked.Changed && linked.Accepted.Composition == command.Composition, "the draft keeps the composition link");
		auto publish             = revisioned_review_draft_command{ };
		publish.Command.Id       = target.Draft.Id;
		publish.Command.Kind     = review_draft_command_kind::begin_publish;
		publish.ExpectedRevision = linked.Accepted.Revision;
		publish.Scope            = target.Scope;
		const auto publishing    = apply(linked.Accepted, publish);
		expect(publishing.Changed && publishing.Accepted.Composition == command.Composition, "the explicit publish keeps the link");
		const auto proposal = propose_publication(publishing.Accepted.Draft, review_disposition::comment);
		expect(proposal.has_value(), "the publishing draft proposes its publication");
		return { publishing.Accepted, *proposal };
	}

	/// RFC-BPR-0023's Squash to Draft from the one finding, copied exactly.
	void squash_exact_copy()
	{
		const auto f        = sources{ };
		const auto selected = std::vector<private_source_selection>{ { f.Finding.Source, f.Finding.Source.Key, false } };
		const auto observed = std::vector<private_source_observation>{ f.finding_observation() };
		const auto state    = create_review_composition(review_composition_id{ 3 }, f.Binding, review_composition_mode::exact_copy, selected, observed);
		expect(state.has_value(), "a workspace from the finding is admitted");
		const auto copy = propose_composition_revision(*state, review_composition_revision_id{ 3 }, private_vertex_order{ 24 }, suggested, selected, observed);
		expect(copy.has_value(), "the finding's suggested comment is copied as the candidate");
		const auto result = accept(f, *copy, review_composition_revision_id{ 3 }, selected, observed, "the draft takes exactly the copied candidate");
		expect(result.Draft.Draft.Body == suggested && result.Proposal.Body == suggested, "the draft and the publication carry the suggested comment byte for byte");
		expect_no_private_metadata(result.Proposal.Body, "no private severity, category or runtime provenance is appended on promotion");
		const auto annotation = recognize_feedback_prefix(result.Proposal.Body);
		expect(annotation.Status == feedback_prefix_status::recognized && annotation.Label == feedback_label::issue && annotation.Expectation == feedback_expectation::unspecified,
			   "the public label is the one written; a critical severity does not make it (blocking)");
		// Lineage, kept privately: draft → composition revision → finding → run.
		const auto& link = *result.Draft.Composition;
		expect(link.Scope == f.Binding.Scope && link.Composition == state->Id && link.Revision == review_composition_revision_id{ 3 } && link.BodyDigest == private_digest(suggested),
			   "the draft's composition link names the accepted candidate");
		expect(copy->Revision.Derivation.size() == 1U && copy->Revision.Derivation.front().Kind == private_derivation_kind::copied_from
				   && copy->Revision.Derivation.front().Source == f.Finding.Source,
			   "the candidate derives from the finding");
		const auto* const from = std::get_if<private_finding_source>(&f.Finding.Source.Key);
		expect(from != nullptr && from->Run == f.Run.Run && std::get<private_agent_actor>(copy->Revision.Actor).Run == f.Run.Run,
			   "the finding names its run, whose provenance stays in the private store");
		expect(f.Finding.Finding.Severity == defect_severity::critical && f.Finding.Finding.Category == defect_category::security
				   && f.Run.Metadata.ProtocolReported.front().Settings.Model == std::string{ model_sentinel },
			   "severity, category and the reported model remain private facts of the source");
	}

	/// The reviewer chooses the comment's intent; the source's label is not
	/// imposed, and two labelled sources never make a two-label header.
	void squash_reviewer_authored()
	{
		const auto f        = sources{ };
		const auto selected = std::vector<private_source_selection>{ { f.Finding.Source, f.Finding.Source.Key, false } };
		const auto observed = std::vector<private_source_observation>{ f.finding_observation() };
		const auto state    = create_review_composition(review_composition_id{ 4 }, f.Binding, review_composition_mode::reviewer_authored, selected, observed);
		expect(state.has_value(), "a reviewer-authored workspace from the finding is admitted");
		constexpr auto chosen = std::string_view{ "question: Can the callback be null on the first frame?" };
		const auto     asked  = propose_composition_revision(*state, review_composition_revision_id{ 4 }, private_vertex_order{ 24 }, chosen, selected, observed);
		expect(asked.has_value(), "the reviewer writes the candidate");
		const auto result = accept(f, *asked, review_composition_revision_id{ 4 }, selected, observed, "the draft takes exactly the reviewer's candidate");
		expect(result.Proposal.Body == chosen && recognize_feedback_prefix(result.Proposal.Body).Label == feedback_label::question,
			   "the comment's intent is the reviewer's choice, not the finding's");
		expect(asked->Revision.Derivation.size() == 1U && asked->Revision.Derivation.front().Kind == private_derivation_kind::composed_from,
			   "and its lineage still names the finding");

		// Squash a labelled private revision and the finding together.
		const auto both     = std::vector<private_source_selection>{ { source_of(f.Note), source_of(f.Note).Key, false }, { f.Finding.Source, f.Finding.Source.Key, false } };
		const auto seen     = std::vector<private_source_observation>{ f.note_observation(), f.finding_observation() };
		const auto squashed = create_review_composition(review_composition_id{ 5 }, f.Binding, review_composition_mode::reviewer_authored, both, seen);
		expect(squashed.has_value(), "two labelled sources on one line are squashed into one workspace");
		expect(!create_review_composition(review_composition_id{ 6 }, f.Binding, review_composition_mode::exact_copy, both, seen),
			   "an exact copy of two sources is refused: their texts are never concatenated");
		constexpr auto written = std::string_view{ "issue: Guard the null callback, and rename it." };
		const auto     one     = propose_composition_revision(*squashed, review_composition_revision_id{ 5 }, private_vertex_order{ 24 }, written, both, seen);
		expect(one.has_value(), "the reviewer writes one comment for both");
		const auto merged     = accept(f, *one, review_composition_revision_id{ 5 }, both, seen, "the draft takes exactly the squashed candidate");
		const auto annotation = recognize_feedback_prefix(merged.Proposal.Body);
		expect(merged.Proposal.Body == written && annotation.Label == feedback_label::issue
				   && merged.Proposal.Body.find(f.NoteBody) == std::string::npos,
			   "one header, the reviewer's; the sources' leading labels are not stacked");
		expect(one->Revision.Derivation.size() == 2U, "and the lineage names both sources");
		expect_no_private_metadata(merged.Proposal.Body, "a squash appends no private metadata");
	}

	/// BPR-1210's note copy: each promotion replaces the draft's text.
	void note_copy_replaces()
	{
		const auto f     = sources{ };
		const auto draft = f.draft();
		const auto first = promote_private_text("question: Is the callback ever null?", draft);
		expect(first.has_value(), "a labelled note is copied into the draft");
		const auto once = apply(draft, *first);
		expect(once.Changed, "the draft takes the first note");
		const auto second = promote_private_text(f.NoteBody, once.Accepted);
		expect(second.has_value(), "a second labelled note is copied into the same draft");
		const auto twice = apply(once.Accepted, *second);
		expect(twice.Changed && twice.Accepted.Draft.Body == f.NoteBody, "the second promotion replaces the first, byte for byte");
		expect(recognize_feedback_prefix(twice.Accepted.Draft.Body).Label == feedback_label::nitpick && twice.Accepted.Draft.Body.find("question:") == std::string::npos,
			   "one header remains; two notes never become a stacked header");
		expect(!second->Composition.has_value(), "the copy seam carries no source metadata");
	}

	/// RFC-BPR-0012's finding conversion copies the suggested comment only.
	void finding_conversion()
	{
		const auto f       = sources{ };
		auto       request = agent_finding_promotion_request{ };
		request.Finding    = f.Finding.Finding;
		request.Verdict    = f.Finding.Verdict;
		request.Choice     = agent_finding_promotion_choice::draft_comment;
		request.Draft      = f.draft().Draft;
		const auto result  = promote_agent_finding(request);
		expect(result.promoted() && result.Draft.has_value(), "the finding becomes one local draft edit");
		expect(result.Draft->Body == suggested, "the draft body is the suggested comment, byte for byte");
		expect_no_private_metadata(result.Draft->Body, "no severity, category or provenance is written into the draft");
		expect(recognize_feedback_prefix(result.Draft->Body).Expectation == feedback_expectation::unspecified, "and no (blocking) is derived from a critical severity");
	}
}

auto main() -> int
{
	squash_exact_copy();
	squash_reviewer_authored();
	note_copy_replaces();
	finding_conversion();
	std::cout << "FEEDBACK_PRIVATE_PROMOTION_OK\n";
	return EXIT_SUCCESS;
}
