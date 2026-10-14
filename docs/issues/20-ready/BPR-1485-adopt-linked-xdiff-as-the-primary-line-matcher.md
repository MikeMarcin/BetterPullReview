---
schema: "implement-rfc.card.v1"
id: "BPR-1485"
title: "Adopt linked xdiff as the primary line matcher and delete the first-party matcher and pause and resume"
status: "Ready"
rfcs: ["RFC-BPR-0082"]
depends_on: ["BPR-1465", "BPR-1480", "BPR-1482"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"mandate_revision":"diff-engine-evaluation-r3","source":"RFC-BPR-0082 primary matcher and hunk sliding: adopt l-myers-indent (owner answers 2026-10-13: Q1 dynamic shared library, L speed waiver, approximation clause, restart-only cancellation)"}
---

# BPR-1485: Adopt linked xdiff as the primary line matcher and delete the first-party matcher and pause and resume

The adoption follow-up of [RFC-BPR-0082](../../rfcs/RFC-BPR-0082-diff-engine-evaluation.md) for the primary line matcher
and hunk sliding. On macOS the evaluation candidate `l-myers-indent` (Git
2.56.0's xdiff, Myers with the indent heuristic, built as its own shared
library) passed every gate, agreed with GitHub on 78 of 78 recorded files
against the engine's 45, and left 0 comment-range flips after [[BPR-1482]]
([report](../../verification/RFC-BPR-0082.md),
[L tables](../../verification/RFC-BPR-0082/macOS-L-results.md)). The owner
answered on 2026-10-13: Q1, xdiff only as its own dynamically linked shared
library, as Qt is; its 1.60–2.03× time on L inputs is accepted (speed waiver)
and so are its 7 budget-caused approximations (approximation answer); and
restart-only cancellation replaces BPR-1366's pause and resume once xdiff is
adopted. The answer records are under
[`docs/rfcs/assets/diff-engine-evaluation-v1/`](../../rfcs/assets/diff-engine-evaluation-v1/).

The evaluation's adapter shape is the starting point:
`tests/evaluation/diff_engine/candidates/xdiff_linked.cpp` (the whitespace
mapping that hands xdiff engine-normalized lines whenever the options are not
exact), the `xdiff_shim/` compatibility header, and the
`libbpr_eval_xdiff` shared-library target with the pinned Git 2.56.0 archive in
`tests/evaluation/diff_engine/CMakeLists.txt`.

## Known and out of scope

- Seven corpus comparisons stay approximate through the shared pairing and
  intraline budgets, not through the matcher: `bench-10000-300`,
  `bench-40000-300`, `bench-100000-30`, `bench-100000-300`,
  `patho-huge-replacement`, `patho-long-lines` and `patho-moved-blocks`. The
  owner accepted them; changing those budgets is not part of this card.
- Review Cycle suggestion hunks (the bounded Myers in
  `src/core/src/review_cycle_suggestion.cpp`) stay as they are; whether L
  replaces them is an open owner question.
- Linux and Windows measurements ([[BPR-1468]], [[BPR-1469]]) are not run here.

## Acceptance

- **AC1, library and linkage:** Git 2.56.0's `xdiff/` is acquired by the pinned
  verified-archive pattern (SHA-256, size, licence digest) and built as its own
  shared library, never compiled or statically linked into another target; one
  adapter links it dynamically and is the only code that calls it, and
  `architecture_boundaries` enforces both. Per-file licences (LGPL-2.1-or-later;
  EDL-1.0 for `xhistogram.c` if it ships), the LGPL notice and written source
  offer, corresponding source, `packaging/dependencies.json`, SPDX, release
  notices and the package audits are updated as for Qt's dynamic arrangement,
  and the macOS package carries the library where the application loads it.
- **AC2, primary matcher and hunk sliding:** every diff computation the
  application host runs gets its primary matches from that adapter through a
  host port and the [[BPR-1465]] seam: Myers with the indent heuristic and the
  evaluation's corrected whitespace mapping. On the evaluation corpus the
  product's alignments equal `l-myers-indent`'s (segments digests), and on
  `tests/fixtures/github/diff-oracle-v1` its hunks agree with GitHub on 78 of 78.
- **AC3, first-party matcher deleted:** the first-party primary matcher (the
  LCS table and its ordered-unique, anchor and patience fallbacks) and anything
  left dead are removed. The step meter and the budgets that pairing, intraline
  and the rapidfuzz-cpp scoring still use stay. Core-only builds and tests that
  need a matcher get a test double or the host port, not the old engine.
- **AC4, pause and resume deleted:** BPR-1366's pause-and-resume machinery is
  removed, per the owner's restart-only answer: a stopped diff is abandoned and
  recomputed from scratch the next time it is needed, and the 10 ms stop budget
  holds through abandonment (a test shows it). Every test that asserted
  resumption is changed to the restart behaviour and listed in the closeout,
  and `docs/architecture/diff-pipeline-and-rendering.md` and the "Pausing"
  section of `docs/architecture/system-architecture.md` describe the new
  behaviour.
- **AC5, verification:** the diff suites the RFC lists, `core-only`, both
  architecture-boundary checks and the documentation gate pass; the opt-in
  evaluation build either still configures and builds or the parts that
  measured deleted code are retired, with what was retired recorded here.
