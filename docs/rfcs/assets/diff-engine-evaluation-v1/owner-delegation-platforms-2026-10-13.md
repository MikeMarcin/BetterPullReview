# RFC-BPR-0082 owner delegation on Linux and Windows qualification, 2026-10-13

Live source: [RFC-BPR-0082](../../RFC-BPR-0082-diff-engine-evaluation.md) at
mandate `diff-engine-evaluation-r3`, after linked xdiff (BPR-1485), dtl
intraline (BPR-1481), rapidfuzz-cpp pairing (BPR-1480) and the hunk join
(BPR-1482) landed on main.

## Answer

Asked what the Linux and Windows measurement cards (BPR-1468, BPR-1469) should
do now, the owner answered "decide". The coordinating session decided, under
that delegation: candidate comparison against the first-party baseline is moot,
because that engine is deleted. Both cards are rescoped to qualifying the
adopted stack on their platform:

- build xdiff as its own shared library, plus dtl and rapidfuzz-cpp;
- run the diff suites and `core_diff_*`;
- run the BPR-1485 equivalence tool, checking cross-platform determinism
  against the macOS digests and GitHub agreement 78 of 78;
- run the package audits.

## Affected text

Only AC02 changes. Before:

> - **AC02:** Measurements on macOS, Linux and Windows cover every size class
>   and the measurement list under "Corpus and measurement", with the baseline
>   measured in the same runs and conditions.

After:

> - **AC02:** Measurements on macOS, Linux and Windows cover every size class
>   and the measurement list under "Corpus and measurement", with the baseline
>   measured in the same runs and conditions. *Amended 2026-10-13 under the
>   owner's delegation: macOS carries the measurements; on Linux and Windows
>   the adopted stack is qualified instead (shared-library xdiff, dtl and
>   rapidfuzz-cpp built; the diff suites pass; the BPR-1485 equivalence tool
>   reproduces the macOS digests and GitHub agreement 78 of 78; the package
>   audits pass), because the baseline is deleted* (see "Owner delegation on
>   Linux and Windows qualification" above).

## Preserved and consequential

The macOS measurements, the decision rules, the thresholds and every earlier
owner answer stand; Q3 stays open. BPR-1468 and BPR-1469 keep their IDs, file
names and platform eligibility, and keep their original acceptance as
superseded text. BPR-1470's closeout reads AC02 as amended. The "Corpus and
measurement" section's instruction to run on the Linux and Windows stations
is historical for the candidate measurements.
