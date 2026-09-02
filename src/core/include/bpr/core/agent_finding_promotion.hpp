#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_PROMOTION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_PROMOTION_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace bpr::core {
	/// \brief What the reviewer chose to make of one finding.
	///
	/// \details Three local artifacts, and there is deliberately no fourth. A
	/// published comment is not on this list because publication is not a thing
	/// a finding can be turned into: it is a separate gesture the reviewer
	/// makes afterwards, against text they have had the chance to read.
	///
	/// Dismissal is not on this list either. Dismissing a finding produces no
	/// artifact, so it is not a promotion; it is the reviewer's standing
	/// decision about the finding, and it arrives as
	/// `agent_finding_standing::dismissed` instead.
	enum class agent_finding_promotion_choice : std::uint8_t {
		draft_comment,
		defect,
		private_note,
	};

	/// \brief The number of declared promotion choices.
	inline constexpr std::size_t agent_finding_promotion_choice_count = 3U;

	static_assert(
		static_cast<std::size_t>(agent_finding_promotion_choice::private_note) + 1U
			== agent_finding_promotion_choice_count,
		"agent_finding_promotion_choice_count must cover every declared choice");

	/// \brief Whether the reviewer has already ruled this finding out.
	///
	/// \details `delivered` means the finding arrived and the reviewer has not
	/// decided about it, which is not the same as having accepted it. A
	/// dismissed finding stays dismissed whatever anyone later chooses for it,
	/// so this is carried into promotion rather than being consulted only by
	/// the queue that displays it.
	enum class agent_finding_standing : std::uint8_t {
		delivered,
		dismissed,
	};

	/// \brief Everything one promotion decision is allowed to read.
	///
	/// \details There is no store, no clock, no host port, and no provider
	/// handle here, which is what keeps promotion decidable under `core-only`.
	/// The verdict is supplied rather than recomputed because deciding whether
	/// an anchor holds needs the frozen snapshot, and `validate_agent_finding`
	/// is the one place that decision is made; recomputing it here would create
	/// a second answer to the same question.
	///
	/// `Draft` and `Defect` are the local artifact the reviewer is promoting
	/// *onto*. Neither `review_draft_command` nor `defect_command` carries an
	/// anchor, so the anchor cannot travel inside the command — it can only be
	/// checked, and checking it needs the target itself. Both are optional
	/// because a promotion names one target at most; the choice selects which
	/// one is required, and the other is ignored rather than filled.
	///
	/// The default choice is the private note. A request nobody filled in
	/// should be refused for the verdict it does not have rather than for a
	/// target it was never going to name, and a note is the only choice whose
	/// target is the finding itself.
	struct agent_finding_promotion_request
	{
		agent_finding                  Finding;
		agent_finding_verdict          Verdict;
		agent_finding_promotion_choice Choice
			= agent_finding_promotion_choice::private_note;
		agent_finding_standing              Standing = agent_finding_standing::delivered;
		std::optional<review_comment_draft> Draft;
		std::optional<defect_record>        Defect;

		auto operator<=>(const agent_finding_promotion_request&) const = default;
	};

	/// \brief Why a finding produced no command.
	///
	/// \details Each value is a different mistake with a different repair, in
	/// the shape `review_draft_rejection` and `defect_rejection` already use. A
	/// single "refused" would tell the queue nothing it could show a reviewer,
	/// and "nothing happened" is the one answer a promotion must never give.
	enum class agent_finding_promotion_refusal : std::uint8_t {
		/// Not a refusal. Exactly one command was produced.
		none,
		/// The request names an enumerator nobody declared, so there is no
		/// artifact to make. Refused rather than resolved to a nearby choice.
		unknown_choice,
		/// The reviewer already dismissed this finding. Their decision is read
		/// before the verdict: it holds whatever the anchor turned out to do.
		dismissed,
		/// The verdict refused the anchor. Promotion attaches a finding to
		/// exactly the place it named or to nothing at all.
		unanchored,
		/// The finding was analyzed against another revision. It is not
		/// re-anchored against this one, which is the whole point of staleness.
		stale,
		/// The verdict was reached about a different finding: the two disagree
		/// about the revisions they were computed against. A verdict is only
		/// evidence about the finding it was computed for.
		foreign_verdict,
		/// The finding does not carry what a promotion copies: a path that
		/// names a file, exactly one semantic position, the comparison that
		/// position belongs to, and a claim.
		malformed_finding,
		/// The chosen artifact was not supplied, so the command would have to
		/// name a draft or defect the caller never identified.
		unnamed_target,
		/// The supplied draft or defect is anchored at a different line. The
		/// finding is not moved onto it, and the target is not moved either.
		anchor_elsewhere,
		/// The finding is anchored to a seam between lines. A draft comment and
		/// a defect are both anchored by `source_line_ref`, and the line above
		/// or below a seam is a line the agent did not name.
		boundary_anchor,
		/// The agent proposed no comment, and promotion writes none of its own.
		no_suggested_comment,
	};

	/// \brief The number of declared refusal reasons.
	inline constexpr std::size_t agent_finding_promotion_refusal_count = 11U;

	static_assert(
		static_cast<std::size_t>(
			agent_finding_promotion_refusal::no_suggested_comment)
				+ 1U
			== agent_finding_promotion_refusal_count,
		"agent_finding_promotion_refusal_count must cover every declared reason");

	/// \brief Exactly one local command, or a refusal naming why there is none.
	///
	/// \details **There is no field here a `review_publication_proposal` could
	/// occupy**, and that is how "a finding can never publish" is a property of
	/// the type rather than of the implementation. The three members are the
	/// three local artifacts; `propose_publication` takes a
	/// `review_comment_draft` the reviewer has already begun publishing, and
	/// nothing on this type is one. A promotion cannot even ask for that state:
	/// the draft command it emits is always an `edit`.
	///
	/// At most one member is engaged, and `command_count` exists so that is
	/// checkable rather than merely intended. A promotion that produced a draft
	/// *and* opened a defect would be two decisions the reviewer made once.
	///
	/// The default is a refusal, and it is the same refusal a default-built
	/// request earns: an unfilled promotion is already an unanchored one, so a
	/// result that was never filled in cannot be read as permission.
	struct agent_finding_promotion
	{
		std::optional<review_draft_command> Draft;
		std::optional<defect_command>       Defect;
		std::optional<supplement_note>      Note;
		agent_finding_promotion_refusal     Refusal
			= agent_finding_promotion_refusal::unanchored;

		/// \brief How many commands this result carries. One, or none.
		[[nodiscard]] constexpr auto command_count() const noexcept -> std::size_t
		{
			return static_cast<std::size_t>(Draft.has_value())
				+ static_cast<std::size_t>(Defect.has_value())
				+ static_cast<std::size_t>(Note.has_value());
		}

		/// \brief Whether the finding became exactly one local artifact.
		[[nodiscard]] constexpr auto promoted() const noexcept -> bool
		{
			return Refusal == agent_finding_promotion_refusal::none
				&& command_count() == 1U;
		}

		auto operator<=>(const agent_finding_promotion&) const = default;
	};

	/// \brief A claim always fits a note, so a note is never shortened.
	///
	/// \details The note's text is the claim copied whole. This holds the two
	/// declared bounds in the relation that makes that safe, so a later change
	/// to either one fails here rather than in a truncation nobody wrote.
	static_assert(
		maximum_finding_claim_bytes <= maximum_note_bytes,
		"a promoted note carries the claim verbatim, so the claim bound must fit "
		"inside the note bound");

	/// \brief Turns one finding into one local draft, defect, or private note.
	///
	/// \details Pure: decided from the supplied request alone. It reaches no
	/// store, so the note it returns is a payload rather than a write, and
	/// making it durable as an `application::local_note_record` stays the
	/// host's job.
	///
	/// Decided in a ladder, and the order is the policy:
	///
	/// 1. Whether this is a promotion request at all — a choice nobody
	///    declared names no artifact.
	/// 2. Whether the reviewer already dismissed the finding. Their decision
	///    is read before the verdict because it does not depend on it.
	/// 3. Whether the verdict is about *this* finding, and only then what it
	///    says. A verdict computed for another finding is not evidence about
	///    this one, and believing its state would be the silent reattachment
	///    RFC-BPR-0012 forbids arriving by another road.
	/// 4. Whether the finding carries the facts a promotion copies.
	/// 5. Whether the chosen target can actually be filled. Where it cannot,
	///    the answer is a refusal rather than a default: a command naming a
	///    draft nobody identified, or carrying a body nobody wrote, is worse
	///    than no command.
	///
	/// **Nothing is composed.** The draft body is the suggested comment copied
	/// byte for byte, and the note text is the claim copied byte for byte.
	/// Evidence and consequence are deliberately not folded into the note:
	/// `maximum_finding_evidence_bytes` alone equals `maximum_note_bytes`, so a
	/// composed note could only be repaired by truncating it or by refusing a
	/// well-formed finding, and the queue still shows both fields beside the
	/// claim.
	///
	/// **The anchor is never translated.** A finding's position is already
	/// stated in the RFC-BPR-0004 identities that `review_comment_draft` and
	/// `defect_record` anchor themselves by, so there is no mapping to get
	/// wrong — but neither command carries an anchor field, so the anchor
	/// cannot ride along inside one. It is checked instead: the named draft or
	/// defect must be anchored at exactly the `source_line_ref` the finding
	/// names, and a mismatch is refused. Without that, promotion would move the
	/// agent's claim onto whichever line the caller happened to pass, which is
	/// an inferred anchor arriving from the other direction. A boundary anchor
	/// fills neither, because the line above or below a seam is one the agent
	/// did not name; such a finding remains promotable into a note, which is
	/// attached by file comparison rather than by line.
	///
	/// No `review_mutation_target` is built here, and that is deliberate. It
	/// names a host, repository, pull request, and expected head — the value
	/// `revalidate` and `publication_permitted` consume — and assembling one
	/// from a finding would hand the host a half-formed publication for
	/// something the reviewer has not yet read.
	///
	/// Severity and category are copied straight across, because
	/// `agent_finding` already states them in `defect_severity` and
	/// `defect_category`. There is no ladder mapping, which is where a severity
	/// would otherwise be widened. They arrive as a `reclassify`: a defect is a
	/// local projection over an ordinary review thread and nothing in
	/// `defect_command_kind` creates one, so promotion classifies the defect the
	/// reviewer promoted onto rather than inventing a kind that would file it.
	/// It moves no lifecycle state, so promoting a finding never claims a fix,
	/// verifies anything, or settles a defect on the reviewer's behalf.
	///
	/// Legality of the resulting command is not re-decided here. `apply`
	/// already refuses an edit to a publishing draft and a reclassification of
	/// a settled defect, and repeating those rules would create a second
	/// authority that could disagree with the first.
	///
	/// \returns Exactly one command, or a refusal naming why there is none.
	[[nodiscard]] auto promote_agent_finding(
		const agent_finding_promotion_request& aRequest) -> agent_finding_promotion;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_PROMOTION_HPP
