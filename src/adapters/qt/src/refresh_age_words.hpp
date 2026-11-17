#ifndef BPR_SRC_ADAPTERS_QT_SRC_REFRESH_AGE_WORDS_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_REFRESH_AGE_WORDS_HPP

/// \file
/// \brief How the window says an age: "just now", "2 min ago", "3 hr ago",
/// "2 days ago".
///
/// \details [[BPR-1575]] set the words for the refresh note; [[BPR-2244]]
/// gives the Commits tab's rows the same ones. The core rounds the age
/// (`core::refresh_age_at`) to a clock the host reads.

#include "bpr/core/refresh_feedback.hpp"

#include <QString>

namespace bpr::qt {
	[[nodiscard]] auto refresh_age_words(core::refresh_age aAge) -> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_SRC_REFRESH_AGE_WORDS_HPP
