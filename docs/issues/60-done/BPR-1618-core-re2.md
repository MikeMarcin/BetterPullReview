---
schema: "implement-rfc.card.v1"
id: "BPR-1618"
title: "Move deterministic RE2 source matching into core"
status: "Done"
rfcs: ["RFC-BPR-0077"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"core-re2-r1","reservation":"docs/issues/_reservations/BPR-1618--BPR-1618--42c00e7c1279.json","milestone_plan":{"id":"core-re2-v1","path":"docs/rfcs/plans/core-re2-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3a57eefb951d930dde90f1dbb1305577a4af5cf3","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1618/verification.md","detail":"Pinned production source audit supports scoped externally deterministic RE2/Abseil matching without worker creation; architecture and owner amendment recorded."}},{"criterion":"AC2","outcome":"pass","revision":"3a57eefb951d930dde90f1dbb1305577a4af5cf3","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1618/verification.md","detail":"Regex adapter removed; core-private factory used by Qt-free production matcher, historical epoch host and actual application composition; symbols and Qt-free linkage verified."}},{"criterion":"AC3","outcome":"pass","revision":"3a57eefb951d930dde90f1dbb1305577a4af5cf3","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1618/verification.md","detail":"Fixed spans/order/errors/limits and cancellation pass, including four concurrent callers, warmed shared programs, independent reference and three fresh processes with identical output/no stderr."}},{"criterion":"AC4","outcome":"pass","revision":"3a57eefb951d930dde90f1dbb1305577a4af5cf3","evidence":{"kind":"artifact","path":"docs/issues/_checkpoints/BPR-1618/verification.md","detail":"Core/host/public courses, boundaries/contracts, package/security/binary checks, isolated module/offline corrective checks, format22/tidy22 and documentation gate/67 tests pass; original platform/release obligations retained."}}]
---

# Move deterministic RE2 source matching into core

Owner mandate, 2026-10-18: “If RE2 isn't actually launching its own threads,
let's allow it in core too and remove the unnecessary regex adapter so long
as our usage is deterministic from the outside user's perspective.”

## Acceptance

- **AC1:** Audit the pinned RE2 2025-11-05 and Abseil 20250814.2 production
  call paths for thread creation and semantic dependence on internal clocks,
  entropy, synchronization and logging. Record the narrow approved exception
  and limitations; other effects remain outside core.
- **AC2:** The core owns the production RE2 implementation privately behind
  the existing testable interface. Qt-free core builds and composed Find use
  it; remove the regex adapter files, target and composition wiring.
- **AC3:** Preserve search syntax, Unicode, exact spans/order/errors, limits,
  cancellation and host policy. Verify repeated, warmed and concurrent use
  against fixed expected results and error codes/offsets.
- **AC4:** Focused core-only, host and composed search courses, both boundary
  checks/contracts, dependency/package checks and format/documentation gates
  pass. Preserve prior native/release obligations; no performance gain claimed.
