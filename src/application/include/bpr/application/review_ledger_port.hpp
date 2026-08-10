#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_LEDGER_PORT_HPP

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
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
	inline constexpr std::size_t maximum_private_notes   = 4'096U;
	inline constexpr std::size_t maximum_private_defects = 4'096U;

	using local_note_id = core::strong_id<struct local_note_id_tag>;

	/// \brief Binds a private note to exact compared material.
	struct local_note_attachment
	{
		core::file_identity          File;
		core::file_comparison_digest Comparison;

		auto operator<=>(const local_note_attachment&) const = default;
	};

	/// \brief One durable private note.
	///
	/// \details A note may describe the review as a whole or one exact file
	/// comparison. It never stores a path, source body, or presentation
	/// coordinate. The text uses the same bound as an explicitly selected agent
	/// note so persistence cannot create a value that downstream context must
	/// silently truncate.
	struct local_note_record
	{
		local_note_id                        Id;
		std::optional<local_note_attachment> Attachment;
		core::supplement_note                Note;

		auto operator<=>(const local_note_record&) const = default;
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

	/// \brief Reports whether one note can enter the durable authority.
	[[nodiscard]] auto valid(const local_note_record& aNote) noexcept -> bool;

	/// \brief Reports whether one defect can enter the durable authority.
	[[nodiscard]] auto valid(const durable_defect_record& aDefect) noexcept -> bool;

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

		/// \brief Upserts one bounded private note.
		[[nodiscard]] virtual auto store_note(const local_note_record& aNote)
			-> std::expected<void, ledger_store_failure> = 0;

		/// \brief Removes one note identity. Missing is an idempotent success.
		[[nodiscard]] virtual auto erase_note(local_note_id aNote)
			-> std::expected<void, ledger_store_failure> = 0;

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
