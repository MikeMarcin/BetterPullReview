#include "bpr/adapters/sqlite/review_ledger_store.hpp"

#include "review_ledger_database.hpp"
#include "state_directory.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <system_error>
#include <utility>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::string_view LedgerName = "review-ledger.sqlite3";

		[[nodiscard]] constexpr auto durable_integer(const std::uint64_t aValue) noexcept -> bool
		{
			return aValue > 0U
				&& aValue
				<= static_cast<std::uint64_t>(
					   std::numeric_limits<std::int64_t>::max());
		}

		[[nodiscard]] auto translate(const detail::catalog_database_failure aFailure) noexcept -> application::ledger_store_failure
		{
			switch (aFailure) {
			case detail::catalog_database_failure::integrity_failure:
			case detail::catalog_database_failure::schema_mismatch:
				return application::ledger_store_failure::integrity_failure;
			case detail::catalog_database_failure::cancelled:
				return application::ledger_store_failure::cancelled;
			case detail::catalog_database_failure::quota_exceeded:
				return application::ledger_store_failure::quota_exceeded;
			case detail::catalog_database_failure::invalid_path:
			case detail::catalog_database_failure::invalid_permissions:
			case detail::catalog_database_failure::unavailable:
			case detail::catalog_database_failure::platform_unsupported:
				break;
			}
			return application::ledger_store_failure::unavailable;
		}

		/// \brief Translates a state-directory refusal for this store.
		[[nodiscard]] constexpr auto translate(
			const detail::state_directory_failure aFailure) noexcept
			-> application::ledger_store_failure
		{
			switch (aFailure) {
			case detail::state_directory_failure::invalid_path:
			case detail::state_directory_failure::invalid_permissions:
			case detail::state_directory_failure::platform_unsupported:
			case detail::state_directory_failure::unavailable:
				break;
			}
			return application::ledger_store_failure::unavailable;
		}

		/// \brief Binds one durable ledger to one review scope.
		class sqlite_review_ledger_store final
			: public application::review_ledger_store_port
		{
		public:
			sqlite_review_ledger_store(
				detail::state_directory        aDirectory,
				detail::review_ledger_database aDatabase,
				const std::int64_t             aScope) noexcept
				: Directory{ std::move(aDirectory) }
				, Database{ std::move(aDatabase) }
				, Scope{ aScope }
			{
			}

			auto commit(const application::receipt_persistence_request& aRequest)
				-> std::expected<void, application::ledger_store_failure> override
			{
				const auto stored = Database.store_receipt(Scope, aRequest.Receipt);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				return { };
			}

			auto load(const core::file_identity& aFile) -> std::expected<
				std::optional<core::file_receipt>,
				application::ledger_store_failure> override
			{
				auto loaded = Database.load_receipt(Scope, aFile);
				if (!loaded) {
					return std::unexpected(translate(loaded.error()));
				}
				return *loaded;
			}

			auto store_conversation_receipt(const core::conversation_receipt& aReceipt)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!application::valid(aReceipt)) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto stored = Database.store_conversation_receipt(Scope, aReceipt);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				return { };
			}

			auto store_conversation_receipts(const std::span<const core::conversation_receipt> aReceipts)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (aReceipts.size() > application::maximum_conversation_receipts
					|| !std::ranges::all_of(aReceipts, [](const core::conversation_receipt& aReceipt) { return application::valid(aReceipt); })) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto stored = Database.store_conversation_receipts(Scope, aReceipts);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				return { };
			}

			auto load_conversation_receipts() -> std::expected<
				std::vector<core::conversation_receipt>,
				application::ledger_store_failure> override
			{
				auto loaded = Database.load_conversation_receipts(Scope);
				if (!loaded) {
					return std::unexpected(translate(loaded.error()));
				}
				return std::move(*loaded);
			}

			auto store_note(const application::local_note_record&                 aNote,
							const std::optional<application::local_note_revision> aExpectedRevision)
				-> std::expected<application::local_note_record, application::local_note_failure> override
			{
				return Database.store_note(Scope, aNote, aExpectedRevision);
			}

			auto erase_note(const application::local_note_id       aNote,
							const application::local_note_revision aExpectedRevision)
				-> std::expected<void, application::local_note_failure> override
			{
				return Database.erase_note(Scope, aNote, aExpectedRevision);
			}

			auto store_last_reviewed_head(const core::git_commit_oid& aHead)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!aHead.valid()) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto stored = Database.store_last_reviewed_head(Scope, aHead);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				return { };
			}

			auto erase_last_reviewed_head()
				-> std::expected<void, application::ledger_store_failure> override
			{
				auto erased = Database.erase_last_reviewed_head(Scope);
				if (!erased) {
					return std::unexpected(translate(erased.error()));
				}
				return { };
			}

			auto store_defect(
				const application::durable_defect_record& aDefect)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!application::valid(aDefect)) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto stored = Database.store_defect(Scope, aDefect);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				if (*stored == detail::defect_store_outcome::identity_conflict) {
					return std::unexpected(
						application::ledger_store_failure::conflict);
				}
				return { };
			}

			auto erase_defect(const core::defect_id aDefect)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!durable_integer(aDefect.value())) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto erased = Database.erase_defect(Scope, aDefect);
				if (!erased) {
					return std::unexpected(translate(erased.error()));
				}
				return { };
			}

			auto store_generated_mark(const core::generated_file_mark& aMark)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!aMark.File.valid() || aMark.Mark > core::generated_mark::treat_as_generated
					|| (aMark.Mark == core::generated_mark::none && !aMark.ReviewAnyway)) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto stored = Database.store_generated_mark(Scope, aMark);
				if (!stored) {
					return std::unexpected(translate(stored.error()));
				}
				return { };
			}

			auto erase_generated_mark(const core::file_identity& aFile)
				-> std::expected<void, application::ledger_store_failure> override
			{
				if (!aFile.valid()) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto erased = Database.erase_generated_mark(Scope, aFile);
				if (!erased) {
					return std::unexpected(translate(erased.error()));
				}
				return { };
			}

			auto load_private() -> std::expected<
				application::private_ledger_observation,
				application::ledger_store_failure> override
			{
				auto loaded = Database.load_private(Scope);
				if (!loaded) {
					return std::unexpected(translate(loaded.error()));
				}
				return std::move(*loaded);
			}

			auto admit_agent_session(
				const application::agent_session_admission& aAdmission)
				-> std::expected<core::agent_session, application::ledger_store_failure> override
			{
				if (!application::valid(aAdmission)) {
					return std::unexpected(
						application::ledger_store_failure::invalid_record);
				}
				auto admitted = Database.admit_agent_session(Scope, aAdmission);
				if (!admitted) {
					return std::unexpected(translate(admitted.error()));
				}
				return *admitted;
			}

			auto load_agent_session_catalog() -> std::expected<
				application::agent_session_catalog_state,
				application::ledger_store_failure> override
			{
				auto loaded = Database.load_agent_session_catalog(Scope);
				if (!loaded) {
					return std::unexpected(translate(loaded.error()));
				}
				return *loaded;
			}

		private:
			// [[BPR-0883]]. Held for as long as the database is open, as
			// `state_directory.hpp` promises. Declared first, released last.
			detail::state_directory        Directory;
			detail::review_ledger_database Database;
			std::int64_t                   Scope = 0;
		};
	} // namespace

	auto make_review_ledger_store(
		const review_ledger_configuration& aConfiguration,
		std::stop_token                    aStop)
		-> std::expected<
			std::unique_ptr<application::review_ledger_store_port>,
			application::ledger_store_failure>
	{
		// The same bootstrap the epoch catalog uses. This store used to make
		// the directory itself, which meant whichever store opened first
		// decided what it was — and the one that got there first was often
		// this one, with the weaker policy.
		auto directory = detail::open_state_directory(aConfiguration.StorageRoot);
		if (!directory) {
			return std::unexpected(translate(directory.error()));
		}

		auto opened = detail::review_ledger_database::open(
			*directory, LedgerName, std::move(aStop));
		if (!opened) {
			return std::unexpected(translate(opened.error()));
		}

		const auto scope = opened->scope_id(detail::review_scope{
			.Host        = aConfiguration.Host,
			.Account     = aConfiguration.Account,
			.Repository  = aConfiguration.Repository,
			.PullRequest = aConfiguration.PullRequest,
		});
		if (!scope) {
			return std::unexpected(translate(scope.error()));
		}

		return std::make_unique<sqlite_review_ledger_store>(
			std::move(*directory),
			std::move(*opened),
			*scope);
	}
} // namespace bpr::adapters::sqlite
