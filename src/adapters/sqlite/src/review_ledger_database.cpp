#include "review_ledger_database.hpp"

#include "private_review_database.hpp"
#include "storage_internal.hpp"

#include "bpr/core/hex.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		constexpr std::int32_t LedgerApplicationId = 0x42505244;

		constexpr auto OpenFlags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
			| SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_EXRESCODE;

		/// \brief One of SQLite's sibling files, `-wal` or `-shm`, of `aPath`.
		///
		/// \details Appended to the path itself. A narrow `path.string()` is the
		/// ANSI code page on Windows, which cannot spell a root outside it and
		/// throws there ([[BPR-0883]]).
		[[nodiscard]] auto sibling(
			const std::filesystem::path& aPath, const char* const aSuffix)
			-> std::filesystem::path
		{
			auto result = aPath;
			result += aSuffix;
			return result;
		}

		/// \brief The UTF-8 name SQLite takes on every platform.
		[[nodiscard]] auto sqlite_filename(const std::filesystem::path& aPath)
			-> std::string
		{
			const auto encoded = aPath.u8string();
			return std::string{ reinterpret_cast<const char*>(encoded.data()), encoded.size() };
		}

#if defined(BPR_SQLITE_TESTING)
		using preflight_test_hook                 = void (*)(const std::filesystem::path&);
		preflight_test_hook PreflightBoundaryHook = nullptr;
		preflight_test_hook PreflightCleanupHook  = nullptr;

		void run_test_hook(
			preflight_test_hook&         aHook,
			const std::filesystem::path& aPath)
		{
			if (const auto aHeldHook = std::exchange(aHook, nullptr);
				aHeldHook != nullptr) {
				aHeldHook(aPath);
			}
		}
