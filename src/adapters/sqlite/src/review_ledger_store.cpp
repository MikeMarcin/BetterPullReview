#include "bpr/adapters/sqlite/review_ledger_store.hpp"

#include "review_ledger_database.hpp"

#include <system_error>
#include <utility>

#if !defined(_WIN32)
	#include <sys/stat.h>
#endif

namespace bpr::adapters::sqlite {
	namespace {
		constexpr std::string_view LedgerName = "review-ledger.sqlite3";

		[[nodiscard]] auto translate(const detail::catalog_database_failure aFailure) noexcept -> application::ledger_store_failure
		{
			switch (aFailure) {
			case detail::catalog_database_failure::integrity_failure:
			case detail::catalog_database_failure::schema_mismatch:
				return application::ledger_store_failure::integrity_failure;
			case detail::catalog_database_failure::cancelled:
				return application::ledger_store_failure::cancelled;
			case detail::catalog_database_failure::quota_exceeded:
				return application::ledger_store_failure::conflict;
			case detail::catalog_database_failure::invalid_path:
			case detail::catalog_database_failure::invalid_permissions:
			case detail::catalog_database_failure::unavailable:
			case detail::catalog_database_failure::platform_unsupported:
				break;
			}
			return application::ledger_store_failure::unavailable;
		}

		/// \brief Ensures the state directory exists and is owner-private.
		///
		/// \details The epoch catalog builds this same directory through a
		/// hardened platform-specific bootstrap. This adapter must not depend on
		/// the catalog having been composed first, so it creates what it needs
		/// and narrows the mode. Reaching full parity with the catalog's
		/// bootstrap is tracked separately rather than assumed here.
		[[nodiscard]] auto ensure_state_directory(
			const std::filesystem::path& aRoot)
			-> std::expected<std::filesystem::path, application::ledger_store_failure>
		{
			if (aRoot.empty()) {
				return std::unexpected(application::ledger_store_failure::unavailable);
			}

			auto directory = aRoot / "state" / "v1";
			auto code      = std::error_code{ };
			std::filesystem::create_directories(directory, code);
			if (code && !std::filesystem::is_directory(directory)) {
				return std::unexpected(application::ledger_store_failure::unavailable);
			}

#if !defined(_WIN32)
			if (::chmod(directory.c_str(), S_IRWXU) != 0) {
				return std::unexpected(application::ledger_store_failure::unavailable);
			}
#endif
			return directory;
		}

		/// \brief Binds one durable ledger to one review scope.
		class sqlite_review_ledger_store final
			: public application::review_ledger_store_port
		{
		public:
			sqlite_review_ledger_store(
				detail::review_ledger_database aDatabase,
				const std::int64_t             aScope) noexcept
				: Database{ std::move(aDatabase) }
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

		private:
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
		auto directory = ensure_state_directory(aConfiguration.StorageRoot);
		if (!directory) {
			return std::unexpected(directory.error());
		}

		auto opened = detail::review_ledger_database::open(
			*directory / LedgerName,
			std::move(aStop));
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
			std::move(*opened),
			*scope);
	}
} // namespace bpr::adapters::sqlite
