---
schema: "implement-rfc.card.v1"
id: "BPR-2039"
title: "Set the Colours page's filter box apart from its description"
status: "Backlog"
rfcs: ["RFC-BPR-0034"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2038--BPR-2039--0af453354534.json","origin":"owner note on the accepted BPR-2000-agent-accent look, attended session fd2f7fdb4d5b89ca67e9793084a68d04, decision e8d273c6 (event 68)"}
depends_on: []
---

# Set the Colours page's filter box apart from its description

## Scope and acceptance

The owner accepted the agent accent ([[BPR-2000]]) and noted, on
Appearance › Colours for Dark:

> The description and filter box kind of blend together, took me a second to
> find the filter.

The line "Edits change Dark only. Each theme keeps its own colours." sits
directly on the "Filter roles" field with the same grey and no gap, so the
field reads as more description. Give the filter box its own visible field
and spacing so it is found at a glance in Light, Dark and the high-contrast
variants. The owner accepts the result on a frozen build. Load
frontend-design before the Qt work.

Not scheduled in any milestone; a follow-up to the `agent-tab-asks` look.
