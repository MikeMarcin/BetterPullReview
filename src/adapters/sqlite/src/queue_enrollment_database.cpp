#include "queue_enrollment_database.hpp"

#include "storage_internal.hpp"

#include <sqlite3.h>

#include <array>
#include <charconv>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		// "BPRQ". Distinct from the ledger's "BPRD" so that pointing one store
		// at the other's file fails at the identity check rather than at the
		// first query for a table that is not there.
		constexpr std::int32_t QueueApplicationId = 0x42505251;

		constexpr auto OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
			| SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;

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

		constexpr std::array SchemaVersion2{
			std::string_view{
				"CREATE TABLE observer_rule("
				"ordinal INTEGER PRIMARY KEY CHECK(ordinal > 0),"
				"rule_kind INTEGER NOT NULL CHECK(rule_kind BETWEEN 0 AND 2),"
				"rule_pattern TEXT NOT NULL,"
				"rule_excluding INTEGER NOT NULL CHECK(rule_excluding IN (0, 1)),"
				"UNIQUE(rule_kind, rule_pattern, rule_excluding)"
				") STRICT" },
		};

		constexpr std::array SchemaVersion3{
			std::string_view{
				"CREATE TABLE presentation_preferences("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"role_filter INTEGER NOT NULL CHECK(role_filter BETWEEN -1 AND 4),"
				"observer_filter INTEGER NOT NULL CHECK(observer_filter BETWEEN -1 AND 2),"
				"bucket_filter INTEGER NOT NULL CHECK(bucket_filter BETWEEN -1 AND 6),"
				"repository_filter TEXT NOT NULL,"
				"quiet_filter INTEGER NOT NULL CHECK(quiet_filter BETWEEN 0 AND 2),"
				"sort_order INTEGER NOT NULL CHECK(sort_order BETWEEN 0 AND 2)"
				") STRICT" },
		};

		constexpr std::array SchemaVersion6{
			std::string_view{
				"CREATE TABLE presentation_preferences("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"role_filter INTEGER NOT NULL CHECK(role_filter BETWEEN -1 AND 4),"
				"observer_filter INTEGER NOT NULL CHECK(observer_filter BETWEEN -1 AND 2),"
				"bucket_filter INTEGER NOT NULL CHECK(bucket_filter BETWEEN -1 AND 6),"
				"repository_filter TEXT NOT NULL,"
				"quiet_filter INTEGER NOT NULL CHECK(quiet_filter BETWEEN 0 AND 2),"
				"sort_order INTEGER NOT NULL CHECK(sort_order BETWEEN 0 AND 2),"
				"notifications_enabled INTEGER NOT NULL CHECK(notifications_enabled IN (0, 1)),"
				"notifications_paused INTEGER NOT NULL CHECK(notifications_paused IN (0, 1))"
				") STRICT" },
		};

		/// \brief The statements that carry schema version 3 to version 4.
		///
		/// \details [[BPR-0659]]. The repositories the reviewer keeps in the
		/// drawer, per host and account. Names only; nothing the repository
		/// holds.
		constexpr std::array SchemaVersion4{
			std::string_view{
				"CREATE TABLE saved_repository("
				"host TEXT NOT NULL,"
				"account TEXT NOT NULL,"
				"repository TEXT NOT NULL,"
				"PRIMARY KEY(host, account, repository)"
				") STRICT" },
		};

		/// \brief The statements that carry schema version 4 to version 5.
		///
		/// \details [[BPR-0660]]. The enterprise servers the reviewer keeps:
		/// the hostname and the account name signed in there. No token.
		constexpr std::array SchemaVersion5{
			std::string_view{
				"CREATE TABLE saved_server("
				"hostname TEXT NOT NULL,"
				"account TEXT NOT NULL,"
				"PRIMARY KEY(hostname)"
				") STRICT" },
		};
		/// \brief The tables version 7 rebuilds ([[BPR-0989]]).
		///
		/// \details The enrollment gains the discovery reasons, the reviewer's
		/// local follow, the observed review and the `undiscovered` departure;
		/// the landing preferences admit the `detail_unknown` bucket. Still no
		/// title, body, path or timestamp.
		constexpr std::array SchemaVersion7{
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
				"departed INTEGER NOT NULL CHECK(departed BETWEEN 0 AND 5),"
				"authored INTEGER NOT NULL CHECK(authored IN (0, 1)),"
				"requested INTEGER NOT NULL CHECK(requested IN (0, 1)),"
				"followed INTEGER NOT NULL CHECK(followed IN (0, 1)),"
				"reviewed INTEGER NOT NULL CHECK(reviewed IN (0, 1)),"
				"PRIMARY KEY(host, account, repository, pull_request)"
				") STRICT" },
			std::string_view{
				"CREATE TABLE presentation_preferences("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"role_filter INTEGER NOT NULL CHECK(role_filter BETWEEN -1 AND 4),"
				"observer_filter INTEGER NOT NULL CHECK(observer_filter BETWEEN -1 AND 2),"
				"bucket_filter INTEGER NOT NULL CHECK(bucket_filter BETWEEN -1 AND 7),"
				"repository_filter TEXT NOT NULL,"
				"quiet_filter INTEGER NOT NULL CHECK(quiet_filter BETWEEN 0 AND 2),"
				"sort_order INTEGER NOT NULL CHECK(sort_order BETWEEN 0 AND 2),"
				"notifications_enabled INTEGER NOT NULL CHECK(notifications_enabled IN (0, 1)),"
				"notifications_paused INTEGER NOT NULL CHECK(notifications_paused IN (0, 1))"
				") STRICT" },
		};

		constexpr std::array SchemaVersion8{
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
				"departed INTEGER NOT NULL CHECK(departed BETWEEN 0 AND 5),"
				"authored INTEGER NOT NULL CHECK(authored IN (0, 1)),"
				"requested INTEGER NOT NULL CHECK(requested IN (0, 1)),"
				"followed INTEGER NOT NULL CHECK(followed IN (0, 1)),"
				"reviewed INTEGER NOT NULL CHECK(reviewed IN (0, 1)),"
				"draft INTEGER CHECK(draft IN(0, 1)),"
				"draft_account TEXT NOT NULL,"
				"PRIMARY KEY(host, account, repository, pull_request)"
				") STRICT" }
		};

		struct schema_table
		{
			std::string_view Name;
			std::string_view Sql;
		};

		/// \brief The complete canonical version-two queue schema.
		///
		/// \details The rule ordinal is durable precedence, not an implementation
		/// accident. Exact canonical validation also refuses a current-version
		/// file whose columns, constraints, table kind, or object set changed.
		constexpr std::array SchemaVersion2Tables{
			schema_table{ "enrollment", SchemaVersion1[1] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
		};

		constexpr std::array SchemaVersion3Tables{
			schema_table{ "enrollment", SchemaVersion1[1] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion3[0] },
		};

		constexpr std::array SchemaVersion4Tables{
			schema_table{ "enrollment", SchemaVersion1[1] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion3[0] },
			schema_table{ "saved_repository", SchemaVersion4[0] },
		};
		constexpr std::array SchemaVersion5Tables{
			schema_table{ "enrollment", SchemaVersion1[1] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion3[0] },
			schema_table{ "saved_repository", SchemaVersion4[0] },
			schema_table{ "saved_server", SchemaVersion5[0] },
		};

		constexpr std::array SchemaVersion6Tables{
			schema_table{ "enrollment", SchemaVersion1[1] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion6[0] },
			schema_table{ "saved_repository", SchemaVersion4[0] },
			schema_table{ "saved_server", SchemaVersion5[0] },
		};

		constexpr std::array SchemaVersion7Tables{
			schema_table{ "enrollment", SchemaVersion7[0] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion7[1] },
			schema_table{ "saved_repository", SchemaVersion4[0] },
			schema_table{ "saved_server", SchemaVersion5[0] },
		};

		constexpr std::array SchemaVersion8Tables{
			schema_table{ "enrollment", SchemaVersion8[0] },
			schema_table{ "enrollment_meta", SchemaVersion1[0] },
			schema_table{ "observer_rule", SchemaVersion2[0] },
			schema_table{ "presentation_preferences", SchemaVersion7[1] },
			schema_table{ "saved_repository", SchemaVersion4[0] },
			schema_table{ "saved_server", SchemaVersion5[0] },
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

		[[nodiscard]] auto validate_version_one(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto format = scalar_integer(
				aDatabase,
				"SELECT schema_format FROM enrollment_meta WHERE singleton = 1");
			const auto metaSchema = scalar_text(
				aDatabase,
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'enrollment_meta'");
			const auto enrollmentSchema = scalar_text(
				aDatabase,
				"SELECT sql FROM sqlite_schema WHERE type = 'table' "
				"AND name = 'enrollment'");
			if (!application || *application != QueueApplicationId || !format
				|| *format != 1 || !metaSchema || !enrollmentSchema
				|| *metaSchema != SchemaVersion1[0]
				|| *enrollmentSchema != SchemaVersion1[1]) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto validate_version_two(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			const auto format  = scalar_integer(
				aDatabase,
				"SELECT schema_format FROM enrollment_meta WHERE singleton = 1");
			if (!application || *application != QueueApplicationId || !version
				|| *version != 2 || !format || *format != 2) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto objects = prepare(
				aDatabase,
				"SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name");
			if (!objects) {
				return std::unexpected(objects.error());
			}
			std::size_t index = 0;
			for (;;) {
				const auto row = ::sqlite3_step(objects->Value);
				if (row == SQLITE_DONE) {
					break;
				}
				if (row != SQLITE_ROW || index >= SchemaVersion2Tables.size()) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto text = [&objects](const int aColumn)
					-> std::optional<std::string_view> {
					if (::sqlite3_column_type(objects->Value, aColumn) != SQLITE_TEXT) {
						return std::nullopt;
					}
					const auto* value = reinterpret_cast<const char*>(
						::sqlite3_column_text(objects->Value, aColumn));
					const auto bytes
						= ::sqlite3_column_bytes(objects->Value, aColumn);
					if (value == nullptr || bytes < 0) {
						return std::nullopt;
					}
					return std::string_view{
						value,
						static_cast<std::size_t>(bytes),
					};
				};
				const auto type = text(0);
				const auto name = text(1);
				const auto sql  = text(2);
				if (!type || !name || !sql || *type != "table"
					|| *name != SchemaVersion2Tables[index].Name
					|| canonical_sql(*sql)
						!= canonical_sql(SchemaVersion2Tables[index].Sql)) {
					return std::unexpected(failure::schema_mismatch);
				}
				++index;
			}
			if (index != SchemaVersion2Tables.size()) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto validate_exact_version(::sqlite3* aDatabase, const std::int64_t aVersion, const std::span<const schema_table> aTables)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			const auto format  = scalar_integer(
				aDatabase,
				"SELECT schema_format FROM enrollment_meta WHERE singleton = 1");
			if (!application || *application != QueueApplicationId || !version
				|| *version != aVersion || !format || *format != aVersion) {
				return std::unexpected(failure::schema_mismatch);
			}

			auto objects = prepare(
				aDatabase,
				"SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name");
			if (!objects) {
				return std::unexpected(objects.error());
			}
			std::size_t index = 0U;
			for (;;) {
				const auto row = ::sqlite3_step(objects->Value);
				if (row == SQLITE_DONE) {
					break;
				}
				if (row != SQLITE_ROW || index >= aTables.size()) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto text = [&objects](const int aColumn)
					-> std::optional<std::string_view> {
					if (::sqlite3_column_type(objects->Value, aColumn) != SQLITE_TEXT) {
						return std::nullopt;
					}
					const auto* value = reinterpret_cast<const char*>(
						::sqlite3_column_text(objects->Value, aColumn));
					const auto bytes
						= ::sqlite3_column_bytes(objects->Value, aColumn);
					if (value == nullptr || bytes < 0) {
						return std::nullopt;
					}
					return std::string_view{
						value,
						static_cast<std::size_t>(bytes),
					};
				};
				const auto type = text(0);
				const auto name = text(1);
				const auto sql  = text(2);
				if (!type || !name || !sql || *type != "table"
					|| *name != aTables[index].Name
					|| canonical_sql(*sql)
						!= canonical_sql(aTables[index].Sql)) {
					return std::unexpected(failure::schema_mismatch);
				}
				++index;
			}
			return index == aTables.size()
				? std::expected<void, failure>{ }
				: std::unexpected(failure::schema_mismatch);
		}

		[[nodiscard]] auto validate_version_three(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			return validate_exact_version(aDatabase, 3, SchemaVersion3Tables);
		}

		[[nodiscard]] auto validate_version_four(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			return validate_exact_version(aDatabase, 4, SchemaVersion4Tables);
		}
		[[nodiscard]] auto validate_version_five(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			return validate_exact_version(
				aDatabase, 5, SchemaVersion5Tables);
		}

		[[nodiscard]] auto preflight_existing_database(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!version) {
				return std::unexpected(version.error());
			}
			if (*version == 0) {
				return { };
			}
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			if (!application || *application != QueueApplicationId) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (*version == 1) {
				return validate_version_one(aDatabase);
			}
			if (*version == 2) {
				return validate_version_two(aDatabase);
			}
			if (*version == 3) {
				return validate_version_three(aDatabase);
			}
			if (*version == 4) {
				return validate_version_four(aDatabase);
			}
			if (*version == 5) {
				return validate_version_five(aDatabase);
			}
			if (*version == 6) {
				return validate_exact_version(aDatabase, 6, SchemaVersion6Tables);
			}
			if (*version == 7) {
				return validate_exact_version(aDatabase, 7, SchemaVersion7Tables);
			}
			if (*version == queue_enrollment_schema_version) {
				return validate_exact_version(aDatabase, queue_enrollment_schema_version, SchemaVersion8Tables);
			}
			return std::unexpected(failure::schema_mismatch);
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
			if (*current == 1) {
				if (auto prior = validate_version_one(aDatabase); !prior) {
					return prior;
				}
			}

			if (auto begin = execute(aDatabase, "BEGIN IMMEDIATE", aStop); !begin) {
				return begin;
			}
			auto rollback = [&aDatabase]() noexcept {
				(void)::sqlite3_exec(
					aDatabase, "ROLLBACK", nullptr, nullptr, nullptr);
			};

			if (*current == 0) {
				if (auto created = run_all(aDatabase, SchemaVersion1, aStop);
					!created) {
					rollback();
					return created;
				}
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
						"VALUES(1, 1)",
						aStop);
					!meta) {
					rollback();
					return meta;
				}
			}
			if (*current < 2) {
				if (auto migrated = run_all(aDatabase, SchemaVersion2, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
			}
			if (*current < 3) {
				if (auto migrated = run_all(aDatabase, SchemaVersion3, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
			}
			if (*current < 4) {
				if (auto migrated = run_all(aDatabase, SchemaVersion4, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
			}
			if (*current < 5) {
				if (auto migrated = run_all(aDatabase, SchemaVersion5, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
			}
			if (*current < 6) {
				const auto migration = std::array{
					std::string{ "CREATE TABLE previous_preferences AS SELECT * FROM presentation_preferences" },
					std::string{ "DROP TABLE presentation_preferences" },
					std::string{ SchemaVersion6[0] },
					std::string{ "INSERT INTO presentation_preferences SELECT singleton, role_filter, observer_filter, bucket_filter, repository_filter, quiet_filter, sort_order, 1, 0 FROM previous_preferences" },
					std::string{ "DROP TABLE previous_preferences" },
				};
				for (const auto& sql : migration) {
					if (auto migrated = execute(aDatabase, sql, aStop); !migrated) {
						rollback();
						return migrated;
					}
				}
			}
			if (*current < 7) {
				// [[BPR-0989]]. Every enrollment keeps its membership and
				// departure; the new reasons start false, which is what the
				// store knew before discovery existed.
				const auto migration = std::array{
					std::string{ "CREATE TABLE previous_enrollment AS SELECT * FROM enrollment" },
					std::string{ "DROP TABLE enrollment" },
					std::string{ SchemaVersion7[0] },
					std::string{ "INSERT INTO enrollment SELECT host, account, repository, pull_request, role, matched_rule, rule_kind, rule_pattern, rule_excluding, departed, 0, 0, 0, 0 FROM previous_enrollment" },
					std::string{ "DROP TABLE previous_enrollment" },
					std::string{ "CREATE TABLE previous_preferences AS SELECT * FROM presentation_preferences" },
					std::string{ "DROP TABLE presentation_preferences" },
					std::string{ SchemaVersion7[1] },
					std::string{ "INSERT INTO presentation_preferences SELECT singleton, role_filter, observer_filter, bucket_filter, repository_filter, quiet_filter, sort_order, notifications_enabled, notifications_paused FROM previous_preferences" },
					std::string{ "DROP TABLE previous_preferences" },
				};
				for (const auto& sql : migration) {
					if (auto migrated = execute(aDatabase, sql, aStop); !migrated) {
						rollback();
						return migrated;
					}
				}
			}
			if (*current < 8) {
				const auto migration = std::array{ std::string{ "CREATE TABLE draft_enrollment_migration AS SELECT * FROM enrollment" }, std::string{ "DROP TABLE enrollment" }, std::string{ SchemaVersion8[0] }, std::string{ "INSERT INTO enrollment SELECT *,NULL,'0' FROM draft_enrollment_migration" }, std::string{ "DROP TABLE draft_enrollment_migration" } };
				for (const auto& sql : migration) {
					if (auto migrated = execute(aDatabase, sql, aStop); !migrated) {
						rollback();
						return migrated;
					}
				}
			}
			if (auto meta = execute(
					aDatabase,
					"UPDATE enrollment_meta SET schema_format = "
						+ std::to_string(queue_enrollment_schema_version)
						+ " WHERE singleton = 1",
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
			return validate_exact_version(aDatabase, queue_enrollment_schema_version, SchemaVersion8Tables);
		}

		[[nodiscard]] auto text_of(::sqlite3_stmt* aStatement, const int aColumn)
			-> std::string
		{
			const auto* const bytes = ::sqlite3_column_text(aStatement, aColumn);
			if (bytes == nullptr) {
				return { };
			}
			const auto size = ::sqlite3_column_bytes(aStatement, aColumn);
			return size > 0
				? std::string{
					  reinterpret_cast<const char*>(bytes),
					  static_cast<std::size_t>(size)
				  }
				: std::string{ };
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
			native.c_str(), &database, OpenFlags, nullptr);
		auto owned = queue_enrollment_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}
		return adopt(std::move(owned), std::move(aStop));
	}

	auto queue_enrollment_database::open(
		state_directory& aDirectory, const std::string_view aName, std::stop_token aStop)
		-> std::expected<queue_enrollment_database, failure>
	{
		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}
		auto opened = open_private_store_database(
			aDirectory, aName, OpenFlags | SQLITE_OPEN_NOFOLLOW);
		if (!opened) {
			return std::unexpected(private_store_database_failure(opened.error()));
		}
		return adopt(queue_enrollment_database{ *opened }, std::move(aStop));
	}

	auto queue_enrollment_database::adopt(
		queue_enrollment_database aOwned, std::stop_token aStop)
		-> std::expected<queue_enrollment_database, failure>
	{
		auto* const database = aOwned.Handle;
		if (auto preflight = preflight_existing_database(database); !preflight) {
			return std::unexpected(preflight.error());
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
		return aOwned;
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
			"rule_kind, rule_pattern, rule_excluding, departed,"
			"authored, requested, followed, reviewed, draft, draft_account) "
			"SELECT ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?12, ?13, ?14, ?15, ?16, ?17 "
			"WHERE (SELECT COUNT(*) FROM (SELECT 1 FROM enrollment LIMIT ?11)) < ?11 "
			"OR EXISTS(SELECT 1 FROM enrollment WHERE host = ?1 AND account = ?2 AND repository = ?3 AND pull_request = ?4) "
			"ON CONFLICT(host, account, repository, pull_request) DO UPDATE SET "
			"role = excluded.role, matched_rule = excluded.matched_rule,"
			"rule_kind = excluded.rule_kind, rule_pattern = excluded.rule_pattern,"
			"rule_excluding = excluded.rule_excluding,"
			"departed = excluded.departed,"
			"authored = excluded.authored, requested = excluded.requested,"
			"followed = excluded.followed, reviewed = excluded.reviewed, draft = excluded.draft, draft_account = excluded.draft_account");
		if (!statement) {
			return std::unexpected(statement.error());
		}

		const auto draftAccount = std::to_string(aRecord.DraftAccount.value());
		if ((aRecord.Draft ? ::sqlite3_bind_int(statement->Value, 16, *aRecord.Draft ? 1 : 0) : ::sqlite3_bind_null(statement->Value, 16)) != SQLITE_OK || ::sqlite3_bind_text(statement->Value, 17, draftAccount.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		const auto& ref  = aRecord.Ref;
		const auto& rule = aRecord.Membership.Rule;
		if (::sqlite3_bind_int64(statement->Value, 11, static_cast<std::int64_t>(application::maximum_queue_enrollments)) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
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
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 12, aRecord.Membership.Authored ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 13, aRecord.Membership.Requested ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 14, aRecord.Membership.Followed ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 15, aRecord.Membership.Reviewed ? 1 : 0) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_changes(Handle) == 0) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}

	auto queue_enrollment_database::store_all(std::span<const application::enrollment_record> aRecords, std::stop_token aStop)
		-> std::expected<void, failure>
	{
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE", aStop); !begin) {
			return begin;
		}
		for (const auto& record : aRecords) {
			if (auto stored = store(record); !stored) {
				(void)::sqlite3_exec(Handle, "ROLLBACK", nullptr, nullptr, nullptr);
				return stored;
			}
		}
		if (auto commit = execute(Handle, "COMMIT", aStop); !commit) {
			(void)::sqlite3_exec(Handle, "ROLLBACK", nullptr, nullptr, nullptr);
			return commit;
		}
		return { };
	}

	auto queue_enrollment_database::load_all()
		-> std::expected<std::vector<application::enrollment_record>, failure>
	{
		auto statement = prepare(
			Handle,
			"SELECT host, account, repository, pull_request, role, matched_rule,"
			"rule_kind, rule_pattern, rule_excluding, departed,"
			"authored, requested, followed, reviewed, draft, draft_account FROM enrollment "
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
			if (records.size() >= application::maximum_queue_enrollments) {
				return std::unexpected(failure::quota_exceeded);
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			const auto draftType   = ::sqlite3_column_type(statement->Value, 14);
			const auto draftValue  = ::sqlite3_column_int64(statement->Value, 14);
			const auto accountText = text_of(statement->Value, 15);
			auto       account     = std::uint64_t{ 0 };
			const auto parsed      = std::from_chars(accountText.data(), accountText.data() + accountText.size(), account);
			if ((draftType != SQLITE_NULL && (draftType != SQLITE_INTEGER || (draftValue != 0 && draftValue != 1))) || parsed.ec != std::errc{ } || parsed.ptr != accountText.data() + accountText.size() || std::to_string(account) != accountText) {
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
					.Authored  = ::sqlite3_column_int64(statement->Value, 10) != 0,
					.Requested = ::sqlite3_column_int64(statement->Value, 11) != 0,
					.Followed  = ::sqlite3_column_int64(statement->Value, 12) != 0,
					.Reviewed  = ::sqlite3_column_int64(statement->Value, 13) != 0,
				},
				.Departed     = static_cast<application::enrollment_departure>(::sqlite3_column_int64(statement->Value, 9)),
				.Draft        = draftType == SQLITE_NULL ? std::nullopt : std::optional<bool>{ draftValue != 0 },
				.DraftAccount = core::github_account_id{ account },
			});
		}
		return records;
	}

	auto queue_enrollment_database::store_saved_repository(
		const application::saved_repository& aRepository)
		-> std::expected<void, failure>
	{
		if (!application::valid(aRepository)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"INSERT INTO saved_repository(host, account, repository) "
			"SELECT ?1, ?2, ?3 "
			"WHERE (SELECT COUNT(*) FROM (SELECT 1 FROM saved_repository LIMIT ?4)) < ?4 "
			"OR EXISTS(SELECT 1 FROM saved_repository WHERE host = ?1 AND account = ?2 AND repository = ?3) "
			"ON CONFLICT(host, account, repository) DO UPDATE SET repository = excluded.repository");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_int64(statement->Value, 4, static_cast<std::int64_t>(application::maximum_saved_repositories)) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_bind_text(statement->Value, 1, aRepository.Host.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_text(statement->Value, 2, aRepository.Account.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_text(statement->Value, 3, aRepository.Repository.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_changes(Handle) == 0) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}

	auto queue_enrollment_database::erase_saved_repository(
		const application::saved_repository& aRepository)
		-> std::expected<void, failure>
	{
		if (!application::valid(aRepository)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"DELETE FROM saved_repository WHERE host = ?1 AND account = ?2 "
			"AND repository = ?3");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_text(statement->Value, 1, aRepository.Host.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_text(statement->Value, 2, aRepository.Account.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_text(statement->Value, 3, aRepository.Repository.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}

	auto queue_enrollment_database::load_saved_repositories()
		-> std::expected<std::vector<application::saved_repository>, failure>
	{
		auto statement = prepare(
			Handle,
			"SELECT host, account, repository FROM saved_repository "
			"ORDER BY host, account, repository");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		auto saved = std::vector<application::saved_repository>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(statement->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (saved.size() >= application::maximum_saved_repositories) {
				return std::unexpected(failure::quota_exceeded);
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			auto repository = application::saved_repository{
				.Host       = text_of(statement->Value, 0),
				.Account    = text_of(statement->Value, 1),
				.Repository = text_of(statement->Value, 2),
			};
			if (!application::valid(repository)) {
				return std::unexpected(failure::integrity_failure);
			}
			saved.push_back(std::move(repository));
		}
		return saved;
	}

	auto queue_enrollment_database::store_saved_server(
		const application::saved_server& aServer) -> std::expected<void, failure>
	{
		if (!application::valid(aServer)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"INSERT INTO saved_server(hostname, account) SELECT ?1, ?2 "
			"WHERE (SELECT COUNT(*) FROM (SELECT 1 FROM saved_server LIMIT ?3)) < ?3 "
			"OR EXISTS(SELECT 1 FROM saved_server WHERE hostname = ?1) "
			"ON CONFLICT(hostname) DO UPDATE SET account = excluded.account");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_int64(statement->Value, 3, static_cast<std::int64_t>(application::maximum_saved_servers)) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_bind_text(statement->Value, 1, aServer.Hostname.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_bind_text(statement->Value, 2, aServer.Account.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_changes(Handle) == 0) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}
	auto queue_enrollment_database::erase_saved_server(
		const application::saved_server& aServer) -> std::expected<void, failure>
	{
		if (!application::valid(aServer)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(Handle, "DELETE FROM saved_server WHERE hostname = ?1");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_text(statement->Value, 1, aServer.Hostname.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}
	auto queue_enrollment_database::load_saved_servers()
		-> std::expected<std::vector<application::saved_server>, failure>
	{
		auto statement = prepare(Handle, "SELECT hostname, account FROM saved_server ORDER BY hostname");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		auto saved = std::vector<application::saved_server>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(statement->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (saved.size() >= application::maximum_saved_servers) {
				return std::unexpected(failure::quota_exceeded);
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			auto server = application::saved_server{
				.Hostname = text_of(statement->Value, 0),
				.Account  = text_of(statement->Value, 1),
			};
			if (!application::valid(server)) {
				return std::unexpected(failure::integrity_failure);
			}
			saved.push_back(std::move(server));
		}
		return saved;
	}
	auto queue_enrollment_database::store_rule(
		const core::observer_rule& aRule) -> std::expected<void, failure>
	{
		if (!core::valid(aRule)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"INSERT INTO observer_rule(ordinal, rule_kind, rule_pattern, rule_excluding) "
			"SELECT (SELECT COALESCE(MAX(ordinal), 0) + 1 FROM observer_rule), ?1, ?2, ?3 "
			"WHERE (SELECT COUNT(*) FROM (SELECT 1 FROM observer_rule LIMIT ?4)) < ?4 "
			"OR EXISTS(SELECT 1 FROM observer_rule WHERE rule_kind = ?1 AND rule_pattern = ?2 AND rule_excluding = ?3) "
			"ON CONFLICT(rule_kind, rule_pattern, rule_excluding) DO UPDATE SET rule_pattern = excluded.rule_pattern");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_int64(statement->Value, 4, static_cast<std::int64_t>(application::maximum_queue_rules)) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_bind_int64(
				statement->Value,
				1,
				static_cast<std::int64_t>(aRule.Kind))
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   2,
				   aRule.Pattern.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value, 3, aRule.Excluding ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_changes(Handle) == 0) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}

	auto queue_enrollment_database::erase_rule(
		const core::observer_rule& aRule) -> std::expected<void, failure>
	{
		if (!core::valid(aRule)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"DELETE FROM observer_rule WHERE rule_kind = ?1 "
			"AND rule_pattern = ?2 AND rule_excluding = ?3");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		if (::sqlite3_bind_int64(
				statement->Value,
				1,
				static_cast<std::int64_t>(aRule.Kind))
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   2,
				   aRule.Pattern.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value, 3, aRule.Excluding ? 1 : 0)
				!= SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}

	auto queue_enrollment_database::apply_rule_change(
		const application::queue_rule_intent&                 aIntent,
		const std::span<const application::enrollment_record> aEnrollments,
		std::stop_token                                       aStop)
		-> std::expected<void, failure>
	{
		if (aEnrollments.size() > application::maximum_queue_enrollments) {
			return std::unexpected(failure::quota_exceeded);
		}
		if (!core::valid(aIntent.Rule)) {
			return std::unexpected(failure::integrity_failure);
		}
		if (auto begun = execute(Handle, "BEGIN IMMEDIATE", aStop); !begun) {
			return begun;
		}
		const auto rollback = [this]() noexcept {
			(void)::sqlite3_exec(Handle, "ROLLBACK", nullptr, nullptr, nullptr);
		};
		const auto changed = aIntent.Action == application::queue_rule_action::add
			? store_rule(aIntent.Rule)
			: erase_rule(aIntent.Rule);
		if (!changed) {
			rollback();
			return changed;
		}
		for (const auto& enrollment : aEnrollments) {
			if (aStop.stop_requested()) {
				rollback();
				return std::unexpected(failure::cancelled);
			}
			if (auto stored = store(enrollment); !stored) {
				rollback();
				return stored;
			}
		}
		if (auto committed = execute(Handle, "COMMIT", aStop); !committed) {
			rollback();
			return committed;
		}
		return { };
	}

	auto queue_enrollment_database::load_rules()
		-> std::expected<std::vector<core::observer_rule>, failure>
	{
		auto statement = prepare(
			Handle,
			"SELECT rule_kind, rule_pattern, rule_excluding FROM observer_rule "
			"ORDER BY ordinal");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		auto rules = std::vector<core::observer_rule>{ };
		for (;;) {
			const auto stepped = ::sqlite3_step(statement->Value);
			if (stepped == SQLITE_DONE) {
				break;
			}
			if (rules.size() >= application::maximum_queue_rules) {
				return std::unexpected(failure::quota_exceeded);
			}
			if (stepped != SQLITE_ROW) {
				return std::unexpected(failure::integrity_failure);
			}
			auto rule = core::observer_rule{
				.Kind = static_cast<core::observer_rule_kind>(
					::sqlite3_column_int64(statement->Value, 0)),
				.Pattern   = text_of(statement->Value, 1),
				.Excluding = ::sqlite3_column_int64(statement->Value, 2) != 0,
			};
			if (!core::valid(rule)) {
				return std::unexpected(failure::integrity_failure);
			}
			rules.push_back(std::move(rule));
		}
		return rules;
	}

	auto queue_enrollment_database::store_preferences(
		const application::queue_presentation_preferences& aPreferences)
		-> std::expected<void, failure>
	{
		if (!application::valid(aPreferences)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto statement = prepare(
			Handle,
			"INSERT INTO presentation_preferences("
			"singleton, role_filter, observer_filter, bucket_filter,"
			"repository_filter, quiet_filter, sort_order, notifications_enabled, notifications_paused) "
			"VALUES(1, ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8) "
			"ON CONFLICT(singleton) DO UPDATE SET "
			"role_filter = excluded.role_filter,"
			"observer_filter = excluded.observer_filter,"
			"bucket_filter = excluded.bucket_filter,"
			"repository_filter = excluded.repository_filter,"
			"quiet_filter = excluded.quiet_filter,"
			"sort_order = excluded.sort_order,"
			"notifications_enabled = excluded.notifications_enabled,"
			"notifications_paused = excluded.notifications_paused");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		const auto optionalValue = [](const auto& aValue) -> std::int64_t {
			return aValue.has_value()
				? static_cast<std::int64_t>(*aValue)
				: -1;
		};
		if (::sqlite3_bind_int64(
				statement->Value, 1, optionalValue(aPreferences.Role))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   2,
				   optionalValue(aPreferences.ObserverReason))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value, 3, optionalValue(aPreferences.Bucket))
				!= SQLITE_OK
			|| ::sqlite3_bind_text(
				   statement->Value,
				   4,
				   aPreferences.Repository.c_str(),
				   -1,
				   SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   5,
				   static_cast<std::int64_t>(aPreferences.Quiet))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   statement->Value,
				   6,
				   static_cast<std::int64_t>(aPreferences.Sort))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 7, aPreferences.Notifications.Enabled ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_bind_int64(statement->Value, 8, aPreferences.Notifications.Paused ? 1 : 0) != SQLITE_OK
			|| ::sqlite3_step(statement->Value) != SQLITE_DONE) {
			return std::unexpected(failure::unavailable);
		}
		return { };
	}

	auto queue_enrollment_database::load_preferences()
		-> std::expected<application::queue_presentation_preferences, failure>
	{
		auto statement = prepare(
			Handle,
			"SELECT role_filter, observer_filter, bucket_filter,"
			"repository_filter, quiet_filter, sort_order, notifications_enabled, notifications_paused "
			"FROM presentation_preferences WHERE singleton = 1");
		if (!statement) {
			return std::unexpected(statement.error());
		}
		const auto stepped = ::sqlite3_step(statement->Value);
		if (stepped == SQLITE_DONE) {
			return application::queue_presentation_preferences{ };
		}
		if (stepped != SQLITE_ROW) {
			return std::unexpected(failure::integrity_failure);
		}
		const auto optionalRole = [](const std::int64_t aValue)
			-> std::optional<core::github_role> {
			return aValue < 0
				? std::nullopt
				: std::optional{
					  static_cast<core::github_role>(aValue)
				  };
		};
		const auto optionalObserver = [](const std::int64_t aValue)
			-> std::optional<core::observer_rule_kind> {
			return aValue < 0
				? std::nullopt
				: std::optional{
					  static_cast<core::observer_rule_kind>(aValue)
				  };
		};
		const auto optionalBucket = [](const std::int64_t aValue)
			-> std::optional<core::attention_bucket> {
			return aValue < 0
				? std::nullopt
				: std::optional{
					  static_cast<core::attention_bucket>(aValue)
				  };
		};
		auto preferences = application::queue_presentation_preferences{
			.Role           = optionalRole(::sqlite3_column_int64(statement->Value, 0)),
			.ObserverReason = optionalObserver(
				::sqlite3_column_int64(statement->Value, 1)),
			.Bucket = optionalBucket(
				::sqlite3_column_int64(statement->Value, 2)),
			.Repository = text_of(statement->Value, 3),
			.Quiet      = static_cast<application::queue_quiet_filter>(
				::sqlite3_column_int64(statement->Value, 4)),
			.Sort = static_cast<application::queue_sort_order>(
				::sqlite3_column_int64(statement->Value, 5)),
			.Notifications = { ::sqlite3_column_int64(statement->Value, 6) == 1, ::sqlite3_column_int64(statement->Value, 7) == 1 },
		};
		if (!application::valid(preferences)) {
			return std::unexpected(failure::integrity_failure);
		}
		return preferences;
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
