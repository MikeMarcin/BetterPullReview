#include "review_ledger_database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		constexpr std::int32_t LedgerApplicationId = 0x42505244;

		/// \brief The statements that build schema version 1.
		///
		/// \details `STRICT` refuses a value of the wrong type rather than
		/// coercing it, and the length checks refuse an identity that is not a
		/// 256-bit digest. Both turn a storage mistake into a failure at the
		/// row that caused it instead of a puzzle read back later.
		constexpr std::array SchemaVersion1{
			std::string_view{
				"CREATE TABLE ledger_meta("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"schema_format INTEGER NOT NULL"
				") STRICT" },
			std::string_view{
				"CREATE TABLE review_scope("
				"scope_id INTEGER PRIMARY KEY,"
				"host TEXT NOT NULL,"
				"account TEXT NOT NULL,"
				"repository TEXT NOT NULL,"
				"pull_request INTEGER NOT NULL,"
				"UNIQUE(host, account, repository, pull_request)"
				") STRICT" },
			std::string_view{
				"CREATE TABLE file_receipt("
				"scope_id INTEGER NOT NULL "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"file_identity BLOB NOT NULL CHECK(length(file_identity) = 32),"
				"material_id BLOB NOT NULL CHECK(length(material_id) = 32),"
				"read_state INTEGER NOT NULL CHECK(read_state BETWEEN 0 AND 2),"
				"accepted INTEGER NOT NULL CHECK(accepted IN (0, 1)),"
				"excluded INTEGER NOT NULL CHECK(excluded IN (0, 1)),"
				"PRIMARY KEY(scope_id, file_identity)"
				") STRICT" },
		};

		/// \brief The statements that carry schema version 1 to version 2.
		constexpr std::array SchemaVersion1To2{
			std::string_view{
				"CREATE TABLE conversation_receipt("
				"scope_id INTEGER NOT NULL "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"conversation INTEGER NOT NULL,"
				"seen_comment INTEGER NOT NULL,"
				"seen_edit INTEGER NOT NULL,"
				"accepted_comment INTEGER NOT NULL,"
				"accepted_edit INTEGER NOT NULL,"
				"PRIMARY KEY(scope_id, conversation)"
				") STRICT" },
		};

		[[nodiscard]] auto run_all(
			::sqlite3*                              aDatabase,
			const std::span<const std::string_view> aStatements,
			std::stop_token                         aStop)
			-> std::expected<void, failure>
		{
			for (const auto sql : aStatements) {
				if (auto result = execute(aDatabase, sql, aStop); !result) {
					return result;
				}
			}
			return { };
		}

		[[nodiscard]] auto set_version(
			::sqlite3*         aDatabase,
			const std::int64_t aVersion,
			std::stop_token    aStop) -> std::expected<void, failure>
		{
			const auto sql
				= "PRAGMA user_version = " + std::to_string(aVersion);
			return execute(aDatabase, sql, aStop);
		}

		/// \brief Creates or migrates the schema inside one transaction.
		///
		/// \details The whole advance is one transaction, so a database is
		/// never left between two versions. A partially migrated ledger would
		/// be indistinguishable from a corrupt one.
		[[nodiscard]] auto establish_schema(
			::sqlite3*      aDatabase,
			std::stop_token aStop) -> std::expected<void, failure>
		{
			const auto current = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!current) {
				return std::unexpected(current.error());
			}
			if (*current == review_ledger_schema_version) {
				return { };
			}
			// Forward only. A newer database is refused rather than opened,
			// because this build cannot know what the newer version added and
			// would silently drop it on the next write.
			if (*current > review_ledger_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			auto version = *current;
			if (version == 0) {
				if (auto created = run_all(aDatabase, SchemaVersion1, aStop);
					!created) {
					rollback();
					return created;
				}
				// Derived from the constant rather than written out, so the
				// stamp and the validation cannot disagree.
				if (auto app = execute(
						aDatabase,
						"PRAGMA application_id = "
							+ std::to_string(LedgerApplicationId),
						aStop);
					!app) {
					rollback();
					return app;
				}
				if (auto meta = execute(
						aDatabase,
						"INSERT INTO ledger_meta(singleton, schema_format) "
						"VALUES(1, 1)",
						aStop);
					!meta) {
					rollback();
					return meta;
				}
				version = 1;
			}
			if (version == 1) {
				if (auto migrated
					= run_all(aDatabase, SchemaVersion1To2, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
				version = 2;
			}

			if (auto recorded = execute(
					aDatabase,
					"UPDATE ledger_meta SET schema_format = "
						+ std::to_string(version),
					aStop);
				!recorded) {
				rollback();
				return recorded;
			}
			if (auto stamped = set_version(aDatabase, version, aStop); !stamped) {
				rollback();
				return stamped;
			}
			if (auto commit = execute(aDatabase, "COMMIT", aStop); !commit) {
				rollback();
				return commit;
			}
			return { };
		}

		[[nodiscard]] auto validate(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!application || *application != LedgerApplicationId || !version
				|| *version != review_ledger_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto meta = prepare(
				aDatabase,
				"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0)
					!= review_ledger_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}
	} // namespace

	review_ledger_database::review_ledger_database(::sqlite3* aHandle) noexcept
		: Handle{ aHandle }
	{
	}

	review_ledger_database::review_ledger_database(
		review_ledger_database&& aOther) noexcept
		: Handle{ std::exchange(aOther.Handle, nullptr) }
	{
	}

	auto review_ledger_database::operator=(
		review_ledger_database&& aOther) noexcept -> review_ledger_database&
	{
		if (this != &aOther) {
			if (Handle != nullptr) {
				(void)::sqlite3_close(Handle);
			}
			Handle = std::exchange(aOther.Handle, nullptr);
		}
		return *this;
	}

	review_ledger_database::~review_ledger_database()
	{
		if (Handle != nullptr) {
			(void)::sqlite3_close(Handle);
		}
	}

	auto review_ledger_database::handle() const noexcept -> ::sqlite3*
	{
		return Handle;
	}

	auto review_ledger_database::open(
		const std::filesystem::path& aPath,
		std::stop_token              aStop)
		-> std::expected<review_ledger_database, failure>
	{
		if (aPath.empty() || !aPath.is_absolute()) {
			return std::unexpected(failure::invalid_path);
		}

		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}

		::sqlite3* database = nullptr;
		const auto native   = aPath.string();
		const auto code     = ::sqlite3_open_v2(
			native.c_str(),
			&database,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX
				| SQLITE_OPEN_EXRESCODE,
			nullptr);
		auto owned = review_ledger_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}

		if (auto configured = configure_connection(database); !configured) {
			return std::unexpected(configured.error());
		}
		if (auto established = establish_schema(database, aStop);
			!established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return owned;
	}

	auto review_ledger_database::scope_id(const review_scope& aScope)
		-> std::expected<std::int64_t, failure>
	{
		const auto bind = [&aScope](::sqlite3_stmt* aStatement) -> bool {
			return ::sqlite3_bind_text(
					   aStatement,
					   1,
					   aScope.Host.data(),
					   static_cast<int>(aScope.Host.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK
				&& ::sqlite3_bind_text(
					   aStatement,
					   2,
					   aScope.Account.data(),
					   static_cast<int>(aScope.Account.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK
				&& ::sqlite3_bind_text(
					   aStatement,
					   3,
					   aScope.Repository.data(),
					   static_cast<int>(aScope.Repository.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK
				&& ::sqlite3_bind_int64(aStatement, 4, aScope.PullRequest)
				== SQLITE_OK;
		};

		auto insert = prepare(
			Handle,
			"INSERT OR IGNORE INTO review_scope"
			"(host, account, repository, pull_request) VALUES(?1, ?2, ?3, ?4)");
		if (!insert) {
			return std::unexpected(insert.error());
		}
		if (!bind(insert->Value)) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(insert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		auto select = prepare(
			Handle,
			"SELECT scope_id FROM review_scope WHERE host = ?1 AND account = ?2 "
			"AND repository = ?3 AND pull_request = ?4");
		if (!select) {
			return std::unexpected(select.error());
		}
		if (!bind(select->Value)) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(select->Value) != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return ::sqlite3_column_int64(select->Value, 0);
	}

	auto review_ledger_database::store_receipt(
		const std::int64_t        aScope,
		const core::file_receipt& aReceipt) -> std::expected<void, failure>
	{
		auto upsert = prepare(
			Handle,
			"INSERT INTO file_receipt(scope_id, file_identity, material_id, "
			"read_state, accepted, excluded) VALUES(?1, ?2, ?3, ?4, ?5, ?6) "
			"ON CONFLICT(scope_id, file_identity) DO UPDATE SET "
			"material_id = excluded.material_id, "
			"read_state = excluded.read_state, "
			"accepted = excluded.accepted, "
			"excluded = excluded.excluded");
		if (!upsert) {
			return std::unexpected(upsert.error());
		}

		const auto& file     = aReceipt.File.bytes();
		const auto& material = aReceipt.Material.bytes();
		if (::sqlite3_bind_int64(upsert->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_blob(
				   upsert->Value,
				   2,
				   file.data(),
				   static_cast<int>(file.size()),
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_blob(
				   upsert->Value,
				   3,
				   material.data(),
				   static_cast<int>(material.size()),
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 4, static_cast<std::int64_t>(aReceipt.Read))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(upsert->Value, 5, aReceipt.Accepted ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(upsert->Value, 6, aReceipt.Excluded ? 1 : 0)
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(upsert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto review_ledger_database::load_receipt(
		const std::int64_t         aScope,
		const core::file_identity& aFile)
		-> std::expected<std::optional<core::file_receipt>, failure>
	{
		auto select = prepare(
			Handle,
			"SELECT material_id, read_state, accepted, excluded FROM "
			"file_receipt WHERE scope_id = ?1 AND file_identity = ?2");
		if (!select) {
			return std::unexpected(select.error());
		}

		const auto& file = aFile.bytes();
		if (::sqlite3_bind_int64(select->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_blob(
				   select->Value,
				   2,
				   file.data(),
				   static_cast<int>(file.size()),
				   SQLITE_TRANSIENT)
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}

		const auto row = ::sqlite3_step(select->Value);
		if (row == SQLITE_DONE) {
			return std::optional<core::file_receipt>{ };
		}
		if (row != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		if (::sqlite3_column_type(select->Value, 0) != SQLITE_BLOB
			|| ::sqlite3_column_bytes(select->Value, 0) != 32) {
			return std::unexpected(failure::integrity_failure);
		}

		auto        material = std::array<std::uint8_t, 32>{ };
		const auto* bytes
			= static_cast<const std::uint8_t*>(::sqlite3_column_blob(select->Value, 0));
		std::copy_n(bytes, material.size(), material.begin());

		const auto read = ::sqlite3_column_int64(select->Value, 1);
		if (read < 0 || read > 2) {
			return std::unexpected(failure::integrity_failure);
		}

		return std::optional{ core::file_receipt{
			.File     = aFile,
			.Material = core::file_material_id{ material },
			.Read     = static_cast<core::file_read_state>(read),
			.Accepted = ::sqlite3_column_int64(select->Value, 2) != 0,
			.Excluded = ::sqlite3_column_int64(select->Value, 3) != 0,
		} };
	}

	auto review_ledger_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>
	{
		auto tables = prepare(
			aDatabase,
			"SELECT name FROM sqlite_schema WHERE type = 'table' "
			"AND name NOT LIKE 'sqlite_%' ORDER BY name");
		if (!tables) {
			return std::unexpected(tables.error());
		}

		auto names = std::vector<std::string>{ };
		while (::sqlite3_step(tables->Value) == SQLITE_ROW) {
			const auto* text = ::sqlite3_column_text(tables->Value, 0);
			if (text == nullptr) {
				return std::unexpected(failure::schema_mismatch);
			}
			names.emplace_back(reinterpret_cast<const char*>(text));
		}

		auto columns = std::vector<std::string>{ };
		for (const auto& table : names) {
			// `PRAGMA table_info` rather than the `pragma_table_info`
			// table-valued function, because `prepare` refuses a virtual table
			// and that refusal is a policy worth keeping.
			auto info = prepare(aDatabase, "PRAGMA table_info(" + table + ")");
			if (!info) {
				return std::unexpected(info.error());
			}
			while (::sqlite3_step(info->Value) == SQLITE_ROW) {
				const auto* text = ::sqlite3_column_text(info->Value, 1);
				if (text == nullptr) {
					return std::unexpected(failure::schema_mismatch);
				}
				columns.emplace_back(
					table + "." + reinterpret_cast<const char*>(text));
			}
		}
		return columns;
	}
} // namespace bpr::adapters::sqlite::detail
