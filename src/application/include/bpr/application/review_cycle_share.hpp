#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_SHARE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_SHARE_HPP

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_export.hpp"
#include "bpr/application/review_cycle_round.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/core/review_cycle_share.hpp"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Compose feedback and Share feedback for a local Review Cycle
/// (RFC-BPR-0056, RFC-BPR-0061, BPR-1161), on the effect lane.
/// \details Compose opens the cycle's latest round draft, or starts the first
/// one with every open note, and shows the exact payload the draft's revision
/// would freeze as. Sharing decides the route for that round, freezes it and
/// renders what leaves in one step; a refused route, a selection that cannot
/// be frozen or output over the route's budget freezes nothing. Saving writes
/// only the file the reviewer chose. What happened is recorded as a delivery
/// observation and nothing more: no route waits for, or claims, a receipt.
namespace bpr::application {
	/// The largest readable document Compose feedback shows in full; a larger
	/// one is measured and saved, not shown in part.
	inline constexpr std::uint64_t maximum_shown_round_document_bytes = 4U * 1024U * 1024U;

	/// \brief The draft Compose feedback shows.
	struct cycle_share_draft
	{
		cycle_round_preview Preview;
		/// Every note of the cycle as it stands, in item order.
		std::vector<core::cycle_next_round_item> Notes;
		/// The readable document of the preview, exactly as Copy and Save send
		/// it, when within `maximum_shown_round_document_bytes`.
		std::optional<std::string> Document;
		/// Its full size, whether shown or not.
		std::uint64_t DocumentBytes = 0;
		/// The profile's stored disclosure policy, which every route's
		/// decision combines with the host's ([[BPR-1169]]).
		core::cycle_disclosure_policy Policy;
	};
	/// \brief Opens the cycle's latest draft, brought to its notes' current
	/// wording; or, with none, starts one: from the latest frozen round as
	/// Next round does, or with every open note against the latest capture.
	[[nodiscard]] auto open_share_draft(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aBegin,
										const cycle_operation_id& aSave, std::stop_token aStop) -> cycle_store_result<cycle_share_draft>;
	/// \brief Saves one edit as the draft's next revision and reads it back.
	/// An edit that changes nothing writes nothing.
	[[nodiscard]] auto edit_share_draft(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aSave,
										const core::cycle_round_draft_ref& aDraft, std::uint64_t aExpectedRevision, const core::cycle_compose_edit& aEdit, std::stop_token aStop)
		-> cycle_store_result<cycle_share_draft>;

	struct cycle_share_request
	{
		cycle_operation_id          Freeze;
		core::cycle_round_draft_ref Draft;
		/// The draft revision the reviewer saw previewed.
		std::uint64_t           ExpectedDraftRevision = 0;
		core::cycle_share_route Route                 = core::cycle_share_route::copy_document;
		/// Imposed by the host, combined with the profile's stored policy.
		core::cycle_disclosure_policy Policy;
	};
	/// \brief A share before it leaves: the decision, and either why nothing
	/// was frozen or the frozen round and the exact bytes for the route.
	struct cycle_share_output
	{
		core::cycle_authorization                     Decision = core::cycle_authorization::allowed;
		std::optional<core::cycle_round_refusal>      Refusal;
		std::optional<core::cycle_round_output_limit> Limit;
		std::optional<cycle_frozen_round>             Round;
		/// The readable document or the bundle; empty for Use in external agent.
		std::string                 Bytes;
		core::cycle_round_file_kind Format = core::cycle_round_file_kind::markdown;
	};
	/// \brief Decides the route for the round the draft's revision freezes as,
	/// freezes it and renders what leaves. Repeating it for the same revision
	/// reuses the round.
	[[nodiscard]] auto prepare_share(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_share_request& aRequest,
									 std::stop_token aStop) -> cycle_store_result<cycle_share_output>;

	/// \brief A save to the file the reviewer chose, and its observation.
	struct cycle_file_share
	{
		cycle_share_output                  Output;
		std::optional<cycle_round_save>     Save;
		std::optional<cycle_export_failure> Failure;
		/// The `saved` observation, when the file now holds the bytes.
		std::optional<cycle_write_receipt> Recorded;
	};
	/// \brief Shares to a file: `prepare_share`, then the save to
	/// `aDestination`, whose name (never its folder) is `aName`, then the
	/// observation. `aConfirmed` is the digest of other bytes there the
	/// reviewer agreed to replace.
	[[nodiscard]] auto share_to_file(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_share_request& aRequest,
									 cycle_export_destination& aDestination, const std::string& aName, const std::optional<core::cycle_material_digest>& aConfirmed,
									 const cycle_operation_id& aRecord, std::stop_token aStop) -> cycle_store_result<cycle_file_share>;

	/// \brief One frozen round leaving as it was frozen (BPR-1167): printed by
	/// the terminal or saved to a chosen file. Nothing is frozen or amended.
	struct cycle_round_export_request
	{
		core::cycle_round_id           Round;
		core::cycle_disclosure_channel Channel = core::cycle_disclosure_channel::local_file;
		core::cycle_round_file_kind    Format  = core::cycle_round_file_kind::markdown;
		/// Imposed by the host, combined with the profile's stored policy.
		core::cycle_disclosure_policy Policy;
	};
	/// \brief Reads the frozen round, decides its channel as every exporter
	/// does and renders it within the saved document's or bundle's budget. A
	/// refused channel or an output over budget renders nothing.
	[[nodiscard]] auto export_frozen_round(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_export_request& aRequest, std::stop_token aStop)
		-> cycle_store_result<cycle_share_output>;
	/// \brief `export_frozen_round` to `aDestination`, whose name (never its
	/// folder) is `aName`, then the `saved` observation. `aConfirmed` is the
	/// digest of other bytes there the reviewer agreed to replace.
	[[nodiscard]] auto export_frozen_round_to_file(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, const cycle_round_export_request& aRequest,
												   cycle_export_destination& aDestination, const std::string& aName, const std::optional<core::cycle_material_digest>& aConfirmed,
												   const cycle_operation_id& aRecord, std::stop_token aStop) -> cycle_store_result<cycle_file_share>;

	/// \brief The observation of these bytes leaving through `aChannel`.
	[[nodiscard]] auto delivery_observation(const cycle_frozen_round& aRound, core::cycle_delivery_state aState, core::cycle_disclosure_channel aChannel, std::string_view aBytes,
											core::cycle_round_file_kind aFormat, std::string aDestination = { }) -> core::cycle_delivery_observation;
	/// \brief Every delivery observation of the cycle, in order.
	[[nodiscard]] auto all_deliveries(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_delivery_record>>;

	/// \brief The platform clipboard, as the composition owns it. Called on
	/// the decision lane, where the platform requires it.
	class review_cycle_clipboard
	{
	public:
		enum class outcome : std::uint8_t {
			/// The platform reports holding exactly this text.
			confirmed,
			/// It was handed over; the platform did not confirm holding it.
			unconfirmed,
			/// There is no clipboard to write to.
			unavailable,
		};
		virtual ~review_cycle_clipboard() = default;
		/// \brief Puts plain text on the clipboard and reports what the platform says.
		virtual auto write(std::string_view aText) -> outcome = 0;
	};
}

#endif
