---
schema: "implement-rfc.card.v1"
id: "BPR-1230"
title: "Windows: github_review_comment_permalink fails at canonical RFC0022 path escaping, contained and uncontained"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-main-win station run, 2026-09-29","reservation":"docs/issues/_reservations/BPR-1229--BPR-1237--8ddd5fd98cf2.json"}
---

# BPR-1230: Windows: github_review_comment_permalink fails at canonical RFC0022 path escaping, contained and uncontained

Found by bpr-main-win on 2026-09-29: Windows 11 Home 10.0.26200, MSVC 19.51,
Qt 6.11.2, `build/windows-dev` Debug with warnings as errors, at `22d18416`
with `537f718d`'s build fixes and again at later `main`. The station logs
are in `C:\Users\mikem\bpr-main-helpers\run\station-0929\` and are not committed.

## Observation

`github_review_comment_permalink` fails on Windows. Contained, it stops at
`FAILED: publication SQLite journal opens`. Run directly, uncontained, it gets
further: it prints `recorded-contract-refusal-courses:29`, then
`FAILED: range/historical/plain-view shares canonical RFC0022 path escaping`,
exit 1. So the uncontained failure is an assertion about path escaping, which
Windows fails. It is not a boundary effect.

The source, `tests/github_contract/review_comment_permalink_tests.cpp`, last
changed in `24b7f66a` (2026-09-27). Its target needed
`BetterPullReview::Platform` to compile on Windows, which `537f718d` added.

## Scope

- Find which escaping differs on Windows. Plausible, not measured: a native
  path separator or drive letter reaching the escaped path.
- Fix it in the layer that owns the escaping. Decide the row's containment
  separately: the store open fails contained as the other scratch-root store
  rows do.

## Acceptance

- The row passes on Windows, and the escaping assertion passes unchanged.
- macOS and Linux still pass.
