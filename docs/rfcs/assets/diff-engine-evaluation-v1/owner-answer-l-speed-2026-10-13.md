# RFC-BPR-0082 owner answer on shape L's speed, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r3`, after the [Q2 answer](owner-answer-2026-10-13.md)
and the [Q1 answer](owner-answer-q1-2026-10-13.md). Given by the owner in the
coordinating session on 2026-10-13, after the L re-measurement reported that
linked xdiff took 1.60–2.03× the baseline's median time on L inputs and so
failed the primary rule's 2× requirement.

## Answer

Verbatim: "I don't care about that, just use it anyways so we don't have to
maintain ours."

As relayed with it: the answer waives the primary rule's speed and size
condition for shape L. The owner accepts L being slower on large inputs in
order to stop maintaining the first-party primary matcher. It waives nothing
else: correctness (suites, refusals, valid results), determinism, the 10 ms
stop budget, memory, placement and Q1's condition (xdiff only as its own
dynamically linked shared library) all still apply. If a gate other than speed
fails, the evaluation stops and reports instead of adopting.

## Affected text

Only the primary matcher's row of "Decision rules" changes, for shape L only.
Before:

> | Primary matcher | On L and pathological inputs it produces no approximate result where the baseline does, and is at least 2× faster or 2× smaller in median. No class regresses its median by more than 10%. GitHub hunk agreement is at least the baseline's, and comment-range flips do not increase. |

After:

> | Primary matcher | On L and pathological inputs it produces no approximate result where the baseline does, and is at least 2× faster or 2× smaller in median. No class regresses its median by more than 10%. GitHub hunk agreement is at least the baseline's, and comment-range flips do not increase. *For shape L the owner waived the speed and size conditions on 2026-10-13 (the 2× requirement and the 10% median-time regression); the rest of the row and every gate still apply.* |

## Preserved and consequential

The thresholds for every other shape, the gates, Q3 and the acceptance
criteria are unchanged. The 10% regression clause is read as part of the speed
condition the answer waives, because the owner accepted L being slower on large
inputs. The row's first clause, "no approximate result where the baseline does"
on L and pathological inputs, is not a speed or size condition and stays. On the
re-measurement L is never approximate where the baseline is not, and its own
matcher never approximates, but 7 of the baseline's 11 approximate L and
pathological comparisons stay approximate under L through the shared pairing
and intraline budgets (an unpaired replacement or an omitted intraline pass).
Whether the waiver extends to that clause is put back to the owner; until then
L is not adopted.
