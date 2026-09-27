---
schema: "implement-rfc.card.v1"
id: "BPR-1219"
title: "Qualify structured diagnostics and reconcile RFC acceptance"
status: "Parked"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1218"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7ab119933439878d858100410083080ab4d1ea7b","evidence":{"kind":"observation","detail":"Native macOS/Windows/Linux developer logging, lifecycle and controls pass; universal Mac unpublished package, Quill provenance, explicit installer/container/crash limits in docs/verification/RFC-BPR-0027.md."}},{"criterion":"AC2","outcome":"pending","revision":"7ab119933439878d858100410083080ab4d1ea7b","evidence":{"kind":"observation","detail":"Three protected exact-source attended candidates prepared at cursor19; actual human verdicts remain required. No live sign-in."}},{"criterion":"AC3","outcome":"pending","revision":"7ab119933439878d858100410083080ab4d1ea7b","evidence":{"kind":"observation","detail":"All nine criteria reconciled in BPR-0601; RFC/index remain open and accepted M5 main landing awaits actual acceptance."}}]
---

# Qualify structured diagnostics and reconcile RFC acceptance

Reconcile all RFC-BPR-0027 criteria and platform, dependency, packaging, native and attended obligations against exact candidates; preserve gaps until evidence exists.

## Acceptance

- AC1: Run focused integration and native logging/lifecycle/control courses on each claimed platform; bind dependency and package provenance.
- AC2: Prepare required attended review with exact candidates and pending decisions; no live sign-in without separate authority.
- AC3: Reconcile BPR-0603 and BPR-0601, RFC/index and all full criteria, land accepted scope on main.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.


## Native evidence and attended handoff

Final source `7ab119933439878d858100410083080ab4d1ea7b`.
[Verification](../../verification/RFC-BPR-0027.md) and
[durable checkpoint](../10-backlog/BPR-0601-close-rfc-0027.md#m5-checkpoint--human-pending-writer-released)
bind the actual native/course/package outcomes and preserve all limits.
Three attended criteria are prepared in session
`c1565558e5fd3fdc277e62622604873b`, cursor 19. No verdict supplied; M5 and the
full RFC remain unaccepted, so main integration remains pending. Source ownership
is released to the coordinator after the committed checkpoint; only the handed
off local review server remains active.
