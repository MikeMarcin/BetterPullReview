---
schema: "implement-rfc.card.v1"
id: "BPR-2272"
title: "Three drafting tests expect the implied weight that BPR-2264 now drops"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"defect","mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2271--BPR-2273--d45a84d10eb3.json","found_by":"BPR-2270 verification, 2026-11-18","related_cards":["BPR-2264","BPR-2267","BPR-2270"]}
---

# BPR-2272: Three drafting tests expect the implied weight that BPR-2264 now drops

## Observed

BPR-2270's wider run: `application_agent_chat_conversation_contract`,
`application_agent_finding_workshop_guidance_custom_contract` and
`application_agent_finding_workshop_injection_contract` still expect
`suggestion (non-blocking)` and `issue (blocking)` to survive drafting.
The owner decided decorations appear only when they add something the
label does not already say, so BPR-2264's reader drops a weight the label
implies.

## Acceptance

- AC1. The three tests expect the stripped form.
- AC2. The injection test still proves injected text is never executed or
  obeyed; only the implied-weight normalization differs from byte-exact.
- AC3. All three pass.
