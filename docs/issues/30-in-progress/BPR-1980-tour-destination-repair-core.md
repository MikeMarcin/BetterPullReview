---
schema: "implement-rfc.card.v1"
id: "BPR-1980"
title: "Plan and apply exact tour destination repairs in the core"
status: "In Progress"
rfcs: ["RFC-BPR-0041"]
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"milestone_plan":{"id":"tour-destination-repairs-v1","path":"docs/rfcs/plans/tour-destination-repairs-v1/milestone-plan.json"},"mandate_revision":"tour-destination-repairs-r1","reservation":"docs/issues/_reservations/BPR-1979--BPR-1980--d8b8b8649256.json"}
acceptance: []
---

# BPR-1980: Plan and apply exact tour destination repairs in the core

Implementation prerequisite of [[BPR-1979]], mandate `tour-destination-repairs-r1`.
This card supplies the deterministic policy and edit seam; the parent retains all
editor, publication, documentation and mixed-publication acceptance.

## Acceptance

- AC1. The core reports every incompatible stop by stable identity, title and typed
  reason, with explicit exact-head re-pin, nearest-hunk relocation and qualified
  file-subject repair availability. Changed, ambiguous, missing or unavailable
  mappings never offer an exact re-pin. Unknown/denied capabilities grant no write.
- AC2. Accepted-stop repairs use the ordinary revision-bound local edit protocol,
  preserve narrative and stop identity, retain readiness unless changed line
  numbers are referenced in its explanation, and can be undone/restored. Re-pin
  consumes the existing unchanged-material mapping at the observed head; relocation
  uses the same nearest-hunk clamp as agent proposals, including provider context.
- AC3. A core composition with a historical exact stop, an off-hunk stop and native
  stops exposes both blockers, applies the two repairs, then assesses as one review.
  Negative and boundary checks, core-only tests, both architecture checks, formatting
  and documentation gates pass against the candidate. This supplies no UI or live
  host acceptance for the parent.
