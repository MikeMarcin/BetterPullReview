#include "bpr/adapters/sqlite/epoch_catalog.hpp"

#include "catalog_database.hpp"

#include "bpr/core/anonymous_read_policy.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace bpr::adapters::sqlite {
	namespace {
		namespace app  = application;
		namespace core = bpr::core;
		using failure  = app::epoch_catalog_failure;
		template <typename Value>
		using result = app::epoch_catalog_result<Value>;

		class statement final
		{
		public:
			statement()                                    = default;
			statement(const statement&)                    = delete;
			auto operator=(const statement&) -> statement& = delete;
			statement(statement&& aOther) noexcept
				: Value{ std::exchange(aOther.Value, nullptr) }
			{
			}
			auto operator=(statement&& aOther) noexcept -> statement&
			{
				if (this != &aOther) {
					if (Value != nullptr) {
						(void)::sqlite3_finalize(Value);
					}
					Value = std::exchange(aOther.Value, nullptr);
				}
				return *this;
			}
			~statement()
			{
				if (Value != nullptr) {
					(void)::sqlite3_finalize(Value);
				}
			}

			::sqlite3_stmt* Value = nullptr;
		};

		struct stop_context
		{
			std::stop_token Stop;
		};

		auto stop_progress(void* aOpaque) noexcept -> int
		{
			const auto* context = static_cast<const stop_context*>(aOpaque);
			return context != nullptr && context->Stop.stop_requested() ? 1 : 0;
		}

		class progress_guard final
		{
		public:
			progress_guard(::sqlite3* aDatabase, std::stop_token aStop)
				: Database{ aDatabase }
				, Context{ .Stop = aStop }
			{
				::sqlite3_progress_handler(Database, 128, stop_progress, &Context);
			}
			~progress_guard()
			{
				::sqlite3_progress_handler(Database, 0, nullptr, nullptr);
			}

		private:
			::sqlite3*   Database;
			stop_context Context;
		};

		[[nodiscard]] auto sql_failure(
			::sqlite3*      aDatabase,
			std::stop_token aStop,
			const failure   aFallback = failure::unavailable) noexcept -> failure
		{
			if (aStop.stop_requested()) {
				return failure::cancelled;
			}
			switch (detail::classify_sqlite_result(
				::sqlite3_extended_errcode(aDatabase))) {
			case detail::sqlite_result_classification::busy:
				return failure::busy;
			case detail::sqlite_result_classification::integrity_failure:
				return failure::integrity_failure;
			case detail::sqlite_result_classification::cancelled:
				return failure::cancelled;
			case detail::sqlite_result_classification::quota_exceeded:
				return failure::quota_exceeded;
			case detail::sqlite_result_classification::unavailable:
				return aFallback;
			}
			return aFallback;
		}

		[[nodiscard]] auto prepare(::sqlite3* aDatabase, std::string_view aSql)
			-> result<statement>
		{
			statement   output;
			const char* tail = nullptr;
			const auto  code = ::sqlite3_prepare_v3(
				aDatabase,
				aSql.data(),
				static_cast<int>(aSql.size()),
				SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_NO_VTAB,
				&output.Value,
				&tail);
			if (code != SQLITE_OK || output.Value == nullptr) {
				return std::unexpected(sql_failure(aDatabase, { }));
			}
			while (tail != aSql.data() + aSql.size()) {
				const auto value = static_cast<unsigned char>(*tail);
				if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
					return std::unexpected(failure::schema_mismatch);
				}
				++tail;
			}
			return output;
		}

		[[nodiscard]] auto execute(
			::sqlite3*       aDatabase,
			std::string_view aSql,
			std::stop_token  aStop) -> result<void>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			auto query = prepare(aDatabase, aSql);
			if (!query) {
				return std::unexpected(query.error());
			}
			if (::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			return { };
		}

		class transaction final
		{
		public:
			transaction(::sqlite3* aDatabase, std::stop_token aStop)
				: Database{ aDatabase }
				, Stop{ aStop }
			{
			}
			transaction(const transaction&)                    = delete;
			auto operator=(const transaction&) -> transaction& = delete;
			~transaction()
			{
				if (Active) {
					(void)::sqlite3_exec(
						Database, "ROLLBACK", nullptr, nullptr, nullptr);
				}
			}

			[[nodiscard]] auto begin() -> result<void>
			{
				auto opened = execute(Database, "BEGIN IMMEDIATE", Stop);
				Active      = opened.has_value();
				return opened;
			}

			[[nodiscard]] auto commit() -> result<void>
			{
				auto committed = execute(Database, "COMMIT", Stop);
				if (committed) {
					Active = false;
				}
				return committed;
			}

		private:
			::sqlite3*      Database;
			std::stop_token Stop;
			bool            Active = false;
		};

		class read_transaction final
		{
		public:
			read_transaction(::sqlite3* aDatabase, std::stop_token aStop)
				: Database{ aDatabase }
				, Stop{ aStop }
			{
			}
			read_transaction(const read_transaction&)                    = delete;
			auto operator=(const read_transaction&) -> read_transaction& = delete;
			~read_transaction()
			{
				if (Active) {
					(void)::sqlite3_exec(
						Database, "ROLLBACK", nullptr, nullptr, nullptr);
				}
			}

			[[nodiscard]] auto begin() -> result<void>
			{
				auto opened = execute(Database, "BEGIN", Stop);
				Active      = opened.has_value();
				return opened;
			}

			[[nodiscard]] auto commit() -> result<void>
			{
				auto committed = execute(Database, "COMMIT", Stop);
				if (committed) {
					Active = false;
				}
				return committed;
			}

		private:
			::sqlite3*      Database;
			std::stop_token Stop;
			bool            Active = false;
		};

		template <typename Bytes>
		[[nodiscard]] auto bind_bytes(
			::sqlite3_stmt* aQuery, const int aIndex, const Bytes& aBytes) noexcept -> bool
		{
			return ::sqlite3_bind_blob(
					   aQuery,
					   aIndex,
					   aBytes.data(),
					   static_cast<int>(aBytes.size()),
					   SQLITE_TRANSIENT)
				== SQLITE_OK;
		}

		[[nodiscard]] constexpr auto encode_u64(const std::uint64_t aValue) noexcept
			-> std::array<std::uint8_t, 8>
		{
			std::array<std::uint8_t, 8> output{ };
			for (std::size_t index = 0; index < output.size(); ++index) {
				output[index] = static_cast<std::uint8_t>(
					aValue >> ((output.size() - index - 1U) * 8U));
			}
			return output;
		}

		[[nodiscard]] auto bind_u64(
			::sqlite3_stmt* aQuery, const int aIndex, const std::uint64_t aValue) noexcept
			-> bool
		{
			const auto encoded = encode_u64(aValue);
			return bind_bytes(aQuery, aIndex, encoded);
		}

		template <typename Identity>
		[[nodiscard]] auto bind_fixed(
			::sqlite3_stmt* aQuery, const int aIndex, const Identity& aIdentity) noexcept
			-> bool
		{
			return bind_bytes(aQuery, aIndex, aIdentity.bytes());
		}

		[[nodiscard]] auto bind_operation(
			::sqlite3_stmt*                 aQuery,
			const int                       aIndex,
			const core::epoch_operation_id& aOperation) noexcept -> bool
		{
			return bind_bytes(aQuery, aIndex, aOperation.bytes());
		}

		[[nodiscard]] auto bind_oid(
			::sqlite3_stmt*                     aQuery,
			const int                           aAlgorithmIndex,
			const int                           aDigestIndex,
			const core::git_hash_algorithm      aAlgorithm,
			const std::array<std::uint8_t, 32>& aBytes) noexcept -> bool
		{
			const auto count = aAlgorithm == core::git_hash_algorithm::sha1 ? 20U
				: aAlgorithm == core::git_hash_algorithm::sha256            ? 32U
																			: 0U;
			return count != 0U
				&& ::sqlite3_bind_int(
					   aQuery, aAlgorithmIndex, static_cast<int>(aAlgorithm))
				== SQLITE_OK
				&& ::sqlite3_bind_blob(
					   aQuery,
					   aDigestIndex,
					   aBytes.data(),
					   static_cast<int>(count),
					   SQLITE_TRANSIENT)
				== SQLITE_OK;
		}

		template <typename Oid>
		[[nodiscard]] auto bind_oid(
			::sqlite3_stmt* aQuery,
			const int       aAlgorithmIndex,
			const int       aDigestIndex,
			const Oid&      aOid) noexcept -> bool
		{
			return bind_oid(
				aQuery, aAlgorithmIndex, aDigestIndex, aOid.algorithm(), aOid.bytes());
		}

		[[nodiscard]] auto bind_pack(
			::sqlite3_stmt*          aQuery,
			const int                aAlgorithmIndex,
			const int                aDigestIndex,
			const app::git_pack_oid& aPack) noexcept -> bool
		{
			return bind_oid(
				aQuery,
				aAlgorithmIndex,
				aDigestIndex,
				aPack.Algorithm,
				aPack.Digest);
		}

		[[nodiscard]] auto read_blob(
			::sqlite3_stmt*         aQuery,
			const int               aColumn,
			std::span<std::uint8_t> aOutput) noexcept -> bool
		{
			if (::sqlite3_column_type(aQuery, aColumn) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(aQuery, aColumn)
					!= static_cast<int>(aOutput.size())) {
				return false;
			}
			const auto* value = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aQuery, aColumn));
			if (value == nullptr) {
				return false;
			}
			std::memcpy(aOutput.data(), value, aOutput.size());
			return true;
		}

		[[nodiscard]] auto read_u64(
			::sqlite3_stmt* aQuery, const int aColumn) noexcept
			-> std::optional<std::uint64_t>
		{
			std::array<std::uint8_t, 8> encoded{ };
			if (!read_blob(aQuery, aColumn, encoded)) {
				return std::nullopt;
			}
			std::uint64_t output = 0U;
			for (const auto byte : encoded) {
				output = (output << 8U) | byte;
			}
			if (output == 0U) {
				return std::nullopt;
			}
			return output;
		}

		// The same eight bytes, read without the nonzero rule. `read_u64`
		// refuses a zero because under the authenticated mode no identity may
		// be zero, and that rule is right there. A public-preview row stores
		// exactly zero because it names no reader, so it is read through this
		// one and the caller's mode decides which value is correct. Nothing
		// here treats zero as a sentinel: it is only ever compared against the
		// zero the key itself carries, and `valid` has already refused a key
		// whose reader disagrees with its mode.
		[[nodiscard]] auto read_u64_including_absent(
			::sqlite3_stmt* aQuery, const int aColumn) noexcept
			-> std::optional<std::uint64_t>
		{
			std::array<std::uint8_t, 8> encoded{ };
			if (!read_blob(aQuery, aColumn, encoded)) {
				return std::nullopt;
			}
			std::uint64_t output = 0U;
			for (const auto byte : encoded) {
				output = (output << 8U) | byte;
			}
			return output;
		}

		template <typename Identity>
		[[nodiscard]] auto read_fixed(::sqlite3_stmt* aQuery, const int aColumn)
			-> std::optional<Identity>
		{
			std::array<std::uint8_t, 32> bytes{ };
			if (!read_blob(aQuery, aColumn, bytes)) {
				return std::nullopt;
			}
			Identity output{ bytes };
			return output.valid() ? std::optional<Identity>{ output } : std::nullopt;
		}

		template <typename Oid>
		[[nodiscard]] auto read_oid(
			::sqlite3_stmt* aQuery, const int aAlgorithmColumn, const int aDigestColumn)
			-> std::optional<Oid>
		{
			if (::sqlite3_column_type(aQuery, aAlgorithmColumn) != SQLITE_INTEGER) {
				return std::nullopt;
			}
			const auto algorithm = static_cast<core::git_hash_algorithm>(
				::sqlite3_column_int(aQuery, aAlgorithmColumn));
			const auto size = algorithm == core::git_hash_algorithm::sha1 ? 20U
				: algorithm == core::git_hash_algorithm::sha256           ? 32U
																		  : 0U;
			if (size == 0U || ::sqlite3_column_type(aQuery, aDigestColumn) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(aQuery, aDigestColumn)
					!= static_cast<int>(size)) {
				return std::nullopt;
			}
			std::array<std::uint8_t, 32> bytes{ };
			const auto*                  source = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aQuery, aDigestColumn));
			if (source == nullptr) {
				return std::nullopt;
			}
			std::memcpy(bytes.data(), source, size);
			Oid output{ algorithm, bytes };
			return output.valid() ? std::optional<Oid>{ output } : std::nullopt;
		}

		[[nodiscard]] auto read_pack(
			::sqlite3_stmt* aQuery, const int aAlgorithmColumn, const int aDigestColumn)
			-> std::optional<app::git_pack_oid>
		{
			if (::sqlite3_column_type(aQuery, aAlgorithmColumn) != SQLITE_INTEGER) {
				return std::nullopt;
			}
			const auto algorithm = static_cast<core::git_hash_algorithm>(
				::sqlite3_column_int(aQuery, aAlgorithmColumn));
			const auto size = algorithm == core::git_hash_algorithm::sha1 ? 20U
				: algorithm == core::git_hash_algorithm::sha256           ? 32U
																		  : 0U;
			if (size == 0U || ::sqlite3_column_type(aQuery, aDigestColumn) != SQLITE_BLOB
				|| ::sqlite3_column_bytes(aQuery, aDigestColumn)
					!= static_cast<int>(size)) {
				return std::nullopt;
			}
			app::git_pack_oid output{ .Algorithm = algorithm };
			const auto*       source = static_cast<const std::uint8_t*>(
				::sqlite3_column_blob(aQuery, aDigestColumn));
			if (source == nullptr) {
				return std::nullopt;
			}
			std::memcpy(output.Digest.data(), source, size);
			return app::valid(output) ? std::optional{ output } : std::nullopt;
		}

		template <typename Enum>
		[[nodiscard]] constexpr auto stored_enum(const Enum aValue) noexcept -> int
		{
			return static_cast<int>(aValue) + 1;
		}

		template <typename Enum>
		[[nodiscard]] constexpr auto restored_enum(const int aValue) noexcept -> Enum
		{
			return static_cast<Enum>(aValue - 1);
		}

		[[nodiscard]] auto map_failure(
			const detail::catalog_database_failure aValue) noexcept -> failure
		{
			using source = detail::catalog_database_failure;
			switch (aValue) {
			case source::invalid_path:
				return failure::invalid_path;
			case source::invalid_permissions:
				return failure::invalid_permissions;
			case source::unavailable:
				return failure::unavailable;
			case source::schema_mismatch:
				return failure::schema_mismatch;
			case source::integrity_failure:
				return failure::integrity_failure;
			case source::quota_exceeded:
				return failure::quota_exceeded;
			case source::cancelled:
				return failure::cancelled;
			case source::platform_unsupported:
				return failure::platform_unsupported;
			}
			return failure::unavailable;
		}

		struct catalog_state final
		{
			explicit catalog_state(detail::catalog_database aDatabase)
				: Database{ std::move(aDatabase) }
			{
			}

			detail::catalog_database Database;
			std::mutex               Mutex;
		};

		[[nodiscard]] auto read_catalog_identity(::sqlite3* aDatabase)
			-> result<core::epoch_catalog_incarnation_id>
		{
			auto query = prepare(
				aDatabase,
				"SELECT incarnation FROM catalog_meta WHERE singleton = 1");
			if (!query || ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(
					query ? failure::schema_mismatch : query.error());
			}
			auto incarnation = read_fixed<core::epoch_catalog_incarnation_id>(
				query->Value, 0);
			if (!incarnation || ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return *incarnation;
		}

		[[nodiscard]] auto random_identity() -> result<std::array<std::uint8_t, 32>>
		{
			std::array<std::uint8_t, 32> bytes{ };
			if (!detail::secure_random_bytes(bytes)
				|| std::ranges::all_of(bytes, [](const auto aByte) {
					   return aByte == 0U;
				   })) {
				return std::unexpected(failure::unavailable);
			}
			return bytes;
		}

		class sqlite_epoch_catalog_session final : public app::epoch_catalog_session
		{
		public:
			sqlite_epoch_catalog_session(
				std::shared_ptr<catalog_state> aState,
				app::epoch_catalog_binding     aBinding,
				core::application_session_id   aApplicationSession)
				: State{ std::move(aState) }
				, Binding{ aBinding }
				, ApplicationSession{ aApplicationSession }
			{
			}

			[[nodiscard]] auto binding() const noexcept
				-> const app::epoch_catalog_binding& override
			{
				return Binding;
			}

			[[nodiscard]] auto snapshot(std::stop_token aStop)
				-> result<app::epoch_catalog_snapshot> override;
			[[nodiscard]] auto source_observation_exists(
				core::epoch_source_observation_id aObservation,
				std::stop_token                   aStop) -> result<bool> override;
			[[nodiscard]] auto canonical_review(
				const app::canonical_review_request& aRequest,
				std::stop_token                      aStop)
				-> result<core::catalog_review_association> override;
			[[nodiscard]] auto exact_anchors(
				const app::exact_anchor_lookup_request& aRequest,
				std::stop_token                         aStop) -> result<app::exact_anchor_lookup_result> override;
			[[nodiscard]] auto history(
				const app::epoch_history_page_request& aRequest,
				std::stop_token                        aStop) -> result<app::epoch_history_page> override;
			[[nodiscard]] auto mark_anchor_availability(
				const app::mark_anchor_availability_request& aRequest,
				std::stop_token                              aStop) -> result<app::catalog_commit_result> override;
			[[nodiscard]] auto reserve_append(
				const app::append_reservation_request& aRequest,
				std::stop_token                        aStop) -> result<app::append_operation_record> override;
			[[nodiscard]] auto record_spool(
				const app::record_spool_request& aRequest,
				std::stop_token                  aStop) -> result<app::append_operation_record> override;
			[[nodiscard]] auto record_install(
				const app::record_install_request& aRequest,
				std::stop_token                    aStop) -> result<app::append_operation_record> override;
			[[nodiscard]] auto record_candidate(
				const app::record_candidate_request& aRequest,
				std::stop_token                      aStop) -> result<app::append_operation_record> override;
			[[nodiscard]] auto transition_operation(
				const app::operation_transition_request& aRequest,
				std::stop_token                          aStop) -> result<app::epoch_recovery_operation> override;
			[[nodiscard]] auto finalize_append(
				const app::finalize_append_request& aRequest,
				std::stop_token                     aStop) -> result<app::catalog_commit_result> override;
			[[nodiscard]] auto associate_review(
				const app::associate_review_request& aRequest,
				std::stop_token                      aStop) -> result<app::catalog_commit_result> override;
			[[nodiscard]] auto begin_prune(
				const app::begin_prune_request& aRequest,
				std::stop_token                 aStop) -> result<app::maintenance_operation_record> override;
			[[nodiscard]] auto finalize_prune(
				const app::finalize_prune_request& aRequest,
				std::stop_token                    aStop) -> result<app::catalog_commit_result> override;
			[[nodiscard]] auto begin_cache_delete(
				const app::begin_cache_delete_request& aRequest,
				std::stop_token                        aStop) -> result<app::maintenance_operation_record> override;
			[[nodiscard]] auto cache_delete_impact(
				core::repository_cache_scope_id aCache,
				std::stop_token                 aStop) -> result<app::cache_delete_impact> override;
			[[nodiscard]] auto finalize_cache_delete(
				const app::finalize_cache_delete_request& aRequest,
				std::stop_token                           aStop) -> result<void> override;
			[[nodiscard]] auto recovery_page(
				std::optional<core::epoch_operation_id> aAfter,
				std::uint16_t                           aLimit,
				std::stop_token                         aStop) -> result<app::epoch_recovery_page> override;

		private:
			[[nodiscard]] auto load_append(
				core::epoch_operation_id aOperation) -> result<app::append_operation_record>;
			[[nodiscard]] auto load_maintenance(
				core::epoch_operation_id aOperation)
				-> result<app::maintenance_operation_record>;

			std::shared_ptr<catalog_state> State;
			app::epoch_catalog_binding     Binding;
			core::application_session_id   ApplicationSession;
		};

		class sqlite_epoch_catalog_port final : public app::epoch_catalog_port
		{
		public:
			explicit sqlite_epoch_catalog_port(std::shared_ptr<catalog_state> aState)
				: State{ std::move(aState) }
			{
			}

			[[nodiscard]] auto open(
				const app::epoch_catalog_open_request& aRequest,
				std::stop_token                        aStop)
				-> result<std::unique_ptr<app::epoch_catalog_session>> override;

		private:
			std::shared_ptr<catalog_state> State;
		};

		// The catalog stores the access mode as a small positive integer
		// rather than the enumerator's own value, so the schema's CHECK
		// constraints read as constraints rather than as a coincidence of C++
		// enumerator ordering, and a reordering of that enumeration cannot
		// silently re-read every stored row as the other mode.
		[[nodiscard]] constexpr auto access_mode_column(
			const core::github_access_mode aMode) noexcept -> int
		{
			switch (aMode) {
			case core::github_access_mode::authenticated:
				return 1;
			case core::github_access_mode::public_preview:
				return 2;
			}
			return 0;
		}

		// The mode is bound with the rest of the authority and never
		// separately, so no query can filter on an identity while ignoring
		// which mode owns it. It is bound last so a caller's own trailing
		// parameters stay numbered relative to the whole key.
		[[nodiscard]] auto bind_authority_key(
			::sqlite3_stmt*                         aQuery,
			const app::epoch_catalog_authority_key& aKey,
			const int                               aFirst) noexcept -> bool
		{
			const auto mode = access_mode_column(aKey.Mode);
			return mode != 0 && bind_u64(aQuery, aFirst, aKey.Profile.value())
				&& bind_u64(aQuery, aFirst + 1, aKey.ProfileRevision.value())
				&& bind_u64(aQuery, aFirst + 2, aKey.Host.value())
				&& bind_fixed(aQuery, aFirst + 3, aKey.HostAuthority)
				&& bind_u64(aQuery, aFirst + 4, aKey.Account.value())
				&& bind_u64(aQuery, aFirst + 5, aKey.Repository.value())
				&& ::sqlite3_bind_int(
					   aQuery, aFirst + 6, static_cast<int>(aKey.ObjectFormat))
				== SQLITE_OK
				&& ::sqlite3_bind_int(aQuery, aFirst + 7, mode) == SQLITE_OK;
		}

		[[nodiscard]] auto compact_retired_cache_journals(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const core::repository_cache_scope_id&    aCache,
			std::stop_token                           aStop) -> result<void>
		{
			auto nonterminal = prepare(
				aDatabase,
				"SELECT 1 FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND terminal NOT IN (1, 2) LIMIT 1");
			if (!nonterminal || !bind_fixed(nonterminal->Value, 1, aIncarnation)
				|| !bind_fixed(nonterminal->Value, 2, aCache)) {
				return std::unexpected(
					nonterminal ? failure::unavailable : nonterminal.error());
			}
			const auto nonterminalCode = ::sqlite3_step(nonterminal->Value);
			if (nonterminalCode == SQLITE_ROW) {
				return std::unexpected(failure::repair_required);
			}
			if (nonterminalCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}

			struct cleanup_statement final
			{
				std::string_view Delete;
				std::string_view Verify;
			};
			constexpr std::array cleanup{
				cleanup_statement{
					"DELETE FROM append_candidate_review WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_candidate_review d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_hidden_candidate WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_hidden_candidate d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_installed_artifact WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_installed_artifact d "
					"JOIN operation o ON o.incarnation = d.incarnation "
					"AND o.operation = d.operation WHERE d.incarnation = ?1 "
					"AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_install WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_install d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_inventory WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_inventory d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_receipt WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_receipt d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_source_review WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_source_review d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM append_operation WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM append_operation d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM prune_operation WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM prune_operation d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
				cleanup_statement{
					"DELETE FROM cache_delete_scope WHERE incarnation = ?1 "
					"AND operation IN (SELECT operation FROM operation "
					"WHERE incarnation = ?1 AND cache_scope = ?2)",
					"SELECT count(*) FROM cache_delete_scope d JOIN operation o "
					"ON o.incarnation = d.incarnation AND o.operation = d.operation "
					"WHERE d.incarnation = ?1 AND o.cache_scope = ?2" },
			};
			for (const auto& cleanupStatement : cleanup) {
				auto removed = prepare(aDatabase, cleanupStatement.Delete);
				if (!removed || !bind_fixed(removed->Value, 1, aIncarnation)
					|| !bind_fixed(removed->Value, 2, aCache)
					|| ::sqlite3_step(removed->Value) != SQLITE_DONE) {
					return std::unexpected(
						removed ? sql_failure(aDatabase, aStop) : removed.error());
				}
				auto remaining = prepare(aDatabase, cleanupStatement.Verify);
				if (!remaining || !bind_fixed(remaining->Value, 1, aIncarnation)
					|| !bind_fixed(remaining->Value, 2, aCache)
					|| ::sqlite3_step(remaining->Value) != SQLITE_ROW
					|| ::sqlite3_column_type(remaining->Value, 0) != SQLITE_INTEGER
					|| ::sqlite3_column_int64(remaining->Value, 0) != 0
					|| ::sqlite3_step(remaining->Value) != SQLITE_DONE) {
					return std::unexpected(
						remaining ? failure::repair_required : remaining.error());
				}
			}

			auto removedOperations = prepare(
				aDatabase,
				"DELETE FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND terminal IN (1, 2)");
			if (!removedOperations
				|| !bind_fixed(removedOperations->Value, 1, aIncarnation)
				|| !bind_fixed(removedOperations->Value, 2, aCache)
				|| ::sqlite3_step(removedOperations->Value) != SQLITE_DONE) {
				return std::unexpected(
					removedOperations ? sql_failure(aDatabase, aStop)
									  : removedOperations.error());
			}
			auto remainingOperations = prepare(
				aDatabase,
				"SELECT count(*) FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2");
			if (!remainingOperations
				|| !bind_fixed(remainingOperations->Value, 1, aIncarnation)
				|| !bind_fixed(remainingOperations->Value, 2, aCache)
				|| ::sqlite3_step(remainingOperations->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(remainingOperations->Value, 0)
					!= SQLITE_INTEGER
				|| ::sqlite3_column_int64(remainingOperations->Value, 0) != 0
				|| ::sqlite3_step(remainingOperations->Value) != SQLITE_DONE) {
				return std::unexpected(
					remainingOperations ? failure::repair_required
										: remainingOperations.error());
			}
			return { };
		}

		auto sqlite_epoch_catalog_port::open(
			const app::epoch_catalog_open_request& aRequest,
			std::stop_token                        aStop)
			-> result<std::unique_ptr<app::epoch_catalog_session>>
		{
			if (!app::valid(aRequest)) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto incarnation = read_catalog_identity(database);
			if (!incarnation) {
				return std::unexpected(incarnation.error());
			}

			std::optional<core::repository_cache_scope_id> cacheScope;
			auto                                           repositoryLifecycle = 0;
			auto                                           repository          = prepare(
				database,
				"SELECT cache_scope, lifecycle FROM repository_scope "
				"WHERE incarnation = ?1 AND profile_id = ?2 "
				"AND profile_revision = ?3 AND host_id = ?4 "
				"AND host_authority = ?5 AND account_id = ?6 "
				"AND repository_id = ?7 AND object_format = ?8 "
				"AND access_mode = ?9 AND lifecycle != 4");
			if (!repository || !bind_fixed(repository->Value, 1, *incarnation)
				|| !bind_authority_key(repository->Value, aRequest.Authority, 2)) {
				return std::unexpected(
					repository ? failure::unavailable : repository.error());
			}
			auto code = ::sqlite3_step(repository->Value);
			if (code == SQLITE_ROW) {
				cacheScope = read_fixed<core::repository_cache_scope_id>(
					repository->Value, 0);
				const auto lifecycle = ::sqlite3_column_int(repository->Value, 1);
				if (!cacheScope || lifecycle < 1 || lifecycle > 3
					|| ::sqlite3_step(repository->Value) != SQLITE_DONE) {
					return std::unexpected(failure::repair_required);
				}
				repositoryLifecycle = lifecycle;
			} else if (code != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}

			if (!cacheScope) {
				constexpr std::size_t scopeAllocationAttempts = 8U;
				for (std::size_t attempt = 0U;
					 attempt < scopeAllocationAttempts && !cacheScope;
					 ++attempt) {
					auto identity = random_identity();
					if (!identity) {
						return std::unexpected(identity.error());
					}
					const auto candidate = core::repository_cache_scope_id{ *identity };
					auto       insert    = prepare(
						database,
						"INSERT INTO repository_scope(incarnation, cache_scope, "
						"profile_id, profile_revision, host_id, host_authority, "
						"account_id, repository_id, object_format, access_mode, "
						"lifecycle) "
						"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1)");
					if (!insert || !bind_fixed(insert->Value, 1, *incarnation)
						|| !bind_fixed(insert->Value, 2, candidate)
						|| !bind_authority_key(
							insert->Value, aRequest.Authority, 3)) {
						return std::unexpected(failure::unavailable);
					}
					const auto insertCode = ::sqlite3_step(insert->Value);
					if (insertCode == SQLITE_DONE) {
						cacheScope          = candidate;
						repositoryLifecycle = 1;
					} else if ((static_cast<unsigned>(insertCode) & 0xFFU) != SQLITE_CONSTRAINT) {
						return std::unexpected(sql_failure(database, aStop));
					}
				}
				if (!cacheScope) {
					return std::unexpected(failure::authority_mismatch);
				}
			}

			if (repositoryLifecycle == 1) {
				auto activeScopeCount = prepare(
					database,
					"SELECT count(*) FROM pull_request_scope "
					"WHERE incarnation = ?1 AND cache_scope = ?2");
				if (!activeScopeCount
					|| !bind_fixed(
						activeScopeCount->Value, 1, *incarnation)
					|| !bind_fixed(activeScopeCount->Value, 2, *cacheScope)
					|| ::sqlite3_step(activeScopeCount->Value) != SQLITE_ROW) {
					return std::unexpected(
						activeScopeCount ? failure::unavailable
										 : activeScopeCount.error());
				}
				const auto activeCount = ::sqlite3_column_int64(activeScopeCount->Value, 0);
				if (activeCount < 0
					|| ::sqlite3_step(activeScopeCount->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				auto retired = prepare(
					database,
					"SELECT p.pr_scope, p.cache_scope, p.viewer_id, "
					"p.pull_request_number, p.lifecycle "
					"FROM repository_scope r JOIN pull_request_scope p "
					"ON p.incarnation = r.incarnation "
					"AND p.cache_scope = r.cache_scope "
					"WHERE r.incarnation = ?1 AND r.profile_id = ?2 "
					"AND r.profile_revision = ?3 AND r.host_id = ?4 "
					"AND r.host_authority = ?5 AND r.account_id = ?6 "
					"AND r.repository_id = ?7 AND r.object_format = ?8 "
					"AND r.access_mode = ?9 "
					"AND r.lifecycle = 4 AND p.pull_request_id = ?10");
				if (!retired || !bind_fixed(retired->Value, 1, *incarnation)
					|| !bind_authority_key(retired->Value, aRequest.Authority, 2)
					|| !bind_u64(
						retired->Value, 10, aRequest.Authority.PullRequest.value())) {
					return std::unexpected(
						retired ? failure::unavailable : retired.error());
				}
				const auto retiredCode = ::sqlite3_step(retired->Value);
				if (retiredCode == SQLITE_ROW) {
					if (activeCount
						>= static_cast<std::int64_t>(
							app::maximum_cache_delete_scope_count)) {
						return std::unexpected(failure::quota_exceeded);
					}
					const auto retiredScope = read_fixed<core::pull_request_authority_scope_id>(
						retired->Value, 0);
					const auto retiredCache = read_fixed<core::repository_cache_scope_id>(
						retired->Value, 1);
					const auto viewer = read_u64(retired->Value, 2);
					if (!retiredScope || !retiredCache || !viewer
						|| *viewer != aRequest.Authority.Viewer.value()
						|| ::sqlite3_column_int64(retired->Value, 3)
							!= aRequest.Authority.Number.value()
						|| ::sqlite3_column_int(retired->Value, 4) != 3
						|| ::sqlite3_step(retired->Value) != SQLITE_DONE) {
						return std::unexpected(failure::repair_required);
					}
					if (auto compacted = compact_retired_cache_journals(
							database, *incarnation, *retiredCache, aStop);
						!compacted) {
						return std::unexpected(compacted.error());
					}
					auto reactivate = prepare(
						database,
						"UPDATE pull_request_scope SET cache_scope = ?1, "
						"lifecycle = 1, retained_epoch_count = 0, "
						"retention_rehydration_required = CASE "
						"WHEN latest_live_ordinal IS NOT NULL AND "
						"(SELECT count(*) FROM epoch_anchor a WHERE "
						"a.incarnation = pull_request_scope.incarnation AND "
						"a.pr_scope = pull_request_scope.pr_scope AND "
						"a.ordinal = pull_request_scope.latest_live_ordinal AND "
						"a.availability = 4) = 2 THEN 1 ELSE 0 END "
						"WHERE incarnation = ?2 AND pr_scope = ?3 "
						"AND cache_scope = ?4 AND lifecycle = 3");
					if (!reactivate
						|| !bind_fixed(reactivate->Value, 1, *cacheScope)
						|| !bind_fixed(reactivate->Value, 2, *incarnation)
						|| !bind_fixed(reactivate->Value, 3, *retiredScope)
						|| !bind_fixed(reactivate->Value, 4, *retiredCache)
						|| ::sqlite3_step(reactivate->Value) != SQLITE_DONE
						|| ::sqlite3_changes(database) != 1) {
						return std::unexpected(failure::repair_required);
					}
				} else if (retiredCode != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
			}

			std::optional<core::pull_request_authority_scope_id> prScope;
			std::int64_t                                         catalogRevision = 0;
			auto                                                 pullRequest     = prepare(
				database,
				"SELECT pr_scope, viewer_id, pull_request_number, "
				"catalog_revision, lifecycle FROM pull_request_scope "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND pull_request_id = ?3");
			if (!pullRequest || !bind_fixed(pullRequest->Value, 1, *incarnation)
				|| !bind_fixed(pullRequest->Value, 2, *cacheScope)
				|| !bind_u64(
					pullRequest->Value, 3, aRequest.Authority.PullRequest.value())) {
				return std::unexpected(
					pullRequest ? failure::unavailable : pullRequest.error());
			}
			code = ::sqlite3_step(pullRequest->Value);
			if (code == SQLITE_ROW) {
				prScope = read_fixed<core::pull_request_authority_scope_id>(
					pullRequest->Value, 0);
				const auto viewer
					= read_u64_including_absent(pullRequest->Value, 1);
				catalogRevision = ::sqlite3_column_int64(pullRequest->Value, 3);
				if (!prScope || !viewer
					|| *viewer != aRequest.Authority.Viewer.value()
					|| ::sqlite3_column_int64(pullRequest->Value, 2)
						!= aRequest.Authority.Number.value()
					|| catalogRevision < 0
					|| (::sqlite3_column_int(pullRequest->Value, 4) != 1
						&& ::sqlite3_column_int(pullRequest->Value, 4) != 3
						&& ::sqlite3_column_int(pullRequest->Value, 4) != 4)
					|| ::sqlite3_step(pullRequest->Value) != SQLITE_DONE) {
					return std::unexpected(failure::authority_mismatch);
				}
			} else if (code != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}

			if (!prScope) {
				if (repositoryLifecycle != 1) {
					return std::unexpected(failure::operation_conflict);
				}
				auto scopeCount = prepare(
					database,
					"SELECT count(*) FROM pull_request_scope "
					"WHERE incarnation = ?1 AND cache_scope = ?2");
				if (!scopeCount
					|| !bind_fixed(scopeCount->Value, 1, *incarnation)
					|| !bind_fixed(scopeCount->Value, 2, *cacheScope)
					|| ::sqlite3_step(scopeCount->Value) != SQLITE_ROW) {
					return std::unexpected(
						scopeCount ? failure::unavailable : scopeCount.error());
				}
				const auto count = ::sqlite3_column_int64(scopeCount->Value, 0);
				if (count < 0
					|| ::sqlite3_step(scopeCount->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (count
					>= static_cast<std::int64_t>(
						app::maximum_cache_delete_scope_count)) {
					return std::unexpected(failure::quota_exceeded);
				}
				constexpr std::size_t scopeAllocationAttempts = 8U;
				for (std::size_t attempt = 0U;
					 attempt < scopeAllocationAttempts && !prScope;
					 ++attempt) {
					auto identity = random_identity();
					if (!identity) {
						return std::unexpected(identity.error());
					}
					const auto candidate = core::pull_request_authority_scope_id{ *identity };
					auto       insert    = prepare(
						database,
						"INSERT INTO pull_request_scope(incarnation, pr_scope, "
						"cache_scope, viewer_id, access_mode, pull_request_id, "
						"pull_request_number, next_ordinal, catalog_revision, "
						"retained_epoch_count, "
						"latest_live_ordinal, previous_distinct_head_ordinal, "
						"lifecycle, retention_rehydration_required) "
						"VALUES(?1, ?2, ?3, ?4, ?7, ?5, ?6, 1, 0, 0, NULL, NULL, "
						"1, 0)");
					if (!insert || !bind_fixed(insert->Value, 1, *incarnation)
						|| !bind_fixed(insert->Value, 2, candidate)
						|| !bind_fixed(insert->Value, 3, *cacheScope)
						|| !bind_u64(
							insert->Value, 4, aRequest.Authority.Viewer.value())
						|| !bind_u64(
							insert->Value,
							5,
							aRequest.Authority.PullRequest.value())
						|| ::sqlite3_bind_int64(
							   insert->Value, 6, aRequest.Authority.Number.value())
							!= SQLITE_OK
						// The pull-request scope carries the mode its
						// repository scope carries; the schema's foreign key
						// refuses any other pairing, so a mixed-authority row
						// cannot be written even by mistake here.
						|| ::sqlite3_bind_int(
							   insert->Value,
							   7,
							   access_mode_column(aRequest.Authority.Mode))
							!= SQLITE_OK) {
						return std::unexpected(failure::unavailable);
					}
					const auto insertCode = ::sqlite3_step(insert->Value);
					if (insertCode == SQLITE_DONE) {
						prScope = candidate;
					} else if ((static_cast<unsigned>(insertCode) & 0xFFU) != SQLITE_CONSTRAINT) {
						return std::unexpected(sql_failure(database, aStop));
					}
				}
				if (!prScope) {
					return std::unexpected(failure::authority_mismatch);
				}
			}
			auto scopeIntegrity = prepare(
				database,
				"SELECT p.catalog_revision, p.retained_epoch_count, p.lifecycle, "
				"r.lifecycle, p.retention_rehydration_required, "
				"(SELECT count(*) FROM epoch_anchor a "
				"WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
				"AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
				"FROM pull_request_scope p JOIN repository_scope r "
				"ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
				"WHERE p.incarnation = ?1 AND p.pr_scope = ?2 "
				"AND p.cache_scope = ?3");
			if (!scopeIntegrity
				|| !bind_fixed(scopeIntegrity->Value, 1, *incarnation)
				|| !bind_fixed(scopeIntegrity->Value, 2, *prScope)
				|| !bind_fixed(scopeIntegrity->Value, 3, *cacheScope)
				|| ::sqlite3_step(scopeIntegrity->Value) != SQLITE_ROW) {
				return std::unexpected(
					scopeIntegrity ? failure::repair_required
								   : scopeIntegrity.error());
			}
			const auto verifiedRevision            = ::sqlite3_column_int64(scopeIntegrity->Value, 0);
			const auto retained                    = ::sqlite3_column_int64(scopeIntegrity->Value, 1);
			const auto prLifecycle                 = ::sqlite3_column_int(scopeIntegrity->Value, 2);
			const auto verifiedRepositoryLifecycle = ::sqlite3_column_int(scopeIntegrity->Value, 3);
			const auto observedRetained            = ::sqlite3_column_int64(scopeIntegrity->Value, 5);
			const auto rehydrationRequired         = ::sqlite3_column_int(scopeIntegrity->Value, 4);
			const auto lifecycleCoherent           = (prLifecycle == 1
													  && (verifiedRepositoryLifecycle == 1
														  || verifiedRepositoryLifecycle == 2))
				|| (prLifecycle == 4
					&& (verifiedRepositoryLifecycle == 1
						|| verifiedRepositoryLifecycle == 2))
				|| (prLifecycle == 3 && verifiedRepositoryLifecycle == 3);
			if (verifiedRevision != catalogRevision || retained < 0
				|| retained != observedRetained
				|| retained
					> static_cast<std::int64_t>(
						app::maximum_retained_epoch_count_per_pull_request)
				|| verifiedRepositoryLifecycle != repositoryLifecycle
				|| !lifecycleCoherent
				|| (rehydrationRequired != 0 && rehydrationRequired != 1)
				|| (rehydrationRequired == 1 && prLifecycle != 1)
				|| ::sqlite3_step(scopeIntegrity->Value) != SQLITE_DONE) {
				return std::unexpected(failure::repair_required);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}

			app::epoch_catalog_binding binding{
				.Authority = core::pull_request_authority{
					.Profile         = aRequest.Authority.Profile,
					.ProfileRevision = aRequest.Authority.ProfileRevision,
					.Host            = aRequest.Authority.Host,
					.HostAuthority   = aRequest.Authority.HostAuthority,
					.Account         = aRequest.Authority.Account,
					.Viewer          = aRequest.Authority.Viewer,
					.Repository      = aRequest.Authority.Repository,
					.Cache           = *cacheScope,
					.PullRequest     = aRequest.Authority.PullRequest,
					.Number          = aRequest.Authority.Number,
					.Scope           = *prScope,
					// Without this the binding a public-preview key opens
					// would hand back an authenticated authority, and every
					// key derived from it - the Git cache scope, the
					// comparison digest - would land in the signed-in
					// namespace.
					.Mode = aRequest.Authority.Mode,
				},
				.Catalog = core::epoch_catalog_identity{
					.Incarnation = *incarnation,
					.Revision    = core::epoch_catalog_revision{ catalogRevision },
				},
				.ObjectFormat = aRequest.Authority.ObjectFormat,
			};
			return std::make_unique<sqlite_epoch_catalog_session>(
				State, binding, aRequest.ApplicationSession);
		}

		[[nodiscard]] auto load_epoch_summary(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority,
			const core::epoch_ordinal                    aOrdinal) -> result<core::review_epoch_summary>
		{
			auto query = prepare(
				aDatabase,
				"SELECT e.source_observation, e.source_snapshot, "
				"e.identity_generation, e.pull_request_generation, e.base_branch, "
				"e.observed_at, "
				"e.origin, e.base_transition, e.head_transition, "
				"e.material_relation, e.primary_cause, "
				"e.material_base_algorithm, e.material_base_digest, "
				"e.material_head_algorithm, e.material_head_digest, "
				"e.material_policy_revision, "
				"b.object_algorithm, b.object_digest, b.availability, "
				"h.object_algorithm, h.object_digest, h.availability "
				"FROM epoch e "
				"JOIN epoch_anchor b ON b.incarnation = e.incarnation "
				"AND b.pr_scope = e.pr_scope AND b.ordinal = e.ordinal "
				"AND b.endpoint = 1 "
				"JOIN epoch_anchor h ON h.incarnation = e.incarnation "
				"AND h.pr_scope = e.pr_scope AND h.ordinal = e.ordinal "
				"AND h.endpoint = 2 "
				"WHERE e.incarnation = ?1 AND e.pr_scope = ?2 "
				"AND e.ordinal = ?3");
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aAuthority)
				|| ::sqlite3_bind_int64(query->Value, 3, aOrdinal.value()) != SQLITE_OK) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			if (::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(failure::repair_required);
			}
			const auto observation        = read_fixed<core::epoch_source_observation_id>(query->Value, 0);
			const auto snapshot           = read_u64(query->Value, 1);
			const auto identityGeneration = read_u64(query->Value, 2);
			const auto prGeneration       = read_u64(query->Value, 3);
			const auto branch             = read_fixed<core::base_branch_id>(query->Value, 4);
			const auto base               = read_oid<core::git_commit_oid>(query->Value, 16, 17);
			const auto head               = read_oid<core::git_commit_oid>(query->Value, 19, 20);
			if (!observation || !snapshot || !identityGeneration || !prGeneration
				|| !branch || !base || !head
				|| ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 6) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 7) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 8) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 9) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 10) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 18) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 21) != SQLITE_INTEGER) {
				return std::unexpected(failure::schema_mismatch);
			}
			std::optional<core::epoch_material_identity> material;
			if (::sqlite3_column_type(query->Value, 11) != SQLITE_NULL) {
				const auto baseTree = read_oid<core::git_tree_oid>(query->Value, 11, 12);
				const auto headTree = read_oid<core::git_tree_oid>(query->Value, 13, 14);
				const auto policy   = read_u64(query->Value, 15);
				if (!baseTree || !headTree || !policy) {
					return std::unexpected(failure::schema_mismatch);
				}
				material = core::epoch_material_identity{
					.BaseTree       = *baseTree,
					.HeadTree       = *headTree,
					.PolicyRevision = core::comparison_policy_revision{ *policy },
				};
			}
			core::review_epoch_summary output{
				.Id = core::review_epoch_id{
					.Authority   = aAuthority,
					.Incarnation = aIncarnation,
					.Ordinal     = aOrdinal,
				},
				.Source = core::pull_request_source_revision{
					.Observation           = *observation,
					.Snapshot              = core::provider_snapshot_id{ *snapshot },
					.IdentityGeneration    = core::github_identity_generation{ *identityGeneration },
					.PullRequestGeneration = core::request_generation{ *prGeneration },
				},
				.BaseBranch = *branch,
				.Base       = *base,
				.Head       = *head,
				.Material   = material,
				.Cause      = core::epoch_cause{
					.Origin   = restored_enum<core::epoch_origin>(::sqlite3_column_int(query->Value, 6)),
					.Base     = restored_enum<core::epoch_base_transition>(::sqlite3_column_int(query->Value, 7)),
					.Head     = restored_enum<core::epoch_head_transition>(::sqlite3_column_int(query->Value, 8)),
					.Material = restored_enum<core::epoch_material_relation>(::sqlite3_column_int(query->Value, 9)),
					.Primary  = restored_enum<core::epoch_primary_cause>(::sqlite3_column_int(query->Value, 10)),
				},
				.ObservedAt       = core::observed_utc_milliseconds{ ::sqlite3_column_int64(query->Value, 5) },
				.BaseAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 18)),
				.HeadAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 21)),
			};
			if (::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}

		[[nodiscard]] auto linked_review_count(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const app::prune_epoch_fact&              aFact,
			const std::string_view                    aPredicate) -> result<std::int64_t>
		{
			auto query = prepare(
				aDatabase,
				std::string{
					"SELECT count(*) FROM review_association "
					"WHERE incarnation = ?1 AND pr_scope = ?2 "
					"AND linked_ordinal = ?3 AND commit_algorithm = ?4 "
					"AND commit_digest = ?5 AND " }
					+ std::string{ aPredicate });
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aFact.Epoch.Authority)
				|| ::sqlite3_bind_int64(
					   query->Value, 3, aFact.Epoch.Ordinal.value())
					!= SQLITE_OK
				|| !bind_oid(query->Value, 4, 5, aFact.Retained.Head)
				|| ::sqlite3_step(query->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			const auto count = ::sqlite3_column_int64(query->Value, 0);
			if (count < 0 || ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return count;
		}

		struct pull_request_scope_state final
		{
			core::epoch_catalog_revision    Revision;
			core::repository_cache_scope_id Cache;
			std::uint64_t                   RetainedEpochCount  = 0U;
			int                             Lifecycle           = 0;
			int                             RepositoryLifecycle = 0;
		};

		[[nodiscard]] auto load_pull_request_scope_state(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority)
			-> result<pull_request_scope_state>
		{
			auto query = prepare(
				aDatabase,
				"SELECT p.catalog_revision, p.cache_scope, p.retained_epoch_count, "
				"p.lifecycle, r.lifecycle, (SELECT count(*) FROM epoch_anchor a "
				"WHERE a.incarnation = p.incarnation AND a.pr_scope = p.pr_scope "
				"AND a.endpoint = 1 AND a.availability NOT IN (3, 4)) "
				"FROM pull_request_scope p JOIN repository_scope r "
				"ON r.incarnation = p.incarnation "
				"AND r.cache_scope = p.cache_scope "
				"WHERE p.incarnation = ?1 AND p.pr_scope = ?2");
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aAuthority)) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			if (::sqlite3_step(query->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 4) != SQLITE_INTEGER
				|| ::sqlite3_column_type(query->Value, 5) != SQLITE_INTEGER) {
				return std::unexpected(failure::authority_mismatch);
			}
			auto cache = read_fixed<core::repository_cache_scope_id>(query->Value, 1);
			if (!cache) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto               retained         = ::sqlite3_column_int64(query->Value, 2);
			const auto               observedRetained = ::sqlite3_column_int64(query->Value, 5);
			pull_request_scope_state output{
				.Revision = core::epoch_catalog_revision{
					::sqlite3_column_int64(query->Value, 0) },
				.Cache               = *cache,
				.RetainedEpochCount  = static_cast<std::uint64_t>(retained),
				.Lifecycle           = ::sqlite3_column_int(query->Value, 3),
				.RepositoryLifecycle = ::sqlite3_column_int(query->Value, 4),
			};
			if (retained < 0 || observedRetained < 0
				|| retained != observedRetained
				|| output.RetainedEpochCount
					> app::maximum_retained_epoch_count_per_pull_request
				|| !core::valid(output.Revision) || output.Lifecycle < 1
				|| output.Lifecycle > 4 || output.RepositoryLifecycle < 1
				|| output.RepositoryLifecycle > 4
				|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}

		[[nodiscard]] auto require_active_pull_request_scope(
			::sqlite3*                         aDatabase,
			const app::epoch_catalog_binding&  aBinding,
			const core::epoch_catalog_revision aExpected,
			const bool                         aRequireAdvanceCapacity  = false,
			const bool                         aRequireRetainedCapacity = false) -> result<void>
		{
			auto state = load_pull_request_scope_state(
				aDatabase,
				aBinding.Catalog.Incarnation,
				aBinding.Authority.Scope);
			if (!state) {
				return std::unexpected(state.error());
			}
			if (state->Cache != aBinding.Authority.Cache || state->Lifecycle != 1
				|| state->RepositoryLifecycle != 1) {
				return std::unexpected(failure::authority_mismatch);
			}
			if (state->Revision != aExpected
				|| (aRequireAdvanceCapacity
					&& state->Revision.value()
						>= core::maximum_epoch_catalog_revision)) {
				return std::unexpected(failure::revision_conflict);
			}
			if (
				aRequireRetainedCapacity
				&& state->RetainedEpochCount
					>= app::maximum_retained_epoch_count_per_pull_request) {
				return std::unexpected(failure::quota_exceeded);
			}
			return { };
		}

		[[nodiscard]] auto require_readable_pull_request_scope(
			const pull_request_scope_state&                   aState,
			const app::epoch_catalog_binding&                 aBinding,
			const std::optional<core::epoch_catalog_revision> aExpected = std::nullopt)
			-> result<void>
		{
			if (aState.Cache != aBinding.Authority.Cache) {
				return std::unexpected(failure::authority_mismatch);
			}
			if (aExpected && aState.Revision != *aExpected) {
				return std::unexpected(failure::revision_conflict);
			}
			if (
				(aState.Lifecycle == 1 && aState.RepositoryLifecycle == 1)
				|| (aState.Lifecycle == 3
					&& (aState.RepositoryLifecycle == 3
						|| aState.RepositoryLifecycle == 4))) {
				return { };
			}
			if (aState.Lifecycle == 4) {
				return std::unexpected(failure::repair_required);
			}
			if (aState.RepositoryLifecycle == 2) {
				return std::unexpected(failure::operation_conflict);
			}
			return std::unexpected(failure::repair_required);
		}

		[[nodiscard]] auto require_no_blocking_operation(
			::sqlite3*                        aDatabase,
			const app::epoch_catalog_binding& aBinding,
			const bool                        aWholeCache) -> result<void>
		{
			auto query = prepare(
				aDatabase,
				aWholeCache
					? "SELECT 1 FROM operation WHERE incarnation = ?1 "
					  "AND cache_scope = ?2 AND terminal = 0 "
					  "LIMIT 1"
					: "SELECT 1 FROM operation WHERE incarnation = ?1 "
					  "AND cache_scope = ?2 AND pr_scope = ?3 "
					  "AND terminal NOT IN (1, 2) LIMIT 1");
			if (!query || !bind_fixed(query->Value, 1, aBinding.Catalog.Incarnation)
				|| !bind_fixed(query->Value, 2, aBinding.Authority.Cache)
				|| (!aWholeCache
					&& !bind_fixed(query->Value, 3, aBinding.Authority.Scope))) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			const auto code = ::sqlite3_step(query->Value);
			if (code == SQLITE_ROW) {
				return std::unexpected(failure::operation_conflict);
			}
			if (code != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, { }));
			}
			return { };
		}

		[[nodiscard]] constexpr auto advanced_once(
			const core::epoch_catalog_revision aExpected,
			const core::epoch_catalog_revision aObserved) noexcept -> bool
		{
			return core::valid(aExpected) && core::valid(aObserved)
				&& aExpected.value() < core::maximum_epoch_catalog_revision
				&& aObserved.value() == aExpected.value() + 1;
		}

		[[nodiscard]] constexpr auto finalized_operation_revision_matches(
			const app::epoch_catalog_operation_phase    aPhase,
			const app::epoch_catalog_operation_revision aExpected,
			const app::epoch_catalog_operation_revision aObserved,
			const app::epoch_catalog_operation_phase    aPendingPhase) noexcept -> bool
		{
			if (aPhase == aPendingPhase) {
				return aObserved.value() == aExpected.value() + 1;
			}
			return aPhase == app::epoch_catalog_operation_phase::complete
				&& aExpected.value()
				< std::numeric_limits<std::int64_t>::max() - 1
				&& aObserved.value() == aExpected.value() + 2;
		}

		[[nodiscard]] auto load_review_association(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority,
			const core::github_viewer_actor_id           aViewer,
			const core::review_association_id&           aAssociation)
			-> result<core::catalog_review_association>
		{
			auto query = prepare(
				aDatabase,
				"SELECT review_id, actor_id, review_order, review_state, "
				"commit_algorithm, commit_digest, availability "
				"FROM review_association WHERE incarnation = ?1 "
				"AND pr_scope = ?2 AND association = ?3");
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aAuthority)
				|| !bind_fixed(query->Value, 3, aAssociation)) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			if (::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(failure::repair_required);
			}
			const auto review = read_u64(query->Value, 0);
			const auto actor  = read_u64(query->Value, 1);
			const auto commit = read_oid<core::git_commit_oid>(query->Value, 4, 5);
			if (!review || !actor || *actor != aViewer.value() || !commit) {
				return std::unexpected(failure::schema_mismatch);
			}
			core::catalog_review_association output{
				.Association = aAssociation,
				.Review      = core::github_review_id{ *review },
				.Actor       = core::github_viewer_actor_id{ *actor },
				.Order       = core::github_review_order{
					static_cast<std::uint16_t>(
						::sqlite3_column_int(query->Value, 2)) },
				.State        = restored_enum<core::github_review_state>(::sqlite3_column_int(query->Value, 3)),
				.Commit       = *commit,
				.Availability = restored_enum<core::anchor_availability>(::sqlite3_column_int(query->Value, 6)),
			};
			if (::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}

		struct canonical_review_fact final
		{
			core::catalog_review_association   Review;
			std::optional<core::epoch_ordinal> LinkedOrdinal;
		};

		[[nodiscard]] auto canonicalize_review(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority,
			const core::catalog_review_association&      aReview,
			const std::optional<std::pair<core::epoch_ordinal, core::git_commit_oid>>&
				aProjectedHead = std::nullopt) -> result<canonical_review_fact>
		{
			canonical_review_fact output{ .Review = aReview };
			if (aProjectedHead && aReview.Commit == aProjectedHead->second) {
				output.Review.Availability = core::anchor_availability::available;
				output.LinkedOrdinal       = aProjectedHead->first;
				return output;
			}
			auto matching = prepare(
				aDatabase,
				"SELECT ordinal, availability FROM epoch_anchor "
				"WHERE incarnation = ?1 AND pr_scope = ?2 AND endpoint = 2 "
				"AND object_algorithm = ?3 AND object_digest = ?4 "
				"ORDER BY ordinal DESC LIMIT 1");
			if (!matching || !bind_fixed(matching->Value, 1, aIncarnation)
				|| !bind_fixed(matching->Value, 2, aAuthority)
				|| !bind_oid(matching->Value, 3, 4, aReview.Commit)) {
				return std::unexpected(matching ? failure::unavailable
												: matching.error());
			}
			const auto code = ::sqlite3_step(matching->Value);
			if (code == SQLITE_ROW) {
				output.LinkedOrdinal = core::epoch_ordinal{
					::sqlite3_column_int64(matching->Value, 0)
				};
				output.Review.Availability = restored_enum<core::anchor_availability>(
					::sqlite3_column_int(matching->Value, 1));
				if (!core::valid(*output.LinkedOrdinal)
					|| ::sqlite3_column_type(matching->Value, 1) != SQLITE_INTEGER
					|| ::sqlite3_step(matching->Value) != SQLITE_DONE
					|| !app::valid(output.Review)) {
					return std::unexpected(failure::schema_mismatch);
				}
				return output;
			}
			if (code != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, { }));
			}
			if (aReview.Availability != core::anchor_availability::unavailable
				&& aReview.Availability
					!= core::anchor_availability::never_observed) {
				return std::unexpected(failure::operation_conflict);
			}
			return output;
		}

		[[nodiscard]] auto catalog_contains_source_observation(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const core::epoch_source_observation_id&  aObservation,
			std::stop_token                           aStop) -> result<bool>
		{
			auto query = prepare(
				aDatabase,
				"SELECT EXISTS(SELECT 1 FROM epoch "
				"INDEXED BY epoch_source_revision "
				"WHERE incarnation = ?1 AND source_observation = ?2) "
				"OR EXISTS(SELECT 1 FROM operation "
				"INDEXED BY operation_source_observation "
				"WHERE incarnation = ?3 AND kind = 1 "
				"AND source_observation = ?4)");
			if (!query || !bind_fixed(query->Value, 1, aIncarnation)
				|| !bind_fixed(query->Value, 2, aObservation)
				|| !bind_fixed(query->Value, 3, aIncarnation)
				|| !bind_fixed(query->Value, 4, aObservation)) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			if (::sqlite3_step(query->Value) != SQLITE_ROW
				|| ::sqlite3_column_type(query->Value, 0) != SQLITE_INTEGER) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			const auto exists = ::sqlite3_column_int(query->Value, 0);
			if ((exists != 0 && exists != 1)
				|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			return exists != 0;
		}

		auto sqlite_epoch_catalog_session::snapshot(std::stop_token aStop)
			-> result<app::epoch_catalog_snapshot>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto state = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			if (!state) {
				return std::unexpected(state.error());
			}
			if (auto readable = require_readable_pull_request_scope(
					*state, Binding);
				!readable) {
				return std::unexpected(readable.error());
			}
			auto query = prepare(
				database,
				"SELECT catalog_revision, latest_live_ordinal, "
				"previous_distinct_head_ordinal, retention_rehydration_required, "
				"next_ordinal "
				"FROM pull_request_scope "
				"WHERE incarnation = ?1 AND pr_scope = ?2");
			if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(
					query ? sql_failure(database, aStop, failure::authority_mismatch)
						  : query.error());
			}
			const auto                         revision = ::sqlite3_column_int64(query->Value, 0);
			std::optional<core::epoch_ordinal> latest;
			std::optional<core::epoch_ordinal> previous;
			if (::sqlite3_column_type(query->Value, 1) == SQLITE_INTEGER) {
				latest = core::epoch_ordinal{
					::sqlite3_column_int64(query->Value, 1)
				};
			}
			if (::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER) {
				previous = core::epoch_ordinal{
					::sqlite3_column_int64(query->Value, 2)
				};
			}
			const auto                         rehydrationRequired = ::sqlite3_column_int(query->Value, 3);
			std::optional<core::epoch_ordinal> nextOrdinal;
			const auto                         nextOrdinalType = ::sqlite3_column_type(query->Value, 4);
			if (nextOrdinalType == SQLITE_INTEGER) {
				nextOrdinal = core::epoch_ordinal{
					::sqlite3_column_int64(query->Value, 4)
				};
			}
			if (revision < 0
				|| (rehydrationRequired != 0 && rehydrationRequired != 1)
				|| (latest && !core::valid(*latest))
				|| (previous && !core::valid(*previous))
				|| (nextOrdinalType != SQLITE_NULL
					&& (nextOrdinalType != SQLITE_INTEGER
						|| !nextOrdinal || !core::valid(*nextOrdinal)))
				|| (latest && nextOrdinal && latest->value() >= nextOrdinal->value())
				|| (previous && nextOrdinal
					&& previous->value() >= nextOrdinal->value())
				|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto snapshotBinding             = Binding;
			snapshotBinding.Catalog.Revision = core::epoch_catalog_revision{ revision };
			app::epoch_catalog_snapshot output{
				.Binding              = snapshotBinding,
				.NextOrdinal          = nextOrdinal,
				.RetentionRehydration = rehydrationRequired != 0
					? core::retention_rehydration_requirement::required
					: core::retention_rehydration_requirement::not_required,
			};
			if (latest) {
				auto loaded = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*latest);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.LatestLive = *loaded;
			}
			if (rehydrationRequired != 0
				&& (!output.LatestLive
					|| output.LatestLive->BaseAvailability
						!= core::anchor_availability::cache_deleted
					|| output.LatestLive->HeadAvailability
						!= core::anchor_availability::cache_deleted)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (previous) {
				auto loaded = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*previous);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.PreviousDistinctHead = *loaded;
			}
			auto current = prepare(
				database,
				"SELECT association FROM current_viewer_review "
				"WHERE incarnation = ?1 AND pr_scope = ?2");
			if (!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(current->Value, 2, Binding.Authority.Scope)) {
				return std::unexpected(current ? failure::unavailable : current.error());
			}
			const auto code = ::sqlite3_step(current->Value);
			if (code == SQLITE_ROW) {
				auto association = read_fixed<core::review_association_id>(
					current->Value, 0);
				if (!association || ::sqlite3_step(current->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				auto loaded = load_review_association(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					Binding.Authority.Viewer,
					*association);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.CurrentViewerReview = *loaded;
			} else if (code != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			Binding = snapshotBinding;
			return output;
		}

		auto sqlite_epoch_catalog_session::source_observation_exists(
			const core::epoch_source_observation_id aObservation,
			std::stop_token                         aStop) -> result<bool>
		{
			if (!aObservation.valid()) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto exists = catalog_contains_source_observation(
				database,
				Binding.Catalog.Incarnation,
				aObservation,
				aStop);
			if (!exists) {
				return std::unexpected(exists.error());
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return *exists;
		}

		auto sqlite_epoch_catalog_session::canonical_review(
			const app::canonical_review_request& aRequest,
			std::stop_token                      aStop) -> result<core::catalog_review_association>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
				|| aRequest.Candidate.Actor != Binding.Authority.Viewer
				|| aRequest.Candidate.Commit.algorithm() != Binding.ObjectFormat
				|| (aRequest.ProjectedHead
					&& aRequest.ProjectedHead->Head.algorithm()
						!= Binding.ObjectFormat)) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto state = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			if (!state) {
				return std::unexpected(state.error());
			}
			if (auto readable = require_readable_pull_request_scope(
					*state, Binding, aRequest.Catalog.Revision);
				!readable) {
				return std::unexpected(readable.error());
			}

			auto existing = prepare(
				database,
				"SELECT association FROM review_association "
				"WHERE incarnation = ?1 AND pr_scope = ?2 AND review_id = ?3");
			if (!existing
				|| !bind_fixed(
					existing->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(existing->Value, 2, Binding.Authority.Scope)
				|| !bind_u64(existing->Value, 3, aRequest.Candidate.Review.value())) {
				return std::unexpected(
					existing ? failure::unavailable : existing.error());
			}
			std::optional<core::review_association_id> association;
			const auto                                 existingCode = ::sqlite3_step(existing->Value);
			if (existingCode == SQLITE_ROW) {
				association = read_fixed<core::review_association_id>(
					existing->Value, 0);
				if (!association || ::sqlite3_step(existing->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				auto loaded = load_review_association(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					Binding.Authority.Viewer,
					*association);
				if (!loaded || loaded->Review != aRequest.Candidate.Review) {
					return std::unexpected(
						loaded ? failure::schema_mismatch : loaded.error());
				}
			} else if (existingCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}

			if (!association) {
				auto identityCollision = prepare(
					database,
					"SELECT 1 FROM review_association WHERE incarnation = ?1 "
					"AND pr_scope = ?2 AND association = ?3 LIMIT 1");
				if (!identityCollision
					|| !bind_fixed(
						identityCollision->Value,
						1,
						Binding.Catalog.Incarnation)
					|| !bind_fixed(
						identityCollision->Value, 2, Binding.Authority.Scope)
					|| !bind_fixed(
						identityCollision->Value,
						3,
						aRequest.ProposedAssociation)) {
					return std::unexpected(
						identityCollision ? failure::unavailable
										  : identityCollision.error());
				}
				const auto collisionCode = ::sqlite3_step(identityCollision->Value);
				if (collisionCode == SQLITE_ROW) {
					return std::unexpected(failure::operation_conflict);
				}
				if (collisionCode != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
				association = aRequest.ProposedAssociation;
			}

			auto orderCollision = prepare(
				database,
				"SELECT 1 FROM review_association WHERE incarnation = ?1 "
				"AND pr_scope = ?2 AND review_order = ?3 AND review_id != ?4 "
				"LIMIT 1");
			if (!orderCollision
				|| !bind_fixed(
					orderCollision->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(orderCollision->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int(
					   orderCollision->Value, 3, aRequest.Candidate.Order.value())
					!= SQLITE_OK
				|| !bind_u64(
					orderCollision->Value, 4, aRequest.Candidate.Review.value())) {
				return std::unexpected(
					orderCollision ? failure::unavailable
								   : orderCollision.error());
			}
			const auto orderCode = ::sqlite3_step(orderCollision->Value);
			if (orderCode == SQLITE_ROW) {
				return std::unexpected(failure::operation_conflict);
			}
			if (orderCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}

			auto candidate = core::catalog_review_association{
				.Association  = *association,
				.Review       = aRequest.Candidate.Review,
				.Actor        = aRequest.Candidate.Actor,
				.Order        = aRequest.Candidate.Order,
				.State        = aRequest.Candidate.State,
				.Commit       = aRequest.Candidate.Commit,
				.Availability = core::anchor_availability::never_observed,
			};
			std::optional<std::pair<core::epoch_ordinal, core::git_commit_oid>>
				projected;
			if (aRequest.ProjectedHead) {
				projected = std::pair{
					aRequest.ProjectedHead->Ordinal,
					aRequest.ProjectedHead->Head
				};
			}
			auto canonical = canonicalize_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				candidate,
				projected);
			if (!canonical) {
				return std::unexpected(canonical.error());
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return canonical->Review;
		}

		auto sqlite_epoch_catalog_session::exact_anchors(
			const app::exact_anchor_lookup_request& aRequest,
			std::stop_token                         aStop) -> result<app::exact_anchor_lookup_result>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto scope = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			if (!scope) {
				return std::unexpected(scope.error());
			}
			if (auto readable = require_readable_pull_request_scope(
					*scope, Binding, aRequest.Catalog.Revision);
				!readable) {
				return std::unexpected(readable.error());
			}
			app::exact_anchor_lookup_result output{ .Catalog = aRequest.Catalog };
			output.Anchors.reserve(aRequest.Anchors.size());
			for (const auto& identity : aRequest.Anchors) {
				if (identity.Authority != Binding.Authority.Scope) {
					return std::unexpected(failure::authority_mismatch);
				}
				core::comparison_anchor anchor{ .Identity = identity };
				if (identity.Kind == core::comparison_anchor_kind::review_commit) {
					auto query = prepare(
						database,
						"SELECT actor_id, commit_algorithm, commit_digest, "
						"availability "
						"FROM review_association WHERE incarnation = ?1 "
						"AND pr_scope = ?2 AND association = ?3");
					if (!query
						|| !bind_fixed(
							query->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
						|| !bind_fixed(query->Value, 3, identity.Review)
						|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
						return std::unexpected(failure::authority_mismatch);
					}
					const auto actor        = read_u64(query->Value, 0);
					auto       commit       = read_oid<core::git_commit_oid>(query->Value, 1, 2);
					const auto availability = ::sqlite3_column_int(query->Value, 3);
					if (!actor || *actor != Binding.Authority.Viewer.value() || !commit
						|| ::sqlite3_column_type(query->Value, 3) != SQLITE_INTEGER
						|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
						return std::unexpected(failure::schema_mismatch);
					}
					anchor.Commit       = *commit;
					anchor.Availability = restored_enum<core::anchor_availability>(
						availability);
				} else {
					auto query = prepare(
						database,
						"SELECT object_algorithm, object_digest, availability "
						"FROM epoch_anchor WHERE incarnation = ?1 "
						"AND pr_scope = ?2 AND ordinal = ?3 AND endpoint = ?4");
					const auto endpoint = identity.Kind == core::comparison_anchor_kind::epoch_base
						? 1
						: 2;
					if (!query
						|| !bind_fixed(
							query->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_fixed(query->Value, 2, Binding.Authority.Scope)
						|| ::sqlite3_bind_int64(
							   query->Value, 3, identity.Epoch.value())
							!= SQLITE_OK
						|| ::sqlite3_bind_int(query->Value, 4, endpoint) != SQLITE_OK
						|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
						return std::unexpected(failure::authority_mismatch);
					}
					auto       commit       = read_oid<core::git_commit_oid>(query->Value, 0, 1);
					const auto availability = ::sqlite3_column_int(query->Value, 2);
					if (!commit
						|| ::sqlite3_column_type(query->Value, 2) != SQLITE_INTEGER
						|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
						return std::unexpected(failure::schema_mismatch);
					}
					anchor.Commit       = *commit;
					anchor.Availability = restored_enum<core::anchor_availability>(
						availability);
				}
				output.Anchors.push_back(anchor);
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::mark_anchor_availability(
			const app::mark_anchor_availability_request& aRequest,
			std::stop_token                              aStop) -> result<app::catalog_commit_result>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision, true);
				!active) {
				return std::unexpected(active.error());
			}
			if (auto idle = require_no_blocking_operation(database, Binding, false);
				!idle) {
				return std::unexpected(idle.error());
			}
			for (const auto& change : aRequest.Changes) {
				if (change.Anchor.Authority != Binding.Authority.Scope
					|| change.ExpectedCommit.algorithm() != Binding.ObjectFormat) {
					return std::unexpected(failure::authority_mismatch);
				}
				result<statement> update = std::unexpected(failure::unavailable);
				if (change.Anchor.Kind == core::comparison_anchor_kind::review_commit) {
					update = prepare(
						database,
						"UPDATE review_association SET availability = ?1 "
						"WHERE incarnation = ?2 AND pr_scope = ?3 "
						"AND association = ?4 AND commit_algorithm = ?5 "
						"AND commit_digest = ?6 AND availability = ?7");
					if (update
						&& (::sqlite3_bind_int(
								update->Value, 1, stored_enum(change.Next))
								!= SQLITE_OK
							|| !bind_fixed(
								update->Value, 2, Binding.Catalog.Incarnation)
							|| !bind_fixed(
								update->Value, 3, Binding.Authority.Scope)
							|| !bind_fixed(update->Value, 4, change.Anchor.Review)
							|| !bind_oid(
								update->Value, 5, 6, change.ExpectedCommit)
							|| ::sqlite3_bind_int(
								   update->Value, 7, stored_enum(change.Expected))
								!= SQLITE_OK)) {
						return std::unexpected(failure::unavailable);
					}
				} else {
					update = prepare(
						database,
						"UPDATE epoch_anchor SET availability = ?1 "
						"WHERE incarnation = ?2 AND pr_scope = ?3 "
						"AND ordinal = ?4 AND endpoint = ?5 "
						"AND object_algorithm = ?6 AND object_digest = ?7 "
						"AND availability = ?8");
					const auto endpoint = change.Anchor.Kind
							== core::comparison_anchor_kind::epoch_base
						? 1
						: 2;
					if (update
						&& (::sqlite3_bind_int(
								update->Value, 1, stored_enum(change.Next))
								!= SQLITE_OK
							|| !bind_fixed(
								update->Value, 2, Binding.Catalog.Incarnation)
							|| !bind_fixed(
								update->Value, 3, Binding.Authority.Scope)
							|| ::sqlite3_bind_int64(
								   update->Value, 4, change.Anchor.Epoch.value())
								!= SQLITE_OK
							|| ::sqlite3_bind_int(update->Value, 5, endpoint)
								!= SQLITE_OK
							|| !bind_oid(
								update->Value, 6, 7, change.ExpectedCommit)
							|| ::sqlite3_bind_int(
								   update->Value, 8, stored_enum(change.Expected))
								!= SQLITE_OK)) {
						return std::unexpected(failure::unavailable);
					}
				}
				if (!update || ::sqlite3_step(update->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::revision_conflict);
				}
			}
			auto revision = prepare(
				database,
				"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND catalog_revision = ?3 "
				"AND catalog_revision < 9223372036854775807");
			if (!revision
				|| !bind_fixed(revision->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(revision->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   revision->Value, 3, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(revision->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			Binding.Catalog.Revision = core::epoch_catalog_revision{
				aRequest.Catalog.Revision.value() + 1
			};
			return app::catalog_commit_result{
				.Catalog = Binding.Catalog,
				.Changed = true,
			};
		}

		auto sqlite_epoch_catalog_session::history(
			const app::epoch_history_page_request& aRequest,
			std::stop_token                        aStop) -> result<app::epoch_history_page>
		{
			if (!app::valid(Binding, aRequest)) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto state = load_pull_request_scope_state(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope);
			if (!state) {
				return std::unexpected(state.error());
			}
			if (auto readable = require_readable_pull_request_scope(
					*state, Binding, aRequest.Catalog.Revision);
				!readable) {
				return std::unexpected(readable.error());
			}
			auto ordinals = prepare(
				database,
				aRequest.Cursor
					? "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
					  "AND pr_scope = ?2 AND ordinal < ?3 "
					  "ORDER BY ordinal DESC LIMIT ?4"
					: "SELECT ordinal FROM epoch WHERE incarnation = ?1 "
					  "AND pr_scope = ?2 ORDER BY ordinal DESC LIMIT ?3");
			if (!ordinals
				|| !bind_fixed(ordinals->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(ordinals->Value, 2, Binding.Authority.Scope)) {
				return std::unexpected(ordinals ? failure::unavailable : ordinals.error());
			}
			const auto fetchLimit = static_cast<std::int64_t>(aRequest.Limit) + 1;
			if (aRequest.Cursor) {
				if (::sqlite3_bind_int64(
						ordinals->Value, 3, aRequest.Cursor->Before.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int64(ordinals->Value, 4, fetchLimit)
						!= SQLITE_OK) {
					return std::unexpected(failure::unavailable);
				}
			} else if (::sqlite3_bind_int64(ordinals->Value, 3, fetchLimit) != SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
			std::vector<core::epoch_ordinal> values;
			while (true) {
				const auto code = ::sqlite3_step(ordinals->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW || aStop.stop_requested()) {
					return std::unexpected(sql_failure(database, aStop));
				}
				values.emplace_back(::sqlite3_column_int64(ordinals->Value, 0));
			}
			const auto hasMore = values.size() > aRequest.Limit;
			if (hasMore) {
				values.resize(aRequest.Limit);
			}
			app::epoch_history_page output{
				.Authority = Binding.Authority.Scope,
				.Catalog   = aRequest.Catalog,
			};
			output.Epochs.reserve(values.size());
			for (const auto ordinal : values) {
				auto loaded = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					ordinal);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				output.Epochs.push_back(*loaded);
			}
			if (hasMore && !values.empty()) {
				output.Continuation = app::epoch_history_cursor{
					.Authority   = Binding.Authority.Scope,
					.Incarnation = Binding.Catalog.Incarnation,
					.Revision    = aRequest.Catalog.Revision,
					.Before      = values.back(),
				};
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		[[nodiscard]] constexpr auto coherent_candidate_review(
			const std::optional<core::catalog_review_association>&      aCandidate,
			const std::optional<core::current_viewer_review_candidate>& aSource,
			core::github_viewer_actor_id                                aViewer) noexcept -> bool;

		[[nodiscard]] auto coherent_install_facts(
			const app::durable_git_spool_fact&     aSpool,
			const app::durable_git_preimport_fact& aPreimport,
			const app::durable_git_install_fact&   aInstall) noexcept -> bool
		{
			if (aInstall.Pack != aSpool.ExpectedPack
				|| aInstall.InstalledPack.Bytes != aSpool.Spool.Bytes) {
				return false;
			}
			if (!aPreimport.ExpectedPackPreexisting) {
				return true;
			}
			const auto expected = std::ranges::find_if(
				aPreimport.Packs, [&aSpool](const auto& aEntry) {
					return aEntry.Pack == aSpool.ExpectedPack;
				});
			return expected != aPreimport.Packs.end()
				&& aInstall.InstalledPack.Bytes == expected->PackBytes
				&& aInstall.InstalledIndex.Bytes == expected->IndexBytes;
		}

		auto sqlite_epoch_catalog_session::load_append(
			const core::epoch_operation_id aOperation)
			-> result<app::append_operation_record>
		{
			auto* database = State->Database.handle();
			auto  query    = prepare(
				database,
				"SELECT o.phase, o.operation_revision, o.repair_from_phase, "
				"o.recovery_input_compacted, o.application_session, "
				"o.request_id, o.epoch_generation, o.source_observation, "
				"o.source_snapshot, o.identity_generation, "
				"o.pull_request_generation, "
				"o.expected_revision, o.reserved_ordinal, "
				"a.base_branch, a.revision_origin, a.base_algorithm, a.base_digest, "
				"a.head_algorithm, a.head_digest, a.source_review_present "
				"FROM operation o JOIN append_operation a "
				"ON a.incarnation = o.incarnation AND a.operation = o.operation "
				"WHERE o.incarnation = ?1 AND o.operation = ?2 AND o.kind = 1 "
				"AND o.cache_scope = ?3 AND o.pr_scope = ?4");
			if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(query->Value, 2, aOperation)
				|| !bind_fixed(query->Value, 3, Binding.Authority.Cache)
				|| !bind_fixed(query->Value, 4, Binding.Authority.Scope)
				|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(
					query ? failure::operation_conflict : query.error());
			}
			const auto applicationSession = read_u64(query->Value, 4);
			const auto request            = read_u64(query->Value, 5);
			const auto generation         = read_u64(query->Value, 6);
			const auto observation        = read_fixed<core::epoch_source_observation_id>(query->Value, 7);
			const auto source             = read_u64(query->Value, 8);
			const auto identityGeneration = read_u64(query->Value, 9);
			const auto prGeneration       = read_u64(query->Value, 10);
			const auto branch             = read_fixed<core::base_branch_id>(query->Value, 13);
			const auto base               = read_oid<core::git_commit_oid>(query->Value, 15, 16);
			const auto head               = read_oid<core::git_commit_oid>(query->Value, 17, 18);
			if (!applicationSession || !request || !generation || !observation
				|| !source || !identityGeneration || !prGeneration || !branch
				|| !base || !head) {
				return std::unexpected(failure::schema_mismatch);
			}
			core::accepted_pull_request_revision accepted{
				.Source = core::pull_request_source_revision{
					.Observation           = *observation,
					.Snapshot              = core::provider_snapshot_id{ *source },
					.IdentityGeneration    = core::github_identity_generation{ *identityGeneration },
					.PullRequestGeneration = core::request_generation{ *prGeneration },
				},
				.Origin     = restored_enum<core::pull_request_revision_origin>(::sqlite3_column_int(query->Value, 14)),
				.BaseBranch = *branch,
				.Base       = *base,
				.Head       = *head,
			};
			const auto phase = static_cast<app::epoch_catalog_operation_phase>(
				::sqlite3_column_int(query->Value, 0));
			const auto operationRevision = app::epoch_catalog_operation_revision{
				::sqlite3_column_int64(query->Value, 1)
			};
			std::optional<app::epoch_catalog_operation_phase> repairFromPhase;
			if (::sqlite3_column_type(query->Value, 2) == SQLITE_INTEGER) {
				repairFromPhase = static_cast<app::epoch_catalog_operation_phase>(
					::sqlite3_column_int(query->Value, 2));
			} else if (::sqlite3_column_type(query->Value, 2) != SQLITE_NULL) {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto recoveryInputCompacted = ::sqlite3_column_int(query->Value, 3);
			const auto catalogRevision        = ::sqlite3_column_int64(query->Value, 11);
			const auto ordinal                = core::epoch_ordinal{
				::sqlite3_column_int64(query->Value, 12)
			};
			const auto sourceReviewPresent = ::sqlite3_column_int(query->Value, 19);
			if (!app::valid(operationRevision) || catalogRevision < 0
				|| !core::valid(ordinal)
				|| (recoveryInputCompacted != 0 && recoveryInputCompacted != 1)
				|| (sourceReviewPresent != 0 && sourceReviewPresent != 1)
				|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			auto sourceReview = prepare(
				database,
				"SELECT review_id, actor_id, review_order, review_state, "
				"commit_algorithm, commit_digest FROM append_source_review "
				"WHERE incarnation = ?1 AND operation = ?2");
			if (!sourceReview
				|| !bind_fixed(
					sourceReview->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(sourceReview->Value, 2, aOperation)) {
				return std::unexpected(
					sourceReview ? failure::unavailable : sourceReview.error());
			}
			auto sourceReviewCode = ::sqlite3_step(sourceReview->Value);
			if ((sourceReviewCode == SQLITE_ROW) != (sourceReviewPresent != 0)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (sourceReviewCode == SQLITE_ROW) {
				const auto review = read_u64(sourceReview->Value, 0);
				const auto actor  = read_u64(sourceReview->Value, 1);
				const auto commit = read_oid<core::git_commit_oid>(
					sourceReview->Value, 4, 5);
				const auto order = ::sqlite3_column_int(sourceReview->Value, 2);
				const auto state = ::sqlite3_column_int(sourceReview->Value, 3);
				if (!review || !actor
					|| *actor != Binding.Authority.Viewer.value() || !commit
					|| ::sqlite3_step(sourceReview->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				accepted.CurrentViewerReview = core::current_viewer_review_candidate{
					.Review = core::github_review_id{ *review },
					.Actor  = core::github_viewer_actor_id{ *actor },
					.Order  = core::github_review_order{
						static_cast<std::uint16_t>(order) },
					.State  = restored_enum<core::github_review_state>(state),
					.Commit = *commit,
				};
			} else if (sourceReviewCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			app::append_operation_record output{
				.Context = app::append_operation_context{
					.ApplicationSession = core::application_session_id{ *applicationSession },
					.Request            = core::epoch_request_id{ *request },
					.Generation         = core::epoch_generation{ *generation },
					.Operation          = aOperation,
					.Revision           = accepted,
					.Catalog            = core::epoch_catalog_identity{
						.Incarnation = Binding.Catalog.Incarnation,
						.Revision    = core::epoch_catalog_revision{ catalogRevision },
					},
				},
				.ReservedOrdinal        = ordinal,
				.OperationRevision      = operationRevision,
				.Phase                  = phase,
				.RepairFromPhase        = repairFromPhase,
				.RecoveryInputCompacted = recoveryInputCompacted != 0,
				.StagingBase            = app::epoch_catalog_ref_identity{
					.Authority = Binding.Authority.Scope,
					.Ordinal   = ordinal,
					.Operation = aOperation,
					.Staging   = true,
					.Head      = false,
				},
				.StagingHead = app::epoch_catalog_ref_identity{
					.Authority = Binding.Authority.Scope,
					.Ordinal   = ordinal,
					.Operation = aOperation,
					.Staging   = true,
					.Head      = true,
				},
				.FinalBase = app::epoch_catalog_ref_identity{
					.Authority = Binding.Authority.Scope,
					.Ordinal   = ordinal,
				},
				.FinalHead = app::epoch_catalog_ref_identity{
					.Authority = Binding.Authority.Scope,
					.Ordinal   = ordinal,
					.Head      = true,
				},
			};
			auto receipt = prepare(
				database,
				"SELECT spool_identity, spool_bytes, expected_pack_algorithm, "
				"expected_pack_digest, observed_pack_algorithm, "
				"observed_pack_digest, repository_bytes, "
				"expected_pack_preexisting, inventory_count FROM append_receipt "
				"WHERE incarnation = ?1 AND operation = ?2");
			if (!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(receipt->Value, 2, aOperation)) {
				return std::unexpected(receipt ? failure::unavailable : receipt.error());
			}
			auto receiptCode = ::sqlite3_step(receipt->Value);
			if (receiptCode == SQLITE_ROW) {
				std::array<std::uint8_t, 32> spoolIdentity{ };
				const auto                   expected = read_pack(receipt->Value, 2, 3);
				const auto                   observed = read_pack(receipt->Value, 4, 5);
				if (!read_blob(receipt->Value, 0, spoolIdentity) || !expected
					|| !observed) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Spool = app::durable_git_spool_fact{
					.Operation    = aOperation,
					.ExpectedPack = *expected,
					.ObservedPack = *observed,
					.Spool        = app::git_artifact_fact{
						.Role     = app::git_artifact_role::completed_spool,
						.Identity = app::git_artifact_identity{ spoolIdentity },
						.Bytes    = static_cast<std::uint64_t>(
							::sqlite3_column_int64(receipt->Value, 1)),
					},
				};
				app::durable_git_preimport_fact preimport{
					.Operation       = aOperation,
					.ExpectedPack    = *expected,
					.RepositoryBytes = static_cast<std::uint64_t>(
						::sqlite3_column_int64(receipt->Value, 6)),
					.ExpectedPackPreexisting = ::sqlite3_column_int(receipt->Value, 7) != 0,
				};
				const auto expectedInventoryCount = ::sqlite3_column_int64(receipt->Value, 8);
				if (expectedInventoryCount < 0
					|| expectedInventoryCount
						> static_cast<std::int64_t>(
							app::maximum_git_cache_pack_count)) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (::sqlite3_step(receipt->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				auto inventory = prepare(
					database,
					"SELECT position, pack_algorithm, pack_digest, pack_bytes, "
					"index_bytes "
					"FROM append_inventory WHERE incarnation = ?1 "
					"AND operation = ?2 ORDER BY position LIMIT 4097");
				if (!inventory
					|| !bind_fixed(
						inventory->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(inventory->Value, 2, aOperation)) {
					return std::unexpected(
						inventory ? failure::unavailable : inventory.error());
				}
				while (true) {
					const auto inventoryCode = ::sqlite3_step(inventory->Value);
					if (inventoryCode == SQLITE_DONE) {
						break;
					}
					if (inventoryCode != SQLITE_ROW) {
						return std::unexpected(sql_failure(database, { }));
					}
					const auto position = ::sqlite3_column_int64(inventory->Value, 0);
					const auto pack     = read_pack(inventory->Value, 1, 2);
					if (!pack
						|| position
							!= static_cast<std::int64_t>(preimport.Packs.size())) {
						return std::unexpected(failure::schema_mismatch);
					}
					if (preimport.Packs.size() >= app::maximum_git_cache_pack_count) {
						return std::unexpected(failure::schema_mismatch);
					}
					preimport.Packs.push_back(app::git_pack_inventory_entry{
						.Pack      = *pack,
						.PackBytes = static_cast<std::uint64_t>(
							::sqlite3_column_int64(inventory->Value, 3)),
						.IndexBytes = static_cast<std::uint64_t>(
							::sqlite3_column_int64(inventory->Value, 4)),
					});
				}
				if (preimport.Packs.size()
					!= static_cast<std::size_t>(expectedInventoryCount)) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Preimport = std::move(preimport);
			} else if (receiptCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto install = prepare(
				database,
				"SELECT i.pack_algorithm, i.pack_digest, i.base_algorithm, "
				"i.base_digest, i.base_tree_algorithm, i.base_tree_digest, "
				"i.head_algorithm, i.head_digest, i.head_tree_algorithm, "
				"i.head_tree_digest, i.head_lineage, p.artifact_identity, "
				"p.artifact_bytes, x.artifact_identity, x.artifact_bytes "
				"FROM append_install i "
				"JOIN append_installed_artifact p ON p.incarnation = i.incarnation "
				"AND p.operation = i.operation AND p.artifact_role = 2 "
				"JOIN append_installed_artifact x ON x.incarnation = i.incarnation "
				"AND x.operation = i.operation AND x.artifact_role = 3 "
				"WHERE i.incarnation = ?1 AND i.operation = ?2");
			if (!install || !bind_fixed(install->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(install->Value, 2, aOperation)) {
				return std::unexpected(install ? failure::unavailable : install.error());
			}
			const auto installCode = ::sqlite3_step(install->Value);
			if (installCode == SQLITE_ROW) {
				const auto                   pack         = read_pack(install->Value, 0, 1);
				const auto                   installBase  = read_oid<core::git_commit_oid>(install->Value, 2, 3);
				const auto                   baseTree     = read_oid<core::git_tree_oid>(install->Value, 4, 5);
				const auto                   installHead  = read_oid<core::git_commit_oid>(install->Value, 6, 7);
				const auto                   headTree     = read_oid<core::git_tree_oid>(install->Value, 8, 9);
				const auto                   lineageValue = ::sqlite3_column_int(install->Value, 10);
				std::array<std::uint8_t, 32> packIdentity{ };
				std::array<std::uint8_t, 32> indexIdentity{ };
				const auto                   packBytes  = ::sqlite3_column_int64(install->Value, 12);
				const auto                   indexBytes = ::sqlite3_column_int64(install->Value, 14);
				if (!pack || !installBase || !baseTree || !installHead || !headTree
					|| ::sqlite3_column_type(install->Value, 10) != SQLITE_INTEGER
					|| lineageValue
						< stored_enum(core::head_lineage::not_applicable)
					|| lineageValue > stored_enum(core::head_lineage::unknown)
					|| !read_blob(install->Value, 11, packIdentity)
					|| !read_blob(install->Value, 13, indexIdentity)
					|| ::sqlite3_step(install->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto lineage = restored_enum<core::head_lineage>(lineageValue);
				output.Install     = app::durable_git_install_fact{
					.Operation     = aOperation,
					.Pack          = *pack,
					.InstalledPack = app::git_artifact_fact{
						.Role     = app::git_artifact_role::installed_pack,
						.Identity = app::git_artifact_identity{ packIdentity },
						.Bytes    = static_cast<std::uint64_t>(packBytes),
					},
					.InstalledIndex = app::git_artifact_fact{
						.Role     = app::git_artifact_role::installed_index,
						.Identity = app::git_artifact_identity{ indexIdentity },
						.Bytes    = static_cast<std::uint64_t>(indexBytes),
					},
					.Base        = *installBase,
					.BaseTree    = *baseTree,
					.Head        = *installHead,
					.HeadTree    = *headTree,
					.HeadLineage = lineage,
				};
			} else if (installCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto candidate = prepare(
				database,
				"SELECT material_base_algorithm, material_base_digest, "
				"material_head_algorithm, material_head_digest, "
				"material_policy_revision, observed_at, origin, base_transition, "
				"head_transition, material_relation, primary_cause, "
				"base_availability, head_availability, candidate_review_present "
				"FROM append_hidden_candidate WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!candidate
				|| !bind_fixed(candidate->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(candidate->Value, 2, aOperation)) {
				return std::unexpected(candidate ? failure::unavailable : candidate.error());
			}
			const auto candidateCode = ::sqlite3_step(candidate->Value);
			if (candidateCode == SQLITE_ROW) {
				const auto candidateReviewPresent = ::sqlite3_column_int(candidate->Value, 13);
				if (candidateReviewPresent != 0 && candidateReviewPresent != 1) {
					return std::unexpected(failure::schema_mismatch);
				}
				std::optional<core::epoch_material_identity> material;
				if (::sqlite3_column_type(candidate->Value, 0) != SQLITE_NULL) {
					const auto baseTree = read_oid<core::git_tree_oid>(candidate->Value, 0, 1);
					const auto headTree = read_oid<core::git_tree_oid>(candidate->Value, 2, 3);
					const auto policy   = read_u64(candidate->Value, 4);
					if (!baseTree || !headTree || !policy) {
						return std::unexpected(failure::schema_mismatch);
					}
					material = core::epoch_material_identity{
						.BaseTree       = *baseTree,
						.HeadTree       = *headTree,
						.PolicyRevision = core::comparison_policy_revision{ *policy },
					};
				}
				core::append_epoch_mutation mutation{
					.Epoch = core::review_epoch_summary{
						.Id = core::review_epoch_id{
							.Authority   = Binding.Authority.Scope,
							.Incarnation = Binding.Catalog.Incarnation,
							.Ordinal     = ordinal,
						},
						.Source     = accepted.Source,
						.BaseBranch = accepted.BaseBranch,
						.Base       = accepted.Base,
						.Head       = accepted.Head,
						.Material   = material,
						.Cause      = core::epoch_cause{
							.Origin   = restored_enum<core::epoch_origin>(::sqlite3_column_int(candidate->Value, 6)),
							.Base     = restored_enum<core::epoch_base_transition>(::sqlite3_column_int(candidate->Value, 7)),
							.Head     = restored_enum<core::epoch_head_transition>(::sqlite3_column_int(candidate->Value, 8)),
							.Material = restored_enum<core::epoch_material_relation>(::sqlite3_column_int(candidate->Value, 9)),
							.Primary  = restored_enum<core::epoch_primary_cause>(::sqlite3_column_int(candidate->Value, 10)),
						},
						.ObservedAt       = core::observed_utc_milliseconds{ ::sqlite3_column_int64(candidate->Value, 5) },
						.BaseAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(candidate->Value, 11)),
						.HeadAvailability = restored_enum<core::anchor_availability>(::sqlite3_column_int(candidate->Value, 12)),
					},
				};
				if (::sqlite3_step(candidate->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				auto review = prepare(
					database,
					"SELECT association, review_id, actor_id, review_order, "
					"review_state, commit_algorithm, commit_digest, availability "
					"FROM append_candidate_review WHERE incarnation = ?1 "
					"AND operation = ?2");
				if (!review
					|| !bind_fixed(review->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(review->Value, 2, aOperation)) {
					return std::unexpected(review ? failure::unavailable : review.error());
				}
				const auto reviewCode = ::sqlite3_step(review->Value);
				if ((reviewCode == SQLITE_ROW) != (candidateReviewPresent != 0)) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (reviewCode == SQLITE_ROW) {
					const auto association  = read_fixed<core::review_association_id>(review->Value, 0);
					const auto reviewId     = read_u64(review->Value, 1);
					const auto actor        = read_u64(review->Value, 2);
					const auto commit       = read_oid<core::git_commit_oid>(review->Value, 5, 6);
					const auto order        = ::sqlite3_column_int(review->Value, 3);
					const auto state        = ::sqlite3_column_int(review->Value, 4);
					const auto availability = ::sqlite3_column_int(review->Value, 7);
					if (!association || !reviewId || !actor || !commit
						|| ::sqlite3_step(review->Value) != SQLITE_DONE) {
						return std::unexpected(failure::schema_mismatch);
					}
					mutation.Review = core::catalog_review_association{
						.Association = *association,
						.Review      = core::github_review_id{ *reviewId },
						.Actor       = core::github_viewer_actor_id{ *actor },
						.Order       = core::github_review_order{
							static_cast<std::uint16_t>(order) },
						.State        = restored_enum<core::github_review_state>(state),
						.Commit       = *commit,
						.Availability = restored_enum<core::anchor_availability>(availability),
					};
				} else if (reviewCode != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
				output.Candidate = std::move(mutation);
			} else if (candidateCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			const auto exactSpoolPair    = output.Spool && output.Preimport;
			const auto noSpoolPair       = !output.Spool && !output.Preimport;
			const auto coherentSpoolPair = exactSpoolPair || noSpoolPair;
			const auto exactInstall      = output.Install.has_value();
			const auto exactCandidate    = output.Candidate.has_value();
			using enum app::epoch_catalog_operation_phase;
			const auto factPhase          = output.RepairFromPhase.value_or(output.Phase);
			const auto coherentFactPrefix = coherentSpoolPair
				&& ((noSpoolPair && !exactInstall && !exactCandidate)
					|| (exactSpoolPair && !exactInstall && !exactCandidate)
					|| (exactSpoolPair && exactInstall && !exactCandidate)
					|| (exactSpoolPair && exactInstall && exactCandidate));
			const auto completeFactsValid = output.Phase == complete
				&& output.RecoveryInputCompacted && noSpoolPair && exactInstall
				&& exactCandidate;
			const auto abortedFactsValid = output.Phase == aborted
				&& output.RecoveryInputCompacted && noSpoolPair
				&& (!exactCandidate || exactInstall);
			const auto recoverableFactsValid = !output.RecoveryInputCompacted
				&& output.Phase != complete && output.Phase != aborted
				&& ((factPhase == preparing
					 && noSpoolPair && !exactInstall && !exactCandidate)
					|| (factPhase == spool_recorded && exactSpoolPair
						&& !exactInstall && !exactCandidate)
					|| ((factPhase == imported || factPhase == staging_ready)
						&& exactSpoolPair && exactInstall && !exactCandidate)
					|| ((factPhase == commit_authorized
						 || factPhase == ref_commit_in_progress
						 || factPhase == refs_committed
						 || factPhase == visible_cleanup_pending)
						&& exactSpoolPair && exactInstall && exactCandidate)
					|| (factPhase == abort_requested && coherentFactPrefix));
			const auto phaseFactsValid = completeFactsValid
				|| abortedFactsValid || recoverableFactsValid;
			if (!phaseFactsValid) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (
				output.Install
				&& (output.Install->Pack.Algorithm != Binding.ObjectFormat
					|| output.Install->Base != output.Context.Revision.Base
					|| output.Install->Head != output.Context.Revision.Head
					|| (!output.RecoveryInputCompacted
						&& (!output.Spool || !output.Preimport
							|| !coherent_install_facts(
								*output.Spool,
								*output.Preimport,
								*output.Install))))) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (output.Candidate) {
				const auto& epoch = output.Candidate->Epoch;
				if (
					epoch.Source != output.Context.Revision.Source
					|| epoch.BaseBranch != output.Context.Revision.BaseBranch
					|| epoch.Base != output.Context.Revision.Base
					|| epoch.Head != output.Context.Revision.Head
					|| epoch.BaseAvailability
						!= core::anchor_availability::available
					|| epoch.HeadAvailability
						!= core::anchor_availability::available
					|| !coherent_candidate_review(
						output.Candidate->Review,
						output.Context.Revision.CurrentViewerReview,
						Binding.Authority.Viewer)
					|| !epoch.Material || !output.Install
					|| epoch.Material->BaseTree != output.Install->BaseTree
					|| epoch.Material->HeadTree != output.Install->HeadTree
					|| ((output.Context.Revision.Origin
						 == core::pull_request_revision_origin::
							 explicit_historical_import)
						!= (epoch.Cause.Origin
							== core::epoch_origin::explicit_historical_import))) {
					return std::unexpected(failure::schema_mismatch);
				}
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}

		[[nodiscard]] auto valid_append_context(
			const app::append_operation_context& aContext,
			const app::epoch_catalog_binding&    aBinding,
			const core::application_session_id   aSession) noexcept -> bool
		{
			return aContext.ApplicationSession == aSession
				&& aContext.Request.value() != 0U && aContext.Generation.value() != 0U
				&& aContext.Operation.valid() && core::valid(aContext.Revision)
				&& aContext.Revision.Base.algorithm() == aBinding.ObjectFormat
				&& (!aContext.Revision.CurrentViewerReview
					|| aContext.Revision.CurrentViewerReview->Actor
						== aBinding.Authority.Viewer)
				&& aContext.Catalog == aBinding.Catalog;
		}

		[[nodiscard]] constexpr auto coherent_candidate_review(
			const std::optional<core::catalog_review_association>&      aCandidate,
			const std::optional<core::current_viewer_review_candidate>& aSource,
			const core::github_viewer_actor_id                          aViewer) noexcept -> bool
		{
			if (aCandidate.has_value() != aSource.has_value()) {
				return false;
			}
			if (!aCandidate) {
				return true;
			}
			return aSource && aCandidate->Actor == aViewer
				&& aCandidate->Review == aSource->Review
				&& aCandidate->Actor == aSource->Actor
				&& aCandidate->Order == aSource->Order
				&& aCandidate->State == aSource->State
				&& aCandidate->Commit == aSource->Commit;
		}

		auto sqlite_epoch_catalog_session::reserve_append(
			const app::append_reservation_request& aRequest,
			std::stop_token                        aStop) -> result<app::append_operation_record>
		{
			if (!valid_append_context(aRequest.Context, Binding, ApplicationSession)
				|| !core::valid(aRequest.ProposedOrdinal)) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database,
					Binding,
					aRequest.Context.Catalog.Revision,
					true,
					true);
				!active) {
				return std::unexpected(active.error());
			}
			auto existingIdentity = prepare(
				database,
				"SELECT kind FROM operation WHERE incarnation = ?1 "
				"AND operation = ?2");
			if (!existingIdentity
				|| !bind_fixed(
					existingIdentity->Value,
					1,
					Binding.Catalog.Incarnation)
				|| !bind_operation(
					existingIdentity->Value, 2, aRequest.Context.Operation)) {
				return std::unexpected(
					existingIdentity ? failure::unavailable
									 : existingIdentity.error());
			}
			const auto existingIdentityCode = ::sqlite3_step(existingIdentity->Value);
			if (existingIdentityCode == SQLITE_ROW) {
				const auto kind = ::sqlite3_column_int(existingIdentity->Value, 0);
				if (
					::sqlite3_column_type(existingIdentity->Value, 0)
						!= SQLITE_INTEGER
					|| kind < 1 || kind > 3
					|| ::sqlite3_step(existingIdentity->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (kind == 1) {
					auto existing = load_append(aRequest.Context.Operation);
					if (
						existing && existing->Context == aRequest.Context
						&& existing->ReservedOrdinal == aRequest.ProposedOrdinal) {
						return existing;
					}
					if (!existing
						&& existing.error() != failure::operation_conflict) {
						return std::unexpected(existing.error());
					}
				}
				return std::unexpected(failure::operation_identity_conflict);
			}
			if (existingIdentityCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}

			auto usedSource = catalog_contains_source_observation(
				database,
				Binding.Catalog.Incarnation,
				aRequest.Context.Revision.Source.Observation,
				aStop);
			if (!usedSource) {
				return std::unexpected(usedSource.error());
			}
			if (*usedSource) {
				return std::unexpected(failure::source_observation_conflict);
			}
			auto scope = prepare(
				database,
				"SELECT next_ordinal, catalog_revision FROM pull_request_scope "
				"WHERE incarnation = ?1 AND pr_scope = ?2 AND lifecycle = 1");
			if (!scope || !bind_fixed(scope->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(scope->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_step(scope->Value) != SQLITE_ROW) {
				return std::unexpected(failure::authority_mismatch);
			}
			if (::sqlite3_column_type(scope->Value, 0) == SQLITE_NULL) {
				return std::unexpected(failure::ordinal_exhausted);
			}
			const auto next     = ::sqlite3_column_int64(scope->Value, 0);
			const auto revision = ::sqlite3_column_int64(scope->Value, 1);
			if (next != aRequest.ProposedOrdinal.value()
				|| revision != aRequest.Context.Catalog.Revision.value()
				|| ::sqlite3_step(scope->Value) != SQLITE_DONE) {
				return std::unexpected(failure::revision_conflict);
			}
			if (auto idle = require_no_blocking_operation(database, Binding, false);
				!idle) {
				return std::unexpected(idle.error());
			}
			auto operation = prepare(
				database,
				"INSERT INTO operation(incarnation, operation, kind, phase, "
				"last_evidence, operation_revision, cache_scope, pr_scope, "
				"expected_revision, application_session, request_id, "
				"epoch_generation, source_observation, source_snapshot, "
				"identity_generation, pull_request_generation, reserved_ordinal, "
				"detail_count, "
				"retained_epoch_count, terminal) "
				"VALUES(?1, ?2, 1, 1, 1, 1, ?3, ?4, ?5, ?6, ?7, ?8, ?9, "
				"?10, ?11, ?12, ?13, 0, 0, 0)");
			if (!operation
				|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(operation->Value, 2, aRequest.Context.Operation)
				|| !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
				|| !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   operation->Value, 5, aRequest.Context.Catalog.Revision.value())
					!= SQLITE_OK
				|| !bind_u64(
					operation->Value, 6, aRequest.Context.ApplicationSession.value())
				|| !bind_u64(operation->Value, 7, aRequest.Context.Request.value())
				|| !bind_u64(operation->Value, 8, aRequest.Context.Generation.value())
				|| !bind_fixed(
					operation->Value,
					9,
					aRequest.Context.Revision.Source.Observation)
				|| !bind_u64(
					operation->Value,
					10,
					aRequest.Context.Revision.Source.Snapshot.value())
				|| !bind_u64(
					operation->Value,
					11,
					aRequest.Context.Revision.Source.IdentityGeneration.value())
				|| !bind_u64(
					operation->Value,
					12,
					aRequest.Context.Revision.Source.PullRequestGeneration.value())
				|| ::sqlite3_bind_int64(
					   operation->Value, 13, aRequest.ProposedOrdinal.value())
					!= SQLITE_OK
				|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					database, { }, failure::operation_identity_conflict));
			}
			auto append = prepare(
				database,
				"INSERT INTO append_operation(incarnation, operation, base_branch, "
				"revision_origin, base_algorithm, base_digest, head_algorithm, "
				"head_digest, source_review_present, staging_ref_format, "
				"final_ref_format) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1, 1)");
			if (!append || !bind_fixed(append->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(append->Value, 2, aRequest.Context.Operation)
				|| !bind_fixed(append->Value, 3, aRequest.Context.Revision.BaseBranch)
				|| ::sqlite3_bind_int(
					   append->Value,
					   4,
					   stored_enum(aRequest.Context.Revision.Origin))
					!= SQLITE_OK
				|| !bind_oid(append->Value, 5, 6, aRequest.Context.Revision.Base)
				|| !bind_oid(append->Value, 7, 8, aRequest.Context.Revision.Head)
				|| ::sqlite3_bind_int(
					   append->Value,
					   9,
					   aRequest.Context.Revision.CurrentViewerReview ? 1 : 0)
					!= SQLITE_OK
				|| ::sqlite3_step(append->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			if (aRequest.Context.Revision.CurrentViewerReview) {
				const auto& review = *aRequest.Context.Revision.CurrentViewerReview;
				auto        insert = prepare(
					database,
					"INSERT INTO append_source_review(incarnation, operation, "
					"review_id, actor_id, review_order, review_state, "
					"commit_algorithm, commit_digest) "
					"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)");
				if (!insert
					|| !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(insert->Value, 2, aRequest.Context.Operation)
					|| !bind_u64(insert->Value, 3, review.Review.value())
					|| !bind_u64(insert->Value, 4, review.Actor.value())
					|| ::sqlite3_bind_int(insert->Value, 5, review.Order.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int(
						   insert->Value, 6, stored_enum(review.State))
						!= SQLITE_OK
					|| !bind_oid(insert->Value, 7, 8, review.Commit)
					|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
			}
			auto advance = prepare(
				database,
				"UPDATE pull_request_scope SET next_ordinal = "
				"CASE WHEN next_ordinal = 9223372036854775807 THEN NULL "
				"ELSE next_ordinal + 1 END "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND next_ordinal = ?3 AND catalog_revision = ?4");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(advance->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   advance->Value, 3, aRequest.ProposedOrdinal.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   advance->Value, 4, aRequest.Context.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			auto output = load_append(aRequest.Context.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::record_spool(
			const app::record_spool_request& aRequest,
			std::stop_token                  aStop) -> result<app::append_operation_record>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
				|| aRequest.Spool.ExpectedPack.Algorithm != Binding.ObjectFormat) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision);
				!active) {
				return std::unexpected(active.error());
			}
			auto existing = load_append(aRequest.Operation);
			if (!existing) {
				return std::unexpected(existing.error());
			}
			if (existing->Phase == app::epoch_catalog_operation_phase::spool_recorded
				&& existing->OperationRevision.value()
					== aRequest.ExpectedOperationRevision.value() + 1
				&& existing->Spool == aRequest.Spool
				&& existing->Preimport == aRequest.Preimport) {
				return existing;
			}
			if (existing->Phase != app::epoch_catalog_operation_phase::preparing
				|| existing->OperationRevision != aRequest.ExpectedOperationRevision) {
				return std::unexpected(failure::phase_conflict);
			}
			auto receipt = prepare(
				database,
				"INSERT INTO append_receipt(incarnation, operation, spool_identity, "
				"spool_role, spool_bytes, expected_pack_algorithm, "
				"expected_pack_digest, observed_pack_algorithm, "
				"observed_pack_digest, repository_bytes, "
				"expected_pack_preexisting, inventory_count) "
				"VALUES(?1, ?2, ?3, 1, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)");
			if (!receipt || !bind_fixed(receipt->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(receipt->Value, 2, aRequest.Operation)
				|| !bind_bytes(receipt->Value, 3, aRequest.Spool.Spool.Identity.Bytes)
				|| ::sqlite3_bind_int64(
					   receipt->Value,
					   4,
					   static_cast<std::int64_t>(aRequest.Spool.Spool.Bytes))
					!= SQLITE_OK
				|| !bind_pack(receipt->Value, 5, 6, aRequest.Spool.ExpectedPack)
				|| !bind_pack(receipt->Value, 7, 8, aRequest.Spool.ObservedPack)
				|| ::sqlite3_bind_int64(
					   receipt->Value,
					   9,
					   static_cast<std::int64_t>(aRequest.Preimport.RepositoryBytes))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   receipt->Value,
					   10,
					   aRequest.Preimport.ExpectedPackPreexisting ? 1 : 0)
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   receipt->Value,
					   11,
					   static_cast<std::int64_t>(aRequest.Preimport.Packs.size()))
					!= SQLITE_OK
				|| ::sqlite3_step(receipt->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			auto inventory = prepare(
				database,
				"INSERT INTO append_inventory(incarnation, operation, position, "
				"pack_algorithm, pack_digest, pack_bytes, index_bytes) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7)");
			if (!inventory) {
				return std::unexpected(inventory.error());
			}
			for (std::size_t index = 0; index < aRequest.Preimport.Packs.size(); ++index) {
				const auto& pack = aRequest.Preimport.Packs[index];
				if (::sqlite3_reset(inventory->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(inventory->Value) != SQLITE_OK
					|| !bind_fixed(
						inventory->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(inventory->Value, 2, aRequest.Operation)
					|| ::sqlite3_bind_int64(
						   inventory->Value, 3, static_cast<std::int64_t>(index))
						!= SQLITE_OK
					|| !bind_pack(inventory->Value, 4, 5, pack.Pack)
					|| ::sqlite3_bind_int64(
						   inventory->Value,
						   6,
						   static_cast<std::int64_t>(pack.PackBytes))
						!= SQLITE_OK
					|| ::sqlite3_bind_int64(
						   inventory->Value,
						   7,
						   static_cast<std::int64_t>(pack.IndexBytes))
						!= SQLITE_OK
					|| ::sqlite3_step(inventory->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			auto advance = prepare(
				database,
				"UPDATE operation SET phase = 2, last_evidence = 1, "
				"operation_revision = operation_revision + 1 "
				"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
				"AND phase = 1 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope = ?5 "
				"AND operation_revision < 9223372036854775806");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advance->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int64(
					   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
				|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			auto output = load_append(aRequest.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::record_install(
			const app::record_install_request& aRequest,
			std::stop_token                    aStop) -> result<app::append_operation_record>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision);
				!active) {
				return std::unexpected(active.error());
			}
			auto existing = load_append(aRequest.Operation);
			if (!existing) {
				return std::unexpected(existing.error());
			}
			if (aRequest.Install.Base != existing->Context.Revision.Base
				|| aRequest.Install.Head != existing->Context.Revision.Head
				|| aRequest.Install.Pack.Algorithm != Binding.ObjectFormat) {
				return std::unexpected(failure::operation_conflict);
			}
			if (!existing->Spool || !existing->Preimport
				|| !coherent_install_facts(
					*existing->Spool, *existing->Preimport, aRequest.Install)) {
				return std::unexpected(failure::operation_conflict);
			}
			if (existing->Phase == app::epoch_catalog_operation_phase::imported
				&& existing->OperationRevision.value()
					== aRequest.ExpectedOperationRevision.value() + 1
				&& existing->Install == aRequest.Install) {
				return existing;
			}
			if (existing->Phase != app::epoch_catalog_operation_phase::spool_recorded
				|| existing->OperationRevision != aRequest.ExpectedOperationRevision
				|| !existing->Spool || !existing->Preimport
				|| existing->Spool->ExpectedPack != aRequest.Install.Pack) {
				return std::unexpected(failure::phase_conflict);
			}
			const auto& install = aRequest.Install;
			auto        insert  = prepare(
				database,
				"INSERT INTO append_install(incarnation, operation, pack_algorithm, "
				"pack_digest, base_algorithm, base_digest, base_tree_algorithm, "
				"base_tree_digest, head_algorithm, head_digest, "
				"head_tree_algorithm, head_tree_digest, head_lineage) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
				"?13)");
			if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(insert->Value, 2, aRequest.Operation)
				|| !bind_pack(insert->Value, 3, 4, install.Pack)
				|| !bind_oid(insert->Value, 5, 6, install.Base)
				|| !bind_oid(insert->Value, 7, 8, install.BaseTree)
				|| !bind_oid(insert->Value, 9, 10, install.Head)
				|| !bind_oid(insert->Value, 11, 12, install.HeadTree)
				|| ::sqlite3_bind_int(
					   insert->Value, 13, stored_enum(install.HeadLineage))
					!= SQLITE_OK
				|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			auto artifact = prepare(
				database,
				"INSERT INTO append_installed_artifact(incarnation, operation, "
				"artifact_role, artifact_identity, artifact_bytes) "
				"VALUES(?1, ?2, ?3, ?4, ?5)");
			if (!artifact) {
				return std::unexpected(artifact.error());
			}
			const std::array artifacts{ install.InstalledPack, install.InstalledIndex };
			for (const auto& value : artifacts) {
				if (::sqlite3_reset(artifact->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(artifact->Value) != SQLITE_OK
					|| !bind_fixed(
						artifact->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(artifact->Value, 2, aRequest.Operation)
					|| ::sqlite3_bind_int(
						   artifact->Value, 3, static_cast<int>(value.Role))
						!= SQLITE_OK
					|| !bind_bytes(artifact->Value, 4, value.Identity.Bytes)
					|| ::sqlite3_bind_int64(
						   artifact->Value, 5, static_cast<std::int64_t>(value.Bytes))
						!= SQLITE_OK
					|| ::sqlite3_step(artifact->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			auto advance = prepare(
				database,
				"UPDATE operation SET phase = 3, last_evidence = 1, "
				"operation_revision = operation_revision + 1 "
				"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
				"AND phase = 2 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope = ?5 "
				"AND operation_revision < 9223372036854775806");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advance->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int64(
					   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
				|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			auto output = load_append(aRequest.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::record_candidate(
			const app::record_candidate_request& aRequest,
			std::stop_token                      aStop) -> result<app::append_operation_record>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision, true);
				!active) {
				return std::unexpected(active.error());
			}
			auto existing = load_append(aRequest.Operation);
			if (!existing) {
				return std::unexpected(existing.error());
			}
			const auto& epoch             = aRequest.Candidate.Epoch;
			const auto  candidateCoherent = coherent_candidate_review(
				aRequest.Candidate.Review,
				existing->Context.Revision.CurrentViewerReview,
				Binding.Authority.Viewer);
			const auto materialCoherent = epoch.Material && existing->Install
				&& epoch.Material->BaseTree == existing->Install->BaseTree
				&& epoch.Material->HeadTree == existing->Install->HeadTree;
			const auto provenanceCoherent = (existing->Context.Revision.Origin
											 == core::pull_request_revision_origin::explicit_historical_import)
				== (epoch.Cause.Origin
					== core::epoch_origin::explicit_historical_import);
			if (!candidateCoherent || !materialCoherent || !provenanceCoherent
				|| epoch.BaseAvailability != core::anchor_availability::available
				|| epoch.HeadAvailability != core::anchor_availability::available) {
				return std::unexpected(failure::operation_conflict);
			}
			if (aRequest.Candidate.Review) {
				auto canonical = canonicalize_review(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*aRequest.Candidate.Review,
					std::pair{ epoch.Id.Ordinal, epoch.Head });
				if (!canonical) {
					return std::unexpected(canonical.error());
				}
				if (canonical->Review != *aRequest.Candidate.Review) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			if (existing->Phase == app::epoch_catalog_operation_phase::commit_authorized
				&& existing->Candidate == aRequest.Candidate
				&& existing->OperationRevision.value()
					== aRequest.ExpectedOperationRevision.value() + 1) {
				return existing;
			}
			if (existing->Phase != app::epoch_catalog_operation_phase::staging_ready
				|| existing->OperationRevision != aRequest.ExpectedOperationRevision
				|| !existing->Install || epoch.Id.Authority != Binding.Authority.Scope
				|| epoch.Id.Ordinal != existing->ReservedOrdinal
				|| epoch.Source != existing->Context.Revision.Source
				|| epoch.BaseBranch != existing->Context.Revision.BaseBranch
				|| epoch.Base != existing->Context.Revision.Base
				|| epoch.Head != existing->Context.Revision.Head) {
				return std::unexpected(failure::operation_conflict);
			}
			auto insert = prepare(
				database,
				"INSERT INTO append_hidden_candidate(incarnation, operation, "
				"material_base_algorithm, material_base_digest, "
				"material_head_algorithm, material_head_digest, "
				"material_policy_revision, observed_at, origin, base_transition, "
				"head_transition, material_relation, primary_cause, "
				"base_availability, head_availability, candidate_review_present) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, "
				"?12, ?13, ?14, ?15, ?16)");
			if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(insert->Value, 2, aRequest.Operation)) {
				return std::unexpected(insert ? failure::unavailable : insert.error());
			}
			const auto& material = *epoch.Material;
			if (!bind_oid(insert->Value, 3, 4, material.BaseTree)
				|| !bind_oid(insert->Value, 5, 6, material.HeadTree)
				|| !bind_u64(insert->Value, 7, material.PolicyRevision.value())) {
				return std::unexpected(failure::unavailable);
			}
			if (::sqlite3_bind_int64(
					insert->Value, 8, epoch.ObservedAt.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 9, stored_enum(epoch.Cause.Origin))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 10, stored_enum(epoch.Cause.Base))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 11, stored_enum(epoch.Cause.Head))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 12, stored_enum(epoch.Cause.Material))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 13, stored_enum(epoch.Cause.Primary))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 14, stored_enum(epoch.BaseAvailability))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 15, stored_enum(epoch.HeadAvailability))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 16, aRequest.Candidate.Review ? 1 : 0)
					!= SQLITE_OK
				|| ::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			if (aRequest.Candidate.Review) {
				const auto& review       = *aRequest.Candidate.Review;
				auto        reviewInsert = prepare(
					database,
					"INSERT INTO append_candidate_review(incarnation, operation, "
					"association, review_id, actor_id, review_order, review_state, "
					"commit_algorithm, commit_digest, availability) "
					"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)");
				if (!reviewInsert
					|| !bind_fixed(
						reviewInsert->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(reviewInsert->Value, 2, aRequest.Operation)
					|| !bind_fixed(reviewInsert->Value, 3, review.Association)
					|| !bind_u64(reviewInsert->Value, 4, review.Review.value())
					|| !bind_u64(reviewInsert->Value, 5, review.Actor.value())
					|| ::sqlite3_bind_int(
						   reviewInsert->Value, 6, review.Order.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int(
						   reviewInsert->Value, 7, stored_enum(review.State))
						!= SQLITE_OK
					|| !bind_oid(reviewInsert->Value, 8, 9, review.Commit)
					|| ::sqlite3_bind_int(
						   reviewInsert->Value,
						   10,
						   stored_enum(review.Availability))
						!= SQLITE_OK
					|| ::sqlite3_step(reviewInsert->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			auto advance = prepare(
				database,
				"UPDATE operation SET phase = 5, "
				"operation_revision = operation_revision + 1, "
				"last_evidence = 3 WHERE incarnation = ?1 AND operation = ?2 "
				"AND kind = 1 AND phase = 4 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope = ?5 "
				"AND operation_revision < 9223372036854775806");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advance->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int64(
					   advance->Value, 3, aRequest.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
				|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			auto output = load_append(aRequest.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::load_maintenance(
			const core::epoch_operation_id aOperation)
			-> result<app::maintenance_operation_record>
		{
			auto* database = State->Database.handle();
			auto  query    = prepare(
				database,
				"SELECT o.kind, o.phase, o.operation_revision, o.repair_from_phase, "
				"o.cache_scope, "
				"o.detail_count, o.retained_epoch_count "
				"FROM operation o WHERE o.incarnation = ?1 "
				"AND o.operation = ?2 AND ("
				"(o.kind = 2 AND o.cache_scope = ?3 AND o.pr_scope = ?4) OR "
				"(o.kind = 3 AND o.pr_scope IS NULL AND EXISTS("
				"SELECT 1 FROM repository_scope current "
				"JOIN repository_scope owned "
				"ON owned.incarnation = current.incarnation "
				"AND owned.profile_id = current.profile_id "
				"AND owned.profile_revision = current.profile_revision "
				"AND owned.host_id = current.host_id "
				"AND owned.host_authority = current.host_authority "
				"AND owned.account_id = current.account_id "
				"AND owned.access_mode = current.access_mode "
				"AND owned.repository_id = current.repository_id "
				"AND owned.object_format = current.object_format "
				"WHERE current.incarnation = o.incarnation "
				"AND current.cache_scope = ?3 "
				"AND owned.cache_scope = o.cache_scope)))");
			if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(query->Value, 2, aOperation)
				|| !bind_fixed(query->Value, 3, Binding.Authority.Cache)
				|| !bind_fixed(query->Value, 4, Binding.Authority.Scope)
				|| ::sqlite3_step(query->Value) != SQLITE_ROW) {
				return std::unexpected(
					query ? failure::operation_conflict : query.error());
			}
			auto                              cache               = read_fixed<core::repository_cache_scope_id>(query->Value, 4);
			const auto                        expectedDetailCount = ::sqlite3_column_int64(query->Value, 5);
			const auto                        retainedEpochCount  = ::sqlite3_column_int64(query->Value, 6);
			app::maintenance_operation_record output{
				.Incarnation = Binding.Catalog.Incarnation,
				.Operation   = aOperation,
				.Kind        = static_cast<app::epoch_catalog_operation_kind>(
					::sqlite3_column_int(query->Value, 0)),
				.Phase = static_cast<app::epoch_catalog_operation_phase>(
					::sqlite3_column_int(query->Value, 1)),
				.RepairFromPhase   = ::sqlite3_column_type(query->Value, 3) == SQLITE_NULL
					? std::nullopt
					: std::optional{ static_cast<app::epoch_catalog_operation_phase>(
						  ::sqlite3_column_int(query->Value, 3)) },
				.OperationRevision = app::epoch_catalog_operation_revision{
					::sqlite3_column_int64(query->Value, 2) },
			};
			if (!cache || expectedDetailCount < 1
				|| expectedDetailCount
					> static_cast<std::int64_t>(
						app::maximum_cache_delete_scope_count)
				|| retainedEpochCount < 0
				|| !app::valid(output.OperationRevision)
				|| ::sqlite3_step(query->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			output.Cache                             = *cache;
			std::uint64_t observedRetainedEpochCount = 0U;
			if (output.Kind == app::epoch_catalog_operation_kind::prune) {
				auto rows = prepare(
					database,
					"SELECT pr_scope, ordinal, base_algorithm, base_digest, "
					"base_availability, head_algorithm, head_digest, "
					"head_availability FROM prune_operation "
					"WHERE incarnation = ?1 AND operation = ?2 "
					"ORDER BY pr_scope, ordinal LIMIT 101");
				if (!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(rows->Value, 2, aOperation)) {
					return std::unexpected(rows ? failure::unavailable : rows.error());
				}
				while (true) {
					const auto code = ::sqlite3_step(rows->Value);
					if (code == SQLITE_DONE) {
						break;
					}
					if (code != SQLITE_ROW) {
						return std::unexpected(sql_failure(database, { }));
					}
					const auto authority = read_fixed<core::pull_request_authority_scope_id>(
						rows->Value, 0);
					const auto ordinal = core::epoch_ordinal{
						::sqlite3_column_int64(rows->Value, 1)
					};
					const auto base             = read_oid<core::git_commit_oid>(rows->Value, 2, 3);
					const auto head             = read_oid<core::git_commit_oid>(rows->Value, 5, 6);
					const auto baseAvailability = restored_enum<core::anchor_availability>(
						::sqlite3_column_int(rows->Value, 4));
					const auto headAvailability = restored_enum<core::anchor_availability>(
						::sqlite3_column_int(rows->Value, 7));
					if (!authority || *authority != Binding.Authority.Scope
						|| !core::valid(ordinal) || !base || !head
						|| ::sqlite3_column_type(rows->Value, 4) != SQLITE_INTEGER
						|| ::sqlite3_column_type(rows->Value, 7) != SQLITE_INTEGER
						|| !app::valid_prune_anchor_availability(baseAvailability)
						|| !app::valid_prune_anchor_availability(headAvailability)) {
						return std::unexpected(failure::schema_mismatch);
					}
					if (output.Epochs.size() >= app::maximum_prune_epoch_count) {
						return std::unexpected(failure::schema_mismatch);
					}
					output.Epochs.push_back(app::prune_epoch_fact{
						.Epoch = core::review_epoch_id{
							.Authority   = *authority,
							.Incarnation = Binding.Catalog.Incarnation,
							.Ordinal     = ordinal,
						},
						.Retained = app::git_retained_ref_pair{
							.Scope   = *authority,
							.Ordinal = ordinal,
							.Base    = *base,
							.Head    = *head,
						},
						.BaseRef = app::epoch_catalog_ref_identity{
							.Authority = *authority,
							.Ordinal   = ordinal,
						},
						.HeadRef = app::epoch_catalog_ref_identity{
							.Authority = *authority,
							.Ordinal   = ordinal,
							.Head      = true,
						},
						.ExpectedBaseAvailability = baseAvailability,
						.ExpectedHeadAvailability = headAvailability,
					});
				}
			} else if (output.Kind == app::epoch_catalog_operation_kind::cache_delete) {
				auto rows = prepare(
					database,
					"SELECT cache_scope, pr_scope, expected_revision, epoch_count "
					"FROM cache_delete_scope "
					"WHERE incarnation = ?1 AND operation = ?2 "
					"ORDER BY pr_scope LIMIT 10001");
				if (!rows || !bind_fixed(rows->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(rows->Value, 2, aOperation)) {
					return std::unexpected(rows ? failure::unavailable : rows.error());
				}
				while (true) {
					const auto code = ::sqlite3_step(rows->Value);
					if (code == SQLITE_DONE) {
						break;
					}
					if (code != SQLITE_ROW) {
						return std::unexpected(sql_failure(database, { }));
					}
					auto rowCache = read_fixed<core::repository_cache_scope_id>(
						rows->Value, 0);
					auto authority = read_fixed<core::pull_request_authority_scope_id>(
						rows->Value, 1);
					const auto revision   = ::sqlite3_column_int64(rows->Value, 2);
					const auto epochCount = ::sqlite3_column_int64(rows->Value, 3);
					if (!rowCache || *rowCache != output.Cache || !authority
						|| revision < 0 || epochCount < 0
						|| observedRetainedEpochCount
							> static_cast<std::uint64_t>(
								  std::numeric_limits<std::int64_t>::max())
								- static_cast<std::uint64_t>(epochCount)) {
						return std::unexpected(failure::schema_mismatch);
					}
					observedRetainedEpochCount += static_cast<std::uint64_t>(epochCount);
					if (output.Scopes.size()
						>= app::maximum_cache_delete_scope_count) {
						return std::unexpected(failure::schema_mismatch);
					}
					output.Scopes.push_back(app::cache_delete_scope_fact{
						.Authority        = *authority,
						.ExpectedRevision = core::epoch_catalog_revision{ revision },
					});
				}
			} else {
				return std::unexpected(failure::schema_mismatch);
			}
			const auto actualDetailCount = output.Kind
					== app::epoch_catalog_operation_kind::prune
				? output.Epochs.size()
				: output.Scopes.size();
			if (actualDetailCount
				!= static_cast<std::size_t>(expectedDetailCount)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (
				output.Kind == app::epoch_catalog_operation_kind::prune
				&& (expectedDetailCount
						> static_cast<std::int64_t>(app::maximum_prune_epoch_count)
					|| retainedEpochCount != 0)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (
				output.Kind == app::epoch_catalog_operation_kind::cache_delete
				&& observedRetainedEpochCount
					!= static_cast<std::uint64_t>(retainedEpochCount)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::schema_mismatch);
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::transition_operation(
			const app::operation_transition_request& aRequest,
			std::stop_token                          aStop) -> result<app::epoch_recovery_operation>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, { } };
			transaction      write{ database, { } };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto       currentAppend      = aRequest.Kind == app::epoch_catalog_operation_kind::append
				? load_append(aRequest.Operation)
				: result<app::append_operation_record>{
					  std::unexpected(failure::operation_conflict)
				  };
			auto       currentMaintenance = aRequest.Kind != app::epoch_catalog_operation_kind::append
				? load_maintenance(aRequest.Operation)
				: result<app::maintenance_operation_record>{
					  std::unexpected(failure::operation_conflict)
				  };
			const auto phase              = currentAppend ? currentAppend->Phase
				: currentMaintenance                      ? currentMaintenance->Phase
														  : aRequest.Expected;
			const auto revision           = currentAppend ? currentAppend->OperationRevision
				: currentMaintenance                      ? currentMaintenance->OperationRevision
														  : app::epoch_catalog_operation_revision{ };
			if (!currentAppend && !currentMaintenance) {
				return std::unexpected(failure::operation_conflict);
			}
			if (currentMaintenance && currentMaintenance->Kind != aRequest.Kind) {
				return std::unexpected(failure::operation_conflict);
			}
			if (aStop.stop_requested() && !app::point_of_no_cancellation(phase)) {
				return std::unexpected(failure::cancelled);
			}
			if (phase == aRequest.Next
				&& revision.value() == aRequest.ExpectedOperationRevision.value() + 1) {
				if (currentAppend) {
					return app::epoch_recovery_operation{ *currentAppend };
				}
				return app::epoch_recovery_operation{ *currentMaintenance };
			}
			if (phase != aRequest.Expected
				|| revision != aRequest.ExpectedOperationRevision) {
				return std::unexpected(failure::phase_conflict);
			}
			if (currentAppend) {
				if (aRequest.Next == app::epoch_catalog_operation_phase::staging_ready
					&& !currentAppend->Install) {
					return std::unexpected(failure::phase_conflict);
				}
			}
			if (aRequest.Kind != app::epoch_catalog_operation_kind::cache_delete) {
				if (auto active = require_active_pull_request_scope(
						database, Binding, aRequest.Catalog.Revision);
					!active) {
					return std::unexpected(active.error());
				}
			}
			if (aRequest.Next == app::epoch_catalog_operation_phase::repair_required) {
				const auto repairingTombstonedCache = aRequest.Kind
						== app::epoch_catalog_operation_kind::cache_delete
					&& aRequest.Expected
						== app::epoch_catalog_operation_phase::
							catalog_tombstoned_cleanup_pending;
				if (repairingTombstonedCache) {
					for (const auto& scopeFact : currentMaintenance->Scopes) {
						auto scope = load_pull_request_scope_state(
							database,
							Binding.Catalog.Incarnation,
							scopeFact.Authority);
						if (!scope || scope->Cache != currentMaintenance->Cache
							|| scope->Lifecycle != 3
							|| scope->RepositoryLifecycle != 3
							|| scope->RetainedEpochCount != 0U) {
							return std::unexpected(
								scope ? failure::operation_conflict : scope.error());
						}
					}
				}
				auto failClosed = prepare(
					database,
					aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
						? repairingTombstonedCache
							? "UPDATE repository_scope SET lifecycle = 3 "
							  "WHERE incarnation = ?1 AND cache_scope = ?2 "
							  "AND lifecycle = 3"
							: "UPDATE repository_scope SET lifecycle = 2 "
							  "WHERE incarnation = ?1 AND cache_scope = ?2 "
							  "AND lifecycle IN (1, 2)"
						: "UPDATE pull_request_scope SET lifecycle = 4 "
						  "WHERE incarnation = ?1 AND pr_scope = ?2 "
						  "AND cache_scope = ?3 AND lifecycle = 1");
				if (!failClosed) {
					return std::unexpected(failClosed.error());
				}
				const auto authorityBound = aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
					? bind_fixed(
						  failClosed->Value, 2, Binding.Authority.Cache)
					: bind_fixed(
						  failClosed->Value, 2, Binding.Authority.Scope)
						&& bind_fixed(
							failClosed->Value, 3, Binding.Authority.Cache);
				if (!bind_fixed(
						failClosed->Value, 1, Binding.Catalog.Incarnation)
					|| !authorityBound
					|| ::sqlite3_step(failClosed->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			if (
				aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
				&& aRequest.Expected
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending
				&& aRequest.Next == app::epoch_catalog_operation_phase::complete) {
				auto retire = prepare(
					database,
					"UPDATE repository_scope SET lifecycle = 4 "
					"WHERE incarnation = ?1 AND cache_scope = ?2 "
					"AND lifecycle = 3");
				if (!retire
					|| !bind_fixed(
						retire->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(retire->Value, 2, Binding.Authority.Cache)
					|| ::sqlite3_step(retire->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			if (
				aRequest.Kind == app::epoch_catalog_operation_kind::cache_delete
				&& aRequest.Expected
					== app::epoch_catalog_operation_phase::abort_requested
				&& aRequest.Next == app::epoch_catalog_operation_phase::aborted) {
				auto reactivate = prepare(
					database,
					"UPDATE repository_scope SET lifecycle = 1 "
					"WHERE incarnation = ?1 AND cache_scope = ?2 "
					"AND lifecycle = 2");
				if (!reactivate
					|| !bind_fixed(
						reactivate->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(
						reactivate->Value, 2, Binding.Authority.Cache)
					|| ::sqlite3_step(reactivate->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(
						reactivate ? failure::operation_conflict
								   : reactivate.error());
				}
			}
			if (
				currentAppend
				&& (aRequest.Next == app::epoch_catalog_operation_phase::complete
					|| aRequest.Next
						== app::epoch_catalog_operation_phase::aborted)) {
				auto inventory = prepare(
					database,
					"DELETE FROM append_inventory WHERE incarnation = ?1 "
					"AND operation = ?2");
				if (!inventory
					|| !bind_fixed(
						inventory->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(inventory->Value, 2, aRequest.Operation)
					|| ::sqlite3_step(inventory->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database)
						!= static_cast<int>(
							currentAppend->Preimport
								? currentAppend->Preimport->Packs.size()
								: 0U)) {
					return std::unexpected(failure::operation_conflict);
				}
				auto receipt = prepare(
					database,
					"DELETE FROM append_receipt WHERE incarnation = ?1 "
					"AND operation = ?2");
				if (!receipt
					|| !bind_fixed(
						receipt->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(receipt->Value, 2, aRequest.Operation)
					|| ::sqlite3_step(receipt->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database)
						!= (currentAppend->Spool ? 1 : 0)) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			const auto terminal = aRequest.Next == app::epoch_catalog_operation_phase::complete ? 1
				: aRequest.Next == app::epoch_catalog_operation_phase::aborted                  ? 2
				: aRequest.Next == app::epoch_catalog_operation_phase::repair_required
				? 3
				: 0;
			auto       update   = prepare(
				database,
				"UPDATE operation SET phase = ?1, last_evidence = ?2, "
				"operation_revision = operation_revision + 1, terminal = ?3 "
				", repair_from_phase = CASE WHEN ?1 = 12 THEN ?7 ELSE NULL END "
				", recovery_input_compacted = CASE "
				"WHEN ?6 = 1 AND ?1 IN (9, 11) THEN 1 ELSE 0 END "
				"WHERE incarnation = ?4 AND operation = ?5 AND kind = ?6 "
				"AND phase = ?7 AND operation_revision = ?8 "
				"AND cache_scope = ?9 AND ((kind IN (1, 2) AND pr_scope = ?10) "
				"OR (kind = 3 AND pr_scope IS NULL)) "
				"AND operation_revision < ?11");
			if (!update
				|| ::sqlite3_bind_int(
					   update->Value, 1, static_cast<int>(aRequest.Next))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   update->Value, 2, static_cast<int>(aRequest.Evidence))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(update->Value, 3, terminal) != SQLITE_OK
				|| !bind_fixed(update->Value, 4, Binding.Catalog.Incarnation)
				|| !bind_operation(update->Value, 5, aRequest.Operation)
				|| ::sqlite3_bind_int(
					   update->Value, 6, static_cast<int>(aRequest.Kind))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   update->Value, 7, static_cast<int>(aRequest.Expected))
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   update->Value, 8, aRequest.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(update->Value, 9, Binding.Authority.Cache)
				|| !bind_fixed(update->Value, 10, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   update->Value,
					   11,
					   app::terminal(aRequest.Next)
						   ? app::maximum_epoch_catalog_operation_revision
						   : app::maximum_epoch_catalog_operation_revision - 1)
					!= SQLITE_OK
				|| ::sqlite3_step(update->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::phase_conflict);
			}
			result<app::epoch_recovery_operation> loaded = std::unexpected(failure::operation_conflict);
			if (aRequest.Kind == app::epoch_catalog_operation_kind::append) {
				auto append = load_append(aRequest.Operation);
				if (!append) {
					return std::unexpected(append.error());
				}
				loaded = app::epoch_recovery_operation{ *append };
			} else {
				auto maintenance = load_maintenance(aRequest.Operation);
				if (!maintenance) {
					return std::unexpected(maintenance.error());
				}
				loaded = app::epoch_recovery_operation{ *maintenance };
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return loaded;
		}

		[[nodiscard]] auto persist_review(
			::sqlite3*                                   aDatabase,
			const core::epoch_catalog_incarnation_id&    aIncarnation,
			const core::pull_request_authority_scope_id& aAuthority,
			const core::catalog_review_association&      aReview,
			const core::provider_snapshot_id             aSnapshot,
			const std::optional<core::epoch_ordinal>     aLinked,
			std::stop_token                              aStop) -> result<void>
		{
			auto upsert = prepare(
				aDatabase,
				"INSERT INTO review_association(incarnation, pr_scope, association, "
				"review_id, actor_id, review_order, review_state, commit_algorithm, "
				"commit_digest, availability, linked_ordinal, source_snapshot) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12) "
				"ON CONFLICT(incarnation, pr_scope, association) DO UPDATE SET "
				"review_id = excluded.review_id, actor_id = excluded.actor_id, "
				"review_order = excluded.review_order, "
				"review_state = excluded.review_state, "
				"commit_algorithm = excluded.commit_algorithm, "
				"commit_digest = excluded.commit_digest, "
				"availability = excluded.availability, "
				"linked_ordinal = excluded.linked_ordinal, "
				"source_snapshot = excluded.source_snapshot");
			if (!upsert || !bind_fixed(upsert->Value, 1, aIncarnation)
				|| !bind_fixed(upsert->Value, 2, aAuthority)
				|| !bind_fixed(upsert->Value, 3, aReview.Association)
				|| !bind_u64(upsert->Value, 4, aReview.Review.value())
				|| !bind_u64(upsert->Value, 5, aReview.Actor.value())
				|| ::sqlite3_bind_int(upsert->Value, 6, aReview.Order.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   upsert->Value, 7, stored_enum(aReview.State))
					!= SQLITE_OK
				|| !bind_oid(upsert->Value, 8, 9, aReview.Commit)
				|| ::sqlite3_bind_int(
					   upsert->Value, 10, stored_enum(aReview.Availability))
					!= SQLITE_OK
				|| (aLinked
						? ::sqlite3_bind_int64(
							  upsert->Value, 11, aLinked->value())
						: ::sqlite3_bind_null(upsert->Value, 11))
					!= SQLITE_OK
				|| !bind_u64(upsert->Value, 12, aSnapshot.value())
				|| ::sqlite3_step(upsert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					aDatabase, aStop, failure::operation_conflict));
			}
			auto pointer = prepare(
				aDatabase,
				"INSERT INTO current_viewer_review(incarnation, pr_scope, "
				"association, accepted_snapshot) VALUES(?1, ?2, ?3, ?4) "
				"ON CONFLICT(incarnation, pr_scope) DO UPDATE SET "
				"association = excluded.association, "
				"accepted_snapshot = excluded.accepted_snapshot");
			if (!pointer || !bind_fixed(pointer->Value, 1, aIncarnation)
				|| !bind_fixed(pointer->Value, 2, aAuthority)
				|| !bind_fixed(pointer->Value, 3, aReview.Association)
				|| !bind_u64(pointer->Value, 4, aSnapshot.value())
				|| ::sqlite3_step(pointer->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(aDatabase, aStop));
			}
			return { };
		}

		auto sqlite_epoch_catalog_session::finalize_append(
			const app::finalize_append_request& aRequest,
			std::stop_token                     aStop) -> result<app::catalog_commit_result>
		{
			if (!app::valid(aRequest)
				|| aRequest.Catalog.Incarnation != Binding.Catalog.Incarnation
				|| aRequest.Mutation.Epoch.Id.Authority != Binding.Authority.Scope
				|| aRequest.Mutation.Epoch.Base.algorithm() != Binding.ObjectFormat) {
				return std::unexpected(failure::invalid_request);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, { } };
			transaction      write{ database, { } };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto operation = load_append(aRequest.Operation);
			if (!operation) {
				return std::unexpected(operation.error());
			}
			if (
				operation->Phase
					== app::epoch_catalog_operation_phase::visible_cleanup_pending
				|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
				auto scope = load_pull_request_scope_state(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope);
				auto epoch = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					aRequest.Mutation.Epoch.Id.Ordinal);
				if (!scope || scope->Cache != Binding.Authority.Cache
					|| scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
					|| !advanced_once(aRequest.Catalog.Revision, scope->Revision)
					|| !finalized_operation_revision_matches(
						operation->Phase,
						aRequest.ExpectedOperationRevision,
						operation->OperationRevision,
						app::epoch_catalog_operation_phase::
							visible_cleanup_pending)
					|| operation->Context.Catalog != aRequest.Catalog
					|| operation->Candidate != aRequest.Mutation || !epoch
					|| *epoch != aRequest.Mutation.Epoch) {
					return std::unexpected(failure::operation_conflict);
				}
				if (aRequest.Mutation.Epoch.Cause.Origin
					!= core::epoch_origin::explicit_historical_import) {
					auto retention = prepare(
						database,
						"SELECT retention_rehydration_required "
						"FROM pull_request_scope WHERE incarnation = ?1 "
						"AND pr_scope = ?2");
					if (!retention
						|| !bind_fixed(
							retention->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_fixed(
							retention->Value, 2, Binding.Authority.Scope)
						|| ::sqlite3_step(retention->Value) != SQLITE_ROW
						|| ::sqlite3_column_int(retention->Value, 0) != 0
						|| ::sqlite3_step(retention->Value) != SQLITE_DONE) {
						return std::unexpected(failure::operation_conflict);
					}
				}
				if (aRequest.Mutation.Review) {
					auto canonical = canonicalize_review(
						database,
						Binding.Catalog.Incarnation,
						Binding.Authority.Scope,
						*aRequest.Mutation.Review);
					if (!canonical) {
						return std::unexpected(canonical.error());
					}
					auto review = load_review_association(
						database,
						Binding.Catalog.Incarnation,
						Binding.Authority.Scope,
						Binding.Authority.Viewer,
						aRequest.Mutation.Review->Association);
					auto current = prepare(
						database,
						"SELECT association, accepted_snapshot "
						"FROM current_viewer_review WHERE incarnation = ?1 "
						"AND pr_scope = ?2");
					if (!review || *review != canonical->Review || !current
						|| !bind_fixed(
							current->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_fixed(
							current->Value, 2, Binding.Authority.Scope)
						|| ::sqlite3_step(current->Value) != SQLITE_ROW) {
						return std::unexpected(failure::operation_conflict);
					}
					auto association = read_fixed<core::review_association_id>(
						current->Value, 0);
					auto snapshot = read_u64(current->Value, 1);
					if (!association || !snapshot
						|| *association != aRequest.Mutation.Review->Association
						|| *snapshot
							!= operation->Context.Revision.Source.Snapshot.value()
						|| ::sqlite3_step(current->Value) != SQLITE_DONE) {
						return std::unexpected(failure::operation_conflict);
					}
				}
				Binding.Catalog.Revision = scope->Revision;
				return app::catalog_commit_result{
					.Catalog       = Binding.Catalog,
					.Changed       = true,
					.InsertedEpoch = aRequest.Mutation.Epoch.Id,
				};
			}
			if (aStop.stop_requested()
				&& !app::point_of_no_cancellation(operation->Phase)) {
				return std::unexpected(failure::cancelled);
			}
			if (aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (auto active = require_active_pull_request_scope(
					database,
					Binding,
					aRequest.Catalog.Revision,
					true,
					true);
				!active) {
				return std::unexpected(active.error());
			}
			if (operation->Phase != app::epoch_catalog_operation_phase::refs_committed
				|| operation->OperationRevision != aRequest.ExpectedOperationRevision
				|| operation->Candidate != aRequest.Mutation
				|| aRequest.Mutation.Epoch.Id.Authority != Binding.Authority.Scope) {
				return std::unexpected(failure::phase_conflict);
			}
			if (!coherent_candidate_review(
					aRequest.Mutation.Review,
					operation->Context.Revision.CurrentViewerReview,
					Binding.Authority.Viewer)) {
				return std::unexpected(failure::operation_conflict);
			}
			const auto&                        epoch = aRequest.Mutation.Epoch;
			std::optional<core::epoch_ordinal> latest;
			std::optional<core::epoch_ordinal> previous;
			auto                               pointers = prepare(
				database,
				"SELECT latest_live_ordinal, previous_distinct_head_ordinal, "
				"retention_rehydration_required FROM pull_request_scope "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND catalog_revision = ?3 AND lifecycle = 1");
			if (!pointers
				|| !bind_fixed(pointers->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(pointers->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   pointers->Value, 3, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(pointers->Value) != SQLITE_ROW) {
				return std::unexpected(failure::revision_conflict);
			}
			if (::sqlite3_column_type(pointers->Value, 0) == SQLITE_INTEGER) {
				latest = core::epoch_ordinal{
					::sqlite3_column_int64(pointers->Value, 0)
				};
			}
			if (::sqlite3_column_type(pointers->Value, 1) == SQLITE_INTEGER) {
				previous = core::epoch_ordinal{
					::sqlite3_column_int64(pointers->Value, 1)
				};
			}
			const auto retentionRequired = ::sqlite3_column_int(pointers->Value, 2);
			if ((retentionRequired != 0 && retentionRequired != 1)
				|| ::sqlite3_step(pointers->Value) != SQLITE_DONE) {
				return std::unexpected(failure::schema_mismatch);
			}
			std::optional<core::review_epoch_summary> priorLatest;
			if (latest) {
				auto loaded = load_epoch_summary(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*latest);
				if (!loaded) {
					return std::unexpected(loaded.error());
				}
				priorLatest = *loaded;
			}
			const auto rehydrating = epoch.Cause.Origin
				== core::epoch_origin::retention_rehydration;
			const auto historical = epoch.Cause.Origin
				== core::epoch_origin::explicit_historical_import;
			const auto sameSemanticRevision = priorLatest
				&& priorLatest->BaseBranch == epoch.BaseBranch
				&& priorLatest->Base == epoch.Base
				&& priorLatest->Head == epoch.Head;
			const auto freshSourceObservation = priorLatest
				&& priorLatest->Source.Observation != epoch.Source.Observation;
			const auto semanticRevisionChanged = priorLatest
				&& (priorLatest->BaseBranch != epoch.BaseBranch
					|| priorLatest->Base != epoch.Base
					|| priorLatest->Head != epoch.Head);
			if (rehydrating
				&& (retentionRequired == 0 || !sameSemanticRevision
					|| !freshSourceObservation
					|| priorLatest->BaseAvailability
						!= core::anchor_availability::cache_deleted
					|| priorLatest->HeadAvailability
						!= core::anchor_availability::cache_deleted)) {
				return std::unexpected(failure::operation_conflict);
			}
			if (retentionRequired != 0 && !historical && !rehydrating
				&& !semanticRevisionChanged) {
				return std::unexpected(failure::operation_conflict);
			}
			auto insert = prepare(
				database,
				"INSERT INTO epoch(incarnation, pr_scope, ordinal, "
				"source_observation, source_snapshot, identity_generation, "
				"pull_request_generation, base_branch, "
				"observed_at, origin, base_transition, head_transition, "
				"material_relation, primary_cause, material_base_algorithm, "
				"material_base_digest, material_head_algorithm, "
				"material_head_digest, material_policy_revision) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
				"?13, ?14, ?15, ?16, ?17, ?18, ?19)");
			if (!insert || !bind_fixed(insert->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(insert->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(insert->Value, 3, epoch.Id.Ordinal.value())
					!= SQLITE_OK
				|| !bind_fixed(insert->Value, 4, epoch.Source.Observation)
				|| !bind_u64(insert->Value, 5, epoch.Source.Snapshot.value())
				|| !bind_u64(
					insert->Value, 6, epoch.Source.IdentityGeneration.value())
				|| !bind_u64(
					insert->Value, 7, epoch.Source.PullRequestGeneration.value())
				|| !bind_fixed(insert->Value, 8, epoch.BaseBranch)
				|| ::sqlite3_bind_int64(insert->Value, 9, epoch.ObservedAt.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 10, stored_enum(epoch.Cause.Origin))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 11, stored_enum(epoch.Cause.Base))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 12, stored_enum(epoch.Cause.Head))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 13, stored_enum(epoch.Cause.Material))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(
					   insert->Value, 14, stored_enum(epoch.Cause.Primary))
					!= SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
			if (!epoch.Material
				|| !bind_oid(insert->Value, 15, 16, epoch.Material->BaseTree)
				|| !bind_oid(insert->Value, 17, 18, epoch.Material->HeadTree)
				|| !bind_u64(
					insert->Value, 19, epoch.Material->PolicyRevision.value())) {
				return std::unexpected(failure::operation_conflict);
			}
			if (::sqlite3_step(insert->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					database, { }, failure::operation_conflict));
			}
			auto anchor = prepare(
				database,
				"INSERT INTO epoch_anchor(incarnation, pr_scope, ordinal, endpoint, "
				"object_algorithm, object_digest, ref_format, availability) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, 1, ?7)");
			if (!anchor) {
				return std::unexpected(anchor.error());
			}
			const std::array anchorValues{
				std::pair{ epoch.Base, epoch.BaseAvailability },
				std::pair{ epoch.Head, epoch.HeadAvailability },
			};
			for (std::size_t index = 0; index < anchorValues.size(); ++index) {
				if (::sqlite3_reset(anchor->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
					|| !bind_fixed(
						anchor->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(anchor->Value, 2, Binding.Authority.Scope)
					|| ::sqlite3_bind_int64(
						   anchor->Value, 3, epoch.Id.Ordinal.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int(
						   anchor->Value, 4, static_cast<int>(index + 1U))
						!= SQLITE_OK
					|| !bind_oid(anchor->Value, 5, 6, anchorValues[index].first)
					|| ::sqlite3_bind_int(
						   anchor->Value,
						   7,
						   stored_enum(anchorValues[index].second))
						!= SQLITE_OK
					|| ::sqlite3_step(anchor->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
			}
			if (aRequest.Mutation.Review) {
				auto canonical = canonicalize_review(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					*aRequest.Mutation.Review);
				if (!canonical) {
					return std::unexpected(canonical.error());
				}
				auto persisted = persist_review(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope,
					canonical->Review,
					operation->Context.Revision.Source.Snapshot,
					canonical->LinkedOrdinal,
					{ });
				if (!persisted) {
					return std::unexpected(persisted.error());
				}
			}
			if (epoch.Cause.Origin != core::epoch_origin::explicit_historical_import) {
				if (latest && priorLatest->Head != epoch.Head) {
					previous = latest;
				}
				latest = epoch.Id.Ordinal;
			}
			auto advanceScope = prepare(
				database,
				"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
				"retained_epoch_count = retained_epoch_count + 1, "
				"latest_live_ordinal = ?1, previous_distinct_head_ordinal = ?2, "
				"retention_rehydration_required = CASE WHEN ?3 = 1 THEN "
				"retention_rehydration_required ELSE 0 END "
				"WHERE incarnation = ?4 AND pr_scope = ?5 "
				"AND catalog_revision = ?6 AND lifecycle = 1 "
				"AND retained_epoch_count < ?7 "
				"AND catalog_revision < 9223372036854775807");
			if (!advanceScope
				|| (latest
						? ::sqlite3_bind_int64(
							  advanceScope->Value, 1, latest->value())
						: ::sqlite3_bind_null(advanceScope->Value, 1))
					!= SQLITE_OK
				|| (previous
						? ::sqlite3_bind_int64(
							  advanceScope->Value, 2, previous->value())
						: ::sqlite3_bind_null(advanceScope->Value, 2))
					!= SQLITE_OK
				|| ::sqlite3_bind_int(advanceScope->Value, 3, historical ? 1 : 0)
					!= SQLITE_OK
				|| !bind_fixed(
					advanceScope->Value, 4, Binding.Catalog.Incarnation)
				|| !bind_fixed(advanceScope->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   advanceScope->Value, 6, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   advanceScope->Value,
					   7,
					   static_cast<std::int64_t>(
						   app::maximum_retained_epoch_count_per_pull_request))
					!= SQLITE_OK
				|| ::sqlite3_step(advanceScope->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			auto advanceOperation = prepare(
				database,
				"UPDATE operation SET phase = 8, last_evidence = 1, "
				"operation_revision = operation_revision + 1 "
				"WHERE incarnation = ?1 AND operation = ?2 AND kind = 1 "
				"AND phase = 7 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope = ?5 "
				"AND operation_revision < 9223372036854775806");
			if (!advanceOperation
				|| !bind_fixed(
					advanceOperation->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advanceOperation->Value, 2, aRequest.Operation)
				|| ::sqlite3_bind_int64(
					   advanceOperation->Value,
					   3,
					   aRequest.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(
					advanceOperation->Value, 4, Binding.Authority.Cache)
				|| !bind_fixed(
					advanceOperation->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_step(advanceOperation->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			Binding.Catalog.Revision = core::epoch_catalog_revision{
				aRequest.Catalog.Revision.value() + 1
			};
			return app::catalog_commit_result{
				.Catalog       = Binding.Catalog,
				.Changed       = true,
				.InsertedEpoch = epoch.Id,
			};
		}

		auto sqlite_epoch_catalog_session::associate_review(
			const app::associate_review_request& aRequest,
			std::stop_token                      aStop) -> result<app::catalog_commit_result>
		{
			if (!app::valid(aRequest) || aRequest.Catalog != Binding.Catalog
				|| aRequest.Review.Actor != Binding.Authority.Viewer
				|| aRequest.Review.Commit.algorithm() != Binding.ObjectFormat) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision, true);
				!active) {
				return std::unexpected(active.error());
			}
			if (auto idle = require_no_blocking_operation(database, Binding, false);
				!idle) {
				return std::unexpected(idle.error());
			}
			auto canonical = canonicalize_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				aRequest.Review);
			if (!canonical) {
				return std::unexpected(canonical.error());
			}
			const auto& reviewFact = canonical->Review;
			auto        current    = prepare(
				database,
				"SELECT c.association, c.accepted_snapshot, r.review_id, "
				"r.actor_id, r.review_order, r.review_state, r.commit_algorithm, "
				"r.commit_digest, r.availability "
				"FROM current_viewer_review c JOIN review_association r "
				"ON r.incarnation = c.incarnation AND r.pr_scope = c.pr_scope "
				"AND r.association = c.association "
				"WHERE c.incarnation = ?1 AND c.pr_scope = ?2");
			if (!current || !bind_fixed(current->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(current->Value, 2, Binding.Authority.Scope)) {
				return std::unexpected(current ? failure::unavailable : current.error());
			}
			const auto currentCode = ::sqlite3_step(current->Value);
			if (currentCode == SQLITE_ROW) {
				const auto association = read_fixed<core::review_association_id>(current->Value, 0);
				const auto snapshot    = read_u64(current->Value, 1);
				const auto review      = read_u64(current->Value, 2);
				const auto actor       = read_u64(current->Value, 3);
				const auto commit      = read_oid<core::git_commit_oid>(current->Value, 6, 7);
				if (!association || !snapshot || !review || !actor || !commit) {
					return std::unexpected(failure::schema_mismatch);
				}
				const auto exact = *association == reviewFact.Association
					&& *snapshot == aRequest.AcceptedSnapshot.value()
					&& *review == reviewFact.Review.value()
					&& *actor == reviewFact.Actor.value()
					&& ::sqlite3_column_int(current->Value, 4)
						== reviewFact.Order.value()
					&& ::sqlite3_column_int(current->Value, 5)
						== stored_enum(reviewFact.State)
					&& *commit == reviewFact.Commit
					&& ::sqlite3_column_int(current->Value, 8)
						== stored_enum(reviewFact.Availability);
				if (::sqlite3_step(current->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (exact) {
					return app::catalog_commit_result{
						.Catalog = Binding.Catalog,
						.Changed = false,
					};
				}
			} else if (currentCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, aStop));
			}
			auto persisted = persist_review(
				database,
				Binding.Catalog.Incarnation,
				Binding.Authority.Scope,
				reviewFact,
				aRequest.AcceptedSnapshot,
				canonical->LinkedOrdinal,
				aStop);
			if (!persisted) {
				return std::unexpected(persisted.error());
			}
			auto revision = prepare(
				database,
				"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1 "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND catalog_revision = ?3 AND lifecycle = 1 "
				"AND catalog_revision < 9223372036854775807");
			if (!revision
				|| !bind_fixed(revision->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(revision->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   revision->Value, 3, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(revision->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			Binding.Catalog.Revision = core::epoch_catalog_revision{
				aRequest.Catalog.Revision.value() + 1
			};
			return app::catalog_commit_result{
				.Catalog = Binding.Catalog,
				.Changed = true,
			};
		}

		auto sqlite_epoch_catalog_session::begin_prune(
			const app::begin_prune_request& aRequest,
			std::stop_token                 aStop) -> result<app::maintenance_operation_record>
		{
			if (!app::valid(aRequest)
				|| aRequest.Catalog.Incarnation != Binding.Catalog.Incarnation
				|| std::ranges::any_of(aRequest.Epochs, [this](const auto& aValue) {
					   return aValue.Epoch.Authority != Binding.Authority.Scope;
				   })) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			const auto verifyAnchors = [&](const auto& aFacts, const bool aFinalized)
				-> result<void> {
				for (const auto& fact : aFacts) {
					auto epoch = load_epoch_summary(
						database,
						Binding.Catalog.Incarnation,
						Binding.Authority.Scope,
						fact.Epoch.Ordinal);
					if (!epoch || epoch->Base != fact.Retained.Base
						|| epoch->Head != fact.Retained.Head
						|| epoch->BaseAvailability
							!= (aFinalized
									? core::anchor_availability::pruned
									: fact.ExpectedBaseAvailability)
						|| epoch->HeadAvailability
							!= (aFinalized
									? core::anchor_availability::pruned
									: fact.ExpectedHeadAvailability)) {
						return std::unexpected(failure::authority_mismatch);
					}
					if (aFinalized) {
						auto nonprunedReviews = linked_review_count(
							database,
							Binding.Catalog.Incarnation,
							fact,
							"availability != 3");
						if (!nonprunedReviews || *nonprunedReviews != 0) {
							return std::unexpected(failure::operation_conflict);
						}
					}
				}
				return { };
			};
			auto existing = load_maintenance(aRequest.Operation);
			if (existing) {
				if (existing->Kind == app::epoch_catalog_operation_kind::prune
					&& existing->Epochs == aRequest.Epochs) {
					auto expected = prepare(
						database,
						"SELECT expected_revision FROM operation "
						"WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
						"AND cache_scope = ?3 AND pr_scope = ?4");
					if (!expected
						|| !bind_fixed(
							expected->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_operation(expected->Value, 2, aRequest.Operation)
						|| !bind_fixed(
							expected->Value, 3, Binding.Authority.Cache)
						|| !bind_fixed(
							expected->Value, 4, Binding.Authority.Scope)
						|| ::sqlite3_step(expected->Value) != SQLITE_ROW
						|| ::sqlite3_column_type(expected->Value, 0)
							!= SQLITE_INTEGER
						|| ::sqlite3_column_int64(expected->Value, 0)
							!= aRequest.Catalog.Revision.value()
						|| ::sqlite3_step(expected->Value) != SQLITE_DONE) {
						return std::unexpected(failure::operation_conflict);
					}
					const auto finalized = existing->Phase
							== app::epoch_catalog_operation_phase::
								visible_cleanup_pending
						|| existing->Phase
							== app::epoch_catalog_operation_phase::complete;
					if (finalized) {
						auto scope = load_pull_request_scope_state(
							database,
							Binding.Catalog.Incarnation,
							Binding.Authority.Scope);
						if (!scope || scope->Cache != Binding.Authority.Cache
							|| scope->Lifecycle != 1
							|| scope->RepositoryLifecycle != 1
							|| scope->Revision != Binding.Catalog.Revision
							|| !advanced_once(
								aRequest.Catalog.Revision, scope->Revision)) {
							return std::unexpected(failure::operation_conflict);
						}
					} else {
						if (aRequest.Catalog != Binding.Catalog) {
							return std::unexpected(failure::operation_conflict);
						}
						if (auto active = require_active_pull_request_scope(
								database, Binding, aRequest.Catalog.Revision, true);
							!active) {
							return std::unexpected(active.error());
						}
					}
					if (auto verified = verifyAnchors(existing->Epochs, finalized);
						!verified) {
						return std::unexpected(failure::operation_conflict);
					}
					return existing;
				}
				return std::unexpected(failure::operation_conflict);
			}
			if (aRequest.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (auto active = require_active_pull_request_scope(
					database, Binding, aRequest.Catalog.Revision, true);
				!active) {
				return std::unexpected(active.error());
			}
			auto liveScope = prepare(
				database,
				"SELECT 1 FROM pull_request_scope WHERE incarnation = ?1 "
				"AND pr_scope = ?2 AND catalog_revision = ?3 AND lifecycle = 1");
			if (!liveScope
				|| !bind_fixed(
					liveScope->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(liveScope->Value, 2, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   liveScope->Value, 3, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(liveScope->Value) != SQLITE_ROW
				|| ::sqlite3_step(liveScope->Value) != SQLITE_DONE) {
				return std::unexpected(failure::revision_conflict);
			}
			if (auto verified = verifyAnchors(aRequest.Epochs, false); !verified) {
				return std::unexpected(verified.error());
			}
			auto operation = prepare(
				database,
				"INSERT INTO operation(incarnation, operation, kind, phase, "
				"last_evidence, operation_revision, cache_scope, pr_scope, "
				"expected_revision, application_session, request_id, "
				"epoch_generation, source_observation, source_snapshot, "
				"identity_generation, pull_request_generation, reserved_ordinal, "
				"detail_count, "
				"retained_epoch_count, terminal) "
				"VALUES(?1, ?2, 2, 13, 1, 1, ?3, ?4, ?5, ?6, NULL, NULL, "
				"NULL, NULL, NULL, NULL, NULL, ?7, 0, 0)");
			if (!operation
				|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(operation->Value, 2, aRequest.Operation)
				|| !bind_fixed(operation->Value, 3, Binding.Authority.Cache)
				|| !bind_fixed(operation->Value, 4, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   operation->Value, 5, aRequest.Catalog.Revision.value())
					!= SQLITE_OK
				|| !bind_u64(operation->Value, 6, ApplicationSession.value())
				|| ::sqlite3_bind_int64(
					   operation->Value,
					   7,
					   static_cast<std::int64_t>(aRequest.Epochs.size()))
					!= SQLITE_OK
				|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					database, aStop, failure::operation_conflict));
			}
			auto detail = prepare(
				database,
				"INSERT INTO prune_operation(incarnation, operation, pr_scope, "
				"ordinal, base_algorithm, base_digest, base_availability, "
				"head_algorithm, head_digest, head_availability, ref_format) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, 1)");
			if (!detail) {
				return std::unexpected(detail.error());
			}
			for (const auto& fact : aRequest.Epochs) {
				if (::sqlite3_reset(detail->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
					|| !bind_fixed(
						detail->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(detail->Value, 2, aRequest.Operation)
					|| !bind_fixed(detail->Value, 3, fact.Epoch.Authority)
					|| ::sqlite3_bind_int64(
						   detail->Value, 4, fact.Epoch.Ordinal.value())
						!= SQLITE_OK
					|| !bind_oid(detail->Value, 5, 6, fact.Retained.Base)
					|| ::sqlite3_bind_int(
						   detail->Value,
						   7,
						   stored_enum(fact.ExpectedBaseAvailability))
						!= SQLITE_OK
					|| !bind_oid(detail->Value, 8, 9, fact.Retained.Head)
					|| ::sqlite3_bind_int(
						   detail->Value,
						   10,
						   stored_enum(fact.ExpectedHeadAvailability))
						!= SQLITE_OK
					|| ::sqlite3_step(detail->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			auto output = load_maintenance(aRequest.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::finalize_prune(
			const app::finalize_prune_request& aRequest,
			std::stop_token                    aStop) -> result<app::catalog_commit_result>
		{
			if (!app::valid(aRequest)
				|| aRequest.Transition.Catalog.Incarnation
					!= Binding.Catalog.Incarnation) {
				return std::unexpected(failure::invalid_request);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, { } };
			transaction      write{ database, { } };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto operation = load_maintenance(aRequest.Transition.Operation);
			if (!operation
				|| operation->Kind != app::epoch_catalog_operation_kind::prune) {
				return std::unexpected(failure::phase_conflict);
			}
			std::vector<app::git_retained_ref_pair> expected;
			expected.reserve(operation->Epochs.size());
			for (const auto& epoch : operation->Epochs) {
				expected.push_back(epoch.Retained);
			}
			if (expected != aRequest.ObservedAbsent) {
				return std::unexpected(failure::operation_conflict);
			}
			if (
				operation->Phase
					== app::epoch_catalog_operation_phase::visible_cleanup_pending
				|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
				auto scope = load_pull_request_scope_state(
					database,
					Binding.Catalog.Incarnation,
					Binding.Authority.Scope);
				if (!scope || scope->Cache != Binding.Authority.Cache
					|| scope->Lifecycle != 1 || scope->RepositoryLifecycle != 1
					|| !advanced_once(
						aRequest.Transition.Catalog.Revision, scope->Revision)
					|| !finalized_operation_revision_matches(
						operation->Phase,
						aRequest.Transition.ExpectedOperationRevision,
						operation->OperationRevision,
						app::epoch_catalog_operation_phase::
							visible_cleanup_pending)) {
					return std::unexpected(failure::operation_conflict);
				}
				for (const auto& fact : operation->Epochs) {
					auto epoch = load_epoch_summary(
						database,
						Binding.Catalog.Incarnation,
						fact.Epoch.Authority,
						fact.Epoch.Ordinal);
					if (!epoch || epoch->Base != fact.Retained.Base
						|| epoch->Head != fact.Retained.Head
						|| epoch->BaseAvailability
							!= core::anchor_availability::pruned
						|| epoch->HeadAvailability
							!= core::anchor_availability::pruned) {
						return std::unexpected(failure::operation_conflict);
					}
					auto nonprunedReviews = linked_review_count(
						database,
						Binding.Catalog.Incarnation,
						fact,
						"availability != 3");
					if (!nonprunedReviews || *nonprunedReviews != 0) {
						return std::unexpected(failure::operation_conflict);
					}
				}
				Binding.Catalog.Revision = scope->Revision;
				return app::catalog_commit_result{
					.Catalog = Binding.Catalog,
					.Changed = true,
				};
			}
			if (aStop.stop_requested()
				&& !app::point_of_no_cancellation(operation->Phase)) {
				return std::unexpected(failure::cancelled);
			}
			if (aRequest.Transition.Catalog != Binding.Catalog) {
				return std::unexpected(failure::invalid_request);
			}
			if (auto active = require_active_pull_request_scope(
					database,
					Binding,
					aRequest.Transition.Catalog.Revision,
					true);
				!active) {
				return std::unexpected(active.error());
			}
			if (operation->Phase
					!= app::epoch_catalog_operation_phase::ref_delete_in_progress
				|| operation->OperationRevision
					!= aRequest.Transition.ExpectedOperationRevision) {
				return std::unexpected(failure::phase_conflict);
			}
			auto anchor = prepare(
				database,
				"UPDATE epoch_anchor SET availability = ?1 "
				"WHERE incarnation = ?2 AND pr_scope = ?3 AND ordinal = ?4 "
				"AND endpoint = ?5 AND object_algorithm = ?6 "
				"AND object_digest = ?7 AND availability = ?8");
			if (!anchor) {
				return std::unexpected(anchor.error());
			}
			auto reviews = prepare(
				database,
				"UPDATE review_association SET availability = ?1 "
				"WHERE incarnation = ?2 AND pr_scope = ?3 "
				"AND linked_ordinal = ?4 AND commit_algorithm = ?5 "
				"AND commit_digest = ?6 AND availability IN (1, 2, 5, 6)");
			if (!reviews) {
				return std::unexpected(reviews.error());
			}
			for (const auto& fact : operation->Epochs) {
				auto destructiveReviews = linked_review_count(
					database,
					Binding.Catalog.Incarnation,
					fact,
					"availability IN (3, 4)");
				if (!destructiveReviews || *destructiveReviews != 0) {
					return std::unexpected(failure::operation_conflict);
				}
				const std::array anchors{
					std::pair{
						fact.Retained.Base,
						fact.ExpectedBaseAvailability,
					},
					std::pair{
						fact.Retained.Head,
						fact.ExpectedHeadAvailability,
					},
				};
				for (std::size_t index = 0; index < anchors.size(); ++index) {
					if (::sqlite3_reset(anchor->Value) != SQLITE_OK
						|| ::sqlite3_clear_bindings(anchor->Value) != SQLITE_OK
						|| ::sqlite3_bind_int(
							   anchor->Value,
							   1,
							   stored_enum(core::anchor_availability::pruned))
							!= SQLITE_OK
						|| !bind_fixed(
							anchor->Value, 2, Binding.Catalog.Incarnation)
						|| !bind_fixed(anchor->Value, 3, fact.Epoch.Authority)
						|| ::sqlite3_bind_int64(
							   anchor->Value, 4, fact.Epoch.Ordinal.value())
							!= SQLITE_OK
						|| ::sqlite3_bind_int(
							   anchor->Value, 5, static_cast<int>(index + 1U))
							!= SQLITE_OK
						|| !bind_oid(anchor->Value, 6, 7, anchors[index].first)
						|| ::sqlite3_bind_int(
							   anchor->Value,
							   8,
							   stored_enum(anchors[index].second))
							!= SQLITE_OK
						|| ::sqlite3_step(anchor->Value) != SQLITE_DONE
						|| ::sqlite3_changes(database) != 1) {
						return std::unexpected(failure::operation_conflict);
					}
				}
				if (::sqlite3_reset(reviews->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
					|| ::sqlite3_bind_int(
						   reviews->Value,
						   1,
						   stored_enum(core::anchor_availability::pruned))
						!= SQLITE_OK
					|| !bind_fixed(
						reviews->Value, 2, Binding.Catalog.Incarnation)
					|| !bind_fixed(reviews->Value, 3, fact.Epoch.Authority)
					|| ::sqlite3_bind_int64(
						   reviews->Value, 4, fact.Epoch.Ordinal.value())
						!= SQLITE_OK
					|| !bind_oid(reviews->Value, 5, 6, fact.Retained.Head)
					|| ::sqlite3_step(reviews->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
				auto nonprunedReviews = linked_review_count(
					database,
					Binding.Catalog.Incarnation,
					fact,
					"availability != 3");
				if (!nonprunedReviews || *nonprunedReviews != 0) {
					return std::unexpected(failure::operation_conflict);
				}
			}
			auto revision = prepare(
				database,
				"UPDATE pull_request_scope SET catalog_revision = catalog_revision + 1, "
				"retained_epoch_count = retained_epoch_count - ?1 "
				"WHERE incarnation = ?2 AND pr_scope = ?3 "
				"AND catalog_revision = ?4 AND lifecycle = 1 "
				"AND retained_epoch_count >= ?1 "
				"AND catalog_revision < 9223372036854775807");
			if (!revision
				|| ::sqlite3_bind_int64(
					   revision->Value,
					   1,
					   static_cast<std::int64_t>(operation->Epochs.size()))
					!= SQLITE_OK
				|| !bind_fixed(revision->Value, 2, Binding.Catalog.Incarnation)
				|| !bind_fixed(revision->Value, 3, Binding.Authority.Scope)
				|| ::sqlite3_bind_int64(
					   revision->Value, 4, Binding.Catalog.Revision.value())
					!= SQLITE_OK
				|| ::sqlite3_step(revision->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			auto advance = prepare(
				database,
				"UPDATE operation SET phase = 8, last_evidence = 6, "
				"operation_revision = operation_revision + 1 "
				"WHERE incarnation = ?1 AND operation = ?2 AND kind = 2 "
				"AND phase = 14 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope = ?5 "
				"AND operation_revision < 9223372036854775806");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advance->Value, 2, aRequest.Transition.Operation)
				|| ::sqlite3_bind_int64(
					   advance->Value,
					   3,
					   aRequest.Transition.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
				|| !bind_fixed(advance->Value, 5, Binding.Authority.Scope)
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			Binding.Catalog.Revision = core::epoch_catalog_revision{
				aRequest.Transition.Catalog.Revision.value() + 1
			};
			return app::catalog_commit_result{
				.Catalog = Binding.Catalog,
				.Changed = true,
			};
		}

		[[nodiscard]] auto load_cache_impact(
			::sqlite3*                                aDatabase,
			const core::epoch_catalog_incarnation_id& aIncarnation,
			const core::repository_cache_scope_id&    aCache)
			-> result<app::cache_delete_impact>
		{
			app::cache_delete_impact output{
				.Incarnation = aIncarnation,
				.Cache       = aCache,
			};
			auto scopes = prepare(
				aDatabase,
				"SELECT p.pr_scope, p.catalog_revision, p.retained_epoch_count, "
				"(SELECT count(*) FROM epoch e "
				" JOIN epoch_anchor a ON a.incarnation = e.incarnation "
				" AND a.pr_scope = e.pr_scope AND a.ordinal = e.ordinal "
				" AND a.endpoint = 1 AND a.availability NOT IN (3, 4) "
				" WHERE e.incarnation = p.incarnation AND e.pr_scope = p.pr_scope), "
				"p.lifecycle, r.lifecycle "
				"FROM pull_request_scope p JOIN repository_scope r "
				"ON r.incarnation = p.incarnation AND r.cache_scope = p.cache_scope "
				"WHERE p.incarnation = ?1 AND p.cache_scope = ?2 "
				"ORDER BY p.pr_scope LIMIT 10001");
			if (!scopes || !bind_fixed(scopes->Value, 1, aIncarnation)
				|| !bind_fixed(scopes->Value, 2, aCache)) {
				return std::unexpected(scopes ? failure::unavailable : scopes.error());
			}
			while (true) {
				const auto code = ::sqlite3_step(scopes->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW) {
					return std::unexpected(sql_failure(aDatabase, { }));
				}
				auto authority = read_fixed<core::pull_request_authority_scope_id>(
					scopes->Value, 0);
				const auto revision            = ::sqlite3_column_int64(scopes->Value, 1);
				const auto retained            = ::sqlite3_column_int64(scopes->Value, 2);
				const auto count               = ::sqlite3_column_int64(scopes->Value, 3);
				const auto prLifecycle         = ::sqlite3_column_int(scopes->Value, 4);
				const auto repositoryLifecycle = ::sqlite3_column_int(scopes->Value, 5);
				if (!authority || revision < 0 || retained < 0 || count < 0
					|| retained != count
					|| output.RetainedEpochCount
						> std::numeric_limits<std::uint64_t>::max()
							- static_cast<std::uint64_t>(count)) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (repositoryLifecycle != 1 && repositoryLifecycle != 2) {
					return std::unexpected(failure::authority_mismatch);
				}
				if (prLifecycle != 1 && prLifecycle != 4) {
					return std::unexpected(failure::repair_required);
				}
				if (output.Scopes.size() >= app::maximum_cache_delete_scope_count) {
					return std::unexpected(failure::schema_mismatch);
				}
				output.Scopes.push_back(app::cache_delete_scope_fact{
					.Authority        = *authority,
					.ExpectedRevision = core::epoch_catalog_revision{ revision },
				});
				output.RetainedEpochCount += static_cast<std::uint64_t>(count);
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::authority_mismatch);
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::cache_delete_impact(
			const core::repository_cache_scope_id aCache,
			std::stop_token                       aStop) -> result<app::cache_delete_impact>
		{
			if (!aCache.valid() || aCache != Binding.Authority.Cache) {
				return std::unexpected(failure::authority_mismatch);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			progress_guard   progress{ State->Database.handle(), aStop };
			return load_cache_impact(
				State->Database.handle(), Binding.Catalog.Incarnation, aCache);
		}

		auto sqlite_epoch_catalog_session::begin_cache_delete(
			const app::begin_cache_delete_request& aRequest,
			std::stop_token                        aStop) -> result<app::maintenance_operation_record>
		{
			if (!app::valid(aRequest)
				|| aRequest.Impact.Incarnation != Binding.Catalog.Incarnation
				|| aRequest.Impact.Cache != Binding.Authority.Cache) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			transaction      write{ database, aStop };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			if (aRequest.Impact.RetainedEpochCount
				> static_cast<std::uint64_t>(
					std::numeric_limits<std::int64_t>::max())) {
				return std::unexpected(failure::quota_exceeded);
			}
			auto existing = load_maintenance(aRequest.Operation);
			if (existing) {
				if (existing->Kind == app::epoch_catalog_operation_kind::cache_delete
					&& existing->Cache == aRequest.Impact.Cache
					&& existing->Scopes == aRequest.Impact.Scopes) {
					auto retained = prepare(
						database,
						"SELECT retained_epoch_count FROM operation "
						"WHERE incarnation = ?1 AND operation = ?2 "
						"AND kind = 3 AND cache_scope = ?3 AND pr_scope IS NULL");
					if (!retained
						|| !bind_fixed(
							retained->Value, 1, Binding.Catalog.Incarnation)
						|| !bind_operation(retained->Value, 2, aRequest.Operation)
						|| !bind_fixed(retained->Value, 3, aRequest.Impact.Cache)
						|| ::sqlite3_step(retained->Value) != SQLITE_ROW
						|| ::sqlite3_column_int64(retained->Value, 0)
							!= static_cast<std::int64_t>(
								aRequest.Impact.RetainedEpochCount)
						|| ::sqlite3_step(retained->Value) != SQLITE_DONE) {
						return std::unexpected(failure::operation_conflict);
					}
					return existing;
				}
				return std::unexpected(failure::operation_conflict);
			}
			if (auto idle = require_no_blocking_operation(database, Binding, true);
				!idle) {
				return std::unexpected(idle.error());
			}
			auto current = load_cache_impact(
				database, Binding.Catalog.Incarnation, aRequest.Impact.Cache);
			if (!current || *current != aRequest.Impact) {
				return std::unexpected(failure::revision_conflict);
			}
			for (const auto& scope : aRequest.Impact.Scopes) {
				auto state = load_pull_request_scope_state(
					database, Binding.Catalog.Incarnation, scope.Authority);
				if (!state || state->Cache != aRequest.Impact.Cache
					|| (state->Lifecycle != 1 && state->Lifecycle != 4)
					|| (state->RepositoryLifecycle != 1
						&& state->RepositoryLifecycle != 2)) {
					return std::unexpected(
						state ? failure::authority_mismatch : state.error());
				}
				if (state->Revision != scope.ExpectedRevision
					|| state->Revision.value()
						>= core::maximum_epoch_catalog_revision) {
					return std::unexpected(failure::revision_conflict);
				}
			}
			auto operation = prepare(
				database,
				"INSERT INTO operation(incarnation, operation, kind, phase, "
				"last_evidence, operation_revision, cache_scope, pr_scope, "
				"expected_revision, application_session, request_id, "
				"epoch_generation, source_observation, source_snapshot, "
				"identity_generation, pull_request_generation, reserved_ordinal, "
				"detail_count, "
				"retained_epoch_count, terminal) "
				"VALUES(?1, ?2, 3, 16, 1, 1, ?3, NULL, NULL, ?4, NULL, NULL, "
				"NULL, NULL, NULL, NULL, NULL, ?5, ?6, 0)");
			if (!operation
				|| !bind_fixed(operation->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(operation->Value, 2, aRequest.Operation)
				|| !bind_fixed(operation->Value, 3, aRequest.Impact.Cache)
				|| !bind_u64(operation->Value, 4, ApplicationSession.value())
				|| ::sqlite3_bind_int64(
					   operation->Value,
					   5,
					   static_cast<std::int64_t>(aRequest.Impact.Scopes.size()))
					!= SQLITE_OK
				|| ::sqlite3_bind_int64(
					   operation->Value,
					   6,
					   static_cast<std::int64_t>(aRequest.Impact.RetainedEpochCount))
					!= SQLITE_OK
				|| ::sqlite3_step(operation->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(
					database, aStop, failure::operation_conflict));
			}
			auto detail = prepare(
				database,
				"INSERT INTO cache_delete_scope(incarnation, operation, cache_scope, "
				"pr_scope, expected_revision, epoch_count) "
				"VALUES(?1, ?2, ?3, ?4, ?5, ?6)");
			if (!detail) {
				return std::unexpected(detail.error());
			}
			for (const auto& scope : aRequest.Impact.Scopes) {
				auto countQuery = prepare(
					database,
					"SELECT count(*) FROM epoch e JOIN epoch_anchor a "
					"ON a.incarnation = e.incarnation AND a.pr_scope = e.pr_scope "
					"AND a.ordinal = e.ordinal AND a.endpoint = 1 "
					"AND a.availability NOT IN (3, 4) "
					"WHERE e.incarnation = ?1 AND e.pr_scope = ?2");
				if (!countQuery
					|| !bind_fixed(
						countQuery->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(countQuery->Value, 2, scope.Authority)
					|| ::sqlite3_step(countQuery->Value) != SQLITE_ROW) {
					return std::unexpected(failure::unavailable);
				}
				const auto count = ::sqlite3_column_int64(countQuery->Value, 0);
				if (count < 0 || ::sqlite3_step(countQuery->Value) != SQLITE_DONE) {
					return std::unexpected(failure::schema_mismatch);
				}
				if (::sqlite3_reset(detail->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(detail->Value) != SQLITE_OK
					|| !bind_fixed(
						detail->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_operation(detail->Value, 2, aRequest.Operation)
					|| !bind_fixed(detail->Value, 3, aRequest.Impact.Cache)
					|| !bind_fixed(detail->Value, 4, scope.Authority)
					|| ::sqlite3_bind_int64(
						   detail->Value, 5, scope.ExpectedRevision.value())
						!= SQLITE_OK
					|| ::sqlite3_bind_int64(detail->Value, 6, count) != SQLITE_OK
					|| ::sqlite3_step(detail->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, aStop));
				}
			}
			auto mark = prepare(
				database,
				"UPDATE repository_scope SET lifecycle = 2 "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND lifecycle IN (1, 2)");
			if (!mark || !bind_fixed(mark->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(mark->Value, 2, aRequest.Impact.Cache)
				|| ::sqlite3_step(mark->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			auto output = load_maintenance(aRequest.Operation);
			if (!output) {
				return std::unexpected(output.error());
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}

		auto sqlite_epoch_catalog_session::finalize_cache_delete(
			const app::finalize_cache_delete_request& aRequest,
			std::stop_token                           aStop) -> result<void>
		{
			if (!app::valid(aRequest)
				|| aRequest.Transition.Catalog.Incarnation
					!= Binding.Catalog.Incarnation) {
				return std::unexpected(failure::invalid_request);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, { } };
			transaction      write{ database, { } };
			if (auto begun = write.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto operation = load_maintenance(aRequest.Transition.Operation);
			if (!operation
				|| operation->Kind
					!= app::epoch_catalog_operation_kind::cache_delete
				|| operation->Cache != Binding.Authority.Cache) {
				return std::unexpected(failure::phase_conflict);
			}
			if (
				operation->Phase
					== app::epoch_catalog_operation_phase::
						catalog_tombstoned_cleanup_pending
				|| operation->Phase == app::epoch_catalog_operation_phase::complete) {
				if (!finalized_operation_revision_matches(
						operation->Phase,
						aRequest.Transition.ExpectedOperationRevision,
						operation->OperationRevision,
						app::epoch_catalog_operation_phase::
							catalog_tombstoned_cleanup_pending)) {
					return std::unexpected(failure::operation_conflict);
				}
				auto repository = prepare(
					database,
					"SELECT lifecycle FROM repository_scope "
					"WHERE incarnation = ?1 AND cache_scope = ?2");
				const auto expectedLifecycle = operation->Phase == app::epoch_catalog_operation_phase::complete
					? 4
					: 3;
				if (!repository
					|| !bind_fixed(
						repository->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(repository->Value, 2, operation->Cache)
					|| ::sqlite3_step(repository->Value) != SQLITE_ROW
					|| ::sqlite3_column_int(repository->Value, 0)
						!= expectedLifecycle
					|| ::sqlite3_step(repository->Value) != SQLITE_DONE) {
					return std::unexpected(failure::operation_conflict);
				}
				std::optional<core::epoch_catalog_revision> boundRevision;
				for (const auto& scopeFact : operation->Scopes) {
					auto scope = load_pull_request_scope_state(
						database,
						Binding.Catalog.Incarnation,
						scopeFact.Authority);
					if (!scope || scope->Lifecycle != 3
						|| scope->RepositoryLifecycle != expectedLifecycle
						|| !advanced_once(
							scopeFact.ExpectedRevision, scope->Revision)
						|| scope->RetainedEpochCount != 0U) {
						return std::unexpected(failure::operation_conflict);
					}
					if (scopeFact.Authority == Binding.Authority.Scope) {
						boundRevision = scope->Revision;
					}
				}
				if (!boundRevision) {
					return std::unexpected(failure::authority_mismatch);
				}
				Binding.Catalog.Revision = *boundRevision;
				return { };
			}
			if (aStop.stop_requested()
				&& !app::point_of_no_cancellation(operation->Phase)) {
				return std::unexpected(failure::cancelled);
			}
			if (aRequest.Transition.Catalog != Binding.Catalog
				|| operation->Phase
					!= app::epoch_catalog_operation_phase::quarantined
				|| operation->OperationRevision
					!= aRequest.Transition.ExpectedOperationRevision) {
				return std::unexpected(failure::phase_conflict);
			}
			for (const auto& scopeFact : operation->Scopes) {
				auto scope = load_pull_request_scope_state(
					database,
					Binding.Catalog.Incarnation,
					scopeFact.Authority);
				if (!scope || scope->Cache != operation->Cache
					|| (scope->Lifecycle != 1 && scope->Lifecycle != 4)
					|| scope->RepositoryLifecycle != 2) {
					return std::unexpected(
						scope ? failure::authority_mismatch : scope.error());
				}
				if (scope->Revision != scopeFact.ExpectedRevision
					|| scope->Revision.value()
						>= core::maximum_epoch_catalog_revision) {
					return std::unexpected(failure::revision_conflict);
				}
			}
			auto saturatedRepair = prepare(
				database,
				"SELECT 1 FROM operation WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND terminal = 3 "
				"AND operation_revision = 9223372036854775807 LIMIT 1");
			if (!saturatedRepair
				|| !bind_fixed(
					saturatedRepair->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(saturatedRepair->Value, 2, operation->Cache)) {
				return std::unexpected(
					saturatedRepair ? failure::unavailable
									: saturatedRepair.error());
			}
			const auto saturatedCode = ::sqlite3_step(saturatedRepair->Value);
			if (saturatedCode == SQLITE_ROW) {
				return std::unexpected(failure::operation_conflict);
			}
			if (saturatedCode != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto compactRepairInventory = prepare(
				database,
				"DELETE FROM append_inventory WHERE incarnation = ?1 "
				"AND operation IN (SELECT operation FROM operation "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND kind = 1 AND terminal = 3)");
			auto compactRepairReceipt = prepare(
				database,
				"DELETE FROM append_receipt WHERE incarnation = ?1 "
				"AND operation IN (SELECT operation FROM operation "
				"WHERE incarnation = ?1 AND cache_scope = ?2 "
				"AND kind = 1 AND terminal = 3)");
			if (!compactRepairInventory || !compactRepairReceipt
				|| !bind_fixed(
					compactRepairInventory->Value,
					1,
					Binding.Catalog.Incarnation)
				|| !bind_fixed(
					compactRepairInventory->Value, 2, operation->Cache)
				|| ::sqlite3_step(compactRepairInventory->Value) != SQLITE_DONE
				|| !bind_fixed(
					compactRepairReceipt->Value,
					1,
					Binding.Catalog.Incarnation)
				|| !bind_fixed(
					compactRepairReceipt->Value, 2, operation->Cache)
				|| ::sqlite3_step(compactRepairReceipt->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto supersedeRepair = prepare(
				database,
				"UPDATE operation SET phase = 11, last_evidence = 5, "
				"operation_revision = operation_revision + 1, terminal = 2, "
				"repair_from_phase = NULL, recovery_input_compacted = "
				"CASE WHEN kind = 1 THEN 1 ELSE 0 END WHERE incarnation = ?1 "
				"AND cache_scope = ?2 AND operation != ?3 AND terminal = 3 "
				"AND operation_revision < 9223372036854775807");
			if (!supersedeRepair
				|| !bind_fixed(
					supersedeRepair->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(supersedeRepair->Value, 2, operation->Cache)
				|| !bind_operation(
					supersedeRepair->Value, 3, aRequest.Transition.Operation)
				|| ::sqlite3_step(supersedeRepair->Value) != SQLITE_DONE) {
				return std::unexpected(sql_failure(database, { }));
			}
			auto tombstone = prepare(
				database,
				"UPDATE pull_request_scope SET lifecycle = 3, "
				"catalog_revision = catalog_revision + 1, retained_epoch_count = 0 "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND catalog_revision = ?3 AND lifecycle IN (1, 4) "
				"AND catalog_revision < 9223372036854775807");
			auto anchors = prepare(
				database,
				"UPDATE epoch_anchor SET availability = 4 "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND availability != 4");
			auto reviews = prepare(
				database,
				"UPDATE review_association SET availability = 4 "
				"WHERE incarnation = ?1 AND pr_scope = ?2 "
				"AND availability != 4");
			if (!tombstone || !anchors || !reviews) {
				return std::unexpected(failure::unavailable);
			}
			for (const auto& scope : operation->Scopes) {
				if (::sqlite3_reset(anchors->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(anchors->Value) != SQLITE_OK
					|| !bind_fixed(
						anchors->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(anchors->Value, 2, scope.Authority)
					|| ::sqlite3_step(anchors->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
				if (::sqlite3_reset(reviews->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(reviews->Value) != SQLITE_OK
					|| !bind_fixed(
						reviews->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(reviews->Value, 2, scope.Authority)
					|| ::sqlite3_step(reviews->Value) != SQLITE_DONE) {
					return std::unexpected(sql_failure(database, { }));
				}
				if (::sqlite3_reset(tombstone->Value) != SQLITE_OK
					|| ::sqlite3_clear_bindings(tombstone->Value) != SQLITE_OK
					|| !bind_fixed(
						tombstone->Value, 1, Binding.Catalog.Incarnation)
					|| !bind_fixed(tombstone->Value, 2, scope.Authority)
					|| ::sqlite3_bind_int64(
						   tombstone->Value, 3, scope.ExpectedRevision.value())
						!= SQLITE_OK
					|| ::sqlite3_step(tombstone->Value) != SQLITE_DONE
					|| ::sqlite3_changes(database) != 1) {
					return std::unexpected(failure::revision_conflict);
				}
			}
			auto repository = prepare(
				database,
				"UPDATE repository_scope SET lifecycle = 3 "
				"WHERE incarnation = ?1 AND cache_scope = ?2 AND lifecycle = 2");
			if (!repository
				|| !bind_fixed(repository->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(repository->Value, 2, operation->Cache)
				|| ::sqlite3_step(repository->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::revision_conflict);
			}
			auto advance = prepare(
				database,
				"UPDATE operation SET phase = 19, last_evidence = 1, "
				"operation_revision = operation_revision + 1 "
				"WHERE incarnation = ?1 AND operation = ?2 AND kind = 3 "
				"AND phase = 18 AND operation_revision = ?3 "
				"AND cache_scope = ?4 AND pr_scope IS NULL "
				"AND operation_revision < 9223372036854775806");
			if (!advance || !bind_fixed(advance->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_operation(advance->Value, 2, aRequest.Transition.Operation)
				|| ::sqlite3_bind_int64(
					   advance->Value,
					   3,
					   aRequest.Transition.ExpectedOperationRevision.value())
					!= SQLITE_OK
				|| !bind_fixed(advance->Value, 4, Binding.Authority.Cache)
				|| ::sqlite3_step(advance->Value) != SQLITE_DONE
				|| ::sqlite3_changes(database) != 1) {
				return std::unexpected(failure::operation_conflict);
			}
			if (auto committed = write.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			for (const auto& scope : operation->Scopes) {
				if (scope.Authority == Binding.Authority.Scope) {
					Binding.Catalog.Revision = core::epoch_catalog_revision{
						scope.ExpectedRevision.value() + 1
					};
					break;
				}
			}
			return { };
		}

		auto sqlite_epoch_catalog_session::recovery_page(
			const std::optional<core::epoch_operation_id> aAfter,
			const std::uint16_t                           aLimit,
			std::stop_token                               aStop) -> result<app::epoch_recovery_page>
		{
			if ((aAfter && !aAfter->valid()) || aLimit == 0U
				|| aLimit > app::maximum_epoch_recovery_page_size) {
				return std::unexpected(failure::invalid_request);
			}
			if (aStop.stop_requested()) {
				return std::unexpected(failure::cancelled);
			}
			std::scoped_lock lock{ State->Mutex };
			auto*            database = State->Database.handle();
			progress_guard   progress{ database, aStop };
			read_transaction read{ database, aStop };
			if (auto begun = read.begin(); !begun) {
				return std::unexpected(begun.error());
			}
			auto query = prepare(
				database,
				aAfter
					? "SELECT o.operation, o.kind FROM operation o "
					  "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
					  "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
					  "AND o.pr_scope = ?3) OR (o.kind = 3 "
					  "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
					  "FROM repository_scope current JOIN repository_scope owned "
					  "ON owned.incarnation = current.incarnation "
					  "AND owned.profile_id = current.profile_id "
					  "AND owned.profile_revision = current.profile_revision "
					  "AND owned.host_id = current.host_id "
					  "AND owned.host_authority = current.host_authority "
					  "AND owned.account_id = current.account_id "
					  "AND owned.access_mode = current.access_mode "
					  "AND owned.repository_id = current.repository_id "
					  "AND owned.object_format = current.object_format "
					  "WHERE current.incarnation = o.incarnation "
					  "AND current.cache_scope = ?2 "
					  "AND owned.cache_scope = o.cache_scope))) "
					  "AND o.operation >= ?4 ORDER BY o.operation LIMIT ?5"
					: "SELECT o.operation, o.kind FROM operation o "
					  "WHERE o.incarnation = ?1 AND o.terminal NOT IN (1, 2) "
					  "AND ((o.kind IN (1, 2) AND o.cache_scope = ?2 "
					  "AND o.pr_scope = ?3) OR (o.kind = 3 "
					  "AND o.pr_scope IS NULL AND EXISTS(SELECT 1 "
					  "FROM repository_scope current JOIN repository_scope owned "
					  "ON owned.incarnation = current.incarnation "
					  "AND owned.profile_id = current.profile_id "
					  "AND owned.profile_revision = current.profile_revision "
					  "AND owned.host_id = current.host_id "
					  "AND owned.host_authority = current.host_authority "
					  "AND owned.account_id = current.account_id "
					  "AND owned.access_mode = current.access_mode "
					  "AND owned.repository_id = current.repository_id "
					  "AND owned.object_format = current.object_format "
					  "WHERE current.incarnation = o.incarnation "
					  "AND current.cache_scope = ?2 "
					  "AND owned.cache_scope = o.cache_scope))) "
					  "ORDER BY o.operation LIMIT ?4");
			if (!query || !bind_fixed(query->Value, 1, Binding.Catalog.Incarnation)
				|| !bind_fixed(query->Value, 2, Binding.Authority.Cache)
				|| !bind_fixed(query->Value, 3, Binding.Authority.Scope)) {
				return std::unexpected(query ? failure::unavailable : query.error());
			}
			const auto fetch = static_cast<std::int64_t>(aLimit) + 1;
			if (aAfter) {
				if (!bind_operation(query->Value, 4, *aAfter)
					|| ::sqlite3_bind_int64(query->Value, 5, fetch) != SQLITE_OK) {
					return std::unexpected(failure::unavailable);
				}
			} else if (::sqlite3_bind_int64(query->Value, 4, fetch) != SQLITE_OK) {
				return std::unexpected(failure::unavailable);
			}
			std::vector<std::pair<core::epoch_operation_id, int>> identities;
			while (true) {
				const auto code = ::sqlite3_step(query->Value);
				if (code == SQLITE_DONE) {
					break;
				}
				if (code != SQLITE_ROW || aStop.stop_requested()) {
					return std::unexpected(sql_failure(database, aStop));
				}
				auto operation = read_fixed<core::epoch_operation_id>(query->Value, 0);
				if (!operation) {
					return std::unexpected(failure::schema_mismatch);
				}
				identities.emplace_back(
					*operation, ::sqlite3_column_int(query->Value, 1));
			}
			app::epoch_recovery_page output{
				.Incarnation = Binding.Catalog.Incarnation,
			};
			if (identities.size() > aLimit) {
				output.Continuation = identities.back().first;
				identities.pop_back();
			}
			output.Operations.reserve(identities.size());
			for (const auto& [operation, kind] : identities) {
				if (kind == static_cast<int>(app::epoch_catalog_operation_kind::append)) {
					auto loaded = load_append(operation);
					if (!loaded) {
						return std::unexpected(loaded.error());
					}
					output.Operations.emplace_back(*loaded);
				} else {
					auto loaded = load_maintenance(operation);
					if (!loaded) {
						return std::unexpected(loaded.error());
					}
					output.Operations.emplace_back(*loaded);
				}
			}
			if (!app::valid(output)) {
				return std::unexpected(failure::schema_mismatch);
			}
			if (auto committed = read.commit(); !committed) {
				return std::unexpected(committed.error());
			}
			return output;
		}
	} // namespace

	auto make_epoch_catalog(
		const epoch_catalog_configuration& aConfiguration,
		std::stop_token                    aStop)
		-> app::epoch_catalog_result<std::unique_ptr<app::epoch_catalog_port>>
	{
		auto database = detail::catalog_database::open(
			aConfiguration.StorageRoot, aStop);
		if (!database) {
			return std::unexpected(map_failure(database.error()));
		}
		auto state = std::make_shared<catalog_state>(std::move(*database));
		return std::make_unique<sqlite_epoch_catalog_port>(std::move(state));
	}
} // namespace bpr::adapters::sqlite
