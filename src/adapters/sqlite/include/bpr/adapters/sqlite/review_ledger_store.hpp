#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_LEDGER_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_LEDGER_STORE_HPP

#include "bpr/application/review_ledger_port.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>

namespace bpr::adapters::sqlite {
	/// \brief Describes the review ledger configuration record.
	struct review_ledger_configuration
	{
		// The owner-private application storage directory. The adapter owns the
		// fixed state/v1/review-ledger.sqlite3 path beneath it, separate from
		// the epoch catalog so that deleting private review state cannot take
		// the derived git cache with it.
		std::filesystem::path StorageRoot;

		// The review these receipts belong to. Every row is scoped by all four,
		// because a pull request number is unique only within a repository, a
		// repository only within an account, and an account only within a host.
		std::string  Host;
		std::string  Account;
		std::string  Repository;
		std::int64_t PullRequest = 0;
	};

	/// \brief Creates the durable review ledger store.
	[[nodiscard]] auto make_review_ledger_store(
		const review_ledger_configuration& aConfiguration,
		std::stop_token                    aStop = { })
		-> std::expected<
			std::unique_ptr<application::review_ledger_store_port>,
			application::ledger_store_failure>;
} // namespace bpr::adapters::sqlite

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_LEDGER_STORE_HPP
