#include "queue_enrollment_database.hpp"

#include <sqlite3.h>

#include <array>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRQ". Distinct from the ledger's "BPRD" so that pointing one store
		// at the other's file fails at the identity check rather than at the
		// first query for a table that is not there.
		constexpr std::int32_t QueueApplicationId = 0x42505251;

		/// \brief The statements that build schema version 1.
		///
		/// \details `STRICT` refuses a value of the wrong type rather than
		/// coercing it, and every enumerated column carries its range. Both
		/// turn a storage mistake into a failure at the row that caused it.
		///
		/// There is no column for a title, a comment, a diff, or a branch. The
		/// only free text is `rule_pattern`, which is what the reviewer typed
		/// into their own rule — local configuration, not repository content.
		constexpr std::array SchemaVersion1{
			std::string_view{
				"CREATE TABLE enrollment_meta("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"schema_format INTEGER NOT NULL"
				") STRICT" },
			std::string_view{
				"CREATE TABLE enrollment("
				"host TEXT NOT NULL,"
				"account TEXT NOT NULL,"
				"repository TEXT NOT NULL,"
				"pull_request INTEGER NOT NULL,"
				"role INTEGER NOT NULL CHECK(role BETWEEN 0 AND 4),"
				"matched_rule INTEGER NOT NULL CHECK(matched_rule IN (0, 1)),"
				"rule_kind INTEGER NOT NULL CHECK(rule_kind BETWEEN 0 AND 2),"
				"rule_pattern TEXT NOT NULL,"
				"rule_excluding INTEGER NOT NULL CHECK(rule_excluding IN (0, 1)),"
				"departed INTEGER NOT NULL CHECK(departed BETWEEN 0 AND 4),"
				"PRIMARY KEY(host, account, repository, pull_request)"
				") STRICT" },
		};

		[[nodiscard]] auto run_all(
			::sqlite3*                              aDatabase,
			const std::span<const std::string_view> aStatements,
			std::stop_token                         aStop)
			-> std::expected<void, failure>
		{
			for (const auto sql : aStatements) {
				if (auto ran = execute(aDatabase, sql, aStop); !ran) {
					return ran;
				}
			}
			return { };
		}

		[[nodiscard]] auto set_version(
			::sqlite3* aDatabase, const std::int64_t aVersion, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto sql
				= "PRAGMA user_version = " + std::to_string(aVersion);
			return execute(aDatabase, sql, aStop);
		}

		/// \brief Creates the schema inside one transaction.
		[[nodiscard]] auto establish_schema(
			::sqlite3* aDatabase, std::stop_token aStop)
			-> std::expected<void, failure>
		{
			const auto current = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!current) {
				return std::unexpected(current.error());
			}
			if (*current == queue_enrollment_schema_version) {
				return { };
			}
			// Forward only. A newer database is refused rather than opened,
			// because this build cannot know what the newer version added and
			// would silently drop it on the next write.
			if (*current > queue_enrollment_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			if (auto created = run_all(aDatabase, SchemaVersion1, aStop);
				!created) {
				rollback();
				return created;
			}
			// Derived from the constant rather than written out, so the stamp
			// and the validation cannot disagree.
			if (auto app = execute(
					aDatabase,
					"PRAGMA application_id = "
						+ std::to_string(QueueApplicationId),
					aStop);
				!app) {
				rollback();
				return app;
			}
			if (auto meta = execute(
					aDatabase,
					"INSERT INTO enrollment_meta(singleton, schema_format) "
					"VALUES(1, "
						+ std::to_string(queue_enrollment_schema_version) + ")",
					aStop);
				!meta) {
				rollback();
				return meta;
			}
			if (auto stamped = set_version(
					aDatabase, queue_enrollment_schema_version, aStop);
				!stamped) {
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
			if (!application || *application != QueueApplicationId || !version
				|| *version != queue_enrollment_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto meta = prepare(
				aDatabase,
				"SELECT schema_format FROM enrollment_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0)
					!= queue_enrollment_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto text_of(::sqlite3_stmt* aStatement, const int aColumn)
			-> std::string
		{
			const auto* const bytes = ::sqlite3_column_text(aStatement, aColumn);
			if (bytes == nullptr) {
				return { };
			}
			return std::string{ reinterpret_cast<const char*>(bytes) };
		}
	} // namespace

	queue_enrollment_database::queue_enrollment_database(
		::sqlite3* const aHandle) noexcept
		: Handle{ aHandle }
	{
	}

	queue_enrollment_database::queue_enrollment_database(
		queue_enrollment_database&& aOther) noexcept
		: Handle{ std::exchange(aOther.Handle, nullptr) }
	{
	}

	auto queue_enrollment_database::operator=(
		queue_enrollment_database&& aOther) noexcept -> queue_enrollment_database&
	{
		if (this != &aOther) {
			if (Handle != nullptr) {
				(void)::sqlite3_close_v2(Handle);
			}
			Handle = std::exchange(aOther.Handle, nullptr);
		}
		return *this;
	}

	queue_enrollment_database::~queue_enrollment_database()
	{
		if (Handle != nullptr) {
			(void)::sqlite3_close_v2(Handle);
			Handle = nullptr;
		}
	}

	auto queue_enrollment_database::open(
		const std::filesystem::path& aPath, std::stop_token aStop)
		-> std::expected<queue_enrollment_database, failure>
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
		auto owned = queue_enrollment_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}

		if (auto configured = configure_connection(database); !configured) {
			return std::unexpected(configured.error());
		}
		if (auto established = establish_schema(database, aStop); !established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return owned;
	}

	auto queue_enrollment_database::handle() const noexcept -> ::sqlite3*
	{
		return Handle;
	}

	auto queue_enrollment_database::store(
		const application::enrollment_record& aRecord)
		-> std::expected<void, failure>
	{
		auto statement = prepare(
			Handle,
			"INSERT INTO enrollment("
			"host, account, repository, pull_request, role, matched_rule,"
			"rule_kind, rule_pattern, rule_excluding, departed) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10) "
			"ON CONFLICT(host, account, repository, pull_request) DO UPDATE SET "
			"role = excluded.role, matched_rule = excluded.matched_rule,"
			"rule_kind = excluded.rule_kind, rule_pattern = excluded.rule_pattern,"
			"rule_excluding = excluded.rule_excluding,"
			"departed = excluded.departed");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto& ref  = aRecord.Ref;
		const auto& rule = aRecord.Membership.Rule;
		if (::sqlite3_bind_text(
				statement->Value, 1, ref.Host.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value, 2, ref.Account.c_str(), -1, SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   3,
				   ref.Repository.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 4, ref.Number) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   5,
				   static_cast<std::int64_t>(aRecord.Membership.Role))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   6,
				   aRecord.Membership.MatchedRule ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value, 7, static_cast<std::int64_t>(rule.Kind))
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   8,
				   rule.Pattern.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 9, rule.Excluding ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   10,
				   static_cast<std::int64_t>(aRecord.Departed))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}

	auto queue_enrollment_database::load_all()
		-> std::expected<std::vector<application::enrollment_record>, failure>
	{
		auto statement = prepare(
			Handle,
			"SELECT host, account, repository, pull_request, role, matched_rule,"
			"rule_kind, rule_pattern, rule_excluding, departed FROM enrollment "
			"ORDER BY host, account, repository, pull_request");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		auto records = std::vector<application::enrollment_record>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(statement->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			records.push_back(application::enrollment_record{
				.Ref = core::pull_request_ref{
					.Host       = text_of(statement->Value, 0),
					.Account    = text_of(statement->Value, 1),
					.Repository = text_of(statement->Value, 2),
					.Number     = ::sqlite3_column_int64(statement->Value, 3),
				},
				.Membership = core::queue_membership{
					.Role        = static_cast<core::github_role>(::sqlite3_column_int64(statement->Value, 4)),
					.MatchedRule = ::sqlite3_column_int64(statement->Value, 5) != 0,
					.Rule        = core::observer_rule{
						.Kind      = static_cast<core::observer_rule_kind>(::sqlite3_column_int64(statement->Value, 6)),
						.Pattern   = text_of(statement->Value, 7),
						.Excluding = ::sqlite3_column_int64(statement->Value, 8) != 0,
					},
				},
				.Departed = static_cast<application::enrollment_departure>(::sqlite3_column_int64(statement->Value, 9)),
			});
		}
		return records;
	}

	auto queue_enrollment_columns(::sqlite3* const aDatabase)
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
		auto found = std::vector<std::string>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(tables->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			names.push_back(text_of(tables->Value, 0));
		}

		for (const auto& table : names) {
			// `PRAGMA table_info` rather than the `pragma_table_info`
			// table-valued function, because `prepare` refuses a virtual table
			// and that refusal is a policy worth keeping.
			auto columns = prepare(aDatabase, "PRAGMA table_info(" + table + ")");
			if (!columns) {
				return std::unexpected(columns.error());
			}
			for (;;) {
				const auto stepped = ::sqlite3_step(columns->Value);
				if (stepped == SQLITE_DONE) {
					break;
				}
				if (stepped != SQLITE_ROW) {
					return std::unexpected(failure::integrity_failure);
				}
				found.push_back(table + "." + text_of(columns->Value, 1));
			}
		}
		return found;
	}
} // namespace bpr::adapters::sqlite::detail
