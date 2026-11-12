#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DRAFT_EVIDENCE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DRAFT_EVIDENCE_HPP

#include "bpr/core/git_objects.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief The evidence an author builds while preparing a local PR draft
/// (RFC-BPR-0109): claims, checks, results, artifacts and open questions.
/// \details These are the record's shapes only; the decisions over them
/// (import, binding, currency, scope and selection) are in
/// `local_draft_evidence.hpp`. Nothing here is a reviewer's verification or
/// an approval, and no result is ever an execution BPR observed: every
/// result is the author's or a producer's report. The observed class is
/// reserved for a later BPR-supervised action and has no value here.
namespace bpr::core {
	inline constexpr std::size_t maximum_evidence_claims          = 256U;
	inline constexpr std::size_t maximum_evidence_checks          = 256U;
	inline constexpr std::size_t maximum_evidence_results         = 1024U;
	inline constexpr std::size_t maximum_evidence_questions       = 256U;
	inline constexpr std::size_t maximum_evidence_imports         = 256U;
	inline constexpr std::size_t maximum_evidence_artifacts       = 32U;
	inline constexpr std::size_t maximum_evidence_inputs          = 256U;
	inline constexpr std::size_t maximum_evidence_claim_revisions = 32U;
	inline constexpr std::size_t maximum_evidence_text_bytes      = 16U * 1024U;
	inline constexpr std::size_t maximum_evidence_name_bytes      = 256U;
	/// \brief An import document larger than this is refused before it is read.
	inline constexpr std::uint64_t maximum_evidence_import_bytes = 16U * 1024U * 1024U;
	/// \brief The most bytes of one artifact copied into the private store.
	inline constexpr std::uint64_t maximum_evidence_artifact_bytes = 16U * 1024U * 1024U;

	/// \brief Who produced a record.
	enum class evidence_source : std::uint8_t {
		/// The author, in the workbench or in an import they wrote.
		author,
		/// An external tool or companion skill, through an import.
		tool,
		/// An agent (an Agent tab agent under its build allowance, say),
		/// through an import. Its claimed pass is a report.
		agent,
	};
	[[nodiscard]] auto name(evidence_source aSource) noexcept -> std::string_view;

	struct evidence_origin
	{
		evidence_source Source = evidence_source::author;
		/// The producer's name and version, as the import stated them.
		std::string Producer;
		std::string Version;
		/// The import that brought the record (1-based), or 0 when it was
		/// entered in the workbench.
		std::uint64_t Import                                    = 0;
		auto          operator<=>(const evidence_origin&) const = default;
	};

	/// \brief The author's decision on a proposed claim, check or question.
	/// What the author enters is accepted; what an import brings is
	/// proposed until the author accepts or rejects it.
	enum class evidence_disposition : std::uint8_t {
		proposed,
		accepted,
		rejected,
	};
	[[nodiscard]] auto name(evidence_disposition aDisposition) noexcept -> std::string_view;

	enum class claim_kind : std::uint8_t {
		bug_fix,
		preserved_behavior,
		compatibility,
		performance,
		formal_property,
		other,
	};
	[[nodiscard]] auto name(claim_kind aKind) noexcept -> std::string_view;

	/// \brief What the author says the change does. A claim has no verified
	/// state: results stand beside it, they never settle it.
	struct evidence_claim
	{
		std::uint64_t Id   = 0;
		claim_kind    Kind = claim_kind::other;
		std::string   Text;
		/// Earlier wordings, oldest first. A revision keeps them.
		std::vector<std::string> Earlier;
		evidence_disposition     Disposition = evidence_disposition::accepted;
		evidence_origin          Origin;
		bool                     Selected                                 = false;
		auto                     operator<=>(const evidence_claim&) const = default;
	};

	/// \brief How a check examines the change. Methods are not grades of
	/// one assurance and are never interchanged.
	enum class check_method : std::uint8_t {
		test,
		benchmark,
		fuzz,
		property_test,
		inspection,
		/// A model checker run over a model (TLA+, Alloy, a bounded checker).
		model_check,
		/// A proof assistant or verifier run over a stated property.
		proof,
		other,
	};
	[[nodiscard]] auto name(check_method aMethod) noexcept -> std::string_view;
	/// \brief A model check or a proof: its result concerns what it analyzed
	/// within its bounds, never the whole change.
	[[nodiscard]] auto formal(check_method aMethod) noexcept -> bool;

	/// \brief What a formal check analyzed. Correspondence with production
	/// code is recorded only when it was checked, and separately.
	struct formal_scope
	{
		std::string Property;
		/// The model or implementation analyzed.
		std::string Subject;
		/// Tool and version.
		std::string Tool;
		std::string Assumptions;
		std::string Bounds;
		std::string Correspondence;
		auto        operator<=>(const formal_scope&) const = default;
	};

