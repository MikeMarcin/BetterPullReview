#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RESPONSE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_RESPONSE_HPP

#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_response.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

/// \file
/// \brief Appending what recipients say back to a frozen round
/// (RFC-BPR-0057, BPR-1164).
/// \details A structured response is read as untrusted data, checked against
/// the frozen round it is attached to (cycle, round, canonical digest, exact
/// item revisions), stored as its canonical bytes and appended. A manual
/// reply is the reviewer's own entry of someone's prose. Both are statements
/// only: appending one writes no source, captures no revision, runs nothing
/// its evidence names, records no reviewer decision and changes no note,
/// suggestion or round. Runs on the effect lane.
namespace bpr::application {
	/// \brief The outcome of one append: a receipt, or why nothing was appended.
	struct cycle_response_result
	{
		std::optional<cycle_write_receipt>               Receipt;
		std::optional<core::cycle_response_decode_error> Decode;
		std::optional<core::cycle_response_refusal>      Refusal;
		/// The chosen file could not be read.
		std::optional<cycle_export_failure> Read;
	};

	/// \brief Reads the chosen response file, within `core::maximum_response_document_bytes`.
	using cycle_response_file_reader = std::function<std::expected<std::vector<std::uint8_t>, cycle_export_failure>(std::stop_token)>;

	/// \brief Checks a response document against round `aRound` of the cycle and appends it.
	[[nodiscard]] auto import_cycle_response(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound,
											 std::span<const std::uint8_t> aDocument, const cycle_operation_id& aOperation, std::stop_token aStop)
		-> cycle_store_result<cycle_response_result>;
	/// \brief Appends a reply the reviewer entered for round `aRound`; its
	/// round name is taken from the round.
	[[nodiscard]] auto attach_manual_reply(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, core::cycle_round_id aRound, core::cycle_manual_reply aReply,
										   const cycle_operation_id& aOperation, std::stop_token aStop) -> cycle_store_result<cycle_response_result>;

	using cycle_response_entries = std::vector<std::pair<core::cycle_response_record, core::cycle_response_content>>;
	/// \brief Every statement of the cycle with its verified content, in append order.
	[[nodiscard]] auto read_cycle_responses(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop)
		-> cycle_store_result<cycle_response_entries>;
}

#endif
