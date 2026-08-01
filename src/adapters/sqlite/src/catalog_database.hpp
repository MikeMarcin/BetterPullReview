#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>

struct sqlite3;

namespace bpr::adapters::sqlite::detail
{
[[nodiscard]] auto secure_random_bytes(
    std::span<std::uint8_t> output) noexcept -> bool;

enum class sqlite_result_classification
{
    unavailable,
    busy,
    integrity_failure,
    cancelled,
    quota_exceeded,
};

[[nodiscard]] auto classify_sqlite_result(int result_code) noexcept
    -> sqlite_result_classification;

enum class catalog_database_failure
{
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
    catalog_database() = default;
    catalog_database(const catalog_database&) = delete;
    auto operator=(const catalog_database&) -> catalog_database& = delete;
    catalog_database(catalog_database&& other) noexcept;
    auto operator=(catalog_database&& other) noexcept
        -> catalog_database&;
    ~catalog_database();

    [[nodiscard]] static auto open(
        const std::filesystem::path& storage_root,
        std::stop_token stop)
        -> std::expected<catalog_database, catalog_database_failure>;

    [[nodiscard]] auto handle() const noexcept -> ::sqlite3*;
    [[nodiscard]] auto path() const noexcept
        -> const std::filesystem::path&;

private:
    catalog_database(
        ::sqlite3* handle,
        std::filesystem::path path) noexcept;
    catalog_database(
        ::sqlite3* handle,
        std::filesystem::path path,
        std::unique_ptr<catalog_database_platform_state> platform_state)
        noexcept;

    ::sqlite3* Handle = nullptr;
    std::filesystem::path Path;
    std::unique_ptr<catalog_database_platform_state> PlatformState;
};
} // namespace bpr::adapters::sqlite::detail
