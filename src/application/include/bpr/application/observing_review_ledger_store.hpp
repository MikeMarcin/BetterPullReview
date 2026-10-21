#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OBSERVING_REVIEW_LEDGER_STORE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OBSERVING_REVIEW_LEDGER_STORE_HPP
#include "bpr/application/review_ledger_port.hpp"
#include <functional>
#include <memory>

namespace bpr::application {
	/// \brief Identifies the accepted durable domain requiring observation.
	enum class ledger_change_domain : std::uint8_t { receipt,
													 conversation,
													 private_records };
	/// \brief A bounded invalidation; receipt changes may identify one exact file.
	struct ledger_change
	{
		ledger_change_domain               Domain = ledger_change_domain::private_records;
		std::optional<core::file_identity> File;
		auto                               operator<=>(const ledger_change&) const = default;
	};
	/// \brief Owns a ledger store and observes only successful mutations.
	/// \details The callback executes on the caller's lane and must marshal downstream
	/// work. It must not reenter a mutating host. Callback exceptions cannot veto a
	/// completed durable operation; store exceptions propagate unchanged.
	class observing_review_ledger_store final : public review_ledger_store_port
	{
	public:
		/// \brief Takes a non-null store and an optional accepted-write observer.
		observing_review_ledger_store(std::unique_ptr<review_ledger_store_port> aStore, std::function<void(ledger_change)> aObserver);
		[[nodiscard]] auto commit(const receipt_persistence_request& aRequest) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto load(const core::file_identity& aFile) -> std::expected<std::optional<core::file_receipt>, ledger_store_failure> override;
		[[nodiscard]] auto store_conversation_receipt(const core::conversation_receipt& aReceipt) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto load_conversation_receipts() -> std::expected<std::vector<core::conversation_receipt>, ledger_store_failure> override;
		[[nodiscard]] auto store_note(const local_note_record& aNote, std::optional<local_note_revision> aExpectedRevision = std::nullopt) -> std::expected<local_note_record, local_note_failure> override;
		[[nodiscard]] auto erase_note(local_note_id aNote, local_note_revision aExpectedRevision) -> std::expected<void, local_note_failure> override;
		[[nodiscard]] auto store_last_reviewed_head(const core::git_commit_oid& aHead) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto erase_last_reviewed_head() -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto store_defect(const durable_defect_record& aDefect) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto erase_defect(core::defect_id aDefect) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto store_generated_mark(const core::generated_file_mark& aMark) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto erase_generated_mark(const core::file_identity& aFile) -> std::expected<void, ledger_store_failure> override;
		[[nodiscard]] auto load_private() -> std::expected<private_ledger_observation, ledger_store_failure> override;
		[[nodiscard]] auto admit_agent_session(const agent_session_admission& aAdmission) -> std::expected<core::agent_session, ledger_store_failure> override;
		[[nodiscard]] auto load_agent_session_catalog() -> std::expected<agent_session_catalog_state, ledger_store_failure> override;

	private:
		void                                      notify(ledger_change aChange) noexcept;
		std::unique_ptr<review_ledger_store_port> Store;
		std::function<void(ledger_change)>        Observer;
	};
}
#endif
