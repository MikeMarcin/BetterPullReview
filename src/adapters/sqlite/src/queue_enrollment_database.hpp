#ifndef BPR_SRC_ADAPTERS_SQLITE_SRC_QUEUE_ENROLLMENT_DATABASE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_SRC_QUEUE_ENROLLMENT_DATABASE_HPP

#include "sqlite_connection.hpp"

#include "bpr/application/review_queue_enrollment.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

struct sqlite3;

namespace bpr::adapters::sqlite::detail {
	/// \brief The schema version this build writes and validates.
	///
	/// \details Version two adds the durable observer-rule authority. Version one
	/// retained only the exact rule copied into an enrollment reason, which was
	/// enough to explain a row but could not restore or delete the user's rule.
	inline constexpr std::int64_t queue_enrollment_schema_version = 2;

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

		/// \brief Opens or creates the store at one path.
		[[nodiscard]] static auto open(
			const std::filesystem::path& aPath, std::stop_token aStop)
			-> std::expected<queue_enrollment_database, failure>;

		[[nodiscard]] auto handle() const noexcept -> ::sqlite3*;

		/// \brief Writes one enrollment, replacing any it already holds.
		///
		/// \details One statement in one implicit transaction, so an
		/// enrollment is never half stored — a record with a departure reason
		/// but no membership would be worse than no record at all.
		[[nodiscard]] auto store(const application::enrollment_record& aRecord)
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

	private:
		explicit queue_enrollment_database(::sqlite3* aHandle) noexcept;

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
