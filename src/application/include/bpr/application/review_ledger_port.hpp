#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Names why the durable ledger refused one operation.
	///
	/// \details These are storage outcomes, not review outcomes. A receipt the
	/// core refused never reaches the store, so nothing here overlaps
	/// `core::file_receipt_rejection`.
	enum class ledger_store_failure : std::uint8_t {
		unavailable,
		conflict,
		integrity_failure,
		quota_exceeded,
		cancelled,
		invalid_record,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(ledger_store_failure aFailure) noexcept
		-> std::string_view;

	/// \brief Carries one accepted receipt to the durable ledger.
	///
	/// \details This is a *request*, not the write. The host holds it only long
	/// enough to hand it to the store, and publishes nothing until the store
	/// answers.
	struct receipt_persistence_request
	{
		core::file_receipt Receipt;

		auto operator<=>(const receipt_persistence_request&) const = default;
	};

	/// \brief Bounds one review's retained private records.
	///
	/// \details The store may contain many reviews, but one observation is never
	/// allowed to turn a single review into an unbounded allocation. The values
	/// are deliberately larger than the agent supplement limits: persistence is
	/// the authority, while a later invocation selects a smaller explicit subset.
	inline constexpr std::size_t maximum_private_notes           = 4'096U;
	inline constexpr std::size_t maximum_private_defects         = 4'096U;
	inline constexpr std::size_t maximum_retained_agent_sessions = 128U;

	/// [[BPR-1210]]. The longest path a file note keeps, in UTF-8 bytes.
	inline constexpr std::size_t maximum_note_path_bytes = 4'096U;

	using local_note_id       = core::strong_id<struct local_note_id_tag>;
	using local_note_revision = core::strong_id<struct local_note_revision_tag>;

	/// \brief Binds a private note to exact compared material.
	///
	/// \details [[BPR-1591]]. A note written on a selected line also keeps
	/// that line (RFC-BPR-0085 D3); `core::place_note` decides where it shows.
	/// A note written before ledger schema 10, or on the whole file, has none.
	struct local_note_attachment
	{
		core::file_identity                   File;
		core::file_comparison_digest          Comparison;
		std::optional<core::note_line_anchor> Line = std::nullopt;

		auto operator<=>(const local_note_attachment&) const = default;
	};

	/// \brief One durable private note.
	///
	/// \details A note may describe the review as a whole or one exact file
	/// comparison. It stores no source body or presentation coordinate. A file
	/// note keeps the file's path as it was when the note was written
	/// ([[BPR-1210]], the owner's amendment of RFC-BPR-0007 on 2026-10-06), so
	/// it can still name a file that a later update dropped; a note written
	/// before then has none. The text uses the same bound as an explicitly selected agent
	/// note so persistence cannot create a value that downstream context must
	/// silently truncate. Revision zero denotes an unpersisted candidate.
	/// Only the separate expected revision grants edit or delete authority; the
	/// store assigns each accepted record its positive durable revision.
	struct local_note_record
	{
		local_note_id                        Id;
		std::optional<local_note_attachment> Attachment;
		core::supplement_note                Note;
		local_note_revision                  Revision;
		/// The attached file's path when the note was written; only with an
		/// attachment, at most `maximum_note_path_bytes`.
		std::optional<std::string> FilePath;

		auto operator<=>(const local_note_record&) const = default;
	};

	/// \brief [[BPR-1591]]. What `core::place_note` decides a note's place from.
	[[nodiscard]] inline auto note_anchor_of(const local_note_record& aNote) -> core::note_anchor
	{
		auto anchor = core::note_anchor{ };
		if (aNote.Attachment) {
			anchor.File       = aNote.Attachment->File;
			anchor.Comparison = aNote.Attachment->Comparison;
			anchor.Line       = aNote.Attachment->Line;
		}
		return anchor;
	}

	/// \brief A refused note mutation and its current durable competitor.
	struct local_note_failure
	{
		ledger_store_failure             Reason;
		std::optional<local_note_record> Current;
		auto                             operator<=>(const local_note_failure&) const = default;
	};

	/// \brief One window's rejected mutation, separate from accepted records.
	struct rejected_local_note_mutation
	{
		local_note_id                      Id;
		std::optional<local_note_record>   Candidate;
		std::optional<local_note_revision> ExpectedRevision;
		local_note_failure                 Failure;
		auto                               operator<=>(const rejected_local_note_mutation&) const = default;
	};

	/// \brief A local defect bound to its ordinary shared review thread.
	///
	/// \details The thread identity is linkage, not duplicated GitHub state. The
	/// file identity and both semantic anchors make deleted and outdated context
	/// durable without ever inventing a nearest-line substitute.
	struct durable_defect_record
	{
		core::file_identity          File;
		core::review_conversation_id Conversation;
		core::defect_history         History;

		auto operator<=>(const durable_defect_record&) const = default;
	};

	/// \brief One bounded, deterministic observation of private ledger state.
	struct private_ledger_observation
	{
		std::vector<local_note_record>      Notes;
		std::optional<core::git_commit_oid> LastReviewedHead;
		std::vector<durable_defect_record>  Defects;

		auto operator<=>(const private_ledger_observation&) const = default;
	};

	/// \brief The immutable metadata required to admit one agent session.
	struct agent_session_admission
	{
		core::agent_profile_id       Profile;
		core::agent_revision_binding Binding;

		auto operator<=>(const agent_session_admission&) const = default;
	};

	/// \brief The latest retained session and the next durable identity.
	///
	/// \details `Next` is absent only when the monotonic per-review sequence is
	/// exhausted. Loading this value never creates or advances that sequence.
	struct agent_session_catalog_state
	{
		std::optional<core::agent_session>    Latest;
		std::optional<core::agent_session_id> Next;

		auto operator<=>(const agent_session_catalog_state&) const = default;
	};

	/// \brief Bounds one review's retained conversation receipts.
	inline constexpr std::size_t maximum_conversation_receipts = 4'096U;

	/// \brief Reports whether one conversation receipt can enter the store.
	///
	/// \details [[BPR-0656]]. The conversation must be named; the cursors
	/// may be absent (zero) for a conversation nothing has been seen in.
	[[nodiscard]] auto valid(const core::conversation_receipt& aReceipt) noexcept
		-> bool;

	/// \brief Reports whether one note can enter the durable authority.
	[[nodiscard]] auto valid(const local_note_record& aNote) noexcept -> bool;

	/// \brief Reports whether one defect can enter the durable authority.
	[[nodiscard]] auto valid(const durable_defect_record& aDefect) noexcept -> bool;

	/// \brief Reports whether one metadata-only session admission is valid.
	[[nodiscard]] auto valid(const agent_session_admission& aAdmission) noexcept
		-> bool;

	/// \brief Reports whether one durable metadata-only session is valid.
	[[nodiscard]] auto valid(const core::agent_session& aSession) noexcept -> bool;

	/// \brief Narrow durable agent-session catalog used by the invocation host.
	///
	/// \details [[BPR-1211]]. The catalog is durable storage, so neither call
	/// answers inside itself: each answer arrives later, exactly once, on the
	/// caller's decision lane.
	class agent_session_catalog_port
	{
	public:
		virtual ~agent_session_catalog_port() = default;

		/// \brief Allocates and commits one never-reused session identity.
		virtual void admit(const agent_session_admission&                                                aAdmission,
						   std::function<void(std::expected<core::agent_session, ledger_store_failure>)> aDone)
			= 0;

		/// \brief Loads the latest retained session and next identity without writing.
		virtual void session_catalog(
			std::function<void(std::expected<agent_session_catalog_state, ledger_store_failure>)> aDone)
			= 0;
	};

	/// \brief The durable ledger, as the host sees it.
	///
	/// \details The interface is deliberately narrow. The host does not open,
	/// migrate, or close the database; it stores and loads receipts, and every
	/// answer is typed.
	class review_ledger_store_port
	{
	public:
		review_ledger_store_port()                                                   = default;
		review_ledger_store_port(const review_ledger_store_port&)                    = delete;
		auto operator=(const review_ledger_store_port&) -> review_ledger_store_port& = delete;
		review_ledger_store_port(review_ledger_store_port&&)                         = delete;
		auto operator=(review_ledger_store_port&&) -> review_ledger_store_port&      = delete;
		virtual ~review_ledger_store_port()                                          = default;

		/// \brief Commits one receipt, answering with a typed completion.
		[[nodiscard]] virtual auto commit(
			const receipt_persistence_request& aRequest)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Reads one receipt back from durable storage.
		[[nodiscard]] virtual auto load(const core::file_identity& aFile)
			-> std::expected<std::optional<core::file_receipt>, ledger_store_failure>
			= 0;

		/// \brief Commits one conversation receipt, replacing the one it names.
		///
		/// \details [[BPR-0656]]. The reader's cursor through one
		/// conversation of this review.
		[[nodiscard]] virtual auto store_conversation_receipt(
			const core::conversation_receipt& aReceipt)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Reads every conversation receipt of this review.
		///
		/// \details Bounded by `maximum_conversation_receipts`; the order is
		/// by conversation identity.
		[[nodiscard]] virtual auto load_conversation_receipts()
			-> std::expected<std::vector<core::conversation_receipt>, ledger_store_failure>
			= 0;

		/// \brief Creates a note when no revision is supplied, otherwise compares and replaces.
		[[nodiscard]] virtual auto store_note(const local_note_record&           aNote,
											  std::optional<local_note_revision> aExpectedRevision = std::nullopt)
			-> std::expected<local_note_record, local_note_failure> = 0;

		/// \brief Removes only the exact accepted note revision.
		[[nodiscard]] virtual auto erase_note(local_note_id aNote, local_note_revision aExpectedRevision)
			-> std::expected<void, local_note_failure> = 0;

		/// \brief Replaces the review's last-reviewed head.
		[[nodiscard]] virtual auto store_last_reviewed_head(
			const core::git_commit_oid& aHead)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Removes the last-reviewed head. Missing is an idempotent success.
		[[nodiscard]] virtual auto erase_last_reviewed_head()
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Upserts one durable local defect.
		[[nodiscard]] virtual auto store_defect(
			const durable_defect_record& aDefect)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Removes one defect identity. Missing is an idempotent success.
		[[nodiscard]] virtual auto erase_defect(core::defect_id aDefect)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Loads the bounded private records for this store's exact scope.
		[[nodiscard]] virtual auto load_private()
			-> std::expected<private_ledger_observation, ledger_store_failure> = 0;

		/// \brief Atomically allocates and stores immutable agent-session metadata.
		[[nodiscard]] virtual auto admit_agent_session(
			const agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, ledger_store_failure> = 0;

		/// \brief Loads retained session metadata without creating a sequence row.
		[[nodiscard]] virtual auto load_agent_session_catalog()
			-> std::expected<agent_session_catalog_state, ledger_store_failure> = 0;
	};

	/// \brief Reports one accepted receipt to whatever presents it.
	struct receipt_projection
	{
		core::file_receipt Receipt;

		auto operator<=>(const receipt_projection&) const = default;
	};

	/// \brief Reports one receipt that was not accepted, and why.
	///
	/// \details Exactly one of the two reasons applies: the core refused the
	/// command, or the store refused the write. Carrying both in one record
	/// keeps a caller from having to guess which failure vocabulary to consult.
	struct receipt_failure_publication
	{
		core::file_receipt                  Retained;
		core::file_receipt_rejection        Rejection = core::file_receipt_rejection::none;
		std::optional<ledger_store_failure> Store;

		auto operator<=>(const receipt_failure_publication&) const = default;
	};

	/// \brief Receives accepted receipts and refusals.
	class receipt_projection_port
	{
	public:
		receipt_projection_port()                                                  = default;
		receipt_projection_port(const receipt_projection_port&)                    = delete;
		auto operator=(const receipt_projection_port&) -> receipt_projection_port& = delete;
		receipt_projection_port(receipt_projection_port&&)                         = delete;
		auto operator=(receipt_projection_port&&) -> receipt_projection_port&      = delete;
		virtual ~receipt_projection_port()                                         = default;

		virtual void publish(const receipt_projection& aProjection)    = 0;
		virtual void fail(const receipt_failure_publication& aFailure) = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
