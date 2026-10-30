#include "bpr/adapters/sqlite/storage_diagnostics.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "storage_diagnostics_internal.hpp"

#include <sqlite3.h>

#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		using core::operation_store;
		using action  = core::operation_action;
		using failure = core::operation_failure;

		struct receiver
		{
			const storage_diagnostics_scope*                       Owner = nullptr;
			std::shared_ptr<application::diagnostic_log_port>      Log;
			std::shared_ptr<application::aggregated_operation_log> Aggregate;
		};

		struct registry
		{
			std::mutex            Mutex;
			std::vector<receiver> Receivers;
			std::atomic_bool      Active{ false };
		};

		/// Never destroyed: SQLite may log while statics are torn down.
		[[nodiscard]] auto shared_registry() -> registry&
		{
			static auto* const value = new registry{ };
			return *value;
		}

		[[nodiscard]] auto current() -> receiver
		{
			auto& shared = shared_registry();
			if (!shared.Active.load(std::memory_order_relaxed)) {
				return { };
			}
			const auto lock = std::scoped_lock{ shared.Mutex };
			return shared.Receivers.empty() ? receiver{ } : shared.Receivers.back();
		}

		enum class direction : std::uint8_t {
			none,
			unknown,
			read,
			write,
		};

		// The calling thread's last prepared statement, and whether it is
		// recording now (a log never re-enters this file's recording).
		thread_local operation_store StatementStore     = operation_store::unknown;
		thread_local direction       StatementDirection = direction::none;
		thread_local bool            Recording          = false;

		class recording_guard final
		{
		public:
			recording_guard() noexcept
				: Entered{ !std::exchange(Recording, true) }
			{
			}
			~recording_guard()
			{
				if (Entered) {
					Recording = false;
				}
			}
			recording_guard(const recording_guard&)                    = delete;
			auto operator=(const recording_guard&) -> recording_guard& = delete;

			[[nodiscard]] auto entered() const noexcept -> bool { return Entered; }

		private:
			bool Entered;
		};

		void record(const core::operation_diagnosis& aDiagnosis, const bool aAggregate) noexcept
		{
			try {
				const auto target = current();
				if (aAggregate && target.Aggregate) {
					target.Aggregate->record(aDiagnosis);
				} else if (!aAggregate) {
					application::record_operation(target.Log.get(), aDiagnosis);
				}
			} catch (...) {
				// A diagnostic that cannot be formed is dropped.
			}
		}

		[[nodiscard]] auto failure_of(const detail::catalog_database_failure aFailure) noexcept -> failure
		{
			using storage = detail::catalog_database_failure;
			switch (aFailure) {
			case storage::invalid_path:         return failure::invalid_path;
			case storage::invalid_permissions:  return failure::invalid_permissions;
			case storage::unavailable:          return failure::unavailable;
			case storage::schema_mismatch:      return failure::schema_mismatch;
			case storage::integrity_failure:    return failure::integrity_failure;
			case storage::quota_exceeded:       return failure::quota_exceeded;
			case storage::cancelled:            return failure::cancelled;
			case storage::platform_unsupported: return failure::platform_unsupported;
			}
			return failure::unavailable;
		}

		/// A SQLite result code's failure; none for a code that is not one
		/// (a notice, a warning, a schema change SQLite retries itself).
		[[nodiscard]] auto failure_of_result(const int aCode) noexcept -> std::optional<failure>
		{
			switch (aCode & 0xFF) {
			case SQLITE_OK:
			case SQLITE_ROW:
			case SQLITE_DONE:
			case SQLITE_NOTICE:
			case SQLITE_WARNING:
			case SQLITE_SCHEMA:     return std::nullopt;
			case SQLITE_BUSY:       return failure::busy;
			case SQLITE_LOCKED:     return failure::locked;
			case SQLITE_READONLY:   return failure::read_only;
			case SQLITE_INTERRUPT:  return failure::cancelled;
			case SQLITE_IOERR:      return failure::io_error;
			case SQLITE_CORRUPT:    return failure::integrity_failure;
			case SQLITE_FULL:
			case SQLITE_TOOBIG:     return failure::quota_exceeded;
			case SQLITE_CANTOPEN:   return failure::cannot_open;
			case SQLITE_CONSTRAINT: return failure::constraint;
			case SQLITE_MISMATCH:   return failure::schema_mismatch;
			case SQLITE_PERM:
			case SQLITE_AUTH:       return failure::invalid_permissions;
			case SQLITE_NOTADB:     return failure::not_a_database;
			case SQLITE_ERROR:
			case SQLITE_MISUSE:
			case SQLITE_RANGE:
			case SQLITE_FORMAT:     return failure::sql_error;
			default:                return failure::unavailable;
			}
		}

		[[nodiscard]] auto store_of(::sqlite3* const aDatabase) noexcept -> operation_store
		{
			const char* const name = aDatabase == nullptr ? nullptr : ::sqlite3_db_filename(aDatabase, "main");
			if (name == nullptr) {
				return operation_store::other;
			}
			const auto path  = std::string_view{ name };
			const auto slash = path.find_last_of("/\\");
			return storage_store_of(slash == std::string_view::npos ? path : path.substr(slash + 1U));
		}

		/// The store's schema version, read directly so it is not a statement
		/// of the store's own.
		[[nodiscard]] auto schema_of(::sqlite3* const aDatabase) noexcept -> std::uint32_t
		{
			::sqlite3_stmt* statement = nullptr;
			auto            version   = std::int64_t{ 0 };
			if (::sqlite3_prepare_v2(aDatabase, "PRAGMA user_version", -1, &statement, nullptr) == SQLITE_OK && statement != nullptr && ::sqlite3_step(statement) == SQLITE_ROW) {
				version = ::sqlite3_column_int64(statement, 0);
			}
			(void)::sqlite3_finalize(statement);
			return version < 0 ? 0U : static_cast<std::uint32_t>(std::min<std::int64_t>(version, std::numeric_limits<std::uint32_t>::max()));
		}

		/// The version a `PRAGMA user_version = N` statement writes; none for
		/// any other statement.
		[[nodiscard]] auto written_schema(std::string_view aSql) noexcept -> std::optional<std::uint32_t>
		{
			constexpr auto pragma = std::string_view{ "user_version" };
			const auto     at     = aSql.find(pragma);
			if (at == std::string_view::npos) {
				return std::nullopt;
			}
			aSql.remove_prefix(at + pragma.size());
			const auto skip = [&aSql] {
				while (!aSql.empty() && std::isspace(static_cast<unsigned char>(aSql.front())) != 0) {
					aSql.remove_prefix(1U);
				}
			};
			skip();
			if (!aSql.starts_with('=')) {
				return std::nullopt;
			}
			aSql.remove_prefix(1U);
			skip();
			auto       version = std::uint32_t{ 0 };
			const auto parsed  = std::from_chars(aSql.data(), aSql.data() + aSql.size(), version);
			return parsed.ec == std::errc{ } && parsed.ptr != aSql.data() ? std::optional{ version } : std::nullopt;
		}

		/// Transaction control writes whatever `sqlite3_stmt_readonly` says.
		[[nodiscard]] auto transaction_control(std::string_view aSql) noexcept -> bool
		{
			while (!aSql.empty() && std::isspace(static_cast<unsigned char>(aSql.front())) != 0) {
				aSql.remove_prefix(1U);
			}
			const auto starts = [aSql](const std::string_view aWord) {
				return aSql.size() >= aWord.size() && std::ranges::equal(aSql.substr(0U, aWord.size()), aWord, [](const char aLeft, const char aRight) { return std::toupper(static_cast<unsigned char>(aLeft)) == aRight; });
			};
			return starts("BEGIN") || starts("COMMIT") || starts("END") || starts("ROLLBACK") || starts("SAVEPOINT") || starts("RELEASE");
		}

		/// SQLite's error log: it must not call SQLite, so the store and
		/// direction are the thread's last prepared statement's.
		void error_log(void*, const int aCode, const char*) noexcept
		{
			if (!shared_registry().Active.load(std::memory_order_relaxed) || Recording) {
				return;
			}
			const auto kind = failure_of_result(aCode);
			if (!kind) {
				return;
			}
			const auto guard = recording_guard{ };
			if ((aCode & 0xFF) == SQLITE_CANTOPEN) {
				record({ .Action = action::storage_open, .Failure = *kind, .Store = operation_store::unknown }, true);
				return;
			}
			const auto known  = StatementDirection != direction::none;
			const auto chosen = StatementDirection == direction::read ? action::storage_read
				: StatementDirection == direction::write              ? action::storage_write
																	  : action::storage_statement;
			record({ .Action = chosen, .Failure = *kind, .Store = known ? StatementStore : operation_store::unknown }, true);
		}
	} // namespace

	storage_diagnostics_scope::storage_diagnostics_scope(std::shared_ptr<application::diagnostic_log_port> aLog, const std::chrono::steady_clock::duration aWindow)
		: Log{ aLog ? std::make_shared<application::aggregated_operation_log>(aLog, aWindow) : nullptr }
	{
		if (!aLog) {
			return;
		}
		auto&      shared = shared_registry();
		const auto lock   = std::scoped_lock{ shared.Mutex };
		shared.Receivers.push_back({ .Owner = this, .Log = std::move(aLog), .Aggregate = Log });
		shared.Active.store(true, std::memory_order_relaxed);
	}

	storage_diagnostics_scope::~storage_diagnostics_scope()
	{
		if (!Log) {
			return;
		}
		{
			auto&      shared = shared_registry();
			const auto lock   = std::scoped_lock{ shared.Mutex };
			std::erase_if(shared.Receivers, [this](const receiver& aReceiver) { return aReceiver.Owner == this; });
			shared.Active.store(!shared.Receivers.empty(), std::memory_order_relaxed);
		}
		Log->flush();
	}

	void storage_diagnostics_scope::flush() noexcept
	{
		if (Log) {
			Log->flush();
		}
	}

	auto storage_store_of(const std::string_view aFileName) noexcept -> core::operation_store
	{
		constexpr auto names = std::array{
			std::pair{ std::string_view{ "agent-setup.sqlite3" }, operation_store::agent_setup },
			std::pair{ std::string_view{ "appearance-settings.sqlite3" }, operation_store::appearance_settings },
			std::pair{ std::string_view{ "diagnostic-preferences.sqlite3" }, operation_store::diagnostic_preferences },
			std::pair{ std::string_view{ "workbench-settings.sqlite3" }, operation_store::workbench_settings },
			std::pair{ std::string_view{ "review-queue.sqlite3" }, operation_store::review_queue },
			std::pair{ std::string_view{ "review-ledger.sqlite3" }, operation_store::review_ledger },
			std::pair{ std::string_view{ "review-tours.sqlite3" }, operation_store::review_tours },
			std::pair{ std::string_view{ "review-cycles.sqlite3" }, operation_store::review_cycles },
			std::pair{ std::string_view{ "review-overviews.sqlite3" }, operation_store::review_overviews },
			std::pair{ std::string_view{ "window-restoration.sqlite3" }, operation_store::window_restoration },
			std::pair{ std::string_view{ "remembered-connections.sqlite3" }, operation_store::remembered_connections },
			std::pair{ std::string_view{ "device-bindings.sqlite3" }, operation_store::device_bindings },
			std::pair{ std::string_view{ "review-mutation-recovery.sqlite3" }, operation_store::review_mutation_recovery },
			std::pair{ std::string_view{ "epoch-catalog.sqlite3" }, operation_store::epoch_catalog },
			std::pair{ std::string_view{ "markdown-images.sqlite3" }, operation_store::markdown_images },
			std::pair{ std::string_view{ "insights.sqlite3" }, operation_store::insights },
			std::pair{ std::string_view{ "profile-registry.sqlite3" }, operation_store::profile_registry },
			std::pair{ std::string_view{ "release-update.sqlite3" }, operation_store::release_update },
			std::pair{ std::string_view{ "update-draft-recovery.sqlite3" }, operation_store::update_draft_recovery },
			std::pair{ std::string_view{ "ledger.db" }, operation_store::clone_ledger },
		};
		const auto found = std::ranges::find(names, aFileName, &std::pair<std::string_view, operation_store>::first);
		return found == names.end() ? operation_store::other : found->second;
	}
} // namespace bpr::adapters::sqlite

