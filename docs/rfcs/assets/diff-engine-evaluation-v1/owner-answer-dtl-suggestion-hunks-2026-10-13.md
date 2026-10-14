# RFC-BPR-0082 owner answer on dtl for Review Cycle suggestion hunks, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) after
the [Q2 answer](owner-answer-2026-10-13.md) and the
[restart-only answer](owner-answer-restart-2026-10-13.md). Given by the owner
on 2026-10-13 (item B of that day's decisions), as relayed by the coordinating
session.

## Answer

**Accept.** Extend dtl's core approval beyond per-line intraline comparison to
the bounded Review Cycle suggestion-hunk matcher: `matched_lines` in
`src/core/src/review_cycle_suggestion.cpp`, a first-party Myers capped at 512
edits, takes its matched lines from dtl's minimal edit script instead. dtl
remains never a whole-file line matcher in the diff engine.

## Affected text

Only Q2 changes. Before:

> **Q2. May vendored pure-computation third-party code enter the core?**
> This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
> histogram port. *Answered 2026-10-13: Accept for rapidfuzz-cpp and for dtl,
> dtl for per-line intraline comparison only* (see "Owner answer" above). The
> diff-match-patch port and a JGit-derived histogram port were not asked about
> and remain unapproved; both were rejected on measurement.

After:

> **Q2. May vendored pure-computation third-party code enter the core?**
> This affects dtl, rapidfuzz-cpp, a diff-match-patch port and a JGit-derived
> histogram port. *Answered 2026-10-13: Accept for rapidfuzz-cpp and for dtl,
> dtl for per-line intraline comparison only* (see "Owner answer" above). The
> diff-match-patch port and a JGit-derived histogram port were not asked about
> and remain unapproved; both were rejected on measurement. *Extended
> 2026-10-13: dtl may also compute the Review Cycle suggestion hunks' line
> edit, bounded at 512 changes, and never the diff engine's whole-file line
> matcher* (see "Owner answer on dtl for Review Cycle suggestion hunks"
> above).

## Preserved and consequential

No threshold, decision rule or acceptance criterion changes, and the RFC's
status is unchanged. The suggestion hunks keep their contract: inputs bounded
as before, the 512-edit cap with the whole changed middle as one exact hunk
past it, and the same alignment the first-party Myers chose. dtl must never
reach its restart past 2,000,000 path points. The architecture-boundary rule
allows dtl in exactly `src/core/src/diff_intraline.cpp` and
`src/core/src/review_cycle_suggestion.cpp`; both `AGENTS.md` files,
`cmake/VerifyCoreBoundary.cmake` and `docs/architecture/system-architecture.md`
name exactly the two uses. [[BPR-1498]] carries the change.
