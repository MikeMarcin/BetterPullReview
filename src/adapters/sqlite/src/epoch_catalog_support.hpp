#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_EPOCH_CATALOG_SUPPORT_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_EPOCH_CATALOG_SUPPORT_HPP
#include "bpr/adapters/sqlite/epoch_catalog.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "catalog_database.hpp"
#include "sqlite_connection.hpp"

#if defined(BPR_SQLITE_TESTING)
	#include "bpr/adapters/sqlite/testing.hpp"
	#include <atomic>
#endif

#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <stop_token>
#include <string_view>
#include <utility>

// Statements, transactions, catalog state and the session and port classes the epoch catalogue's sources share.
namespace bpr::adapters::sqlite::epoch_catalog_detail {
	namespace app  = application;
	namespace core = bpr::core;

#if defined(BPR_SQLITE_TESTING)
	inline std::atomic<detail::testing::epoch_journal_checkpoint> JournalCheckpoint = nullptr;
#endif
	using failure = app::epoch_catalog_failure;

	template <typename Value>
	using result = app::epoch_catalog_result<Value>;

	class statement final
	{
	public:
		statement()                                    = default;
		statement(const statement&)                    = delete;
		auto operator=(const statement&) -> statement& = delete;
		statement(statement&& aOther) noexcept;
		auto operator=(statement&& aOther) noexcept -> statement&;
		~statement();

		::sqlite3_stmt* Value = nullptr;
	};

	struct stop_context
	{
		std::stop_token Stop;
	};

	auto stop_progress(void* aOpaque) noexcept -> int;

	class progress_guard final
	{
	public:
		progress_guard(::sqlite3* aDatabase, std::stop_token aStop);

	private:
		stop_context                   Context;
		detail::progress_handler_scope Handler;
	};

	[[nodiscard]] auto sql_failure(
		::sqlite3*      aDatabase,
		std::stop_token aStop,
		const failure   aFallback = failure::unavailable) noexcept -> failure;

	[[nodiscard]] auto prepare(::sqlite3* aDatabase, std::string_view aSql)
		-> result<statement>;

	[[nodiscard]] auto execute(
		::sqlite3*       aDatabase,
		std::string_view aSql,
		std::stop_token  aStop) -> result<void>;

	class transaction final
	{
	public:
		transaction(::sqlite3* aDatabase, std::stop_token aStop);
		transaction(const transaction&)                    = delete;
		auto operator=(const transaction&) -> transaction& = delete;
		~transaction();

		[[nodiscard]] auto begin() -> result<void>;

		[[nodiscard]] auto commit() -> result<void>;

	private:
		::sqlite3*      Database;
		std::stop_token Stop;
		bool            Active = false;
	};

	class read_transaction final
	{
	public:
		read_transaction(::sqlite3* aDatabase, std::stop_token aStop);
		read_transaction(const read_transaction&)                    = delete;
		auto operator=(const read_transaction&) -> read_transaction& = delete;
		~read_transaction();

		[[nodiscard]] auto begin() -> result<void>;

		[[nodiscard]] auto commit() -> result<void>;

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
		-> bool;

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
		const core::epoch_operation_id& aOperation) noexcept -> bool;

	[[nodiscard]] auto bind_oid(
		::sqlite3_stmt*                     aQuery,
		const int                           aAlgorithmIndex,
		const int                           aDigestIndex,
		const core::git_hash_algorithm      aAlgorithm,
		const std::array<std::uint8_t, 32>& aBytes) noexcept -> bool;

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

	[[nodiscard]] auto read_blob(
		::sqlite3_stmt*         aQuery,
		const int               aColumn,
		std::span<std::uint8_t> aOutput) noexcept -> bool;

	[[nodiscard]] auto read_u64(
		::sqlite3_stmt* aQuery, const int aColumn) noexcept
		-> std::optional<std::uint64_t>;

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
		-> std::optional<std::uint64_t>;

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

