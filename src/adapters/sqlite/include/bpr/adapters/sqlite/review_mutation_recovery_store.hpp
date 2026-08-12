#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_MUTATION_RECOVERY_STORE_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_REVIEW_MUTATION_RECOVERY_STORE_HPP

#include "bpr/application/review_mutation_recovery_port.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>

namespace bpr::adapters::sqlite {
	struct review_mutation_recovery_configuration
	{
		std::filesystem::path StorageRoot;
		std::string           Host;
		std::string           Account;
		std::string           Repository;
		std::int64_t          PullRequest = 0;
	};

	[[nodiscard]] auto make_review_mutation_recovery_store(
		const review_mutation_recovery_configuration& aConfiguration)
		-> std::expected<
			std::unique_ptr<application::review_mutation_recovery_port>,
			application::review_mutation_recovery_failure>;
} // namespace bpr::adapters::sqlite

#endif
