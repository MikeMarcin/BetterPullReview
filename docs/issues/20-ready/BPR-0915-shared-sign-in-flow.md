---
schema: "implement-rfc.card.v1"
id: "BPR-0915"
title: "Consolidate Home and PR setup into shared sign-in"
status: "Ready"
rfcs: ["RFC-BPR-0015", "RFC-BPR-0018", "RFC-BPR-0032"]
depends_on: ["BPR-0914"]
priority: "P1"
---

# Shared sign-in flow

Owner mandate: clean up the duplicate setup flow, use a focused sign-in dialog
shared by Home and PR links, and eliminate repeated CLI clicks. Direct mode,
no sub-agents. This explicit UI amendment supersedes RFC-0032's requirement
that active sign-in be presented only inside Settings; authentication authority,
credential persistence and per-write approval remain unchanged.

## Bounded milestone and acceptance

- AC1: Home sign-in and PR setup use one focused account surface with the same
  native/device/CLI operations. Settings retains connection management.
- AC2: Use CLI starts discovery immediately. A sole relevant account advances
  to the existing trust confirmation; multiple accounts offer a meaningful
  choice, with no repeated discovery click. Cancellation defeats late results.
- AC3: A pending PR survives exact-host account choice and opens once after
  acceptance; cancellation, replacement and another host cannot consume it.
- AC4: Focused host, Qt and public-path tests and documentation checks pass.

Baseline: `bf45f693`. Prior investigation is recorded in
`build/sign-in-flow-audit.md`. No unrelated working files were present.
Reuse the existing session/device policies, staged native enrollment rollback,
and account widgets rather than introduce another authentication implementation.
Home personal PR discovery is outside this setup cleanup.