namespace bpr::adapters::sqlite::detail {
	void install_storage_error_log() noexcept
	{
		(void)::sqlite3_config(SQLITE_CONFIG_LOG, &error_log, nullptr);
	}

	auto storage_diagnostics_active() noexcept -> bool
	{
		return shared_registry().Active.load(std::memory_order_relaxed);
	}

	void note_storage_open(::sqlite3* const aDatabase, const std::optional<catalog_database_failure> aFailure, const std::uint32_t aMilliseconds) noexcept
	{
		if (!storage_diagnostics_active()) {
			return;
		}
		const auto guard = recording_guard{ };
		if (!guard.entered()) {
			return;
		}
		record({ .Action = action::storage_open, .Failure = aFailure ? failure_of(*aFailure) : failure::none, .DurationMilliseconds = aMilliseconds, .Store = store_of(aDatabase), .Schema = aFailure ? 0U : schema_of(aDatabase) }, true);
	}

	void note_storage_refused(const std::string_view aFileName, const catalog_database_failure aFailure) noexcept
	{
		if (!storage_diagnostics_active()) {
			return;
		}
		const auto guard = recording_guard{ };
		if (guard.entered()) {
			record({ .Action = action::storage_open, .Failure = failure_of(aFailure), .Store = storage_store_of(aFileName) }, true);
		}
	}

