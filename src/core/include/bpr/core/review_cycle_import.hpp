#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IMPORT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IMPORT_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_round.hpp"
#include "bpr/core/review_cycle_workspace.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// \file
/// \brief Rounds received from elsewhere (RFC-BPR-0056, BPR-1160, BPR-1162).
/// \details An imported round is kept apart from local authoring, in a
/// private quarantine of the profile's cycle store: its canonical payload is
/// immutable, its sender's identities (cycle, round, items, suggestions and
/// project) are kept exactly as sent and never reinterpreted as local ones,
/// and nothing in it is applied to any source. Importing the same round again
/// finds the same record.
namespace bpr::core {
	using cycle_import_id = strong_id<struct cycle_import_id_tag>;

	/// \brief What one imported round's record says about it, beside its
	/// immutable payload.
	struct cycle_import_summary
	{
		cycle_import_id    Id;
		cycle_round_digest Digest;
		/// The sender's wire identities, exactly as sent.
		std::string ForeignCycle;
		std::string ForeignRound;
		std::string ForeignProject;
		auto        operator<=>(const cycle_import_summary&) const = default;
	};
	/// \brief The summary a payload gives an import record.
	[[nodiscard]] auto summarize_import(const cycle_decoded_round& aRound) -> cycle_import_summary;

	/// \brief The key a sender's project name maps from (BPR-1162): derived
	/// from the name alone, never from a label, path or folder.
	[[nodiscard]] auto imported_project_key(std::string_view aForeignProject) -> workspace_project_id;

	/// \brief One sender revision named by the round, mapped to a revision of
	/// the local cycle.
	struct cycle_revision_mapping
	{
		std::string              Foreign;
		cycle_source_revision_id Local;
		auto                     operator<=>(const cycle_revision_mapping&) const = default;
	};
	/// \brief An imported round's optional association with one local cycle.
	/// The imported record itself never changes.
	struct cycle_import_association
	{
		local_cycle_subject                 Cycle;
		std::vector<cycle_revision_mapping> Revisions;
		auto                                operator<=>(const cycle_import_association&) const = default;
	};
	/// \brief The association's stored form, and back; nothing for a
	/// malformed record.
	[[nodiscard]] auto encode_import_association(const cycle_import_association& aAssociation) -> std::string;
	[[nodiscard]] auto decode_import_association(std::string_view aBytes, const local_cycle_authority& aAuthority) -> std::optional<cycle_import_association>;

	enum class cycle_import_problem : std::uint8_t {
		/// No explicit mapping takes the sender's project to this cycle's
		/// project. A matching label, path or folder name does not count.
		unmapped_project,
		/// The target is not an active cycle of this catalog.
		invalid_cycle,
		/// A revision the round names has no mapping.
		unmapped_revision,
		/// A mapping names a revision twice, a revision the round does not
		/// name, or a local revision this cycle never captured.
		invalid_mapping,
		/// A mapped local revision does not hold exactly the bytes the round
		/// names at a path.
		mismatched_source,
		/// The round is already associated differently.
		already_associated,
		/// Forking needs an association with this cycle.
		not_associated,
	};
	[[nodiscard]] auto stable_name(cycle_import_problem aValue) noexcept -> std::string_view;
	struct cycle_import_refusal
	{
		cycle_import_problem Problem = cycle_import_problem::invalid_mapping;
		/// The foreign revision or path at fault.
		std::string Detail;
		auto        operator<=>(const cycle_import_refusal&) const = default;
	};

	/// \brief A file the round names by its bytes: at this sender revision
	/// and path, this blob.
	struct cycle_foreign_file
	{
		std::string           Revision;
		std::string           Path;
		cycle_material_digest Blob;
		auto                  operator<=>(const cycle_foreign_file&) const = default;
	};
	/// \brief Every sender revision the round names, sorted and unique.
	[[nodiscard]] auto foreign_revisions(const cycle_round_payload& aPayload) -> std::vector<std::string>;
	/// \brief Every file the round names by blob, sorted and unique.
	[[nodiscard]] auto foreign_files(const cycle_round_payload& aPayload) -> std::vector<cycle_foreign_file>;
	/// \brief What a mapped local revision holds at one path: its blob, or
	/// nothing when it holds no readable file there.
	struct cycle_mapped_file
	{
		cycle_source_revision_id             Local;
		std::string                          Path;
		std::optional<cycle_material_digest> Blob;
	};
	/// \brief Decides an association of an imported round with a local
	/// cycle from explicit mappings only.
	/// \details The sender's project must map to the cycle's project through
	/// `aProjects`; every revision the round names must map to a captured
	/// revision of the cycle; and every file the round names by blob must be
	/// exactly that blob in its mapped revision (`aObserved`). Repeating the
	/// existing association is accepted; changing it is refused.
	[[nodiscard]] auto associate_import(const cycle_import_summary& aImport, const cycle_round_payload& aPayload, const review_cycle_state& aCycle,
										std::span<const workspace_import_mapping> aProjects, std::vector<cycle_revision_mapping> aRevisions, std::span<const cycle_mapped_file> aObserved,
										const std::optional<cycle_import_association>& aExisting) -> std::expected<cycle_import_association, cycle_import_refusal>;

	/// \brief The local notes a fork of an imported round starts, in round
	/// order: each item's exact body as a note of `unknown` provenance at its
	/// mapped locations, keyed by the sender's item name. `aFiles` holds the
	/// mapped revisions' bytes for range locations. Suggestions stay in the
	/// quarantined round; the fork adds none.
	[[nodiscard]] auto plan_import_fork(const cycle_round_payload& aPayload, const cycle_import_association& aAssociation, std::span<const cycle_round_file> aFiles)
		-> std::expected<std::vector<std::pair<std::string, begin_cycle_feedback>>, cycle_import_refusal>;
}

#endif
