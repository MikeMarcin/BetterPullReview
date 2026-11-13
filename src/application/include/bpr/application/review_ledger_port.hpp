#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/generated_files.hpp"
#include "bpr/core/local_comment.hpp"
#include "bpr/core/note_placement.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
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
	/// RFC-BPR-0127: Local comments keep the note bound and do not count
	/// against the 64 active drafts.
	inline constexpr std::size_t maximum_local_comments          = core::maximum_local_comments;
	inline constexpr std::size_t maximum_private_defects         = 4'096U;
	inline constexpr std::size_t maximum_retained_agent_sessions = 128U;

	/// [[BPR-1210]]. The longest path a file comment keeps, in UTF-8 bytes.
	inline constexpr std::size_t maximum_note_path_bytes = 4'096U;

	using local_comment_id       = core::strong_id<struct local_comment_id_tag>;
	using local_comment_revision = core::strong_id<struct local_comment_revision_tag>;

	/// \brief Binds a Local comment to exact compared material.
	///
	/// \details [[BPR-1591]]. A comment written on a selected line also keeps
	/// that line (RFC-BPR-0085 D3); `core::place_note` decides where it shows.
	/// A comment migrated from a note written before ledger schema 10, or one
	/// on the whole file, has none.
	struct local_comment_attachment
	{
		core::file_identity                   File;
		core::file_comparison_digest          Comparison;
		std::optional<core::note_line_anchor> Line = std::nullopt;

		auto operator<=>(const local_comment_attachment&) const = default;
	};

	/// \brief RFC-BPR-0127. One durable Local comment: only on this computer.
	///
	/// \details What was a private note. It may be on the review as a whole,
	/// on one exact file comparison, on a line or a range of it, or a reply
	/// to a thread. A file comment keeps the file's path as it was when it was
	/// written ([[BPR-1210]], the owner's amendment of RFC-BPR-0007 on
	/// 2026-10-06), so it can still name a file a later update dropped; one
	/// migrated from a note written before then has none. The body has a
	/// draft's bound (64 KiB), so Keep local never cuts a draft's text; the
	/// agent supplement applies its own smaller bound to what it is sent.
	/// Revision zero denotes an unpersisted candidate. Only the separate
	/// expected revision grants edit or delete authority; the store assigns
	/// each accepted record its positive durable revision. A migrated note
	/// keeps its note identity as its identity, so what pointed at the note
	/// (a finding card's Go to note) arrives at the comment.
	struct local_comment_record
	{
		local_comment_id                        Id;
		std::optional<local_comment_attachment> Attachment;
		std::string                             Body;
		local_comment_revision                  Revision;
		/// The attached file's path when the comment was written; only with
		/// an attachment, at most `maximum_note_path_bytes`.
		std::optional<std::string> FilePath;
		/// The first line of a range, on the line's side and above it.
		std::optional<std::uint32_t> StartLine = std::nullopt;
		/// The thread a Local reply answers.
		std::optional<std::string> ReplyToThread = std::nullopt;

		auto operator<=>(const local_comment_record&) const = default;
	};

	/// \brief [[BPR-1591]]. What `core::place_note` decides a comment's place from.
	[[nodiscard]] inline auto local_comment_anchor_of(const local_comment_record& aComment) -> core::note_anchor
	{
		auto anchor = core::note_anchor{ };
		if (aComment.Attachment) {
			anchor.File       = aComment.Attachment->File;
			anchor.Comparison = aComment.Attachment->Comparison;
			anchor.Line       = aComment.Attachment->Line;
		}
		return anchor;
	}

	/// \brief RFC-BPR-0127. The core's view of where a comment is.
	[[nodiscard]] inline auto local_comment_place_of(const local_comment_record& aComment) -> core::local_comment_anchor
	{
		return core::local_comment_anchor{ .Place = local_comment_anchor_of(aComment), .StartLine = aComment.StartLine, .ReplyToThread = aComment.ReplyToThread };
	}

	/// \brief A refused note mutation and its current durable competitor.
	struct local_comment_failure
	{
		ledger_store_failure                Reason;
		std::optional<local_comment_record> Current;
		auto                                operator<=>(const local_comment_failure&) const = default;
	};

	/// \brief One window's rejected mutation, separate from accepted records.
	struct rejected_local_comment_mutation
	{
		local_comment_id                      Id;
		std::optional<local_comment_record>   Candidate;
		std::optional<local_comment_revision> ExpectedRevision;
		local_comment_failure                 Failure;
		auto                                  operator<=>(const rejected_local_comment_mutation&) const = default;
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
		/// RFC-BPR-0127. The review's Local comments, by identity.
		std::vector<local_comment_record>   LocalComments;
		std::optional<core::git_commit_oid> LastReviewedHead;
		std::vector<durable_defect_record>  Defects;
		/// RFC-BPR-0089 M2. The reviewer's generated-file marks for this
		/// review: Show as normal, Treat as generated and Review anyway.
		core::generated_file_marks GeneratedMarks;

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
	[[nodiscard]] auto valid(const local_comment_record& aNote) noexcept -> bool;

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

		/// \brief [[BPR-1699]]. Commits these conversation receipts
		/// together, each replacing the one it names, or none of them.
		///
		/// \details One bulk Mark read is one durable change: a failure part
		/// way leaves every receipt as it was.
		[[nodiscard]] virtual auto store_conversation_receipts(
			std::span<const core::conversation_receipt> aReceipts)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Reads every conversation receipt of this review.
		///
		/// \details Bounded by `maximum_conversation_receipts`; the order is
		/// by conversation identity.
		[[nodiscard]] virtual auto load_conversation_receipts()
			-> std::expected<std::vector<core::conversation_receipt>, ledger_store_failure>
			= 0;

		/// \brief Creates a note when no revision is supplied, otherwise compares and replaces.
		[[nodiscard]] virtual auto store_local_comment(const local_comment_record&           aNote,
													   std::optional<local_comment_revision> aExpectedRevision = std::nullopt)
			-> std::expected<local_comment_record, local_comment_failure> = 0;

		/// \brief Removes only the exact accepted note revision.
		[[nodiscard]] virtual auto erase_local_comment(local_comment_id aNote, local_comment_revision aExpectedRevision)
			-> std::expected<void, local_comment_failure> = 0;

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

		/// \brief RFC-BPR-0089 M2. Replaces one file's generated mark. A mark
		/// that says nothing is refused as `invalid_record`.
		[[nodiscard]] virtual auto store_generated_mark(
			const core::generated_file_mark& aMark)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief RFC-BPR-0089 M2. Removes one file's generated mark. Missing
		/// is an idempotent success.
		[[nodiscard]] virtual auto erase_generated_mark(const core::file_identity& aFile)
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
