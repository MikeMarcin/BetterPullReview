#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP

#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/review_ledger.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <expected>
#include <mutex>
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
		std::uint64_t ReceiptLoads         = 0U;
		std::uint64_t ReceiptLoadHits      = 0U;
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
	struct conversation_thread_observation
	{
		core::review_conversation_id                     Conversation;
		std::vector<core::conversation_post_observation> Posts;

		auto operator<=>(const conversation_thread_observation&) const = default;
	};

	/// \brief What the reader has not seen in one conversation.
	struct conversation_read_projection
	{
		core::review_conversation_id         Conversation;
		std::vector<core::review_comment_id> UnreadPosts;

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

	/// \brief Turns receipt commands into durable state and published projections.
	///
	/// \details The order is the contract. A command is decided by the core, the
	/// decision becomes a persistence request, the store answers with a typed
	/// completion, and only then is the accepted projection published. A
	/// projection published before the commit would show a reader a state the
	/// database does not hold — and would survive exactly until the next
	/// restart contradicted it.
	class review_ledger_application_host final : public agent_session_catalog_port
	{
	public:
		review_ledger_application_host(
			review_ledger_store_port& aStore,
			receipt_projection_port&  aProjection) noexcept;

		/// \brief Seeds the retained receipt for one file.
		///
		/// \details The host holds one file's receipt at a time, because the
		/// core decides against the receipt the reader is acting on. It does
		/// not hold the ledger.
		void retain(const core::file_receipt& aReceipt);

		/// \brief Loads and carries one receipt to exact selected material.
		///
		/// \details An absent receipt becomes an in-memory unread receipt without a
		/// write. Changed material is carried by the core and committed before it is
		/// retained or published. Thus merely reopening unchanged material performs
		/// zero writes, while an old receipt can never be applied to new material.
		[[nodiscard]] auto observe_material(
			const core::file_material_observation& aObservation)
			-> std::expected<core::file_receipt, ledger_store_failure>;

		/// \brief Decides, commits, and publishes one receipt command.
		///
		/// \returns The core's outcome. A refused command never reaches the
		/// store, so a rejection here means nothing was written.
		auto submit(const core::file_receipt_command& aCommand)
			-> core::file_receipt_outcome;

		/// \brief Persists one validated local note.
		[[nodiscard]] auto persist(const local_note_record& aNote)
			-> std::expected<void, ledger_store_failure>;

		/// \brief Deletes one local note identity.
		[[nodiscard]] auto erase(local_note_id aNote)
			-> std::expected<void, ledger_store_failure>;

		/// \brief Persists the exact head last reviewed by this reviewer.
		[[nodiscard]] auto persist_last_reviewed_head(
			const core::git_commit_oid& aHead)
			-> std::expected<void, ledger_store_failure>;

		/// \brief Clears the last-reviewed head.
		[[nodiscard]] auto erase_last_reviewed_head()
			-> std::expected<void, ledger_store_failure>;

		/// \brief Persists one validated local defect and its retained history.
		[[nodiscard]] auto persist(const durable_defect_record& aDefect)
			-> std::expected<void, ledger_store_failure>;

		/// \brief Deletes one local defect identity.
		[[nodiscard]] auto erase(core::defect_id aDefect)
			-> std::expected<void, ledger_store_failure>;

		/// \brief Loads one bounded deterministic private-state observation.
		[[nodiscard]] auto private_observation()
			-> std::expected<private_ledger_observation, ledger_store_failure>;

		/// \brief Durably admits one immutable metadata-only agent session.
		[[nodiscard]] auto admit(const agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, ledger_store_failure> override;

		/// \brief Loads the latest retained session and next durable identity.
		[[nodiscard]] auto session_catalog()
			-> std::expected<agent_session_catalog_state, ledger_store_failure> override;

		/// \brief Reads one file's durable receipt without retaining or publishing it.
		///
		/// \details [[BPR-0647]]. The overview names every file's read state,
		/// which is the ledger's to answer; this is a read of the store through
		/// the host, so no composition reaches for the store itself. An absent
		/// receipt is an engaged optional's absence, not a failure.
		[[nodiscard]] auto load_receipt(const core::file_identity& aFile)
			-> std::expected<std::optional<core::file_receipt>, ledger_store_failure>;

		/// \brief Decides the read state of the observed conversations.
		///
		/// \details [[BPR-0656]]. Loads the review's conversation receipts
		/// from the store once and keeps them; each thread is judged by
		/// `core::unread_posts` against its receipt, or against an empty
		/// receipt when none was ever written. A store failure is typed and
		/// leaves nothing cached, so the next observation asks again.
		[[nodiscard]] auto observe_conversations(
			std::span<const conversation_thread_observation> aThreads)
			-> std::expected<conversation_read_state, ledger_store_failure>;

		/// \brief Reads one conversation up to its latest post, durably.
		///
		/// \details The receipt the core builds is stored before it is kept,
		/// so a refused write changes nothing the next observation reports.
		[[nodiscard]] auto mark_conversation_read(
			const core::conversation_observation& aLatest)
			-> std::expected<void, ledger_store_failure>;
		[[nodiscard]] auto retained() const noexcept -> core::file_receipt;
		[[nodiscard]] auto statistics() const noexcept
			-> review_ledger_host_statistics;

	private:
		review_ledger_store_port* Store      = nullptr;
		receipt_projection_port*  Projection = nullptr;
		[[nodiscard]] auto        conversation_receipts_locked()
			-> std::expected<void, ledger_store_failure>;

		core::file_receipt                                     Retained;
		std::optional<std::vector<core::conversation_receipt>> ConversationReceipts;
		review_ledger_host_statistics                          Statistics;
		mutable std::mutex                                     Mutex;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_APPLICATION_HOST_HPP
