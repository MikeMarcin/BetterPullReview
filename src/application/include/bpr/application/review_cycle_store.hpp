#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_STORE_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/review_cycle_tabs.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

/// \file
/// \brief Durable local Review Cycles catalog (RFC-BPR-0053).
/// \details One profile-private store holds cycles, captured revisions,
/// private feedback threads, round drafts, frozen rounds and the immutable
/// objects they reference. It is a
/// separate database: no pull-request table or record changes.
namespace bpr::application {
	/// \brief Caller-chosen request identity. Retrying with the same identity
	/// and the same request returns the first result without repeating it;
	/// reusing it for a different request is refused.
	using cycle_operation_id = core::fixed_identity_256<struct cycle_operation_id_tag>;

	/// \brief One immutable stored object, addressed by the SHA-256 of its bytes.
	struct cycle_object_ref
	{
		core::cycle_material_digest Digest;
		std::uint64_t               Size                                       = 0;
		auto                        operator<=>(const cycle_object_ref&) const = default;
	};

	inline constexpr std::uint64_t maximum_cycle_object_bytes = 128U * 1024U * 1024U;
	inline constexpr std::size_t   maximum_cycle_draft_bytes  = 512U * 1024U;
	inline constexpr std::size_t   maximum_cycle_page_entries = 64U;
	/// A feedback revision page stops at this many body bytes, after at least one revision.
	inline constexpr std::size_t maximum_cycle_feedback_page_bytes = 512U * 1024U;
	/// The RFC's 2 GiB soft quota exceeds the shared connection policy's 1 GiB
	/// page ceiling, so the default is the stricter admitted limit.
	inline constexpr std::uint64_t default_cycle_retained_budget_bytes = 768U * 1024U * 1024U;

	enum class cycle_store_failure : std::uint8_t {
		unavailable,
		corrupt,
		schema_mismatch,
		wrong_profile,
		not_found,
		/// Expected revision is stale; `Current` holds the stored state.
		conflict,
		/// The core refused the change; see `Refusal`.
		refused,
		/// The operation identity was used for a different request.
		operation_reused,
		bounds,
		/// A referenced object is missing or unreadable; nothing was committed.
		object_unavailable,
		/// The retained budget is full; see `Inventory`. Pinned material is
		/// never evicted to make room.
		quota,
		/// The removal preview no longer matches the cycle.
		preview_stale,
		cancelled,
	};

	/// \brief What retained material occupies.
	struct cycle_retention_inventory
	{
		std::uint64_t Budget                                              = 0;
		std::uint64_t PinnedBytes                                         = 0;
		std::uint64_t PinnedObjects                                       = 0;
		std::uint64_t UnreferencedBytes                                   = 0;
		std::uint64_t UnreferencedObjects                                 = 0;
		auto          operator<=>(const cycle_retention_inventory&) const = default;
	};

	struct cycle_store_error
	{
		cycle_store_failure                      Failure = cycle_store_failure::unavailable;
		core::cycle_refusal                      Refusal = core::cycle_refusal::none;
		std::optional<core::review_cycle_state>  Current;
		std::optional<cycle_retention_inventory> Inventory;
	};
	template <typename T>
	using cycle_store_result = std::expected<T, cycle_store_error>;

	/// \brief The durable result of one write, returned again on retry.
	struct cycle_write_receipt
	{
		core::review_cycle_id Cycle;
		/// Cycle revision after the write; draft revision for `save_draft`.
		std::uint64_t Revision = 0;
		/// Identity the write allocated (cycle, source revision, draft or
		/// round), or zero.
		std::uint64_t Allocated = 0;
		bool          Changed   = false;
		/// True when this call returned an earlier result.
		bool Replayed                                      = false;
		auto operator<=>(const cycle_write_receipt&) const = default;
	};

	struct cycle_capture_commit
	{
		core::local_cycle_subject  Subject;
		std::uint64_t              ExpectedRevision = 0;
		core::record_cycle_capture Capture;
		/// The installed manifest; its digest must be the capture's material.
		cycle_object_ref Manifest;
		/// Every installed content object the manifest references. Each must
		/// be present; all are pinned with the revision in one transaction.
		std::vector<cycle_object_ref> Content;
	};
	struct cycle_freeze_commit
	{
		core::local_cycle_subject   Subject;
		core::cycle_round_draft_ref Draft;
		std::uint64_t               ExpectedDraftRevision = 0;
		/// The installed canonical payload; its digest is the round digest.
		cycle_object_ref Payload;
		/// The round the payload names. When present, a freeze that would
		/// allocate or reuse another round is a conflict and commits nothing.
		std::optional<core::cycle_round_id> Round;
	};

