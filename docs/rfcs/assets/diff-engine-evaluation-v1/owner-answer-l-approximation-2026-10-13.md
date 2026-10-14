# RFC-BPR-0082 owner answer on shape L's approximation clause, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r3`, after the
[L speed answer](owner-answer-l-speed-2026-10-13.md) and the
[restart-only answer](owner-answer-restart-2026-10-13.md). Given by the owner in
the coordinating session on 2026-10-13.

The owner was told: L's own matcher never approximates and removes 4 of the
baseline's 11 approximations on L and pathological inputs; on the 100,000-line
inputs L gives a real alignment where the baseline degrades to one whole-file
replacement; 7 comparisons (`bench-10000-300`, `bench-40000-300`,
`bench-100000-30`, `bench-100000-300`, `patho-huge-replacement`,
`patho-long-lines`, `patho-moved-blocks`) stay approximate through the shared
pairing and intraline budgets.

## Answer

**Accept.** The speed waiver also covers the primary rule's approximation
clause for shape L. L is adopted as the primary line matcher with its 7
budget-caused approximations as known; they are out of scope for the adoption.

## Affected text

Only the italic note on the primary matcher's row of "Decision rules" changes.
Before:

> *For shape L the owner waived the speed and size conditions on 2026-10-13 (the 2× requirement and the 10% median-time regression); the rest of the row and every gate still apply.*

After:

> *For shape L the owner waived the speed and size conditions on 2026-10-13 (the 2× requirement and the 10% median-time regression) and the approximation clause (7 comparisons stay approximate through the shared pairing and intraline budgets); the rest of the row and every gate still apply.*

## Preserved and consequential

Every gate still applies, as do the GitHub-agreement and comment-flip clauses,
which L passes. With this answer L passes every gate and rule clause it is held
to on macOS, so the primary matcher and hunk sliding are adopted as
`l-myers-indent` through [[BPR-1485]], which also deletes the first-party
primary matcher and, per the restart-only answer, the BPR-1366 pause-and-resume
machinery. The thresholds for every other shape are unchanged.
