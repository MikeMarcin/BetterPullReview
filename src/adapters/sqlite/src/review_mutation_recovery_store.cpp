#include "bpr/adapters/sqlite/review_mutation_recovery_store.hpp"

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

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
		constexpr auto RecoverySchemaVersion = std::int64_t{ 2 };
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

		[[nodiscard]] auto translate(const detail::failure aFailure) noexcept
			-> application::review_mutation_recovery_failure
		{
			return aFailure == detail::failure::integrity_failure
					|| aFailure == detail::failure::schema_mismatch
				? application::review_mutation_recovery_failure::integrity_failure
				: application::review_mutation_recovery_failure::unavailable;
		}

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
			recovery_store(::sqlite3* aDatabase, const std::int64_t aScope) noexcept
				: Database{ aDatabase }
				, Scope{ aScope }
			{
			}

			~recovery_store() override
			{
				if (Database != nullptr) {
					static_cast<void>(::sqlite3_close_v2(Database));
				}
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
				if (!application::valid(aRecord)
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
				auto count = detail::prepare(
					Database,
					"SELECT count(*) FROM recovery_attempt WHERE scope_id = ?1");
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

			::sqlite3*   Database = nullptr;
			std::int64_t Scope    = 0;
			std::mutex   Mutex;
		};

		[[nodiscard]] auto establish(::sqlite3* aDatabase)
			-> std::expected<void, detail::failure>
		{
			const auto version = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			if (!version) {
				return std::unexpected(version.error());
			}
			if (*version == 0) {
				if (auto begin = detail::execute(aDatabase, "BEGIN IMMEDIATE"); !begin) {
					return begin;
				}
				for (const auto statement : RecoverySchema) {
					if (auto created = detail::execute(aDatabase, statement); !created) {
						static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
						return created;
					}
				}
				for (const auto statement : {
						 "PRAGMA application_id = 1112560205",
						 "PRAGMA user_version = 2",
						 "INSERT INTO recovery_meta(singleton,schema_format) VALUES(1,2)",
						 "COMMIT" }) {
					if (auto applied = detail::execute(aDatabase, statement); !applied) {
						static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
						return applied;
					}
				}
			}
			if (*version == 1) {
				const auto application = detail::scalar_integer(
					aDatabase, "PRAGMA application_id");
				const auto meta = detail::scalar_integer(
					aDatabase,
					"SELECT schema_format FROM recovery_meta WHERE singleton = 1");
				if (!application || *application != RecoveryApplicationId || !meta
					|| *meta != 1) {
					return std::unexpected(detail::failure::schema_mismatch);
				}
				if (auto schema = validate_schema(aDatabase, RecoverySchemaVersion1);
					!schema) {
					return schema;
				}
				if (auto begin = detail::execute(aDatabase, "BEGIN IMMEDIATE"); !begin) {
					return begin;
				}
				for (const auto statement : {
						 "CREATE TABLE recovery_baseline("
						 "scope_id INTEGER NOT NULL,"
						 "idempotency_key TEXT NOT NULL,"
						 "baseline BLOB NOT NULL CHECK(length(baseline) BETWEEN 2 AND 25802),"
						 "baseline_digest BLOB NOT NULL CHECK(length(baseline_digest) = 32),"
						 "transmission INTEGER NOT NULL CHECK(transmission BETWEEN 0 AND 1),"
						 "PRIMARY KEY(scope_id,idempotency_key),"
						 "FOREIGN KEY(scope_id,idempotency_key) REFERENCES "
						 "recovery_attempt(scope_id,idempotency_key) ON DELETE CASCADE) STRICT",
						 "PRAGMA user_version = 2",
						 "UPDATE recovery_meta SET schema_format = 2 WHERE singleton = 1",
						 "COMMIT" }) {
					if (auto applied = detail::execute(aDatabase, statement); !applied) {
						static_cast<void>(detail::execute(aDatabase, "ROLLBACK"));
						return applied;
					}
				}
			}
			const auto application = detail::scalar_integer(
				aDatabase, "PRAGMA application_id");
			const auto current = detail::scalar_integer(aDatabase, "PRAGMA user_version");
			const auto meta    = detail::scalar_integer(
				aDatabase,
				"SELECT schema_format FROM recovery_meta WHERE singleton = 1");
			if (!application || *application != RecoveryApplicationId || !current
				|| *current != RecoverySchemaVersion || !meta
				|| *meta != RecoverySchemaVersion) {
				return std::unexpected(detail::failure::schema_mismatch);
			}
			return validate_schema(aDatabase, RecoverySchemaVersion2);
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
	} // namespace

	auto make_review_mutation_recovery_store(
		const review_mutation_recovery_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::review_mutation_recovery_port>,
			application::review_mutation_recovery_failure>
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
		::sqlite3* database = nullptr;
		const auto path     = (directory->path() / RecoveryName).string();
		const auto code     = ::sqlite3_open_v2(
			path.c_str(),
			&database,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX
				| SQLITE_OPEN_EXRESCODE,
			nullptr);
		if (code != SQLITE_OK || database == nullptr) {
			if (database != nullptr) {
				static_cast<void>(::sqlite3_close_v2(database));
			}
			return std::unexpected(failure::unavailable);
		}
		auto close = [&] { static_cast<void>(::sqlite3_close_v2(database)); };
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
		return std::unique_ptr<application::review_mutation_recovery_port>{
			new recovery_store{ database, *scope }
		};
	}
} // namespace bpr::adapters::sqlite
