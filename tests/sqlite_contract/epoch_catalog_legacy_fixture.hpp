#ifndef BPR_TESTS_SQLITE_CONTRACT_EPOCH_CATALOG_LEGACY_FIXTURE_HPP
#define BPR_TESTS_SQLITE_CONTRACT_EPOCH_CATALOG_LEGACY_FIXTURE_HPP

namespace bpr::tests::sqlite_contract {
	template <typename Database>
	[[nodiscard]] auto make_version_four_catalog(Database& aDatabase) -> bool
	{
		if (aDatabase.integer("SELECT count(*) FROM append_occurrence") != 0) {
			return false;
		}
		aDatabase.execute("PRAGMA foreign_keys = OFF");
		aDatabase.execute("BEGIN IMMEDIATE");
		aDatabase.execute("DROP TABLE append_residency");
		aDatabase.execute("DROP TABLE append_occurrence");
		aDatabase.execute("CREATE TABLE fixture_meta AS SELECT * FROM catalog_meta");
		aDatabase.execute("DROP TABLE catalog_meta");
		aDatabase.execute("CREATE TABLE catalog_meta(singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1), schema_format INTEGER NOT NULL CHECK(schema_format = 4), incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))) STRICT, WITHOUT ROWID");
		aDatabase.execute("CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation)");
		aDatabase.execute("INSERT INTO catalog_meta SELECT singleton, 4, incarnation FROM fixture_meta");
		aDatabase.execute("DROP TABLE fixture_meta");
		aDatabase.execute("PRAGMA user_version = 4");
		aDatabase.execute("COMMIT");
		aDatabase.execute("PRAGMA foreign_keys = ON");
		return true;
	}

	// The previous production schema contains no portable host or event provenance.
	template <typename Database>
	[[nodiscard]] auto make_version_three_catalog(Database& aDatabase) -> bool
	{
		if (aDatabase.integer("SELECT count(*) FROM epoch_occurrence") != 0) {
			return false;
		}
		aDatabase.execute("PRAGMA foreign_keys = OFF");
		aDatabase.execute("BEGIN IMMEDIATE");
		aDatabase.execute("DROP TABLE append_residency");
		aDatabase.execute("DROP TABLE append_occurrence");
		aDatabase.execute("DROP TABLE epoch_occurrence_supersession");
		aDatabase.execute("DROP TABLE epoch_occurrence");
		aDatabase.execute("DROP TABLE epoch_occurrence_history");
		aDatabase.execute("CREATE TABLE fixture_meta AS SELECT * FROM catalog_meta");
		aDatabase.execute("DROP TABLE catalog_meta");
		aDatabase.execute("CREATE TABLE catalog_meta("
						  "singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),"
						  "schema_format INTEGER NOT NULL CHECK(schema_format = 3),"
						  "incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))"
						  ") STRICT, WITHOUT ROWID");
		aDatabase.execute("CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation)");
		aDatabase.execute("INSERT INTO catalog_meta SELECT singleton, 3, incarnation FROM fixture_meta");
		aDatabase.execute("DROP TABLE fixture_meta");
		aDatabase.execute("PRAGMA user_version = 3");
		aDatabase.execute("COMMIT");
		aDatabase.execute("PRAGMA foreign_keys = ON");
		return true;
	}

	// Restore the exact v2 schema only when doing so discards no ownership facts.
	// Call with all product catalog handles closed.
	template <typename Database>
	[[nodiscard]] auto make_version_two_catalog(Database& aDatabase) -> bool
	{
		if (aDatabase.integer("SELECT count(*) FROM append_receiving") != 0
			|| aDatabase.integer("SELECT count(*) FROM epoch_occurrence") != 0) {
			return false;
		}
		aDatabase.execute("PRAGMA foreign_keys = OFF");
		aDatabase.execute("BEGIN IMMEDIATE");
		aDatabase.execute("DROP TABLE append_residency");
		aDatabase.execute("CREATE TABLE fixture_meta AS SELECT * FROM catalog_meta");
		aDatabase.execute("DROP TABLE catalog_meta");
		aDatabase.execute(
			"CREATE TABLE catalog_meta("
			"singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),"
			"schema_format INTEGER NOT NULL CHECK(schema_format = 2),"
			"incarnation BLOB NOT NULL CHECK(length(incarnation) = 32 AND incarnation != zeroblob(32))"
			") STRICT, WITHOUT ROWID");
		aDatabase.execute("CREATE UNIQUE INDEX catalog_meta_incarnation ON catalog_meta(incarnation)");
		aDatabase.execute("INSERT INTO catalog_meta SELECT singleton, 2, incarnation FROM fixture_meta");
		aDatabase.execute("DROP TABLE fixture_meta");
		aDatabase.execute("DROP TABLE append_receiving");
		aDatabase.execute("DROP TABLE append_occurrence");
		aDatabase.execute("DROP TABLE epoch_occurrence_supersession");
		aDatabase.execute("DROP TABLE epoch_occurrence");
		aDatabase.execute("DROP TABLE epoch_occurrence_history");
		aDatabase.execute("PRAGMA user_version = 2");
		aDatabase.execute("COMMIT");
		aDatabase.execute("PRAGMA foreign_keys = ON");
		return true;
	}
} // namespace bpr::tests::sqlite_contract

#endif
