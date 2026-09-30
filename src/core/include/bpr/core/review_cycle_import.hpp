#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IMPORT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_IMPORT_HPP

#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_round.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Rounds received from elsewhere (RFC-BPR-0056, BPR-1160, BPR-1162).
/// \details An imported round is kept apart from local authoring, in a
/// private quarantine of the profile's cycle store: its canonical payload is
/// immutable, its sender's identities (cycle, round, items, suggestions and
/// project) are kept exactly as sent and never reinterpreted as local ones,
/// and nothing in it is applied to any source. Importing the same round again
/// finds the same record.
namespace bpr::core {
	using cycle_import_id = strong_id<struct cycle_import_id_tag>;

	/// \brief What one imported round's record says about it, beside its
	/// immutable payload.
	struct cycle_import_summary
	{
		cycle_import_id    Id;
		cycle_round_digest Digest;
		/// The sender's wire identities, exactly as sent.
		std::string ForeignCycle;
		std::string ForeignRound;
		std::string ForeignProject;
		auto        operator<=>(const cycle_import_summary&) const = default;
	};
	/// \brief The summary a payload gives an import record.
	[[nodiscard]] auto summarize_import(const cycle_decoded_round& aRound) -> cycle_import_summary;
}

#endif