	struct cycle_page
	{
		std::vector<core::review_cycle_projection> Cycles;
		std::optional<core::review_cycle_id>       Next;
	};
	struct stored_cycle_revision
	{
		core::cycle_source_revision Revision;
		cycle_object_ref            Manifest;
		auto                        operator<=>(const stored_cycle_revision&) const = default;
	};
	struct cycle_revision_page
	{
		std::vector<stored_cycle_revision>            Revisions;
		std::optional<core::cycle_source_revision_id> Next;
	};
	struct stored_cycle_draft
	{
		core::cycle_round_draft   Draft;
		std::vector<std::uint8_t> Body;
	};
	struct stored_cycle_round
	{
		core::cycle_round Round;
		cycle_object_ref  Payload;
		auto              operator<=>(const stored_cycle_round&) const = default;
	};
	struct cycle_round_page
	{
		std::vector<stored_cycle_round>     Rounds;
		std::optional<core::cycle_round_id> Next;
	};
	/// \brief Feedback threads in item order, archived ones included.
	struct cycle_feedback_page
	{
		std::vector<core::cycle_feedback_thread>    Threads;
		std::optional<core::cycle_feedback_item_id> Next;
	};
	/// \brief One thread's revisions in order.
	struct cycle_feedback_revision_page
	{
		std::vector<core::cycle_feedback_revision>           Revisions;
		std::optional<core::private_review_turn_revision_id> Next;
	};
	/// \brief One suggestion's revisions in order.
	struct cycle_suggestion_history_page
	{
		std::vector<core::cycle_suggestion> Revisions;
		std::optional<std::uint64_t>        Next;
	};

	/// \brief What removing a cycle would irrecoverably delete. Local cycles
	/// are not recoverable from GitHub or from exported rounds.
	struct cycle_removal_preview
	{
		core::local_cycle_subject Subject;
		std::uint64_t             Revision  = 0;
		std::uint64_t             Revisions = 0;
		std::uint64_t             Drafts    = 0;
		std::uint64_t             Rounds    = 0;
		std::uint64_t             Feedback  = 0;
		/// Object bytes only this cycle references.
		std::uint64_t ReleasedBytes = 0;
		/// Binds `remove` to exactly what was previewed.
		core::cycle_material_digest Token;
		auto                        operator<=>(const cycle_removal_preview&) const = default;
	};

	/// \brief Profile-private Review Cycles storage.
	/// \details All calls run on the profile owner's serialized effect lane.
	/// Objects are installed and verified in their own transaction before any
	/// catalog row references them, so an interrupted write leaves either a
	/// complete record or none, and at most an unreferenced object that
	/// `reclaim` removes.
	class review_cycle_store
	{
	public:
		virtual ~review_cycle_store() = default;

