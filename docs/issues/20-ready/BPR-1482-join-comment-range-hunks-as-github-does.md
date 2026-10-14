---
schema: "implement-rfc.card.v1"
id: "BPR-1482"
title: "Join comment-range hunks one unchanged line apart, as GitHub does"
status: "Ready"
rfcs: ["RFC-BPR-0082"]
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r2","source":"RFC-BPR-0082 finding outside the decision rules","origin":"owner request 2026-10-13"}
---

# BPR-1482: Join comment-range hunks one unchanged line apart, as GitHub does

A finding of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md). GitHub's REST file list joins two hunks that one unchanged
line separates (`git diff --inter-hunk-context=1`); its raw `.diff` does not.
On all 58 recorded files that had both, joining the raw diff's hunks that way
gave exactly the file list's hunks, and linked xdiff Myers with the indent
heuristic then matched GitHub on 78 of 78 files. BPR's
`github_review_hunks` (`src/core/src/review_comment_range.cpp`) joins only
touching hunks, so multi-line comment ranges across such a one-line gap are
refused here though GitHub accepts them, whatever the matcher: the engine's
comment hunks matched GitHub on 32 of 78 files, but on 45 with GitHub's join.

## Acceptance

- **AC1:** `github_review_hunks` joins hunks at most one unchanged line apart,
  matching GitHub's file list; `core_review_comment_range` covers a one-line
  gap and a two-line gap.
- **AC2:** Against `tests/fixtures/github/diff-oracle-v1`, the engine's comment
  hunks equal GitHub's on every recorded file where its alignment equals
  GitHub's, and the evaluation report's comment-flip counts are refreshed.
