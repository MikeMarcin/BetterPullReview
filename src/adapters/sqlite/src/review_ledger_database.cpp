#include "review_ledger_database.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace bpr::adapters::sqlite::detail {
	namespace {
		constexpr std::int32_t LedgerApplicationId = 0x42505244;

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
				if (row != SQLITE_ROW || index >= SchemaVersion3Tables.size()) {
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
					|| *name != SchemaVersion3Tables[index].Name
					|| canonical_sql(*sql)
						!= canonical_sql(SchemaVersion3Tables[index].Sql)) {
					return std::unexpected(failure::schema_mismatch);
				}
				++index;
			}
			if (index != SchemaVersion3Tables.size()) {
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

	auto review_ledger_database::store_note(
		const std::int64_t                    aScope,
		const application::local_note_record& aNote)
		-> std::expected<void, failure>
	{
		if (!application::valid(aNote)) {
			return std::unexpected(failure::integrity_failure);
		}
		auto upsert = prepare(
			Handle,
			"INSERT INTO local_note(scope_id, note_id, file_identity, "
			"comparison_id, note_text) "
			"SELECT ?1, ?2, ?3, ?4, ?5 WHERE "
			"EXISTS(SELECT 1 FROM local_note WHERE scope_id = ?1 AND note_id = ?2) "
			"OR (SELECT COUNT(*) FROM local_note WHERE scope_id = ?1) < ?6 "
			"ON CONFLICT(scope_id, note_id) DO UPDATE SET "
			"file_identity = excluded.file_identity, "
			"comparison_id = excluded.comparison_id, "
			"note_text = excluded.note_text");
		if (!upsert) {
			return std::unexpected(upsert.error());
		}
		if (::sqlite3_bind_int64(upsert->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   2,
				   static_cast<std::int64_t>(aNote.Id.value()))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (aNote.Attachment) {
			const auto& file       = aNote.Attachment->File.bytes();
			const auto& comparison = aNote.Attachment->Comparison.bytes();
			if (bind_blob(upsert->Value, 3, file) != SQLITE_OK
				|| bind_blob(upsert->Value, 4, comparison) != SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
		} else if (::sqlite3_bind_null(upsert->Value, 3) != SQLITE_OK
				   || ::sqlite3_bind_null(upsert->Value, 4) != SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_bind_text(
				upsert->Value,
				5,
				aNote.Note.Text.data(),
				static_cast<int>(aNote.Note.Text.size()),
				SQLITE_TRANSIENT)
				!= SQLITE_OK
			|| ::sqlite3_bind_int64(
				   upsert->Value,
				   6,
				   static_cast<std::int64_t>(application::maximum_private_notes))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(upsert->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		if (::sqlite3_changes(Handle) == 0) {
			return std::unexpected(failure::quota_exceeded);
		}
		return { };
	}

	auto review_ledger_database::erase_note(
		const std::int64_t               aScope,
		const application::local_note_id aNote) -> std::expected<void, failure>
	{
		if (!durable_integer(aNote.value())) {
			return std::unexpected(failure::integrity_failure);
		}
		auto erase = prepare(
			Handle,
			"DELETE FROM local_note WHERE scope_id = ?1 AND note_id = ?2");
		if (!erase) {
			return std::unexpected(erase.error());
		}
		if (::sqlite3_bind_int64(erase->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   erase->Value,
				   2,
				   static_cast<std::int64_t>(aNote.value()))
				!= SQLITE_OK) {
			return std::unexpected(failure::unavailable);
		}
		if (::sqlite3_step(erase->Value) != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}
		return { };
	}

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

		auto notes = prepare(
			Handle,
			"SELECT note_id, file_identity, comparison_id, note_text FROM "
			"local_note WHERE scope_id = ?1 ORDER BY note_id LIMIT ?2");
		if (!notes
			|| ::sqlite3_bind_int64(notes->Value, 1, aScope) != SQLITE_OK
			|| ::sqlite3_bind_int64(
				   notes->Value,
				   2,
				   static_cast<std::int64_t>(application::maximum_private_notes) + 1)
				!= SQLITE_OK) {
			return std::unexpected(
				notes ? failure::unavailable : notes.error());
		}
		auto row = SQLITE_ROW;
		while ((row = ::sqlite3_step(notes->Value)) == SQLITE_ROW) {
			if (result.Notes.size() == application::maximum_private_notes) {
				return std::unexpected(failure::quota_exceeded);
			}
			const auto  id             = ::sqlite3_column_int64(notes->Value, 0);
			const auto  fileType       = ::sqlite3_column_type(notes->Value, 1);
			const auto  comparisonType = ::sqlite3_column_type(notes->Value, 2);
			const auto  textBytes      = ::sqlite3_column_bytes(notes->Value, 3);
			const auto* text           = ::sqlite3_column_text(notes->Value, 3);
			if (id <= 0 || text == nullptr || textBytes <= 0
				|| static_cast<std::size_t>(textBytes) > core::maximum_note_bytes
				|| ((fileType == SQLITE_NULL) != (comparisonType == SQLITE_NULL))) {
				return std::unexpected(failure::integrity_failure);
			}
			auto note = application::local_note_record{
				.Id   = application::local_note_id{ static_cast<std::uint64_t>(id) },
				.Note = { .Text = std::string{
							  reinterpret_cast<const char*>(text),
							  static_cast<std::size_t>(textBytes) } },
			};
			if (fileType != SQLITE_NULL) {
				const auto file       = read_digest(notes->Value, 1);
				const auto comparison = read_digest(notes->Value, 2);
				if (!file || !comparison) {
					return std::unexpected(failure::integrity_failure);
				}
				note.Attachment = application::local_note_attachment{
					.File       = core::file_identity{ *file },
					.Comparison = core::file_comparison_digest{ *comparison },
				};
			}
			if (!application::valid(note)) {
				return std::unexpected(failure::integrity_failure);
			}
			result.Notes.push_back(std::move(note));
		}
		if (row != SQLITE_DONE) {
			return std::unexpected(catalog_sql_failure(Handle));
		}

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
