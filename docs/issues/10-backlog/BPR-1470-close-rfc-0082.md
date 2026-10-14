---
schema: "implement-rfc.card.v1"
id: "BPR-1470"
title: "Verify and close RFC-BPR-0082"
status: "Backlog"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1465", "BPR-1466", "BPR-1467", "BPR-1468", "BPR-1469"]
priority: "P2"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0082"]
metadata: {"mandate_revision":"diff-engine-evaluation-r1"}
---

# BPR-1470: Verify and close RFC-BPR-0082

Mandate `diff-engine-evaluation-r1` authorizes the evaluation in
[RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md), adopted from RFC-DRAFT-0018.

RFC base: `cc9e9376e1fc02e2cdf3154fa56dfcc14991a8d5`.

Reservation: `docs/issues/_reservations/BPR-1465--BPR-1470--7021f30a9b8c.json`.

## Acceptance

- **AC1:** Every RFC criterion AC01 to AC07 has direct evidence on the exact
  candidate, reconciled against RFC_BASE_SHA, and the closeout checklist below
  agrees with it.
- **AC2:** The recorded outcome follows mechanically from the decision rules
  with all three platforms measured, open owner questions are recorded as
  "eligible pending Qn" rather than adopted, and any follow-up card cites the
  report. The RFC, its index row and this card agree. *Amended 2026-10-13
  (mandate `diff-engine-evaluation-r4`, owner delegation): "all three
  platforms measured" reads as macOS measured and Linux and Windows qualified
  by BPR-1468 and BPR-1469, as RFC AC02 is amended.*

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** The opt-in harness runs every candidate–component pairing and the baseline over the full suite list and corpus through first-party validation. The committed report shows each gate's pass or fail with its commands and counts. | Open |
| **AC02:** Measurements on macOS, Linux and Windows cover every size class and the measurement list under "Corpus and measurement", with the baseline measured in the same runs and conditions. *Amended 2026-10-13 under the owner's delegation: macOS carries the measurements; on Linux and Windows the adopted stack is qualified instead (shared-library xdiff, dtl and rapidfuzz-cpp built; the diff suites pass; the BPR-1485 equivalence tool reproduces the macOS digests and GitHub agreement 78 of 78; the package audits pass), because the baseline is deleted* (see "Owner delegation on Linux and Windows qualification" above). | Open |
| **AC03:** GitHub agreement and comment-range flips are reported per candidate against recorded provider patches. This includes the BPR-0625 and BPR-0719 inputs and a lone-CR case showing LF-only line identity. | Open |
| **AC04:** For each candidate the report records licence per shipped file, linkage, obligations, platform and Git-version dependence, and the owner answer it needs. A package and boundary check shows that no shipped target links an evaluation candidate. | Open |
| **AC05:** Each component's recorded decision follows mechanically from "Decision rules", and any deviation is recorded as an owner decision. | Open |
| **AC06:** Each intraline candidate that passes the mechanical gates receives an owner Accept or Reject on its render set, in its own attended-review session. | Open |
| **AC07:** Existing engine entry points produce identical results before and after the evaluation work. `core-only` and both architecture-boundary checks pass. | Open |

## Current frontier

Imported; no evaluation work has begun.
