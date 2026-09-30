#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_IMPORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_IMPORT_HPP

#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_bundle.hpp"

#include <optional>
#include <span>
#include <stop_token>

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
}

#endif