	/// \brief A check: planned while it has no result. Its command is text
	/// to read, never something BPR runs.
	struct evidence_check
	{
		std::uint64_t              Id     = 0;
		check_method               Method = check_method::test;
		std::string                Title;
		std::string                Command;
		std::vector<std::uint64_t> Claims;
		formal_scope               Formal;
		evidence_disposition       Disposition = evidence_disposition::accepted;
		evidence_origin            Origin;
		auto                       operator<=>(const evidence_check&) const = default;
	};

	enum class evidence_outcome : std::uint8_t {
		pass,
		fail,
		inconclusive,
		not_run,
		interrupted,
	};
	[[nodiscard]] auto name(evidence_outcome aOutcome) noexcept -> std::string_view;

	/// \brief One working file of the tested material: its object, or
	/// nothing when the file was deleted.
	struct evidence_working_file
	{
		std::string                  Path;
		std::optional<git_entry_oid> Object;
		auto                         operator<=>(const evidence_working_file&) const = default;
	};

	/// \brief The material a result was produced on: a commit and every
	/// working file that differed from it. A commit alone means a clean
	/// checkout of that commit.
	struct evidence_material
	{
		git_commit_oid                     Head;
		std::vector<evidence_working_file> Working;
		auto                               operator<=>(const evidence_material&) const = default;
	};

	enum class artifact_state : std::uint8_t {
		/// Copied into the private store; `Object` is its content identity.
		stored,
		/// A remote link: a reference, never downloaded.
		link,
		/// The named file was not there.
		missing,
		/// Larger than `maximum_evidence_artifact_bytes`; not copied.
		too_large,
		/// Outside the import's folder, through a link, or not a regular file.
		refused,
	};
	[[nodiscard]] auto name(artifact_state aState) noexcept -> std::string_view;

	struct evidence_artifact
	{
		std::string    Name;
		artifact_state State = artifact_state::missing;
		/// The URL, or the path as the import named it.
		std::string                  Reference;
		std::optional<git_entry_oid> Object;
		std::uint64_t                Size                                        = 0;
		auto                         operator<=>(const evidence_artifact&) const = default;
	};

	/// \brief One reported result of one check. Results are added, never
	/// rewritten: an earlier result keeps its original scope.
	struct evidence_result
	{
		std::uint64_t    Id      = 0;
		std::uint64_t    Check   = 0;
		evidence_outcome Outcome = evidence_outcome::inconclusive;
		evidence_origin  Origin;
		/// What was tested, as reported; absent when nothing was named.
		std::optional<evidence_material> Material;
		/// The candidate whose material it is (by ordinal), or 0 while the
		/// material is not established as one of this draft's candidates.
		std::uint64_t Candidate = 0;
		/// The paths (a directory ends in `/`) the result depends on. Empty
		/// means unknown: any later candidate needs it rechecked.
		std::vector<std::string>       Inputs;
		std::string                    Environment;
		std::string                    Expected;
		std::string                    Actual;
		std::string                    Limitations;
		std::vector<evidence_artifact> Artifacts;
		std::int64_t                   Recorded                                  = 0;
		bool                           Selected                                  = false;
		auto                           operator<=>(const evidence_result&) const = default;
	};

	struct evidence_question
	{
		std::uint64_t Id = 0;
		std::string   Text;
		/// The claim it concerns, or 0.
		std::uint64_t        Claim       = 0;
		bool                 Resolved    = false;
		evidence_disposition Disposition = evidence_disposition::accepted;
		evidence_origin      Origin;
		bool                 Selected                                    = false;
		auto                 operator<=>(const evidence_question&) const = default;
	};

	/// \brief One accepted import, kept as history.
	struct evidence_import
	{
		std::uint64_t   Ordinal = 0;
		evidence_origin Origin;
		/// SHA-256 of the document, lowercase hex.
		std::string   Digest;
		std::uint64_t Claims                                    = 0;
		std::uint64_t Checks                                    = 0;
		std::uint64_t Results                                   = 0;
		std::uint64_t Questions                                 = 0;
		std::int64_t  At                                        = 0;
		auto          operator<=>(const evidence_import&) const = default;
	};

	/// \brief A draft's evidence. Every record has an identity from one
	/// sequence that is never reused.
	struct draft_evidence
	{
		std::vector<evidence_claim>    Claims;
		std::vector<evidence_check>    Checks;
		std::vector<evidence_result>   Results;
		std::vector<evidence_question> Questions;
		std::vector<evidence_import>   Imports;
		std::uint64_t                  NextId                                   = 1;
		auto                           operator<=>(const draft_evidence&) const = default;
	};
	/// \brief Every record within its bounds, every reference resolved.
	[[nodiscard]] auto valid(const draft_evidence& aEvidence) -> bool;
	/// \brief Every artifact object the evidence keeps.
	[[nodiscard]] auto evidence_objects(const draft_evidence& aEvidence) -> std::vector<git_entry_oid>;
}

#endif
