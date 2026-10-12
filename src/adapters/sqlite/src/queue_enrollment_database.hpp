#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_QUEUE_ENROLLMENT_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_QUEUE_ENROLLMENT_DATABASE_HPP

#include "sqlite_connection.hpp"
#include "state_directory.hpp"

#include "bpr/application/review_queue_enrollment.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Version two adds the durable observer-rule authority. Version
	/// three adds only reviewer-selected landing preferences; derived filtered
	/// membership remains absent from that table.
	/// Five since [[BPR-0660]]: the remembered enterprise servers.
	/// Six since [[BPR-0681]]: notification enablement and pause.
	/// Seven since [[BPR-0989]]: discovery reasons, follow and review on each
	/// enrollment, the `undiscovered` departure and the `detail_unknown` bucket.
	inline constexpr std::int64_t queue_enrollment_schema_version = 8;

	/// \brief Owns the durable queue enrollment database.
	///
	/// \details A separate file from both the epoch catalog and the review
	/// ledger, for the same reason they are separate from each other: deleting
	/// one kind of local state must not destroy another. A reviewer clearing
	/// their queue should not lose their reading progress.
	class queue_enrollment_database final
	{
	public:
		queue_enrollment_database()                                 = default;
		queue_enrollment_database(const queue_enrollment_database&) = delete;
		auto operator=(const queue_enrollment_database&)
			-> queue_enrollment_database& = delete;
		queue_enrollment_database(queue_enrollment_database&& aOther) noexcept;
		auto operator=(queue_enrollment_database&& aOther) noexcept
			-> queue_enrollment_database&;
		~queue_enrollment_database();

		/// \brief Opens or creates a database file by path, for the schema contract.
		/// \details Applies no private-store policy: the file is neither
		/// admitted nor restricted. The durable store opens through the
		/// state-directory overload.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath, std::stop_token aStop)
			-> std::expected<queue_enrollment_database, failure>;

		/// \brief Opens or creates the durable store file `aName` under the held `state/v1`.
		/// \details Through `open_private_store_database` ([[BPR-0883]]): the
		/// file is admitted, opened by its UTF-8 name, restricted to its owner
		/// and, on Windows, bound to the handle SQLite holds. The caller keeps
		/// `aDirectory` for as long as the database is open.
		[[nodiscard]] static auto open(
			state_directory& aDirectory, std::string_view aName, std::stop_token aStop)
			-> std::expected<queue_enrollment_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		/// \brief Writes one enrollment, replacing any it already holds.
		///
		/// \details One statement in one implicit transaction, so an
		/// enrollment is never half stored — a record with a departure reason
		/// but no membership would be worse than no record at all.
		[[nodiscard]] auto store(const application::enrollment_record& aRecord)
			-> std::expected<void, failure>;

		/// \brief [[BPR-0989]]. Writes several enrollments in one transaction.
		[[nodiscard]] auto store_all(std::span<const application::enrollment_record> aRecords, std::stop_token aStop)
			-> std::expected<void, failure>;

		/// \brief Reads every enrollment back, departed ones included.
		///
		/// \details Departed records are returned because the reviewer is owed
		/// the reason a pull request left. Dropping them on load would make a
		/// closed review indistinguishable from one that was never enrolled.
		[[nodiscard]] auto load_all()
			-> std::expected<std::vector<application::enrollment_record>, failure>;

		[[nodiscard]] auto store_rule(const core::observer_rule& aRule)
			-> std::expected<void, failure>;
		[[nodiscard]] auto erase_rule(const core::observer_rule& aRule)
			-> std::expected<void, failure>;
		[[nodiscard]] auto apply_rule_change(
			const application::queue_rule_intent&           aIntent,
			std::span<const application::enrollment_record> aEnrollments,
			std::stop_token                                 aStop)
			-> std::expected<void, failure>;
		[[nodiscard]] auto load_rules()
			-> std::expected<std::vector<core::observer_rule>, failure>;
		[[nodiscard]] auto store_preferences(
			const application::queue_presentation_preferences& aPreferences)
			-> std::expected<void, failure>;
		[[nodiscard]] auto load_preferences()
			-> std::expected<application::queue_presentation_preferences, failure>;

		/// \brief Saves one repository; saving it again is not a second row.
		[[nodiscard]] auto store_saved_repository(
			const application::saved_repository& aRepository)
			-> std::expected<void, failure>;
		[[nodiscard]] auto erase_saved_repository(
			const application::saved_repository& aRepository)
			-> std::expected<void, failure>;
		[[nodiscard]] auto load_saved_repositories()
			-> std::expected<std::vector<application::saved_repository>, failure>;
		/// \brief [[BPR-0660]]. One row per hostname; remembering again replaces the account.
		[[nodiscard]] auto store_saved_server(const application::saved_server& aServer)
			-> std::expected<void, failure>;
		[[nodiscard]] auto erase_saved_server(const application::saved_server& aServer)
			-> std::expected<void, failure>;
		[[nodiscard]] auto load_saved_servers()
			-> std::expected<std::vector<application::saved_server>, failure>;

	private:
		explicit queue_enrollment_database(::sqlite3* aHandle) noexcept;

		/// \brief Preflights, configures, establishes and validates a connection this owns.
		[[nodiscard]] static auto adopt(queue_enrollment_database aOwned, std::stop_token aStop)
			-> std::expected<queue_enrollment_database, failure>;

		::sqlite3* Handle = nullptr;
	};

	/// \brief Returns every column the created schema declares.
	///
	/// \details Reported as `table.column`, so a prohibited name can be found
	/// wherever it is added. This checks the storage prohibitions against the
	/// schema the build actually creates rather than against the insert
	/// statements somebody remembered to read.
	[[nodiscard]] auto queue_enrollment_columns(::sqlite3* aDatabase)
		-> std::expected<std::vector<std::string>, failure>;
} // namespace bpr::adapters::sqlite::detail

#endif // BPR_SRC_ADAPTERS_SQLITE_SRC_QUEUE_ENROLLMENT_DATABASE_HPP
