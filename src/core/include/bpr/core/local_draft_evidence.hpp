#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_EVIDENCE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_DRAFT_EVIDENCE_HPP

#include "bpr/core/draft_evidence.hpp"
#include "bpr/core/local_draft.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Decisions over a local draft's evidence (RFC-BPR-0109): recording
/// and revising claims, checks, results and questions; the author's accept
/// or reject of what an import proposes; the one BPR import format; binding
/// a result to a candidate by the material it was produced on; whether a
/// result still applies to a later candidate; what a method's result can
/// speak for; and what is selected for later description and tour use.
/// \details Every transition names the draft version it read, as the draft's
/// own transitions do, and returns the next version or a refusal; nothing is
/// applied in part. The host reads the import document and the artifacts it
/// names, through the read-only workspace port, and hands them in as data.
/// Nothing here runs a command, reads a secret store or labels a result
/// observed.
namespace bpr::core {
	// --- Entered in the workbench ---------------------------------------------

	[[nodiscard]] auto add_claim(const local_draft& aDraft, std::uint64_t aExpectedVersion, claim_kind aKind, std::string aText, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	/// \brief A new wording; the earlier one is kept with the claim.
	[[nodiscard]] auto revise_claim(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aClaim, std::string aText, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	/// \brief A planned check of the given claims.
	[[nodiscard]] auto add_check(const local_draft& aDraft, std::uint64_t aExpectedVersion, check_method aMethod, std::string aTitle, std::string aCommand,
								 std::vector<std::uint64_t> aClaims, formal_scope aFormal, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;

	/// \brief A result the author reports from the workbench.
	struct manual_result
	{
		std::uint64_t    Check   = 0;
		evidence_outcome Outcome = evidence_outcome::inconclusive;
		/// The candidate it was produced on, by ordinal; 0 leaves it unbound.
		std::uint64_t            Candidate = 0;
		std::vector<std::string> Inputs;
		std::string              Environment;
		std::string              Expected;
		std::string              Actual;
		std::string              Limitations;
	};
	[[nodiscard]] auto record_result(const local_draft& aDraft, std::uint64_t aExpectedVersion, manual_result aResult, std::int64_t aNow) -> std::expected<local_draft, draft_refusal>;
	[[nodiscard]] auto add_question(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::string aText, std::uint64_t aClaim, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	[[nodiscard]] auto resolve_question(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aQuestion, bool aResolved, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	/// \brief Accepts or rejects a claim, check or question. Nothing returns
	/// to proposed, and deciding never touches a result.
	[[nodiscard]] auto decide_evidence(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aRecord, evidence_disposition aDisposition, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;
	/// \brief Selects a claim, result or question for later description and
	/// tour use, or leaves it out.
	[[nodiscard]] auto select_evidence(const local_draft& aDraft, std::uint64_t aExpectedVersion, std::uint64_t aRecord, bool aSelected, std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>;

	// --- The import ---------------------------------------------------------------

	/// \brief Why an import was refused whole. Nothing of it is applied.
	enum class evidence_import_refusal : std::uint8_t {
		/// Larger than `maximum_evidence_import_bytes`; not read.
		too_large,
		/// Not a JSON document of the expected shape.
		malformed,
		/// Not a `bpr.evidence-import` document.
		format,
		/// A version this build does not read.
		version,
		/// A record or field this version does not define.
		unknown_field,
		/// A result says it was observed. Only BPR can observe an execution,
		/// and nothing here does.
		observed,
		/// A value outside its kind: an unknown outcome or method, a malformed
		/// identity or path, text over its bound.
		invalid_value,
		/// A key names no record of the document, or an identity no record of the draft.
		reference,
		/// The draft would hold more records than it keeps.
		bounds,
		/// This exact document was imported before.
		duplicate,
		/// The document has no records.
		empty,
		/// The draft changed while the import was read; import it again.
		stale,
	};
	[[nodiscard]] auto name(evidence_import_refusal aRefusal) noexcept -> std::string_view;

	struct evidence_import_refused
	{
		evidence_import_refusal Reason = evidence_import_refusal::malformed;
		/// Where: `results[2].outcome`, say. Never the document's text.
		std::string Detail;
		auto        operator<=>(const evidence_import_refused&) const = default;
	};

	/// \brief An artifact the import names: a local path relative to the
	/// import's folder, or a link.
	struct evidence_artifact_request
	{
		std::string Name;
		std::string Path;
		std::string Url;
		auto        operator<=>(const evidence_artifact_request&) const = default;
	};

	/// \brief A parsed import, not yet applied. Its keys are the document's own.
	struct evidence_import_document
	{
		evidence_origin Origin;
		std::string     Digest;
		struct claim
		{
			std::string Key;
			claim_kind  Kind = claim_kind::other;
			std::string Text;
		};
		struct check
		{
			std::string              Key;
			check_method             Method = check_method::test;
			std::string              Title;
			std::string              Command;
			std::vector<std::string> Claims;
			formal_scope             Formal;
		};
		struct result
		{
			/// A check of this document, by key, or of the draft, by identity.
			std::string                            CheckKey;
			std::uint64_t                          CheckId = 0;
			evidence_outcome                       Outcome = evidence_outcome::inconclusive;
			std::optional<evidence_material>       Material;
			std::vector<std::string>               Inputs;
			std::string                            Environment;
			std::string                            Expected;
			std::string                            Actual;
			std::string                            Limitations;
			std::vector<evidence_artifact_request> Artifacts;
		};
		struct question
		{
			std::string Text;
			std::string ClaimKey;
		};
		std::vector<claim>    Claims;
		std::vector<check>    Checks;
		std::vector<result>   Results;
		std::vector<question> Questions;
	};

	/// \brief Reads one import document. The whole document is refused for
	/// anything this version does not define, before anything is applied.
	[[nodiscard]] auto parse_evidence_import(std::string_view aDocument) -> std::expected<evidence_import_document, evidence_import_refused>;
	/// \brief Every artifact the document names, results in order; the host
	/// resolves each into one `evidence_artifact`, in the same order.
	[[nodiscard]] auto evidence_artifact_requests(const evidence_import_document& aDocument) -> std::vector<evidence_artifact_request>;
	/// \brief Applies a parsed import: claims, checks and questions arrive
	/// proposed; results arrive reported, attached to their checks and bound
	/// by material. `aArtifacts` are the host's resolutions of
	/// `evidence_artifact_requests`, in order.
	[[nodiscard]] auto import_evidence(const local_draft& aDraft, std::uint64_t aExpectedVersion, const evidence_import_document& aDocument, std::span<const evidence_artifact> aArtifacts,
									   std::int64_t aNow) -> std::expected<local_draft, evidence_import_refused>;

	// --- Binding and currency -------------------------------------------------------

	/// \brief The candidate (by ordinal) whose exact material this is: the
	/// same commit and the same working files with the same contents, no
	/// more and no fewer. The latest such candidate; 0 when none is. A commit
	/// alone never binds a candidate that includes working files, and a
	/// matching file name is never enough.
	[[nodiscard]] auto bind_material(const local_draft& aDraft, const evidence_material& aMaterial) -> std::uint64_t;
	/// \brief The material of one of the draft's candidates.
	[[nodiscard]] auto candidate_material(const local_draft_candidate& aCandidate) -> evidence_material;

	/// \brief Whether a result speaks for one candidate.
	enum class evidence_currency : std::uint8_t {
		/// Produced on this candidate's material.
		current,
		/// Produced on an earlier candidate whose recorded inputs this one
		/// leaves unchanged, against the same base.
		carried,
		/// Produced on an earlier candidate; something it may depend on changed,
		/// or what it depends on is unknown.
		needs_recheck,
		/// Not established as any candidate's material.
		unbound,
		/// Produced on a later candidate.
		later,
	};
	[[nodiscard]] auto name(evidence_currency aCurrency) noexcept -> std::string_view;
	[[nodiscard]] auto currency(const local_draft& aDraft, const evidence_result& aResult, std::uint64_t aCandidate) -> evidence_currency;

	/// \brief What a method's result can speak for. None speaks for the whole
	/// change or the pull request.
	enum class evidence_reach : std::uint8_t {
		/// The tested material, under its environment.
		material,
		/// The measured environment only.
		environment,
		/// The model analyzed, within its bounds; not the code.
		model,
		/// The stated property under its assumptions.
		property,
		/// A reading, not an execution.
		reading,
	};
	[[nodiscard]] auto name(evidence_reach aReach) noexcept -> std::string_view;
	[[nodiscard]] auto reach(check_method aMethod) noexcept -> evidence_reach;

	/// \brief Counts for a claim against one candidate. There is no verdict:
	/// a claim is never verified by its results.
	struct claim_evidence
	{
		std::size_t Planned                                  = 0;
		std::size_t Passing                                  = 0;
		std::size_t Failing                                  = 0;
		std::size_t Other                                    = 0;
		std::size_t NeedsRecheck                             = 0;
		std::size_t Unbound                                  = 0;
		auto        operator<=>(const claim_evidence&) const = default;
	};
	[[nodiscard]] auto evidence_for_claim(const local_draft& aDraft, std::uint64_t aClaim, std::uint64_t aCandidate) -> claim_evidence;

	// --- Selection ----------------------------------------------------------------

	/// \brief What a later description or tour may use: accepted, selected
	/// claims without their earlier wordings, selected results of accepted
	/// checks with those checks, and selected accepted questions. Nothing
	/// unselected, proposed, rejected or historical is in it.
	struct evidence_selection
	{
		std::vector<evidence_claim>    Claims;
		std::vector<evidence_check>    Checks;
		std::vector<evidence_result>   Results;
		std::vector<evidence_question> Questions;
		auto                           operator<=>(const evidence_selection&) const = default;
	};
	[[nodiscard]] auto selected_evidence(const local_draft& aDraft) -> evidence_selection;

	// --- Stored form ----------------------------------------------------------------

	/// \brief The evidence as the draft's record stores it: compact JSON.
	[[nodiscard]] auto encode_draft_evidence(const draft_evidence& aEvidence) -> std::string;
	[[nodiscard]] auto decode_draft_evidence(std::string_view aText) -> std::optional<draft_evidence>;
}

#endif
