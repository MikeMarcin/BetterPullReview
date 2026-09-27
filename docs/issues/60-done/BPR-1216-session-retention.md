---
schema: "implement-rfc.card.v1"
id: "BPR-1216"
title: "Bound session diagnostic files and retention"
status: "Done"
rfcs: ["RFC-BPR-0027"]
depends_on: ["BPR-1215"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0027-advanced","path":"docs/rfcs/plans/rfc-bpr-0027-advanced/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3381a02c5a78f4611fd7ab593cc0f6086ec32208","evidence":{"kind":"observation","detail":"Native retention tests enforce per-file/aggregate/count/age limits, active session protection, unsafe-file refusal and four-process same-directory coordination; final focused integration passes 15/15.","reference":"docs/verification/RFC-BPR-0027.md"}},{"criterion":"AC2","outcome":"pass","revision":"3381a02c5a78f4611fd7ab593cc0f6086ec32208","evidence":{"kind":"observation","detail":"Real native short-write/RLIMIT_FSIZE and rotation obstruction fail safely; partial tails are isolated and omitted. Public startup cancellation, full/unavailable logging, shutdown, maintenance preview/export/clear and profile/window courses pass. Native Windows/Linux, ENOSPC-filled disk and human full-RFC observations are not claimed.","reference":"docs/verification/RFC-BPR-0027.md"}}]
---

# Bound session diagnostic files and retention

Replace the fixed-file ceiling with safely owned per-session files and coordinated rotation, aggregate and age retention. Preserve lifecycle and explicit failures.

## Acceptance

- AC1: Enforce file, aggregate, age and session bounds with safe ownership and concurrent-session coordination.
- AC2: Test disk/rotation/partial-tail failures and startup/cancel/shutdown availability through public composition.

Mandate r2 (2026-09-26): implement the full advanced phase and land each accepted milestone on main. No live sign-in or PR mutation is authorized.
