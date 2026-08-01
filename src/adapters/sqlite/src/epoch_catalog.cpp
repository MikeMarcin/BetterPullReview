#include "bpr/adapters/sqlite/epoch_catalog.hpp"

#include "catalog_database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::sqlite
{
namespace
{
namespace app = application;
namespace core = bpr::core;
using failure = app::epoch_catalog_failure;
template<typename Value>
using result = app::epoch_catalog_result<Value>;

class statement final
{
public:
    statement() = default;
    statement(const statement&) = delete;
    auto operator=(const statement&) -> statement& = delete;
    statement(statement&& other) noexcept
        : Value{std::exchange(other.Value, nullptr)}
    {
    }
    auto operator=(statement&& other) noexcept -> statement&
    {
        if(this != &other)
        {
            if(Value != nullptr)
            {
                (void)::sqlite3_finalize(Value);
            }
            Value = std::exchange(other.Value, nullptr);
        }
        return *this;
    }
    ~statement()
    {
        if(Value != nullptr)
        {
            (void)::sqlite3_finalize(Value);
        }
    }

    ::sqlite3_stmt* Value = nullptr;
};

struct stop_context
{
    std::stop_token Stop;
};

auto stop_progress(void* opaque) noexcept -> int
{
    const auto* context = static_cast<const stop_context*>(opaque);
    return context != nullptr && context->Stop.stop_requested() ? 1 : 0;
}

class progress_guard final
{
public:
    progress_guard(::sqlite3* database, std::stop_token stop)
        : Database{database}, Context{.Stop = stop}
    {
        ::sqlite3_progress_handler(Database, 128, stop_progress, &Context);
    }
    ~progress_guard()
    {
        ::sqlite3_progress_handler(Database, 0, nullptr, nullptr);
    }

private:
    ::sqlite3* Database;
    stop_context Context;
};

[[nodiscard]] auto sql_failure(
    ::sqlite3* database,
    std::stop_token stop,
    const failure fallback = failure::unavailable) noexcept -> failure
{
    if(stop.stop_requested())
    {
        return failure::cancelled;
    }
    switch(detail::classify_sqlite_result(
        ::sqlite3_extended_errcode(database)))
    {
    case detail::sqlite_result_classification::busy:
        return failure::busy;
    case detail::sqlite_result_classification::integrity_failure:
        return failure::integrity_failure;
    case detail::sqlite_result_classification::cancelled:
        return failure::cancelled;
    case detail::sqlite_result_classification::quota_exceeded:
        return failure::quota_exceeded;
    case detail::sqlite_result_classification::unavailable:
        return fallback;
    }
    return fallback;
}

[[nodiscard]] auto prepare(::sqlite3* database, std::string_view sql)
    -> result<statement>
{
    statement output;
    const char* tail = nullptr;
    const auto code = ::sqlite3_prepare_v3(
        database,
        sql.data(),
        static_cast<int>(sql.size()),
        SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB,
        &output.Value,
        &tail);
    if(code != SQLITE_OK || output.Value == nullptr)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    while(tail != sql.data() + sql.size())
    {
        const auto value = static_cast<unsigned char>(*tail);
        if(value != ' ' && value != '\t' && value != '\n' && value != '\r')
        {
            return std::unexpected(failure::schema_mismatch);
        }
        ++tail;
    }
    return output;
}

[[nodiscard]] auto execute(
    ::sqlite3* database,
    std::string_view sql,
    std::stop_token stop) -> result<void>
{
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    auto query = prepare(database, sql);
    if(!query)
    {
        return std::unexpected(query.error());
    }
    if(::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    return {};
}

class transaction final
{
public:
    transaction(::sqlite3* database, std::stop_token stop)
        : Database{database}, Stop{stop}
    {
    }
    transaction(const transaction&) = delete;
    auto operator=(const transaction&) -> transaction& = delete;
    ~transaction()
    {
        if(Active)
        {
            (void)::sqlite3_exec(
                Database, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    [[nodiscard]] auto begin() -> result<void>
    {
        auto opened = execute(Database, "BEGIN IMMEDIATE", Stop);
        Active = opened.has_value();
        return opened;
    }

    [[nodiscard]] auto commit() -> result<void>
    {
        auto committed = execute(Database, "COMMIT", Stop);
        if(committed)
        {
            Active = false;
        }
        return committed;
    }

private:
    ::sqlite3* Database;
    std::stop_token Stop;
    bool Active = false;
};

class read_transaction final
{
public:
    read_transaction(::sqlite3* database, std::stop_token stop)
        : Database{database}, Stop{stop}
    {
    }
    read_transaction(const read_transaction&) = delete;
    auto operator=(const read_transaction&) -> read_transaction& = delete;
    ~read_transaction()
    {
        if(Active)
        {
            (void)::sqlite3_exec(
                Database, "ROLLBACK", nullptr, nullptr, nullptr);
        }
    }

    [[nodiscard]] auto begin() -> result<void>
    {
        auto opened = execute(Database, "BEGIN", Stop);
        Active = opened.has_value();
        return opened;
    }

    [[nodiscard]] auto commit() -> result<void>
    {
        auto committed = execute(Database, "COMMIT", Stop);
        if(committed)
        {
            Active = false;
        }
        return committed;
    }

private:
    ::sqlite3* Database;
    std::stop_token Stop;
    bool Active = false;
};

template<typename Bytes>
[[nodiscard]] auto bind_bytes(
    ::sqlite3_stmt* query, const int index, const Bytes& bytes) noexcept -> bool
{
    return ::sqlite3_bind_blob(
               query,
               index,
               bytes.data(),
               static_cast<int>(bytes.size()),
               SQLITE_TRANSIENT)
        == SQLITE_OK;
}

[[nodiscard]] constexpr auto encode_u64(const std::uint64_t value) noexcept
    -> std::array<std::uint8_t, 8>
{
    std::array<std::uint8_t, 8> output{};
    for(std::size_t index = 0; index < output.size(); ++index)
    {
        output[index] = static_cast<std::uint8_t>(
            value >> ((output.size() - index - 1U) * 8U));
    }
    return output;
}

[[nodiscard]] auto bind_u64(
    ::sqlite3_stmt* query, const int index, const std::uint64_t value) noexcept
    -> bool
{
    const auto encoded = encode_u64(value);
    return bind_bytes(query, index, encoded);
}

template<typename Identity>
[[nodiscard]] auto bind_fixed(
    ::sqlite3_stmt* query, const int index, const Identity& identity) noexcept
    -> bool
{
    return bind_bytes(query, index, identity.bytes());
}

[[nodiscard]] auto bind_operation(
    ::sqlite3_stmt* query,
    const int index,
    const core::epoch_operation_id& operation) noexcept -> bool
{
    return bind_bytes(query, index, operation.bytes());
}

[[nodiscard]] auto bind_oid(
    ::sqlite3_stmt* query,
    const int algorithm_index,
    const int digest_index,
    const core::git_hash_algorithm algorithm,
    const std::array<std::uint8_t, 32>& bytes) noexcept -> bool
{
    const auto count = algorithm == core::git_hash_algorithm::sha1 ? 20U
        : algorithm == core::git_hash_algorithm::sha256             ? 32U
                                                                    : 0U;
    return count != 0U
        && ::sqlite3_bind_int(
               query, algorithm_index, static_cast<int>(algorithm))
            == SQLITE_OK
        && ::sqlite3_bind_blob(
               query,
               digest_index,
               bytes.data(),
               static_cast<int>(count),
               SQLITE_TRANSIENT)
            == SQLITE_OK;
}

template<typename Oid>
[[nodiscard]] auto bind_oid(
    ::sqlite3_stmt* query,
    const int algorithm_index,
    const int digest_index,
    const Oid& oid) noexcept -> bool
{
    return bind_oid(
        query, algorithm_index, digest_index, oid.algorithm(), oid.bytes());
}

[[nodiscard]] auto bind_pack(
    ::sqlite3_stmt* query,
    const int algorithm_index,
    const int digest_index,
    const app::git_pack_oid& pack) noexcept -> bool
{
    return bind_oid(
        query,
        algorithm_index,
        digest_index,
        pack.Algorithm,
        pack.Digest);
}

[[nodiscard]] auto read_blob(
    ::sqlite3_stmt* query,
    const int column,
    std::span<std::uint8_t> output) noexcept -> bool
{
    if(::sqlite3_column_type(query, column) != SQLITE_BLOB
       || ::sqlite3_column_bytes(query, column)
           != static_cast<int>(output.size()))
    {
        return false;
    }
    const auto* value = static_cast<const std::uint8_t*>(
        ::sqlite3_column_blob(query, column));
    if(value == nullptr)
    {
        return false;
    }
    std::memcpy(output.data(), value, output.size());
    return true;
}

[[nodiscard]] auto read_u64(
    ::sqlite3_stmt* query, const int column) noexcept
    -> std::optional<std::uint64_t>
{
    std::array<std::uint8_t, 8> encoded{};
    if(!read_blob(query, column, encoded))
    {
        return std::nullopt;
    }
    std::uint64_t output = 0U;
    for(const auto byte : encoded)
    {
        output = (output << 8U) | byte;
    }
    if(output == 0U)
    {
        return std::nullopt;
    }
    return output;
}

template<typename Identity>
[[nodiscard]] auto read_fixed(::sqlite3_stmt* query, const int column)
    -> std::optional<Identity>
{
    std::array<std::uint8_t, 32> bytes{};
    if(!read_blob(query, column, bytes))
    {
        return std::nullopt;
    }
    Identity output{bytes};
    return output.valid() ? std::optional<Identity>{output} : std::nullopt;
}

template<typename Oid>
[[nodiscard]] auto read_oid(
    ::sqlite3_stmt* query, const int algorithm_column, const int digest_column)
    -> std::optional<Oid>
{
    if(::sqlite3_column_type(query, algorithm_column) != SQLITE_INTEGER)
    {
        return std::nullopt;
    }
    const auto algorithm = static_cast<core::git_hash_algorithm>(
        ::sqlite3_column_int(query, algorithm_column));
    const auto size = algorithm == core::git_hash_algorithm::sha1 ? 20U
        : algorithm == core::git_hash_algorithm::sha256           ? 32U
                                                                  : 0U;
    if(size == 0U || ::sqlite3_column_type(query, digest_column) != SQLITE_BLOB
       || ::sqlite3_column_bytes(query, digest_column)
           != static_cast<int>(size))
    {
        return std::nullopt;
    }
    std::array<std::uint8_t, 32> bytes{};
    const auto* source = static_cast<const std::uint8_t*>(
        ::sqlite3_column_blob(query, digest_column));
    if(source == nullptr)
    {
        return std::nullopt;
    }
    std::memcpy(bytes.data(), source, size);
    Oid output{algorithm, bytes};
    return output.valid() ? std::optional<Oid>{output} : std::nullopt;
}

[[nodiscard]] auto read_pack(
    ::sqlite3_stmt* query, const int algorithm_column, const int digest_column)
    -> std::optional<app::git_pack_oid>
{
    if(::sqlite3_column_type(query, algorithm_column) != SQLITE_INTEGER)
    {
        return std::nullopt;
    }
    const auto algorithm = static_cast<core::git_hash_algorithm>(
        ::sqlite3_column_int(query, algorithm_column));
    const auto size = algorithm == core::git_hash_algorithm::sha1 ? 20U
        : algorithm == core::git_hash_algorithm::sha256           ? 32U
                                                                  : 0U;
    if(size == 0U || ::sqlite3_column_type(query, digest_column) != SQLITE_BLOB
       || ::sqlite3_column_bytes(query, digest_column)
           != static_cast<int>(size))
    {
        return std::nullopt;
    }
    app::git_pack_oid output{.Algorithm = algorithm};
    const auto* source = static_cast<const std::uint8_t*>(
        ::sqlite3_column_blob(query, digest_column));
    if(source == nullptr)
    {
        return std::nullopt;
    }
    std::memcpy(output.Digest.data(), source, size);
    return app::valid(output) ? std::optional{output} : std::nullopt;
}

template<typename Enum>
[[nodiscard]] constexpr auto stored_enum(const Enum value) noexcept -> int
{
    return static_cast<int>(value) + 1;
}

template<typename Enum>
[[nodiscard]] constexpr auto restored_enum(const int value) noexcept -> Enum
{
    return static_cast<Enum>(value - 1);
}

[[nodiscard]] auto map_failure(
    const detail::catalog_database_failure value) noexcept -> failure
{
    using source = detail::catalog_database_failure;
    switch(value)
    {
    case source::invalid_path:
        return failure::invalid_path;
    case source::invalid_permissions:
        return failure::invalid_permissions;
    case source::unavailable:
        return failure::unavailable;
    case source::schema_mismatch:
        return failure::schema_mismatch;
    case source::integrity_failure:
        return failure::integrity_failure;
    case source::quota_exceeded:
        return failure::quota_exceeded;
    case source::cancelled:
        return failure::cancelled;
    case source::platform_unsupported:
        return failure::platform_unsupported;
    }
    return failure::unavailable;
}

struct catalog_state final
{
    explicit catalog_state(detail::catalog_database database)
        : Database{std::move(database)}
    {
    }

    detail::catalog_database Database;
    std::mutex Mutex;
};

[[nodiscard]] auto read_catalog_identity(::sqlite3* database)
    -> result<core::epoch_catalog_incarnation_id>
{
    auto query = prepare(
        database,
        "SELECT incarnation FROM catalog_meta WHERE singleton = 1");
    if(!query || ::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(
            query ? failure::schema_mismatch : query.error());
    }
    auto incarnation = read_fixed<core::epoch_catalog_incarnation_id>(
        query->Value, 0);
    if(!incarnation || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return *incarnation;
}

[[nodiscard]] auto random_identity() -> result<std::array<std::uint8_t, 32>>
{
    std::array<std::uint8_t, 32> bytes{};
    if(!detail::secure_random_bytes(bytes)
       || std::ranges::all_of(bytes, [](const auto byte) {
              return byte == 0U;
          }))
    {
        return std::unexpected(failure::unavailable);
    }
    return bytes;
}

class sqlite_epoch_catalog_session final : public app::epoch_catalog_session
{
public:
    sqlite_epoch_catalog_session(
        std::shared_ptr<catalog_state> state,
        app::epoch_catalog_binding binding,
        core::application_session_id application_session)
        : State{std::move(state)}, Binding{binding},
          ApplicationSession{application_session}
    {
    }

    [[nodiscard]] auto binding() const noexcept
        -> const app::epoch_catalog_binding& override
    {
        return Binding;
    }

    [[nodiscard]] auto snapshot(std::stop_token stop)
        -> result<app::epoch_catalog_snapshot> override;
    [[nodiscard]] auto exact_anchors(
        const app::exact_anchor_lookup_request& request,
        std::stop_token stop) -> result<app::exact_anchor_lookup_result> override;
    [[nodiscard]] auto history(
        const app::epoch_history_page_request& request,
        std::stop_token stop) -> result<app::epoch_history_page> override;
    [[nodiscard]] auto mark_anchor_availability(
        const app::mark_anchor_availability_request& request,
        std::stop_token stop) -> result<app::catalog_commit_result> override;
    [[nodiscard]] auto reserve_append(
        const app::append_reservation_request& request,
        std::stop_token stop) -> result<app::append_operation_record> override;
    [[nodiscard]] auto record_spool(
        const app::record_spool_request& request,
        std::stop_token stop) -> result<app::append_operation_record> override;
    [[nodiscard]] auto record_install(
        const app::record_install_request& request,
        std::stop_token stop) -> result<app::append_operation_record> override;
    [[nodiscard]] auto record_candidate(
        const app::record_candidate_request& request,
        std::stop_token stop) -> result<app::append_operation_record> override;
    [[nodiscard]] auto transition_operation(
        const app::operation_transition_request& request,
        std::stop_token stop) -> result<app::epoch_recovery_operation> override;
    [[nodiscard]] auto finalize_append(
        const app::finalize_append_request& request,
        std::stop_token stop) -> result<app::catalog_commit_result> override;
    [[nodiscard]] auto associate_review(
        const app::associate_review_request& request,
        std::stop_token stop) -> result<app::catalog_commit_result> override;
    [[nodiscard]] auto begin_prune(
        const app::begin_prune_request& request,
        std::stop_token stop) -> result<app::maintenance_operation_record> override;
    [[nodiscard]] auto finalize_prune(
        const app::finalize_prune_request& request,
        std::stop_token stop) -> result<app::catalog_commit_result> override;
    [[nodiscard]] auto begin_cache_delete(
        const app::begin_cache_delete_request& request,
        std::stop_token stop) -> result<app::maintenance_operation_record> override;
    [[nodiscard]] auto cache_delete_impact(
        core::repository_cache_scope_id cache,
        std::stop_token stop) -> result<app::cache_delete_impact> override;
    [[nodiscard]] auto finalize_cache_delete(
        const app::finalize_cache_delete_request& request,
        std::stop_token stop) -> result<void> override;
    [[nodiscard]] auto recovery_page(
        std::optional<core::epoch_operation_id> after,
        std::uint16_t limit,
        std::stop_token stop) -> result<app::epoch_recovery_page> override;

private:
    [[nodiscard]] auto load_append(
        core::epoch_operation_id operation) -> result<app::append_operation_record>;
    [[nodiscard]] auto load_maintenance(
        core::epoch_operation_id operation)
        -> result<app::maintenance_operation_record>;

    std::shared_ptr<catalog_state> State;
    app::epoch_catalog_binding Binding;
    core::application_session_id ApplicationSession;
};

class sqlite_epoch_catalog_port final : public app::epoch_catalog_port
{
public:
    explicit sqlite_epoch_catalog_port(std::shared_ptr<catalog_state> state)
        : State{std::move(state)}
    {
    }

    [[nodiscard]] auto open(
        const app::epoch_catalog_open_request& request,
        std::stop_token stop)
        -> result<std::unique_ptr<app::epoch_catalog_session>> override;

private:
    std::shared_ptr<catalog_state> State;
};

[[nodiscard]] auto bind_authority_key(
    ::sqlite3_stmt* query,
    const app::epoch_catalog_authority_key& key,
    const int first) noexcept -> bool
{
    return bind_u64(query, first, key.Profile.value())
        && bind_u64(query, first + 1, key.ProfileRevision.value())
        && bind_u64(query, first + 2, key.Host.value())
        && bind_fixed(query, first + 3, key.HostAuthority)
        && bind_u64(query, first + 4, key.Account.value())
        && bind_u64(query, first + 5, key.Repository.value())
        && ::sqlite3_bind_int(
               query, first + 6, static_cast<int>(key.ObjectFormat))
            == SQLITE_OK;
}

[[nodiscard]] auto compact_retired_cache_journals(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::repository_cache_scope_id& cache,
    std::stop_token stop) -> result<void>
{
    auto nonterminal = prepare(
        database,
        "SELECT 1 FROM operation WHERE incarnation = ?1 "
        "AND cache_scope = ?2 AND terminal NOT IN (1, 2) LIMIT 1");
    if(!nonterminal || !bind_fixed(nonterminal->Value, 1, incarnation)
       || !bind_fixed(nonterminal->Value, 2, cache))
    {
        return std::unexpected(
            nonterminal ? failure::unavailable : nonterminal.error());
    }
    const auto nonterminal_code = ::sqlite3_step(nonterminal->Value);
    if(nonterminal_code == SQLITE_ROW)
    {
        return std::unexpected(failure::repair_required);
    }
    if(nonterminal_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }

    struct cleanup_statement final
    {
        std::string_view Delete;
        std::string_view Verify;
    };
    constexpr std::array Cleanup{
        cleanup_statement{
            "DELETE FROM append_candidate_review WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_candidate_review d "
            "JOIN operation o ON o.incarnation = d.incarnation "
            "AND o.operation = d.operation WHERE d.incarnation = ?1 "
            "AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_hidden_candidate WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_hidden_candidate d "
            "JOIN operation o ON o.incarnation = d.incarnation "
            "AND o.operation = d.operation WHERE d.incarnation = ?1 "
            "AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_installed_artifact WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_installed_artifact d "
            "JOIN operation o ON o.incarnation = d.incarnation "
            "AND o.operation = d.operation WHERE d.incarnation = ?1 "
            "AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_install WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_install d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_inventory WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_inventory d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_receipt WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_receipt d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_source_review WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_source_review d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM append_operation WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM append_operation d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM prune_operation WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM prune_operation d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
        cleanup_statement{
            "DELETE FROM cache_delete_scope WHERE incarnation = ?1 "
            "AND operation IN (SELECT operation FROM operation "
            "WHERE incarnation = ?1 AND cache_scope = ?2)",
            "SELECT count(*) FROM cache_delete_scope d JOIN operation o "
            "ON o.incarnation = d.incarnation AND o.operation = d.operation "
            "WHERE d.incarnation = ?1 AND o.cache_scope = ?2"},
    };
    for(const auto& cleanup : Cleanup)
    {
        auto removed = prepare(database, cleanup.Delete);
        if(!removed || !bind_fixed(removed->Value, 1, incarnation)
           || !bind_fixed(removed->Value, 2, cache)
           || ::sqlite3_step(removed->Value) != SQLITE_DONE)
        {
            return std::unexpected(
                removed ? sql_failure(database, stop) : removed.error());
        }
        auto remaining = prepare(database, cleanup.Verify);
        if(!remaining || !bind_fixed(remaining->Value, 1, incarnation)
           || !bind_fixed(remaining->Value, 2, cache)
           || ::sqlite3_step(remaining->Value) != SQLITE_ROW
           || ::sqlite3_column_type(remaining->Value, 0) != SQLITE_INTEGER
           || ::sqlite3_column_int64(remaining->Value, 0) != 0
           || ::sqlite3_step(remaining->Value) != SQLITE_DONE)
        {
            return std::unexpected(
                remaining ? failure::repair_required : remaining.error());
        }
    }

    auto removed_operations = prepare(
        database,
        "DELETE FROM operation WHERE incarnation = ?1 "
        "AND cache_scope = ?2 AND terminal IN (1, 2)");
    if(!removed_operations
       || !bind_fixed(removed_operations->Value, 1, incarnation)
       || !bind_fixed(removed_operations->Value, 2, cache)
       || ::sqlite3_step(removed_operations->Value) != SQLITE_DONE)
    {
        return std::unexpected(
            removed_operations ? sql_failure(database, stop)
                               : removed_operations.error());
    }
    auto remaining_operations = prepare(
        database,
        "SELECT count(*) FROM operation WHERE incarnation = ?1 "
        "AND cache_scope = ?2");
    if(!remaining_operations
       || !bind_fixed(remaining_operations->Value, 1, incarnation)
       || !bind_fixed(remaining_operations->Value, 2, cache)
       || ::sqlite3_step(remaining_operations->Value) != SQLITE_ROW
       || ::sqlite3_column_type(remaining_operations->Value, 0)
           != SQLITE_INTEGER
       || ::sqlite3_column_int64(remaining_operations->Value, 0) != 0
       || ::sqlite3_step(remaining_operations->Value) != SQLITE_DONE)
    {
        return std::unexpected(
            remaining_operations ? failure::repair_required
                                 : remaining_operations.error());
    }
    return {};
}

auto sqlite_epoch_catalog_port::open(
    const app::epoch_catalog_open_request& request,
    std::stop_token stop)
    -> result<std::unique_ptr<app::epoch_catalog_session>>
{
    if(!app::valid(request))
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto incarnation = read_catalog_identity(database);
    if(!incarnation)
    {
        return std::unexpected(incarnation.error());
    }

    std::optional<core::repository_cache_scope_id> cache_scope;
    auto repository_lifecycle = 0;
    auto repository = prepare(
        database,
        "SELECT cache_scope, lifecycle FROM repository_scope "
        "WHERE incarnation = ?1 AND profile_id = ?2 "
        "AND profile_revision = ?3 AND host_id = ?4 "
        "AND host_authority = ?5 AND account_id = ?6 "
        "AND repository_id = ?7 AND object_format = ?8 "
        "AND lifecycle != 4");
    if(!repository || !bind_fixed(repository->Value, 1, *incarnation)
       || !bind_authority_key(repository->Value, request.Authority, 2))
    {
        return std::unexpected(
            repository ? failure::unavailable : repository.error());
    }
    auto code = ::sqlite3_step(repository->Value);
    if(code == SQLITE_ROW)
    {
        cache_scope = read_fixed<core::repository_cache_scope_id>(
            repository->Value, 0);
        const auto lifecycle = ::sqlite3_column_int(repository->Value, 1);
        if(!cache_scope || lifecycle < 1 || lifecycle > 3
           || ::sqlite3_step(repository->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::repair_required);
        }
        repository_lifecycle = lifecycle;
    }
    else if(code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }

    if(!cache_scope)
    {
        constexpr std::size_t ScopeAllocationAttempts = 8U;
        for(std::size_t attempt = 0U;
            attempt < ScopeAllocationAttempts && !cache_scope;
            ++attempt)
        {
            auto identity = random_identity();
            if(!identity)
            {
                return std::unexpected(identity.error());
            }
            const auto candidate = core::repository_cache_scope_id{*identity};
            auto insert = prepare(
                database,
                "INSERT INTO repository_scope(incarnation, cache_scope, "
                "profile_id, profile_revision, host_id, host_authority, "
                "account_id, repository_id, object_format, lifecycle) "
                "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1)");
            if(!insert || !bind_fixed(insert->Value, 1, *incarnation)
               || !bind_fixed(insert->Value, 2, candidate)
               || !bind_authority_key(
                   insert->Value, request.Authority, 3))
            {
                return std::unexpected(failure::unavailable);
            }
            const auto insert_code = ::sqlite3_step(insert->Value);
            if(insert_code == SQLITE_DONE)
            {
                cache_scope = candidate;
                repository_lifecycle = 1;
            }
            else if((insert_code & 0xFF) != SQLITE_CONSTRAINT)
            {
                return std::unexpected(sql_failure(database, stop));
            }
        }
        if(!cache_scope)
        {
            return std::unexpected(failure::authority_mismatch);
        }
    }

    if(repository_lifecycle == 1)
    {
        auto active_scope_count = prepare(
            database,
            "SELECT count(*) FROM pull_request_scope "
            "WHERE incarnation = ?1 AND cache_scope = ?2");
        if(!active_scope_count
           || !bind_fixed(
               active_scope_count->Value, 1, *incarnation)
           || !bind_fixed(active_scope_count->Value, 2, *cache_scope)
           || ::sqlite3_step(active_scope_count->Value) != SQLITE_ROW)
        {
            return std::unexpected(
                active_scope_count ? failure::unavailable
                                   : active_scope_count.error());
        }
        const auto active_count =
            ::sqlite3_column_int64(active_scope_count->Value, 0);
        if(active_count < 0
           || ::sqlite3_step(active_scope_count->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        auto retired = prepare(
            database,
            "SELECT p.pr_scope, p.cache_scope, p.viewer_id, "
            "p.pull_request_number, p.lifecycle "
            "FROM repository_scope r JOIN pull_request_scope p "
            "ON p.incarnation = r.incarnation "
            "AND p.cache_scope = r.cache_scope "
            "WHERE r.incarnation = ?1 AND r.profile_id = ?2 "
            "AND r.profile_revision = ?3 AND r.host_id = ?4 "
            "AND r.host_authority = ?5 AND r.account_id = ?6 "
            "AND r.repository_id = ?7 AND r.object_format = ?8 "
            "AND r.lifecycle = 4 AND p.pull_request_id = ?9");
        if(!retired || !bind_fixed(retired->Value, 1, *incarnation)
           || !bind_authority_key(retired->Value, request.Authority, 2)
           || !bind_u64(
               retired->Value, 9, request.Authority.PullRequest.value()))
        {
            return std::unexpected(
                retired ? failure::unavailable : retired.error());
        }
        const auto retired_code = ::sqlite3_step(retired->Value);
        if(retired_code == SQLITE_ROW)
        {
            if(active_count
               >= static_cast<std::int64_t>(
                   app::maximum_cache_delete_scope_count))
            {
                return std::unexpected(failure::quota_exceeded);
            }
            const auto retired_scope =
                read_fixed<core::pull_request_authority_scope_id>(
                    retired->Value, 0);
            const auto retired_cache =
                read_fixed<core::repository_cache_scope_id>(
                    retired->Value, 1);
            const auto viewer = read_u64(retired->Value, 2);
            if(!retired_scope || !retired_cache || !viewer
               || *viewer != request.Authority.Viewer.value()
               || ::sqlite3_column_int64(retired->Value, 3)
                   != request.Authority.Number.value()
               || ::sqlite3_column_int(retired->Value, 4) != 3
               || ::sqlite3_step(retired->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::repair_required);
            }
            if(auto compacted = compact_retired_cache_journals(
                   database, *incarnation, *retired_cache, stop);
               !compacted)
            {
                return std::unexpected(compacted.error());
            }
            auto reactivate = prepare(
                database,
                "UPDATE pull_request_scope SET cache_scope = ?1, "
                "lifecycle = 1, retained_epoch_count = 0, "
                "retention_rehydration_required = CASE "
                "WHEN latest_live_ordinal IS NOT NULL AND "
                "(SELECT count(*) FROM epoch_anchor a WHERE "
                "a.incarnation = pull_request_scope.incarnation AND "
                "a.pr_scope = pull_request_scope.pr_scope AND "
                "a.ordinal = pull_request_scope.latest_live_ordinal AND "
                "a.availability = 4) = 2 THEN 1 ELSE 0 END "
                "WHERE incarnation = ?2 AND pr_scope = ?3 "
                "AND cache_scope = ?4 AND lifecycle = 3");
            if(!reactivate
               || !bind_fixed(reactivate->Value, 1, *cache_scope)
               || !bind_fixed(reactivate->Value, 2, *incarnation)
               || !bind_fixed(reactivate->Value, 3, *retired_scope)
               || !bind_fixed(reactivate->Value, 4, *retired_cache)
               || ::sqlite3_step(reactivate->Value) != SQLITE_DONE
               || ::sqlite3_changes(database) != 1)
            {
                return std::unexpected(failure::repair_required);
            }
        }
        else if(retired_code != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
    }

    std::optional<core::pull_request_authority_scope_id> pr_scope;
    std::int64_t catalog_revision = 0;
    auto pull_request = prepare(
        database,
        "SELECT pr_scope, viewer_id, pull_request_number, "
        "catalog_revision, lifecycle FROM pull_request_scope "
        "WHERE incarnation = ?1 AND cache_scope = ?2 "
        "AND pull_request_id = ?3");
    if(!pull_request || !bind_fixed(pull_request->Value, 1, *incarnation)
       || !bind_fixed(pull_request->Value, 2, *cache_scope)
       || !bind_u64(
           pull_request->Value, 3, request.Authority.PullRequest.value()))
    {
        return std::unexpected(
            pull_request ? failure::unavailable : pull_request.error());
    }
    code = ::sqlite3_step(pull_request->Value);
    if(code == SQLITE_ROW)
    {
        pr_scope = read_fixed<core::pull_request_authority_scope_id>(
            pull_request->Value, 0);
        const auto viewer = read_u64(pull_request->Value, 1);
        catalog_revision = ::sqlite3_column_int64(pull_request->Value, 3);
        if(!pr_scope || !viewer
           || *viewer != request.Authority.Viewer.value()
           || ::sqlite3_column_int64(pull_request->Value, 2)
               != request.Authority.Number.value()
           || catalog_revision < 0
           || (::sqlite3_column_int(pull_request->Value, 4) != 1
               && ::sqlite3_column_int(pull_request->Value, 4) != 3
               && ::sqlite3_column_int(pull_request->Value, 4) != 4)
           || ::sqlite3_step(pull_request->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::authority_mismatch);
        }
    }
    else if(code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }

    if(!pr_scope)
    {
        if(repository_lifecycle != 1)
        {
            return std::unexpected(failure::operation_conflict);
        }
        auto scope_count = prepare(
            database,
            "SELECT count(*) FROM pull_request_scope "
            "WHERE incarnation = ?1 AND cache_scope = ?2");
        if(!scope_count
           || !bind_fixed(scope_count->Value, 1, *incarnation)
           || !bind_fixed(scope_count->Value, 2, *cache_scope)
           || ::sqlite3_step(scope_count->Value) != SQLITE_ROW)
        {
            return std::unexpected(
                scope_count ? failure::unavailable : scope_count.error());
        }
        const auto count = ::sqlite3_column_int64(scope_count->Value, 0);
        if(count < 0
           || ::sqlite3_step(scope_count->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(count
           >= static_cast<std::int64_t>(
               app::maximum_cache_delete_scope_count))
        {
            return std::unexpected(failure::quota_exceeded);
        }
        constexpr std::size_t ScopeAllocationAttempts = 8U;
        for(std::size_t attempt = 0U;
            attempt < ScopeAllocationAttempts && !pr_scope;
            ++attempt)
        {
            auto identity = random_identity();
            if(!identity)
            {
                return std::unexpected(identity.error());
            }
            const auto candidate =
                core::pull_request_authority_scope_id{*identity};
            auto insert = prepare(
                database,
                "INSERT INTO pull_request_scope(incarnation, pr_scope, "
                "cache_scope, viewer_id, pull_request_id, "
                "pull_request_number, next_ordinal, catalog_revision, "
                "retained_epoch_count, "
                "latest_live_ordinal, previous_distinct_head_ordinal, "
                "lifecycle, retention_rehydration_required) "
                "VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1, 0, 0, NULL, NULL, 1, 0)");
            if(!insert || !bind_fixed(insert->Value, 1, *incarnation)
               || !bind_fixed(insert->Value, 2, candidate)
               || !bind_fixed(insert->Value, 3, *cache_scope)
               || !bind_u64(
                   insert->Value, 4, request.Authority.Viewer.value())
               || !bind_u64(
                   insert->Value,
                   5,
                   request.Authority.PullRequest.value())
               || ::sqlite3_bind_int64(
                      insert->Value, 6, request.Authority.Number.value())
                   != SQLITE_OK)
            {
                return std::unexpected(failure::unavailable);
            }
            const auto insert_code = ::sqlite3_step(insert->Value);
            if(insert_code == SQLITE_DONE)
            {
                pr_scope = candidate;
            }
            else if((insert_code & 0xFF) != SQLITE_CONSTRAINT)
            {
                return std::unexpected(sql_failure(database, stop));
            }
        }
        if(!pr_scope)
        {
            return std::unexpected(failure::authority_mismatch);
        }
    }
    auto scope_integrity = prepare(
        database,
        "SELECT p.catalog_revision, p.retained_epoch_count, p.lifecycle, "
        "r.lifecycle, p.retention_rehydration_required, "
        "(SELECT count(*) FROM epoch_anchor a "
        "WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
        "AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
        "FROM pull_request_scope p JOIN repository_scope r "
        "ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
        "WHERE p.incarnation = ?1 AND p.pr_scope = ?2 "
        "AND p.cache_scope = ?3");
    if(!scope_integrity
       || !bind_fixed(scope_integrity->Value, 1, *incarnation)
       || !bind_fixed(scope_integrity->Value, 2, *pr_scope)
       || !bind_fixed(scope_integrity->Value, 3, *cache_scope)
       || ::sqlite3_step(scope_integrity->Value) != SQLITE_ROW)
    {
        return std::unexpected(
            scope_integrity ? failure::repair_required
                            : scope_integrity.error());
    }
    const auto verified_revision =
        ::sqlite3_column_int64(scope_integrity->Value, 0);
    const auto retained = ::sqlite3_column_int64(scope_integrity->Value, 1);
    const auto pr_lifecycle = ::sqlite3_column_int(scope_integrity->Value, 2);
    const auto verified_repository_lifecycle =
        ::sqlite3_column_int(scope_integrity->Value, 3);
    const auto observed_retained =
        ::sqlite3_column_int64(scope_integrity->Value, 5);
    const auto rehydration_required =
        ::sqlite3_column_int(scope_integrity->Value, 4);
    const auto lifecycle_coherent =
        (pr_lifecycle == 1
         && (verified_repository_lifecycle == 1
             || verified_repository_lifecycle == 2))
        || (pr_lifecycle == 4
            && (verified_repository_lifecycle == 1
                || verified_repository_lifecycle == 2))
        || (pr_lifecycle == 3 && verified_repository_lifecycle == 3);
    if(verified_revision != catalog_revision || retained < 0
       || retained != observed_retained
       || retained
           > static_cast<std::int64_t>(
               app::maximum_retained_epoch_count_per_pull_request)
       || verified_repository_lifecycle != repository_lifecycle
       || !lifecycle_coherent
       || (rehydration_required != 0 && rehydration_required != 1)
       || (rehydration_required == 1 && pr_lifecycle != 1)
       || ::sqlite3_step(scope_integrity->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::repair_required);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }

    app::epoch_catalog_binding binding{
        .Authority =
            core::pull_request_authority{
                .Profile = request.Authority.Profile,
                .ProfileRevision = request.Authority.ProfileRevision,
                .Host = request.Authority.Host,
                .HostAuthority = request.Authority.HostAuthority,
                .Account = request.Authority.Account,
                .Viewer = request.Authority.Viewer,
                .Repository = request.Authority.Repository,
                .Cache = *cache_scope,
                .PullRequest = request.Authority.PullRequest,
                .Number = request.Authority.Number,
                .Scope = *pr_scope,
            },
        .Catalog =
            core::epoch_catalog_identity{
                .Incarnation = *incarnation,
                .Revision = core::epoch_catalog_revision{catalog_revision},
            },
        .ObjectFormat = request.Authority.ObjectFormat,
    };
    return std::make_unique<sqlite_epoch_catalog_session>(
        State, binding, request.ApplicationSession);
}

[[nodiscard]] auto load_epoch_summary(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::pull_request_authority_scope_id& authority,
    const core::epoch_ordinal ordinal) -> result<core::review_epoch_summary>
{
    auto query = prepare(
        database,
        "SELECT e.source_observation, e.source_snapshot, "
        "e.identity_generation, e.pull_request_generation, e.base_branch, "
        "e.observed_at, "
        "e.origin, e.base_transition, e.head_transition, "
        "e.material_relation, e.primary_cause, "
        "e.material_base_algorithm, e.material_base_digest, "
        "e.material_head_algorithm, e.material_head_digest, "
        "e.material_policy_revision, "
        "b.object_algorithm, b.object_digest, b.availability, "
        "h.object_algorithm, h.object_digest, h.availability "
        "FROM epoch e "
        "JOIN epoch_anchor b ON b.incarnation = e.incarnation "
        "AND b.pr_scope = e.pr_scope AND b.ordinal = e.ordinal "
        "AND b.endpoint = 1 "
        "JOIN epoch_anchor h ON h.incarnation = e.incarnation "
        "AND h.pr_scope = e.pr_scope AND h.ordinal = e.ordinal "
        "AND h.endpoint = 2 "
        "WHERE e.incarnation = ?1 AND e.pr_scope = ?2 "
        "AND e.ordinal = ?3");
    if(!query || !bind_fixed(query->Value, 1, incarnation)
       || !bind_fixed(query->Value, 2, authority)
       || ::sqlite3_bind_int64(query->Value, 3, ordinal.value()) != SQLITE_OK)
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    if(::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(failure::repair_required);
    }
    const auto observation =
        read_fixed<core::epoch_source_observation_id>(query->Value, 0);
    const auto snapshot = read_u64(query->Value, 1);
    const auto identity_generation = read_u64(query->Value, 2);
    const auto pr_generation = read_u64(query->Value, 3);
    const auto branch = read_fixed<core::base_branch_id>(query->Value, 4);
    const auto base = read_oid<core::git_commit_oid>(query->Value, 16, 17);
    const auto head = read_oid<core::git_commit_oid>(query->Value, 19, 20);
    if(!observation || !snapshot || !identity_generation || !pr_generation
       || !branch || !base || !head
       || ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 6) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 7) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 8) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 9) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 10) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 18) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 21) != SQLITE_INTEGER)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    std::optional<core::epoch_material_identity> material;
    if(::sqlite3_column_type(query->Value, 11) != SQLITE_NULL)
    {
        const auto base_tree =
            read_oid<core::git_tree_oid>(query->Value, 11, 12);
        const auto head_tree =
            read_oid<core::git_tree_oid>(query->Value, 13, 14);
        const auto policy = read_u64(query->Value, 15);
        if(!base_tree || !head_tree || !policy)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        material = core::epoch_material_identity{
            .BaseTree = *base_tree,
            .HeadTree = *head_tree,
            .PolicyRevision = core::comparison_policy_revision{*policy},
        };
    }
    core::review_epoch_summary output{
        .Id =
            core::review_epoch_id{
                .Authority = authority,
                .Incarnation = incarnation,
                .Ordinal = ordinal,
            },
        .Source =
            core::pull_request_source_revision{
                .Observation = *observation,
                .Snapshot = core::provider_snapshot_id{*snapshot},
                .IdentityGeneration =
                    core::github_identity_generation{*identity_generation},
                .PullRequestGeneration =
                    core::request_generation{*pr_generation},
            },
        .BaseBranch = *branch,
        .Base = *base,
        .Head = *head,
        .Material = material,
        .Cause =
            core::epoch_cause{
                .Origin = restored_enum<core::epoch_origin>(
                    ::sqlite3_column_int(query->Value, 6)),
                .Base = restored_enum<core::epoch_base_transition>(
                    ::sqlite3_column_int(query->Value, 7)),
                .Head = restored_enum<core::epoch_head_transition>(
                    ::sqlite3_column_int(query->Value, 8)),
                .Material = restored_enum<core::epoch_material_relation>(
                    ::sqlite3_column_int(query->Value, 9)),
                .Primary = restored_enum<core::epoch_primary_cause>(
                    ::sqlite3_column_int(query->Value, 10)),
            },
        .ObservedAt = core::observed_utc_milliseconds{
            ::sqlite3_column_int64(query->Value, 5)},
        .BaseAvailability = restored_enum<core::anchor_availability>(
            ::sqlite3_column_int(query->Value, 18)),
        .HeadAvailability = restored_enum<core::anchor_availability>(
            ::sqlite3_column_int(query->Value, 21)),
    };
    if(::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return output;
}

[[nodiscard]] auto linked_review_count(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const app::prune_epoch_fact& fact,
    const std::string_view predicate) -> result<std::int64_t>
{
    auto query = prepare(
        database,
        std::string{
            "SELECT count(*) FROM review_association "
            "WHERE incarnation = ?1 AND pr_scope = ?2 "
            "AND linked_ordinal = ?3 AND commit_algorithm = ?4 "
            "AND commit_digest = ?5 AND "}
            + std::string{predicate});
    if(!query || !bind_fixed(query->Value, 1, incarnation)
       || !bind_fixed(query->Value, 2, fact.Epoch.Authority)
       || ::sqlite3_bind_int64(
              query->Value, 3, fact.Epoch.Ordinal.value())
            != SQLITE_OK
       || !bind_oid(query->Value, 4, 5, fact.Retained.Head)
       || ::sqlite3_step(query->Value) != SQLITE_ROW
       || ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER)
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    const auto count = ::sqlite3_column_int64(query->Value, 0);
    if(count < 0 || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return count;
}

struct pull_request_scope_state final
{
    core::epoch_catalog_revision Revision;
    core::repository_cache_scope_id Cache;
    std::uint64_t RetainedEpochCount = 0U;
    int Lifecycle = 0;
    int RepositoryLifecycle = 0;
};

[[nodiscard]] auto load_pull_request_scope_state(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::pull_request_authority_scope_id& authority)
    -> result<pull_request_scope_state>
{
    auto query = prepare(
        database,
        "SELECT p.catalog_revision, p.cache_scope, p.retained_epoch_count, "
        "p.lifecycle, r.lifecycle, (SELECT count(*) FROM epoch_anchor a "
        "WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
        "AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
        "FROM pull_request_scope p JOIN repository_scope r "
        "ON r.incarnation = p.incarnation "
        "AND r.cache_scope = p.cache_scope "
        "WHERE p.incarnation = ?1 AND p.pr_scope = ?2");
    if(!query || !bind_fixed(query->Value, 1, incarnation)
       || !bind_fixed(query->Value, 2, authority))
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    if(::sqlite3_step(query->Value) != SQLITE_ROW
       || ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 4) != SQLITE_INTEGER
       || ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER)
    {
        return std::unexpected(failure::authority_mismatch);
    }
    auto cache = read_fixed<core::repository_cache_scope_id>(query->Value, 1);
    if(!cache)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto retained = ::sqlite3_column_int64(query->Value, 2);
    const auto observed_retained = ::sqlite3_column_int64(query->Value, 5);
    pull_request_scope_state output{
        .Revision = core::epoch_catalog_revision{
            ::sqlite3_column_int64(query->Value, 0)},
        .Cache = *cache,
        .RetainedEpochCount = static_cast<std::uint64_t>(retained),
        .Lifecycle = ::sqlite3_column_int(query->Value, 3),
        .RepositoryLifecycle = ::sqlite3_column_int(query->Value, 4),
    };
    if(retained < 0 || observed_retained < 0
       || retained != observed_retained
       || output.RetainedEpochCount
           > app::maximum_retained_epoch_count_per_pull_request
       || !core::valid(output.Revision) || output.Lifecycle < 1
       || output.Lifecycle > 4 || output.RepositoryLifecycle < 1
       || output.RepositoryLifecycle > 4
       || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return output;
}

[[nodiscard]] auto require_active_pull_request_scope(
    ::sqlite3* database,
    const app::epoch_catalog_binding& binding,
    const core::epoch_catalog_revision expected,
    const bool require_advance_capacity = false,
    const bool require_retained_capacity = false) -> result<void>
{
    auto state = load_pull_request_scope_state(
        database,
        binding.Catalog.Incarnation,
        binding.Authority.Scope);
    if(!state)
    {
        return std::unexpected(state.error());
    }
    if(state->Cache != binding.Authority.Cache || state->Lifecycle != 1
       || state->RepositoryLifecycle != 1)
    {
        return std::unexpected(failure::authority_mismatch);
    }
    if(state->Revision != expected
       || (
           require_advance_capacity
           && state->Revision.value()
               >= core::maximum_epoch_catalog_revision))
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(
        require_retained_capacity
        && state->RetainedEpochCount
            >= app::maximum_retained_epoch_count_per_pull_request)
    {
        return std::unexpected(failure::quota_exceeded);
    }
    return {};
}

[[nodiscard]] auto require_readable_pull_request_scope(
    const pull_request_scope_state& state,
    const app::epoch_catalog_binding& binding,
    const std::optional<core::epoch_catalog_revision> expected = std::nullopt)
    -> result<void>
{
    if(state.Cache != binding.Authority.Cache)
    {
        return std::unexpected(failure::authority_mismatch);
    }
    if(expected && state.Revision != *expected)
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(
        (state.Lifecycle == 1 && state.RepositoryLifecycle == 1)
        || (
            state.Lifecycle == 3
            && (state.RepositoryLifecycle == 3
                || state.RepositoryLifecycle == 4)))
    {
        return {};
    }
    if(state.Lifecycle == 4)
    {
        return std::unexpected(failure::repair_required);
    }
    if(state.RepositoryLifecycle == 2)
    {
        return std::unexpected(failure::operation_conflict);
    }
    return std::unexpected(failure::repair_required);
}

[[nodiscard]] auto require_no_blocking_operation(
    ::sqlite3* database,
    const app::epoch_catalog_binding& binding,
    const bool whole_cache) -> result<void>
{
    auto query = prepare(
        database,
        whole_cache
            ? "SELECT 1 FROM operation WHERE incarnation = ?1 "
              "AND cache_scope = ?2 AND terminal = 0 "
              "LIMIT 1"
            : "SELECT 1 FROM operation WHERE incarnation = ?1 "
              "AND cache_scope = ?2 AND pr_scope = ?3 "
              "AND terminal NOT IN (1, 2) LIMIT 1");
    if(!query || !bind_fixed(query->Value, 1, binding.Catalog.Incarnation)
       || !bind_fixed(query->Value, 2, binding.Authority.Cache)
       || (
           !whole_cache
           && !bind_fixed(query->Value, 3, binding.Authority.Scope)))
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    const auto code = ::sqlite3_step(query->Value);
    if(code == SQLITE_ROW)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    return {};
}

[[nodiscard]] constexpr auto advanced_once(
    const core::epoch_catalog_revision expected,
    const core::epoch_catalog_revision observed) noexcept -> bool
{
    return core::valid(expected) && core::valid(observed)
        && expected.value() < core::maximum_epoch_catalog_revision
        && observed.value() == expected.value() + 1;
}

[[nodiscard]] constexpr auto finalized_operation_revision_matches(
    const app::epoch_catalog_operation_phase phase,
    const app::epoch_catalog_operation_revision expected,
    const app::epoch_catalog_operation_revision observed,
    const app::epoch_catalog_operation_phase pending_phase) noexcept -> bool
{
    if(phase == pending_phase)
    {
        return observed.value() == expected.value() + 1;
    }
    return phase == app::epoch_catalog_operation_phase::complete
        && expected.value()
            < std::numeric_limits<std::int64_t>::max() - 1
        && observed.value() == expected.value() + 2;
}

[[nodiscard]] auto load_review_association(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::pull_request_authority_scope_id& authority,
    const core::github_viewer_actor_id viewer,
    const core::review_association_id& association)
    -> result<core::catalog_review_association>
{
    auto query = prepare(
        database,
        "SELECT review_id, actor_id, review_order, review_state, "
        "commit_algorithm, commit_digest, availability "
        "FROM review_association WHERE incarnation = ?1 "
        "AND pr_scope = ?2 AND association = ?3");
    if(!query || !bind_fixed(query->Value, 1, incarnation)
       || !bind_fixed(query->Value, 2, authority)
       || !bind_fixed(query->Value, 3, association))
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    if(::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(failure::repair_required);
    }
    const auto review = read_u64(query->Value, 0);
    const auto actor = read_u64(query->Value, 1);
    const auto commit = read_oid<core::git_commit_oid>(query->Value, 4, 5);
    if(!review || !actor || *actor != viewer.value() || !commit)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    core::catalog_review_association output{
        .Association = association,
        .Review = core::github_review_id{*review},
        .Actor = core::github_viewer_actor_id{*actor},
        .Order = core::github_review_order{
            static_cast<std::uint16_t>(
                ::sqlite3_column_int(query->Value, 2))},
        .State = restored_enum<core::github_review_state>(
            ::sqlite3_column_int(query->Value, 3)),
        .Commit = *commit,
        .Availability = restored_enum<core::anchor_availability>(
            ::sqlite3_column_int(query->Value, 6)),
    };
    if(::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return output;
}

struct canonical_review_fact final
{
    core::catalog_review_association Review;
    std::optional<core::epoch_ordinal> LinkedOrdinal;
};

[[nodiscard]] auto canonicalize_review(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::pull_request_authority_scope_id& authority,
    const core::catalog_review_association& review,
    const std::optional<std::pair<core::epoch_ordinal, core::git_commit_oid>>&
        projected_head = std::nullopt) -> result<canonical_review_fact>
{
    canonical_review_fact output{.Review = review};
    if(projected_head && review.Commit == projected_head->second)
    {
        output.Review.Availability = core::anchor_availability::available;
        output.LinkedOrdinal = projected_head->first;
        return output;
    }
    auto matching = prepare(
        database,
        "SELECT ordinal, availability FROM epoch_anchor "
        "WHERE incarnation = ?1 AND pr_scope = ?2 AND endpoint = 2 "
        "AND object_algorithm = ?3 AND object_digest = ?4 "
        "ORDER BY ordinal DESC LIMIT 1");
    if(!matching || !bind_fixed(matching->Value, 1, incarnation)
       || !bind_fixed(matching->Value, 2, authority)
       || !bind_oid(matching->Value, 3, 4, review.Commit))
    {
        return std::unexpected(matching ? failure::unavailable
                                        : matching.error());
    }
    const auto code = ::sqlite3_step(matching->Value);
    if(code == SQLITE_ROW)
    {
        output.LinkedOrdinal = core::epoch_ordinal{
            ::sqlite3_column_int64(matching->Value, 0)};
        output.Review.Availability =
            restored_enum<core::anchor_availability>(
                ::sqlite3_column_int(matching->Value, 1));
        if(!core::valid(*output.LinkedOrdinal)
           || ::sqlite3_column_type(matching->Value, 1) != SQLITE_INTEGER
           || ::sqlite3_step(matching->Value) != SQLITE_DONE
           || !app::valid(output.Review))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        return output;
    }
    if(code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    if(review.Availability != core::anchor_availability::unavailable
       && review.Availability
           != core::anchor_availability::never_observed)
    {
        return std::unexpected(failure::operation_conflict);
    }
    return output;
}

auto sqlite_epoch_catalog_session::snapshot(std::stop_token stop)
    -> result<app::epoch_catalog_snapshot>
{
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    read_transaction read{database, stop};
    if(auto begun = read.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto state = load_pull_request_scope_state(
        database,
        Binding.Catalog.Incarnation,
        Binding.Authority.Scope);
    if(!state)
    {
        return std::unexpected(state.error());
    }
    if(auto readable = require_readable_pull_request_scope(
           *state, Binding);
       !readable)
    {
        return std::unexpected(readable.error());
    }
    auto query = prepare(
        database,
        "SELECT catalog_revision, latest_live_ordinal, "
        "previous_distinct_head_ordinal, retention_rehydration_required "
        "FROM pull_request_scope "
        "WHERE incarnation = ?1 AND pr_scope = ?2");
    if(!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(query->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(
            query ? sql_failure(database, stop, failure::authority_mismatch)
                  : query.error());
    }
    const auto revision = ::sqlite3_column_int64(query->Value, 0);
    std::optional<core::epoch_ordinal> latest;
    std::optional<core::epoch_ordinal> previous;
    if(::sqlite3_column_type(query->Value, 1) == SQLITE_INTEGER)
    {
        latest = core::epoch_ordinal{
            ::sqlite3_column_int64(query->Value, 1)};
    }
    if(::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER)
    {
        previous = core::epoch_ordinal{
            ::sqlite3_column_int64(query->Value, 2)};
    }
    const auto rehydration_required = ::sqlite3_column_int(query->Value, 3);
    if(revision < 0
       || (rehydration_required != 0 && rehydration_required != 1)
       || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    auto snapshot_binding = Binding;
    snapshot_binding.Catalog.Revision =
        core::epoch_catalog_revision{revision};
    app::epoch_catalog_snapshot output{
        .Binding = snapshot_binding,
        .RetentionRehydration = rehydration_required != 0
            ? core::retention_rehydration_requirement::required
            : core::retention_rehydration_requirement::not_required,
    };
    if(latest)
    {
        auto loaded = load_epoch_summary(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            *latest);
        if(!loaded)
        {
            return std::unexpected(loaded.error());
        }
        output.LatestLive = *loaded;
    }
    if(rehydration_required != 0
       && (!output.LatestLive
           || output.LatestLive->BaseAvailability
               != core::anchor_availability::cache_deleted
           || output.LatestLive->HeadAvailability
               != core::anchor_availability::cache_deleted))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(previous)
    {
        auto loaded = load_epoch_summary(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            *previous);
        if(!loaded)
        {
            return std::unexpected(loaded.error());
        }
        output.PreviousDistinctHead = *loaded;
    }
    auto current = prepare(
        database,
        "SELECT association FROM current_viewer_review "
        "WHERE incarnation = ?1 AND pr_scope = ?2");
    if(!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(current->Value, 2, Binding.Authority.Scope))
    {
        return std::unexpected(current ? failure::unavailable : current.error());
    }
    const auto code = ::sqlite3_step(current->Value);
    if(code == SQLITE_ROW)
    {
        auto association = read_fixed<core::review_association_id>(
            current->Value, 0);
        if(!association || ::sqlite3_step(current->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        auto loaded = load_review_association(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            Binding.Authority.Viewer,
            *association);
        if(!loaded)
        {
            return std::unexpected(loaded.error());
        }
        output.CurrentViewerReview = *loaded;
    }
    else if(code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    if(auto committed = read.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    Binding = snapshot_binding;
    return output;
}

auto sqlite_epoch_catalog_session::exact_anchors(
    const app::exact_anchor_lookup_request& request,
    std::stop_token stop) -> result<app::exact_anchor_lookup_result>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    read_transaction read{database, stop};
    if(auto begun = read.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto scope = load_pull_request_scope_state(
        database,
        Binding.Catalog.Incarnation,
        Binding.Authority.Scope);
    if(!scope)
    {
        return std::unexpected(scope.error());
    }
    if(auto readable = require_readable_pull_request_scope(
           *scope, Binding, request.Catalog.Revision);
       !readable)
    {
        return std::unexpected(readable.error());
    }
    app::exact_anchor_lookup_result output{.Catalog = request.Catalog};
    output.Anchors.reserve(request.Anchors.size());
    for(const auto& identity : request.Anchors)
    {
        if(identity.Authority != Binding.Authority.Scope)
        {
            return std::unexpected(failure::authority_mismatch);
        }
        core::comparison_anchor anchor{.Identity = identity};
        if(identity.Kind == core::comparison_anchor_kind::review_commit)
        {
            auto query = prepare(
                database,
                "SELECT actor_id, commit_algorithm, commit_digest, "
                "availability "
                "FROM review_association WHERE incarnation = ?1 "
                "AND pr_scope = ?2 AND association = ?3");
            if(!query
               || !bind_fixed(
                   query->Value, 1, Binding.Catalog.Incarnation)
               || !bind_fixed(query->Value, 2, Binding.Authority.Scope)
               || !bind_fixed(query->Value, 3, identity.Review)
               || ::sqlite3_step(query->Value) != SQLITE_ROW)
            {
                return std::unexpected(failure::authority_mismatch);
            }
            const auto actor = read_u64(query->Value, 0);
            auto commit =
                read_oid<core::git_commit_oid>(query->Value, 1, 2);
            const auto availability = ::sqlite3_column_int(query->Value, 3);
            if(!actor || *actor != Binding.Authority.Viewer.value() || !commit
               || ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
               || ::sqlite3_step(query->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            anchor.Commit = *commit;
            anchor.Availability = restored_enum<core::anchor_availability>(
                availability);
        }
        else
        {
            auto query = prepare(
                database,
                "SELECT object_algorithm, object_digest, availability "
                "FROM epoch_anchor WHERE incarnation = ?1 "
                "AND pr_scope = ?2 AND ordinal = ?3 AND endpoint = ?4");
            const auto endpoint =
                identity.Kind == core::comparison_anchor_kind::epoch_base
                ? 1
                : 2;
            if(!query
               || !bind_fixed(
                   query->Value, 1, Binding.Catalog.Incarnation)
               || !bind_fixed(query->Value, 2, Binding.Authority.Scope)
               || ::sqlite3_bind_int64(
                      query->Value, 3, identity.Epoch.value())
                   != SQLITE_OK
               || ::sqlite3_bind_int(query->Value, 4, endpoint) != SQLITE_OK
               || ::sqlite3_step(query->Value) != SQLITE_ROW)
            {
                return std::unexpected(failure::authority_mismatch);
            }
            auto commit =
                read_oid<core::git_commit_oid>(query->Value, 0, 1);
            const auto availability = ::sqlite3_column_int(query->Value, 2);
            if(!commit
               || ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
               || ::sqlite3_step(query->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            anchor.Commit = *commit;
            anchor.Availability = restored_enum<core::anchor_availability>(
                availability);
        }
        output.Anchors.push_back(anchor);
    }
    if(auto committed = read.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::mark_anchor_availability(
    const app::mark_anchor_availability_request& request,
    std::stop_token stop) -> result<app::catalog_commit_result>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision, true);
       !active)
    {
        return std::unexpected(active.error());
    }
    if(auto idle = require_no_blocking_operation(database, Binding, false);
       !idle)
    {
        return std::unexpected(idle.error());
    }
    for(const auto& change : request.Changes)
    {
        if(change.Anchor.Authority != Binding.Authority.Scope
           || change.ExpectedCommit.algorithm() != Binding.ObjectFormat)
        {
            return std::unexpected(failure::authority_mismatch);
        }
        result<statement> update = std::unexpected(failure::unavailable);
        if(change.Anchor.Kind == core::comparison_anchor_kind::review_commit)
        {
            update = prepare(
                database,
                "UPDATE review_association SET availability = ?1 "
                "WHERE incarnation = ?2 AND pr_scope = ?3 "
                "AND association = ?4 AND commit_algorithm = ?5 "
                "AND commit_digest = ?6 AND availability = ?7");
            if(update
               && (::sqlite3_bind_int(
                       update->Value, 1, stored_enum(change.Next))
                       != SQLITE_OK
                   || !bind_fixed(
                       update->Value, 2, Binding.Catalog.Incarnation)
                   || !bind_fixed(
                       update->Value, 3, Binding.Authority.Scope)
                   || !bind_fixed(update->Value, 4, change.Anchor.Review)
                   || !bind_oid(
                       update->Value, 5, 6, change.ExpectedCommit)
                   || ::sqlite3_bind_int(
                          update->Value, 7, stored_enum(change.Expected))
                       != SQLITE_OK))
            {
                return std::unexpected(failure::unavailable);
            }
        }
        else
        {
            update = prepare(
                database,
                "UPDATE epoch_anchor SET availability = ?1 "
                "WHERE incarnation = ?2 AND pr_scope = ?3 "
                "AND ordinal = ?4 AND endpoint = ?5 "
                "AND object_algorithm = ?6 AND object_digest = ?7 "
                "AND availability = ?8");
            const auto endpoint =
                change.Anchor.Kind
                    == core::comparison_anchor_kind::epoch_base
                ? 1
                : 2;
            if(update
               && (::sqlite3_bind_int(
                       update->Value, 1, stored_enum(change.Next))
                       != SQLITE_OK
                   || !bind_fixed(
                       update->Value, 2, Binding.Catalog.Incarnation)
                   || !bind_fixed(
                       update->Value, 3, Binding.Authority.Scope)
                   || ::sqlite3_bind_int64(
                          update->Value, 4, change.Anchor.Epoch.value())
                       != SQLITE_OK
                   || ::sqlite3_bind_int(update->Value, 5, endpoint)
                       != SQLITE_OK
                   || !bind_oid(
                       update->Value, 6, 7, change.ExpectedCommit)
                   || ::sqlite3_bind_int(
                          update->Value, 8, stored_enum(change.Expected))
                       != SQLITE_OK))
            {
                return std::unexpected(failure::unavailable);
            }
        }
        if(!update || ::sqlite3_step(update->Value) != SQLITE_DONE
           || ::sqlite3_changes(database) != 1)
        {
            return std::unexpected(failure::revision_conflict);
        }
    }
    auto revision = prepare(
        database,
        "UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND catalog_revision = ?3 "
        "AND catalog_revision < 9223372036854775807");
    if(!revision
       || !bind_fixed(revision->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(revision->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              revision->Value, 3, request.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(revision->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    Binding.Catalog.Revision = core::epoch_catalog_revision{
        request.Catalog.Revision.value() + 1};
    return app::catalog_commit_result{
        .Catalog = Binding.Catalog,
        .Changed = true,
    };
}

auto sqlite_epoch_catalog_session::history(
    const app::epoch_history_page_request& request,
    std::stop_token stop) -> result<app::epoch_history_page>
{
    if(!app::valid(Binding, request))
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    read_transaction read{database, stop};
    if(auto begun = read.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto state = load_pull_request_scope_state(
        database,
        Binding.Catalog.Incarnation,
        Binding.Authority.Scope);
    if(!state)
    {
        return std::unexpected(state.error());
    }
    if(auto readable = require_readable_pull_request_scope(
           *state, Binding, request.Catalog.Revision);
       !readable)
    {
        return std::unexpected(readable.error());
    }
    auto ordinals = prepare(
        database,
        request.Cursor
            ? "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
              "AND pr_scope = ?2 AND ordinal < ?3 "
              "ORDER BY ordinal DESC LIMIT ?4"
            : "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
              "AND pr_scope = ?2 ORDER BY ordinal DESC LIMIT ?3");
    if(!ordinals
       || !bind_fixed(ordinals->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(ordinals->Value, 2, Binding.Authority.Scope))
    {
        return std::unexpected(ordinals ? failure::unavailable : ordinals.error());
    }
    const auto fetch_limit = static_cast<std::int64_t>(request.Limit) + 1;
    if(request.Cursor)
    {
        if(::sqlite3_bind_int64(
               ordinals->Value, 3, request.Cursor->Before.value())
               != SQLITE_OK
           || ::sqlite3_bind_int64(ordinals->Value, 4, fetch_limit)
               != SQLITE_OK)
        {
            return std::unexpected(failure::unavailable);
        }
    }
    else if(::sqlite3_bind_int64(ordinals->Value, 3, fetch_limit) != SQLITE_OK)
    {
        return std::unexpected(failure::unavailable);
    }
    std::vector<core::epoch_ordinal> values;
    while(true)
    {
        const auto code = ::sqlite3_step(ordinals->Value);
        if(code == SQLITE_DONE)
        {
            break;
        }
        if(code != SQLITE_ROW || stop.stop_requested())
        {
            return std::unexpected(sql_failure(database, stop));
        }
        values.emplace_back(::sqlite3_column_int64(ordinals->Value, 0));
    }
    const auto has_more = values.size() > request.Limit;
    if(has_more)
    {
        values.resize(request.Limit);
    }
    app::epoch_history_page output{
        .Authority = Binding.Authority.Scope,
        .Catalog = request.Catalog,
    };
    output.Epochs.reserve(values.size());
    for(const auto ordinal : values)
    {
        auto loaded = load_epoch_summary(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            ordinal);
        if(!loaded)
        {
            return std::unexpected(loaded.error());
        }
        output.Epochs.push_back(*loaded);
    }
    if(has_more && !values.empty())
    {
        output.Continuation = app::epoch_history_cursor{
            .Authority = Binding.Authority.Scope,
            .Incarnation = Binding.Catalog.Incarnation,
            .Revision = request.Catalog.Revision,
            .Before = values.back(),
        };
    }
    if(auto committed = read.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

[[nodiscard]] constexpr auto coherent_candidate_review(
    const std::optional<core::catalog_review_association>& candidate,
    const std::optional<core::current_viewer_review_candidate>& source,
    core::github_viewer_actor_id viewer) noexcept -> bool;

[[nodiscard]] auto coherent_install_facts(
    const app::durable_git_spool_fact& spool,
    const app::durable_git_preimport_fact& preimport,
    const app::durable_git_install_fact& install) noexcept -> bool
{
    if(install.Pack != spool.ExpectedPack
       || install.InstalledPack.Bytes != spool.Spool.Bytes)
    {
        return false;
    }
    if(!preimport.ExpectedPackPreexisting)
    {
        return true;
    }
    const auto expected = std::ranges::find_if(
        preimport.Packs, [&spool](const auto& entry) {
            return entry.Pack == spool.ExpectedPack;
        });
    return expected != preimport.Packs.end()
        && install.InstalledPack.Bytes == expected->PackBytes
        && install.InstalledIndex.Bytes == expected->IndexBytes;
}

auto sqlite_epoch_catalog_session::load_append(
    const core::epoch_operation_id operation)
    -> result<app::append_operation_record>
{
    auto* database = State->Database.handle();
    auto query = prepare(
        database,
        "SELECT o.phase, o.operation_revision, o.repair_from_phase, "
        "o.recovery_input_compacted, o.application_session, "
        "o.request_id, o.epoch_generation, o.source_observation, "
        "o.source_snapshot, o.identity_generation, "
        "o.pull_request_generation, "
        "o.expected_revision, o.reserved_ordinal, "
        "a.base_branch, a.revision_origin, a.base_algorithm, a.base_digest, "
        "a.head_algorithm, a.head_digest, a.source_review_present "
        "FROM operation o JOIN append_operation a "
        "ON a.incarnation = o.incarnation AND a.operation = o.operation "
        "WHERE o.incarnation = ?1 AND o.operation = ?2 AND o.kind = 1 "
        "AND o.cache_scope = ?3 AND o.pr_scope = ?4");
    if(!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(query->Value, 2, operation)
       || !bind_fixed(query->Value, 3, Binding.Authority.Cache)
       || !bind_fixed(query->Value, 4, Binding.Authority.Scope)
       || ::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(
            query ? failure::operation_conflict : query.error());
    }
    const auto application_session = read_u64(query->Value, 4);
    const auto request = read_u64(query->Value, 5);
    const auto generation = read_u64(query->Value, 6);
    const auto observation =
        read_fixed<core::epoch_source_observation_id>(query->Value, 7);
    const auto source = read_u64(query->Value, 8);
    const auto identity_generation = read_u64(query->Value, 9);
    const auto pr_generation = read_u64(query->Value, 10);
    const auto branch = read_fixed<core::base_branch_id>(query->Value, 13);
    const auto base = read_oid<core::git_commit_oid>(query->Value, 15, 16);
    const auto head = read_oid<core::git_commit_oid>(query->Value, 17, 18);
    if(!application_session || !request || !generation || !observation
       || !source || !identity_generation || !pr_generation || !branch
       || !base || !head)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    core::accepted_pull_request_revision accepted{
        .Source =
            core::pull_request_source_revision{
                .Observation = *observation,
                .Snapshot = core::provider_snapshot_id{*source},
                .IdentityGeneration =
                    core::github_identity_generation{*identity_generation},
                .PullRequestGeneration =
                    core::request_generation{*pr_generation},
            },
        .Origin = restored_enum<core::pull_request_revision_origin>(
            ::sqlite3_column_int(query->Value, 14)),
        .BaseBranch = *branch,
        .Base = *base,
        .Head = *head,
    };
    const auto phase = static_cast<app::epoch_catalog_operation_phase>(
        ::sqlite3_column_int(query->Value, 0));
    const auto operation_revision = app::epoch_catalog_operation_revision{
        ::sqlite3_column_int64(query->Value, 1)};
    std::optional<app::epoch_catalog_operation_phase> repair_from_phase;
    if(::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER)
    {
        repair_from_phase =
            static_cast<app::epoch_catalog_operation_phase>(
                ::sqlite3_column_int(query->Value, 2));
    }
    else if(::sqlite3_column_type(query->Value, 2) != SQLITE_NULL)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto recovery_input_compacted =
        ::sqlite3_column_int(query->Value, 3);
    const auto catalog_revision = ::sqlite3_column_int64(query->Value, 11);
    const auto ordinal = core::epoch_ordinal{
        ::sqlite3_column_int64(query->Value, 12)};
    const auto source_review_present = ::sqlite3_column_int(query->Value, 19);
    if(!app::valid(operation_revision) || catalog_revision < 0
       || !core::valid(ordinal)
       || (recovery_input_compacted != 0 && recovery_input_compacted != 1)
       || (source_review_present != 0 && source_review_present != 1)
       || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    auto source_review = prepare(
        database,
        "SELECT review_id, actor_id, review_order, review_state, "
        "commit_algorithm, commit_digest FROM append_source_review "
        "WHERE incarnation = ?1 AND operation = ?2");
    if(!source_review
       || !bind_fixed(
           source_review->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(source_review->Value, 2, operation))
    {
        return std::unexpected(
            source_review ? failure::unavailable : source_review.error());
    }
    auto source_review_code = ::sqlite3_step(source_review->Value);
    if((source_review_code == SQLITE_ROW) != (source_review_present != 0))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(source_review_code == SQLITE_ROW)
    {
        const auto review = read_u64(source_review->Value, 0);
        const auto actor = read_u64(source_review->Value, 1);
        const auto commit = read_oid<core::git_commit_oid>(
            source_review->Value, 4, 5);
        const auto order = ::sqlite3_column_int(source_review->Value, 2);
        const auto state = ::sqlite3_column_int(source_review->Value, 3);
        if(!review || !actor
           || *actor != Binding.Authority.Viewer.value() || !commit
           || ::sqlite3_step(source_review->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        accepted.CurrentViewerReview = core::current_viewer_review_candidate{
            .Review = core::github_review_id{*review},
            .Actor = core::github_viewer_actor_id{*actor},
            .Order = core::github_review_order{
                static_cast<std::uint16_t>(order)},
            .State = restored_enum<core::github_review_state>(
                state),
            .Commit = *commit,
        };
    }
    else if(source_review_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    app::append_operation_record output{
        .Context =
            app::append_operation_context{
                .ApplicationSession =
                    core::application_session_id{*application_session},
                .Request = core::epoch_request_id{*request},
                .Generation = core::epoch_generation{*generation},
                .Operation = operation,
                .Revision = accepted,
                .Catalog =
                    core::epoch_catalog_identity{
                        .Incarnation = Binding.Catalog.Incarnation,
                        .Revision =
                            core::epoch_catalog_revision{catalog_revision},
                    },
            },
        .ReservedOrdinal = ordinal,
        .OperationRevision = operation_revision,
        .Phase = phase,
        .RepairFromPhase = repair_from_phase,
        .RecoveryInputCompacted = recovery_input_compacted != 0,
        .StagingBase =
            app::epoch_catalog_ref_identity{
                .Authority = Binding.Authority.Scope,
                .Ordinal = ordinal,
                .Operation = operation,
                .Staging = true,
                .Head = false,
            },
        .StagingHead =
            app::epoch_catalog_ref_identity{
                .Authority = Binding.Authority.Scope,
                .Ordinal = ordinal,
                .Operation = operation,
                .Staging = true,
                .Head = true,
            },
        .FinalBase =
            app::epoch_catalog_ref_identity{
                .Authority = Binding.Authority.Scope,
                .Ordinal = ordinal,
            },
        .FinalHead =
            app::epoch_catalog_ref_identity{
                .Authority = Binding.Authority.Scope,
                .Ordinal = ordinal,
                .Head = true,
            },
    };
    auto receipt = prepare(
        database,
        "SELECT spool_identity, spool_bytes, expected_pack_algorithm, "
        "expected_pack_digest, observed_pack_algorithm, "
        "observed_pack_digest, repository_bytes, "
        "expected_pack_preexisting, inventory_count FROM append_receipt "
        "WHERE incarnation = ?1 AND operation = ?2");
    if(!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(receipt->Value, 2, operation))
    {
        return std::unexpected(receipt ? failure::unavailable : receipt.error());
    }
    auto receipt_code = ::sqlite3_step(receipt->Value);
    if(receipt_code == SQLITE_ROW)
    {
        std::array<std::uint8_t, 32> spool_identity{};
        const auto expected = read_pack(receipt->Value, 2, 3);
        const auto observed = read_pack(receipt->Value, 4, 5);
        if(!read_blob(receipt->Value, 0, spool_identity) || !expected
           || !observed)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        output.Spool = app::durable_git_spool_fact{
            .Operation = operation,
            .ExpectedPack = *expected,
            .ObservedPack = *observed,
            .Spool =
                app::git_artifact_fact{
                    .Role = app::git_artifact_role::completed_spool,
                    .Identity = app::git_artifact_identity{spool_identity},
                    .Bytes = static_cast<std::uint64_t>(
                        ::sqlite3_column_int64(receipt->Value, 1)),
                },
        };
        app::durable_git_preimport_fact preimport{
            .Operation = operation,
            .ExpectedPack = *expected,
            .RepositoryBytes = static_cast<std::uint64_t>(
                ::sqlite3_column_int64(receipt->Value, 6)),
            .ExpectedPackPreexisting =
                ::sqlite3_column_int(receipt->Value, 7) != 0,
        };
        const auto expected_inventory_count =
            ::sqlite3_column_int64(receipt->Value, 8);
        if(expected_inventory_count < 0
           || expected_inventory_count
               > static_cast<std::int64_t>(
                   app::maximum_git_cache_pack_count))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(::sqlite3_step(receipt->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        auto inventory = prepare(
            database,
            "SELECT position, pack_algorithm, pack_digest, pack_bytes, "
            "index_bytes "
            "FROM append_inventory WHERE incarnation = ?1 "
            "AND operation = ?2 ORDER BY position LIMIT 4097");
        if(!inventory
           || !bind_fixed(
               inventory->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(inventory->Value, 2, operation))
        {
            return std::unexpected(
                inventory ? failure::unavailable : inventory.error());
        }
        while(true)
        {
            const auto inventory_code = ::sqlite3_step(inventory->Value);
            if(inventory_code == SQLITE_DONE)
            {
                break;
            }
            if(inventory_code != SQLITE_ROW)
            {
                return std::unexpected(sql_failure(database, {}));
            }
            const auto position = ::sqlite3_column_int64(inventory->Value, 0);
            const auto pack = read_pack(inventory->Value, 1, 2);
            if(!pack
               || position
                   != static_cast<std::int64_t>(preimport.Packs.size()))
            {
                return std::unexpected(failure::schema_mismatch);
            }
            if(preimport.Packs.size() >= app::maximum_git_cache_pack_count)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            preimport.Packs.push_back(app::git_pack_inventory_entry{
                .Pack = *pack,
                .PackBytes = static_cast<std::uint64_t>(
                    ::sqlite3_column_int64(inventory->Value, 3)),
                .IndexBytes = static_cast<std::uint64_t>(
                    ::sqlite3_column_int64(inventory->Value, 4)),
            });
        }
        if(preimport.Packs.size()
           != static_cast<std::size_t>(expected_inventory_count))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        output.Preimport = std::move(preimport);
    }
    else if(receipt_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    auto install = prepare(
        database,
        "SELECT i.pack_algorithm, i.pack_digest, i.base_algorithm, "
        "i.base_digest, i.base_tree_algorithm, i.base_tree_digest, "
        "i.head_algorithm, i.head_digest, i.head_tree_algorithm, "
        "i.head_tree_digest, p.artifact_identity, p.artifact_bytes, "
        "x.artifact_identity, x.artifact_bytes "
        "FROM append_install i "
        "JOIN append_installed_artifact p ON p.incarnation = i.incarnation "
        "AND p.operation = i.operation AND p.artifact_role = 2 "
        "JOIN append_installed_artifact x ON x.incarnation = i.incarnation "
        "AND x.operation = i.operation AND x.artifact_role = 3 "
        "WHERE i.incarnation = ?1 AND i.operation = ?2");
    if(!install || !bind_fixed(install->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(install->Value, 2, operation))
    {
        return std::unexpected(install ? failure::unavailable : install.error());
    }
    const auto install_code = ::sqlite3_step(install->Value);
    if(install_code == SQLITE_ROW)
    {
        const auto pack = read_pack(install->Value, 0, 1);
        const auto install_base =
            read_oid<core::git_commit_oid>(install->Value, 2, 3);
        const auto base_tree =
            read_oid<core::git_tree_oid>(install->Value, 4, 5);
        const auto install_head =
            read_oid<core::git_commit_oid>(install->Value, 6, 7);
        const auto head_tree =
            read_oid<core::git_tree_oid>(install->Value, 8, 9);
        std::array<std::uint8_t, 32> pack_identity{};
        std::array<std::uint8_t, 32> index_identity{};
        const auto pack_bytes = ::sqlite3_column_int64(install->Value, 11);
        const auto index_bytes = ::sqlite3_column_int64(install->Value, 13);
        if(!pack || !install_base || !base_tree || !install_head || !head_tree
           || !read_blob(install->Value, 10, pack_identity)
           || !read_blob(install->Value, 12, index_identity)
           || ::sqlite3_step(install->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        output.Install = app::durable_git_install_fact{
            .Operation = operation,
            .Pack = *pack,
            .InstalledPack =
                app::git_artifact_fact{
                    .Role = app::git_artifact_role::installed_pack,
                    .Identity = app::git_artifact_identity{pack_identity},
                    .Bytes = static_cast<std::uint64_t>(pack_bytes),
                },
            .InstalledIndex =
                app::git_artifact_fact{
                    .Role = app::git_artifact_role::installed_index,
                    .Identity = app::git_artifact_identity{index_identity},
                    .Bytes = static_cast<std::uint64_t>(index_bytes),
                },
            .Base = *install_base,
            .BaseTree = *base_tree,
            .Head = *install_head,
            .HeadTree = *head_tree,
        };
    }
    else if(install_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    auto candidate = prepare(
        database,
        "SELECT material_base_algorithm, material_base_digest, "
        "material_head_algorithm, material_head_digest, "
        "material_policy_revision, observed_at, origin, base_transition, "
        "head_transition, material_relation, primary_cause, "
        "base_availability, head_availability, candidate_review_present "
        "FROM append_hidden_candidate WHERE incarnation = ?1 "
        "AND operation = ?2");
    if(!candidate
       || !bind_fixed(candidate->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(candidate->Value, 2, operation))
    {
        return std::unexpected(candidate ? failure::unavailable : candidate.error());
    }
    const auto candidate_code = ::sqlite3_step(candidate->Value);
    if(candidate_code == SQLITE_ROW)
    {
        const auto candidate_review_present =
            ::sqlite3_column_int(candidate->Value, 13);
        if(candidate_review_present != 0 && candidate_review_present != 1)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        std::optional<core::epoch_material_identity> material;
        if(::sqlite3_column_type(candidate->Value, 0) != SQLITE_NULL)
        {
            const auto base_tree =
                read_oid<core::git_tree_oid>(candidate->Value, 0, 1);
            const auto head_tree =
                read_oid<core::git_tree_oid>(candidate->Value, 2, 3);
            const auto policy = read_u64(candidate->Value, 4);
            if(!base_tree || !head_tree || !policy)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            material = core::epoch_material_identity{
                .BaseTree = *base_tree,
                .HeadTree = *head_tree,
                .PolicyRevision = core::comparison_policy_revision{*policy},
            };
        }
        core::append_epoch_mutation mutation{
            .Epoch =
                core::review_epoch_summary{
                    .Id =
                        core::review_epoch_id{
                            .Authority = Binding.Authority.Scope,
                            .Incarnation = Binding.Catalog.Incarnation,
                            .Ordinal = ordinal,
                        },
                    .Source = accepted.Source,
                    .BaseBranch = accepted.BaseBranch,
                    .Base = accepted.Base,
                    .Head = accepted.Head,
                    .Material = material,
                    .Cause =
                        core::epoch_cause{
                            .Origin = restored_enum<core::epoch_origin>(
                                ::sqlite3_column_int(candidate->Value, 6)),
                            .Base = restored_enum<core::epoch_base_transition>(
                                ::sqlite3_column_int(candidate->Value, 7)),
                            .Head = restored_enum<core::epoch_head_transition>(
                                ::sqlite3_column_int(candidate->Value, 8)),
                            .Material = restored_enum<core::epoch_material_relation>(
                                ::sqlite3_column_int(candidate->Value, 9)),
                            .Primary = restored_enum<core::epoch_primary_cause>(
                                ::sqlite3_column_int(candidate->Value, 10)),
                        },
                    .ObservedAt = core::observed_utc_milliseconds{
                        ::sqlite3_column_int64(candidate->Value, 5)},
                    .BaseAvailability = restored_enum<core::anchor_availability>(
                        ::sqlite3_column_int(candidate->Value, 11)),
                    .HeadAvailability = restored_enum<core::anchor_availability>(
                        ::sqlite3_column_int(candidate->Value, 12)),
                },
        };
        if(::sqlite3_step(candidate->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        auto review = prepare(
            database,
            "SELECT association, review_id, actor_id, review_order, "
            "review_state, commit_algorithm, commit_digest, availability "
            "FROM append_candidate_review WHERE incarnation = ?1 "
            "AND operation = ?2");
        if(!review
           || !bind_fixed(review->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(review->Value, 2, operation))
        {
            return std::unexpected(review ? failure::unavailable : review.error());
        }
        const auto review_code = ::sqlite3_step(review->Value);
        if((review_code == SQLITE_ROW) != (candidate_review_present != 0))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(review_code == SQLITE_ROW)
        {
            const auto association =
                read_fixed<core::review_association_id>(review->Value, 0);
            const auto review_id = read_u64(review->Value, 1);
            const auto actor = read_u64(review->Value, 2);
            const auto commit =
                read_oid<core::git_commit_oid>(review->Value, 5, 6);
            const auto order = ::sqlite3_column_int(review->Value, 3);
            const auto state = ::sqlite3_column_int(review->Value, 4);
            const auto availability = ::sqlite3_column_int(review->Value, 7);
            if(!association || !review_id || !actor || !commit
               || ::sqlite3_step(review->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            mutation.Review = core::catalog_review_association{
                .Association = *association,
                .Review = core::github_review_id{*review_id},
                .Actor = core::github_viewer_actor_id{*actor},
                .Order = core::github_review_order{
                    static_cast<std::uint16_t>(order)},
                .State = restored_enum<core::github_review_state>(
                    state),
                .Commit = *commit,
                .Availability = restored_enum<core::anchor_availability>(
                    availability),
            };
        }
        else if(review_code != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
        output.Candidate = std::move(mutation);
    }
    else if(candidate_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    const auto exact_spool_pair = output.Spool && output.Preimport;
    const auto no_spool_pair = !output.Spool && !output.Preimport;
    const auto coherent_spool_pair = exact_spool_pair || no_spool_pair;
    const auto exact_install = output.Install.has_value();
    const auto exact_candidate = output.Candidate.has_value();
    using enum app::epoch_catalog_operation_phase;
    const auto fact_phase = output.RepairFromPhase.value_or(output.Phase);
    const auto coherent_fact_prefix = coherent_spool_pair
        && (
            (no_spool_pair && !exact_install && !exact_candidate)
            || (exact_spool_pair && !exact_install && !exact_candidate)
            || (exact_spool_pair && exact_install && !exact_candidate)
            || (exact_spool_pair && exact_install && exact_candidate));
    const auto complete_facts_valid = output.Phase == complete
        && output.RecoveryInputCompacted && no_spool_pair && exact_install
        && exact_candidate;
    const auto aborted_facts_valid = output.Phase == aborted
        && output.RecoveryInputCompacted && no_spool_pair
        && (!exact_candidate || exact_install);
    const auto recoverable_facts_valid = !output.RecoveryInputCompacted
        && output.Phase != complete && output.Phase != aborted
        && ((fact_phase == preparing
         && no_spool_pair && !exact_install && !exact_candidate)
        || (
            fact_phase == spool_recorded && exact_spool_pair
            && !exact_install && !exact_candidate)
        || (
            (fact_phase == imported || fact_phase == staging_ready)
            && exact_spool_pair && exact_install && !exact_candidate)
        || (
            (fact_phase == commit_authorized
             || fact_phase == ref_commit_in_progress
             || fact_phase == refs_committed
             || fact_phase == visible_cleanup_pending)
            && exact_spool_pair && exact_install && exact_candidate)
        || (
            fact_phase == abort_requested && coherent_fact_prefix));
    const auto phase_facts_valid = complete_facts_valid
        || aborted_facts_valid || recoverable_facts_valid;
    if(!phase_facts_valid)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(
        output.Install
        && (
            output.Install->Pack.Algorithm != Binding.ObjectFormat
            || output.Install->Base != output.Context.Revision.Base
            || output.Install->Head != output.Context.Revision.Head
            || (
                !output.RecoveryInputCompacted
                && (
                    !output.Spool || !output.Preimport
                    || !coherent_install_facts(
                        *output.Spool,
                        *output.Preimport,
                        *output.Install)))))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(output.Candidate)
    {
        const auto& epoch = output.Candidate->Epoch;
        if(
            epoch.Source != output.Context.Revision.Source
            || epoch.BaseBranch != output.Context.Revision.BaseBranch
            || epoch.Base != output.Context.Revision.Base
            || epoch.Head != output.Context.Revision.Head
            || epoch.BaseAvailability
                != core::anchor_availability::available
            || epoch.HeadAvailability
                != core::anchor_availability::available
            || !coherent_candidate_review(
                output.Candidate->Review,
                output.Context.Revision.CurrentViewerReview,
                Binding.Authority.Viewer)
            || !epoch.Material || !output.Install
            || epoch.Material->BaseTree != output.Install->BaseTree
            || epoch.Material->HeadTree != output.Install->HeadTree
            || ((output.Context.Revision.Origin
                    == core::pull_request_revision_origin::
                        explicit_historical_import)
                != (epoch.Cause.Origin
                    == core::epoch_origin::explicit_historical_import)))
        {
            return std::unexpected(failure::schema_mismatch);
        }
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return output;
}

[[nodiscard]] auto valid_append_context(
    const app::append_operation_context& context,
    const app::epoch_catalog_binding& binding,
    const core::application_session_id session) noexcept -> bool
{
    return context.ApplicationSession == session
        && context.Request.value() != 0U && context.Generation.value() != 0U
        && context.Operation.valid() && core::valid(context.Revision)
        && context.Revision.Base.algorithm() == binding.ObjectFormat
        && (
            !context.Revision.CurrentViewerReview
            || context.Revision.CurrentViewerReview->Actor
                == binding.Authority.Viewer)
        && context.Catalog == binding.Catalog;
}

[[nodiscard]] constexpr auto coherent_candidate_review(
    const std::optional<core::catalog_review_association>& candidate,
    const std::optional<core::current_viewer_review_candidate>& source,
    const core::github_viewer_actor_id viewer) noexcept -> bool
{
    if(candidate.has_value() != source.has_value())
    {
        return false;
    }
    if(!candidate)
    {
        return true;
    }
    return source && candidate->Actor == viewer
        && candidate->Review == source->Review
        && candidate->Actor == source->Actor
        && candidate->Order == source->Order
        && candidate->State == source->State
        && candidate->Commit == source->Commit;
}

auto sqlite_epoch_catalog_session::reserve_append(
    const app::append_reservation_request& request,
    std::stop_token stop) -> result<app::append_operation_record>
{
    if(!valid_append_context(request.Context, Binding, ApplicationSession)
       || !core::valid(request.ProposedOrdinal))
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database,
           Binding,
           request.Context.Catalog.Revision,
           true,
           true);
       !active)
    {
        return std::unexpected(active.error());
    }
    auto existing = load_append(request.Context.Operation);
    if(existing)
    {
        if(existing->Context == request.Context
           && existing->ReservedOrdinal == request.ProposedOrdinal)
        {
            return existing;
        }
        return std::unexpected(failure::operation_conflict);
    }
    auto published_source = prepare(
        database,
        "SELECT 1 FROM epoch WHERE incarnation = ?1 "
        "AND source_observation = ?2 LIMIT 1");
    if(!published_source
       || !bind_fixed(
           published_source->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(
           published_source->Value,
           2,
           request.Context.Revision.Source.Observation))
    {
        return std::unexpected(
            published_source ? failure::unavailable
                             : published_source.error());
    }
    const auto published_source_code =
        ::sqlite3_step(published_source->Value);
    if(published_source_code == SQLITE_ROW)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(published_source_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    auto scope = prepare(
        database,
        "SELECT next_ordinal, catalog_revision FROM pull_request_scope "
        "WHERE incarnation = ?1 AND pr_scope = ?2 AND lifecycle = 1");
    if(!scope || !bind_fixed(scope->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(scope->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_step(scope->Value) != SQLITE_ROW)
    {
        return std::unexpected(failure::authority_mismatch);
    }
    if(::sqlite3_column_type(scope->Value, 0) == SQLITE_NULL)
    {
        return std::unexpected(failure::ordinal_exhausted);
    }
    const auto next = ::sqlite3_column_int64(scope->Value, 0);
    const auto revision = ::sqlite3_column_int64(scope->Value, 1);
    if(next != request.ProposedOrdinal.value()
       || revision != request.Context.Catalog.Revision.value()
       || ::sqlite3_step(scope->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto operation = prepare(
        database,
        "INSERT INTO operation(incarnation, operation, kind, phase, "
        "last_evidence, operation_revision, cache_scope, pr_scope, "
        "expected_revision, application_session, request_id, "
        "epoch_generation, source_observation, source_snapshot, "
        "identity_generation, pull_request_generation, reserved_ordinal, "
        "detail_count, "
        "retained_epoch_count, terminal) "
        "VALUES(?1, ?2, 1, 1, 1, 1, ?3, ?4, ?5, ?6, ?7, ?8, ?9, "
        "?10, ?11, ?12, ?13, 0, 0, 0)");
    if(!operation
       || !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(operation->Value, 2, request.Context.Operation)
       || !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
       || !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              operation->Value, 5, request.Context.Catalog.Revision.value())
            != SQLITE_OK
       || !bind_u64(
           operation->Value, 6, request.Context.ApplicationSession.value())
       || !bind_u64(operation->Value, 7, request.Context.Request.value())
       || !bind_u64(operation->Value, 8, request.Context.Generation.value())
       || !bind_fixed(
           operation->Value,
           9,
           request.Context.Revision.Source.Observation)
       || !bind_u64(
           operation->Value,
           10,
           request.Context.Revision.Source.Snapshot.value())
       || !bind_u64(
           operation->Value,
           11,
           request.Context.Revision.Source.IdentityGeneration.value())
       || !bind_u64(
           operation->Value,
           12,
           request.Context.Revision.Source.PullRequestGeneration.value())
       || ::sqlite3_bind_int64(
              operation->Value, 13, request.ProposedOrdinal.value())
            != SQLITE_OK
       || ::sqlite3_step(operation->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(
            database, {}, failure::operation_conflict));
    }
    auto append = prepare(
        database,
        "INSERT INTO append_operation(incarnation, operation, base_branch, "
        "revision_origin, base_algorithm, base_digest, head_algorithm, "
        "head_digest, source_review_present, staging_ref_format, "
        "final_ref_format) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, 1)");
    if(!append || !bind_fixed(append->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(append->Value, 2, request.Context.Operation)
       || !bind_fixed(append->Value, 3, request.Context.Revision.BaseBranch)
       || ::sqlite3_bind_int(
              append->Value,
              4,
              stored_enum(request.Context.Revision.Origin))
            != SQLITE_OK
       || !bind_oid(append->Value, 5, 6, request.Context.Revision.Base)
       || !bind_oid(append->Value, 7, 8, request.Context.Revision.Head)
       || ::sqlite3_bind_int(
              append->Value,
              9,
              request.Context.Revision.CurrentViewerReview ? 1 : 0)
            != SQLITE_OK
       || ::sqlite3_step(append->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    if(request.Context.Revision.CurrentViewerReview)
    {
        const auto& review = *request.Context.Revision.CurrentViewerReview;
        auto insert = prepare(
            database,
            "INSERT INTO append_source_review(incarnation, operation, "
            "review_id, actor_id, review_order, review_state, "
            "commit_algorithm, commit_digest) "
            "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
        if(!insert
           || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(insert->Value, 2, request.Context.Operation)
           || !bind_u64(insert->Value, 3, review.Review.value())
           || !bind_u64(insert->Value, 4, review.Actor.value())
           || ::sqlite3_bind_int(insert->Value, 5, review.Order.value())
               != SQLITE_OK
           || ::sqlite3_bind_int(
                  insert->Value, 6, stored_enum(review.State))
               != SQLITE_OK
           || !bind_oid(insert->Value, 7, 8, review.Commit)
           || ::sqlite3_step(insert->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
    }
    auto advance = prepare(
        database,
        "UPDATE pull_request_scope SET next_ordinal = "
        "CASE WHEN next_ordinal = 9223372036854775807 THEN NULL "
        "ELSE next_ordinal + 1 END "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND next_ordinal = ?3 AND catalog_revision = ?4");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(advance->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              advance->Value, 3, request.ProposedOrdinal.value())
            != SQLITE_OK
       || ::sqlite3_bind_int64(
              advance->Value, 4, request.Context.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto output = load_append(request.Context.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::record_spool(
    const app::record_spool_request& request,
    std::stop_token stop) -> result<app::append_operation_record>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog
       || request.Spool.ExpectedPack.Algorithm != Binding.ObjectFormat)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision);
       !active)
    {
        return std::unexpected(active.error());
    }
    auto existing = load_append(request.Operation);
    if(!existing)
    {
        return std::unexpected(existing.error());
    }
    if(existing->Phase == app::epoch_catalog_operation_phase::spool_recorded
       && existing->OperationRevision.value()
           == request.ExpectedOperationRevision.value() + 1
       && existing->Spool == request.Spool
       && existing->Preimport == request.Preimport)
    {
        return existing;
    }
    if(existing->Phase != app::epoch_catalog_operation_phase::preparing
       || existing->OperationRevision != request.ExpectedOperationRevision)
    {
        return std::unexpected(failure::phase_conflict);
    }
    auto receipt = prepare(
        database,
        "INSERT INTO append_receipt(incarnation, operation, spool_identity, "
        "spool_role, spool_bytes, expected_pack_algorithm, "
        "expected_pack_digest, observed_pack_algorithm, "
        "observed_pack_digest, repository_bytes, "
        "expected_pack_preexisting, inventory_count) "
        "VALUES(?1, ?2, ?3, 1, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
    if(!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(receipt->Value, 2, request.Operation)
       || !bind_bytes(receipt->Value, 3, request.Spool.Spool.Identity.Bytes)
       || ::sqlite3_bind_int64(
              receipt->Value,
              4,
              static_cast<std::int64_t>(request.Spool.Spool.Bytes))
            != SQLITE_OK
       || !bind_pack(receipt->Value, 5, 6, request.Spool.ExpectedPack)
       || !bind_pack(receipt->Value, 7, 8, request.Spool.ObservedPack)
       || ::sqlite3_bind_int64(
              receipt->Value,
              9,
              static_cast<std::int64_t>(request.Preimport.RepositoryBytes))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              receipt->Value,
              10,
              request.Preimport.ExpectedPackPreexisting ? 1 : 0)
            != SQLITE_OK
       || ::sqlite3_bind_int64(
              receipt->Value,
              11,
              static_cast<std::int64_t>(request.Preimport.Packs.size()))
            != SQLITE_OK
       || ::sqlite3_step(receipt->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    auto inventory = prepare(
        database,
        "INSERT INTO append_inventory(incarnation, operation, position, "
        "pack_algorithm, pack_digest, pack_bytes, index_bytes) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)");
    if(!inventory)
    {
        return std::unexpected(inventory.error());
    }
    for(std::size_t index = 0; index < request.Preimport.Packs.size(); ++index)
    {
        const auto& pack = request.Preimport.Packs[index];
        if(::sqlite3_reset(inventory->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(inventory->Value) != SQLITE_OK
           || !bind_fixed(
               inventory->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(inventory->Value, 2, request.Operation)
           || ::sqlite3_bind_int64(
                  inventory->Value, 3, static_cast<std::int64_t>(index))
               != SQLITE_OK
           || !bind_pack(inventory->Value, 4, 5, pack.Pack)
           || ::sqlite3_bind_int64(
                  inventory->Value,
                  6,
                  static_cast<std::int64_t>(pack.PackBytes))
               != SQLITE_OK
           || ::sqlite3_bind_int64(
                  inventory->Value,
                  7,
                  static_cast<std::int64_t>(pack.IndexBytes))
               != SQLITE_OK
           || ::sqlite3_step(inventory->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, stop));
        }
    }
    auto advance = prepare(
        database,
        "UPDATE operation SET phase = 2, last_evidence = 1, "
        "operation_revision = operation_revision + 1 "
        "WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
        "AND phase = 1 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope = ?5 "
        "AND operation_revision < 9223372036854775806");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance->Value, 2, request.Operation)
       || ::sqlite3_bind_int64(
              advance->Value, 3, request.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
       || !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    auto output = load_append(request.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::record_install(
    const app::record_install_request& request,
    std::stop_token stop) -> result<app::append_operation_record>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision);
       !active)
    {
        return std::unexpected(active.error());
    }
    auto existing = load_append(request.Operation);
    if(!existing)
    {
        return std::unexpected(existing.error());
    }
    if(request.Install.Base != existing->Context.Revision.Base
       || request.Install.Head != existing->Context.Revision.Head
       || request.Install.Pack.Algorithm != Binding.ObjectFormat)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(!existing->Spool || !existing->Preimport
       || !coherent_install_facts(
           *existing->Spool, *existing->Preimport, request.Install))
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(existing->Phase == app::epoch_catalog_operation_phase::imported
       && existing->OperationRevision.value()
           == request.ExpectedOperationRevision.value() + 1
       && existing->Install == request.Install)
    {
        return existing;
    }
    if(existing->Phase != app::epoch_catalog_operation_phase::spool_recorded
       || existing->OperationRevision != request.ExpectedOperationRevision
       || !existing->Spool || !existing->Preimport
       || existing->Spool->ExpectedPack != request.Install.Pack)
    {
        return std::unexpected(failure::phase_conflict);
    }
    const auto& install = request.Install;
    auto insert = prepare(
        database,
        "INSERT INTO append_install(incarnation, operation, pack_algorithm, "
        "pack_digest, base_algorithm, base_digest, base_tree_algorithm, "
        "base_tree_digest, head_algorithm, head_digest, "
        "head_tree_algorithm, head_tree_digest) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
    if(!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(insert->Value, 2, request.Operation)
       || !bind_pack(insert->Value, 3, 4, install.Pack)
       || !bind_oid(insert->Value, 5, 6, install.Base)
       || !bind_oid(insert->Value, 7, 8, install.BaseTree)
       || !bind_oid(insert->Value, 9, 10, install.Head)
       || !bind_oid(insert->Value, 11, 12, install.HeadTree)
       || ::sqlite3_step(insert->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    auto artifact = prepare(
        database,
        "INSERT INTO append_installed_artifact(incarnation, operation, "
        "artifact_role, artifact_identity, artifact_bytes) "
        "VALUES(?1, ?2, ?3, ?4, ?5)");
    if(!artifact)
    {
        return std::unexpected(artifact.error());
    }
    const std::array artifacts{install.InstalledPack, install.InstalledIndex};
    for(const auto& value : artifacts)
    {
        if(::sqlite3_reset(artifact->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(artifact->Value) != SQLITE_OK
           || !bind_fixed(
               artifact->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(artifact->Value, 2, request.Operation)
           || ::sqlite3_bind_int(
                  artifact->Value, 3, static_cast<int>(value.Role))
               != SQLITE_OK
           || !bind_bytes(artifact->Value, 4, value.Identity.Bytes)
           || ::sqlite3_bind_int64(
                  artifact->Value, 5, static_cast<std::int64_t>(value.Bytes))
               != SQLITE_OK
           || ::sqlite3_step(artifact->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, stop));
        }
    }
    auto advance = prepare(
        database,
        "UPDATE operation SET phase = 3, last_evidence = 1, "
        "operation_revision = operation_revision + 1 "
        "WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
        "AND phase = 2 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope = ?5 "
        "AND operation_revision < 9223372036854775806");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance->Value, 2, request.Operation)
       || ::sqlite3_bind_int64(
              advance->Value, 3, request.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
       || !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    auto output = load_append(request.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::record_candidate(
    const app::record_candidate_request& request,
    std::stop_token stop) -> result<app::append_operation_record>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision, true);
       !active)
    {
        return std::unexpected(active.error());
    }
    auto existing = load_append(request.Operation);
    if(!existing)
    {
        return std::unexpected(existing.error());
    }
    const auto& epoch = request.Candidate.Epoch;
    const auto candidate_coherent = coherent_candidate_review(
        request.Candidate.Review,
        existing->Context.Revision.CurrentViewerReview,
        Binding.Authority.Viewer);
    const auto material_coherent = epoch.Material && existing->Install
        && epoch.Material->BaseTree == existing->Install->BaseTree
        && epoch.Material->HeadTree == existing->Install->HeadTree;
    const auto provenance_coherent =
        (existing->Context.Revision.Origin
         == core::pull_request_revision_origin::explicit_historical_import)
        == (epoch.Cause.Origin
            == core::epoch_origin::explicit_historical_import);
    if(!candidate_coherent || !material_coherent || !provenance_coherent
       || epoch.BaseAvailability != core::anchor_availability::available
       || epoch.HeadAvailability != core::anchor_availability::available)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(request.Candidate.Review)
    {
        auto canonical = canonicalize_review(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            *request.Candidate.Review,
            std::pair{epoch.Id.Ordinal, epoch.Head});
        if(!canonical)
        {
            return std::unexpected(canonical.error());
        }
        if(canonical->Review != *request.Candidate.Review)
        {
            return std::unexpected(failure::operation_conflict);
        }
    }
    if(existing->Phase == app::epoch_catalog_operation_phase::commit_authorized
       && existing->Candidate == request.Candidate
       && existing->OperationRevision.value()
           == request.ExpectedOperationRevision.value() + 1)
    {
        return existing;
    }
    if(existing->Phase != app::epoch_catalog_operation_phase::staging_ready
       || existing->OperationRevision != request.ExpectedOperationRevision
       || !existing->Install || epoch.Id.Authority != Binding.Authority.Scope
       || epoch.Id.Ordinal != existing->ReservedOrdinal
       || epoch.Source != existing->Context.Revision.Source
       || epoch.BaseBranch != existing->Context.Revision.BaseBranch
       || epoch.Base != existing->Context.Revision.Base
       || epoch.Head != existing->Context.Revision.Head)
    {
        return std::unexpected(failure::operation_conflict);
    }
    auto insert = prepare(
        database,
        "INSERT INTO append_hidden_candidate(incarnation, operation, "
        "material_base_algorithm, material_base_digest, "
        "material_head_algorithm, material_head_digest, "
        "material_policy_revision, observed_at, origin, base_transition, "
        "head_transition, material_relation, primary_cause, "
        "base_availability, head_availability, candidate_review_present) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, "
        "?12, ?13, ?14, ?15, ?16)");
    if(!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(insert->Value, 2, request.Operation))
    {
        return std::unexpected(insert ? failure::unavailable : insert.error());
    }
    const auto& material = *epoch.Material;
    if(!bind_oid(insert->Value, 3, 4, material.BaseTree)
       || !bind_oid(insert->Value, 5, 6, material.HeadTree)
       || !bind_u64(insert->Value, 7, material.PolicyRevision.value()))
    {
        return std::unexpected(failure::unavailable);
    }
    if(::sqlite3_bind_int64(
           insert->Value, 8, epoch.ObservedAt.value())
           != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 9, stored_enum(epoch.Cause.Origin))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 10, stored_enum(epoch.Cause.Base))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 11, stored_enum(epoch.Cause.Head))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 12, stored_enum(epoch.Cause.Material))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 13, stored_enum(epoch.Cause.Primary))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 14, stored_enum(epoch.BaseAvailability))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 15, stored_enum(epoch.HeadAvailability))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 16, request.Candidate.Review ? 1 : 0)
            != SQLITE_OK
       || ::sqlite3_step(insert->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    if(request.Candidate.Review)
    {
        const auto& review = *request.Candidate.Review;
        auto review_insert = prepare(
            database,
            "INSERT INTO append_candidate_review(incarnation, operation, "
            "association, review_id, actor_id, review_order, review_state, "
            "commit_algorithm, commit_digest, availability) "
            "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
        if(!review_insert
           || !bind_fixed(
               review_insert->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(review_insert->Value, 2, request.Operation)
           || !bind_fixed(review_insert->Value, 3, review.Association)
           || !bind_u64(review_insert->Value, 4, review.Review.value())
           || !bind_u64(review_insert->Value, 5, review.Actor.value())
           || ::sqlite3_bind_int(
                  review_insert->Value, 6, review.Order.value())
               != SQLITE_OK
           || ::sqlite3_bind_int(
                  review_insert->Value, 7, stored_enum(review.State))
               != SQLITE_OK
           || !bind_oid(review_insert->Value, 8, 9, review.Commit)
           || ::sqlite3_bind_int(
                  review_insert->Value,
                  10,
                  stored_enum(review.Availability))
               != SQLITE_OK
           || ::sqlite3_step(review_insert->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, stop));
        }
    }
    auto advance = prepare(
        database,
        "UPDATE operation SET phase = 5, "
        "operation_revision = operation_revision + 1, "
        "last_evidence = 3 WHERE incarnation = ?1 AND operation = ?2 "
        "AND kind = 1 AND phase = 4 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope = ?5 "
        "AND operation_revision < 9223372036854775806");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance->Value, 2, request.Operation)
       || ::sqlite3_bind_int64(
              advance->Value, 3, request.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
       || !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    auto output = load_append(request.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::load_maintenance(
    const core::epoch_operation_id operation)
    -> result<app::maintenance_operation_record>
{
    auto* database = State->Database.handle();
    auto query = prepare(
        database,
        "SELECT o.kind, o.phase, o.operation_revision, o.repair_from_phase, "
        "o.cache_scope, "
        "o.detail_count, o.retained_epoch_count "
        "FROM operation o WHERE o.incarnation = ?1 "
        "AND o.operation = ?2 AND ("
        "(o.kind = 2 AND o.cache_scope = ?3 AND o.pr_scope = ?4) OR "
        "(o.kind = 3 AND o.pr_scope IS NULL AND EXISTS("
        "SELECT 1 FROM repository_scope current "
        "JOIN repository_scope owned "
        "ON owned.incarnation = current.incarnation "
        "AND owned.profile_id = current.profile_id "
        "AND owned.profile_revision = current.profile_revision "
        "AND owned.host_id = current.host_id "
        "AND owned.host_authority = current.host_authority "
        "AND owned.account_id = current.account_id "
        "AND owned.repository_id = current.repository_id "
        "AND owned.object_format = current.object_format "
        "WHERE current.incarnation = o.incarnation "
        "AND current.cache_scope = ?3 "
        "AND owned.cache_scope = o.cache_scope)))");
    if(!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(query->Value, 2, operation)
       || !bind_fixed(query->Value, 3, Binding.Authority.Cache)
       || !bind_fixed(query->Value, 4, Binding.Authority.Scope)
       || ::sqlite3_step(query->Value) != SQLITE_ROW)
    {
        return std::unexpected(
            query ? failure::operation_conflict : query.error());
    }
    auto cache = read_fixed<core::repository_cache_scope_id>(query->Value, 4);
    const auto expected_detail_count = ::sqlite3_column_int64(query->Value, 5);
    const auto retained_epoch_count = ::sqlite3_column_int64(query->Value, 6);
    app::maintenance_operation_record output{
        .Incarnation = Binding.Catalog.Incarnation,
        .Operation = operation,
        .Kind = static_cast<app::epoch_catalog_operation_kind>(
            ::sqlite3_column_int(query->Value, 0)),
        .Phase = static_cast<app::epoch_catalog_operation_phase>(
            ::sqlite3_column_int(query->Value, 1)),
        .RepairFromPhase =
            ::sqlite3_column_type(query->Value, 3) == SQLITE_NULL
            ? std::nullopt
            : std::optional{static_cast<app::epoch_catalog_operation_phase>(
                  ::sqlite3_column_int(query->Value, 3))},
        .OperationRevision = app::epoch_catalog_operation_revision{
            ::sqlite3_column_int64(query->Value, 2)},
    };
    if(!cache || expected_detail_count < 1
       || expected_detail_count
           > static_cast<std::int64_t>(
               app::maximum_cache_delete_scope_count)
       || retained_epoch_count < 0
       || !app::valid(output.OperationRevision)
       || ::sqlite3_step(query->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    output.Cache = *cache;
    std::uint64_t observed_retained_epoch_count = 0U;
    if(output.Kind == app::epoch_catalog_operation_kind::prune)
    {
        auto rows = prepare(
            database,
            "SELECT pr_scope, ordinal, base_algorithm, base_digest, "
            "base_availability, head_algorithm, head_digest, "
            "head_availability FROM prune_operation "
            "WHERE incarnation = ?1 AND operation = ?2 "
            "ORDER BY pr_scope, ordinal LIMIT 101");
        if(!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(rows->Value, 2, operation))
        {
            return std::unexpected(rows ? failure::unavailable : rows.error());
        }
        while(true)
        {
            const auto code = ::sqlite3_step(rows->Value);
            if(code == SQLITE_DONE)
            {
                break;
            }
            if(code != SQLITE_ROW)
            {
                return std::unexpected(sql_failure(database, {}));
            }
            const auto authority =
                read_fixed<core::pull_request_authority_scope_id>(
                    rows->Value, 0);
            const auto ordinal = core::epoch_ordinal{
                ::sqlite3_column_int64(rows->Value, 1)};
            const auto base =
                read_oid<core::git_commit_oid>(rows->Value, 2, 3);
            const auto head =
                read_oid<core::git_commit_oid>(rows->Value, 5, 6);
            const auto base_availability =
                restored_enum<core::anchor_availability>(
                    ::sqlite3_column_int(rows->Value, 4));
            const auto head_availability =
                restored_enum<core::anchor_availability>(
                    ::sqlite3_column_int(rows->Value, 7));
            if(!authority || *authority != Binding.Authority.Scope
               || !core::valid(ordinal) || !base || !head
               || ::sqlite3_column_type(rows->Value, 4) != SQLITE_INTEGER
               || ::sqlite3_column_type(rows->Value, 7) != SQLITE_INTEGER
               || !app::valid_prune_anchor_availability(base_availability)
               || !app::valid_prune_anchor_availability(head_availability))
            {
                return std::unexpected(failure::schema_mismatch);
            }
            if(output.Epochs.size() >= app::maximum_prune_epoch_count)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            output.Epochs.push_back(app::prune_epoch_fact{
                .Epoch =
                    core::review_epoch_id{
                        .Authority = *authority,
                        .Incarnation = Binding.Catalog.Incarnation,
                        .Ordinal = ordinal,
                    },
                .Retained =
                    app::git_retained_ref_pair{
                        .Scope = *authority,
                        .Ordinal = ordinal,
                        .Base = *base,
                        .Head = *head,
                    },
                .BaseRef =
                    app::epoch_catalog_ref_identity{
                        .Authority = *authority,
                        .Ordinal = ordinal,
                    },
                .HeadRef =
                    app::epoch_catalog_ref_identity{
                        .Authority = *authority,
                        .Ordinal = ordinal,
                        .Head = true,
                    },
                .ExpectedBaseAvailability = base_availability,
                .ExpectedHeadAvailability = head_availability,
            });
        }
    }
    else if(output.Kind == app::epoch_catalog_operation_kind::cache_delete)
    {
        auto rows = prepare(
            database,
            "SELECT cache_scope, pr_scope, expected_revision, epoch_count "
            "FROM cache_delete_scope "
            "WHERE incarnation = ?1 AND operation = ?2 "
            "ORDER BY pr_scope LIMIT 10001");
        if(!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(rows->Value, 2, operation))
        {
            return std::unexpected(rows ? failure::unavailable : rows.error());
        }
        while(true)
        {
            const auto code = ::sqlite3_step(rows->Value);
            if(code == SQLITE_DONE)
            {
                break;
            }
            if(code != SQLITE_ROW)
            {
                return std::unexpected(sql_failure(database, {}));
            }
            auto cache = read_fixed<core::repository_cache_scope_id>(
                rows->Value, 0);
            auto authority =
                read_fixed<core::pull_request_authority_scope_id>(
                    rows->Value, 1);
            const auto revision = ::sqlite3_column_int64(rows->Value, 2);
            const auto epoch_count = ::sqlite3_column_int64(rows->Value, 3);
            if(!cache || *cache != output.Cache || !authority
               || revision < 0 || epoch_count < 0
               || observed_retained_epoch_count
                   > static_cast<std::uint64_t>(
                         std::numeric_limits<std::int64_t>::max())
                       - static_cast<std::uint64_t>(epoch_count))
            {
                return std::unexpected(failure::schema_mismatch);
            }
            observed_retained_epoch_count +=
                static_cast<std::uint64_t>(epoch_count);
            if(output.Scopes.size()
               >= app::maximum_cache_delete_scope_count)
            {
                return std::unexpected(failure::schema_mismatch);
            }
            output.Scopes.push_back(app::cache_delete_scope_fact{
                .Authority = *authority,
                .ExpectedRevision = core::epoch_catalog_revision{revision},
            });
        }
    }
    else
    {
        return std::unexpected(failure::schema_mismatch);
    }
    const auto actual_detail_count = output.Kind
            == app::epoch_catalog_operation_kind::prune
        ? output.Epochs.size()
        : output.Scopes.size();
    if(actual_detail_count
       != static_cast<std::size_t>(expected_detail_count))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(
        output.Kind == app::epoch_catalog_operation_kind::prune
        && (
            expected_detail_count
                > static_cast<std::int64_t>(app::maximum_prune_epoch_count)
            || retained_epoch_count != 0))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(
        output.Kind == app::epoch_catalog_operation_kind::cache_delete
        && observed_retained_epoch_count
            != static_cast<std::uint64_t>(retained_epoch_count))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    return output;
}

auto sqlite_epoch_catalog_session::transition_operation(
    const app::operation_transition_request& request,
    std::stop_token stop) -> result<app::epoch_recovery_operation>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, {}};
    transaction write{database, {}};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto current_append = request.Kind == app::epoch_catalog_operation_kind::append
        ? load_append(request.Operation)
        : result<app::append_operation_record>{
              std::unexpected(failure::operation_conflict)};
    auto current_maintenance =
        request.Kind != app::epoch_catalog_operation_kind::append
        ? load_maintenance(request.Operation)
        : result<app::maintenance_operation_record>{
              std::unexpected(failure::operation_conflict)};
    const auto phase = current_append ? current_append->Phase
        : current_maintenance          ? current_maintenance->Phase
                                       : request.Expected;
    const auto revision = current_append ? current_append->OperationRevision
        : current_maintenance ? current_maintenance->OperationRevision
                              : app::epoch_catalog_operation_revision{};
    if(!current_append && !current_maintenance)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(current_maintenance && current_maintenance->Kind != request.Kind)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(stop.stop_requested() && !app::point_of_no_cancellation(phase))
    {
        return std::unexpected(failure::cancelled);
    }
    if(phase == request.Next
       && revision.value() == request.ExpectedOperationRevision.value() + 1)
    {
        if(current_append)
        {
            return app::epoch_recovery_operation{*current_append};
        }
        return app::epoch_recovery_operation{*current_maintenance};
    }
    if(phase != request.Expected
       || revision != request.ExpectedOperationRevision)
    {
        return std::unexpected(failure::phase_conflict);
    }
    if(current_append)
    {
        if(request.Next == app::epoch_catalog_operation_phase::staging_ready
           && !current_append->Install)
        {
            return std::unexpected(failure::phase_conflict);
        }
    }
    if(request.Kind != app::epoch_catalog_operation_kind::cache_delete)
    {
        if(auto active = require_active_pull_request_scope(
               database, Binding, request.Catalog.Revision);
           !active)
        {
            return std::unexpected(active.error());
        }
    }
    if(request.Next == app::epoch_catalog_operation_phase::repair_required)
    {
        const auto repairing_tombstoned_cache = request.Kind
                == app::epoch_catalog_operation_kind::cache_delete
            && request.Expected
                == app::epoch_catalog_operation_phase::
                    catalog_tombstoned_cleanup_pending;
        if(repairing_tombstoned_cache)
        {
            for(const auto& scope_fact : current_maintenance->Scopes)
            {
                auto scope = load_pull_request_scope_state(
                    database,
                    Binding.Catalog.Incarnation,
                    scope_fact.Authority);
                if(!scope || scope->Cache != current_maintenance->Cache
                   || scope->Lifecycle != 3
                   || scope->RepositoryLifecycle != 3
                   || scope->RetainedEpochCount != 0U)
                {
                    return std::unexpected(
                        scope ? failure::operation_conflict : scope.error());
                }
            }
        }
        auto fail_closed = prepare(
            database,
            request.Kind == app::epoch_catalog_operation_kind::cache_delete
                ? repairing_tombstoned_cache
                    ? "UPDATE repository_scope SET lifecycle = 3 "
                      "WHERE incarnation = ?1 AND cache_scope = ?2 "
                      "AND lifecycle = 3"
                    : "UPDATE repository_scope SET lifecycle = 2 "
                      "WHERE incarnation = ?1 AND cache_scope = ?2 "
                      "AND lifecycle IN (1, 2)"
                : "UPDATE pull_request_scope SET lifecycle = 4 "
                  "WHERE incarnation = ?1 AND pr_scope = ?2 "
                  "AND cache_scope = ?3 AND lifecycle = 1");
        if(!fail_closed)
        {
            return std::unexpected(fail_closed.error());
        }
        const auto authority_bound =
            request.Kind == app::epoch_catalog_operation_kind::cache_delete
            ? bind_fixed(
                fail_closed->Value, 2, Binding.Authority.Cache)
            : bind_fixed(
                  fail_closed->Value, 2, Binding.Authority.Scope)
                && bind_fixed(
                    fail_closed->Value, 3, Binding.Authority.Cache);
        if(!bind_fixed(
               fail_closed->Value, 1, Binding.Catalog.Incarnation)
           || !authority_bound
           || ::sqlite3_step(fail_closed->Value) != SQLITE_DONE
           || ::sqlite3_changes(database) != 1)
        {
            return std::unexpected(failure::operation_conflict);
        }
    }
    if(
        request.Kind == app::epoch_catalog_operation_kind::cache_delete
        && request.Expected
            == app::epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending
        && request.Next == app::epoch_catalog_operation_phase::complete)
    {
        auto retire = prepare(
            database,
            "UPDATE repository_scope SET lifecycle = 4 "
            "WHERE incarnation = ?1 AND cache_scope = ?2 "
            "AND lifecycle = 3");
        if(!retire
           || !bind_fixed(
               retire->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(retire->Value, 2, Binding.Authority.Cache)
           || ::sqlite3_step(retire->Value) != SQLITE_DONE
           || ::sqlite3_changes(database) != 1)
        {
            return std::unexpected(failure::operation_conflict);
        }
    }
    if(
        request.Kind == app::epoch_catalog_operation_kind::cache_delete
        && request.Expected
            == app::epoch_catalog_operation_phase::abort_requested
        && request.Next == app::epoch_catalog_operation_phase::aborted)
    {
        auto reactivate = prepare(
            database,
            "UPDATE repository_scope SET lifecycle = 1 "
            "WHERE incarnation = ?1 AND cache_scope = ?2 "
            "AND lifecycle = 2");
        if(!reactivate
           || !bind_fixed(
               reactivate->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(
               reactivate->Value, 2, Binding.Authority.Cache)
           || ::sqlite3_step(reactivate->Value) != SQLITE_DONE
           || ::sqlite3_changes(database) != 1)
        {
            return std::unexpected(
                reactivate ? failure::operation_conflict
                           : reactivate.error());
        }
    }
    if(
        current_append
        && (
            request.Next == app::epoch_catalog_operation_phase::complete
            || request.Next
                == app::epoch_catalog_operation_phase::aborted))
    {
        auto inventory = prepare(
            database,
            "DELETE FROM append_inventory WHERE incarnation = ?1 "
            "AND operation = ?2");
        if(!inventory
           || !bind_fixed(
               inventory->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(inventory->Value, 2, request.Operation)
           || ::sqlite3_step(inventory->Value) != SQLITE_DONE
           || ::sqlite3_changes(database)
               != static_cast<int>(
                   current_append->Preimport
                       ? current_append->Preimport->Packs.size()
                       : 0U))
        {
            return std::unexpected(failure::operation_conflict);
        }
        auto receipt = prepare(
            database,
            "DELETE FROM append_receipt WHERE incarnation = ?1 "
            "AND operation = ?2");
        if(!receipt
           || !bind_fixed(
               receipt->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(receipt->Value, 2, request.Operation)
           || ::sqlite3_step(receipt->Value) != SQLITE_DONE
           || ::sqlite3_changes(database)
               != (current_append->Spool ? 1 : 0))
        {
            return std::unexpected(failure::operation_conflict);
        }
    }
    const auto terminal =
        request.Next == app::epoch_catalog_operation_phase::complete ? 1
        : request.Next == app::epoch_catalog_operation_phase::aborted ? 2
        : request.Next == app::epoch_catalog_operation_phase::repair_required
            ? 3
            : 0;
    auto update = prepare(
        database,
        "UPDATE operation SET phase = ?1, last_evidence = ?2, "
        "operation_revision = operation_revision + 1, terminal = ?3 "
        ", repair_from_phase = CASE WHEN ?1 = 12 THEN ?7 ELSE NULL END "
        ", recovery_input_compacted = CASE "
        "WHEN ?6 = 1 AND ?1 IN (9, 11) THEN 1 ELSE 0 END "
        "WHERE incarnation = ?4 AND operation = ?5 AND kind = ?6 "
        "AND phase = ?7 AND operation_revision = ?8 "
        "AND cache_scope = ?9 AND ((kind IN (1, 2) AND pr_scope = ?10) "
        "OR (kind = 3 AND pr_scope IS NULL)) "
        "AND operation_revision < ?11");
    if(!update
       || ::sqlite3_bind_int(
              update->Value, 1, static_cast<int>(request.Next))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              update->Value, 2, static_cast<int>(request.Evidence))
            != SQLITE_OK
       || ::sqlite3_bind_int(update->Value, 3, terminal) != SQLITE_OK
       || !bind_fixed(update->Value, 4, Binding.Catalog.Incarnation)
       || !bind_operation(update->Value, 5, request.Operation)
       || ::sqlite3_bind_int(
              update->Value, 6, static_cast<int>(request.Kind))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              update->Value, 7, static_cast<int>(request.Expected))
            != SQLITE_OK
       || ::sqlite3_bind_int64(
              update->Value, 8, request.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(update->Value, 9, Binding.Authority.Cache)
       || !bind_fixed(update->Value, 10, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              update->Value,
              11,
              app::terminal(request.Next)
                  ? app::maximum_epoch_catalog_operation_revision
                  : app::maximum_epoch_catalog_operation_revision - 1)
            != SQLITE_OK
       || ::sqlite3_step(update->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::phase_conflict);
    }
    result<app::epoch_recovery_operation> loaded =
        std::unexpected(failure::operation_conflict);
    if(request.Kind == app::epoch_catalog_operation_kind::append)
    {
        auto append = load_append(request.Operation);
        if(!append)
        {
            return std::unexpected(append.error());
        }
        loaded = app::epoch_recovery_operation{*append};
    }
    else
    {
        auto maintenance = load_maintenance(request.Operation);
        if(!maintenance)
        {
            return std::unexpected(maintenance.error());
        }
        loaded = app::epoch_recovery_operation{*maintenance};
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return loaded;
}

[[nodiscard]] auto persist_review(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::pull_request_authority_scope_id& authority,
    const core::catalog_review_association& review,
    const core::provider_snapshot_id snapshot,
    const std::optional<core::epoch_ordinal> linked,
    std::stop_token stop) -> result<void>
{
    auto upsert = prepare(
        database,
        "INSERT INTO review_association(incarnation, pr_scope, association, "
        "review_id, actor_id, review_order, review_state, commit_algorithm, "
        "commit_digest, availability, linked_ordinal, source_snapshot) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12) "
        "ON CONFLICT(incarnation, pr_scope, association) DO UPDATE SET "
        "review_id = excluded.review_id, actor_id = excluded.actor_id, "
        "review_order = excluded.review_order, "
        "review_state = excluded.review_state, "
        "commit_algorithm = excluded.commit_algorithm, "
        "commit_digest = excluded.commit_digest, "
        "availability = excluded.availability, "
        "linked_ordinal = excluded.linked_ordinal, "
        "source_snapshot = excluded.source_snapshot");
    if(!upsert || !bind_fixed(upsert->Value, 1, incarnation)
       || !bind_fixed(upsert->Value, 2, authority)
       || !bind_fixed(upsert->Value, 3, review.Association)
       || !bind_u64(upsert->Value, 4, review.Review.value())
       || !bind_u64(upsert->Value, 5, review.Actor.value())
       || ::sqlite3_bind_int(upsert->Value, 6, review.Order.value())
           != SQLITE_OK
       || ::sqlite3_bind_int(
              upsert->Value, 7, stored_enum(review.State))
            != SQLITE_OK
       || !bind_oid(upsert->Value, 8, 9, review.Commit)
       || ::sqlite3_bind_int(
              upsert->Value, 10, stored_enum(review.Availability))
            != SQLITE_OK
       || (linked
               ? ::sqlite3_bind_int64(
                     upsert->Value, 11, linked->value())
               : ::sqlite3_bind_null(upsert->Value, 11))
            != SQLITE_OK
       || !bind_u64(upsert->Value, 12, snapshot.value())
       || ::sqlite3_step(upsert->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(
            database, stop, failure::operation_conflict));
    }
    auto pointer = prepare(
        database,
        "INSERT INTO current_viewer_review(incarnation, pr_scope, "
        "association, accepted_snapshot) VALUES(?1, ?2, ?3, ?4) "
        "ON CONFLICT(incarnation, pr_scope) DO UPDATE SET "
        "association = excluded.association, "
        "accepted_snapshot = excluded.accepted_snapshot");
    if(!pointer || !bind_fixed(pointer->Value, 1, incarnation)
       || !bind_fixed(pointer->Value, 2, authority)
       || !bind_fixed(pointer->Value, 3, review.Association)
       || !bind_u64(pointer->Value, 4, snapshot.value())
       || ::sqlite3_step(pointer->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    return {};
}

auto sqlite_epoch_catalog_session::finalize_append(
    const app::finalize_append_request& request,
    std::stop_token stop) -> result<app::catalog_commit_result>
{
    if(!app::valid(request)
       || request.Catalog.Incarnation != Binding.Catalog.Incarnation
       || request.Mutation.Epoch.Id.Authority != Binding.Authority.Scope
       || request.Mutation.Epoch.Base.algorithm() != Binding.ObjectFormat)
    {
        return std::unexpected(failure::invalid_request);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, {}};
    transaction write{database, {}};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto operation = load_append(request.Operation);
    if(!operation)
    {
        return std::unexpected(operation.error());
    }
    if(
        operation->Phase
            == app::epoch_catalog_operation_phase::visible_cleanup_pending
        || operation->Phase == app::epoch_catalog_operation_phase::complete)
    {
        auto scope = load_pull_request_scope_state(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope);
        auto epoch = load_epoch_summary(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            request.Mutation.Epoch.Id.Ordinal);
        if(!scope || scope->Cache != Binding.Authority.Cache
           || scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
           || !advanced_once(request.Catalog.Revision, scope->Revision)
           || !finalized_operation_revision_matches(
               operation->Phase,
               request.ExpectedOperationRevision,
               operation->OperationRevision,
               app::epoch_catalog_operation_phase::
                   visible_cleanup_pending)
           || operation->Context.Catalog != request.Catalog
           || operation->Candidate != request.Mutation || !epoch
           || *epoch != request.Mutation.Epoch)
        {
            return std::unexpected(failure::operation_conflict);
        }
        if(request.Mutation.Epoch.Cause.Origin
           != core::epoch_origin::explicit_historical_import)
        {
            auto retention = prepare(
                database,
                "SELECT retention_rehydration_required "
                "FROM pull_request_scope WHERE incarnation = ?1 "
                "AND pr_scope = ?2");
            if(!retention
               || !bind_fixed(
                   retention->Value, 1, Binding.Catalog.Incarnation)
               || !bind_fixed(
                   retention->Value, 2, Binding.Authority.Scope)
               || ::sqlite3_step(retention->Value) != SQLITE_ROW
               || ::sqlite3_column_int(retention->Value, 0) != 0
               || ::sqlite3_step(retention->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::operation_conflict);
            }
        }
        if(request.Mutation.Review)
        {
            auto canonical = canonicalize_review(
                database,
                Binding.Catalog.Incarnation,
                Binding.Authority.Scope,
                *request.Mutation.Review);
            if(!canonical)
            {
                return std::unexpected(canonical.error());
            }
            auto review = load_review_association(
                database,
                Binding.Catalog.Incarnation,
                Binding.Authority.Scope,
                Binding.Authority.Viewer,
                request.Mutation.Review->Association);
            auto current = prepare(
                database,
                "SELECT association, accepted_snapshot "
                "FROM current_viewer_review WHERE incarnation = ?1 "
                "AND pr_scope = ?2");
            if(!review || *review != canonical->Review || !current
               || !bind_fixed(
                   current->Value, 1, Binding.Catalog.Incarnation)
               || !bind_fixed(
                   current->Value, 2, Binding.Authority.Scope)
               || ::sqlite3_step(current->Value) != SQLITE_ROW)
            {
                return std::unexpected(failure::operation_conflict);
            }
            auto association = read_fixed<core::review_association_id>(
                current->Value, 0);
            auto snapshot = read_u64(current->Value, 1);
            if(!association || !snapshot
               || *association != request.Mutation.Review->Association
               || *snapshot
                   != operation->Context.Revision.Source.Snapshot.value()
               || ::sqlite3_step(current->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::operation_conflict);
            }
        }
        Binding.Catalog.Revision = scope->Revision;
        return app::catalog_commit_result{
            .Catalog = Binding.Catalog,
            .Changed = true,
            .InsertedEpoch = request.Mutation.Epoch.Id,
        };
    }
    if(stop.stop_requested()
       && !app::point_of_no_cancellation(operation->Phase))
    {
        return std::unexpected(failure::cancelled);
    }
    if(request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(auto active = require_active_pull_request_scope(
           database,
           Binding,
           request.Catalog.Revision,
           true,
           true);
       !active)
    {
        return std::unexpected(active.error());
    }
    if(operation->Phase != app::epoch_catalog_operation_phase::refs_committed
       || operation->OperationRevision != request.ExpectedOperationRevision
       || operation->Candidate != request.Mutation
       || request.Mutation.Epoch.Id.Authority != Binding.Authority.Scope)
    {
        return std::unexpected(failure::phase_conflict);
    }
    if(!coherent_candidate_review(
           request.Mutation.Review,
           operation->Context.Revision.CurrentViewerReview,
           Binding.Authority.Viewer))
    {
        return std::unexpected(failure::operation_conflict);
    }
    const auto& epoch = request.Mutation.Epoch;
    std::optional<core::epoch_ordinal> latest;
    std::optional<core::epoch_ordinal> previous;
    auto pointers = prepare(
        database,
        "SELECT latest_live_ordinal, previous_distinct_head_ordinal, "
        "retention_rehydration_required FROM pull_request_scope "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND catalog_revision = ?3 AND lifecycle = 1");
    if(!pointers
       || !bind_fixed(pointers->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(pointers->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              pointers->Value, 3, request.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(pointers->Value) != SQLITE_ROW)
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(::sqlite3_column_type(pointers->Value, 0) == SQLITE_INTEGER)
    {
        latest = core::epoch_ordinal{
            ::sqlite3_column_int64(pointers->Value, 0)};
    }
    if(::sqlite3_column_type(pointers->Value, 1) == SQLITE_INTEGER)
    {
        previous = core::epoch_ordinal{
            ::sqlite3_column_int64(pointers->Value, 1)};
    }
    const auto retention_required =
        ::sqlite3_column_int(pointers->Value, 2);
    if((retention_required != 0 && retention_required != 1)
       || ::sqlite3_step(pointers->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::schema_mismatch);
    }
    std::optional<core::review_epoch_summary> prior_latest;
    if(latest)
    {
        auto loaded = load_epoch_summary(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            *latest);
        if(!loaded)
        {
            return std::unexpected(loaded.error());
        }
        prior_latest = *loaded;
    }
    const auto rehydrating = epoch.Cause.Origin
        == core::epoch_origin::retention_rehydration;
    const auto historical = epoch.Cause.Origin
        == core::epoch_origin::explicit_historical_import;
    const auto same_semantic_revision = prior_latest
        && prior_latest->BaseBranch == epoch.BaseBranch
        && prior_latest->Base == epoch.Base
        && prior_latest->Head == epoch.Head;
    const auto fresh_source_observation = prior_latest
        && prior_latest->Source.Observation != epoch.Source.Observation;
    const auto semantic_revision_changed = prior_latest
        && (prior_latest->BaseBranch != epoch.BaseBranch
            || prior_latest->Base != epoch.Base
            || prior_latest->Head != epoch.Head);
    if(rehydrating
       && (retention_required == 0 || !same_semantic_revision
           || !fresh_source_observation
           || prior_latest->BaseAvailability
               != core::anchor_availability::cache_deleted
           || prior_latest->HeadAvailability
               != core::anchor_availability::cache_deleted))
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(retention_required != 0 && !historical && !rehydrating
       && !semantic_revision_changed)
    {
        return std::unexpected(failure::operation_conflict);
    }
    auto insert = prepare(
        database,
        "INSERT INTO epoch(incarnation, pr_scope, ordinal, "
        "source_observation, source_snapshot, identity_generation, "
        "pull_request_generation, base_branch, "
        "observed_at, origin, base_transition, head_transition, "
        "material_relation, primary_cause, material_base_algorithm, "
        "material_base_digest, material_head_algorithm, "
        "material_head_digest, material_policy_revision) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
        "?13, ?14, ?15, ?16, ?17, ?18, ?19)");
    if(!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(insert->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(insert->Value, 3, epoch.Id.Ordinal.value())
           != SQLITE_OK
       || !bind_fixed(insert->Value, 4, epoch.Source.Observation)
       || !bind_u64(insert->Value, 5, epoch.Source.Snapshot.value())
       || !bind_u64(
           insert->Value, 6, epoch.Source.IdentityGeneration.value())
       || !bind_u64(
           insert->Value, 7, epoch.Source.PullRequestGeneration.value())
       || !bind_fixed(insert->Value, 8, epoch.BaseBranch)
       || ::sqlite3_bind_int64(insert->Value, 9, epoch.ObservedAt.value())
           != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 10, stored_enum(epoch.Cause.Origin))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 11, stored_enum(epoch.Cause.Base))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 12, stored_enum(epoch.Cause.Head))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 13, stored_enum(epoch.Cause.Material))
            != SQLITE_OK
       || ::sqlite3_bind_int(
              insert->Value, 14, stored_enum(epoch.Cause.Primary))
            != SQLITE_OK)
    {
        return std::unexpected(failure::unavailable);
    }
    if(!epoch.Material
       || !bind_oid(insert->Value, 15, 16, epoch.Material->BaseTree)
       || !bind_oid(insert->Value, 17, 18, epoch.Material->HeadTree)
       || !bind_u64(
           insert->Value, 19, epoch.Material->PolicyRevision.value()))
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(::sqlite3_step(insert->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(
            database, {}, failure::operation_conflict));
    }
    auto anchor = prepare(
        database,
        "INSERT INTO epoch_anchor(incarnation, pr_scope, ordinal, endpoint, "
        "object_algorithm, object_digest, ref_format, availability) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1, ?7)");
    if(!anchor)
    {
        return std::unexpected(anchor.error());
    }
    const std::array anchor_values{
        std::pair{epoch.Base, epoch.BaseAvailability},
        std::pair{epoch.Head, epoch.HeadAvailability},
    };
    for(std::size_t index = 0; index < anchor_values.size(); ++index)
    {
        if(::sqlite3_reset(anchor->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
           || !bind_fixed(
               anchor->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(anchor->Value, 2, Binding.Authority.Scope)
           || ::sqlite3_bind_int64(
                  anchor->Value, 3, epoch.Id.Ordinal.value())
               != SQLITE_OK
           || ::sqlite3_bind_int(
                  anchor->Value, 4, static_cast<int>(index + 1U))
               != SQLITE_OK
           || !bind_oid(anchor->Value, 5, 6, anchor_values[index].first)
           || ::sqlite3_bind_int(
                  anchor->Value,
                  7,
                  stored_enum(anchor_values[index].second))
               != SQLITE_OK
           || ::sqlite3_step(anchor->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
    }
    if(request.Mutation.Review)
    {
        auto canonical = canonicalize_review(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            *request.Mutation.Review);
        if(!canonical)
        {
            return std::unexpected(canonical.error());
        }
        auto persisted = persist_review(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope,
            canonical->Review,
            operation->Context.Revision.Source.Snapshot,
            canonical->LinkedOrdinal,
            {});
        if(!persisted)
        {
            return std::unexpected(persisted.error());
        }
    }
    if(epoch.Cause.Origin != core::epoch_origin::explicit_historical_import)
    {
        if(latest && prior_latest->Head != epoch.Head)
        {
            previous = latest;
        }
        latest = epoch.Id.Ordinal;
    }
    auto advance_scope = prepare(
        database,
        "UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
        "retained_epoch_count = retained_epoch_count + 1, "
        "latest_live_ordinal = ?1, previous_distinct_head_ordinal = ?2, "
        "retention_rehydration_required = CASE WHEN ?3 = 1 THEN "
        "retention_rehydration_required ELSE 0 END "
        "WHERE incarnation = ?4 AND pr_scope = ?5 "
        "AND catalog_revision = ?6 AND lifecycle = 1 "
        "AND retained_epoch_count < ?7 "
        "AND catalog_revision < 9223372036854775807");
    if(!advance_scope
       || (latest
               ? ::sqlite3_bind_int64(
                     advance_scope->Value, 1, latest->value())
               : ::sqlite3_bind_null(advance_scope->Value, 1))
            != SQLITE_OK
       || (previous
               ? ::sqlite3_bind_int64(
                     advance_scope->Value, 2, previous->value())
               : ::sqlite3_bind_null(advance_scope->Value, 2))
            != SQLITE_OK
       || ::sqlite3_bind_int(advance_scope->Value, 3, historical ? 1 : 0)
            != SQLITE_OK
       || !bind_fixed(
           advance_scope->Value, 4, Binding.Catalog.Incarnation)
       || !bind_fixed(advance_scope->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              advance_scope->Value, 6, request.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_bind_int64(
              advance_scope->Value,
              7,
              static_cast<std::int64_t>(
                  app::maximum_retained_epoch_count_per_pull_request))
            != SQLITE_OK
       || ::sqlite3_step(advance_scope->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto advance_operation = prepare(
        database,
        "UPDATE operation SET phase = 8, last_evidence = 1, "
        "operation_revision = operation_revision + 1 "
        "WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
        "AND phase = 7 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope = ?5 "
        "AND operation_revision < 9223372036854775806");
    if(!advance_operation
       || !bind_fixed(
           advance_operation->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance_operation->Value, 2, request.Operation)
       || ::sqlite3_bind_int64(
              advance_operation->Value,
              3,
              request.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(
           advance_operation->Value, 4, Binding.Authority.Cache)
       || !bind_fixed(
           advance_operation->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_step(advance_operation->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    Binding.Catalog.Revision = core::epoch_catalog_revision{
        request.Catalog.Revision.value() + 1};
    return app::catalog_commit_result{
        .Catalog = Binding.Catalog,
        .Changed = true,
        .InsertedEpoch = epoch.Id,
    };
}

auto sqlite_epoch_catalog_session::associate_review(
    const app::associate_review_request& request,
    std::stop_token stop) -> result<app::catalog_commit_result>
{
    if(!app::valid(request) || request.Catalog != Binding.Catalog
       || request.Review.Actor != Binding.Authority.Viewer
       || request.Review.Commit.algorithm() != Binding.ObjectFormat)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision, true);
       !active)
    {
        return std::unexpected(active.error());
    }
    if(auto idle = require_no_blocking_operation(database, Binding, false);
       !idle)
    {
        return std::unexpected(idle.error());
    }
    auto canonical = canonicalize_review(
        database,
        Binding.Catalog.Incarnation,
        Binding.Authority.Scope,
        request.Review);
    if(!canonical)
    {
        return std::unexpected(canonical.error());
    }
    const auto& review_fact = canonical->Review;
    auto current = prepare(
        database,
        "SELECT c.association, c.accepted_snapshot, r.review_id, "
        "r.actor_id, r.review_order, r.review_state, r.commit_algorithm, "
        "r.commit_digest, r.availability "
        "FROM current_viewer_review c JOIN review_association r "
        "ON r.incarnation = c.incarnation AND r.pr_scope = c.pr_scope "
        "AND r.association = c.association "
        "WHERE c.incarnation = ?1 AND c.pr_scope = ?2");
    if(!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(current->Value, 2, Binding.Authority.Scope))
    {
        return std::unexpected(current ? failure::unavailable : current.error());
    }
    const auto current_code = ::sqlite3_step(current->Value);
    if(current_code == SQLITE_ROW)
    {
        const auto association =
            read_fixed<core::review_association_id>(current->Value, 0);
        const auto snapshot = read_u64(current->Value, 1);
        const auto review = read_u64(current->Value, 2);
        const auto actor = read_u64(current->Value, 3);
        const auto commit =
            read_oid<core::git_commit_oid>(current->Value, 6, 7);
        if(!association || !snapshot || !review || !actor || !commit)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        const auto exact = *association == review_fact.Association
            && *snapshot == request.AcceptedSnapshot.value()
            && *review == review_fact.Review.value()
            && *actor == review_fact.Actor.value()
            && ::sqlite3_column_int(current->Value, 4)
                == review_fact.Order.value()
            && ::sqlite3_column_int(current->Value, 5)
                == stored_enum(review_fact.State)
            && *commit == review_fact.Commit
            && ::sqlite3_column_int(current->Value, 8)
                == stored_enum(review_fact.Availability);
        if(::sqlite3_step(current->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(exact)
        {
            return app::catalog_commit_result{
                .Catalog = Binding.Catalog,
                .Changed = false,
            };
        }
    }
    else if(current_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, stop));
    }
    auto persisted = persist_review(
        database,
        Binding.Catalog.Incarnation,
        Binding.Authority.Scope,
        review_fact,
        request.AcceptedSnapshot,
        canonical->LinkedOrdinal,
        stop);
    if(!persisted)
    {
        return std::unexpected(persisted.error());
    }
    auto revision = prepare(
        database,
        "UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND catalog_revision = ?3 AND lifecycle = 1 "
        "AND catalog_revision < 9223372036854775807");
    if(!revision
       || !bind_fixed(revision->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(revision->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              revision->Value, 3, request.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(revision->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    Binding.Catalog.Revision = core::epoch_catalog_revision{
        request.Catalog.Revision.value() + 1};
    return app::catalog_commit_result{
        .Catalog = Binding.Catalog,
        .Changed = true,
    };
}

auto sqlite_epoch_catalog_session::begin_prune(
    const app::begin_prune_request& request,
    std::stop_token stop) -> result<app::maintenance_operation_record>
{
    if(!app::valid(request)
       || request.Catalog.Incarnation != Binding.Catalog.Incarnation
       || std::ranges::any_of(request.Epochs, [this](const auto& value) {
              return value.Epoch.Authority != Binding.Authority.Scope;
          }))
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    const auto verify_anchors = [&](const auto& facts, const bool finalized)
        -> result<void> {
        for(const auto& fact : facts)
        {
            auto epoch = load_epoch_summary(
                database,
                Binding.Catalog.Incarnation,
                Binding.Authority.Scope,
                fact.Epoch.Ordinal);
            if(!epoch || epoch->Base != fact.Retained.Base
               || epoch->Head != fact.Retained.Head
               || epoch->BaseAvailability
                   != (finalized
                           ? core::anchor_availability::pruned
                           : fact.ExpectedBaseAvailability)
               || epoch->HeadAvailability
                   != (finalized
                           ? core::anchor_availability::pruned
                           : fact.ExpectedHeadAvailability))
            {
                return std::unexpected(failure::authority_mismatch);
            }
            if(finalized)
            {
                auto nonpruned_reviews = linked_review_count(
                    database,
                    Binding.Catalog.Incarnation,
                    fact,
                    "availability != 3");
                if(!nonpruned_reviews || *nonpruned_reviews != 0)
                {
                    return std::unexpected(failure::operation_conflict);
                }
            }
        }
        return {};
    };
    auto existing = load_maintenance(request.Operation);
    if(existing)
    {
        if(existing->Kind == app::epoch_catalog_operation_kind::prune
           && existing->Epochs == request.Epochs)
        {
            auto expected = prepare(
                database,
                "SELECT expected_revision FROM operation "
                "WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
                "AND cache_scope = ?3 AND pr_scope = ?4");
            if(!expected
               || !bind_fixed(
                   expected->Value, 1, Binding.Catalog.Incarnation)
               || !bind_operation(expected->Value, 2, request.Operation)
               || !bind_fixed(
                   expected->Value, 3, Binding.Authority.Cache)
               || !bind_fixed(
                   expected->Value, 4, Binding.Authority.Scope)
               || ::sqlite3_step(expected->Value) != SQLITE_ROW
               || ::sqlite3_column_type(expected->Value, 0)
                   != SQLITE_INTEGER
               || ::sqlite3_column_int64(expected->Value, 0)
                   != request.Catalog.Revision.value()
               || ::sqlite3_step(expected->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::operation_conflict);
            }
            const auto finalized = existing->Phase
                    == app::epoch_catalog_operation_phase::
                        visible_cleanup_pending
                || existing->Phase
                    == app::epoch_catalog_operation_phase::complete;
            if(finalized)
            {
                auto scope = load_pull_request_scope_state(
                    database,
                    Binding.Catalog.Incarnation,
                    Binding.Authority.Scope);
                if(!scope || scope->Cache != Binding.Authority.Cache
                   || scope->Lifecycle != 1
                   || scope->RepositoryLifecycle != 1
                   || scope->Revision != Binding.Catalog.Revision
                   || !advanced_once(
                       request.Catalog.Revision, scope->Revision))
                {
                    return std::unexpected(failure::operation_conflict);
                }
            }
            else
            {
                if(request.Catalog != Binding.Catalog)
                {
                    return std::unexpected(failure::operation_conflict);
                }
                if(auto active = require_active_pull_request_scope(
                       database, Binding, request.Catalog.Revision, true);
                   !active)
                {
                    return std::unexpected(active.error());
                }
            }
            if(auto verified = verify_anchors(existing->Epochs, finalized);
               !verified)
            {
                return std::unexpected(failure::operation_conflict);
            }
            return existing;
        }
        return std::unexpected(failure::operation_conflict);
    }
    if(request.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(auto active = require_active_pull_request_scope(
           database, Binding, request.Catalog.Revision, true);
       !active)
    {
        return std::unexpected(active.error());
    }
    auto live_scope = prepare(
        database,
        "SELECT 1 FROM pull_request_scope WHERE incarnation = ?1 "
        "AND pr_scope = ?2 AND catalog_revision = ?3 AND lifecycle = 1");
    if(!live_scope
       || !bind_fixed(
           live_scope->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(live_scope->Value, 2, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              live_scope->Value, 3, request.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(live_scope->Value) != SQLITE_ROW
       || ::sqlite3_step(live_scope->Value) != SQLITE_DONE)
    {
        return std::unexpected(failure::revision_conflict);
    }
    if(auto verified = verify_anchors(request.Epochs, false); !verified)
    {
        return std::unexpected(verified.error());
    }
    auto operation = prepare(
        database,
        "INSERT INTO operation(incarnation, operation, kind, phase, "
        "last_evidence, operation_revision, cache_scope, pr_scope, "
        "expected_revision, application_session, request_id, "
        "epoch_generation, source_observation, source_snapshot, "
        "identity_generation, pull_request_generation, reserved_ordinal, "
        "detail_count, "
        "retained_epoch_count, terminal) "
        "VALUES(?1, ?2, 2, 13, 1, 1, ?3, ?4, ?5, ?6, NULL, NULL, "
        "NULL, NULL, NULL, NULL, NULL, ?7, 0, 0)");
    if(!operation
       || !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(operation->Value, 2, request.Operation)
       || !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
       || !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              operation->Value, 5, request.Catalog.Revision.value())
            != SQLITE_OK
       || !bind_u64(operation->Value, 6, ApplicationSession.value())
       || ::sqlite3_bind_int64(
              operation->Value,
              7,
              static_cast<std::int64_t>(request.Epochs.size()))
            != SQLITE_OK
       || ::sqlite3_step(operation->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(
            database, stop, failure::operation_conflict));
    }
    auto detail = prepare(
        database,
        "INSERT INTO prune_operation(incarnation, operation, pr_scope, "
        "ordinal, base_algorithm, base_digest, base_availability, "
        "head_algorithm, head_digest, head_availability, ref_format) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1)");
    if(!detail)
    {
        return std::unexpected(detail.error());
    }
    for(const auto& fact : request.Epochs)
    {
        if(::sqlite3_reset(detail->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
           || !bind_fixed(
               detail->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(detail->Value, 2, request.Operation)
           || !bind_fixed(detail->Value, 3, fact.Epoch.Authority)
           || ::sqlite3_bind_int64(
                  detail->Value, 4, fact.Epoch.Ordinal.value())
               != SQLITE_OK
           || !bind_oid(detail->Value, 5, 6, fact.Retained.Base)
           || ::sqlite3_bind_int(
                  detail->Value,
                  7,
                  stored_enum(fact.ExpectedBaseAvailability))
               != SQLITE_OK
           || !bind_oid(detail->Value, 8, 9, fact.Retained.Head)
           || ::sqlite3_bind_int(
                  detail->Value,
                  10,
                  stored_enum(fact.ExpectedHeadAvailability))
               != SQLITE_OK
           || ::sqlite3_step(detail->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, stop));
        }
    }
    auto output = load_maintenance(request.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::finalize_prune(
    const app::finalize_prune_request& request,
    std::stop_token stop) -> result<app::catalog_commit_result>
{
    if(!app::valid(request)
       || request.Transition.Catalog.Incarnation
           != Binding.Catalog.Incarnation)
    {
        return std::unexpected(failure::invalid_request);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, {}};
    transaction write{database, {}};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto operation = load_maintenance(request.Transition.Operation);
    if(!operation
       || operation->Kind != app::epoch_catalog_operation_kind::prune)
    {
        return std::unexpected(failure::phase_conflict);
    }
    std::vector<app::git_retained_ref_pair> expected;
    expected.reserve(operation->Epochs.size());
    for(const auto& epoch : operation->Epochs)
    {
        expected.push_back(epoch.Retained);
    }
    if(expected != request.ObservedAbsent)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(
        operation->Phase
            == app::epoch_catalog_operation_phase::visible_cleanup_pending
        || operation->Phase == app::epoch_catalog_operation_phase::complete)
    {
        auto scope = load_pull_request_scope_state(
            database,
            Binding.Catalog.Incarnation,
            Binding.Authority.Scope);
        if(!scope || scope->Cache != Binding.Authority.Cache
           || scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
           || !advanced_once(
               request.Transition.Catalog.Revision, scope->Revision)
           || !finalized_operation_revision_matches(
               operation->Phase,
               request.Transition.ExpectedOperationRevision,
               operation->OperationRevision,
               app::epoch_catalog_operation_phase::
                   visible_cleanup_pending))
        {
            return std::unexpected(failure::operation_conflict);
        }
        for(const auto& fact : operation->Epochs)
        {
            auto epoch = load_epoch_summary(
                database,
                Binding.Catalog.Incarnation,
                fact.Epoch.Authority,
                fact.Epoch.Ordinal);
            if(!epoch || epoch->Base != fact.Retained.Base
               || epoch->Head != fact.Retained.Head
               || epoch->BaseAvailability
                   != core::anchor_availability::pruned
               || epoch->HeadAvailability
                   != core::anchor_availability::pruned)
            {
                return std::unexpected(failure::operation_conflict);
            }
            auto nonpruned_reviews = linked_review_count(
                database,
                Binding.Catalog.Incarnation,
                fact,
                "availability != 3");
            if(!nonpruned_reviews || *nonpruned_reviews != 0)
            {
                return std::unexpected(failure::operation_conflict);
            }
        }
        Binding.Catalog.Revision = scope->Revision;
        return app::catalog_commit_result{
            .Catalog = Binding.Catalog,
            .Changed = true,
        };
    }
    if(stop.stop_requested()
       && !app::point_of_no_cancellation(operation->Phase))
    {
        return std::unexpected(failure::cancelled);
    }
    if(request.Transition.Catalog != Binding.Catalog)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(auto active = require_active_pull_request_scope(
           database,
           Binding,
           request.Transition.Catalog.Revision,
           true);
       !active)
    {
        return std::unexpected(active.error());
    }
    if(operation->Phase
           != app::epoch_catalog_operation_phase::ref_delete_in_progress
       || operation->OperationRevision
           != request.Transition.ExpectedOperationRevision)
    {
        return std::unexpected(failure::phase_conflict);
    }
    auto anchor = prepare(
        database,
        "UPDATE epoch_anchor SET availability = ?1 "
        "WHERE incarnation = ?2 AND pr_scope = ?3 AND ordinal = ?4 "
        "AND endpoint = ?5 AND object_algorithm = ?6 "
        "AND object_digest = ?7 AND availability = ?8");
    if(!anchor)
    {
        return std::unexpected(anchor.error());
    }
    auto reviews = prepare(
        database,
        "UPDATE review_association SET availability = ?1 "
        "WHERE incarnation = ?2 AND pr_scope = ?3 "
        "AND linked_ordinal = ?4 AND commit_algorithm = ?5 "
        "AND commit_digest = ?6 AND availability IN (1, 2, 5, 6)");
    if(!reviews)
    {
        return std::unexpected(reviews.error());
    }
    for(const auto& fact : operation->Epochs)
    {
        auto destructive_reviews = linked_review_count(
            database,
            Binding.Catalog.Incarnation,
            fact,
            "availability IN (3, 4)");
        if(!destructive_reviews || *destructive_reviews != 0)
        {
            return std::unexpected(failure::operation_conflict);
        }
        const std::array anchors{
            std::pair{
                fact.Retained.Base,
                fact.ExpectedBaseAvailability,
            },
            std::pair{
                fact.Retained.Head,
                fact.ExpectedHeadAvailability,
            },
        };
        for(std::size_t index = 0; index < anchors.size(); ++index)
        {
            if(::sqlite3_reset(anchor->Value) != SQLITE_OK
               || ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
               || ::sqlite3_bind_int(
                      anchor->Value,
                      1,
                      stored_enum(core::anchor_availability::pruned))
                   != SQLITE_OK
               || !bind_fixed(
                   anchor->Value, 2, Binding.Catalog.Incarnation)
               || !bind_fixed(anchor->Value, 3, fact.Epoch.Authority)
               || ::sqlite3_bind_int64(
                      anchor->Value, 4, fact.Epoch.Ordinal.value())
                   != SQLITE_OK
               || ::sqlite3_bind_int(
                      anchor->Value, 5, static_cast<int>(index + 1U))
                   != SQLITE_OK
               || !bind_oid(anchor->Value, 6, 7, anchors[index].first)
               || ::sqlite3_bind_int(
                      anchor->Value,
                      8,
                      stored_enum(anchors[index].second))
                   != SQLITE_OK
               || ::sqlite3_step(anchor->Value) != SQLITE_DONE
               || ::sqlite3_changes(database) != 1)
            {
                return std::unexpected(failure::operation_conflict);
            }
        }
        if(::sqlite3_reset(reviews->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
           || ::sqlite3_bind_int(
                  reviews->Value,
                  1,
                  stored_enum(core::anchor_availability::pruned))
               != SQLITE_OK
           || !bind_fixed(
               reviews->Value, 2, Binding.Catalog.Incarnation)
           || !bind_fixed(reviews->Value, 3, fact.Epoch.Authority)
           || ::sqlite3_bind_int64(
                  reviews->Value, 4, fact.Epoch.Ordinal.value())
               != SQLITE_OK
           || !bind_oid(reviews->Value, 5, 6, fact.Retained.Head)
           || ::sqlite3_step(reviews->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
        auto nonpruned_reviews = linked_review_count(
            database,
            Binding.Catalog.Incarnation,
            fact,
            "availability != 3");
        if(!nonpruned_reviews || *nonpruned_reviews != 0)
        {
            return std::unexpected(failure::operation_conflict);
        }
    }
    auto revision = prepare(
        database,
        "UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
        "retained_epoch_count = retained_epoch_count - ?1 "
        "WHERE incarnation = ?2 AND pr_scope = ?3 "
        "AND catalog_revision = ?4 AND lifecycle = 1 "
        "AND retained_epoch_count >= ?1 "
        "AND catalog_revision < 9223372036854775807");
    if(!revision
       || ::sqlite3_bind_int64(
              revision->Value,
              1,
              static_cast<std::int64_t>(operation->Epochs.size()))
            != SQLITE_OK
       || !bind_fixed(revision->Value, 2, Binding.Catalog.Incarnation)
       || !bind_fixed(revision->Value, 3, Binding.Authority.Scope)
       || ::sqlite3_bind_int64(
              revision->Value, 4, Binding.Catalog.Revision.value())
            != SQLITE_OK
       || ::sqlite3_step(revision->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto advance = prepare(
        database,
        "UPDATE operation SET phase = 8, last_evidence = 6, "
        "operation_revision = operation_revision + 1 "
        "WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
        "AND phase = 14 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope = ?5 "
        "AND operation_revision < 9223372036854775806");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance->Value, 2, request.Transition.Operation)
       || ::sqlite3_bind_int64(
              advance->Value,
              3,
              request.Transition.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
       || !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    Binding.Catalog.Revision = core::epoch_catalog_revision{
        request.Transition.Catalog.Revision.value() + 1};
    return app::catalog_commit_result{
        .Catalog = Binding.Catalog,
        .Changed = true,
    };
}

[[nodiscard]] auto load_cache_impact(
    ::sqlite3* database,
    const core::epoch_catalog_incarnation_id& incarnation,
    const core::repository_cache_scope_id& cache)
    -> result<app::cache_delete_impact>
{
    app::cache_delete_impact output{
        .Incarnation = incarnation,
        .Cache = cache,
    };
    auto scopes = prepare(
        database,
        "SELECT p.pr_scope, p.catalog_revision, p.retained_epoch_count, "
        "(SELECT count(*) FROM epoch e "
        " JOIN epoch_anchor a ON a.incarnation = e.incarnation "
        " AND a.pr_scope = e.pr_scope AND a.ordinal = e.ordinal "
        " AND a.endpoint = 1 AND a.availability NOT IN (3, 4) "
        " WHERE e.incarnation = p.incarnation AND e.pr_scope = p.pr_scope), "
        "p.lifecycle, r.lifecycle "
        "FROM pull_request_scope p JOIN repository_scope r "
        "ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
        "WHERE p.incarnation = ?1 AND p.cache_scope = ?2 "
        "ORDER BY p.pr_scope LIMIT 10001");
    if(!scopes || !bind_fixed(scopes->Value, 1, incarnation)
       || !bind_fixed(scopes->Value, 2, cache))
    {
        return std::unexpected(scopes ? failure::unavailable : scopes.error());
    }
    while(true)
    {
        const auto code = ::sqlite3_step(scopes->Value);
        if(code == SQLITE_DONE)
        {
            break;
        }
        if(code != SQLITE_ROW)
        {
            return std::unexpected(sql_failure(database, {}));
        }
        auto authority = read_fixed<core::pull_request_authority_scope_id>(
            scopes->Value, 0);
        const auto revision = ::sqlite3_column_int64(scopes->Value, 1);
        const auto retained = ::sqlite3_column_int64(scopes->Value, 2);
        const auto count = ::sqlite3_column_int64(scopes->Value, 3);
        const auto pr_lifecycle = ::sqlite3_column_int(scopes->Value, 4);
        const auto repository_lifecycle =
            ::sqlite3_column_int(scopes->Value, 5);
        if(!authority || revision < 0 || retained < 0 || count < 0
           || retained != count
           || output.RetainedEpochCount
               > std::numeric_limits<std::uint64_t>::max()
                   - static_cast<std::uint64_t>(count))
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(repository_lifecycle != 1 && repository_lifecycle != 2)
        {
            return std::unexpected(failure::authority_mismatch);
        }
        if(pr_lifecycle != 1 && pr_lifecycle != 4)
        {
            return std::unexpected(failure::repair_required);
        }
        if(output.Scopes.size() >= app::maximum_cache_delete_scope_count)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        output.Scopes.push_back(app::cache_delete_scope_fact{
            .Authority = *authority,
            .ExpectedRevision = core::epoch_catalog_revision{revision},
        });
        output.RetainedEpochCount += static_cast<std::uint64_t>(count);
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::authority_mismatch);
    }
    return output;
}

auto sqlite_epoch_catalog_session::cache_delete_impact(
    const core::repository_cache_scope_id cache,
    std::stop_token stop) -> result<app::cache_delete_impact>
{
    if(!cache.valid() || cache != Binding.Authority.Cache)
    {
        return std::unexpected(failure::authority_mismatch);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    progress_guard progress{State->Database.handle(), stop};
    return load_cache_impact(
        State->Database.handle(), Binding.Catalog.Incarnation, cache);
}

auto sqlite_epoch_catalog_session::begin_cache_delete(
    const app::begin_cache_delete_request& request,
    std::stop_token stop) -> result<app::maintenance_operation_record>
{
    if(!app::valid(request)
       || request.Impact.Incarnation != Binding.Catalog.Incarnation
       || request.Impact.Cache != Binding.Authority.Cache)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    transaction write{database, stop};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    if(request.Impact.RetainedEpochCount
       > static_cast<std::uint64_t>(
           std::numeric_limits<std::int64_t>::max()))
    {
        return std::unexpected(failure::quota_exceeded);
    }
    auto existing = load_maintenance(request.Operation);
    if(existing)
    {
        if(existing->Kind == app::epoch_catalog_operation_kind::cache_delete
           && existing->Cache == request.Impact.Cache
           && existing->Scopes == request.Impact.Scopes)
        {
            auto retained = prepare(
                database,
                "SELECT retained_epoch_count FROM operation "
                "WHERE incarnation = ?1 AND operation = ?2 "
                "AND kind = 3 AND cache_scope = ?3 AND pr_scope IS NULL");
            if(!retained
               || !bind_fixed(
                   retained->Value, 1, Binding.Catalog.Incarnation)
               || !bind_operation(retained->Value, 2, request.Operation)
               || !bind_fixed(retained->Value, 3, request.Impact.Cache)
               || ::sqlite3_step(retained->Value) != SQLITE_ROW
               || ::sqlite3_column_int64(retained->Value, 0)
                   != static_cast<std::int64_t>(
                       request.Impact.RetainedEpochCount)
               || ::sqlite3_step(retained->Value) != SQLITE_DONE)
            {
                return std::unexpected(failure::operation_conflict);
            }
            return existing;
        }
        return std::unexpected(failure::operation_conflict);
    }
    if(auto idle = require_no_blocking_operation(database, Binding, true);
       !idle)
    {
        return std::unexpected(idle.error());
    }
    auto current = load_cache_impact(
        database, Binding.Catalog.Incarnation, request.Impact.Cache);
    if(!current || *current != request.Impact)
    {
        return std::unexpected(failure::revision_conflict);
    }
    for(const auto& scope : request.Impact.Scopes)
    {
        auto state = load_pull_request_scope_state(
            database, Binding.Catalog.Incarnation, scope.Authority);
        if(!state || state->Cache != request.Impact.Cache
           || (state->Lifecycle != 1 && state->Lifecycle != 4)
           || (state->RepositoryLifecycle != 1
               && state->RepositoryLifecycle != 2))
        {
            return std::unexpected(
                state ? failure::authority_mismatch : state.error());
        }
        if(state->Revision != scope.ExpectedRevision
           || state->Revision.value()
               >= core::maximum_epoch_catalog_revision)
        {
            return std::unexpected(failure::revision_conflict);
        }
    }
    auto operation = prepare(
        database,
        "INSERT INTO operation(incarnation, operation, kind, phase, "
        "last_evidence, operation_revision, cache_scope, pr_scope, "
        "expected_revision, application_session, request_id, "
        "epoch_generation, source_observation, source_snapshot, "
        "identity_generation, pull_request_generation, reserved_ordinal, "
        "detail_count, "
        "retained_epoch_count, terminal) "
        "VALUES(?1, ?2, 3, 16, 1, 1, ?3, NULL, NULL, ?4, NULL, NULL, "
        "NULL, NULL, NULL, NULL, NULL, ?5, ?6, 0)");
    if(!operation
       || !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(operation->Value, 2, request.Operation)
       || !bind_fixed(operation->Value, 3, request.Impact.Cache)
       || !bind_u64(operation->Value, 4, ApplicationSession.value())
       || ::sqlite3_bind_int64(
              operation->Value,
              5,
              static_cast<std::int64_t>(request.Impact.Scopes.size()))
            != SQLITE_OK
       || ::sqlite3_bind_int64(
              operation->Value,
              6,
              static_cast<std::int64_t>(request.Impact.RetainedEpochCount))
            != SQLITE_OK
       || ::sqlite3_step(operation->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(
            database, stop, failure::operation_conflict));
    }
    auto detail = prepare(
        database,
        "INSERT INTO cache_delete_scope(incarnation, operation, cache_scope, "
        "pr_scope, expected_revision, epoch_count) "
        "VALUES(?1, ?2, ?3, ?4, ?5, ?6)");
    if(!detail)
    {
        return std::unexpected(detail.error());
    }
    for(const auto& scope : request.Impact.Scopes)
    {
        auto count_query = prepare(
            database,
            "SELECT count(*) FROM epoch e JOIN epoch_anchor a "
            "ON a.incarnation = e.incarnation AND a.pr_scope = e.pr_scope "
            "AND a.ordinal = e.ordinal AND a.endpoint = 1 "
            "AND a.availability NOT IN (3, 4) "
            "WHERE e.incarnation = ?1 AND e.pr_scope = ?2");
        if(!count_query
           || !bind_fixed(
               count_query->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(count_query->Value, 2, scope.Authority)
           || ::sqlite3_step(count_query->Value) != SQLITE_ROW)
        {
            return std::unexpected(failure::unavailable);
        }
        const auto count = ::sqlite3_column_int64(count_query->Value, 0);
        if(count < 0 || ::sqlite3_step(count_query->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        if(::sqlite3_reset(detail->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
           || !bind_fixed(
               detail->Value, 1, Binding.Catalog.Incarnation)
           || !bind_operation(detail->Value, 2, request.Operation)
           || !bind_fixed(detail->Value, 3, request.Impact.Cache)
           || !bind_fixed(detail->Value, 4, scope.Authority)
           || ::sqlite3_bind_int64(
                  detail->Value, 5, scope.ExpectedRevision.value())
               != SQLITE_OK
           || ::sqlite3_bind_int64(detail->Value, 6, count) != SQLITE_OK
           || ::sqlite3_step(detail->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, stop));
        }
    }
    auto mark = prepare(
        database,
        "UPDATE repository_scope SET lifecycle = 2 "
        "WHERE incarnation = ?1 AND cache_scope = ?2 "
        "AND lifecycle IN (1, 2)");
    if(!mark || !bind_fixed(mark->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(mark->Value, 2, request.Impact.Cache)
       || ::sqlite3_step(mark->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto output = load_maintenance(request.Operation);
    if(!output)
    {
        return std::unexpected(output.error());
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}

auto sqlite_epoch_catalog_session::finalize_cache_delete(
    const app::finalize_cache_delete_request& request,
    std::stop_token stop) -> result<void>
{
    if(!app::valid(request)
       || request.Transition.Catalog.Incarnation
           != Binding.Catalog.Incarnation)
    {
        return std::unexpected(failure::invalid_request);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, {}};
    transaction write{database, {}};
    if(auto begun = write.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto operation = load_maintenance(request.Transition.Operation);
    if(!operation
       || operation->Kind
           != app::epoch_catalog_operation_kind::cache_delete
       || operation->Cache != Binding.Authority.Cache)
    {
        return std::unexpected(failure::phase_conflict);
    }
    if(
        operation->Phase
            == app::epoch_catalog_operation_phase::
                catalog_tombstoned_cleanup_pending
        || operation->Phase == app::epoch_catalog_operation_phase::complete)
    {
        if(!finalized_operation_revision_matches(
               operation->Phase,
               request.Transition.ExpectedOperationRevision,
               operation->OperationRevision,
               app::epoch_catalog_operation_phase::
                   catalog_tombstoned_cleanup_pending))
        {
            return std::unexpected(failure::operation_conflict);
        }
        auto repository = prepare(
            database,
            "SELECT lifecycle FROM repository_scope "
            "WHERE incarnation = ?1 AND cache_scope = ?2");
        const auto expected_lifecycle =
            operation->Phase == app::epoch_catalog_operation_phase::complete
            ? 4
            : 3;
        if(!repository
           || !bind_fixed(
               repository->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(repository->Value, 2, operation->Cache)
           || ::sqlite3_step(repository->Value) != SQLITE_ROW
           || ::sqlite3_column_int(repository->Value, 0)
               != expected_lifecycle
           || ::sqlite3_step(repository->Value) != SQLITE_DONE)
        {
            return std::unexpected(failure::operation_conflict);
        }
        std::optional<core::epoch_catalog_revision> bound_revision;
        for(const auto& scope_fact : operation->Scopes)
        {
            auto scope = load_pull_request_scope_state(
                database,
                Binding.Catalog.Incarnation,
                scope_fact.Authority);
            if(!scope || scope->Lifecycle != 3
               || scope->RepositoryLifecycle != expected_lifecycle
               || !advanced_once(
                   scope_fact.ExpectedRevision, scope->Revision)
               || scope->RetainedEpochCount != 0U)
            {
                return std::unexpected(failure::operation_conflict);
            }
            if(scope_fact.Authority == Binding.Authority.Scope)
            {
                bound_revision = scope->Revision;
            }
        }
        if(!bound_revision)
        {
            return std::unexpected(failure::authority_mismatch);
        }
        Binding.Catalog.Revision = *bound_revision;
        return {};
    }
    if(stop.stop_requested()
       && !app::point_of_no_cancellation(operation->Phase))
    {
        return std::unexpected(failure::cancelled);
    }
    if(request.Transition.Catalog != Binding.Catalog
       || operation->Phase
           != app::epoch_catalog_operation_phase::quarantined
       || operation->OperationRevision
           != request.Transition.ExpectedOperationRevision)
    {
        return std::unexpected(failure::phase_conflict);
    }
    for(const auto& scope_fact : operation->Scopes)
    {
        auto scope = load_pull_request_scope_state(
            database,
            Binding.Catalog.Incarnation,
            scope_fact.Authority);
        if(!scope || scope->Cache != operation->Cache
           || (scope->Lifecycle != 1 && scope->Lifecycle != 4)
           || scope->RepositoryLifecycle != 2)
        {
            return std::unexpected(
                scope ? failure::authority_mismatch : scope.error());
        }
        if(scope->Revision != scope_fact.ExpectedRevision
           || scope->Revision.value()
               >= core::maximum_epoch_catalog_revision)
        {
            return std::unexpected(failure::revision_conflict);
        }
    }
    auto saturated_repair = prepare(
        database,
        "SELECT 1 FROM operation WHERE incarnation = ?1 "
        "AND cache_scope = ?2 AND terminal = 3 "
        "AND operation_revision = 9223372036854775807 LIMIT 1");
    if(!saturated_repair
       || !bind_fixed(
           saturated_repair->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(saturated_repair->Value, 2, operation->Cache))
    {
        return std::unexpected(
            saturated_repair ? failure::unavailable
                             : saturated_repair.error());
    }
    const auto saturated_code = ::sqlite3_step(saturated_repair->Value);
    if(saturated_code == SQLITE_ROW)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(saturated_code != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    auto compact_repair_inventory = prepare(
        database,
        "DELETE FROM append_inventory WHERE incarnation = ?1 "
        "AND operation IN (SELECT operation FROM operation "
        "WHERE incarnation = ?1 AND cache_scope = ?2 "
        "AND kind = 1 AND terminal = 3)");
    auto compact_repair_receipt = prepare(
        database,
        "DELETE FROM append_receipt WHERE incarnation = ?1 "
        "AND operation IN (SELECT operation FROM operation "
        "WHERE incarnation = ?1 AND cache_scope = ?2 "
        "AND kind = 1 AND terminal = 3)");
    if(!compact_repair_inventory || !compact_repair_receipt
       || !bind_fixed(
           compact_repair_inventory->Value,
           1,
           Binding.Catalog.Incarnation)
       || !bind_fixed(
           compact_repair_inventory->Value, 2, operation->Cache)
       || ::sqlite3_step(compact_repair_inventory->Value) != SQLITE_DONE
       || !bind_fixed(
           compact_repair_receipt->Value,
           1,
           Binding.Catalog.Incarnation)
       || !bind_fixed(
           compact_repair_receipt->Value, 2, operation->Cache)
       || ::sqlite3_step(compact_repair_receipt->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    auto supersede_repair = prepare(
        database,
        "UPDATE operation SET phase = 11, last_evidence = 5, "
        "operation_revision = operation_revision + 1, terminal = 2, "
        "repair_from_phase = NULL, recovery_input_compacted = "
        "CASE WHEN kind = 1 THEN 1 ELSE 0 END WHERE incarnation = ?1 "
        "AND cache_scope = ?2 AND operation != ?3 AND terminal = 3 "
        "AND operation_revision < 9223372036854775807");
    if(!supersede_repair
       || !bind_fixed(
           supersede_repair->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(supersede_repair->Value, 2, operation->Cache)
       || !bind_operation(
           supersede_repair->Value, 3, request.Transition.Operation)
       || ::sqlite3_step(supersede_repair->Value) != SQLITE_DONE)
    {
        return std::unexpected(sql_failure(database, {}));
    }
    auto tombstone = prepare(
        database,
        "UPDATE pull_request_scope SET lifecycle = 3, "
        "catalog_revision = catalog_revision + 1, retained_epoch_count = 0 "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND catalog_revision = ?3 AND lifecycle IN (1, 4) "
        "AND catalog_revision < 9223372036854775807");
    auto anchors = prepare(
        database,
        "UPDATE epoch_anchor SET availability = 4 "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND availability != 3 AND availability != 4");
    auto reviews = prepare(
        database,
        "UPDATE review_association SET availability = 4 "
        "WHERE incarnation = ?1 AND pr_scope = ?2 "
        "AND availability != 3 AND availability != 4");
    if(!tombstone || !anchors || !reviews)
    {
        return std::unexpected(failure::unavailable);
    }
    for(const auto& scope : operation->Scopes)
    {
        if(::sqlite3_reset(anchors->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(anchors->Value) != SQLITE_OK
           || !bind_fixed(
               anchors->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(anchors->Value, 2, scope.Authority)
           || ::sqlite3_step(anchors->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
        if(::sqlite3_reset(reviews->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
           || !bind_fixed(
               reviews->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(reviews->Value, 2, scope.Authority)
           || ::sqlite3_step(reviews->Value) != SQLITE_DONE)
        {
            return std::unexpected(sql_failure(database, {}));
        }
        if(::sqlite3_reset(tombstone->Value) != SQLITE_OK
           || ::sqlite3_clear_bindings(tombstone->Value) != SQLITE_OK
           || !bind_fixed(
               tombstone->Value, 1, Binding.Catalog.Incarnation)
           || !bind_fixed(tombstone->Value, 2, scope.Authority)
           || ::sqlite3_bind_int64(
                  tombstone->Value, 3, scope.ExpectedRevision.value())
               != SQLITE_OK
           || ::sqlite3_step(tombstone->Value) != SQLITE_DONE
           || ::sqlite3_changes(database) != 1)
        {
            return std::unexpected(failure::revision_conflict);
        }
    }
    auto repository = prepare(
        database,
        "UPDATE repository_scope SET lifecycle = 3 "
        "WHERE incarnation = ?1 AND cache_scope = ?2 AND lifecycle = 2");
    if(!repository
       || !bind_fixed(repository->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(repository->Value, 2, operation->Cache)
       || ::sqlite3_step(repository->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::revision_conflict);
    }
    auto advance = prepare(
        database,
        "UPDATE operation SET phase = 19, last_evidence = 1, "
        "operation_revision = operation_revision + 1 "
        "WHERE incarnation = ?1 AND operation = ?2 AND kind = 3 "
        "AND phase = 18 AND operation_revision = ?3 "
        "AND cache_scope = ?4 AND pr_scope IS NULL "
        "AND operation_revision < 9223372036854775806");
    if(!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
       || !bind_operation(advance->Value, 2, request.Transition.Operation)
       || ::sqlite3_bind_int64(
              advance->Value,
              3,
              request.Transition.ExpectedOperationRevision.value())
            != SQLITE_OK
       || !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
       || ::sqlite3_step(advance->Value) != SQLITE_DONE
       || ::sqlite3_changes(database) != 1)
    {
        return std::unexpected(failure::operation_conflict);
    }
    if(auto committed = write.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    for(const auto& scope : operation->Scopes)
    {
        if(scope.Authority == Binding.Authority.Scope)
        {
            Binding.Catalog.Revision = core::epoch_catalog_revision{
                scope.ExpectedRevision.value() + 1};
            break;
        }
    }
    return {};
}

auto sqlite_epoch_catalog_session::recovery_page(
    const std::optional<core::epoch_operation_id> after,
    const std::uint16_t limit,
    std::stop_token stop) -> result<app::epoch_recovery_page>
{
    if((after && !after->valid()) || limit == 0U
       || limit > app::maximum_epoch_recovery_page_size)
    {
        return std::unexpected(failure::invalid_request);
    }
    if(stop.stop_requested())
    {
        return std::unexpected(failure::cancelled);
    }
    std::scoped_lock lock{State->Mutex};
    auto* database = State->Database.handle();
    progress_guard progress{database, stop};
    read_transaction read{database, stop};
    if(auto begun = read.begin(); !begun)
    {
        return std::unexpected(begun.error());
    }
    auto query = prepare(
        database,
        after
            ? "SELECT o.operation, o.kind FROM operation o "
              "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
              "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
              "AND o.pr_scope = ?3) OR (o.kind = 3 "
              "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
              "FROM repository_scope current JOIN repository_scope owned "
              "ON owned.incarnation = current.incarnation "
              "AND owned.profile_id = current.profile_id "
              "AND owned.profile_revision = current.profile_revision "
              "AND owned.host_id = current.host_id "
              "AND owned.host_authority = current.host_authority "
              "AND owned.account_id = current.account_id "
              "AND owned.repository_id = current.repository_id "
              "AND owned.object_format = current.object_format "
              "WHERE current.incarnation = o.incarnation "
              "AND current.cache_scope = ?2 "
              "AND owned.cache_scope = o.cache_scope))) "
              "AND o.operation >= ?4 ORDER BY o.operation LIMIT ?5"
            : "SELECT o.operation, o.kind FROM operation o "
              "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
              "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
              "AND o.pr_scope = ?3) OR (o.kind = 3 "
              "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
              "FROM repository_scope current JOIN repository_scope owned "
              "ON owned.incarnation = current.incarnation "
              "AND owned.profile_id = current.profile_id "
              "AND owned.profile_revision = current.profile_revision "
              "AND owned.host_id = current.host_id "
              "AND owned.host_authority = current.host_authority "
              "AND owned.account_id = current.account_id "
              "AND owned.repository_id = current.repository_id "
              "AND owned.object_format = current.object_format "
              "WHERE current.incarnation = o.incarnation "
              "AND current.cache_scope = ?2 "
              "AND owned.cache_scope = o.cache_scope))) "
              "ORDER BY o.operation LIMIT ?4");
    if(!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
       || !bind_fixed(query->Value, 2, Binding.Authority.Cache)
       || !bind_fixed(query->Value, 3, Binding.Authority.Scope))
    {
        return std::unexpected(query ? failure::unavailable : query.error());
    }
    const auto fetch = static_cast<std::int64_t>(limit) + 1;
    if(after)
    {
        if(!bind_operation(query->Value, 4, *after)
           || ::sqlite3_bind_int64(query->Value, 5, fetch) != SQLITE_OK)
        {
            return std::unexpected(failure::unavailable);
        }
    }
    else if(::sqlite3_bind_int64(query->Value, 4, fetch) != SQLITE_OK)
    {
        return std::unexpected(failure::unavailable);
    }
    std::vector<std::pair<core::epoch_operation_id, int>> identities;
    while(true)
    {
        const auto code = ::sqlite3_step(query->Value);
        if(code == SQLITE_DONE)
        {
            break;
        }
        if(code != SQLITE_ROW || stop.stop_requested())
        {
            return std::unexpected(sql_failure(database, stop));
        }
        auto operation =
            read_fixed<core::epoch_operation_id>(query->Value, 0);
        if(!operation)
        {
            return std::unexpected(failure::schema_mismatch);
        }
        identities.emplace_back(
            *operation, ::sqlite3_column_int(query->Value, 1));
    }
    app::epoch_recovery_page output{
        .Incarnation = Binding.Catalog.Incarnation,
    };
    if(identities.size() > limit)
    {
        output.Continuation = identities.back().first;
        identities.pop_back();
    }
    output.Operations.reserve(identities.size());
    for(const auto& [operation, kind] : identities)
    {
        if(kind == static_cast<int>(app::epoch_catalog_operation_kind::append))
        {
            auto loaded = load_append(operation);
            if(!loaded)
            {
                return std::unexpected(loaded.error());
            }
            output.Operations.emplace_back(*loaded);
        }
        else
        {
            auto loaded = load_maintenance(operation);
            if(!loaded)
            {
                return std::unexpected(loaded.error());
            }
            output.Operations.emplace_back(*loaded);
        }
    }
    if(!app::valid(output))
    {
        return std::unexpected(failure::schema_mismatch);
    }
    if(auto committed = read.commit(); !committed)
    {
        return std::unexpected(committed.error());
    }
    return output;
}
} // namespace

auto make_epoch_catalog(
    const epoch_catalog_configuration& configuration,
    std::stop_token stop)
    -> app::epoch_catalog_result<std::unique_ptr<app::epoch_catalog_port>>
{
    auto database = detail::catalog_database::open(
        configuration.StorageRoot, stop);
    if(!database)
    {
        return std::unexpected(map_failure(database.error()));
    }
    auto state = std::make_shared<catalog_state>(std::move(*database));
    return std::make_unique<sqlite_epoch_catalog_port>(std::move(state));
}
} // namespace bpr::adapters::sqlite
