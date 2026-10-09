#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"
#include "bpr/adapters/sqlite/review_submission_recovery_store.hpp"

#include "review_tour_codec.hpp"
#include "sqlite_connection.hpp"
#include "state_directory.hpp"
#include "storage_internal.hpp"

#include "bpr/core/sha256.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		constexpr auto RecoveryApplicationId = std::int32_t{ 0x4250524D };
		constexpr auto RecoverySchemaVersion = std::int64_t{ 5 };
		constexpr auto RecoveryName          = std::string_view{
			"review-mutation-recovery.sqlite3"
		};

		constexpr std::array RecoverySchema{
			std::string_view{
				"CREATE TABLE recovery_meta("
				"singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
				"schema_format INTEGER NOT NULL) STRICT" },
			std::string_view{
				"CREATE TABLE recovery_scope("
				"scope_id INTEGER PRIMARY KEY,"
				"host TEXT NOT NULL,account TEXT NOT NULL,"
				"repository TEXT NOT NULL,pull_request INTEGER NOT NULL,"
				"UNIQUE(host,account,repository,pull_request)) STRICT" },
			std::string_view{
				"CREATE TABLE recovery_attempt("
				"scope_id INTEGER NOT NULL REFERENCES recovery_scope(scope_id) ON DELETE CASCADE,"
				"idempotency_key TEXT NOT NULL CHECK(length(idempotency_key) = 64),"
				"request_fingerprint BLOB NOT NULL CHECK(length(request_fingerprint) = 32),"
				"authority_fingerprint BLOB NOT NULL CHECK(length(authority_fingerprint) = 32),"
				"body TEXT NOT NULL CHECK(length(CAST(body AS BLOB)) BETWEEN 1 AND 65536),"
				"resolved_path TEXT NOT NULL CHECK(length(CAST(resolved_path AS BLOB)) BETWEEN 1 AND 4096),"
				"comparison BLOB NOT NULL CHECK(length(comparison) = 32),"
				"semantic_line INTEGER NOT NULL CHECK(semantic_line BETWEEN 0 AND 4294967295),"
				"commit_algorithm INTEGER NOT NULL CHECK(commit_algorithm BETWEEN 1 AND 2),"
				"commit_oid BLOB NOT NULL CHECK(length(commit_oid) = 32),"
				"state INTEGER NOT NULL CHECK(state BETWEEN 0 AND 1),"
				"server_comment_id TEXT CHECK(server_comment_id IS NULL OR "
				"length(CAST(server_comment_id AS BLOB)) BETWEEN 1 AND 256),"
				"CHECK((state = 0 AND server_comment_id IS NULL) OR "
				"(state = 1 AND server_comment_id IS NOT NULL)),"
				"PRIMARY KEY(scope_id,idempotency_key)) STRICT" },
			std::string_view{
				"CREATE TABLE recovery_baseline("
				"scope_id INTEGER NOT NULL,"
				"idempotency_key TEXT NOT NULL,"
				"baseline BLOB NOT NULL CHECK(length(baseline) BETWEEN 2 AND 25802),"
				"baseline_digest BLOB NOT NULL CHECK(length(baseline_digest) = 32),"
				"transmission INTEGER NOT NULL CHECK(transmission BETWEEN 0 AND 1),"
				"PRIMARY KEY(scope_id,idempotency_key),"
				"FOREIGN KEY(scope_id,idempotency_key) REFERENCES "
				"recovery_attempt(scope_id,idempotency_key) ON DELETE CASCADE) STRICT" },
			std::string_view{
				"CREATE TABLE submission_attempt("
				"scope_id INTEGER NOT NULL REFERENCES recovery_scope(scope_id) ON DELETE CASCADE,"
				"idempotency_key TEXT NOT NULL CHECK(length(CAST(idempotency_key AS BLOB)) BETWEEN 1 AND 256),"
				"request_fingerprint BLOB NOT NULL CHECK(length(request_fingerprint) = 32),"
				"authority_fingerprint BLOB NOT NULL CHECK(length(authority_fingerprint) = 32),"
				"request BLOB NOT NULL CHECK(length(request) BETWEEN 8 AND 98304),"
				"state INTEGER NOT NULL CHECK(state BETWEEN 0 AND 1),"
				"review_id BLOB CHECK(review_id IS NULL OR length(review_id) = 8),"
				"review_node_id TEXT CHECK(review_node_id IS NULL OR length(CAST(review_node_id AS BLOB)) BETWEEN 1 AND 256),"
				"CHECK((state = 0 AND review_id IS NULL AND review_node_id IS NULL) OR (state = 1 AND review_id IS NOT NULL AND review_node_id IS NOT NULL)),"
				"PRIMARY KEY(scope_id,idempotency_key)) STRICT" },
			std::string_view{
				"CREATE TABLE submission_baseline("
				"scope_id INTEGER NOT NULL,idempotency_key TEXT NOT NULL,"
				"baseline BLOB NOT NULL CHECK(length(baseline) BETWEEN 8 AND 808),"
				"baseline_digest BLOB NOT NULL CHECK(length(baseline_digest) = 32),"
				"transmission INTEGER NOT NULL CHECK(transmission BETWEEN 0 AND 1),"
				"PRIMARY KEY(scope_id,idempotency_key),"
				"FOREIGN KEY(scope_id,idempotency_key) REFERENCES submission_attempt(scope_id,idempotency_key) ON DELETE CASCADE) STRICT" },
			std::string_view{
				"CREATE TABLE logical_publication("
				"scope_id INTEGER NOT NULL REFERENCES recovery_scope(scope_id) ON DELETE CASCADE,"
				"publication_key TEXT NOT NULL CHECK(length(publication_key) = 64),"
				"legacy_key TEXT NOT NULL CHECK(length(legacy_key) = 64),"
				"record BLOB NOT NULL CHECK(length(record) BETWEEN 8 AND 200000),"
				"record_digest BLOB NOT NULL CHECK(length(record_digest) = 32),"
				"PRIMARY KEY(scope_id,publication_key),UNIQUE(scope_id,legacy_key)) STRICT" },
			std::string_view{ "CREATE TABLE staged_publication("
							  "scope_id INTEGER NOT NULL REFERENCES recovery_scope(scope_id) ON DELETE CASCADE,"
							  "publication_key TEXT NOT NULL CHECK(length(CAST(publication_key AS BLOB)) BETWEEN 1 AND 256),"
							  "record BLOB NOT NULL CHECK(length(record) BETWEEN 8 AND 1048576),"
							  "record_digest BLOB NOT NULL CHECK(length(record_digest)=32),"
							  "state INTEGER NOT NULL CHECK(state BETWEEN 0 AND 1),"
							  "PRIMARY KEY(scope_id,publication_key)) STRICT" },
		};
		constexpr std::array RecoverySchemaVersion1{
			std::pair{ std::string_view{ "recovery_attempt" }, RecoverySchema[2] },
			std::pair{ std::string_view{ "recovery_meta" }, RecoverySchema[0] },
			std::pair{ std::string_view{ "recovery_scope" }, RecoverySchema[1] },
		};
		constexpr std::array RecoverySchemaVersion2{
			std::pair{ std::string_view{ "recovery_attempt" }, RecoverySchema[2] },
			std::pair{ std::string_view{ "recovery_baseline" }, RecoverySchema[3] },
			std::pair{ std::string_view{ "recovery_meta" }, RecoverySchema[0] },
			std::pair{ std::string_view{ "recovery_scope" }, RecoverySchema[1] },
		};

		constexpr std::array RecoverySchemaVersion3{
			std::pair{ std::string_view{ "recovery_attempt" }, RecoverySchema[2] },
			std::pair{ std::string_view{ "recovery_baseline" }, RecoverySchema[3] },
			std::pair{ std::string_view{ "recovery_meta" }, RecoverySchema[0] },
			std::pair{ std::string_view{ "recovery_scope" }, RecoverySchema[1] },
			std::pair{ std::string_view{ "submission_attempt" }, RecoverySchema[4] },
			std::pair{ std::string_view{ "submission_baseline" }, RecoverySchema[5] },
		};

		constexpr std::array RecoverySchemaVersion4{
			std::pair{ std::string_view{ "logical_publication" }, RecoverySchema[6] },
			std::pair{ std::string_view{ "recovery_attempt" }, RecoverySchema[2] },
			std::pair{ std::string_view{ "recovery_baseline" }, RecoverySchema[3] },
			std::pair{ std::string_view{ "recovery_meta" }, RecoverySchema[0] },
			std::pair{ std::string_view{ "recovery_scope" }, RecoverySchema[1] },
			std::pair{ std::string_view{ "submission_attempt" }, RecoverySchema[4] },
			std::pair{ std::string_view{ "submission_baseline" }, RecoverySchema[5] },
		};
		constexpr std::array RecoverySchemaVersion5{
			std::pair{ std::string_view{ "logical_publication" }, RecoverySchema[6] },
			std::pair{ std::string_view{ "recovery_attempt" }, RecoverySchema[2] },
			std::pair{ std::string_view{ "recovery_baseline" }, RecoverySchema[3] },
			std::pair{ std::string_view{ "recovery_meta" }, RecoverySchema[0] },
			std::pair{ std::string_view{ "recovery_scope" }, RecoverySchema[1] },
			std::pair{ std::string_view{ "staged_publication" }, RecoverySchema[7] },
			std::pair{ std::string_view{ "submission_attempt" }, RecoverySchema[4] },
			std::pair{ std::string_view{ "submission_baseline" }, RecoverySchema[5] },
		};
		[[nodiscard]] auto encode_publication(const application::review_publication_recovery_record& aRecord) -> std::vector<std::uint8_t>;
		[[nodiscard]] auto decode_publication(std::span<const std::uint8_t> aBytes) -> std::optional<application::review_publication_recovery_record>;

		[[nodiscard]] auto translate(const detail::failure aFailure) noexcept
			-> application::review_mutation_recovery_failure
		{
			return aFailure == detail::failure::integrity_failure
					|| aFailure == detail::failure::schema_mismatch
				? application::review_mutation_recovery_failure::integrity_failure
				: application::review_mutation_recovery_failure::unavailable;
		}

		/// \brief The recovery journal, open and scoped, with the `state/v1` it
		/// was opened under.
		///
		/// \details [[BPR-0883]]. The directory is held for as long as the
		/// connection is open, as `state_directory.hpp` promises; the store
		/// that takes the journal closes the connection.
		struct recovery_journal final
		{
			detail::state_directory Directory;
			::sqlite3*              Database = nullptr;
			std::int64_t            Scope    = 0;
		};

		[[nodiscard]] auto valid_server_comment_id(
			const std::string_view aValue) noexcept -> bool
		{
			return !aValue.empty() && aValue.size() <= 256U
				&& std::ranges::all_of(aValue, [](const unsigned char aCharacter) {
					   return aCharacter >= 0x20U && aCharacter != 0x7FU;
				   });
		}

		[[nodiscard]] auto encode_baseline(
			const std::vector<std::string>& aIds) -> std::vector<std::uint8_t>
		{
			auto result = std::vector<std::uint8_t>{
				static_cast<std::uint8_t>(aIds.size() >> 8U),
				static_cast<std::uint8_t>(aIds.size()),
			};
			for (const auto& id : aIds) {
				result.push_back(static_cast<std::uint8_t>(id.size() >> 8U));
				result.push_back(static_cast<std::uint8_t>(id.size()));
				result.insert(result.end(), id.begin(), id.end());
			}
			return result;
		}

		[[nodiscard]] auto baseline_digest(
			const std::vector<std::uint8_t>& aEncoded)
			-> std::array<std::uint8_t, 32>
		{
			return core::sha256(std::span{ aEncoded });
		}

		[[nodiscard]] auto baseline_at(
			::sqlite3_stmt*                                    aStatement,
			const int                                          aColumn,
			const std::optional<std::array<std::uint8_t, 32>>& aExpectedDigest)
			-> std::expected<
				std::optional<std::vector<std::string>>,
				application::review_mutation_recovery_failure>
		{
			using failure = application::review_mutation_recovery_failure;
			if (::sqlite3_column_type(aStatement, aColumn) == SQLITE_NULL) {
				if (aExpectedDigest.has_value()) {
					return std::unexpected(failure::integrity_failure);
				}
				return std::optional<std::vector<std::string>>{ };
			}
			if (::sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB) {
				return std::unexpected(failure::integrity_failure);
			}
			const auto  size = ::sqlite3_column_bytes(aStatement, aColumn);
			const auto* data = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aStatement, aColumn));
			if (data == nullptr || size < 2 || size > 25'802) {
				return std::unexpected(failure::integrity_failure);
			}
			if (!aExpectedDigest.has_value()
				|| core::sha256(std::span{ data, static_cast<std::size_t>(size) })
					!= *aExpectedDigest) {
				return std::unexpected(failure::integrity_failure);
			}
			auto       offset   = std::size_t{ };
			const auto nextSize = [&]() -> std::optional<std::size_t> {
				if (offset + 2U > static_cast<std::size_t>(size)) {
					return std::nullopt;
				}
				const auto value = (static_cast<std::size_t>(data[offset]) << 8U)
					| static_cast<std::size_t>(data[offset + 1U]);
				offset += 2U;
				return value;
			};
			const auto count = nextSize();
			if (!count.has_value()
				|| *count > application::maximum_review_mutation_baseline_comments) {
				return std::unexpected(failure::integrity_failure);
			}
			auto result = std::vector<std::string>{ };
			result.reserve(*count);
			for (auto index = std::size_t{ }; index < *count; ++index) {
				const auto length = nextSize();
				if (!length.has_value() || *length == 0U || *length > 256U
					|| offset + *length > static_cast<std::size_t>(size)) {
					return std::unexpected(failure::integrity_failure);
				}
				result.emplace_back(
					reinterpret_cast<const char*>(data + offset), *length);
				offset += *length;
			}
			if (offset != static_cast<std::size_t>(size)) {
				return std::unexpected(failure::integrity_failure);
			}
			return std::optional{ std::move(result) };
		}

		[[nodiscard]] auto bind_text(
			::sqlite3_stmt*        aStatement,
			const int              aIndex,
			const std::string_view aValue) noexcept -> bool
		{
			return aValue.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max())
				&& ::sqlite3_bind_text(
					   aStatement,
					   aIndex,
					   aValue.data(),
					   static_cast<int>(aValue.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK;
		}

		template <typename Bytes>
		[[nodiscard]] auto bind_blob(
			::sqlite3_stmt* aStatement,
			const int       aIndex,
			const Bytes&    aValue) noexcept -> bool
		{
			return ::sqlite3_bind_blob(
					   aStatement,
					   aIndex,
					   aValue.data(),
					   static_cast<int>(aValue.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK;
		}

		[[nodiscard]] auto text_at(
			::sqlite3_stmt* aStatement,
			const int       aColumn) -> std::optional<std::string>
		{
			if (::sqlite3_column_type(aStatement, aColumn) != SQLITE_TEXT) {
				return std::nullopt;
			}
			const auto* data = reinterpret_cast<const char*>(
				::sqlite3_column_text(aStatement, aColumn));
			const auto size = ::sqlite3_column_bytes(aStatement, aColumn);
			return data != nullptr && size >= 0
				? std::optional{ std::string{ data, static_cast<std::size_t>(size) } }
				: std::nullopt;
		}

		template <std::size_t Size>
		[[nodiscard]] auto validate_schema(
			::sqlite3* aDatabase,
			const std::array<std::pair<std::string_view, std::string_view>, Size>&
				aExpected) -> std::expected<void, detail::failure>
		{
			auto query = detail::prepare(
				aDatabase,
				"SELECT name,sql FROM sqlite_schema WHERE type = 'table' "
				"AND name NOT LIKE 'sqlite_%' ORDER BY name");
			if (!query) {
				return std::unexpected(query.error());
			}
			for (const auto& [expectedName, expectedSql] : aExpected) {
				if (::sqlite3_step(query->Value) != SQLITE_ROW) {
					return std::unexpected(detail::failure::schema_mismatch);
				}
				const auto name = text_at(query->Value, 0);
				const auto sql  = text_at(query->Value, 1);
				if (!name || !sql || *name != expectedName
					|| detail::canonical_sql(*sql)
						!= detail::canonical_sql(expectedSql)) {
					return std::unexpected(detail::failure::schema_mismatch);
				}
			}
			return ::sqlite3_step(query->Value) == SQLITE_DONE
				? std::expected<void, detail::failure>{ }
				: std::unexpected(detail::failure::schema_mismatch);
		}

		[[nodiscard]] auto digest_at(
			::sqlite3_stmt* aStatement,
			const int       aColumn)
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
			auto result = std::array<std::uint8_t, 32>{ };
			std::copy_n(source, result.size(), result.begin());
			return result;
		}

		[[nodiscard]] auto transmission_from_storage(
			const bool                         aBaselineMissing,
			const std::optional<std::int64_t>& aStored)
			-> application::review_mutation_recovery_transmission
		{
			if (aBaselineMissing) {
				return application::review_mutation_recovery_transmission::
					baseline_incomplete;
			}
			return *aStored == 0
				? application::review_mutation_recovery_transmission::prepared
				: application::review_mutation_recovery_transmission::
					  possibly_transmitted;
		}

		[[nodiscard]] auto same_attempt(
			const application::review_mutation_recovery_record& aLeft,
			const application::review_mutation_recovery_record& aRight) noexcept
			-> bool
		{
			return aLeft.IdempotencyKey == aRight.IdempotencyKey
				&& aLeft.RequestFingerprint == aRight.RequestFingerprint
				&& aLeft.AuthorityFingerprint == aRight.AuthorityFingerprint
				&& aLeft.Body == aRight.Body && aLeft.Anchor == aRight.Anchor;
		}

		class recovery_store final
			: public application::review_mutation_recovery_port
		{
		public:
			explicit recovery_store(recovery_journal aJournal) noexcept
				: Directory{ std::move(aJournal.Directory) }
				, Database{ aJournal.Database }
				, Scope{ aJournal.Scope }
			{
			}

			~recovery_store() override
			{
				if (Database != nullptr) {
					static_cast<void>(::sqlite3_close_v2(Database));
				}
			}

			auto load_publication(const std::string& aKey) -> std::expected<std::optional<application::review_publication_recovery_record>, application::review_mutation_recovery_failure> override
			{
				std::scoped_lock lock{ Mutex };
				return load_publication_locked(aKey);
			}
			auto advance_publication(const std::optional<application::review_publication_recovery_record>& aExpected, const application::review_publication_recovery_record& aNext)
				-> std::expected<application::review_publication_recovery_record, application::review_mutation_recovery_failure> override
			{
				using failure = application::review_mutation_recovery_failure;
				if (!application::valid_publication_transition(aExpected, aNext)) {
					return std::unexpected(failure::conflict);
				}
				std::scoped_lock lock{ Mutex };
				if (!detail::execute(Database, "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				const auto transaction = [&]() -> std::expected<void, failure> {
					const auto current = load_publication_locked(aNext.Key);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (*current != aExpected) {
						return std::unexpected(failure::conflict);
					}
					const auto legacy = load_locked(aNext.LegacyKey);
					if (!legacy) {
						return std::unexpected(legacy.error());
					}
					if (legacy->has_value()) {
						return std::unexpected(failure::conflict);
					}
					if (!aExpected) {
						auto count = detail::prepare(Database, "SELECT (SELECT count(*) FROM recovery_attempt WHERE scope_id=?1)+(SELECT count(*) FROM logical_publication WHERE scope_id=?1)");
						if (!count || ::sqlite3_bind_int64(count->Value, 1, Scope) != SQLITE_OK || ::sqlite3_step(count->Value) != SQLITE_ROW) {
							return std::unexpected(failure::unavailable);
						}
						if (::sqlite3_column_int64(count->Value, 0) >= static_cast<std::int64_t>(application::maximum_review_mutation_recovery_records)) {
							return std::unexpected(failure::capacity);
						}
					}
					const auto bytes  = encode_publication(aNext);
					const auto digest = core::sha256(std::span{ bytes });
					auto       write  = detail::prepare(Database, aExpected ? "UPDATE logical_publication SET record=?4,record_digest=?5 WHERE scope_id=?1 AND publication_key=?2 AND legacy_key=?3" : "INSERT INTO logical_publication(scope_id,publication_key,legacy_key,record,record_digest) VALUES(?1,?2,?3,?4,?5)");
					if (!write || ::sqlite3_bind_int64(write->Value, 1, Scope) != SQLITE_OK || !bind_text(write->Value, 2, aNext.Key)
						|| !bind_text(write->Value, 3, aNext.LegacyKey) || !bind_blob(write->Value, 4, bytes) || !bind_blob(write->Value, 5, digest)
						|| ::sqlite3_step(write->Value) != SQLITE_DONE || ::sqlite3_changes(Database) != 1) {
						return std::unexpected(failure::conflict);
					}
					return { };
				}();
				if (!transaction || !detail::execute(Database, "COMMIT")) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
					return std::unexpected(transaction ? failure::unavailable : transaction.error());
				}
				return aNext;
			}

			auto load(const std::string& aIdempotencyKey)
				-> std::expected<
					std::optional<application::review_mutation_recovery_record>,
					application::review_mutation_recovery_failure> override
			{
				std::scoped_lock lock{ Mutex };
				return load_locked(aIdempotencyKey);
			}

			auto admit(const application::review_mutation_recovery_record& aRecord)
				-> std::expected<
					application::review_mutation_recovery_admission,
					application::review_mutation_recovery_failure> override
			{
				using failure = application::review_mutation_recovery_failure;
				// [[BPR-1335]] v002. The legacy table keeps no side, and reads
				// every row back on the head side, so it takes no other.
				if (!application::valid(aRecord)
					|| aRecord.Anchor.Semantic.Side != core::diff_side::right
					|| !aRecord.BaselineExactCommentIds.has_value()
					|| aRecord.Transmission
						!= application::review_mutation_recovery_transmission::prepared
					|| aRecord.State
						!= application::review_mutation_recovery_state::unresolved) {
					return std::unexpected(failure::conflict);
				}
				std::scoped_lock lock{ Mutex };
				if (!detail::execute(Database, "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				auto rollback = [&] {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				};
				auto existing = load_locked(aRecord.IdempotencyKey);
				if (!existing.has_value()) {
					rollback();
					return std::unexpected(existing.error());
				}
				if (existing->has_value()) {
					if (!same_attempt(**existing, aRecord)) {
						rollback();
						return std::unexpected(failure::conflict);
					}
					auto result = **existing;
					if (!detail::execute(Database, "COMMIT")) {
						rollback();
						return std::unexpected(failure::unavailable);
					}
					return application::review_mutation_recovery_admission{
						.Record = std::move(result),
					};
				}
				auto logical = detail::prepare(Database, "SELECT count(*) FROM logical_publication WHERE scope_id=?1 AND legacy_key=?2");
				if (!logical || ::sqlite3_bind_int64(logical->Value, 1, Scope) != SQLITE_OK || !bind_text(logical->Value, 2, aRecord.IdempotencyKey)
					|| ::sqlite3_step(logical->Value) != SQLITE_ROW || ::sqlite3_column_int64(logical->Value, 0) != 0) {
					rollback();
					return std::unexpected(failure::conflict);
				}

				auto count = detail::prepare(
					Database,
					"SELECT (SELECT count(*) FROM recovery_attempt WHERE scope_id = ?1) + (SELECT count(*) FROM logical_publication WHERE scope_id = ?1)");
				if (!count || ::sqlite3_bind_int64(count->Value, 1, Scope) != SQLITE_OK
					|| ::sqlite3_step(count->Value) != SQLITE_ROW) {
					rollback();
					return std::unexpected(failure::unavailable);
				}
				if (::sqlite3_column_int64(count->Value, 0)
					>= static_cast<std::int64_t>(
						application::maximum_review_mutation_recovery_records)) {
					rollback();
					return std::unexpected(failure::capacity);
				}
				auto insert = detail::prepare(
					Database,
					"INSERT INTO recovery_attempt("
					"scope_id,idempotency_key,request_fingerprint,authority_fingerprint,"
					"body,resolved_path,comparison,semantic_line,commit_algorithm,"
					"commit_oid,state,server_comment_id) "
					"VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,0,NULL)");
				const auto& anchor         = aRecord.Anchor;
				const auto  baseline       = encode_baseline(*aRecord.BaselineExactCommentIds);
				const auto  baselineDigest = baseline_digest(baseline);
				if (!insert || ::sqlite3_bind_int64(insert->Value, 1, Scope) != SQLITE_OK
					|| !bind_text(insert->Value, 2, aRecord.IdempotencyKey)
					|| !bind_blob(insert->Value, 3, aRecord.RequestFingerprint)
					|| !bind_blob(insert->Value, 4, aRecord.AuthorityFingerprint)
					|| !bind_text(insert->Value, 5, aRecord.Body)
					|| !bind_text(insert->Value, 6, anchor.Path)
					|| !bind_blob(insert->Value, 7, anchor.Semantic.Comparison.bytes())
					|| ::sqlite3_bind_int64(insert->Value, 8, anchor.Semantic.Line)
						!= SQLITE_OK
					|| ::sqlite3_bind_int64(
						   insert->Value,
						   9,
						   static_cast<std::int64_t>(anchor.Commit.algorithm()))
						!= SQLITE_OK
					|| !bind_blob(insert->Value, 10, anchor.Commit.bytes())
					|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
					rollback();
					return std::unexpected(failure::unavailable);
				}
				auto insertBaseline = detail::prepare(
					Database,
					"INSERT INTO recovery_baseline("
					"scope_id,idempotency_key,baseline,baseline_digest,transmission) "
					"VALUES(?1,?2,?3,?4,0)");
				if (!insertBaseline
					|| ::sqlite3_bind_int64(insertBaseline->Value, 1, Scope) != SQLITE_OK
					|| !bind_text(insertBaseline->Value, 2, aRecord.IdempotencyKey)
					|| !bind_blob(insertBaseline->Value, 3, baseline)
					|| !bind_blob(insertBaseline->Value, 4, baselineDigest)
					|| ::sqlite3_step(insertBaseline->Value) != SQLITE_DONE
					|| !detail::execute(Database, "COMMIT")) {
					rollback();
					return std::unexpected(failure::unavailable);
				}
				return application::review_mutation_recovery_admission{
					.Record   = aRecord,
					.Inserted = true,
				};
			}

			auto mark_possibly_transmitted(
				const std::string&                  aIdempotencyKey,
				const std::array<std::uint8_t, 32>& aRequestFingerprint)
				-> std::expected<
					application::review_mutation_recovery_record,
					application::review_mutation_recovery_failure> override
			{
				using failure = application::review_mutation_recovery_failure;
				std::scoped_lock lock{ Mutex };
				if (!detail::execute(Database, "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				auto rollback = [&] {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				};
				auto existing = load_locked(aIdempotencyKey);
				if (!existing || !existing->has_value()
					|| (**existing).RequestFingerprint != aRequestFingerprint
					|| (**existing).State
						!= application::review_mutation_recovery_state::unresolved
					|| !(**existing).BaselineExactCommentIds.has_value()) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				if ((**existing).Transmission
					== application::review_mutation_recovery_transmission::possibly_transmitted) {
					auto result = **existing;
					if (!detail::execute(Database, "COMMIT")) {
						rollback();
						return std::unexpected(failure::unavailable);
					}
					return result;
				}
				auto update = detail::prepare(
					Database,
					"UPDATE recovery_baseline SET transmission = 1 "
					"WHERE scope_id = ?1 AND idempotency_key = ?2 "
					"AND transmission = 0");
				if (!update || ::sqlite3_bind_int64(update->Value, 1, Scope) != SQLITE_OK
					|| !bind_text(update->Value, 2, aIdempotencyKey)
					|| ::sqlite3_step(update->Value) != SQLITE_DONE
					|| ::sqlite3_changes(Database) != 1) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				auto marked = load_locked(aIdempotencyKey);
				if (!marked || !marked->has_value()
					|| !detail::execute(Database, "COMMIT")) {
					rollback();
					return std::unexpected(failure::unavailable);
				}
				return **marked;
			}

			auto resolve(
				const std::string&                          aIdempotencyKey,
				const std::array<std::uint8_t, 32>&         aRequestFingerprint,
				const application::review_mutation_receipt& aReceipt)
				-> std::expected<
					application::review_mutation_recovery_record,
					application::review_mutation_recovery_failure> override
			{
				using failure = application::review_mutation_recovery_failure;
				if (!valid_server_comment_id(aReceipt.ServerCommentId)) {
					return std::unexpected(failure::conflict);
				}
				std::scoped_lock lock{ Mutex };
				if (!detail::execute(Database, "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				auto rollback = [&] {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
				};
				auto existing = load_locked(aIdempotencyKey);
				if (!existing || !existing->has_value()
					|| (**existing).RequestFingerprint != aRequestFingerprint
					|| !application::valid(**existing)) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				if ((**existing).State
					== application::review_mutation_recovery_state::succeeded) {
					if (!(**existing).Receipt.has_value()
						|| (**existing).Receipt->ServerCommentId
							!= aReceipt.ServerCommentId) {
						rollback();
						return std::unexpected(failure::conflict);
					}
					auto result = **existing;
					if (!detail::execute(Database, "COMMIT")) {
						rollback();
						return std::unexpected(failure::unavailable);
					}
					return result;
				}
				if (!(**existing).BaselineExactCommentIds.has_value()) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				if ((**existing).Transmission
					!= application::review_mutation_recovery_transmission::possibly_transmitted) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				if (std::ranges::binary_search(
						*(**existing).BaselineExactCommentIds,
						aReceipt.ServerCommentId)) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				auto update = detail::prepare(
					Database,
					"UPDATE recovery_attempt SET state = 1,server_comment_id = ?1 "
					"WHERE scope_id = ?2 AND idempotency_key = ?3 "
					"AND request_fingerprint = ?4 AND state = 0");
				if (!update || !bind_text(update->Value, 1, aReceipt.ServerCommentId)
					|| ::sqlite3_bind_int64(update->Value, 2, Scope) != SQLITE_OK
					|| !bind_text(update->Value, 3, aIdempotencyKey)
					|| !bind_blob(update->Value, 4, aRequestFingerprint)
					|| ::sqlite3_step(update->Value) != SQLITE_DONE
					|| ::sqlite3_changes(Database) != 1) {
					rollback();
					return std::unexpected(failure::conflict);
				}
				auto resolved = load_locked(aIdempotencyKey);
				if (!resolved || !resolved->has_value()
					|| !detail::execute(Database, "COMMIT")) {
					rollback();
					return std::unexpected(failure::unavailable);
				}
				return **resolved;
			}

		private:
			[[nodiscard]] auto load_publication_locked(const std::string& aKey) -> std::expected<std::optional<application::review_publication_recovery_record>, application::review_mutation_recovery_failure>
			{
				using failure = application::review_mutation_recovery_failure;
				auto query    = detail::prepare(Database, "SELECT legacy_key,record,record_digest FROM logical_publication WHERE scope_id=?1 AND publication_key=?2");
				if (!query || ::sqlite3_bind_int64(query->Value, 1, Scope) != SQLITE_OK || !bind_text(query->Value, 2, aKey)) {
					return std::unexpected(failure::unavailable);
				}
				const auto row = ::sqlite3_step(query->Value);
				if (row == SQLITE_DONE) {
					return std::optional<application::review_publication_recovery_record>{ };
				}
				if (row != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				const auto  legacy = text_at(query->Value, 0);
				const auto  digest = digest_at(query->Value, 2);
				const auto  size   = ::sqlite3_column_bytes(query->Value, 1);
				const auto* data   = static_cast<const std::uint8_t*>(::sqlite3_column_blob(query->Value, 1));
				if (!legacy || !digest || ::sqlite3_column_type(query->Value, 1) != SQLITE_BLOB || !data || size < 8 || size > 200000) {
					return std::unexpected(failure::integrity_failure);
				}
				const auto bytes = std::span{ data, static_cast<std::size_t>(size) };
				if (core::sha256(bytes) != *digest) {
					return std::unexpected(failure::integrity_failure);
				}
				auto decoded = decode_publication(bytes);
				if (!decoded || decoded->Key != aKey || decoded->LegacyKey != *legacy || ::sqlite3_step(query->Value) != SQLITE_DONE) {
					return std::unexpected(failure::integrity_failure);
				}
				return decoded;
			}

			[[nodiscard]] auto load_locked(const std::string& aIdempotencyKey)
				-> std::expected<
					std::optional<application::review_mutation_recovery_record>,
					application::review_mutation_recovery_failure>
			{
				using failure = application::review_mutation_recovery_failure;
				auto query    = detail::prepare(
					Database,
					"SELECT request_fingerprint,authority_fingerprint,body,resolved_path,"
					"comparison,semantic_line,commit_algorithm,commit_oid,state,"
					"server_comment_id,recovery_baseline.baseline,"
					"recovery_baseline.baseline_digest,recovery_baseline.transmission "
					"FROM recovery_attempt "
					"LEFT JOIN recovery_baseline USING(scope_id,idempotency_key) "
					"WHERE recovery_attempt.scope_id = ?1 "
					"AND recovery_attempt.idempotency_key = ?2");
				if (!query || ::sqlite3_bind_int64(query->Value, 1, Scope) != SQLITE_OK
					|| !bind_text(query->Value, 2, aIdempotencyKey)) {
					return std::unexpected(failure::unavailable);
				}
				const auto row = ::sqlite3_step(query->Value);
				if (row == SQLITE_DONE) {
					return std::optional<application::review_mutation_recovery_record>{ };
				}
				if (row != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				const auto request      = digest_at(query->Value, 0);
				const auto authority    = digest_at(query->Value, 1);
				const auto body         = text_at(query->Value, 2);
				const auto path         = text_at(query->Value, 3);
				const auto comparison   = digest_at(query->Value, 4);
				const auto line         = ::sqlite3_column_int64(query->Value, 5);
				const auto algorithm    = ::sqlite3_column_int64(query->Value, 6);
				const auto commit       = digest_at(query->Value, 7);
				const auto state        = ::sqlite3_column_int64(query->Value, 8);
				const auto receipt      = state == 1
					? text_at(query->Value, 9)
					: std::optional<std::string>{ };
				const auto digest       = digest_at(query->Value, 11);
				const auto baseline     = baseline_at(query->Value, 10, digest);
				const auto transmission = ::sqlite3_column_type(query->Value, 12)
						== SQLITE_INTEGER
					? std::optional{ ::sqlite3_column_int64(query->Value, 12) }
					: std::nullopt;
				if (!request || !authority || !body || !path || !comparison || !commit
					|| !baseline.has_value()
					|| line < 0 || line > std::numeric_limits<std::uint32_t>::max()
					|| algorithm < 1 || algorithm > 2 || state < 0 || state > 1
					|| (state == 1 && !receipt.has_value())
					|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
					return std::unexpected(failure::integrity_failure);
				}
				const auto baselineMissing = !baseline->has_value();
				if (baselineMissing
						? digest.has_value() || transmission.has_value()
						: !digest.has_value() || !transmission.has_value()
							|| (*transmission != 0 && *transmission != 1)) {
					return std::unexpected(failure::integrity_failure);
				}
				// Populated field by field from a value-initialized record rather than
				// aggregate-initialized in place. GCC 15.2 at -O3 loses the correlation
				// between the guard above and this object's string subobjects once the
				// enclosing expected's inlining is folded in, and reports Anchor.Path as
				// possibly uninitialized. This is the restructure BPR-0064 settled on.
				auto record                 = application::review_mutation_recovery_record{ };
				record.IdempotencyKey       = aIdempotencyKey;
				record.RequestFingerprint   = *request;
				record.AuthorityFingerprint = *authority;
				record.Body                 = *body;

				record.Anchor.Semantic.Comparison
					= core::file_comparison_digest{ *comparison };
				record.Anchor.Semantic.Side = core::diff_side::right;
				record.Anchor.Semantic.Line = static_cast<std::uint32_t>(line);
				record.Anchor.Path          = *path;
				record.Anchor.Commit        = core::git_commit_oid{
					static_cast<core::git_hash_algorithm>(algorithm), *commit
				};

				record.BaselineExactCommentIds = *baseline;
				record.Transmission
					= transmission_from_storage(baselineMissing, transmission);
				record.State
					= static_cast<application::review_mutation_recovery_state>(state);
				if (receipt.has_value()) {
					record.Receipt = application::review_mutation_receipt{
						.ServerCommentId = *receipt,
						.Duplicate       = false,
					};
				}
				return application::valid(record)
					? std::expected<
						  std::optional<application::review_mutation_recovery_record>,
						  failure>{ std::optional{ std::move(record) } }
					: std::unexpected(failure::integrity_failure);
			}

			detail::state_directory Directory;
			::sqlite3*              Database = nullptr;
			std::int64_t            Scope    = 0;
			std::mutex              Mutex;
		};

		class submission_writer
		{
		public:
			std::vector<std::uint8_t> Bytes;
			void                      integer(std::uint64_t aValue)
			{
				for (auto shift = 0U; shift < 64U; shift += 8U) {
					Bytes.push_back(static_cast<std::uint8_t>((aValue >> shift) & 255U));
				}
			}
			void text(std::string_view aValue)
			{
				integer(aValue.size());
				Bytes.insert(Bytes.end(), aValue.begin(), aValue.end());
			}
			void digest(const std::array<std::uint8_t, 32>& aValue) { Bytes.insert(Bytes.end(), aValue.begin(), aValue.end()); }
			void oid(const core::git_commit_oid& aValue)
			{
				integer(static_cast<std::uint64_t>(aValue.algorithm()));
				Bytes.insert(Bytes.end(), aValue.bytes().begin(), aValue.bytes().end());
			}
		};
		class submission_reader
		{
		public:
			explicit submission_reader(std::span<const std::uint8_t> aBytes)
				: Bytes{ aBytes }
			{ }
			auto integer() -> std::uint64_t
			{
				if (Bytes.size() < 8U) {
					Good = false;
					return 0U;
				}
				std::uint64_t value = 0U;
				for (auto shift = 0U; shift < 64U; shift += 8U) {
					value |= static_cast<std::uint64_t>(Bytes[shift / 8U]) << shift;
				}
				Bytes = Bytes.subspan(8U);
				return value;
			}
			auto text(std::size_t aMaximum = 256U) -> std::string
			{
				const auto size = integer();
				if (!Good || size > aMaximum || size > Bytes.size()) {
					Good = false;
					return { };
				}
				std::string value{ reinterpret_cast<const char*>(Bytes.data()), static_cast<std::size_t>(size) };
				Bytes = Bytes.subspan(static_cast<std::size_t>(size));
				return value;
			}
			auto bounded(const std::uint64_t aMaximum) -> std::uint64_t
			{
				const auto value = integer();
				if (value > aMaximum) {
					Good = false;
					return 0U;
				}
				return value;
			}
			auto digest() -> std::array<std::uint8_t, 32>
			{
				std::array<std::uint8_t, 32> result{ };
				if (Bytes.size() < 32U) {
					Good = false;
					return result;
				}
				std::ranges::copy(Bytes.first(32U), result.begin());
				Bytes = Bytes.subspan(32U);
				return result;
			}
			auto oid() -> core::git_commit_oid
			{
				const auto algorithm = integer();
				if (!Good || algorithm < 1U || algorithm > 2U || Bytes.size() < 32U) {
					Good = false;
					return { };
				}
				std::array<std::uint8_t, 32> bytes{ };
				std::ranges::copy(Bytes.first(32U), bytes.begin());
				Bytes = Bytes.subspan(32U);
				return core::git_commit_oid{ static_cast<core::git_hash_algorithm>(algorithm), bytes };
			}
			template <typename T>
			void identity(T& aId)
			{
				using value_type = decltype(aId.value());
				const auto value = integer();
				if (value > std::numeric_limits<value_type>::max()) {
					Good = false;
					return;
				}
				aId = T{ static_cast<value_type>(value) };
			}
			auto done() const -> bool { return Good && Bytes.empty(); }

		private:
			std::span<const std::uint8_t> Bytes;
			bool                          Good = true;
		};
		// [[BPR-1335]] v002. Version 1 children are on the head side and keep
		// no side; version 2 writes each child's side after its line. A record
		// whose children are all on the head side is still written as version
		// 1, byte for byte as before, so only a base-side one needs version 2.
		constexpr auto PublicationRecordVersion     = std::uint64_t{ 1U };
		constexpr auto PublicationRecordSideVersion = std::uint64_t{ 2U };
		void           encode_child(submission_writer& aOut, const std::optional<application::review_mutation_recovery_record>& aChild, const std::uint64_t aVersion)
		{
			aOut.integer(aChild.has_value() ? 1U : 0U);
			if (!aChild) {
				return;
			}
			const auto& c = *aChild;
			aOut.text(c.IdempotencyKey);
			aOut.digest(c.RequestFingerprint);
			aOut.digest(c.AuthorityFingerprint);
			aOut.text(c.Body);
			aOut.text(c.Anchor.Path);
			aOut.digest(c.Anchor.Semantic.Comparison.bytes());
			aOut.integer(c.Anchor.Semantic.Line);
			if (aVersion >= PublicationRecordSideVersion) {
				aOut.integer(static_cast<std::uint64_t>(c.Anchor.Semantic.Side));
			}
			aOut.oid(c.Anchor.Commit);
			aOut.integer(static_cast<std::uint64_t>(c.Transmission));
			aOut.integer(static_cast<std::uint64_t>(c.State));
			aOut.integer(c.BaselineExactCommentIds->size());
			for (const auto& id : *c.BaselineExactCommentIds) {
				aOut.text(id);
			}
			aOut.integer(c.Receipt.has_value() ? 1U : 0U);
			if (c.Receipt) {
				aOut.text(c.Receipt->ServerCommentId);
				aOut.integer(c.Receipt->Duplicate ? 1U : 0U);
				aOut.integer(static_cast<std::uint64_t>(c.Receipt->Representation));
			}
		}
		auto decode_child(submission_reader& aIn, const std::uint64_t aVersion) -> std::optional<application::review_mutation_recovery_record>
		{
			if (aIn.bounded(1U) == 0U) {
				return std::nullopt;
			}
			application::review_mutation_recovery_record c;
			c.IdempotencyKey             = aIn.text(64U);
			c.RequestFingerprint         = aIn.digest();
			c.AuthorityFingerprint       = aIn.digest();
			c.Body                       = aIn.text(65536U);
			c.Anchor.Path                = aIn.text(4096U);
			c.Anchor.Semantic.Comparison = core::file_comparison_digest{ aIn.digest() };
			c.Anchor.Semantic.Side       = core::diff_side::right;
			c.Anchor.Semantic.Line       = static_cast<std::uint32_t>(aIn.bounded(4294967295U));
			if (aVersion >= PublicationRecordSideVersion) {
				c.Anchor.Semantic.Side = static_cast<core::diff_side>(aIn.bounded(static_cast<std::uint64_t>(core::diff_side::right)));
			}
			c.Anchor.Commit = aIn.oid();
			c.Transmission               = static_cast<application::review_mutation_recovery_transmission>(aIn.bounded(2U));
			c.State                      = static_cast<application::review_mutation_recovery_state>(aIn.bounded(1U));
			const auto count             = aIn.bounded(application::maximum_review_mutation_baseline_comments);
			c.BaselineExactCommentIds.emplace();
			for (auto index = std::uint64_t{ 0 }; index < count; ++index) {
				c.BaselineExactCommentIds->push_back(aIn.text());
			}
			if (aIn.bounded(1U) != 0U) {
				c.Receipt.emplace();
				c.Receipt->ServerCommentId = aIn.text();
				c.Receipt->Duplicate       = aIn.bounded(1U) != 0U;
				c.Receipt->Representation  = static_cast<core::review_publication_representation>(aIn.bounded(1U));
			}
			return c;
		}
		auto encode_publication(const application::review_publication_recovery_record& aRecord) -> std::vector<std::uint8_t>
		{
			const auto base    = [](const std::optional<application::review_mutation_recovery_record>& aChild) {
                return aChild.has_value() && aChild->Anchor.Semantic.Side != core::diff_side::right;
			};
			const auto version = base(aRecord.Native) || base(aRecord.Fallback) ? PublicationRecordSideVersion : PublicationRecordVersion;
			submission_writer out;
			out.integer(version);
			out.text(aRecord.Key);
			out.text(aRecord.LegacyKey);
			out.digest(aRecord.Fingerprint);
			out.integer(static_cast<std::uint64_t>(aRecord.State));
			const auto& auth = aRecord.Authorization;
			out.integer(static_cast<std::uint64_t>(auth.Source));
			out.digest(auth.ContractDigest);
			out.digest(auth.ScopeDigest);
			out.integer(auth.Generation);
			out.integer(auth.ObservedAt);
			out.integer(auth.ExpiresAt);
			encode_child(out, aRecord.Native, version);
			encode_child(out, aRecord.Fallback, version);
			return out.Bytes;
		}
		auto decode_publication(const std::span<const std::uint8_t> aBytes) -> std::optional<application::review_publication_recovery_record>
		{
			submission_reader in{ aBytes };
			const auto        version = in.integer();
			if (version != PublicationRecordVersion && version != PublicationRecordSideVersion) {
				return std::nullopt;
			}
			application::review_publication_recovery_record result;
			result.Key          = in.text(64U);
			result.LegacyKey    = in.text(64U);
			result.Fingerprint  = in.digest();
			result.State        = static_cast<core::review_publication_state>(in.bounded(9U));
			auto& auth          = result.Authorization;
			auth.Source         = static_cast<application::review_fallback_authorization_source>(in.bounded(2U));
			auth.ContractDigest = in.digest();
			auth.ScopeDigest    = in.digest();
			auth.Generation     = in.integer();
			auth.ObservedAt     = in.integer();
			auth.ExpiresAt      = in.integer();
			result.Native       = decode_child(in, version);
			result.Fallback     = decode_child(in, version);
			return in.done() && application::valid(result) ? std::optional{ std::move(result) } : std::nullopt;
		}

		// The snapshot has its own version and preserves full-width IDs and literal body bytes.
		auto encode_submission(const application::review_submission_request& aRequest) -> std::vector<std::uint8_t>
		{
			submission_writer out;
			out.integer(1U);
			const auto& proposal = aRequest.Proposal;
			const auto& target   = aRequest.LiveBinding->Target;
			const auto& actor    = aRequest.LiveBinding->ActingIdentity;
			out.integer(proposal.Draft.value());
			out.integer(proposal.Attempt.value());
			out.integer(static_cast<std::uint64_t>(proposal.Disposition));
			out.text(aRequest.IdempotencyKey);
			out.text(proposal.Body);
			out.integer(target.Semantic.Host.value());
			out.integer(target.Semantic.Repository.value());
			out.integer(target.Semantic.PullRequest.value());
			out.oid(target.Semantic.ExpectedHead);
			out.text(target.Hostname.value());
			out.integer(static_cast<std::uint64_t>(target.ServerContract));
			out.text(target.Repository.Owner);
			out.text(target.Repository.Name);
			out.integer(target.Number.value());
			out.integer(target.RepositoryDatabaseId);
			out.text(target.RepositoryNodeId);
			out.integer(target.PullRequestDatabaseId);
			out.text(target.PullRequestNodeId);
			out.integer(target.BaseRepositoryDatabaseId);
			out.oid(target.BaseOid);
			out.text(target.BaseRef);
			out.text(target.HeadRef);
			out.integer(target.Open ? 1U : 0U);
			out.integer(actor.Account.value());
			out.integer(actor.DatabaseId);
			out.text(actor.NodeId);
			out.text(actor.Login);
			return out.Bytes;
		}
		auto decode_submission(std::span<const std::uint8_t> aBytes) -> std::optional<application::review_submission_request>
		{
			submission_reader in{ aBytes };
			if (in.integer() != 1U) {
				return std::nullopt;
			}
			application::review_submission_request request;
			request.LiveBinding.emplace();
			auto& proposal = request.Proposal;
			auto& target   = request.LiveBinding->Target;
			auto& actor    = request.LiveBinding->ActingIdentity;
			in.identity(proposal.Draft);
			in.identity(proposal.Attempt);
			const auto disposition = in.integer();
			if (disposition > 2U) {
				return std::nullopt;
			}
			proposal.Disposition   = static_cast<core::review_disposition>(disposition);
			request.IdempotencyKey = in.text();
			proposal.Body          = in.text(core::maximum_review_submission_body_bytes);
			in.identity(target.Semantic.Host);
			in.identity(target.Semantic.Repository);
			in.identity(target.Semantic.PullRequest);
			target.Semantic.ExpectedHead = in.oid();
			target.Hostname              = application::github_canonical_hostname{ in.text() };
			const auto server            = in.integer();
			if (server > std::numeric_limits<std::uint8_t>::max()) {
				return std::nullopt;
			}
			target.ServerContract   = static_cast<application::github_server_contract>(server);
			target.Repository.Owner = in.text();
			target.Repository.Name  = in.text();
			in.identity(target.Number);
			target.RepositoryDatabaseId     = in.integer();
			target.RepositoryNodeId         = in.text();
			target.PullRequestDatabaseId    = in.integer();
			target.PullRequestNodeId        = in.text();
			target.BaseRepositoryDatabaseId = in.integer();
			target.BaseOid                  = in.oid();
			target.BaseRef                  = in.text();
			target.HeadRef                  = in.text();
			const auto open                 = in.integer();
			if (open != 1U) {
				return std::nullopt;
			}
			target.Open = true;
			in.identity(actor.Account);
			actor.DatabaseId       = in.integer();
			actor.NodeId           = in.text();
			actor.Login            = in.text();
			proposal.Target        = target.Semantic;
			proposal.ActingAccount = actor.Account;
			return in.done() && application::valid(request) ? std::optional{ std::move(request) } : std::nullopt;
		}
		auto submission_blob_at(::sqlite3_stmt* aStatement, int aColumn, std::size_t aMaximum) -> std::optional<std::vector<std::uint8_t>>
		{
			if (::sqlite3_column_type(aStatement, aColumn) != SQLITE_BLOB) {
				return std::nullopt;
			}
			const auto  size = ::sqlite3_column_bytes(aStatement, aColumn);
			const auto* data = static_cast<const std::uint8_t*>(::sqlite3_column_blob(aStatement, aColumn));
			if (size < 1 || static_cast<std::size_t>(size) > aMaximum || !data) {
				return std::nullopt;
			}
			return std::vector<std::uint8_t>{ data, data + size };
		}
		void staged_actor_codec(detail::tour_codec& aCodec, application::staged_review_actor& aValue)
		{
			aCodec.number(aValue.DatabaseId);
			aCodec.text(aValue.NodeId, 256);
			aCodec.text(aValue.Login, 256);
		}
		void staged_review_codec(detail::tour_codec& aCodec, application::staged_review_entity& aValue)
		{
			aCodec.id(aValue.Id);
			aCodec.text(aValue.NodeId, 256);
			aCodec.text(aValue.WebUrl, 4096);
			staged_actor_codec(aCodec, aValue.Actor);
			aCodec.oid(aValue.Commit);
			aCodec.text(aValue.Body);
			aCodec.enumeration(aValue.State);
		}
		void staged_comment_codec(detail::tour_codec& aCodec, application::staged_review_comment& aValue)
		{
			aCodec.number(aValue.Id);
			aCodec.text(aValue.NodeId, 256);
			aCodec.text(aValue.WebUrl, 4096);
			staged_actor_codec(aCodec, aValue.Actor);
			aCodec.id(aValue.Review);
			aCodec.optional(aValue.Source, [&](auto& aSource) { aCodec.destination(aSource); });
			aCodec.text(aValue.Body);
			aCodec.number(aValue.Reply);
		}
		void staged_record_codec(detail::tour_codec& aCodec, application::staged_publication_record& aRecord)
		{
			std::uint64_t version = 3;
			aCodec.number(version);
			if (version != 1 && version != 2 && version != 3) {
				aCodec.Good = false;
				return;
			}
			std::string summary;
			if (!aCodec.Reading) {
				const auto bytes = encode_submission(aRecord.Plan.Submission);
				summary.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
			}
			aCodec.text(summary, 98304);
			if (aCodec.Reading) {
				const auto decoded = decode_submission(std::span{ reinterpret_cast<const std::uint8_t*>(summary.data()), summary.size() });
				if (!decoded) {
					aCodec.Good = false;
					return;
				}
				aRecord.Plan.Submission = *decoded;
			}
			aCodec.owner(aRecord.Plan.Owner);
			aCodec.number(aRecord.Plan.Generation);
			aCodec.list(aRecord.Plan.Stops, application::maximum_staged_review_comments, [&](auto& aStop) {
				aCodec.id(aStop.Id);
				aCodec.destination(aStop.Source);
				aCodec.text(aStop.Title);
				aCodec.text(aStop.Narrative);
				aCodec.text(aStop.SourceLink, 4096);
			});
			aCodec.number(aRecord.Plan.Compatibility);
			aCodec.optional(aRecord.Plan.Description, [&](auto& aEdit) {aCodec.text(aEdit.Baseline);aCodec.text(aEdit.Desired);if(version>=2){aCodec.number(aEdit.AppendIndexLink);
} });
			aCodec.number(aRecord.Revision);
			aCodec.list(aRecord.BaselineIds, application::maximum_staged_review_pages * 100U, [&](auto& aId) { aCodec.number(aId); });
			aCodec.optional(aRecord.Review, [&](auto& aReview) { staged_review_codec(aCodec, aReview); });
			aCodec.list(aRecord.Comments, application::maximum_staged_review_comments, [&](auto& aComment) { staged_comment_codec(aCodec, aComment); });
			aCodec.number(aRecord.Patched);
			aCodec.number(aRecord.Published);
			aCodec.number(aRecord.DescriptionDone);
			aCodec.optional(aRecord.Pending, [&](auto& aRequest) {
				aRequest.Binding = *aRecord.Plan.Submission.LiveBinding;
				aCodec.enumeration(aRequest.Operation);
				aCodec.id(aRequest.Review);
				aCodec.number(aRequest.Comment);
				aCodec.number(aRequest.Page);
				aCodec.text(aRequest.Body);
				aCodec.list(aRequest.Lines, application::maximum_staged_review_comments, [&](auto& aLine) {aCodec.destination(aLine.Source);aCodec.text(aLine.Body); });
			});
			aCodec.number(aRecord.PossiblyTransmitted);
			if (version >= 2) {
				aCodec.optional(aRecord.Index, [&](auto& index) { staged_comment_codec(aCodec, index); });
			}
			if (version >= 3) {
				aCodec.optional(aRecord.Plan.Update, [&](auto& value) { aCodec.tour(value); });
				aCodec.number(aRecord.Updated);
				aCodec.number(aRecord.DestinationsPublished);
			}
		}
		auto encode_staged_record(const application::staged_publication_record& aRecord) -> std::vector<std::uint8_t>
		{
			auto               copy = aRecord;
			detail::tour_codec codec;
			staged_record_codec(codec, copy);
			return codec.Good ? std::move(codec.Bytes) : std::vector<std::uint8_t>{ };
		}
		auto decode_staged_record(std::span<const std::uint8_t> aBytes) -> std::optional<application::staged_publication_record>
		{
			detail::tour_codec codec;
			codec.Input   = aBytes;
			codec.Reading = true;
			application::staged_publication_record result;
			staged_record_codec(codec, result);
			if (!codec.Good || !codec.Input.empty() || !application::valid(result)) {
				return std::nullopt;
			}
			return result;
		}

		class submission_store final : public application::review_submission_recovery_port
		{
			using failure = application::review_mutation_recovery_failure;
			using record  = application::review_submission_recovery_record;

		public:
			submission_store(recovery_journal aJournal, review_mutation_recovery_configuration aConfiguration)
				: Directory{ std::move(aJournal.Directory) }
				, Database{ aJournal.Database }
				, Scope{ aJournal.Scope }
				, Configuration{ std::move(aConfiguration) }
			{ }
			~submission_store() override { static_cast<void>(::sqlite3_close_v2(Database)); }
			auto load_staged(const std::string& aKey) -> std::expected<std::optional<application::staged_publication_record>, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return load_staged_locked(aKey);
			}
			auto advance_staged(const std::optional<application::staged_publication_record>& aBefore, const application::staged_publication_record& aAfter) -> std::expected<application::staged_publication_record, failure> override
			{
				if (!application::valid_staged_transition(aBefore, aAfter) || !matches_scope(aAfter.Plan.Submission)) {
					return std::unexpected(failure::conflict);
				}
				std::scoped_lock lock{ Mutex };
				return transaction<application::staged_publication_record>([&]() -> std::expected<application::staged_publication_record, failure> {
					const auto& key     = aAfter.Plan.Submission.IdempotencyKey;
					const auto  current = load_staged_locked(key);
					if (!current) {
						return std::unexpected(current.error());
					}
					if (*current != aBefore) {
						return std::unexpected(failure::conflict);
					}
					if (!aBefore) {
						auto count = detail::prepare(Database, "SELECT count(*) FROM staged_publication WHERE scope_id=?1");
						if (!count || ::sqlite3_bind_int64(count->Value, 1, Scope) != SQLITE_OK || ::sqlite3_step(count->Value) != SQLITE_ROW) {
							return std::unexpected(failure::unavailable);
						}
						if (::sqlite3_column_int64(count->Value, 0) >= static_cast<std::int64_t>(application::maximum_review_mutation_recovery_records)) {
							return std::unexpected(failure::capacity);
						}
					}
					const auto bytes = encode_staged_record(aAfter);
					if (bytes.empty() || bytes.size() > 1048576U) {
						return std::unexpected(failure::capacity);
					}
					const auto hash     = core::sha256(bytes);
					auto       write    = detail::prepare(Database, aBefore ? "UPDATE staged_publication SET record=?3,record_digest=?4,state=?5 WHERE scope_id=?1 AND publication_key=?2" : "INSERT INTO staged_publication(scope_id,publication_key,record,record_digest,state) VALUES(?1,?2,?3,?4,?5)");
					const bool complete = aAfter.Published && (!aAfter.Plan.Description || aAfter.DescriptionDone);
					if (!write || !scope_key(write->Value, 1, key) || !bind_blob(write->Value, 3, bytes) || !bind_blob(write->Value, 4, hash) || ::sqlite3_bind_int(write->Value, 5, complete ? 1 : 0) != SQLITE_OK || ::sqlite3_step(write->Value) != SQLITE_DONE || ::sqlite3_changes(Database) != 1) {
						return std::unexpected(failure::unavailable);
					}
					return aAfter;
				});
			}
			auto pending_staged() -> std::expected<std::vector<application::staged_publication_record>, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return transaction<std::vector<application::staged_publication_record>>([&]() -> std::expected<std::vector<application::staged_publication_record>, failure> {
					auto query = detail::prepare(Database, "SELECT publication_key FROM staged_publication WHERE scope_id=?1 AND state=0 ORDER BY publication_key LIMIT 1025");
					if (!query || ::sqlite3_bind_int64(query->Value, 1, Scope) != SQLITE_OK) {
						return std::unexpected(failure::unavailable);
					}
					std::vector<application::staged_publication_record> records;
					int                                                 row;
					while ((row = ::sqlite3_step(query->Value)) == SQLITE_ROW) {
						const auto key = text_at(query->Value, 0);
						if (!key || records.size() == application::maximum_review_mutation_recovery_records) {
							return std::unexpected(failure::capacity);
						}
						auto value = load_staged_locked(*key);
						if (!value || !*value) {
							return std::unexpected(failure::integrity_failure);
						}
						records.push_back(std::move(**value));
					}
					if (row != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					return records;
				});
			}

			auto pending() -> std::expected<std::vector<record>, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return transaction<std::vector<record>>([&]() -> std::expected<std::vector<record>, failure> {
					auto query = detail::prepare(Database, "SELECT idempotency_key FROM submission_attempt WHERE scope_id = ?1 AND state = 0 ORDER BY idempotency_key LIMIT ?2");
					if (!query || ::sqlite3_bind_int64(query->Value, 1, Scope) != SQLITE_OK || ::sqlite3_bind_int64(query->Value, 2, static_cast<std::int64_t>(application::maximum_review_mutation_recovery_records) + 1) != SQLITE_OK) {
						return std::unexpected(failure::unavailable);
					}
					std::vector<record> records;
					int                 row;
					while ((row = ::sqlite3_step(query->Value)) == SQLITE_ROW) {
						if (records.size() == application::maximum_review_mutation_recovery_records) {
							return std::unexpected(failure::capacity);
						}
						const auto key = text_at(query->Value, 0);
						if (!key) {
							return std::unexpected(failure::integrity_failure);
						}
						auto value = load_locked(*key);
						if (!value) {
							return std::unexpected(value.error());
						}
						if (!*value || (**value).State != application::review_mutation_recovery_state::unresolved) {
							return std::unexpected(failure::integrity_failure);
						}
						records.push_back(std::move(**value));
					}
					if (row != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					return records;
				});
			}
			auto load(const std::string& aKey) -> std::expected<std::optional<record>, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return load_locked(aKey);
			}
			auto admit(const record& aRecord) -> std::expected<application::review_submission_recovery_admission, failure> override
			{
				if (!application::valid(aRecord) || !matches_scope(aRecord.Request) || aRecord.Transmission != application::review_mutation_recovery_transmission::prepared || aRecord.State != application::review_mutation_recovery_state::unresolved) {
					return std::unexpected(failure::conflict);
				}
				std::scoped_lock lock{ Mutex };
				return transaction<application::review_submission_recovery_admission>([&]() -> std::expected<application::review_submission_recovery_admission, failure> {
					auto existing = load_locked(aRecord.Request.IdempotencyKey);
					if (!existing) {
						return std::unexpected(existing.error());
					}
					if (*existing) {
						if ((**existing).Request != aRecord.Request || (**existing).RequestFingerprint != aRecord.RequestFingerprint || (**existing).AuthorityFingerprint != aRecord.AuthorityFingerprint) {
							return std::unexpected(failure::conflict);
						}
						return application::review_submission_recovery_admission{ .Record = **existing };
					}
					auto count = detail::prepare(Database, "SELECT count(*) FROM submission_attempt WHERE scope_id = ?1");
					if (!count || ::sqlite3_bind_int64(count->Value, 1, Scope) != SQLITE_OK || ::sqlite3_step(count->Value) != SQLITE_ROW) {
						return std::unexpected(failure::unavailable);
					}
					if (::sqlite3_column_int64(count->Value, 0) >= static_cast<std::int64_t>(application::maximum_review_mutation_recovery_records)) {
						return std::unexpected(failure::capacity);
					}
					auto       insert  = detail::prepare(Database, "INSERT INTO submission_attempt(scope_id,idempotency_key,request_fingerprint,authority_fingerprint,request,state) VALUES(?1,?2,?3,?4,?5,0)");
					const auto encoded = encode_submission(aRecord.Request);
					if (!insert || !scope_key(insert->Value, 1, aRecord.Request.IdempotencyKey) || !bind_blob(insert->Value, 3, aRecord.RequestFingerprint) || !bind_blob(insert->Value, 4, aRecord.AuthorityFingerprint) || !bind_blob(insert->Value, 5, encoded) || ::sqlite3_step(insert->Value) != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					submission_writer baseline;
					baseline.integer(aRecord.BaselineReviewIds->size());
					for (const auto id : *aRecord.BaselineReviewIds) {
						baseline.integer(id.value());
					}
					const auto digest         = baseline_digest(baseline.Bytes);
					auto       insertBaseline = detail::prepare(Database, "INSERT INTO submission_baseline(scope_id,idempotency_key,baseline,baseline_digest,transmission) VALUES(?1,?2,?3,?4,0)");
					if (!insertBaseline || !scope_key(insertBaseline->Value, 1, aRecord.Request.IdempotencyKey) || !bind_blob(insertBaseline->Value, 3, baseline.Bytes) || !bind_blob(insertBaseline->Value, 4, digest) || ::sqlite3_step(insertBaseline->Value) != SQLITE_DONE) {
						return std::unexpected(failure::unavailable);
					}
					return application::review_submission_recovery_admission{ .Record = aRecord, .Inserted = true };
				});
			}
			auto mark_possibly_transmitted(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint) -> std::expected<record, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return transaction<record>([&]() -> std::expected<record, failure> {
					auto loaded = load_locked(aKey);
					if (!loaded || !*loaded || (**loaded).RequestFingerprint != aFingerprint || !(**loaded).BaselineReviewIds || (**loaded).State != application::review_mutation_recovery_state::unresolved) {
						return std::unexpected(failure::conflict);
					}
					if ((**loaded).Transmission == application::review_mutation_recovery_transmission::possibly_transmitted) {
						return **loaded;
					}
					auto update = detail::prepare(Database, "UPDATE submission_baseline SET transmission = 1 WHERE scope_id = ?1 AND idempotency_key = ?2 AND transmission = 0");
					if (!update || !scope_key(update->Value, 1, aKey) || ::sqlite3_step(update->Value) != SQLITE_DONE || ::sqlite3_changes(Database) != 1) {
						return std::unexpected(failure::unavailable);
					}
					(**loaded).Transmission = application::review_mutation_recovery_transmission::possibly_transmitted;
					return **loaded;
				});
			}
			auto resolve(const std::string& aKey, const std::array<std::uint8_t, 32>& aFingerprint, const application::review_submission_receipt& aReceipt) -> std::expected<record, failure> override
			{
				std::scoped_lock lock{ Mutex };
				return transaction<record>([&]() -> std::expected<record, failure> {
					auto loaded = load_locked(aKey);
					if (!loaded || !*loaded || (**loaded).RequestFingerprint != aFingerprint) {
						return std::unexpected(failure::conflict);
					}
					auto candidate = **loaded;
					if (candidate.Receipt) {
						return *candidate.Receipt == aReceipt ? std::expected<record, failure>{ candidate } : std::unexpected(failure::conflict);
					}
					candidate.State   = application::review_mutation_recovery_state::succeeded;
					candidate.Receipt = aReceipt;
					if (!application::valid(candidate)) {
						return std::unexpected(failure::conflict);
					}
					submission_writer id;
					id.integer(aReceipt.ReviewDatabaseId.value());
					auto update = detail::prepare(Database, "UPDATE submission_attempt SET state = 1,review_id = ?1,review_node_id = ?2 WHERE scope_id = ?3 AND idempotency_key = ?4 AND state = 0");
					if (!update || !bind_blob(update->Value, 1, id.Bytes) || !bind_text(update->Value, 2, aReceipt.ReviewNodeId) || !scope_key(update->Value, 3, aKey) || ::sqlite3_step(update->Value) != SQLITE_DONE || ::sqlite3_changes(Database) != 1) {
						return std::unexpected(failure::unavailable);
					}
					return candidate;
				});
			}

		private:
			auto matches_scope(const application::review_submission_request& aRequest) const -> bool
			{
				if (!aRequest.LiveBinding) {
					return false;
				}
				const auto& target = aRequest.LiveBinding->Target;
				return target.Hostname.value() == Configuration.Host && std::to_string(aRequest.LiveBinding->ActingIdentity.Account.value()) == Configuration.Account && target.Repository.Owner + "/" + target.Repository.Name == Configuration.Repository && static_cast<std::int64_t>(target.Number.value()) == Configuration.PullRequest;
			}

			template <typename T, typename TAction>
			auto transaction(TAction aAction) -> std::expected<T, failure>
			{
				if (!detail::execute(Database, "BEGIN IMMEDIATE")) {
					return std::unexpected(failure::unavailable);
				}
				try {
					auto value = aAction();
					if (value && detail::execute(Database, "COMMIT")) {
						return value;
					}
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
					return value ? std::unexpected(failure::unavailable) : value;
				} catch (...) {
					static_cast<void>(detail::execute(Database, "ROLLBACK"));
					return std::unexpected(failure::unavailable);
				}
			}
			auto scope_key(::sqlite3_stmt* aStatement, int aIndex, const std::string& aKey) -> bool { return ::sqlite3_bind_int64(aStatement, aIndex, Scope) == SQLITE_OK && bind_text(aStatement, aIndex + 1, aKey); }
			auto load_staged_locked(const std::string& aKey) -> std::expected<std::optional<application::staged_publication_record>, failure>
			{
				auto query = detail::prepare(Database, "SELECT record,record_digest,state FROM staged_publication WHERE scope_id=?1 AND publication_key=?2");
				if (!query || !scope_key(query->Value, 1, aKey)) {
					return std::unexpected(failure::unavailable);
				}
				const auto row = ::sqlite3_step(query->Value);
				if (row == SQLITE_DONE) {
					return std::optional<application::staged_publication_record>{ };
				}
				if (row != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				const auto bytes  = submission_blob_at(query->Value, 0, 1048576U);
				const auto digest = submission_blob_at(query->Value, 1, 32);
				if (!bytes || !digest || digest->size() != 32 || !std::ranges::equal(core::sha256(*bytes), *digest)) {
					return std::unexpected(failure::integrity_failure);
				}
				const auto record = decode_staged_record(*bytes);
				if (!record || !matches_scope(record->Plan.Submission) || record->Plan.Submission.IdempotencyKey != aKey) {
					return std::unexpected(failure::integrity_failure);
				}
				const bool complete = record->Published && (!record->Plan.Description || record->DescriptionDone);
				if (::sqlite3_column_int(query->Value, 2) != (complete ? 1 : 0) || ::sqlite3_step(query->Value) != SQLITE_DONE) {
					return std::unexpected(failure::integrity_failure);
				}
				return std::optional{ *record };
			}

			auto load_locked(const std::string& aKey) -> std::expected<std::optional<record>, failure>
			{
				auto query = detail::prepare(Database, "SELECT request_fingerprint,authority_fingerprint,request,state,review_id,review_node_id,baseline,baseline_digest,transmission FROM submission_attempt LEFT JOIN submission_baseline USING(scope_id,idempotency_key) WHERE submission_attempt.scope_id = ?1 AND submission_attempt.idempotency_key = ?2");
				if (!query || !scope_key(query->Value, 1, aKey)) {
					return std::unexpected(failure::unavailable);
				}
				const auto row = ::sqlite3_step(query->Value);
				if (row == SQLITE_DONE) {
					return std::optional<record>{ };
				}
				if (row != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				const auto fingerprint = digest_at(query->Value, 0);
				const auto authority   = digest_at(query->Value, 1);
				const auto encoded     = submission_blob_at(query->Value, 2, 98304U);
				const auto state       = ::sqlite3_column_int64(query->Value, 3);
				if (!fingerprint || !authority || !encoded || state < 0 || state > 1) {
					return std::unexpected(failure::integrity_failure);
				}
				const auto request = decode_submission(*encoded);
				if (!request || request->IdempotencyKey != aKey || !matches_scope(*request)) {
					return std::unexpected(failure::integrity_failure);
				}
				record     value{ .Request = *request, .RequestFingerprint = *fingerprint, .AuthorityFingerprint = *authority, .State = static_cast<application::review_mutation_recovery_state>(state) };
				const auto baseline = submission_blob_at(query->Value, 6, 808U);
				const auto digest   = digest_at(query->Value, 7);
				if (baseline) {
					if (!digest || baseline_digest(*baseline) != *digest || ::sqlite3_column_type(query->Value, 8) != SQLITE_INTEGER) {
						return std::unexpected(failure::integrity_failure);
					}
					const auto transmission = ::sqlite3_column_int64(query->Value, 8);
					if (transmission < 0 || transmission > 1) {
						return std::unexpected(failure::integrity_failure);
					}
					submission_reader in{ *baseline };
					const auto        count = in.integer();
					if (count > 100U) {
						return std::unexpected(failure::integrity_failure);
					}
					value.BaselineReviewIds.emplace();
					for (auto index = std::uint64_t{ }; index < count; ++index) {
						value.BaselineReviewIds->emplace_back(in.integer());
					}
					if (!in.done()) {
						return std::unexpected(failure::integrity_failure);
					}
					value.Transmission = transmission == 0 ? application::review_mutation_recovery_transmission::prepared : application::review_mutation_recovery_transmission::possibly_transmitted;
				} else if (::sqlite3_column_type(query->Value, 6) != SQLITE_NULL || ::sqlite3_column_type(query->Value, 7) != SQLITE_NULL || ::sqlite3_column_type(query->Value, 8) != SQLITE_NULL) {
					return std::unexpected(failure::integrity_failure);
				}
				if (state == 1) {
					const auto encodedId = submission_blob_at(query->Value, 4, 8U);
					const auto node      = text_at(query->Value, 5);
					if (!encodedId || !node) {
						return std::unexpected(failure::integrity_failure);
					}
					submission_reader in{ *encodedId };
					const auto        id = in.integer();
					if (!in.done()) {
						return std::unexpected(failure::integrity_failure);
					}
					value.Receipt = application::review_submission_receipt{ .ReviewDatabaseId = application::review_database_id{ id }, .ReviewNodeId = *node, .Target = request->Proposal.Target, .ActingAccount = request->Proposal.ActingAccount, .Disposition = request->Proposal.Disposition };
				} else if (::sqlite3_column_type(query->Value, 4) != SQLITE_NULL || ::sqlite3_column_type(query->Value, 5) != SQLITE_NULL) {
					return std::unexpected(failure::integrity_failure);
				}
				if (::sqlite3_step(query->Value) != SQLITE_DONE || !application::valid(value)) {
					return std::unexpected(failure::integrity_failure);
				}
				return std::optional{ std::move(value) };
			}
			detail::state_directory                      Directory;
			::sqlite3*                                   Database;
			std::int64_t                                 Scope;
			const review_mutation_recovery_configuration Configuration;
			std::mutex                                   Mutex;
		};
		[[nodiscard]] auto establish(::sqlite3* aDatabase) -> std::expected<void, detail::failure>
		{
			if (!detail::execute(aDatabase, "BEGIN IMMEDIATE")) {
				return std::unexpected(detail::failure::unavailable);
			}
			const auto migrate = [&]() -> std::expected<void, detail::failure> {
				const auto version     = detail::scalar_integer(aDatabase, "PRAGMA user_version");
				const auto application = detail::scalar_integer(aDatabase, "PRAGMA application_id");
				if (!version || !application || *version < 0 || *version > RecoverySchemaVersion) {
					return std::unexpected(detail::failure::schema_mismatch);
				}
				if (*version == 0) {
					const std::array<std::pair<std::string_view, std::string_view>, 0> empty{ };
					if (*application != 0 || !validate_schema(aDatabase, empty)) {
						return std::unexpected(detail::failure::schema_mismatch);
					}
					for (const auto statement : RecoverySchema) {
						if (auto created = detail::execute(aDatabase, statement); !created) {
							return created;
						}
					}
					if (!detail::execute(aDatabase, "PRAGMA application_id = 1112560205") || !detail::execute(aDatabase, "INSERT INTO recovery_meta(singleton,schema_format) VALUES(1,5)")) {
						return std::unexpected(detail::failure::unavailable);
					}
				} else {
					const auto meta = detail::scalar_integer(aDatabase, "SELECT schema_format FROM recovery_meta WHERE singleton = 1");
					if (*application != RecoveryApplicationId || !meta || *meta != *version) {
						return std::unexpected(detail::failure::schema_mismatch);
					}
					auto schema = *version == 1 ? validate_schema(aDatabase, RecoverySchemaVersion1) : *version == 2 ? validate_schema(aDatabase, RecoverySchemaVersion2)
						: *version == 3                                                                              ? validate_schema(aDatabase, RecoverySchemaVersion3)
						: *version == 4                                                                              ? validate_schema(aDatabase, RecoverySchemaVersion4)
																													 : validate_schema(aDatabase, RecoverySchemaVersion5);
					if (!schema) {
						return schema;
					}
					if (*version == 1 && !detail::execute(aDatabase, RecoverySchema[3])) {
						return std::unexpected(detail::failure::unavailable);
					}
					if (*version < 3 && (!detail::execute(aDatabase, RecoverySchema[4]) || !detail::execute(aDatabase, RecoverySchema[5]))) {
						return std::unexpected(detail::failure::unavailable);
					}
					if (*version < 4 && !detail::execute(aDatabase, RecoverySchema[6])) {
						return std::unexpected(detail::failure::unavailable);
					}
					if (*version < 5 && !detail::execute(aDatabase, RecoverySchema[7])) {
						return std::unexpected(detail::failure::unavailable);
					}
					if (!detail::execute(aDatabase, "UPDATE recovery_meta SET schema_format = 5 WHERE singleton = 1")) {
						return std::unexpected(detail::failure::unavailable);
					}
				}
				if (!detail::execute(aDatabase, "PRAGMA user_version = 5")) {
					return std::unexpected(detail::failure::unavailable);
				}
				return validate_schema(aDatabase, RecoverySchemaVersion5);
			};
			const auto migrated = migrate();
			if (!migrated || !detail::execute(aDatabase, "COMMIT")) {
				static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
				return migrated ? std::unexpected(detail::failure::unavailable) : migrated;
			}
			return { };
		}

		[[nodiscard]] auto scope_id(
			::sqlite3*                                    aDatabase,
			const review_mutation_recovery_configuration& aConfiguration)
			-> std::expected<std::int64_t, detail::failure>
		{
			auto insert = detail::prepare(
				aDatabase,
				"INSERT INTO recovery_scope(host,account,repository,pull_request) "
				"VALUES(?1,?2,?3,?4) ON CONFLICT DO NOTHING");
			if (!insert || !bind_text(insert->Value, 1, aConfiguration.Host)
				|| !bind_text(insert->Value, 2, aConfiguration.Account)
				|| !bind_text(insert->Value, 3, aConfiguration.Repository)
				|| ::sqlite3_bind_int64(
					   insert->Value, 4, aConfiguration.PullRequest)
					!= SQLITE_OK
				|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(detail::failure::unavailable);
			}
			auto query = detail::prepare(
				aDatabase,
				"SELECT scope_id FROM recovery_scope WHERE host = ?1 AND account = ?2 "
				"AND repository = ?3 AND pull_request = ?4");
			if (!query || !bind_text(query->Value, 1, aConfiguration.Host)
				|| !bind_text(query->Value, 2, aConfiguration.Account)
				|| !bind_text(query->Value, 3, aConfiguration.Repository)
				|| ::sqlite3_bind_int64(query->Value, 4, aConfiguration.PullRequest)
					!= SQLITE_OK
				|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(detail::failure::unavailable);
			}
			const auto result = ::sqlite3_column_int64(query->Value, 0);
			return result > 0 ? std::expected<std::int64_t, detail::failure>{ result }
							  : std::unexpected(detail::failure::integrity_failure);
		}

		/// \brief Opens, migrates and scopes the recovery journal both stores share.
		///
		/// \details [[BPR-0883]]. Through `open_private_store_database`: the
		/// file is admitted, opened by its UTF-8 name, restricted to its owner
		/// and, on Windows, bound to the handle SQLite holds. A narrow
		/// `path.string()` is the ANSI code page on Windows, which cannot spell
		/// a profile root outside it.
		[[nodiscard]] auto open_recovery_journal(
			const review_mutation_recovery_configuration& aConfiguration)
			-> std::expected<recovery_journal, application::review_mutation_recovery_failure>
		{
			using failure = application::review_mutation_recovery_failure;
			if (aConfiguration.StorageRoot.empty() || aConfiguration.Host.empty()
				|| aConfiguration.Account.empty() || aConfiguration.Repository.empty()
				|| aConfiguration.PullRequest <= 0) {
				return std::unexpected(failure::unavailable);
			}
			auto directory = detail::open_state_directory(aConfiguration.StorageRoot);
			if (!directory || !detail::sqlite_runtime_ready()) {
				return std::unexpected(failure::unavailable);
			}
			auto opened = detail::open_private_store_database(
				*directory,
				RecoveryName,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX
					| SQLITE_OPEN_NOFOLLOW | SQLITE_OPEN_EXRESCODE);
			if (!opened) {
				return std::unexpected(failure::unavailable);
			}
			auto* const database = *opened;
			const auto  close    = [database] { static_cast<void>(::sqlite3_close_v2(database)); };
			if (auto configured = detail::configure_connection(database); !configured) {
				close();
				return std::unexpected(translate(configured.error()));
			}
			if (auto schema = establish(database); !schema) {
				close();
				return std::unexpected(translate(schema.error()));
			}
			auto scope = scope_id(database, aConfiguration);
			if (!scope) {
				close();
				return std::unexpected(translate(scope.error()));
			}
			return recovery_journal{
				.Directory = std::move(*directory),
				.Database  = database,
				.Scope     = *scope,
			};
		}
	} // namespace

	auto make_review_mutation_recovery_store(
		const review_mutation_recovery_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::review_mutation_recovery_port>,
			application::review_mutation_recovery_failure>
	{
		auto journal = open_recovery_journal(aConfiguration);
		if (!journal) {
			return std::unexpected(journal.error());
		}
		return std::unique_ptr<application::review_mutation_recovery_port>{
			new recovery_store{ std::move(*journal) }
		};
	}
	auto make_review_submission_recovery_store(
		const review_mutation_recovery_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::review_submission_recovery_port>,
			application::review_mutation_recovery_failure>
	{
		auto journal = open_recovery_journal(aConfiguration);
		if (!journal) {
			return std::unexpected(journal.error());
		}
		return std::unique_ptr<application::review_submission_recovery_port>{
			new submission_store{ std::move(*journal), aConfiguration }
		};
	}
} // namespace bpr::adapters::sqlite
