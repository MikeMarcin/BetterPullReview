#include "sqlite_connection.hpp"

#include <sqlite3.h>

#include <array>
#include <cctype>
#include <stop_token>

namespace bpr::adapters::sqlite::detail {
	namespace {
		auto progress_callback(void* aOpaque) noexcept -> int
		{
			const auto* stop = static_cast<const std::stop_token*>(aOpaque);
			return stop != nullptr && stop->stop_requested() ? 1 : 0;
		}

		// The calling thread's innermost progress handler scope, on any
		// connection. Each scope links to the one it enclosed.
		thread_local progress_handler_scope* innermostScope = nullptr;
	} // namespace

	progress_handler_scope::progress_handler_scope(
		::sqlite3* aDatabase,
		int        aInterval,
		handler    aHandler,
		void*      aContext) noexcept
		: Database{ aDatabase }
		, Interval{ aInterval }
		, Handler{ aHandler }
		, Context{ aContext }
		, Outer{ std::exchange(innermostScope, this) }
	{
		::sqlite3_progress_handler(Database, Interval, Handler, Context);
	}

	progress_handler_scope::~progress_handler_scope()
	{
		innermostScope        = Outer;
		const auto* enclosing = Outer;
		while (enclosing != nullptr && enclosing->Database != Database) {
			enclosing = enclosing->Outer;
		}
		if (enclosing == nullptr) {
			::sqlite3_progress_handler(Database, 0, nullptr, nullptr);
			return;
		}
		::sqlite3_progress_handler(
			Database, enclosing->Interval, enclosing->Handler, enclosing->Context);
	}

	progress_scope::progress_scope(::sqlite3* aDatabase, std::stop_token aStop)
		: Stop{ std::move(aStop) }
		, Handler{ aDatabase, 256, progress_callback, &Stop }
	{
	}

	namespace {
		class runtime final
		{
		public:
			runtime() noexcept
				: Result{ ::sqlite3_initialize() }
			{
			}
			runtime(const runtime&)                    = delete;
			auto operator=(const runtime&) -> runtime& = delete;
			runtime(runtime&&)                         = delete;
			auto operator=(runtime&&) -> runtime&      = delete;
			~runtime()
			{
				if (Result == SQLITE_OK) {
					(void)::sqlite3_shutdown();
				}
			}

			int Result = SQLITE_ERROR;
		};
	} // namespace

	auto sqlite_runtime_ready() noexcept -> bool
	{
		static runtime value;
		return value.Result == SQLITE_OK;
	}

	statement::statement(statement&& aOther) noexcept
		: Value{ std::exchange(aOther.Value, nullptr) }
	{
	}

	auto statement::operator=(statement&& aOther) noexcept -> statement&
	{
		if (this != &aOther) {
			if (Value != nullptr) {
				(void)::sqlite3_finalize(Value);
			}
			Value = std::exchange(aOther.Value, nullptr);
		}
		return *this;
	}

	statement::~statement()
	{
		if (Value != nullptr) {
			(void)::sqlite3_finalize(Value);
		}
	}

	[[nodiscard]] auto catalog_failure_from_sqlite_result(
		const int     aResultCode,
		const failure aFallback) noexcept -> failure
	{
		switch (classify_sqlite_result(aResultCode)) {
		case sqlite_result_classification::integrity_failure:
			return failure::integrity_failure;
		case sqlite_result_classification::cancelled:
			return failure::cancelled;
		case sqlite_result_classification::quota_exceeded:
			return failure::quota_exceeded;
		case sqlite_result_classification::busy:
		case sqlite_result_classification::unavailable:
			return aFallback;
		}
		return aFallback;
	}

	[[nodiscard]] auto catalog_sql_failure(
		::sqlite3*    aDatabase,
		const failure aFallback) noexcept -> failure
	{
		return catalog_failure_from_sqlite_result(
			::sqlite3_extended_errcode(aDatabase), aFallback);
	}

