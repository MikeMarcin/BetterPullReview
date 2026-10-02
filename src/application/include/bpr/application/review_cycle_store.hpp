#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_STORE_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_feedback.hpp"
#include "bpr/core/review_cycle_import.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/review_cycle_response.hpp"
#include "bpr/core/review_cycle_share.hpp"
#include "bpr/core/review_cycle_suggestion.hpp"
#include "bpr/core/review_cycle_tabs.hpp"
#include "bpr/core/review_cycle_verification.hpp"
#include "bpr/core/review_cycle_views.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
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
		cycle_store_failure Failure = cycle_store_failure::unavailable;
		core::cycle_refusal Refusal = core::cycle_refusal::none;
		/// A publication command the core refused (BPR-1174).
		std::optional<core::publication_refusal> Publication;
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

	/// \brief An imported round to admit to quarantine (BPR-1160). Every
	/// object must already be installed; the payload's digest is the round's.
	struct cycle_import_commit
	{
		core::cycle_import_summary    Summary;
		cycle_object_ref              Payload;
		std::vector<cycle_object_ref> Objects;
	};
	/// \brief One quarantined imported round, and the local cycle it was
	/// explicitly associated with, if any (BPR-1162).
	struct stored_cycle_import
	{
		core::cycle_import_summary                    Summary;
		cycle_object_ref                              Payload;
		std::vector<cycle_object_ref>                 Objects;
		std::optional<core::cycle_import_association> Association;
		auto                                          operator<=>(const stored_cycle_import&) const = default;
	};
	/// \brief A local note a fork started, by the sender's item name.
	struct stored_import_fork
	{
		std::string                   ForeignItem;
		core::cycle_feedback_item_ref Item;
		auto                          operator<=>(const stored_import_fork&) const = default;
	};
	struct cycle_import_page
	{
		std::vector<stored_cycle_import>     Imports;
		std::optional<core::cycle_import_id> Next;
	};

	/// \brief One recipient statement to append (BPR-1164). The content object
	/// (the canonical response or manual reply) must already be installed.
	struct cycle_response_commit
	{
		core::local_cycle_subject    Subject;
		core::cycle_round_id         Round;
		core::cycle_round_digest     RoundDigest;
		core::cycle_response_channel Channel = core::cycle_response_channel::manual_entry;
		/// The response's own identifier; empty for a manual reply.
		std::string      ForeignId;
		cycle_object_ref Content;
	};
	struct cycle_response_page
	{
		std::vector<core::cycle_response_record> Responses;
		std::optional<core::cycle_response_id>   Next;
	};
	/// \brief Delivery observations in the order they were made (BPR-1161).
	struct cycle_delivery_page
	{
		std::vector<core::cycle_delivery_record> Deliveries;
		/// The ordinal to continue after.
		std::optional<std::uint64_t> Next;
	};

	struct cycle_verification_page
	{
		std::vector<core::cycle_verification_record> Verifications;
		std::optional<core::cycle_verification_id>   Next;
	};

	/// \brief One recorded publication: the finite plan and its journal
	/// (BPR-1174). `Revision` advances with every journal change.
	struct stored_cycle_publication
	{
		core::cycle_publication_ref Id;
		std::uint64_t               Revision = 0;
		core::publication_plan      Plan;
		core::publication_journal   Journal;
		auto                        operator<=>(const stored_cycle_publication&) const = default;
	};
	struct cycle_publication_page
	{
		std::vector<stored_cycle_publication>     Publications;
		std::optional<core::cycle_publication_id> Next;
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

		/// \brief Admits an imported round to quarantine in one transaction,
		/// pinning its payload and attachments (BPR-1160). A round already
		/// admitted, by digest, returns its record unchanged. `Allocated` is
		/// the import; `Cycle` and `Revision` are zero: no cycle changes.
		virtual auto admit_import(const cycle_operation_id& aOperation, const cycle_import_commit& aCommit, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		virtual auto imports(std::optional<core::cycle_import_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_import_page>                                     = 0;
		virtual auto load_import(core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<std::optional<stored_cycle_import>>                              = 0;
		/// \brief Records an import's association with one active local cycle
		/// (BPR-1162); every mapped revision must be captured. The same
		/// association again changes nothing; a different one is a conflict.
		/// The imported record and payload never change. `Allocated` is the import.
		virtual auto associate_import(const cycle_operation_id& aOperation, core::cycle_import_id aImport, const core::cycle_import_association& aAssociation, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief Starts the fork's local notes in the associated cycle in one
		/// transaction, each recorded against the sender's item name. A second
		/// fork of the same import changes nothing. `Allocated` is the number
		/// of notes; `Revision` the cycle revision after the write.
		virtual auto fork_import(const cycle_operation_id& aOperation, core::cycle_import_id aImport, const core::local_cycle_subject& aSubject, std::uint64_t aExpectedRevision,
								 const std::vector<std::pair<std::string, core::begin_cycle_feedback>>& aNotes, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		virtual auto import_forks(core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<std::vector<stored_import_fork>>                                    = 0;

		/// \brief Records the reviewer's explicit review checkpoint
		/// (BPR-1163): "reviewed through this captured revision". Marking the
		/// revision the latest checkpoint names changes nothing. `Allocated`
		/// is the checkpoint's ordinal. Checkpoints are append-only.
		virtual auto mark_reviewed(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, core::cycle_source_revision_id aRevision, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt>                                                                                                                 = 0;
		virtual auto last_review(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<std::optional<core::cycle_review_checkpoint>> = 0;

		/// \brief Appends one recipient statement to a frozen round of the cycle
		/// (BPR-1164), pinning its content. A structured response already
		/// appended under the same identifier with the same content changes
		/// nothing and returns its record; other content under that identifier
		/// is a conflict. Nothing else in the cycle changes: no revision, note,
		/// suggestion, decision or round. `Allocated` is the response.
		virtual auto append_response(const cycle_operation_id& aOperation, const cycle_response_commit& aCommit, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief The cycle's statements in append order.
		virtual auto responses(const core::local_cycle_subject& aSubject, std::optional<core::cycle_response_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_response_page> = 0;

		/// \brief Appends one observation of a frozen round of the cycle
		/// leaving by one channel (BPR-1161): the round must be this cycle's,
		/// frozen with the observation's digest, and the core must admit the
		/// observation. Nothing else in the cycle changes; an observation is
		/// never a receipt, a reading or a completion. `Allocated` is its ordinal.
		virtual auto record_delivery(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_delivery_observation& aObservation, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief The cycle's delivery observations in order.
		virtual auto deliveries(const core::local_cycle_subject& aSubject, std::optional<std::uint64_t> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_delivery_page> = 0;

		/// \brief Appends one reviewer decision about a note (BPR-1165): the
		/// wording and captured revision it checked and that material's digest.
		/// The note must exist and the revision be captured; the wording and
		/// material are checked against retained records before this is called.
		/// Decisions are append-only; a new one supersedes, never rewrites.
		/// `Allocated` is the decision; the record's `Id` is ignored.
		virtual auto record_verification(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::cycle_verification_record& aRecord, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief The cycle's decisions in append order.
		virtual auto verifications(const core::local_cycle_subject& aSubject, std::optional<core::cycle_verification_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_verification_page> = 0;

		/// \brief Records a finite publication plan with a new journal
		/// (BPR-1174). Nothing is authorized. The final revision must be
		/// captured. `Allocated` is the publication; `Revision` the journal's (1).
		virtual auto record_publication(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, const core::publication_plan& aPlan, std::stop_token aStop)
			-> cycle_store_result<cycle_write_receipt> = 0;
		/// \brief Applies one journal command through the core in one
		/// transaction, before the effect it records is attempted. A stale
		/// revision is a conflict; a refusal names the publication refusal.
		/// `Revision` is the journal's after the change; `Allocated` the effect
		/// sequence a begun effect was given, or zero.
		virtual auto change_publication(const cycle_operation_id& aOperation, const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication,
										std::uint64_t aExpectedRevision, const core::publication_command& aCommand, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
		virtual auto load_publication(const core::local_cycle_subject& aSubject, core::cycle_publication_id aPublication, std::stop_token aStop)
			-> cycle_store_result<std::optional<stored_cycle_publication>>                                                                                                                          = 0;
		virtual auto publications(const core::local_cycle_subject& aSubject, std::optional<core::cycle_publication_id> aAfter, std::stop_token aStop) -> cycle_store_result<cycle_publication_page> = 0;

		virtual auto inventory(std::stop_token aStop) -> cycle_store_result<cycle_retention_inventory> = 0;
		/// \brief Removes unreferenced objects only.
		virtual auto reclaim(std::stop_token aStop) -> cycle_store_result<cycle_retention_inventory>                                                = 0;
		virtual auto removal_preview(const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<cycle_removal_preview> = 0;
		/// \brief Deletes the previewed cycle's records. Its identity is never reissued.
		virtual auto remove(const cycle_operation_id& aOperation, const cycle_removal_preview& aPreview, std::stop_token aStop) -> cycle_store_result<cycle_write_receipt> = 0;
	};
}

#endif
