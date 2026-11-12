---
schema: "implement-rfc.card.v1"
id: "BPR-2121"
title: "Agent setup wizard shows the agent list live"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2121--BPR-2121--3e033025f0bc.json","source_commits":["9f6d8767d","782c8a358"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"782c8a358","evidence":{"kind":"command","detail":"application_agent_chat_setup_list_contract: with the wizard open, agents added, edited and removed in Settings appear, change and leave the Agent step; failed on the old sources with settings-agent-in-setup=false."}},{"criterion":"AC2","outcome":"pass","revision":"782c8a358","evidence":{"kind":"command","detail":"An agent added through the wizard's Custom agent reaches Settings; core_agent_entry, core_agent_setup, application_agent_setup_host, qt_agent_setup_view, architecture_boundaries, format and tidy pass."}}]
---

# Agent setup wizard shows the agent list live

Owner report: an agent added in agent settings did not appear in the agent
setup wizard. The wizard's Agent step drew only per-kind detection rows, fed
by the preferred entry alone. Core `agent_setup_choices()` now builds the step
from every list entry, then found-but-unadded popular agents, then an unused
saved harness; removing the chosen entry re-detects.

## Acceptance

- AC1. With the wizard open, adding, editing or removing an agent in Settings
  updates the Agent step, keeping the open step and pick.
- AC2. An agent added in the wizard appears in Settings.
