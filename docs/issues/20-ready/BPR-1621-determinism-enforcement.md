---
schema: "implement-rfc.card.v1"
id: "BPR-1621"
title: "Align architecture enforcement with external determinism"
status: "Ready"
rfcs: ["RFC-BPR-0001"]
depends_on: []
priority: "P1"
metadata: {"mandate_revision":"determinism-enforcement-r1","RFC_BASE_SHA":"740ec08f84e7ee9755b2c54e0082aedcb008b3ac","reservation":"docs/issues/_reservations/BPR-1621--BPR-1621--8634d00fee9e.json","milestone_plan":{"id":"determinism-enforcement-v1","path":"docs/rfcs/plans/determinism-enforcement-v1/milestone-plan.json"}}
---

# Align architecture enforcement with external determinism

Owner mandate, 2026-10-18: “Let's check the architecture enforcement too and
clean up any stale purity restrictions that contradict the external determinism
rule.” Mandate `determinism-enforcement-r1`; bounded successor to [[BPR-1620]].

## Acceptance

- **AC1:** Audit core, architecture, dependency and platform guards. Remove
  purity-only bans on deterministic data/computation. Preserve host effects,
  implicit decision inputs, scheduling/lifecycle, Qt independence, approved
  dependencies/pins, private wrappers and licensing/link ownership. No product
  behavior or dependency adoption changes.
- **AC2:** Add positive and negative regression cases for every revised class,
  exercising the public CMake guards. Keep real clock/entropy acquisition,
  filesystem access, network/process/database/credentials/logging and host
  scheduling forbidden. State textual-enforcement limits honestly.
- **AC3:** Direct guards, focused enforcement integration and documentation gate
  and contracts pass on the candidate. Record audit, self-review, exact revisions,
  intentional commits and a durable handoff with all owned commands quiescent.
