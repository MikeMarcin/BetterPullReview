#ifndef BPR_TESTS_SQLITE_CONTRACT_REVIEW_LEDGER_LEGACY_FIXTURE_HPP
#define BPR_TESTS_SQLITE_CONTRACT_REVIEW_LEDGER_LEGACY_FIXTURE_HPP
#include "../../src/adapters/sqlite/src/private_review_database.hpp"
#include "../../src/adapters/sqlite/src/sqlite_connection.hpp"
#include "../support/test_expectation.hpp"
namespace bpr::tests::sqlite_contract {
	/// \brief Builds the frozen v6 source shape from a synthetic current fixture.
	/// This is test data construction, never an application downgrade path.
	inline void restore_v6_fixture(sqlite3* aDatabase)
	{
		using adapters::sqlite::detail::execute;
		const std::string_view statements[]{
			"CREATE TABLE local_note(scope_id INTEGER NOT NULL REFERENCES review_scope(scope_id) ON DELETE CASCADE,note_id INTEGER NOT NULL CHECK(note_id > 0),file_identity BLOB,comparison_id BLOB,note_text TEXT NOT NULL CHECK(length(CAST(note_text AS BLOB)) BETWEEN 1 AND 16384),revision INTEGER NOT NULL CHECK(revision > 0),CHECK((file_identity IS NULL AND comparison_id IS NULL) OR (length(file_identity) = 32 AND length(comparison_id) = 32)),PRIMARY KEY(scope_id, note_id)) STRICT",
			"CREATE TABLE local_note_sequence(scope_id INTEGER PRIMARY KEY REFERENCES review_scope(scope_id) ON DELETE CASCADE,last_revision INTEGER NOT NULL CHECK(last_revision > 0)) STRICT",
			"INSERT INTO local_note SELECT n.scope_id,n.note_id,t.file_identity,t.comparison_id,CAST(c.body AS TEXT),n.revision FROM private_legacy_note n JOIN private_thread t ON t.scope_id=n.scope_id AND t.id=n.thread_id JOIN private_turn u ON u.scope_id=t.scope_id AND u.thread_id=t.id JOIN private_content c ON c.scope_id=u.scope_id AND c.id=u.latest_revision",
			"INSERT INTO local_note_sequence SELECT scope_id,note_revision FROM private_scope_state WHERE note_revision>0",
			"DROP TABLE private_note_line", "DROP TABLE private_note_path", "DROP TABLE private_publication", "DROP TABLE private_draft", "DROP TABLE private_run_source", "DROP TABLE private_composition_source", "DROP TABLE private_composition", "DROP TABLE private_edge", "DROP TABLE private_turn", "DROP TABLE private_legacy_note", "DROP TABLE private_thread", "DROP TABLE private_content", "DROP TABLE private_run", "DROP TABLE private_scope_state", "DROP TABLE private_store_identity",
			"UPDATE ledger_meta SET schema_format=6", "PRAGMA user_version=6"
		};
		for (const auto statement : statements) {
			expect(execute(aDatabase, statement).has_value(), "build exact frozen v6 fixture");
		}
	}

	/// \brief [[BPR-1591]]. A current ledger as schema 9 left it: no note lines.
	inline void restore_v9_fixture(sqlite3* aDatabase)
	{
		using adapters::sqlite::detail::execute;
		const std::string_view statements[]{ "DROP TABLE private_note_line", "UPDATE ledger_meta SET schema_format=9", "PRAGMA user_version=9" };
		for (const auto statement : statements) {
			expect(execute(aDatabase, statement).has_value(), "build exact frozen v9 fixture");
		}
	}

	/// \brief Restores schema 8's frozen 64-row counter definition in test data.
	inline void restore_v8_fixture(sqlite3* aDatabase)
	{
		restore_v9_fixture(aDatabase);
		using namespace adapters::sqlite::detail;
		std::string_view state;
		for (const auto& table : PrivateSchemaTables) {
			if (table.Name == "private_scope_state") {
				state = table.Sql;
			}
		}
		expect(!state.empty(), "frozen schema8 counter definition exists");
		for (const auto statement : { std::string_view{ "BEGIN IMMEDIATE" }, std::string_view{ "ALTER TABLE private_scope_state RENAME TO private_scope_state_v9" }, state, std::string_view{ "INSERT INTO private_scope_state SELECT * FROM private_scope_state_v9" }, std::string_view{ "DROP TABLE private_scope_state_v9" }, std::string_view{ "UPDATE ledger_meta SET schema_format=8" }, std::string_view{ "PRAGMA user_version=8" }, std::string_view{ "COMMIT" } }) {
			expect(execute(aDatabase, statement).has_value(), "build exact frozen v8 fixture");
		}
	}

	/// \brief [[BPR-1210]]. A current ledger as schema 7 left it: no note paths.
	inline void restore_v7_fixture(sqlite3* aDatabase)
	{
		restore_v8_fixture(aDatabase);
		using adapters::sqlite::detail::execute;
		const std::string_view statements[]{ "DROP TABLE private_note_path", "UPDATE ledger_meta SET schema_format=7", "PRAGMA user_version=7" };
		for (const auto statement : statements) {
			expect(execute(aDatabase, statement).has_value(), "build exact frozen v7 fixture");
		}
	}
}
#endif