#endif

		template <typename Bytes>
		[[nodiscard]] auto bind_blob(
			::sqlite3_stmt* aStatement,
			const int       aPosition,
			const Bytes&    aBytes) noexcept -> int
		{
			return ::sqlite3_bind_blob(
				aStatement,
				aPosition,
				aBytes.data(),
				static_cast<int>(aBytes.size()),
				SQLITE_TRANSIENT);
		}

		[[nodiscard]] auto read_digest(
			::sqlite3_stmt* aStatement,
			const int       aColumn) noexcept
			-> std::optional<std::array<std::uint8_t, 32>>
		{
			if (::sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(aStatement, aColumn) != 32) {
				return std::nullopt;
			}
			const auto* source = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aStatement, aColumn));
			if (source == nullptr) {
				return std::nullopt;
			}
			auto bytes = std::array<std::uint8_t, 32>{ };
			std::copy_n(source, bytes.size(), bytes.begin());
			return bytes;
		}

		[[nodiscard]] constexpr auto durable_integer(
			const std::uint64_t aValue) noexcept -> bool
		{
			return aValue > 0U
				&& aValue
				<= static_cast<std::uint64_t>(
					   std::numeric_limits<std::int64_t>::max());
		}

		[[nodiscard]] auto read_agent_session(::sqlite3_stmt* aStatement)
			-> std::optional<core::agent_session>
		{
			const auto session       = ::sqlite3_column_int64(aStatement, 0);
			const auto profile       = ::sqlite3_column_int64(aStatement, 1);
			const auto host          = ::sqlite3_column_int64(aStatement, 2);
			const auto repository    = ::sqlite3_column_int64(aStatement, 3);
			const auto pullRequest   = ::sqlite3_column_int64(aStatement, 4);
			const auto baseAlgorithm = ::sqlite3_column_int64(aStatement, 5);
			const auto base          = read_digest(aStatement, 6);
			const auto headAlgorithm = ::sqlite3_column_int64(aStatement, 7);
			const auto head          = read_digest(aStatement, 8);
			const auto left          = ::sqlite3_column_int64(aStatement, 9);
			const auto right         = ::sqlite3_column_int64(aStatement, 10);
			if (session <= 0 || profile <= 0 || host <= 0 || repository <= 0
				|| pullRequest <= 0 || baseAlgorithm < 1 || baseAlgorithm > 2
				|| headAlgorithm < 1 || headAlgorithm > 2 || !base || !head
				|| left <= 0 || right <= 0) {
				return std::nullopt;
			}
			auto result = core::agent_session{
				.Id      = core::agent_session_id{ static_cast<std::uint64_t>(session) },
				.Profile = core::agent_profile_id{ static_cast<std::uint64_t>(profile) },
				.Binding = {
					.Host        = core::git_host_id{ static_cast<std::uint64_t>(host) },
					.Repository  = core::repository_id{ static_cast<std::uint64_t>(repository) },
					.PullRequest = core::pull_request_id{ static_cast<std::uint64_t>(pullRequest) },
					.BaseOid     = core::git_commit_oid{
						static_cast<core::git_hash_algorithm>(baseAlgorithm), *base },
					.HeadOid    = core::git_commit_oid{ static_cast<core::git_hash_algorithm>(headAlgorithm), *head },
					.LeftEpoch  = core::epoch_ordinal{ left },
					.RightEpoch = core::epoch_ordinal{ right },
				},
			};
			return application::valid(result) ? std::optional{ result } : std::nullopt;
		}

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

		/// \brief The statements that carry schema version 2 to version 3.
		///
		/// \details Notes carry either no attachment or an exact file/comparison
		/// pair. Heads retain the algorithm beside the padded digest. Defects keep
		/// both semantic anchors even though current policy requires them equal, so
		/// losing shared context never erases the original complaint.
		constexpr std::array SchemaVersion2To3{
			std::string_view{
				"CREATE TABLE local_note("
				"scope_id INTEGER NOT NULL "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"note_id INTEGER NOT NULL CHECK(note_id > 0),"
				"file_identity BLOB,"
				"comparison_id BLOB,"
				"note_text TEXT NOT NULL "
				"CHECK(length(CAST(note_text AS BLOB)) BETWEEN 1 AND 16384),"
				"CHECK((file_identity IS NULL AND comparison_id IS NULL) OR "
				"(length(file_identity) = 32 AND length(comparison_id) = 32)),"
				"PRIMARY KEY(scope_id, note_id)"
				") STRICT" },
			std::string_view{
				"CREATE TABLE last_reviewed_head("
				"scope_id INTEGER PRIMARY KEY "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"oid_algorithm INTEGER NOT NULL CHECK(oid_algorithm BETWEEN 1 AND 2),"
				"head_oid BLOB NOT NULL CHECK(length(head_oid) = 32)"
				") STRICT" },
			std::string_view{
				"CREATE TABLE local_defect("
				"scope_id INTEGER NOT NULL "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"defect_id INTEGER NOT NULL CHECK(defect_id > 0),"
				"file_identity BLOB NOT NULL CHECK(length(file_identity) = 32),"
				"conversation INTEGER NOT NULL CHECK(conversation > 0),"
				"anchor_comparison BLOB NOT NULL CHECK(length(anchor_comparison) = 32),"
				"anchor_side INTEGER NOT NULL CHECK(anchor_side BETWEEN 0 AND 1),"
				"anchor_line INTEGER NOT NULL CHECK(anchor_line BETWEEN 0 AND 4294967295),"
				"original_comparison BLOB NOT NULL CHECK(length(original_comparison) = 32),"
				"original_side INTEGER NOT NULL CHECK(original_side BETWEEN 0 AND 1),"
				"original_line INTEGER NOT NULL CHECK(original_line BETWEEN 0 AND 4294967295),"
				"severity INTEGER NOT NULL CHECK(severity BETWEEN 0 AND 3),"
				"category INTEGER NOT NULL CHECK(category BETWEEN 0 AND 6),"
				"lifecycle INTEGER NOT NULL CHECK(lifecycle BETWEEN 0 AND 6),"
				"availability INTEGER NOT NULL CHECK(availability BETWEEN 0 AND 2),"
				"CHECK(anchor_comparison = original_comparison AND "
				"anchor_side = original_side AND anchor_line = original_line),"
				"PRIMARY KEY(scope_id, defect_id),"
				"UNIQUE(scope_id, conversation)"
				") STRICT" },
		};

		/// \brief The statements that carry schema version 3 to version 4.
		///
		/// \details The sequence survives bounded row retention and is therefore
		/// the sole allocation authority. Session rows contain immutable revision
		/// metadata only; private prose and process/workspace facts have no column.
		constexpr std::array SchemaVersion3To4{
			std::string_view{
				"CREATE TABLE agent_session_sequence("
				"scope_id INTEGER PRIMARY KEY "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"last_session_id INTEGER NOT NULL "
				"CHECK(last_session_id BETWEEN 0 AND 9223372036854775807)"
				") STRICT" },
			std::string_view{
				"CREATE TABLE agent_session("
				"scope_id INTEGER NOT NULL "
				"REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
				"session_id INTEGER NOT NULL CHECK(session_id > 0),"
				"profile_id INTEGER NOT NULL CHECK(profile_id > 0),"
				"host_id INTEGER NOT NULL CHECK(host_id > 0),"
				"repository_id INTEGER NOT NULL CHECK(repository_id > 0),"
				"pull_request_id INTEGER NOT NULL CHECK(pull_request_id > 0),"
				"base_oid_algorithm INTEGER NOT NULL "
				"CHECK(base_oid_algorithm BETWEEN 1 AND 2),"
				"base_oid BLOB NOT NULL CHECK(length(base_oid) = 32),"
				"head_oid_algorithm INTEGER NOT NULL "
				"CHECK(head_oid_algorithm BETWEEN 1 AND 2),"
				"head_oid BLOB NOT NULL CHECK(length(head_oid) = 32),"
				"left_epoch INTEGER NOT NULL CHECK(left_epoch > 0),"
				"right_epoch INTEGER NOT NULL CHECK(right_epoch > 0),"
				"PRIMARY KEY(scope_id, session_id)"
				") STRICT" },
		};

		/// \brief The statements that carry schema version 4 to version 5.
		///
		/// \details Every `local_defect` is a private classification of an ordinary
		/// GitHub conversation. Before version 5, classification could persist a
		/// one-based line, force a left-side thread to the right, use an outdated
		/// original line, and bind the thread to an arbitrary retained comparison.
		/// The line correction keeps both immutable copies consistent, but none of
		/// the remaining identity can be recovered from SQLite. Marking every legacy
		/// row outdated retains its audit anchor without presenting it as placeable.
		static_assert(
			static_cast<std::uint8_t>(
				core::defect_anchor_availability::outdated)
			== 1U);
		constexpr std::array SchemaVersion4To5{
			std::string_view{
				"UPDATE local_defect SET anchor_line = CASE WHEN anchor_line > 0 "
				"THEN anchor_line - 1 ELSE anchor_line END, original_line = CASE "
				"WHEN original_line > 0 THEN original_line - 1 ELSE original_line "
				"END, availability = 1 WHERE conversation > 0" },
		};

		constexpr std::string_view SchemaVersion6Note     = "CREATE TABLE local_note("
															"scope_id INTEGER NOT NULL REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
															"note_id INTEGER NOT NULL CHECK(note_id > 0),"
															"file_identity BLOB,comparison_id BLOB,"
															"note_text TEXT NOT NULL CHECK(length(CAST(note_text AS BLOB)) BETWEEN 1 AND 16384),"
															"revision INTEGER NOT NULL CHECK(revision > 0),"
															"CHECK((file_identity IS NULL AND comparison_id IS NULL) OR "
															"(length(file_identity) = 32 AND length(comparison_id) = 32)),"
															"PRIMARY KEY(scope_id, note_id)) STRICT";
		constexpr std::string_view SchemaVersion6Sequence = "CREATE TABLE local_note_sequence("
															"scope_id INTEGER PRIMARY KEY REFERENCES review_scope(scope_id) ON DELETE CASCADE,"
															"last_revision INTEGER NOT NULL CHECK(last_revision > 0)) STRICT";
		constexpr std::array       SchemaVersion5To6{
			std::string_view{ "ALTER TABLE local_note RENAME TO local_note_v5" },
			SchemaVersion6Note,
			std::string_view{ "INSERT INTO local_note SELECT scope_id,note_id,file_identity,comparison_id,note_text,1 FROM local_note_v5" },
			SchemaVersion6Sequence,
			std::string_view{ "INSERT INTO local_note_sequence SELECT scope_id,MAX(revision) FROM local_note GROUP BY scope_id" },
			std::string_view{ "DROP TABLE local_note_v5" },
		};

		struct schema_table
		{
			std::string_view Name;
			std::string_view Sql;
		};

		/// \brief The complete canonical version-three ledger schema.
		///
		/// \details Kept in name order so validation can compare the live
		/// `sqlite_schema` stream without allocating or sorting. Canonical SQL
		/// comparison makes columns, constraints, foreign keys, and `STRICT`
		/// table mode compatibility facts rather than trusting table names.
		constexpr std::array SchemaVersion3Tables{
			schema_table{ "conversation_receipt", SchemaVersion1To2[0] },
			schema_table{ "file_receipt", SchemaVersion1[2] },
			schema_table{ "last_reviewed_head", SchemaVersion2To3[1] },
			schema_table{ "ledger_meta", SchemaVersion1[0] },
			schema_table{ "local_defect", SchemaVersion2To3[2] },
			schema_table{ "local_note", SchemaVersion2To3[0] },
			schema_table{ "review_scope", SchemaVersion1[1] },
		};

		constexpr std::array SchemaVersion1Tables{
			schema_table{ "file_receipt", SchemaVersion1[2] },
			schema_table{ "ledger_meta", SchemaVersion1[0] },
			schema_table{ "review_scope", SchemaVersion1[1] },
		};

		constexpr std::array SchemaVersion2Tables{
			schema_table{ "conversation_receipt", SchemaVersion1To2[0] },
			schema_table{ "file_receipt", SchemaVersion1[2] },
			schema_table{ "ledger_meta", SchemaVersion1[0] },
			schema_table{ "review_scope", SchemaVersion1[1] },
		};

		constexpr std::array SchemaVersion4Tables{
			schema_table{ "agent_session", SchemaVersion3To4[1] },
			schema_table{ "agent_session_sequence", SchemaVersion3To4[0] },
			schema_table{ "conversation_receipt", SchemaVersion1To2[0] },
			schema_table{ "file_receipt", SchemaVersion1[2] },
			schema_table{ "last_reviewed_head", SchemaVersion2To3[1] },
			schema_table{ "ledger_meta", SchemaVersion1[0] },
			schema_table{ "local_defect", SchemaVersion2To3[2] },
			schema_table{ "local_note", SchemaVersion2To3[0] },
			schema_table{ "review_scope", SchemaVersion1[1] },
		};

		constexpr auto SchemaVersion5Tables = SchemaVersion4Tables;

		constexpr std::array SchemaVersion6Tables{
			schema_table{ "agent_session", SchemaVersion3To4[1] },
			schema_table{ "agent_session_sequence", SchemaVersion3To4[0] },
			schema_table{ "conversation_receipt", SchemaVersion1To2[0] },
			schema_table{ "file_receipt", SchemaVersion1[2] },
			schema_table{ "last_reviewed_head", SchemaVersion2To3[1] },
			schema_table{ "ledger_meta", SchemaVersion1[0] },
			schema_table{ "local_defect", SchemaVersion2To3[2] },
			schema_table{ "local_note", SchemaVersion6Note },
			schema_table{ "local_note_sequence", SchemaVersion6Sequence },
			schema_table{ "review_scope", SchemaVersion1[1] },
		};

		const auto SchemaVersion7Tables = [] {
			auto tables = std::vector<schema_table>{ };
			for (const auto& table : SchemaVersion6Tables) {
				if (table.Name != "local_note" && table.Name != "local_note_sequence") {
					tables.push_back(table);
				}
			}
			for (const auto& table : PrivateSchemaTables) {
				tables.push_back({ table.Name, table.Sql });
			}
			std::ranges::sort(tables, { }, &schema_table::Name);
			return tables;
		}();

		/// [[BPR-1210]]. Schema 8 adds the note path table to schema 7.
		const auto SchemaVersion8Tables = [] {
			auto tables = SchemaVersion7Tables;
			tables.push_back({ PrivateNotePathTable.Name, PrivateNotePathTable.Sql });
			std::ranges::sort(tables, { }, &schema_table::Name);
			return tables;
		}();

		const auto SchemaVersion9Tables = [] {
			auto tables = SchemaVersion8Tables;
			for (auto& table : tables) {
				if (table.Name == "private_scope_state") {
					table.Sql = PrivateRetainedDraftState;
				}
			}
			return tables;
		}();

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
			// Forward only. A newer database is refused rather than opened,
			// because this build cannot know what the newer version added and
			// would silently drop it on the next write.
			if (*current > review_ledger_schema_version) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (*current != 0) {
				const auto application
					= scalar_integer(aDatabase, "PRAGMA application_id");
				if (!application || *application != LedgerApplicationId) {
					return std::unexpected(failure::schema_mismatch);
				}
			}
			if (*current == review_ledger_schema_version) {
				return { };
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
			if (version == 2) {
				if (auto migrated
					= run_all(aDatabase, SchemaVersion2To3, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
				version = 3;
			}
			if (version == 3) {
				if (auto migrated
					= run_all(aDatabase, SchemaVersion3To4, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
				version = 4;
			}
			if (version == 4) {
				if (auto migrated
					= run_all(aDatabase, SchemaVersion4To5, aStop);
					!migrated) {
					rollback();
					return migrated;
				}
				version = 5;
			}

			if (version == 5) {
				if (auto migrated = run_all(aDatabase, SchemaVersion5To6, aStop); !migrated) {
					rollback();
					return migrated;
				}
				version = 6;
			}

			if (version == 6) {
				if (auto migrated = migrate_private_review(aDatabase); !migrated) {
					rollback();
					return migrated;
				}
				version = 7;
			}

			if (version == 7) {
				if (auto migrated = migrate_private_note_paths(aDatabase); !migrated) {
					rollback();
					return migrated;
				}
				version = 8;
			}

			if (version == 8) {
				const std::array statements{ std::string_view{ "ALTER TABLE private_scope_state RENAME TO private_scope_state_v8" }, PrivateRetainedDraftState, std::string_view{ "INSERT INTO private_scope_state SELECT * FROM private_scope_state_v8" }, std::string_view{ "DROP TABLE private_scope_state_v8" } };
				if (auto migrated = run_all(aDatabase, statements, aStop); !migrated) {
					rollback();
					return migrated;
				}
				version = 9;
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

		[[nodiscard]] auto validate_schema_objects(
			::sqlite3*                          aDatabase,
			const std::span<const schema_table> aExpected)
			-> std::expected<void, failure>
		{
			auto objects = prepare(
				aDatabase,
				"SELECT type, name, sql FROM sqlite_schema "
				"WHERE name NOT LIKE 'sqlite_%' ORDER BY name");
			if (!objects) {
				return std::unexpected(objects.error());
			}
			std::size_t index = 0;
			while (true) {
				const auto row = ::sqlite3_step(objects->Value);
				if (row == SQLITE_DONE) {
					break;
				}
				if (row != SQLITE_ROW || index >= aExpected.size()) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto text = [&objects](const int aColumn)
					-> std::optional<std::string_view> {
					if (::sqlite3_column_type(objects->Value, aColumn) != SQLITE_TEXT) {
						return std::nullopt;
					}
					const auto* value = reinterpret_cast<const char*>(
						::sqlite3_column_text(objects->Value, aColumn));
					const auto bytes = ::sqlite3_column_bytes(objects->Value, aColumn);
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
					|| *name != aExpected[index].Name
					|| canonical_sql(*sql)
						!= canonical_sql(aExpected[index].Sql)) {
					return std::unexpected(failure::schema_mismatch);
				}
				++index;
			}
			if (index != aExpected.size()) {
				return std::unexpected(failure::schema_mismatch);
			}
			return { };
		}

		[[nodiscard]] auto validate_at_version(
			::sqlite3*                          aDatabase,
			const std::int64_t                  aVersion,
			const std::span<const schema_table> aExpected)
			-> std::expected<void, failure>
		{
			const auto application
				= scalar_integer(aDatabase, "PRAGMA application_id");
			const auto version = scalar_integer(aDatabase, "PRAGMA user_version");
			if (!application || *application != LedgerApplicationId || !version
				|| *version != aVersion) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto meta = prepare(
				aDatabase,
				"SELECT schema_format FROM ledger_meta WHERE singleton = 1");
			if (!meta || ::sqlite3_step(meta->Value) != SQLITE_ROW
				|| ::sqlite3_column_int64(meta->Value, 0) != aVersion
				|| ::sqlite3_step(meta->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return validate_schema_objects(aDatabase, aExpected);
		}

		[[nodiscard]] auto validate(::sqlite3* aDatabase)
			-> std::expected<void, failure>
		{
			return validate_at_version(
				aDatabase,
				review_ledger_schema_version,
				SchemaVersion9Tables);
		}

		struct preflight_file_image
		{
			bool        Exists{ false };
			std::string Bytes;

			auto operator<=>(const preflight_file_image&) const = default;
		};

		struct preflight_ledger_image
		{
			preflight_file_image Main;
			preflight_file_image Wal;
			preflight_file_image Shm;

			auto operator<=>(const preflight_ledger_image&) const = default;
		};

		[[nodiscard]] auto capture_preflight_file(
			const std::filesystem::path& aPath)
			-> std::expected<preflight_file_image, failure>
		{
			auto       error  = std::error_code{ };
			const auto status = std::filesystem::status(aPath, error);
			// `status` reports a missing path as `not_found` and, unlike
			// `exists`, leaves the error code set; a ledger that does not exist
			// yet is the ordinary first open, not an obstruction.
			if (status.type() == std::filesystem::file_type::not_found) {
				return preflight_file_image{ };
			}
			if (error) {
				return std::unexpected(failure::unavailable);
			}
			// A path that exists but is not a file -- a directory left in the
			// ledger's place is the shape the composition contract forces -- is
			// refused before anything reads it. Opening a directory as a
			// stream succeeds on POSIX and the failure only arrives at the
			// first read, where the two standard libraries disagree: libc++
			// hands back an empty image that the schema preflight then
			// misreports as a version mismatch, and libstdc++ throws
			// `std::ios_base::failure` out of `basic_filebuf::underflow`
			// whatever the stream's exception mask says (BPR-0472). Deciding
			// on the file type first gives every platform the same typed
			// refusal.
			if (!std::filesystem::is_regular_file(status)) {
				return std::unexpected(failure::unavailable);
			}
			auto input = std::ifstream{ aPath, std::ios::binary };
			if (!input) {
				return std::unexpected(failure::unavailable);
			}
			// The read itself is contained as well. This function owes its
			// caller a typed failure whatever goes wrong underneath, and an
			// exception escaping an `std::expected` boundary is not one: it
			// unwinds through the whole composition above it, which is how the
			// directory case hung the WSL2 epoch handoff before the check above
			// existed.
			auto bytes = std::string{ };
			try {
				bytes.assign(
					std::istreambuf_iterator<char>{ input },
					std::istreambuf_iterator<char>{ });
			} catch (const std::ios_base::failure&) {
				return std::unexpected(failure::unavailable);
			}
			if (input.bad()) {
				return std::unexpected(failure::unavailable);
			}
			return preflight_file_image{
				.Exists = true,
				.Bytes  = std::move(bytes),
			};
		}

		[[nodiscard]] auto capture_preflight_ledger(
			const std::filesystem::path& aPath)
			-> std::expected<preflight_ledger_image, failure>
		{
			const auto main = capture_preflight_file(aPath);
			const auto wal
				= capture_preflight_file(sibling(aPath, "-wal"));
			const auto shm
				= capture_preflight_file(sibling(aPath, "-shm"));
			if (!main || !wal || !shm) {
				return std::unexpected(failure::unavailable);
			}
			return preflight_ledger_image{ *main, *wal, *shm };
		}

		class preflight_directory final
		{
		public:
			preflight_directory() = default;
			preflight_directory(
				std::filesystem::path aRoot,
				std::filesystem::path aClone)
				: Root{ std::move(aRoot) }
				, Clone{ std::move(aClone) }
			{
			}
			preflight_directory(const preflight_directory&) = delete;
			auto operator=(const preflight_directory&)
				-> preflight_directory& = delete;
			preflight_directory(preflight_directory&& aOther) noexcept
				: Root{ std::exchange(aOther.Root, { }) }
				, Clone{ std::exchange(aOther.Clone, { }) }
				, IdentityRetained{ std::exchange(aOther.IdentityRetained, false) }
				, CleanupAttempted{ std::exchange(aOther.CleanupAttempted, true) }
			{
			}
			auto operator=(preflight_directory&&)
				-> preflight_directory& = delete;
			~preflight_directory()
			{
				(void)cleanup();
			}

			[[nodiscard]] auto file() const -> std::filesystem::path
			{
				return Clone / "ledger.db";
			}

			[[nodiscard]] auto authority(const std::string_view aName) const
				-> std::filesystem::path
			{
				return Root / ("." + std::string{ aName } + "-authority");
			}

			[[nodiscard]] auto initialize()
				-> std::expected<void, failure>
			{
				auto error = std::error_code{ };
				std::filesystem::create_directory(Clone, error);
				if (error) {
					return std::unexpected(failure::unavailable);
				}
				std::filesystem::permissions(
					Clone,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace,
					error);
				if (error) {
					return std::unexpected(failure::unavailable);
				}
				auto identity = std::ofstream{
					Clone / ".identity",
					std::ios::binary | std::ios::trunc,
				};
				identity << "bpr-preflight-identity";
				identity.close();
				if (!identity) {
					return std::unexpected(failure::unavailable);
				}
				std::filesystem::create_hard_link(
					Clone / ".identity", authority("identity"), error);
				if (error) {
					return std::unexpected(failure::unavailable);
				}
				IdentityRetained = true;
				return { };
			}

			[[nodiscard]] auto cleanup() noexcept
				-> std::expected<void, failure>
			{
				if (CleanupAttempted || Root.empty()) {
					return { };
				}
				CleanupAttempted = true;
				auto failed      = false;
				if (!IdentityRetained) {
					auto       error       = std::error_code{ };
					const auto cloneExists = std::filesystem::exists(Clone, error);
					if (error) {
						return std::unexpected(failure::unavailable);
					}
					if (cloneExists) {
						(void)std::filesystem::remove(Clone / ".identity", error);
						failed = static_cast<bool>(error);
						error.clear();
						const auto cloneEmpty
							= std::filesystem::is_empty(Clone, error);
						if (error || !cloneEmpty) {
							return std::unexpected(failure::unavailable);
						}
						(void)std::filesystem::remove(Clone, error);
						if (error) {
							return std::unexpected(failure::unavailable);
						}
					}
					const auto rootEmpty = std::filesystem::is_empty(Root, error);
					if (error || !rootEmpty) {
						return std::unexpected(failure::unavailable);
					}
					(void)std::filesystem::remove(Root, error);
					if (error || failed) {
						return std::unexpected(failure::unavailable);
					}
					Root.clear();
					Clone.clear();
					return { };
				}
				for (const auto name : { "main", "wal" }) {
					const auto path  = authority(name);
					auto       error = std::error_code{ };
					if (!std::filesystem::exists(path, error)) {
						failed = failed || static_cast<bool>(error);
						continue;
					}
					auto sanitized = std::ofstream{
						path,
						std::ios::binary | std::ios::trunc,
					};
					sanitized.close();
					failed = failed || !sanitized;
				}
#if defined(BPR_SQLITE_TESTING)
				run_test_hook(PreflightCleanupHook, Clone);
#endif

				auto       error   = std::error_code{ };
				const auto current = std::filesystem::equivalent(
					authority("identity"), Clone / ".identity", error);
				if (error || !current) {
					for (const auto name : { "main", "wal", "identity" }) {
						error.clear();
						(void)std::filesystem::remove(authority(name), error);
					}
					return std::unexpected(failure::unavailable);
				}

				for (const auto name : {
						 "ledger.db",
						 "ledger.db-wal",
						 "ledger.db-shm",
						 "ledger.db-journal",
						 ".identity",
					 }) {
					error.clear();
					(void)std::filesystem::remove(Clone / name, error);
					failed = failed || static_cast<bool>(error);
				}
				for (const auto name : { "main", "wal", "identity" }) {
					error.clear();
					(void)std::filesystem::remove(authority(name), error);
					failed = failed || static_cast<bool>(error);
				}

				error.clear();
				const auto cloneEmpty = std::filesystem::is_empty(Clone, error);
				if (error || !cloneEmpty) {
					return std::unexpected(failure::unavailable);
				}
				(void)std::filesystem::remove(Clone, error);
				if (error || std::filesystem::exists(Clone, error) || error) {
					return std::unexpected(failure::unavailable);
				}
				const auto rootEmpty = std::filesystem::is_empty(Root, error);
				if (error || !rootEmpty) {
					return std::unexpected(failure::unavailable);
				}
				(void)std::filesystem::remove(Root, error);
				if (error || std::filesystem::exists(Root, error) || error || failed) {
					return std::unexpected(failure::unavailable);
				}
				Root.clear();
				Clone.clear();
				return { };
			}

			std::filesystem::path Root;
			std::filesystem::path Clone;
			bool                  IdentityRetained{ false };
			bool                  CleanupAttempted{ false };
		};

		[[nodiscard]] auto make_preflight_directory()
			-> std::expected<preflight_directory, failure>
		{
			auto       error = std::error_code{ };
			const auto root  = std::filesystem::temp_directory_path(error);
			if (error) {
				return std::unexpected(failure::unavailable);
			}
			for (auto attempt = 0; attempt != 32; ++attempt) {
				auto random = std::array<unsigned char, 16U>{ };
				::sqlite3_randomness(static_cast<int>(random.size()), random.data());
				auto name = std::string{ "bpr-ledger-preflight-" };
				core::append_lower_hex(name, random);
				auto candidate = root / name;
				error.clear();
				if (!std::filesystem::create_directory(candidate, error)) {
					if (error) {
						return std::unexpected(failure::unavailable);
					}
					continue;
				}
				std::filesystem::permissions(
					candidate,
					std::filesystem::perms::owner_all,
					std::filesystem::perm_options::replace,
					error);
				if (error) {
					std::filesystem::remove(candidate, error);
					return std::unexpected(failure::unavailable);
				}
				auto result = preflight_directory{
					candidate,
					candidate / "clone",
				};
				if (auto initialized = result.initialize(); !initialized) {
					const auto cleanup = result.cleanup();
					return std::unexpected(
						cleanup ? initialized.error() : cleanup.error());
				}
				return result;
			}
			return std::unexpected(failure::unavailable);
		}

		[[nodiscard]] auto write_preflight_file(
			const std::filesystem::path& aPath,
			const preflight_file_image&  aImage)
			-> std::expected<void, failure>
		{
			if (!aImage.Exists) {
				return { };
			}
			auto output = std::ofstream{
				aPath,
				std::ios::binary | std::ios::trunc,
			};
			if (!output) {
				return std::unexpected(failure::unavailable);
			}
			output.write(
				aImage.Bytes.data(),
				static_cast<std::streamsize>(aImage.Bytes.size()));
			output.close();
			if (!output) {
				return std::unexpected(failure::unavailable);
			}
			return { };
		}

		[[nodiscard]] auto retain_preflight_file(
			const std::filesystem::path& aPath,
			const std::filesystem::path& aAuthority,
			const preflight_file_image&  aImage)
			-> std::expected<void, failure>
		{
			if (!aImage.Exists) {
				return { };
			}
			auto error = std::error_code{ };
			std::filesystem::create_hard_link(aPath, aAuthority, error);
			if (error) {
				return std::unexpected(failure::unavailable);
			}
			return { };
		}

		[[nodiscard]] auto preflight_existing_schema(
			const std::filesystem::path& aPath)
			-> std::expected<preflight_ledger_image, failure>
		{
			const auto before = capture_preflight_ledger(aPath);
			if (!before) {
				return std::unexpected(before.error());
			}
			if (!before->Main.Exists) {
				if (before->Wal.Exists || before->Shm.Exists) {
					return std::unexpected(failure::schema_mismatch);
				}
				return *before;
			}
			auto directory = make_preflight_directory();
			if (!directory) {
				return std::unexpected(directory.error());
			}
			const auto clone      = directory->file();
			auto       validation = write_preflight_file(clone, before->Main);
			if (validation) {
				validation = retain_preflight_file(
					clone, directory->authority("main"), before->Main);
			}
			if (validation) {
				validation = write_preflight_file(
					sibling(clone, "-wal"), before->Wal);
			}
			if (validation) {
				validation = retain_preflight_file(
					sibling(clone, "-wal"),
					directory->authority("wal"),
					before->Wal);
			}

			::sqlite3* database = nullptr;
			auto       code     = SQLITE_ERROR;
			if (validation) {
				const auto native = sqlite_filename(clone);
				code              = ::sqlite3_open_v2(
					native.c_str(),
					&database,
					SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX
						| SQLITE_OPEN_EXRESCODE,
					nullptr);
			}
			const auto close = [](::sqlite3* aDatabase) {
				if (aDatabase != nullptr) {
					(void)::sqlite3_close(aDatabase);
				}
			};
			auto owned = std::unique_ptr<::sqlite3, decltype(close)>{ database, close };
			if (validation && (code != SQLITE_OK || database == nullptr)) {
				validation = std::unexpected(failure::schema_mismatch);
			} else if (validation) {
				const auto version = scalar_integer(database, "PRAGMA user_version");
				if (!version) {
					validation = std::unexpected(failure::schema_mismatch);
				} else {
					auto expected = std::span<const schema_table>{ };
					if (*version == 1) {
						expected = SchemaVersion1Tables;
					} else if (*version == 2) {
						expected = SchemaVersion2Tables;
					} else if (*version == 3) {
						expected = SchemaVersion3Tables;
					} else if (*version == 4) {
						expected = SchemaVersion4Tables;
					} else if (*version == 5) {
						expected = SchemaVersion5Tables;
					} else if (*version == 6) {
						expected = SchemaVersion6Tables;
					} else if (*version == 7) {
						expected = SchemaVersion7Tables;
					} else if (*version == 8) {
						expected = SchemaVersion8Tables;
					} else if (*version == 9) {
						expected = SchemaVersion9Tables;
					} else {
						validation = std::unexpected(failure::schema_mismatch);
					}
					if (expected.data() != nullptr) {
						validation
							= validate_at_version(database, *version, expected);
					}
				}
			}
			owned.reset();
			const auto cleanup = directory->cleanup();
			if (!cleanup) {
				return std::unexpected(cleanup.error());
			}
			const auto after = capture_preflight_ledger(aPath);
			if (!after || *after != *before) {
				return std::unexpected(failure::unavailable);
			}
			if (!validation) {
				return std::unexpected(validation.error());
			}
			return *before;
		}

		/// \brief Validates an existing ledger on a copy, then confirms that the
		/// file itself did not change meanwhile.
		[[nodiscard]] auto preflight_unchanged(const std::filesystem::path& aPath)
			-> std::expected<void, failure>
		{
			const auto preflight = preflight_existing_schema(aPath);
			if (!preflight) {
				return std::unexpected(preflight.error());
			}
#if defined(BPR_SQLITE_TESTING)
			run_test_hook(PreflightBoundaryHook, aPath);
#endif
			const auto current = capture_preflight_ledger(aPath);
			if (!current || *current != *preflight) {
				return std::unexpected(failure::unavailable);
			}
			return { };
		}
	} // namespace

#if defined(BPR_SQLITE_TESTING)
	void set_review_ledger_preflight_boundary_hook_for_testing(
		preflight_test_hook aHook) noexcept
	{
		PreflightBoundaryHook = aHook;
	}

	void set_review_ledger_preflight_cleanup_hook_for_testing(
		preflight_test_hook aHook) noexcept
	{
		PreflightCleanupHook = aHook;
	}
#endif

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
		if (auto unchanged = preflight_unchanged(aPath); !unchanged) {
			return std::unexpected(unchanged.error());
		}

		::sqlite3* database = nullptr;
		const auto native   = aPath.string();
		const auto code     = ::sqlite3_open_v2(
			native.c_str(), &database, OpenFlags, nullptr);
		auto owned = review_ledger_database{ database };
		if (code != SQLITE_OK || database == nullptr) {
			return std::unexpected(catalog_failure_from_sqlite_result(code));
		}
		return adopt(std::move(owned), std::move(aStop));
	}

	auto review_ledger_database::open(
		state_directory&       aDirectory,
		const std::string_view aName,
		std::stop_token        aStop)
		-> std::expected<review_ledger_database, failure>
	{
		if (!sqlite_runtime_ready()) {
			return std::unexpected(failure::unavailable);
		}
		// Admitted before the preflight copies it, so a link or a file that is
		// not private is refused before any byte of it is read.
		if (auto admitted = admit_private_store_file(aDirectory, aName); !admitted) {
			return std::unexpected(private_store_database_failure(admitted.error()));
		}
		const auto path = aDirectory.path()
			/ std::filesystem::path{ std::u8string{ aName.begin(), aName.end() } };
		if (auto unchanged = preflight_unchanged(path); !unchanged) {
			return std::unexpected(unchanged.error());
		}
		auto opened = open_private_store_database(
			aDirectory, aName, OpenFlags | SQLITE_OPEN_NOFOLLOW);
		if (!opened) {
			return std::unexpected(private_store_database_failure(opened.error()));
		}
		return adopt(review_ledger_database{ *opened }, std::move(aStop));
	}

	auto review_ledger_database::adopt(
		review_ledger_database aOwned, std::stop_token aStop)
		-> std::expected<review_ledger_database, failure>
	{
		auto* const database   = aOwned.Handle;
		const auto  configured = configure_connection(database);
		if (!configured) {
			return std::unexpected(configured.error());
		}
		// RFC-0023 stores a complete 1 MiB assistant body plus bounded metadata in one row.
		// Other databases retain the shared 1 MiB connection default.
		(void)::sqlite3_limit(database, SQLITE_LIMIT_LENGTH, static_cast<int>(2U << 20U));
		if (auto established = establish_schema(database, aStop);
			!established) {
			return std::unexpected(established.error());
		}
		if (auto validated = validate(database); !validated) {
			return std::unexpected(validated.error());
		}
		return aOwned;
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

		const auto selectScope = [&]() -> std::expected<std::int64_t, failure> {
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
			const auto row = ::sqlite3_step(select->Value);
			if (row == SQLITE_ROW) {
				return ::sqlite3_column_int64(select->Value, 0);
			}
			if (row != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(Handle));
			}
			return 0;
		};

		if (auto existing = selectScope(); !existing || *existing != 0) {
			return existing;
		}
		auto insert = prepare(
			Handle,
			"INSERT INTO review_scope"
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
		return selectScope();
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
			|| bind_blob(upsert->Value, 2, file) != SQLITE_OK
			|| bind_blob(upsert->Value, 3, material) != SQLITE_OK
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
			|| bind_blob(select->Value, 2, file) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}

		const auto row = ::sqlite3_step(select->Value);
		if (row == SQLITE_DONE) {
			return std::optional<core::file_receipt>{ };
		}
		if (row != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		const auto material = read_digest(select->Value, 0);
		if (!material) {
			return std::unexpected(failure::integrity_failure);
		}

		const auto read = ::sqlite3_column_int64(select->Value, 1);
		if (read < 0 || read > 2) {
			return std::unexpected(failure::integrity_failure);
		}

		return std::optional{ core::file_receipt{
			.File     = aFile,
			.Material = core::file_material_id{ *material },
			.Read     = static_cast<core::file_read_state>(read),
			.Accepted = ::sqlite3_column_int64(select->Value, 2) != 0,
			.Excluded = ::sqlite3_column_int64(select->Value, 3) != 0,
		} };
	}

	auto review_ledger_database::store_conversation_receipt(
		const std::int64_t                aScope,
		const core::conversation_receipt& aReceipt)
		-> std::expected<void, failure>
	{
		// [[BPR-0656]]. The identities are 63-bit durable halves and the edit
		// versions body digests; all fit the column as signed integers, and a
		// zero cursor is a conversation nothing was seen in.
		const auto fits = [](const std::uint64_t aValue) {
			return aValue
				<= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
		};
		if (!application::valid(aReceipt) || !fits(aReceipt.Conversation.value())
			|| !fits(aReceipt.SeenComment.value()) || !fits(aReceipt.SeenEdit.value())
			|| !fits(aReceipt.AcceptedComment.value())
			|| !fits(aReceipt.AcceptedEdit.value())) {
			return std::unexpected(failure::integrity_failure);
		}
		auto upsert = prepare(
			Handle,
			"INSERT INTO conversation_receipt(scope_id, conversation, "
			"seen_comment, seen_edit, accepted_comment, accepted_edit) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6) "
			"ON CONFLICT(scope_id, conversation) DO UPDATE SET "
			"seen_comment = excluded.seen_comment, "
			"seen_edit = excluded.seen_edit, "
			"accepted_comment = excluded.accepted_comment, "
			"accepted_edit = excluded.accepted_edit");
		if (!upsert) {
			return std::unexpected(upsert.error());
		}
		const auto bind = [&upsert](const int aIndex, const std::uint64_t aValue) {
			return ::sqlite3_bind_int64(
					   upsert->Value, aIndex, static_cast<std::int64_t>(aValue))
				== SQLITE_OK;
		};
		if (::sqlite3_bind_int64(upsert->Value, 1, aScope) != SQLITE_OK
			|| !bind(2, aReceipt.Conversation.value())
			|| !bind(3, aReceipt.SeenComment.value())
			|| !bind(4, aReceipt.SeenEdit.value())
			|| !bind(5, aReceipt.AcceptedComment.value())
			|| !bind(6, aReceipt.AcceptedEdit.value())) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(upsert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto review_ledger_database::load_conversation_receipts(
		const std::int64_t aScope)
		-> std::expected<std::vector<core::conversation_receipt>, failure>
	{
		auto select = prepare(
			Handle,
			"SELECT conversation, seen_comment, seen_edit, accepted_comment, "
			"accepted_edit FROM conversation_receipt WHERE scope_id = ?1 "
			"ORDER BY conversation LIMIT ?2");
		if (!select) {
			return std::unexpected(select.error());
		}
		if (::sqlite3_bind_int64(select->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   select->Value,
				   2,
				   static_cast<std::int64_t>(
					   application::maximum_conversation_receipts))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		auto receipts = std::vector<core::conversation_receipt>{ };
		for (;;) {
			const auto row = ::sqlite3_step(select->Value);
			if (row == SQLITE_DONE) {
				break;
			}
			if (row != SQLITE_ROW) {
				return std::unexpected(catalog_sql_failure(Handle));
			}
			const auto column = [&select](const int aIndex) {
				return ::sqlite3_column_int64(select->Value, aIndex);
			};
			if (column(0) <= 0 || column(1) < 0 || column(2) < 0 || column(3) < 0
				|| column(4) < 0) {
				return std::unexpected(failure::integrity_failure);
			}
			receipts.push_back(core::conversation_receipt{
				.Conversation    = core::review_conversation_id{ static_cast<std::uint64_t>(column(0)) },
				.SeenComment     = core::review_comment_id{ static_cast<std::uint64_t>(column(1)) },
				.SeenEdit        = core::comment_edit_version{ static_cast<std::uint64_t>(column(2)) },
				.AcceptedComment = core::review_comment_id{ static_cast<std::uint64_t>(column(3)) },
				.AcceptedEdit    = core::comment_edit_version{ static_cast<std::uint64_t>(column(4)) },
			});
		}
		return receipts;
	}

	auto review_ledger_database::store_note(std::int64_t aScope, const application::local_note_record& aNote, std::optional<application::local_note_revision> aExpectedRevision)
		-> std::expected<application::local_note_record, application::local_note_failure> { return private_store_note(Handle, aScope, aNote, aExpectedRevision); }
	auto review_ledger_database::erase_note(std::int64_t aScope, application::local_note_id aNote, application::local_note_revision aExpectedRevision)
		-> std::expected<void, application::local_note_failure> { return private_erase_note(Handle, aScope, aNote, aExpectedRevision); }

	auto review_ledger_database::store_last_reviewed_head(
		const std::int64_t          aScope,
		const core::git_commit_oid& aHead) -> std::expected<void, failure>
	{
		if (!aHead.valid()) {
			return std::unexpected(failure::integrity_failure);
		}
		auto upsert = prepare(
			Handle,
			"INSERT INTO last_reviewed_head(scope_id, oid_algorithm, head_oid) "
			"VALUES(?1, ?2, ?3) ON CONFLICT(scope_id) DO UPDATE SET "
			"oid_algorithm = excluded.oid_algorithm, head_oid = excluded.head_oid");
		if (!upsert) {
			return std::unexpected(upsert.error());
		}
		const auto& oid = aHead.bytes();
		if (::sqlite3_bind_int64(upsert->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   2,
				   static_cast<std::int64_t>(aHead.algorithm()))
				!= SQLITE_OK
			|| bind_blob(upsert->Value, 3, oid) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(upsert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto review_ledger_database::erase_last_reviewed_head(
		const std::int64_t aScope) -> std::expected<void, failure>
	{
		auto erase = prepare(
			Handle,
			"DELETE FROM last_reviewed_head WHERE scope_id = ?1");
		if (!erase) {
			return std::unexpected(erase.error());
		}
		if (::sqlite3_bind_int64(erase->Value, 1, aScope) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(erase->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto review_ledger_database::store_defect(
		const std::int64_t                        aScope,
		const application::durable_defect_record& aDefect)
		-> std::expected<defect_store_outcome, failure>
	{
		if (!application::valid(aDefect)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto upsert = prepare(
			Handle,
			"INSERT INTO local_defect(scope_id, defect_id, file_identity, "
			"conversation, anchor_comparison, anchor_side, anchor_line, "
			"original_comparison, original_side, original_line, severity, "
			"category, lifecycle, availability) "
			"SELECT ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
			"?13, ?14 WHERE EXISTS(SELECT 1 FROM local_defect WHERE "
			"scope_id = ?1 AND defect_id = ?2) OR (SELECT COUNT(*) FROM "
			"local_defect WHERE scope_id = ?1) < ?15 "
			"ON CONFLICT(scope_id, defect_id) DO UPDATE SET "
			"severity = excluded.severity, category = excluded.category, "
			"lifecycle = excluded.lifecycle, availability = excluded.availability "
			"WHERE local_defect.file_identity = excluded.file_identity "
			"AND local_defect.conversation = excluded.conversation "
			"AND local_defect.anchor_comparison = excluded.anchor_comparison "
			"AND local_defect.anchor_side = excluded.anchor_side "
			"AND local_defect.anchor_line = excluded.anchor_line "
			"AND local_defect.original_comparison = excluded.original_comparison "
			"AND local_defect.original_side = excluded.original_side "
			"AND local_defect.original_line = excluded.original_line");
		if (!upsert) {
			return std::unexpected(upsert.error());
		}
		const auto& history  = aDefect.History;
		const auto& defect   = history.Defect;
		const auto& file     = aDefect.File.bytes();
		const auto& anchor   = defect.Anchor.Comparison.bytes();
		const auto& original = history.OriginalAnchor.Comparison.bytes();
		if (::sqlite3_bind_int64(upsert->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   2,
				   static_cast<std::int64_t>(defect.Id.value()))
				!= SQLITE_OK
			|| bind_blob(upsert->Value, 3, file) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   4,
				   static_cast<std::int64_t>(aDefect.Conversation.value()))
				!= SQLITE_OK
			|| bind_blob(upsert->Value, 5, anchor) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 6, static_cast<std::int64_t>(defect.Anchor.Side))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(upsert->Value, 7, defect.Anchor.Line)
				!= SQLITE_OK
			|| bind_blob(upsert->Value, 8, original) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   9,
				   static_cast<std::int64_t>(history.OriginalAnchor.Side))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 10, history.OriginalAnchor.Line)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 11, static_cast<std::int64_t>(defect.Severity))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 12, static_cast<std::int64_t>(defect.Category))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value, 13, static_cast<std::int64_t>(defect.State))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   14,
				   static_cast<std::int64_t>(history.Availability))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   15,
				   static_cast<std::int64_t>(application::maximum_private_defects))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(upsert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		if (::sqlite3_changes(Handle) == 0) {
			auto existing = prepare(
				Handle,
				"SELECT 1 FROM local_defect WHERE scope_id = ?1 AND defect_id = ?2");
			if (!existing) {
				return std::unexpected(existing.error());
			}
			if (::sqlite3_bind_int64(existing->Value, 1, aScope) != SQLITE_OK
				|| ::sqlite3_bind_int64(
					   existing->Value,
					   2,
					   static_cast<std::int64_t>(defect.Id.value()))
					!= SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
			const auto row = ::sqlite3_step(existing->Value);
			if (row == SQLITE_ROW) {
				return defect_store_outcome::identity_conflict;
			}
			if (row != SQLITE_DONE) {
				return std::unexpected(catalog_sql_failure(Handle));
			}
			return std::unexpected(failure::quota_exceeded);
		}
		return defect_store_outcome::stored;
	}

	auto review_ledger_database::erase_defect(
		const std::int64_t    aScope,
		const core::defect_id aDefect) -> std::expected<void, failure>
	{
		if (!durable_integer(aDefect.value())) {
			return std::unexpected(failure::integrity_failure);
		}
		auto erase = prepare(
			Handle,
			"DELETE FROM local_defect WHERE scope_id = ?1 AND defect_id = ?2");
		if (!erase) {
			return std::unexpected(erase.error());
		}
		if (::sqlite3_bind_int64(erase->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   erase->Value,
				   2,
				   static_cast<std::int64_t>(aDefect.value()))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(erase->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

	auto review_ledger_database::load_private(const std::int64_t aScope)
		-> std::expected<application::private_ledger_observation, failure>
	{
		auto result = application::private_ledger_observation{ };

		auto notes = private_load_notes(Handle, aScope);
		if (!notes) {
			return std::unexpected(notes.error());
		}
		result.Notes = std::move(*notes);
		auto row     = SQLITE_ROW;

		auto head = prepare(
			Handle,
			"SELECT oid_algorithm, head_oid FROM last_reviewed_head "
			"WHERE scope_id = ?1");
		if (!head || ::sqlite3_bind_int64(head->Value, 1, aScope) != SQLITE_OK) {
			return std::unexpected(head ? failure::unavailable : head.error());
		}
		row = ::sqlite3_step(head->Value);
		if (row == SQLITE_ROW) {
			const auto algorithm = ::sqlite3_column_int64(head->Value, 0);
			const auto oid       = read_digest(head->Value, 1);
			if (!oid || algorithm < 1 || algorithm > 2) {
				return std::unexpected(failure::integrity_failure);
			}
			auto parsed = core::git_commit_oid{
				static_cast<core::git_hash_algorithm>(algorithm),
				*oid,
			};
			if (!parsed.valid()) {
				return std::unexpected(failure::integrity_failure);
			}
			result.LastReviewedHead = parsed;
		} else if (row != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		auto defects = prepare(
			Handle,
			"SELECT defect_id, file_identity, conversation, anchor_comparison, "
			"anchor_side, anchor_line, original_comparison, original_side, "
			"original_line, severity, category, lifecycle, availability FROM "
			"local_defect WHERE scope_id = ?1 ORDER BY defect_id LIMIT ?2");
		if (!defects
			|| ::sqlite3_bind_int64(defects->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   defects->Value,
				   2,
				   static_cast<std::int64_t>(application::maximum_private_defects) + 1)
				!= SQLITE_OK) {
			return std::unexpected(
				defects ? failure::unavailable : defects.error());
		}
		while ((row = ::sqlite3_step(defects->Value)) == SQLITE_ROW) {
			if (result.Defects.size() == application::maximum_private_defects) {
				return std::unexpected(failure::quota_exceeded);
			}
			const auto     id           = ::sqlite3_column_int64(defects->Value, 0);
			const auto     file         = read_digest(defects->Value, 1);
			const auto     conversation = ::sqlite3_column_int64(defects->Value, 2);
			const auto     anchor       = read_digest(defects->Value, 3);
			const auto     side         = ::sqlite3_column_int64(defects->Value, 4);
			const auto     line         = ::sqlite3_column_int64(defects->Value, 5);
			const auto     original     = read_digest(defects->Value, 6);
			const auto     originalSide = ::sqlite3_column_int64(defects->Value, 7);
			const auto     originalLine = ::sqlite3_column_int64(defects->Value, 8);
			const auto     severity     = ::sqlite3_column_int64(defects->Value, 9);
			const auto     category     = ::sqlite3_column_int64(defects->Value, 10);
			const auto     lifecycle    = ::sqlite3_column_int64(defects->Value, 11);
			const auto     availability = ::sqlite3_column_int64(defects->Value, 12);
			constexpr auto maxLine      = static_cast<std::int64_t>(
				std::numeric_limits<std::uint32_t>::max());
			if (id <= 0 || conversation <= 0 || !file || !anchor || !original
				|| side < 0 || side > 1 || line < 0 || line > maxLine
				|| originalSide < 0 || originalSide > 1
				|| originalLine < 0 || originalLine > maxLine
				|| severity < 0 || severity > 3 || category < 0 || category > 6
				|| lifecycle < 0 || lifecycle > 6
				|| availability < 0 || availability > 2) {
				return std::unexpected(failure::integrity_failure);
			}
			const auto source = core::source_line_ref{
				.Comparison = core::file_comparison_digest{ *anchor },
				.Side       = static_cast<core::diff_side>(side),
				.Line       = static_cast<std::uint32_t>(line),
			};
			const auto originalSource = core::source_line_ref{
				.Comparison = core::file_comparison_digest{ *original },
				.Side       = static_cast<core::diff_side>(originalSide),
				.Line       = static_cast<std::uint32_t>(originalLine),
			};
			auto record = application::durable_defect_record{
				.File         = core::file_identity{ *file },
				.Conversation = core::review_conversation_id{
					static_cast<std::uint64_t>(conversation) },
				.History = {
					.Defect = {
						.Id       = core::defect_id{ static_cast<std::uint64_t>(id) },
						.Anchor   = source,
						.Severity = static_cast<core::defect_severity>(severity),
						.Category = static_cast<core::defect_category>(category),
						.State    = static_cast<core::defect_state>(lifecycle),
					},
					.OriginalAnchor = originalSource,
					.Availability   = static_cast<core::defect_anchor_availability>(availability),
				},
			};
			if (!application::valid(record)) {
				return std::unexpected(failure::integrity_failure);
			}
			result.Defects.push_back(std::move(record));
		}
		if (row != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return result;
	}

	auto review_ledger_database::admit_agent_session(
		const std::int64_t                          aScope,
		const application::agent_session_admission& aAdmission)
		-> std::expected<core::agent_session, failure>
	{
		if (!application::valid(aAdmission)) {
			return std::unexpected(failure::integrity_failure);
		}
		if (auto begin = execute(Handle, "BEGIN IMMEDIATE"); !begin) {
			return std::unexpected(begin.error());
		}
		const auto rollback = [this]() noexcept {
			(void)::sqlite3_exec(Handle, "ROLLBACK", nullptr, nullptr, nullptr);
		};

		auto sequence = prepare(
			Handle,
			"SELECT last_session_id FROM agent_session_sequence WHERE scope_id = ?1");
		if (!sequence
			|| ::sqlite3_bind_int64(sequence->Value, 1, aScope) != SQLITE_OK) {
			rollback();
			return std::unexpected(sequence ? failure::unavailable : sequence.error());
		}
		const auto sequenceRow = ::sqlite3_step(sequence->Value);
		auto       last        = std::int64_t{ 0 };
		if (sequenceRow == SQLITE_ROW) {
			last = ::sqlite3_column_int64(sequence->Value, 0);
			if (last < 0 || ::sqlite3_step(sequence->Value) != SQLITE_DONE) {
				rollback();
				return std::unexpected(failure::integrity_failure);
			}
		} else if (sequenceRow != SQLITE_DONE) {
			rollback();
			return std::unexpected(catalog_sql_failure(Handle));
		}
		if (last == std::numeric_limits<std::int64_t>::max()) {
			rollback();
			return std::unexpected(failure::quota_exceeded);
		}

		const auto id      = last + 1;
		const auto session = core::agent_session{
			.Id      = core::agent_session_id{ static_cast<std::uint64_t>(id) },
			.Profile = aAdmission.Profile,
			.Binding = aAdmission.Binding,
		};
		auto insert = prepare(
			Handle,
			"INSERT INTO agent_session(scope_id, session_id, profile_id, host_id, "
			"repository_id, pull_request_id, base_oid_algorithm, base_oid, "
			"head_oid_algorithm, head_oid, left_epoch, right_epoch) "
			"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)");
		const auto& base = session.Binding.BaseOid.bytes();
		const auto& head = session.Binding.HeadOid.bytes();
		if (!insert
			|| ::sqlite3_bind_int64(insert->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(insert->Value, 2, id) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   3,
				   static_cast<std::int64_t>(session.Profile.value()))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   4,
				   static_cast<std::int64_t>(session.Binding.Host.value()))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   5,
				   static_cast<std::int64_t>(session.Binding.Repository.value()))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   6,
				   static_cast<std::int64_t>(session.Binding.PullRequest.value()))
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   7,
				   static_cast<std::int64_t>(session.Binding.BaseOid.algorithm()))
				!= SQLITE_OK
			|| bind_blob(insert->Value, 8, base) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value,
				   9,
				   static_cast<std::int64_t>(session.Binding.HeadOid.algorithm()))
				!= SQLITE_OK
			|| bind_blob(insert->Value, 10, head) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value, 11, session.Binding.LeftEpoch.value())
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   insert->Value, 12, session.Binding.RightEpoch.value())
				!= SQLITE_OK) {
			rollback();
			return std::unexpected(insert ? failure::unavailable : insert.error());
		}
		if (::sqlite3_step(insert->Value) != SQLITE_DONE) {
			rollback();
			return std::unexpected(catalog_sql_failure(Handle));
		}

		auto updateSequence = prepare(
			Handle,
			"INSERT INTO agent_session_sequence(scope_id, last_session_id) "
			"VALUES(?1, ?2) ON CONFLICT(scope_id) DO UPDATE SET "
			"last_session_id = excluded.last_session_id");
		if (!updateSequence
			|| ::sqlite3_bind_int64(updateSequence->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(updateSequence->Value, 2, id) != SQLITE_OK
			|| ::sqlite3_step(updateSequence->Value) != SQLITE_DONE) {
			rollback();
			return std::unexpected(
				updateSequence ? catalog_sql_failure(Handle) : updateSequence.error());
		}

		if (id > static_cast<std::int64_t>(
				application::maximum_retained_agent_sessions)) {
			auto prune = prepare(
				Handle,
				"DELETE FROM agent_session WHERE scope_id = ?1 AND session_id <= ?2");
			const auto threshold = id - static_cast<std::int64_t>(application::maximum_retained_agent_sessions);
			if (!prune
				|| ::sqlite3_bind_int64(prune->Value, 1, aScope) != SQLITE_OK
				|| ::sqlite3_bind_int64(prune->Value, 2, threshold) != SQLITE_OK
				|| ::sqlite3_step(prune->Value) != SQLITE_DONE) {
				rollback();
				return std::unexpected(
					prune ? catalog_sql_failure(Handle) : prune.error());
			}
		}

		if (auto commit = execute(Handle, "COMMIT"); !commit) {
			rollback();
			return std::unexpected(commit.error());
		}
		return session;
	}

	auto review_ledger_database::load_agent_session_catalog(
		const std::int64_t aScope)
		-> std::expected<application::agent_session_catalog_state, failure>
	{
		auto result = application::agent_session_catalog_state{ };
		auto latest = prepare(
			Handle,
			"SELECT session_id, profile_id, host_id, repository_id, pull_request_id, "
			"base_oid_algorithm, base_oid, head_oid_algorithm, head_oid, left_epoch, "
			"right_epoch FROM agent_session WHERE scope_id = ?1 "
			"ORDER BY session_id DESC LIMIT 1");
		if (!latest || ::sqlite3_bind_int64(latest->Value, 1, aScope) != SQLITE_OK) {
			return std::unexpected(latest ? failure::unavailable : latest.error());
		}
		const auto latestRow = ::sqlite3_step(latest->Value);
		if (latestRow == SQLITE_ROW) {
			result.Latest = read_agent_session(latest->Value);
			if (!result.Latest.has_value()
				|| ::sqlite3_step(latest->Value) != SQLITE_DONE) {
				return std::unexpected(failure::integrity_failure);
			}
		} else if (latestRow != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

		auto sequence = prepare(
			Handle,
			"SELECT last_session_id FROM agent_session_sequence WHERE scope_id = ?1");
		if (!sequence
			|| ::sqlite3_bind_int64(sequence->Value, 1, aScope) != SQLITE_OK) {
			return std::unexpected(sequence ? failure::unavailable : sequence.error());
		}
		const auto sequenceRow = ::sqlite3_step(sequence->Value);
		if (sequenceRow == SQLITE_DONE) {
			if (result.Latest.has_value()) {
				return std::unexpected(failure::integrity_failure);
			}
			result.Next = core::agent_session_id{ 1U };
			return result;
		}
		if (sequenceRow != SQLITE_ROW) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		const auto last = ::sqlite3_column_int64(sequence->Value, 0);
		if (last <= 0 || ::sqlite3_step(sequence->Value) != SQLITE_DONE
			|| (result.Latest.has_value()
				&& static_cast<std::uint64_t>(last) < result.Latest->Id.value())) {
			return std::unexpected(failure::integrity_failure);
		}
		if (last < std::numeric_limits<std::int64_t>::max()) {
			result.Next = core::agent_session_id{
				static_cast<std::uint64_t>(last + 1)
			};
		}
		return result;
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
