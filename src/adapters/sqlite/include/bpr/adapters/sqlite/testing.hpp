#pragma once

#if defined(BPR_SQLITE_TESTING)
#include "bpr/application/epoch_catalog_port.hpp"

namespace bpr::adapters::sqlite::detail::testing
{
// Arms one test-only interruption after a complete, fsynced bootstrap
// database exists and before its no-replace canonical publication.
void interrupt_next_bootstrap_before_publication() noexcept;

// Classifies a SQLite result code without requiring a live connection.
[[nodiscard]] auto classify_sqlite_result_code(int result_code) noexcept
    -> application::epoch_catalog_failure;
} // namespace bpr::adapters::sqlite::detail::testing
#endif
