---
schema: "implement-rfc.card.v1"
id: "BPR-2005"
title: "Show what a running turn is doing, its helpers and when it goes quiet"
status: "Done"
rfcs: ["RFC-BPR-0103"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0017-activity-block-and-quiet-notice.md"}
depends_on: ["BPR-2004"]
acceptance: [{"criterion":"RFC-BPR-0103 AC05","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"Held clock: no notice at 119 s, notice at 120 s with both choices, Keep waiting leaves the run and returns after 120 s more, a frame clears it, Stop and keep what arrived stops","command":"ctest --preset dev -R 'qt_agent_activity_view|core_agent_activity'"}},{"criterion":"RFC-BPR-0103 AC06","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"The line names the file read and the command run; last activity restarts on a frame; the composed course saw it live","command":"ctest --preset dev -R 'qt_agent_activity_view|application_agent_chat_helpers_public_path'"}},{"criterion":"RFC-BPR-0103 AC08","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"command","detail":"The tour side chat shows the helper row, the doing line with last activity, the quiet notice, and folds after the turn","command":"ctest --preset dev -R qt_tour_side_chat"}},{"criterion":"RFC-BPR-0103 AC09","outcome":"pass","revision":"12e9c6fb441601e4a55edbc3cbb7c7fe98eadffe","evidence":{"kind":"observation","detail":"Owner accepted BPR-2005-running (eead98a1), BPR-2005-quiet (25cac43a) and BPR-2005-ended (d19d4d2d), attended session d2cc338937915480c469bdbd1ffd7f89, frozen build of 12e9c6fb4, no notes"}}]
---

# Show what a running turn is doing, its helpers and when it goes quiet

## Scope and acceptance

Implements
[RFC-BPR-0103](../../rfcs/RFC-BPR-0103-agent-work-is-visible-and-a-turn-ends-when-the-work-does.md).

Present the running block (what the agent is doing, elapsed time, time since
the last frame, helper rows, Stop), fold it under the answer after the turn,
and show the 120-second quiet notice with its two choices, in the Agent tab
and the tour side chat.

Covers AC05, AC06, AC08 and the owner's look in AC09. Load frontend-design
before the Qt work.

This is part of `agent-work-visible` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
