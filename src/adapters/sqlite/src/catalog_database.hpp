#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_CATALOG_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_CATALOG_DATABASE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	[[nodiscard]] auto secure_random_bytes(
		std::span<std::uint8_t> aOutput) noexcept -> bool;

	enum class sqlite_result_classification {
		unavailable,
		busy,
		integrity_failure,
		cancelled,
		quota_exceeded,
	};

	[[nodiscard]] auto classify_sqlite_result(int aResultCode) noexcept
		-> sqlite_result_classification;

	enum class catalog_database_failure {
		invalid_path,
		invalid_permissions,
		unavailable,
		schema_mismatch,
		integrity_failure,
		quota_exceeded,
		cancelled,
		platform_unsupported,
	};

	struct catalog_database_platform_state;

	class catalog_database final
	{
	public:
		catalog_database()                                           = default;
		catalog_database(const catalog_database&)                    = delete;
		auto operator=(const catalog_database&) -> catalog_database& = delete;
		catalog_database(catalog_database&& aOther) noexcept;
		auto operator=(catalog_database&& aOther) noexcept
			-> catalog_database&;
		~catalog_database();

		[[nodiscard]] static auto open(
			const std::filesystem::path& aStorageRoot,
			std::stop_token              aStop)
			-> std::expected<catalog_database, catalog_database_failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;
		[[nodiscard]] auto path() const noexcept
			-> const std::filesystem::path&;

	private:
		catalog_database(
			::sqlite3*            aHandle,
			std::filesystem::path aPath) noexcept;
		catalog_database(
			::sqlite3*                                       aHandle,
			std::filesystem::path                            aPath,
			std::unique_ptr<catalog_database_platform_state> aPlatformState) noexcept;

		::sqlite3*                                       Handle = nullptr;
		std::filesystem::path                            Path;
		std::unique_ptr<catalog_database_platform_state> PlatformState;
	};
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_CATALOG_DATABASE_HPP
