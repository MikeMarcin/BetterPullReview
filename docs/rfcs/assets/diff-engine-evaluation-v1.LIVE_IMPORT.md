# Diff engine evaluation live import

Mandate `diff-engine-evaluation-r1` adopts the
[original proposal](diff-engine-evaluation-v1/RFC-DRAFT-0018-diff-engine-evaluation.md)
unchanged in scope (SHA-256 of the draft as supplied:
`0101ccf62d70d25162a24fd7553ef50a025112faaa3420a75efeb8de4d52d811`; the retained
copy is byte-for-byte). RFC-DRAFT-0018 maps to RFC-BPR-0082; RFC-BPR-0080 and
RFC-BPR-0081 were already taken by the hashing and C++ parser evaluations.
Immutable RFC_BASE_SHA: `cc9e9376e1fc02e2cdf3154fa56dfcc14991a8d5`.

The draft carried no cards or plan. The integrator split it into six cards,
allocated as BPR-1465 to BPR-1470
(`docs/issues/_reservations/BPR-1465--BPR-1470--7021f30a9b8c.json`): the core
seam first, then the opt-in harness, candidates and corpus, then the macOS
measurements and decisions, then the Linux and Windows stations, then closeout.
Linux and Windows are eligible cards for those stations; this macOS lane does
not claim them.

Changes in the live RFC: front matter, an execution-status section recording the
owner decisions supplied with the mandate (the core library allowance of
2026-10-13; Q1 and Q3 open), the `Acceptance` heading renamed
`Acceptance criteria` for the documentation gate, and links rebased to
`docs/rfcs/`. Excluded: the other drafts under `docs/rfcs/drafts/` and unrelated
configuration changes in the main checkout. Import supplies no measurement or
acceptance.

Owner answer, 2026-10-13 (mandate `diff-engine-evaluation-r2`): Q2 Accept for
rapidfuzz-cpp and dtl (dtl for per-line intraline comparison only); recorded in
the RFC with the [answer record](diff-engine-evaluation-v1/owner-answer-2026-10-13.md).
Follow-ups BPR-1480 to BPR-1482 were allocated
(`docs/issues/_reservations/BPR-1480--BPR-1482--0bbd641765b5.json`).
