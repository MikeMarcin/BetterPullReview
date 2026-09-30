#include "bpr/core/review_cycle_import.hpp"

namespace bpr::core {
	auto summarize_import(const cycle_decoded_round& aRound) -> cycle_import_summary
	{
		return {
			.Digest         = aRound.Digest,
			.ForeignCycle   = aRound.Payload.CycleId,
			.ForeignRound   = aRound.Payload.RoundId,
			.ForeignProject = aRound.Payload.Project.Id,
		};
	}
}