	void note_storage_statement(::sqlite3* const aDatabase) noexcept
	{
		StatementDirection = direction::none;
		if (!storage_diagnostics_active() || Recording) {
			return;
		}
		StatementStore     = store_of(aDatabase);
		StatementDirection = direction::unknown;
	}

	void note_storage_prepared(::sqlite3_stmt* const aStatement, const std::string_view aSql) noexcept
	{
		if (StatementDirection == direction::none || aStatement == nullptr) {
			return;
		}
		StatementDirection = transaction_control(aSql) || ::sqlite3_stmt_readonly(aStatement) == 0 ? direction::write : direction::read;
	}

	void note_storage_finalized() noexcept
	{
		StatementDirection = direction::none;
	}

	void note_storage_executed(::sqlite3* const aDatabase, const std::string_view aSql, const bool aSucceeded) noexcept
	{
		if (!storage_diagnostics_active()) {
			return;
		}
		const auto version = written_schema(aSql);
		if (!version) {
			return;
		}
		const auto guard = recording_guard{ };
		if (!guard.entered()) {
			return;
		}
		auto kind = failure::none;
		if (!aSucceeded) {
			kind = failure_of_result(::sqlite3_extended_errcode(aDatabase)).value_or(failure::unavailable);
		}
		// A migration is rare and records at once; a store that fails to
		// migrate on every open is aggregated like any failure.
		record({ .Action = action::storage_migrate, .Failure = kind, .Store = store_of(aDatabase), .Schema = *version }, kind != failure::none);
	}
} // namespace bpr::adapters::sqlite::detail
