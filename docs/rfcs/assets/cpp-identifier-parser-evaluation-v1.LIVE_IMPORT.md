# C++ identifier parser evaluation live import

Mandate `cpp-identifier-parser-evaluation-r1` adopts the
[original proposal](cpp-identifier-parser-evaluation-v1/RFC-DRAFT-0021-cpp-identifier-parser-evaluation.md)
unchanged (SHA-256 of the draft as supplied:
`eb12227403a7cf6cd07abb6ec94918e2c80a2cf114de95b90bd8286720677afc`). The retained
copy differs only in its link to the uncommitted RFC-DRAFT-0018, which is kept as
code text so the documentation gate finds no broken link.
RFC-DRAFT-0021 maps to RFC-BPR-0081; RFC-BPR-0080 was already taken by another
agent's in-flight import of the hashing-replacement draft, so this one skips it. The integrator's provisional plan cards
BPR-0001 to BPR-0006 map to the allocated BPR-1440 to BPR-1445
([id map](cpp-identifier-parser-evaluation-v1/id-map.json)). Immutable
RFC_BASE_SHA: `530d92eaff1578413c0e669f476e740387441152`.

The draft carried no cards or plan. The integrator prepared the
[source plan](cpp-identifier-parser-evaluation-v1/milestone-plan.json): labelled
corpus and shipped tests first, then the macOS harness and measurements, then the
Windows and Linux stations, then closeout. The live plan is adopted through the
standard helper. Windows and Linux measurements are eligible cards for those
stations; this macOS lane does not claim them.

Changes in the live RFC: front matter, an execution-status section, the
`Acceptance` heading renamed `Acceptance criteria` for the documentation gate,
links rebased to `docs/rfcs/`, and the link to the uncommitted RFC-DRAFT-0018 replaced
by its name. Excluded: the other drafts under `docs/rfcs/drafts/` and unrelated
configuration changes in the main checkout. Open question Q1 stays with the owner.
Import supplies no measurement or acceptance.