	[[nodiscard]] auto prepare(
		::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<statement, failure>
	{
		statement   result;
		const char* tail = nullptr;
		const auto  code = ::sqlite3_prepare_v3(
			aDatabase,
			aSql.data(),
			static_cast<int>(aSql.size()),
			SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB,
			&result.Value,
			&tail);
		if (code != SQLITE_OK || result.Value == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}
		while (tail != aSql.data() + aSql.size()) {
			if (std::isspace(static_cast<unsigned char>(*tail)) == 0) {
				return std::unexpected(failure::schema_mismatch);
			}
			++tail;
		}
		return result;
	}

	[[nodiscard]] auto execute(
		::sqlite3*       aDatabase,
		std::string_view aSql,
		std::stop_token  aStop) -> std::expected<void, failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		progress_scope progress{ aDatabase, aStop };
		auto           prepared = prepare(aDatabase, aSql);
		if (!prepared) {
			return std::unexpected(prepared.error());
		}
		const auto code = ::sqlite3_step(prepared->Value);
		if (code == SQLITE_INTERRUPT || aStop.stop_requested()) {
			return std::unexpected(failure::cancelled);
		}
		if (code != SQLITE_DONE && code != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(aDatabase));
		}
		if (code == SQLITE_ROW) {
			const auto done = ::sqlite3_step(prepared->Value);
			if (done != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(aDatabase));
			}
		}
		return { };
	}

	[[nodiscard]] auto scalar_integer(
		::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<std::int64_t, failure>
	{
		auto prepared = prepare(aDatabase, aSql);
		if (!prepared) {
			return std::unexpected(prepared.error());
		}
		const auto row = ::sqlite3_step(prepared->Value);
		if (row != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(
				aDatabase, failure::schema_mismatch));
		}
		if (::sqlite3_column_type(prepared->Value, 0) != SQLITE_INTEGER) {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto result = ::sqlite3_column_int64(prepared->Value, 0);
		if (::sqlite3_step(prepared->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(
				aDatabase, failure::schema_mismatch));
		}
		return result;
	}

	[[nodiscard]] auto scalar_text(
		::sqlite3* aDatabase, std::string_view aSql)
		-> std::expected<std::string, failure>
	{
		auto prepared = prepare(aDatabase, aSql);
		if (!prepared) {
			return std::unexpected(prepared.error());
		}
		const auto row = ::sqlite3_step(prepared->Value);
		if (row != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(
				aDatabase, failure::schema_mismatch));
		}
		if (::sqlite3_column_type(prepared->Value, 0) != SQLITE_TEXT) {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto* text = reinterpret_cast<const char*>(
			::sqlite3_column_text(prepared->Value, 0));
		const auto bytes = ::sqlite3_column_bytes(prepared->Value, 0);
		if (text == nullptr || bytes < 0) {
			return std::unexpected(failure::schema_mismatch);
		}
		std::string result{ text, static_cast<std::size_t>(bytes) };
		if (::sqlite3_step(prepared->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(
				aDatabase, failure::schema_mismatch));
		}
		return result;
	}

	[[nodiscard]] auto canonical_sql(std::string_view aSql) -> std::string
	{
		std::string result;
		result.reserve(aSql.size());
		for (const auto value : aSql) {
			if (std::isspace(static_cast<unsigned char>(value)) == 0
				&& value != ';') {
				result.push_back(static_cast<char>(std::tolower(
					static_cast<unsigned char>(value))));
			}
		}
		return result;
	}

	[[nodiscard]] auto configure_connection(::sqlite3* aDatabase)
		-> std::expected<void, failure>
	{
		constexpr std::int64_t catalogPageSize         = 4'096;
		constexpr std::int64_t catalogMaximumPageCount = 262'144;

		::sqlite3_extended_result_codes(aDatabase, 1);
		(void)::sqlite3_busy_timeout(aDatabase, 2'000);

		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_LENGTH, static_cast<int>(1U << 20U));
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_SQL_LENGTH, static_cast<int>(1U << 18U));
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_COLUMN, 128);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_EXPR_DEPTH, 64);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_COMPOUND_SELECT, 8);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_VDBE_OP, 250'000);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_FUNCTION_ARG, 32);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_ATTACHED, 0);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_LIKE_PATTERN_LENGTH, 256);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_VARIABLE_NUMBER, 128);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_TRIGGER_DEPTH, 0);
		(void)::sqlite3_limit(aDatabase, SQLITE_LIMIT_WORKER_THREADS, 0);
		if (::sqlite3_limit(aDatabase, SQLITE_LIMIT_TRIGGER_DEPTH, -1) != 0) {
			return std::unexpected(failure::unavailable);
		}

		constexpr std::array configs{
			std::pair{ SQLITE_DBCONFIG_DEFENSIVE, 1 },
			std::pair{ SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0 },
			std::pair{ SQLITE_DBCONFIG_DQS_DDL, 0 },
			std::pair{ SQLITE_DBCONFIG_DQS_DML, 0 },
			std::pair{ SQLITE_DBCONFIG_ENABLE_FKEY, 1 },
			std::pair{ SQLITE_DBCONFIG_ENABLE_TRIGGER, 0 },
			std::pair{ SQLITE_DBCONFIG_ENABLE_VIEW, 0 },
			std::pair{ SQLITE_DBCONFIG_WRITABLE_SCHEMA, 0 },
			std::pair{ SQLITE_DBCONFIG_ENABLE_QPSG, 1 },
		};
		for (const auto& [setting, expected] : configs) {
			auto       actual = -1;
			const auto code   = ::sqlite3_db_config(aDatabase, setting, expected, &actual);
			if (code != SQLITE_OK) {
				return std::unexpected(catalog_failure_from_sqlite_result(code));
			}
			if (actual != expected) {
				return std::unexpected(failure::unavailable);
			}
		}
		if (::sqlite3_compileoption_used("OMIT_LOAD_EXTENSION") == 0
			|| ::sqlite3_compileoption_used("OMIT_SHARED_CACHE") == 0
			|| ::sqlite3_compileoption_used("OMIT_AUTOINIT") == 0
			|| ::sqlite3_compileoption_used("DQS=0") == 0
			|| ::sqlite3_compileoption_used("MAX_ATTACHED=0") == 0
			|| ::sqlite3_compileoption_used("MAX_PAGE_COUNT=262144") == 0) {
			return std::unexpected(failure::unavailable);
		}

		if (auto setPageSize = execute(aDatabase, "PRAGMA page_size = 4096");
			!setPageSize) {
			return setPageSize;
		}
		const auto pageSize = scalar_integer(aDatabase, "PRAGMA page_size");
		if (!pageSize) {
			return std::unexpected(pageSize.error());
		}
		if (*pageSize != catalogPageSize) {
			return std::unexpected(failure::schema_mismatch);
		}
		const auto maximumPageCount = scalar_integer(aDatabase, "PRAGMA max_page_count = 262144");
		if (!maximumPageCount) {
			return std::unexpected(maximumPageCount.error());
		}
		if (*maximumPageCount != catalogMaximumPageCount) {
			return std::unexpected(
				*maximumPageCount > catalogMaximumPageCount
					? failure::quota_exceeded
					: failure::unavailable);
		}

		constexpr std::array pragmas{
			"PRAGMA temp_store = MEMORY",
			"PRAGMA mmap_size = 0",
			"PRAGMA wal_autocheckpoint = 256",
			"PRAGMA synchronous = FULL",
		};
		for (const auto sql : pragmas) {
			if (auto result = execute(aDatabase, sql); !result) {
				return result;
			}
		}
		auto journal = scalar_text(aDatabase, "PRAGMA journal_mode = WAL");
		if (!journal) {
			return std::unexpected(journal.error());
		}
		if (*journal != "wal") {
			return std::unexpected(failure::unavailable);
		}
		auto synchronous              = scalar_integer(aDatabase, "PRAGMA synchronous");
		auto foreignKeys              = scalar_integer(aDatabase, "PRAGMA foreign_keys");
		auto trusted                  = scalar_integer(aDatabase, "PRAGMA trusted_schema");
		auto mmap                     = scalar_integer(aDatabase, "PRAGMA mmap_size");
		auto autocheckpoint           = scalar_integer(aDatabase, "PRAGMA wal_autocheckpoint");
		auto verifiedPageSize         = scalar_integer(aDatabase, "PRAGMA page_size");
		auto verifiedMaximumPageCount = scalar_integer(aDatabase, "PRAGMA max_page_count");
		if (!verifiedPageSize) {
			return std::unexpected(verifiedPageSize.error());
		}
		if (!verifiedMaximumPageCount) {
			return std::unexpected(verifiedMaximumPageCount.error());
		}
		if (*verifiedPageSize != catalogPageSize) {
			return std::unexpected(failure::schema_mismatch);
		}
		if (*verifiedMaximumPageCount != catalogMaximumPageCount) {
			return std::unexpected(
				*verifiedMaximumPageCount > catalogMaximumPageCount
					? failure::quota_exceeded
					: failure::unavailable);
		}
		if (!synchronous || *synchronous != 2 || !foreignKeys
			|| *foreignKeys != 1 || !trusted || *trusted != 0 || !mmap
			|| *mmap != 0 || !autocheckpoint || *autocheckpoint != 256) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}
} // namespace bpr::adapters::sqlite::detail
