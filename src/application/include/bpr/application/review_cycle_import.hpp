#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_IMPORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_IMPORT_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_bundle.hpp"
#include "bpr/core/review_cycle_import.hpp"

#include <optional>
#include <span>
#include <stop_token>
#include <vector>

/// \file
/// \brief Opening a portable review bundle into the profile's private
/// quarantine (RFC-BPR-0056, BPR-1160).
/// \details The whole bundle is validated in memory first; only a bundle
/// that passes every check has its canonical payload and attachments
/// installed as content-addressed objects and its record admitted, in the
/// store's own transaction. Nothing is written anywhere else: no member is
/// extracted to a named file, no source is changed and nothing is run. An
/// interrupted import leaves at most unreferenced objects; importing the same
/// round again finds its record. Runs on the effect lane.
namespace bpr::application {
	/// \brief An import's outcome: the quarantined record, or the reason the
	/// bundle was refused before anything was admitted.
	struct cycle_import_result
	{
		std::optional<stored_cycle_import>      Import;
		std::optional<core::cycle_bundle_error> Refusal;
		/// The round was already quarantined; nothing new was admitted.
		bool Existing = false;
		/// This call returned an earlier call's result.
		bool Replayed = false;
	};
	[[nodiscard]] auto import_round_bundle(review_cycle_store& aStore, const cycle_operation_id& aOperation, std::span<const std::uint8_t> aBundle, std::stop_token aStop)
		-> cycle_store_result<cycle_import_result>;

	/// \brief A quarantined round with its verified content.
	struct cycle_imported_round
	{
		stored_cycle_import       Record;
		core::cycle_decoded_round Content;
	};
	/// \brief Reads a quarantined round and verifies its payload against the
	/// recorded digest; a payload that differs is reported corrupt.
	[[nodiscard]] auto read_imported_round(review_cycle_store& aStore, core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<cycle_imported_round>;

	/// \brief The reviewer's explicit mappings for attaching a quarantined
	/// round to a local cycle (BPR-1162).
	struct cycle_import_attach_request
	{
		cycle_operation_id                          Operation;
		core::cycle_import_id                       Import;
		std::vector<core::workspace_import_mapping> Projects;
		std::vector<core::cycle_revision_mapping>   Revisions;
	};
	struct cycle_import_attach_result
	{
		std::optional<core::cycle_import_association> Association;
		std::optional<core::cycle_import_refusal>     Refusal;
		/// False when the same association was already recorded.
		bool Changed = false;
	};
	/// \brief Associates a quarantined round with `aSubject` only when the
	/// explicit mappings hold: the sender's project maps to the cycle's
	/// project, every sender revision maps to a captured local revision, and
	/// every file the round names by bytes is exactly those bytes in its
	/// mapped revision, as retained. Nothing in the imported round changes.
	[[nodiscard]] auto attach_imported_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject,
											 const cycle_import_attach_request& aRequest, std::stop_token aStop) -> cycle_store_result<cycle_import_attach_result>;

	struct cycle_import_fork_result
	{
		std::vector<stored_import_fork>           Notes;
		std::optional<core::cycle_import_refusal> Refusal;
		/// The round was forked before; nothing new was started.
		bool Existing = false;
	};
	/// \brief Forks an associated round into local authoring: one note of
	/// `unknown` provenance per item, at its mapped locations, recorded
	/// against the sender's item name. Edits from then on are ordinary local
	/// revisions; the quarantined round never changes.
	[[nodiscard]] auto fork_imported_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation,
										   core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<cycle_import_fork_result>;
}

#endif
