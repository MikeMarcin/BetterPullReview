---
schema: "implement-rfc.card.v1"
id: "BPR-2000"
title: "Apply the agent accent to exactly the controls that start a run"
status: "Done"
rfcs: ["RFC-BPR-0101"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0012-agent-accent-rule.md"}
depends_on: []
acceptance: [{"criterion":"RFC-BPR-0101 AC07","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Inventory test walks every button of the composed window: run controls (agentAsk, agentSuggestion, tourSideChatSend/Update, tourDraftTour, tourStopMagic, tourProblemMagic, tourSolutionMagic, tourSuggestOutline) wear the accent and nothing else does; table in docs/ux/shared-action-buttons.md#the-agent-variant","command":"ctest --preset dev -R qt_workbench_agent_send_refusal"}},{"criterion":"RFC-BPR-0101 AC09","outcome":"pass","revision":"073cca27293966fbe3e699b1c6522f5d547b8a54","evidence":{"kind":"command","detail":"Agent accent listed on Appearance > Colours; built-ins #4A3FD6/#A3A0FF (+HC #3A2FC4/#B8B5FF) pass contrast; a changed accent reaches every accented surface and Reset restores; an older theme imports with the built-in value","command":"ctest --preset dev -R 'appearance|colour_role|theme_exchange|contrast|qt_workbench_agent_send_refusal'"}},{"criterion":"RFC-BPR-0101 AC08","outcome":"pass","revision":"c1d445720f0c8e247b56b80b2e45645850b3361f","evidence":{"kind":"observation","detail":"Owner accepted BPR-2000-agent-accent (the accent half of AC08) on frozen build /Users/jaeger/agent-tab-asks-v001.qgjq6axg from c1d445720: attended session fd2f7fdb4d5b89ca67e9793084a68d04, decision e8d273c6-c4b2-49ee-ac86-dd762243676c (event 68), fingerprint 40b96d3f8f1f. Note on the Colours filter box filed as BPR-2039. The footers half was rejected (event 54) and is carried by BPR-2038"}}]
---

# Apply the agent accent to exactly the controls that start a run

## Scope and acceptance

Implements [RFC-BPR-0101](../../rfcs/RFC-BPR-0101-agent-chat-conventions.md).

Add the agent variant and its test to docs/ux/shared-action-buttons.md, give
the accent to every control that dispatches an agent run and remove it from
every control that does not, across the Agent tab, the side chats, the tour,
the comment workshop and Review Cycles, and produce the inventory that shows
the two sets are equal.

Make the accent a palette role, Agent accent, with per-variant built-in
values and a row on Appearance › Colours under the existing rules of
RFC-BPR-0034, and have every accented surface read the role instead of a
fixed value.

Covers AC07, AC09 and the owner's look in AC08. Load frontend-design before the Qt
work.

This is part of `agent-tab-asks` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).
