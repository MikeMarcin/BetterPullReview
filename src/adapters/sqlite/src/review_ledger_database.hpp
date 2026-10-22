#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

#include "bpr/application/review_ledger_port.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Version 1 held the review scope and file receipts. Later versions
	/// added conversation receipts, private records, agent sessions, and the
	/// source-line coordinate correction. The value is deliberately not "1": a
	/// migration mechanism whose only version is the initial one has never
	/// migrated anything, and would be first exercised by a user's database.
	/// [[BPR-1591]] Version 10 adds the private note line.
	inline constexpr std::int64_t review_ledger_schema_version = 11;

	/// \brief Classifies a valid defect write at the SQLite authority boundary.
	enum class defect_store_outcome : std::uint8_t {
		stored,
		identity_conflict,
	};

	/// \brief Identifies the review a row belongs to.
	///
	/// \details Every ledger row is scoped by all four fields. A pull request
	/// number is unique only within a repository, a repository name only within
	/// an account, and an account only within a host, so no shorter key can
	/// keep two reviews apart. Reusing PR number 7 on a second host must not
	/// surface the first host's receipts.
	struct review_scope
	{
		std::string_view Host;
		std::string_view Account;
		std::string_view Repository;
		std::int64_t     PullRequest = 0;
	};

	/// \brief Owns the private review ledger database.
	///
	/// \details The ledger is a separate file from the epoch catalog, because
	/// deleting private review state must not destroy the derived git cache.
	/// It shares the catalog's connection policy rather than carrying its own.
	class review_ledger_database final
	{
	public:
		review_ledger_database()                                                 = default;
		review_ledger_database(const review_ledger_database&)                    = delete;
		auto operator=(const review_ledger_database&) -> review_ledger_database& = delete;
		review_ledger_database(review_ledger_database&& aOther) noexcept;
		auto operator=(review_ledger_database&& aOther) noexcept
			-> review_ledger_database&;
		~review_ledger_database();

		/// \brief Opens or creates a ledger file by path, for the schema contract.
		///
		/// \details A database written by a newer build fails closed with
		/// `schema_mismatch` rather than being opened and partially understood.
		/// Migration is forward-only: there is no downgrade path, because a
		/// downgrade would have to discard data the newer version added.
		///
		/// Applies no private-store policy: the file is neither admitted nor
		/// restricted. The durable store opens through the state-directory
		/// overload.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath,
			std::stop_token              aStop)
			-> std::expected<review_ledger_database, failure>;

		/// \brief Opens or creates the durable ledger `aName` under the held `state/v1`.
		///
		/// \details The same schema preflight and forward-only migration as the
		/// path overload. The file is admitted before the preflight copies it,
		/// and opened through `open_private_store_database` ([[BPR-0883]]): by
		/// its UTF-8 name, restricted to its owner and, on Windows, bound to the
		/// handle SQLite holds. The caller keeps `aDirectory` for as long as the
		/// database is open.
		[[nodiscard]] static auto open(
			state_directory& aDirectory,
			std::string_view aName,
			std::stop_token  aStop)
			-> std::expected<review_ledger_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		/// \brief Returns the scope row id, inserting the scope if it is new.
		[[nodiscard]] auto scope_id(const review_scope& aScope)
			-> std::expected<std::int64_t, failure>;

		/// \brief Commits one file receipt within one scope.
		///
		/// \details The whole write is one statement in one implicit
		/// transaction, so a receipt is never half stored. The material travels
		/// with it, because a receipt that recorded progress without recording
		/// what it was progress *through* could not be invalidated later.
		[[nodiscard]] auto store_receipt(
			std::int64_t              aScope,
			const core::file_receipt& aReceipt)
			-> std::expected<void, failure>;

		/// \brief Reads one file receipt back, if the scope holds one.
		[[nodiscard]] auto load_receipt(
			std::int64_t               aScope,
			const core::file_identity& aFile)
			-> std::expected<std::optional<core::file_receipt>, failure>;

		/// \brief Upserts one conversation receipt in a scope.
		[[nodiscard]] auto store_conversation_receipt(
			std::int64_t                      aScope,
			const core::conversation_receipt& aReceipt)
			-> std::expected<void, failure>;

		/// \brief [[BPR-1699]]. Upserts these conversation receipts in one
		/// transaction: a refused receipt rolls back every one before it.
		[[nodiscard]] auto store_conversation_receipts(
			std::int64_t                                aScope,
			std::span<const core::conversation_receipt> aReceipts)
			-> std::expected<void, failure>;

		/// \brief Loads every conversation receipt of a scope, by conversation.
		[[nodiscard]] auto load_conversation_receipts(std::int64_t aScope)
			-> std::expected<std::vector<core::conversation_receipt>, failure>;

		[[nodiscard]] auto store_note(
			std::int64_t                                    aScope,
			const application::local_note_record&           aNote,
			std::optional<application::local_note_revision> aExpectedRevision = std::nullopt)
			-> std::expected<application::local_note_record, application::local_note_failure>;

		[[nodiscard]] auto erase_note(
			std::int64_t                     aScope,
			application::local_note_id       aNote,
			application::local_note_revision aExpectedRevision) -> std::expected<void, application::local_note_failure>;

		[[nodiscard]] auto store_last_reviewed_head(
			std::int64_t                aScope,
			const core::git_commit_oid& aHead) -> std::expected<void, failure>;

		[[nodiscard]] auto erase_last_reviewed_head(std::int64_t aScope)
			-> std::expected<void, failure>;

		[[nodiscard]] auto store_defect(
			std::int64_t                              aScope,
			const application::durable_defect_record& aDefect)
			-> std::expected<defect_store_outcome, failure>;

		[[nodiscard]] auto erase_defect(
			std::int64_t    aScope,
			core::defect_id aDefect) -> std::expected<void, failure>;

		/// \brief RFC-BPR-0089 M2. Upserts one file's generated mark.
		[[nodiscard]] auto store_generated_mark(
			std::int64_t                     aScope,
			const core::generated_file_mark& aMark) -> std::expected<void, failure>;

		/// \brief RFC-BPR-0089 M2. Removes one file's generated mark.
		[[nodiscard]] auto erase_generated_mark(
			std::int64_t               aScope,
			const core::file_identity& aFile) -> std::expected<void, failure>;

		[[nodiscard]] auto load_private(std::int64_t aScope)
			-> std::expected<application::private_ledger_observation, failure>;

		[[nodiscard]] auto admit_agent_session(
			std::int64_t                                aScope,
			const application::agent_session_admission& aAdmission)
			-> std::expected<core::agent_session, failure>;

		[[nodiscard]] auto load_agent_session_catalog(std::int64_t aScope)
			-> std::expected<application::agent_session_catalog_state, failure>;

	private:
		explicit review_ledger_database(::sqlite3* aHandle) noexcept;

		/// \brief Configures, establishes and validates a connection this owns.
		[[nodiscard]] static auto adopt(review_ledger_database aOwned, std::stop_token aStop)
			-> std::expected<review_ledger_database, failure>;

		::sqlite3* Handle = nullptr;
	};

	/// \brief Returns every column the created schema declares.
	///
	/// \details Reported as `table.column` so a prohibited name can be found
	/// wherever it is added. This exists so the storage prohibitions are
	/// checked against the schema the build actually creates rather than
	/// against the insert statements someone remembered to read.
	[[nodiscard]] auto review_ledger_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_REVIEW_LEDGER_DATABASE_HPP
