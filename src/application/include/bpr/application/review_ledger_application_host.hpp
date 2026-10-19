#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP

#include "bpr/application/insights_observer.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/review_ledger.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace bpr::application {
	/// \brief Records directly observable ledger-host activity.
	struct review_ledger_host_statistics
	{
		std::uint64_t Commands             = 0U;
		std::uint64_t Rejected             = 0U;
		std::uint64_t Unchanged            = 0U;
		std::uint64_t PersistenceRequests  = 0U;
		std::uint64_t Completions          = 0U;
		std::uint64_t StoreFailures        = 0U;
		std::uint64_t Publications         = 0U;
		std::uint64_t FailurePublications  = 0U;
		std::uint64_t MaterialObservations = 0U;
		/// Durable receipt reads that reached the store.
		std::uint64_t ReceiptLoads    = 0U;
		std::uint64_t ReceiptLoadHits = 0U;
		/// [[BPR-0972]]. `cached_receipt` answers served by the retained durable
		/// receipts without a store read.
		std::uint64_t ReceiptCacheHits = 0U;
		/// [[BPR-0972]]. Retained durable receipts dropped because another
		/// writer may have changed them.
		std::uint64_t ReceiptInvalidations = 0U;
		/// [[BPR-0824]]. The subset of `ReceiptLoadHits` found by
		/// `observe_material`, so a later `refresh_receipt` cannot count as one.
		std::uint64_t MaterialReceiptHits  = 0U;
		std::uint64_t PrivateWrites        = 0U;
		std::uint64_t PrivateDeletes       = 0U;
		std::uint64_t PrivateLoads         = 0U;
		std::uint64_t PrivateFailures      = 0U;
		std::uint64_t SessionAdmissions    = 0U;
		std::uint64_t SessionLoads         = 0U;
		std::uint64_t SessionFailures      = 0U;
		std::uint64_t ConversationLoads    = 0U;
		std::uint64_t ConversationWrites   = 0U;
		std::uint64_t ConversationFailures = 0U;
		/// [[BPR-1211]]. Store operations admitted to the effect lane.
		std::uint64_t Operations = 0U;
		/// Operations refused at `maximum_outstanding_ledger_operations`.
		std::uint64_t AdmissionRefusals = 0U;
		/// Operations answered `cancelled`: queued at, or asked after, retirement.
		std::uint64_t CancelledOperations = 0U;
		/// Completions that arrived after retirement; none was applied.
		std::uint64_t LateCompletions = 0U;
		/// The most operations held at once.
		std::uint64_t PeakOutstanding = 0U;

		auto operator<=>(const review_ledger_host_statistics&) const = default;
	};

	enum class file_receipt_action_id : std::uint8_t {
		seen,
		read,
		accepted,
		excluded,
	};

	/// \brief Carries one core-decided receipt gesture to presentation.
	struct file_receipt_action_command_projection
	{
		file_receipt_action_id     Id = file_receipt_action_id::seen;
		core::file_receipt_command Command;
		bool                       Enabled = false;

		auto operator<=>(const file_receipt_action_command_projection&) const
			= default;
	};

	/// \brief Presents the current receipt and its four compact gesture slots.
	struct file_receipt_action_projection
	{
		core::file_receipt                                    Receipt;
		std::array<file_receipt_action_command_projection, 4> Actions;

		auto operator<=>(const file_receipt_action_projection&) const = default;
	};

	/// \brief Projects receipt gesture legality through the core transition.
	[[nodiscard]] auto project_file_receipt_actions(
		const core::file_receipt& aReceipt) noexcept
		-> file_receipt_action_projection;

	/// \brief One conversation of the accepted snapshot, as the host saw it.
	///
	/// \details [[BPR-0656]]. The posts in thread order, root first. The
	/// identities are the durable halves the composition derives from the
	/// GitHub node ids; the edit versions are digests of the bodies, so an
	/// edit is a different version without the provider naming a time.
	/// [[BPR-1601]]. A PR comment or review summary is a one-post conversation;
	/// the existing receipt keeps its read cursor without a storage migration.
	struct conversation_thread_observation
	{
		core::review_conversation_id                     Conversation;
		std::vector<core::conversation_post_observation> Posts;

		auto operator<=>(const conversation_thread_observation&) const = default;
	};

	/// \brief What the reader has not seen in one conversation.
	///
	/// \details `SeenBefore` says a receipt exists: the reader has read this
	/// conversation up to some post. Without one every post is unread, but
	/// nothing is *newer than what I read*, which is what the queue counts
	/// ([[BPR-0659]]).
	struct conversation_read_projection
	{
		core::review_conversation_id         Conversation;
		std::vector<core::review_comment_id> UnreadPosts;
		bool                                 SeenBefore = false;

		auto operator<=>(const conversation_read_projection&) const = default;
	};

	/// \brief The read state of every conversation observed, unread first.
	///
	/// \details Conversations with nothing unread are omitted; a
	/// presentation reads absence as read. `UnreadThreads` counts the
	/// entries.
	struct conversation_read_state
	{
		std::vector<conversation_read_projection> Threads;
		std::uint32_t                             UnreadThreads = 0U;

		auto operator<=>(const conversation_read_state&) const = default;
	};

	/// \brief Bounds the ledger operations one host holds at once.
	///
	/// \details [[BPR-1211]]. Queued and running operations together. A gesture
	/// beyond the bound is refused with `ledger_store_failure::unavailable`
	/// rather than growing a queue behind slow storage.
	inline constexpr std::size_t maximum_outstanding_ledger_operations = 64U;

	/// \brief Opens the review's durable store. Runs on the effect lane.
	using review_ledger_store_opener = std::function<
		std::expected<std::unique_ptr<review_ledger_store_port>, ledger_store_failure>()>;

	/// \brief Receives one ledger operation's terminal on the decision lane.
	template <typename Value>
	using ledger_completion = std::function<void(std::expected<Value, ledger_store_failure>)>;

	/// \brief Receives one note operation's terminal on the decision lane.
	template <typename Value>
	using local_note_completion = std::function<void(std::expected<Value, local_note_failure>)>;

	/// \brief Turns receipt commands into durable state and published projections.
	///
	/// \details The order is the contract. A command is decided by the core, the
	/// decision becomes a persistence request, the store answers with a typed
	/// completion, and only then is the accepted projection published. A
	/// projection published before the commit would show a reader a state the
	/// database does not hold — and would survive exactly until the next
	/// restart contradicted it.
	///
	/// [[BPR-1211]]. Every store operation, including opening the store, runs
	/// on the effect executor, one at a time and in the order it was asked
	/// for. The core decides each command on the decision lane when the
	/// command reaches the head of that queue, against the durable state the
	/// operations before it left, and the typed completion returns to the
	/// decision lane before anything is retained or published. Every member is
	/// called on the decision lane and every terminal is delivered there,
	/// exactly once. No lock is held across storage, so a read here never
	/// waits for a slow store: reads answer from the durable values this host
	/// has already loaded or committed.
	///
	/// Retirement cancels queued operations with `ledger_store_failure::cancelled`,
	/// refuses the completion of a running one (the store may have committed it,
	/// but this host publishes nothing more), and closes the store on the
	/// effect lane once nothing is running.
	class review_ledger_application_host final
	{
	public:
		/// \brief Starts opening the store; `aOpened` answers on the decision lane.
		review_ledger_application_host(
			review_ledger_store_opener                                                      aOpen,
			receipt_projection_port&                                                        aProjection,
			effect_executor&                                                                aEffects,
			decision_executor&                                                              aDecisions,
			ledger_completion<void>                                                         aOpened         = { },
			std::size_t                                                                     aCapacity       = maximum_outstanding_ledger_operations,
			insights_review_capture                                                         aInsights       = { },
			std::function<void(const core::file_receipt&, core::file_receipt_command_kind)> aFileStatistics = { });
		~review_ledger_application_host();

		review_ledger_application_host(const review_ledger_application_host&)                    = delete;
		auto operator=(const review_ledger_application_host&) -> review_ledger_application_host& = delete;
		review_ledger_application_host(review_ledger_application_host&&)                         = delete;
		auto operator=(review_ledger_application_host&&) -> review_ledger_application_host&      = delete;

		/// \brief Cancels queued work and refuses every later completion.
		void retire() noexcept;
		/// \brief Whether the store opened; empty until the open completes.
		[[nodiscard]] auto available() const noexcept -> std::optional<bool>;
		/// \brief Operations queued or running.
		[[nodiscard]] auto outstanding() const noexcept -> std::size_t;
		/// \brief Nothing queued, running, or waiting to deliver its terminal.
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

		/// \brief Seeds the retained receipt for one file.
		///
		/// \details The host holds one file's receipt at a time, because the
		/// core decides against the receipt the reader is acting on. It does
		/// not hold the ledger. A material observation still running for
		/// another file no longer retains or publishes its answer.
		void retain(const core::file_receipt& aReceipt);

		/// \brief Loads and carries one receipt to exact selected material.
		///
		/// \details An absent receipt becomes an in-memory unread receipt without a
		/// write. Changed material is carried by the core and committed before it is
		/// retained or published. Thus merely reopening unchanged material performs
		/// zero writes, while an old receipt can never be applied to new material.
		/// A later observation or `retain` supersedes this one's publication.
		void observe_material(
			const core::file_material_observation& aObservation,
			ledger_completion<core::file_receipt>  aDone = { });

		/// \brief Reloads the retained file and carries to its material only in memory.
		/// \details Sibling invalidation must never write a carry back to durable state.
		void refresh_receipt(ledger_completion<core::file_receipt> aDone = { });

		/// \brief Decides, commits, and publishes one receipt command.
		///
		/// \details Decided when it reaches the head of the queue. A refused
		/// command never reaches the store, so a rejection means nothing was
		/// written.
		void submit(const core::file_receipt_command&             aCommand,
					ledger_completion<core::file_receipt_outcome> aDone = { });

		/// \brief One file's durable receipt, when this host already holds it.
		///
		/// \details [[BPR-0972]]. Outer absence: not yet read; inner absence:
		/// read, and no receipt exists. Never reads the store.
		[[nodiscard]] auto cached_receipt(const core::file_identity& aFile)
			-> std::optional<std::optional<core::file_receipt>>;

		/// \brief Reads the durable receipts of these files that are not yet held.
		///
		/// \details [[BPR-0647]], [[BPR-0972]]. The first answer for a file is
		/// read from the store and retained, absence included; later answers
		/// come from that retained durable value. Every commit this host makes
		/// updates it, so only a write by another host can make it stale, and
		/// that writer's notification must reach `invalidate_receipts` first. A
		/// store failure retains nothing, and neither does an answer read before
		/// an invalidation of its file.
		void load_receipts(std::vector<core::file_identity> aFiles, ledger_completion<void> aDone);

		/// \brief Drops the retained durable receipts of files another writer changed.
		void invalidate_receipts(std::span<const core::file_identity> aFiles);

		/// \brief Drops every retained durable receipt, for a change naming no file.
		void invalidate_all_receipts();

		/// \brief Counts changes to the durable receipts `cached_receipt` answers.
		///
		/// \details Advances on every commit and every invalidation, and when a
		/// fresh store read replaces a retained value with a different one. A
		/// reader that kept the revision of its last complete read may skip
		/// rereading while it is unchanged: nothing it could see has moved.
		[[nodiscard]] auto durable_receipt_revision() const noexcept -> std::uint64_t;

		/// \brief Names the files whose durable receipts moved after a revision.
		///
		/// \details [[BPR-0972]]. Answers from a bounded record of the most
		/// recent changes. Nothing is answered when that record no longer
		/// reaches back to `aRevision`, or a change named no file; the reader
		/// then rereads everything it shows.
		[[nodiscard]] auto durable_changes_since(std::uint64_t aRevision) const
			-> std::optional<std::vector<core::file_identity>>;

		/// \brief Decides the read state of the observed conversations.
		///
		/// \details [[BPR-0656]]. Each thread is judged by `core::unread_posts`
		/// against its receipt, or against an empty receipt when none was ever
		/// written. Empty until `load_conversation_receipts` has succeeded.
		[[nodiscard]] auto unread_conversations(
			std::span<const conversation_thread_observation> aThreads) const
			-> std::optional<conversation_read_state>;

		/// \brief Loads the review's conversation receipts once and keeps them.
		/// \details A failure is typed and keeps nothing, so a later call asks again.
		void load_conversation_receipts(ledger_completion<void> aDone);

		/// \brief Discards cached conversation receipts after a sibling write.
		void invalidate_conversation_receipts();

		/// \brief Reads one conversation up to its latest post, durably.
		///
		/// \details The receipt the core builds is stored before it is kept,
		/// so a refused write changes nothing the next observation reports.
		void mark_conversation_read(const core::conversation_observation& aLatest, ledger_completion<void> aDone);
		/// \brief [[BPR-1582]]. Makes the latest post someone else wrote in one
		/// conversation new again, through `core::mark_conversation_unread`.
		///
		/// \details Answers `invalid_record` without writing when every post
		/// is mine, since nothing could become new.
		void mark_conversation_unread(const conversation_thread_observation& aThread, ledger_completion<void> aDone);

		/// \brief The bounded private-state observation last loaded or committed.
		/// \details Never reads the store; `unavailable` until `load_private` succeeds.
		[[nodiscard]] auto private_observation() const
			-> std::expected<private_ledger_observation, ledger_store_failure>;

		/// \brief Loads the bounded deterministic private-state observation.
		/// \details A failure keeps the last loaded observation.
		void load_private(ledger_completion<void> aDone);

		/// \brief Persists one validated local note.
		void persist(const local_note_record&                 aNote,
					 std::optional<local_note_revision>       aExpectedRevision,
					 local_note_completion<local_note_record> aDone);

		/// \brief Deletes one local note identity.
		void erase(local_note_id aNote, local_note_revision aExpectedRevision, local_note_completion<void> aDone);

		/// \brief Returns the first rejected candidate, retained until explicit dismissal.
		[[nodiscard]] auto rejected_note() const -> std::optional<rejected_local_note_mutation>;
		/// \brief Explicitly dismisses this window's retained rejected candidate.
		void dismiss_rejected_note();

		/// \brief Persists the exact head last reviewed by this reviewer.
		void persist_last_reviewed_head(const core::git_commit_oid& aHead, ledger_completion<void> aDone);

		/// \brief Clears the last-reviewed head.
		void erase_last_reviewed_head(ledger_completion<void> aDone);

		/// \brief Persists one validated local defect and its retained history.
		void persist(const durable_defect_record& aDefect, ledger_completion<void> aDone);

		/// \brief Deletes one local defect identity.
		void erase(core::defect_id aDefect, ledger_completion<void> aDone);

		/// \brief Durably admits one immutable metadata-only agent session.
		void admit(const agent_session_admission& aAdmission, ledger_completion<core::agent_session> aDone);

		/// \brief Loads the latest retained session and next durable identity.
		void session_catalog(ledger_completion<agent_session_catalog_state> aDone);

		[[nodiscard]] auto retained() const noexcept -> core::file_receipt;
		[[nodiscard]] auto statistics() const noexcept
			-> review_ledger_host_statistics;

	private:
		/// \brief Writes the receipt `aDecide` makes of one conversation's
		/// current receipt, or answers `invalid_record` when it makes none.
		void store_conversation_receipt(
			core::review_conversation_id                                                                aConversation,
			std::function<std::optional<core::conversation_receipt>(const core::conversation_receipt&)> aDecide,
			ledger_completion<void>                                                                     aDone);

		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
