# RFC-BPR-0082 owner answer on restart-only cancellation, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r3`, after the
[L speed answer](owner-answer-l-speed-2026-10-13.md). Given by the owner in the
coordinating session on 2026-10-13, after being told that the worst
recomputation seen on the corpus with linked xdiff was about 264 ms (Myers with
the indent heuristic on the 100,000-line benchmark pair; 278 ms at most in the
re-measurement under load).

## Answer

**Accept.** With xdiff adopted, delete the first-party pause-and-resume diff
machinery of BPR-1366. A stopped diff is abandoned and recomputed from scratch
the next time it is needed.

As relayed with it: the 10 ms stop budget must still hold, through abandonment.
Only what becomes dead is removed; the step meter and budgets that other
components still use (intraline, pairing and the rapidfuzz-cpp scoring) stay.
Every test that asserts resumption changes to the restart behaviour. The
deletion belongs to the L adoption work or a follow-up card, and happens only
once L's non-speed gates pass.

## Affected text

Only the closing paragraph of "Decision rules" changes. Before:

> A rejected component keeps the current engine, and a result of no change is
> valid. The report also states whether the best candidate's worst observed time
> would make restart-only cancellation enough. It does not remove BPR-1366's
> resumable machinery.

After:

> A rejected component keeps the current engine, and a result of no change is
> valid. The report also states whether the best candidate's worst observed time
> would make restart-only cancellation enough. It does not remove BPR-1366's
> resumable machinery. *Answered 2026-10-13: restart-only cancellation is
> accepted once xdiff is adopted. The BPR-1366 pause-and-resume machinery is
> then deleted; a stopped diff is abandoned and recomputed from scratch when
> next needed, and the 10 ms stop budget holds through abandonment.*

## Preserved and consequential

Nothing changes while L is not adopted: the engine keeps BPR-1366 as it is.
The gates, including the 10 ms Interruption gate, are unchanged. The step meter,
matrix budget and the budgets of intraline and pairing stay. The architecture
contract text (`docs/architecture/diff-pipeline-and-rendering.md` and the
"Pausing" section of `docs/architecture/system-architecture.md`) changes with
the deletion, not before.
