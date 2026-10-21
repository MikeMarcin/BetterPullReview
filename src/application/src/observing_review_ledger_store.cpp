#include "bpr/application/observing_review_ledger_store.hpp"
#include <stdexcept>
#include <utility>
namespace bpr::application {
	observing_review_ledger_store::observing_review_ledger_store(std::unique_ptr<review_ledger_store_port> aStore, std::function<void(ledger_change)> aObserver)
		: Store{ std::move(aStore) }
		, Observer{ std::move(aObserver) }
	{
		if (!Store) {
			throw std::invalid_argument{ "A ledger observer requires a store." };
		}
	}
	void observing_review_ledger_store::notify(const ledger_change aChange) noexcept
	{
		try {
			if (Observer) {
				Observer(aChange);
			}
		} catch (...) {
			// The durable mutation has already succeeded.
		}
	}
	auto observing_review_ledger_store::commit(const receipt_persistence_request& aRequest) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->commit(aRequest);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::receipt, .File = aRequest.Receipt.File });
		}
		return result;
	}
	auto observing_review_ledger_store::load(const core::file_identity& aFile) -> std::expected<std::optional<core::file_receipt>, ledger_store_failure>
	{
		return Store->load(aFile);
	}
	auto observing_review_ledger_store::store_conversation_receipt(const core::conversation_receipt& aReceipt) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->store_conversation_receipt(aReceipt);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::conversation });
		}
		return result;
	}
	auto observing_review_ledger_store::load_conversation_receipts() -> std::expected<std::vector<core::conversation_receipt>, ledger_store_failure>
	{
		return Store->load_conversation_receipts();
	}
	auto observing_review_ledger_store::store_note(const local_note_record& aNote, std::optional<local_note_revision> aExpectedRevision) -> std::expected<local_note_record, local_note_failure>
	{
		auto result = Store->store_note(aNote, aExpectedRevision);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::erase_note(local_note_id aNote, local_note_revision aExpectedRevision) -> std::expected<void, local_note_failure>
	{
		auto result = Store->erase_note(aNote, aExpectedRevision);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::store_last_reviewed_head(const core::git_commit_oid& aHead) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->store_last_reviewed_head(aHead);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::erase_last_reviewed_head() -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->erase_last_reviewed_head();
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::store_defect(const durable_defect_record& aDefect) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->store_defect(aDefect);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::erase_defect(core::defect_id aDefect) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->erase_defect(aDefect);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::store_generated_mark(const core::generated_file_mark& aMark) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->store_generated_mark(aMark);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::erase_generated_mark(const core::file_identity& aFile) -> std::expected<void, ledger_store_failure>
	{
		auto result = Store->erase_generated_mark(aFile);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::load_private() -> std::expected<private_ledger_observation, ledger_store_failure>
	{
		return Store->load_private();
	}
	auto observing_review_ledger_store::admit_agent_session(const agent_session_admission& aAdmission) -> std::expected<core::agent_session, ledger_store_failure>
	{
		auto result = Store->admit_agent_session(aAdmission);
		if (result) {
			notify(ledger_change{ .Domain = ledger_change_domain::private_records });
		}
		return result;
	}
	auto observing_review_ledger_store::load_agent_session_catalog() -> std::expected<agent_session_catalog_state, ledger_store_failure>
	{
		return Store->load_agent_session_catalog();
	}
}
