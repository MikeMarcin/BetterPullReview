# RFC-BPR-0082 owner answer, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r1` (adopted in commit `227cab569`). Relayed
by the coordinating session on 2026-10-13.

## Answer

**Q2, Accept:** rapidfuzz-cpp (MIT) and dtl (BSD-3-Clause) may both go into
the core as approved libraries. The owner was told that dtl misbehaved as a
whole-file line matcher (invalid edit scripts past 2,000,000 path points, an
uncaught `std::length_error`, a resumed run that never ends) and would be used
only for the per-line intraline comparison, where it passed every macOS gate.

## Affected text

Only Q2 changes. Before:

> **Q2. May vendored pure-computation third-party code enter the core?**
> This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
> histogram port. The owner question is pending in the parent session. Until
> it is answered, these are evaluated only in the opt-in target, and core
> placement cannot be adopted.

After:

> **Q2. May vendored pure-computation third-party code enter the core?**
> This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
> histogram port. *Answered 2026-10-13: Accept for rapidfuzz-cpp and for dtl,
> dtl for per-line intraline comparison only* (see "Owner answer" above). The
> diff-match-patch port and a JGit-derived histogram port were not asked about
> and remain unapproved; both were rejected on measurement.

## Preserved and consequential

No threshold, decision rule or acceptance criterion changes. Q1 and Q3 stay
open. AC06 still binds dtl's intraline adoption to the owner's Accept of its
render set. The decision rules make line-pair similarity's outcome "adopt"
for rapidfuzz-cpp, since its gates and rule passed on macOS and its only open
blocker was Q2; [[BPR-1480]] carries the production change. [[BPR-1481]]
carries dtl's intraline adoption and starts only after an Accept. Core's link
allow-list (`cmake/VerifyCoreBoundary.cmake`), the root and core `AGENTS.md`
rules and `docs/architecture/system-architecture.md` name both libraries.
Linux and Windows measurements ([[BPR-1468]], [[BPR-1469]]) are still owed.
