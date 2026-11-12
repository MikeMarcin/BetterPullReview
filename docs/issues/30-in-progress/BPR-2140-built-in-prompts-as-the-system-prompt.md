---
schema: "implement-rfc.card.v1"
id: "BPR-2140"
title: "Built-in prompts reach the agent as its system prompt, verified, with a paste fallback"
status: "In Progress"
rfcs: ["RFC-BPR-0107"]
depends_on: []
priority: "P1"
platforms: []
metadata: {"mandate_revision": "v0-5-0-prerelease-targets-r1", "reservation": "docs/issues/_reservations/BPR-2138--BPR-2142--6b47e1d85608.json", "origin": "owner agent launch policy, 2026-11-12 (coordinator brief), corporate wrapper test results and their correction of 2026-11-11", "criteria": ["AC09"]}
---

# Built-in prompts reach the agent as its system prompt, verified, with a paste fallback

## Scope and acceptance

P02 settled by the owner (2026-11-12). Built-in prompt instructions go in
Claude's inline `--append-system-prompt "<text>"` (never the `-file` form,
which the corporate wrapper drops) and the run's material in the message;
Codex `exec` gets `-c developer_instructions=…` (verified on 0.167 with a
canary); the App Server pastes. Instructions past the Windows command-line
bound are pasted. A per-agent switch (System prompt / In the message) is
the fallback for a wrapper that drops it. A canary course with the installed
Claude shows a rule given only in the system prompt applied. The wrapper's
own behavioural confirmation of the inline route is pending the owner's
rerun.

Acceptance: AC09 as amended on 2026-11-12.

## Progress (2026-11-12)

Partly built; see the [RFC-BPR-0101 ledger](../../verification/RFC-BPR-0101.md#agent-launch-policy-owner-2026-11-12-bpr-2138-bpr-2141)
for what passes and what remains (its "Not done" list, and the RFC-BPR-0107
ledger's "Pending" list for the prompts still pasted).