		/// \brief This profile's catalog; created with a fresh incarnation on first use.
		virtual auto catalog(std::stop_token aStop) -> cycle_store_result<core::review_cycle_catalog>                                                                          = 0;
		virtual auto create(const cycle_operation_id& aOperation, const core::create_review_cycle& aCommand, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		virtual auto load(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::optional<core::review_cycle_state>>                     = 0;
		virtual auto list(std::optional<core::review_cycle_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_page>                                                = 0;
		/// \brief Rename, archive, unarchive and scope changes. Captures use `record_capture`.
		virtual auto apply(const cycle_operation_id& aOperation, const core::review_cycle_command& aCommand, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;

		/// \brief Durably installs and verifies one object; idempotent by content.
		virtual auto install(std::span<const std::uint8_t> aBytes, std::stop_token aStop) -> cycle_store_result<cycle_object_ref> = 0;
		/// \brief Installs and verifies several objects through one connection
		/// in one transaction: all of them or none. Each is idempotent by
		/// content; the budget applies to the batch as a whole.
		virtual auto install_all(std::span<const std::span<const std::uint8_t>> aObjects, std::stop_token aStop) -> cycle_store_result<std::vector<cycle_object_ref>> = 0;
		/// \brief Reads and verifies one object. Missing or corrupt bytes are
		/// reported, never substituted.
		virtual auto read_object(const cycle_object_ref& aObject, std::stop_token aStop) -> cycle_store_result<std::vector<std::uint8_t>> = 0;

		virtual auto record_capture(const cycle_operation_id& aOperation, const cycle_capture_commit& aCommit, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief True when the revision pinned the object, as its manifest or
		/// as content its manifest references.
		virtual auto revision_holds(const core::local_cycle_subject& aSubject, core::cycle_source_revision_id aRevision, const cycle_object_ref& aObject, std::stop_token aStop)
			-> cycle_store_result<bool>                                                                                                                                                           = 0;
		virtual auto revisions(const core::local_cycle_subject& aSubject, std::optional<core::cycle_source_revision_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_revision_page> = 0;

		virtual auto begin_draft(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, std::uint64_t aExpectedRevision, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt>                                                                                     = 0;
		virtual auto save_draft(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::uint64_t aExpectedDraftRevision, std::span<const std::uint8_t> aBody, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		virtual auto load_draft(const core::local_cycle_subject& aSubject, const core::cycle_round_draft_ref& aDraft, std::stop_token aStop) -> cycle_store_result<std::optional<stored_cycle_draft>>                                                                                                    = 0;
		/// \brief Freezes a draft revision into an immutable round in one
		/// transaction; the same draft revision and payload returns the
		/// existing round.
		virtual auto freeze(const cycle_operation_id& aOperation, const cycle_freeze_commit& aCommit, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt>           = 0;
		virtual auto rounds(const core::local_cycle_subject& aSubject, std::optional<core::cycle_round_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_round_page> = 0;

		/// \brief Starts a private feedback item (RFC-BPR-0055).
		/// \details Every file or anchor target must name content its revision
		/// pinned; the bytes themselves are verified against retained material
		/// before this is called (`review_cycle_feedback_host`). `Allocated`
		/// is the item; `Revision` the cycle revision after the write.
		virtual auto begin_feedback(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, std::uint64_t aExpectedRevision, const core::begin_cycle_feedback& aCommand, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief Appends, edits, adds a reference or changes lifecycle. The
		/// store supplies the revision a reply or edit names. `Revision` is the
		/// thread version after the write; `Allocated` the new revision, or zero.
		virtual auto change_feedback(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::uint64_t aExpectedVersion,
									 const core::cycle_feedback_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt>                                                                 = 0;
		virtual auto load_feedback(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::stop_token aStop) -> cycle_store_result<std::optional<core::cycle_feedback_thread>> = 0;
		virtual auto feedback(const core::local_cycle_subject& aSubject, std::optional<core::cycle_feedback_item_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_feedback_page>                     = 0;
		virtual auto feedback_revisions(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::optional<core::private_review_turn_revision_id> aAfter, std::stop_token aStop)
			-> cycle_store_result<cycle_feedback_revision_page> = 0;

		/// \brief Adds an exact suggestion to a note (BPR-1155). The selection
		/// must name content its revision pinned, and is verified against the
		/// retained bytes before this is called. `Allocated` is the suggestion;
		/// `Revision` the thread version after the write. Nothing is written
		/// anywhere but this store.
		virtual auto add_suggestion(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::uint64_t aExpectedVersion,
									const core::suggest_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief Records a suggestion's successor revision. `Revision` is the
		/// suggestion revision after the write.
		virtual auto change_suggestion(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_suggestion_ref& aSuggestion, std::uint64_t aExpectedRevision,
									   const core::cycle_suggestion_change& aChange, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief The latest revision of each of a note's suggestions, in order.
		virtual auto suggestions(const core::local_cycle_subject& aSubject, const core::cycle_feedback_item_ref& aItem, std::stop_token aStop) -> cycle_store_result<std::vector<core::cycle_suggestion>> = 0;
		virtual auto suggestion_history(const core::local_cycle_subject& aSubject, const core::cycle_suggestion_ref& aSuggestion, std::optional<std::uint64_t> aAfter, std::stop_token aStop)
			-> cycle_store_result<cycle_suggestion_history_page> = 0;

		/// \brief Keeps the cycle's document tabs: exact views and the active
		/// one (BPR-1157). The last write wins; it is presentation state, not
		/// review history, and needs no operation identity.
		virtual auto save_tabs(const core::local_cycle_subject& aSubject, const core::cycle_tabs_record& aRecord, std::stop_token aStop) -> cycle_store_result<void> = 0;
		virtual auto load_tabs(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::optional<core::cycle_tabs_record>>       = 0;

		virtual auto inventory(std::stop_token aStop) -> cycle_store_result<cycle_retention_inventory> = 0;
		/// \brief Removes unreferenced objects only.
		virtual auto reclaim(std::stop_token aStop) -> cycle_store_result<cycle_retention_inventory>                                                = 0;
		virtual auto removal_preview(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<cycle_removal_preview> = 0;
		/// \brief Deletes the previewed cycle's records. Its identity is never reissued.
		virtual auto remove(const cycle_operation_id& aOperation, const cycle_removal_preview& aPreview, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
	};
}

#endif
