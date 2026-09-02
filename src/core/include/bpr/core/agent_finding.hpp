#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_HPP

#include "bpr/core/agent_profile.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/diff_model.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bpr::core {
	/// \brief The longest claim a finding may state.
	inline constexpr std::size_t maximum_finding_claim_bytes = 4U * 1024U;
	/// \brief The longest evidence a finding may quote for its claim.
	inline constexpr std::size_t maximum_finding_evidence_bytes = 16U * 1024U;
	/// \brief The longest consequence a finding may describe.
	inline constexpr std::size_t maximum_finding_consequence_bytes = 4U * 1024U;
	/// \brief The longest comment a finding may suggest a reviewer publish.
	inline constexpr std::size_t maximum_finding_comment_bytes = 16U * 1024U;
	/// \brief The longest statement of context a finding may say it needed.
	inline constexpr std::size_t maximum_finding_required_context_bytes = 4U * 1024U;
	/// \brief The most commits one finding may report having been analyzed against.
	inline constexpr std::size_t maximum_finding_analyzed_oids = 256U;

	/// \brief How sure an agent says it is.
	///
	/// \details Confidence is **data, never authority**. Nothing in this header
	/// reads it: `validate_agent_finding` does not branch on it, and does not
	/// even bound it, because a branch that refused an unknown enumerator would
	/// still be a branch in which confidence changed an anchoring outcome. A
	/// decoder that turns agent bytes into this type is where an enumerator
	/// nobody declared is refused; anchoring is not that place.
	///
	/// `unstated` is the default because an agent that said nothing about its
	/// own confidence has not thereby claimed the lowest one.
	enum class agent_finding_confidence : std::uint8_t {
		unstated,
		low,
		medium,
		high,
	};

	/// \brief One finding's position, expressed only in RFC-BPR-0004 identities.
	///
	/// \details Exactly one of `Line` or `Boundary` is engaged. Those are the
	/// only two identities in `diff_model.hpp` that name a *place*: a line ref
	/// names one line of one side, a boundary ref names the seam between two of
	/// them, and an agent saying "this is wrong" means one or the other.
	///
	/// `Segment`, `Group`, and `Move` are optional refinements rather than
	/// alternative positions. A segment or group covers a span and could not by
	/// itself say which line a claim is about, and a move relation names a pair
	/// of segments — admitting either as a position would let one finding anchor
	/// to two places at once. Carried as identities beside the position, they
	/// say what the frozen alignment already knew about that place, and they are
	/// checked against the snapshot rather than believed.
	///
	/// `line_pair_id` is deliberately absent. A pair is a derived correspondence
	/// between two lines that already have identities, so admitting it would
	/// give one position a second spelling and a promoter would have to choose.
	struct agent_finding_anchor
	{
		std::optional<source_line_ref>      Line;
		std::optional<source_boundary_ref>  Boundary;
		std::optional<alignment_segment_id> Segment;
		std::optional<change_group_id>      Group;
		std::optional<move_relation_id>     Move;

		auto operator<=>(const agent_finding_anchor&) const = default;
	};

	/// \brief One structured result an agent returned about a review.
	///
	/// \details Severity and category reuse the defect vocabulary deliberately.
	/// A second ladder would need a mapping the moment RFC-BPR-0012 promotes a
	/// finding into a private defect, and a mapping is where a severity gets
	/// silently widened or a category invented.
	///
	/// **There is no field here a Qt coordinate could occupy.** Not a
	/// y-position, not a viewport offset, not a scroll position, not a widget
	/// row, not a ribbon path. Position is stated only in RFC-BPR-0004
	/// identities, which are facts about the diff rather than about the screen
	/// showing it, so they survive scrolling, folding, wrapping, and a theme
	/// change. A finding that remembered where a line was drawn would point at
	/// different code the moment anything above it moved, and neither the
	/// reviewer nor the agent would be able to tell that it had.
	///
	/// `AnalyzedBaseOid` and `AnalyzedHeadOid` are the revision pair the finding
	/// was computed against, and they are what staleness is decided from.
	/// `AnalyzedCommits` is provenance the agent reports — which commits it
	/// actually read inside that range — and is bounded but never used to decide
	/// a verdict.
	struct agent_finding
	{
		agent_profile_id            Provider;
		defect_severity             Severity   = defect_severity::minor;
		agent_finding_confidence    Confidence = agent_finding_confidence::unstated;
		defect_category             Category   = defect_category::correctness;
		std::string                 Path;
		line_span                   Lines;
		agent_finding_anchor        Anchor;
		std::string                 Claim;
		std::string                 Evidence;
		std::string                 Consequence;
		std::optional<std::string>  SuggestedComment;
		std::string                 RequiredContext;
		git_commit_oid              AnalyzedBaseOid;
		git_commit_oid              AnalyzedHeadOid;
		std::vector<git_commit_oid> AnalyzedCommits;

		auto operator<=>(const agent_finding&) const = default;
	};

	/// \brief One file exactly as the frozen snapshot recorded it.
	///
	/// \details The line counts and identities a finding is checked against,
	/// and nothing else. This is a small input struct rather than a host or
	/// provider type on purpose: anchoring must be decidable from stated values
	/// alone, and a validator that could reach a live alignment could be handed
	/// a newer one.
	struct agent_finding_snapshot_file
	{
		std::string                       Path;
		file_comparison_digest            Comparison;
		std::uint32_t                     LeftLineCount  = 0;
		std::uint32_t                     RightLineCount = 0;
		std::vector<alignment_segment_id> Segments;
		std::vector<change_group_id>      Groups;
		std::vector<move_relation_id>     Moves;

		auto operator<=>(const agent_finding_snapshot_file&) const = default;
	};

	/// \brief The frozen facts a finding may be checked against.
	///
	/// \details `Binding` is the session's own `agent_revision_binding` — what
	/// the agent was asked about — not what the reviewer is looking at now.
	/// Checking a finding against the current head is the mistake this type
	/// exists to make impossible to write by accident.
	struct agent_finding_snapshot
	{
		agent_revision_binding                   Binding;
		std::vector<agent_finding_snapshot_file> Files;

		auto operator<=>(const agent_finding_snapshot&) const = default;
	};

	/// \brief Whether a finding's anchor holds, and if not, in which way.
	enum class agent_finding_anchor_state : std::uint8_t {
		/// The frozen snapshot contains exactly what the anchor names.
		anchored,
		/// It does not, and no substitute was found because none is looked for.
		unanchored,
		/// The finding was analyzed against a different revision entirely.
		stale,
	};

	/// \brief The number of declared anchor states.
	///
	/// \details Three, and the static assertion below keeps it three. A fourth
	/// state would be somewhere between anchored and refused, and a caller
	/// would have to guess which side of the line it fell on.
	inline constexpr std::size_t agent_finding_anchor_state_count = 3U;

	static_assert(
		static_cast<std::size_t>(agent_finding_anchor_state::stale) + 1U
			== agent_finding_anchor_state_count,
		"agent_finding_anchor_state_count must cover every declared state");

	/// \brief Why an anchor did not hold.
	enum class agent_finding_refusal : std::uint8_t {
		/// Not a refusal. The state is anchored or stale.
		none,
		/// No file in the frozen snapshot has that exact path. Paths are
		/// matched exactly: there is no nearest, prefix, or case-folded match,
		/// because a finding attached to a file nobody named is worse than one
		/// attached to nothing.
		unknown_path,
		/// The claimed range, or the anchored line itself, runs past the end of
		/// that side of the file as the snapshot froze it.
		span_past_end,
		/// The finding is not a well-formed finding: a missing or doubled
		/// position, a range that cannot contain its own anchor, a declared
		/// bound exceeded, an unusable revision, or an identity the frozen
		/// alignment does not contain.
		malformed,
	};

	/// \brief The number of declared refusal reasons.
	inline constexpr std::size_t agent_finding_refusal_count = 4U;

	static_assert(
		static_cast<std::size_t>(agent_finding_refusal::malformed) + 1U
			== agent_finding_refusal_count,
		"agent_finding_refusal_count must cover every declared refusal");

	/// \brief The verdict on one finding's anchor.
	///
	/// \details **There is no field here a line could occupy**, which is how
	/// "never a nearest or inferred line" is a property of the type rather than
	/// of the implementation. A refusal reports that it refused; it does not
	/// hand back somewhere else to look.
	///
	/// The analyzed revisions are copied onto every verdict, refusals included.
	/// A stale verdict without them would be a refusal nobody could act on: the
	/// reviewer needs to see which revision the answer was about in order to
	/// decide between continuing against it and asking again.
	///
	/// The default is a refusal. A verdict that had to be filled in to mean
	/// "no" would eventually be returned half-filled.
	struct agent_finding_verdict
	{
		agent_finding_anchor_state State
			= agent_finding_anchor_state::unanchored;
		agent_finding_refusal Refusal = agent_finding_refusal::malformed;
		git_commit_oid        AnalyzedBaseOid;
		git_commit_oid        AnalyzedHeadOid;

		/// \brief Whether the frozen snapshot contains what the anchor names.
		[[nodiscard]] constexpr auto anchored() const noexcept -> bool
		{
			return State == agent_finding_anchor_state::anchored;
		}

		auto operator<=>(const agent_finding_verdict&) const = default;
	};

	/// \brief Decides whether one finding's anchor holds against a frozen snapshot.
	///
	/// \details Pure: decided from the two supplied values alone. No clock, no
	/// provider, no filesystem, and no current head — a verdict that consulted
	/// any of those could not be replayed, and the last of them is the failure
	/// RFC-BPR-0012 names outright.
	///
	/// Decided in three stages, and the order is itself the policy:
	///
	/// 1. Whether this is a well-formed finding at all. These checks read only
	///    the finding, so a malformed one reports that whatever it is compared
	///    against.
	/// 2. Whether it was analyzed against the revision the snapshot froze. A
	///    finding from another revision is stale, and is refused *before* any
	///    line count is consulted. Measuring it against the newer snapshot is
	///    exactly the silent reattachment the RFC forbids, and it would
	///    sometimes succeed — which is worse than always failing.
	/// 3. Whether the frozen snapshot actually contains what the anchor names.
	///
	/// A snapshot the finding cannot be matched against yields stale rather than
	/// anchored. Refusing to anchor is the only safe direction to fail in.
	///
	/// \returns A verdict carrying the finding's own analyzed revisions in every
	/// state, and never a line the finding did not name.
	[[nodiscard]] auto validate_agent_finding(
		const agent_finding&          aFinding,
		const agent_finding_snapshot& aSnapshot) noexcept
		-> agent_finding_verdict;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_AGENT_FINDING_HPP
