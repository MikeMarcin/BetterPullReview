---
schema: "implement-rfc.card.v1"
id: "BPR-1445"
title: "Verify and close RFC-BPR-0081"
status: "Backlog"
rfcs: ["RFC-BPR-0081"]
depends_on: ["BPR-1440", "BPR-1441", "BPR-1442", "BPR-1443", "BPR-1444"]
priority: "P2"
platforms: ["mac", "win", "linux"]
closeout_for: ["RFC-BPR-0081"]
metadata: {"mandate_revision":"cpp-identifier-parser-evaluation-r1","milestone_plan":{"id":"cpp-identifier-parser-evaluation-v1","path":"docs/rfcs/plans/cpp-identifier-parser-evaluation-v1/milestone-plan.json"}}
---

# BPR-1445: Verify and close RFC-BPR-0081

Mandate `cpp-identifier-parser-evaluation-r1` authorizes the evaluation in
[RFC-BPR-0081](../../rfcs/RFC-BPR-0081-cpp-identifier-parser-evaluation.md), adopted from RFC-DRAFT-0021.

RFC base: `530d92eaff1578413c0e669f476e740387441152`.

## Acceptance

- **AC1:** Every RFC criterion AC01 to AC07 has direct evidence on the exact
  candidate, reconciled against RFC_BASE_SHA, and the closeout checklist below
  agrees with it.
- **AC2:** The recorded outcome follows mechanically from the decision rules
  with all three platforms measured, and any follow-up card it requires cites the
  report. The RFC, its index row and this card agree.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** With the evaluation option on, the harness runs the baseline and each T pin over the whole corpus through the unchanged public classifier path. The committed report shows whether each gate passed, with its commands, counts and raw results. With the option off, no target acquires or links tree-sitter, as the configure log and a link-dependency check show. | Open |
| **AC02:** The labels are committed before any candidate results; third-party sources are named by digest. For every run, the report gives false positives and false negatives per category, and marks each of H1–H8 as confirmed or refuted on the baseline and on T. | Open |
| **AC03:** The macOS, Windows and Linux measurements cover every size class and every item under "Measurements", and they measure the baseline in the same runs. | Open |
| **AC04:** A test stops the refinement partway through and observes `cancelled` with no spans, and the syntax fuzz target runs in the fuzz build. Both remain after the evaluation. The report gives sanitizer, fuzz, deep-nesting and 8 MiB results for the baseline and for T. | Open |
| **AC05:** The recorded outcome (adopt, extend, keep, or eligible pending Q1) follows mechanically from "Decision rules" and states when to run the evaluation again. Any deviation is recorded as an owner decision. | Open |
| **AC06:** The report records the licence of each shipped file (runtime, generated parser and scanner) and the notice, SPDX and corresponding-source changes adoption would need. It also records the archive digests, the size added per slice and the build cost. | Open |
| **AC07:** On the shipping configuration, the evaluation work leaves spans byte-identical over the corpus compared with the revision before it, and `core-only` and both architecture-boundary checks pass. | Open |

## Current frontier

Imported; no evaluation work has begun.
