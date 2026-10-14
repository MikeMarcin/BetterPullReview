---
schema: "implement-rfc.card.v1"
id: "BPR-1498"
title: "Compute Review Cycle suggestion hunks with dtl"
status: "In Progress"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1481"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r4","source":"Owner Accept 2026-10-13: extend dtl's core approval from per-line intraline to the bounded Review Cycle suggestion-hunk matcher (512-edit cap kept)"}
---

# BPR-1498: Compute Review Cycle suggestion hunks with dtl

The owner Accepted on 2026-10-13 replacing the bounded first-party Myers in
`matched_lines` (`src/core/src/review_cycle_suggestion.cpp`, capped at 512
edits) with dtl, extending dtl's core approval beyond per-line intraline
comparison to this one bounded use. dtl stays out of whole-file line matching.

## Acceptance

- **AC1:** The owner's answer is recorded in RFC-BPR-0082 by the amendment
  convention, and both `AGENTS.md` files, `cmake/VerifyCoreBoundary.cmake`,
  the system architecture and the architecture-boundary rule name exactly the
  two approved dtl uses; the rule allows dtl in exactly those two files, with
  a contract test.
- **AC2:** `matched_lines` takes its matches from dtl's minimal edit script,
  synchronously in core, with inputs bounded as before, the 512-edit cap kept
  and dtl's safe size limits respected as `diff_intraline.cpp` does.
- **AC3:** Every existing Review Cycle suggestion test returns identical
  results; `core-only`'s Review Cycle and diff suites and both
  architecture-boundary checks pass.
