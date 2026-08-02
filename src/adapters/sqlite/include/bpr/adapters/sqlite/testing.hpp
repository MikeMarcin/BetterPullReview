#ifndef BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
#define BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP

#if defined(BPR_SQLITE_TESTING)
	#include "bpr/application/epoch_catalog_port.hpp"

namespace bpr::adapters::sqlite::detail::testing {
	// Arms one test-only interruption after a complete, fsynced bootstrap
	// database exists and before its no-replace canonical publication.
	/// \brief Performs the interrupt next bootstrap before publication operation.
	void interrupt_next_bootstrap_before_publication() noexcept;

	// Classifies a SQLite result code without requiring a live connection.
	/// \brief Computes the classify sqlite result code result.
	[[nodiscard]] auto classify_sqlite_result_code(int aResultCode) noexcept
		-> application::epoch_catalog_failure;
} // namespace bpr::adapters::sqlite::detail::testing
#endif

#endif // BPR_SRC_ADAPTERS_SQLITE_INCLUDE_BPR_ADAPTERS_SQLITE_TESTING_HPP
