---
schema: "implement-rfc.card.v1"
id: "BPR-1881"
title: "Tour tab: a call to action to set up the agent, and agent features in one colour"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"kind":"feature","found_by":"owner, 2026-11-06, in coordinator chat","related_cards":["BPR-1878","BPR-1879"]}
acceptance: []
---

# BPR-1881: Tour tab: a call to action to set up the agent, and agent features in one colour

## Owner request (2026-11-06)

The Tour tab's AI drafting uses the agent that the Agent tab sets up. Without
that setup (agent, Repository, Check it works), the agent has much less
context. The owner does not want the wizard to open by itself, on the Tour tab
or by sending the reviewer to the Agent tab, because either would be
disorienting. Instead:

1. When the agent is not yet set up, show a call to action at the top of the
   Tour tab saying to set up the agent, with one click that takes the reviewer
   to the Agent tab's setup.
2. Colour-code every agent feature button on the Tour tab. Use the same colour
   as the call to action.
3. Disable those buttons until setup is finished. A disabled button's tooltip
   says why.
4. Agent features keep that colour after setup too, so the reviewer can tell
   at a glance what is ordinary and what is agentic.

## Acceptance

- AC1. With no finished agent setup, the call to action sits at the top of
  the Tour tab. Its click opens the Agent tab at its setup. It disappears once
  setup is finished.
- AC2. Every agent-driven control on the Tour tab, including the BPR-1878
  bottom picker bar where it fits, uses one agent accent colour that matches
  the call to action. It is legible in light and dark themes and meets
  contrast requirements.
- AC3. Before setup those controls are disabled, with a tooltip naming the
  reason. After setup they are enabled and keep the accent.
- AC4. Owner look: an attended Accept/Reject on a frozen build that shows
  both states.
- AC5. `python3 scripts/verify_documentation.py --root .` passes.