	struct catalog_state final
	{
		explicit catalog_state(detail::catalog_database aDatabase);

		detail::catalog_database Database;
		std::mutex               Mutex;
	};

	class sqlite_epoch_catalog_session final : public app::epoch_catalog_session
	{
	public:
		sqlite_epoch_catalog_session(
			std::shared_ptr<catalog_state> aState,
			app::epoch_catalog_binding     aBinding,
			core::application_session_id   aApplicationSession);

		[[nodiscard]] auto binding() const noexcept
			-> const app::epoch_catalog_binding& override;

		[[nodiscard]] auto supersede_occurrence(const core::epoch_occurrence& aExpected, const core::epoch_occurrence& aReplacement, std::stop_token aStop) -> result<void> override;
		[[nodiscard]] auto occurrences(std::stop_token aStop) -> result<core::epoch_occurrence_history> override;
		[[nodiscard]] auto record_occurrences(const core::epoch_occurrence_history& aHistory, std::stop_token aStop) -> result<void> override;
		[[nodiscard]] auto retained_residency(core::epoch_ordinal aOrdinal, std::stop_token aStop)
			-> result<app::git_revision_residency> override;

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
		[[nodiscard]] auto record_receiving(
			const app::record_receiving_request& aRequest,
			std::stop_token                      aStop) -> result<app::append_operation_record> override;
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
		explicit sqlite_epoch_catalog_port(std::shared_ptr<catalog_state> aState);

		[[nodiscard]] auto open(
			const app::epoch_catalog_open_request& aRequest,
			std::stop_token                        aStop)
			-> result<std::unique_ptr<app::epoch_catalog_session>> override;

	private:
		std::shared_ptr<catalog_state> State;
	};

	[[nodiscard]] auto load_epoch_summary(
		::sqlite3*                                   aDatabase,
		const core::epoch_catalog_incarnation_id&    aIncarnation,
		const core::pull_request_authority_scope_id& aAuthority,
		const core::epoch_ordinal                    aOrdinal) -> result<core::review_epoch_summary>;

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
		-> result<pull_request_scope_state>;

	[[nodiscard]] auto require_active_pull_request_scope(
		::sqlite3*                         aDatabase,
		const app::epoch_catalog_binding&  aBinding,
		const core::epoch_catalog_revision aExpected,
		const bool                         aRequireAdvanceCapacity  = false,
		const bool                         aRequireRetainedCapacity = false) -> result<void>;

	[[nodiscard]] auto require_no_blocking_operation(
		::sqlite3*                        aDatabase,
		const app::epoch_catalog_binding& aBinding,
		const bool                        aWholeCache) -> result<void>;

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
		-> result<core::catalog_review_association>;

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
			aProjectedHead = std::nullopt) -> result<canonical_review_fact>;

	[[nodiscard]] auto catalog_contains_source_observation(
		::sqlite3*                                aDatabase,
		const core::epoch_catalog_incarnation_id& aIncarnation,
		const core::epoch_source_observation_id&  aObservation,
		std::stop_token                           aStop) -> result<bool>;

	[[nodiscard]] auto read_occurrence_rows(::sqlite3*                        aDatabase,
											const app::epoch_catalog_binding& aBinding, std::stop_token aStop, const bool aArchived = false)
		-> result<core::epoch_occurrence_history>;

	[[nodiscard]] constexpr auto coherent_candidate_review(
		const std::optional<core::catalog_review_association>&      aCandidate,
		const std::optional<core::current_viewer_review_candidate>& aSource,
		core::github_viewer_actor_id                                aViewer) noexcept -> bool;

	[[nodiscard]] auto coherent_install_facts(
		const app::durable_git_spool_fact&     aSpool,
		const app::durable_git_preimport_fact& aPreimport,
		const app::durable_git_install_fact&   aInstall) noexcept -> bool;

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

	[[nodiscard]] auto map_failure(
		const detail::catalog_database_failure aValue) noexcept -> failure;
}
#endif
