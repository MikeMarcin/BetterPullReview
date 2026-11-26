#include "bpr/core/agent_live_turn.hpp"

#include <utility>

namespace bpr::core {
	auto reduce_agent_live_turn(const agent_live_turn& aTurn, const agent_live_observation& aSeen) -> agent_live_step
	{
		auto step         = agent_live_step{ };
		step.Turn         = aTurn;
		auto& turn        = step.Turn;
		step.PhaseChanged = aSeen.Phase != aTurn.Phase;
		step.Began        = aTurn.Phase == agent_live_phase::idle && aSeen.Phase == agent_live_phase::preparing;
		step.Ended        = aTurn.Phase != agent_live_phase::idle && aSeen.Phase == agent_live_phase::idle;
		turn.Phase        = aSeen.Phase;
		if (step.Began) {
			// A new question's turn: nothing of the last run is carried.
			turn = agent_live_turn{ .Phase = aSeen.Phase };
		}
		if (aSeen.Phase == agent_live_phase::idle) {
			// An ended turn keeps what it had for its ending, with the words
			// it ended on; an idle tab has nothing more to fold.
			if (step.Ended) {
				turn.Answer = aSeen.Written;
			}
			return step;
		}
		step.SteeredChanged = aSeen.Steered != turn.Steered;
		turn.Steered        = aSeen.Steered;
		turn.Answer         = aSeen.Written;
		turn.FilesOnly      = aSeen.FilesOnly;
		if (aSeen.Phase == agent_live_phase::preparing) {
			turn.Stage = aSeen.WorkspaceChecking ? agent_live_stage::checking : agent_live_stage::preparing;
			turn.Latest.clear();
			return step;
		}
		turn.Stage  = aSeen.Written.empty() ? agent_live_stage::thinking : agent_live_stage::writing;
		turn.Latest = aSeen.Written.empty() ? aSeen.LatestLine : std::string{ };
		turn.Cost   = aSeen.Cost;
		// What the runtime says it is doing, only while it says it.
		turn.Activity          = aSeen.Activity;
		turn.ActivityStartedMs = aSeen.Activity ? aSeen.ActivityStartedMs : 0;
		return step;
	}

	auto decide_agent_live_end(const agent_live_end_facts& aFacts) -> agent_live_end
	{
		if (aFacts.Outcome == agent_turn_outcome::answered) {
			if (!aFacts.FailedWrites.empty()) {
				return agent_live_end::writes_failed;
			}
			return aFacts.Written.empty() ? agent_live_end::answered_silently : agent_live_end::answered;
		}
		// [[BPR-2004]], RFC-BPR-0103. What arrived is kept when the runtime
		// ended under its helpers, or when the reviewer stopped a turn that
		// had written.
		if (aFacts.HelpersUnfinished) {
			return agent_live_end::kept_on_helpers;
		}
		if (aFacts.Outcome == agent_turn_outcome::stopped && !aFacts.Kept.empty()) {
			return agent_live_end::kept_on_stop;
		}
		return agent_live_end::failed;
	}
}
