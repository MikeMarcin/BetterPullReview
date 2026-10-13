# Git metadata readers live import

Mandate `git-metadata-readers-r1` adopts the
[original proposal](git-metadata-readers-v1/RFC-DRAFT-0019-git-metadata-readers.source.txt)
(exact bytes; SHA-256 `f7581fd7e27cb07ad19f77e6bfd2d101441b7c57c85d8b8bc4547ea71ea48c58`).
Immutable RFC_BASE_SHA: `9a4f61976bba23727acd77d10c23c041f87208f0`.

| Source | Live destination |
|---|---|
| RFC-DRAFT-0019-git-metadata-readers.md | [RFC-BPR-0083](../RFC-BPR-0083-git-metadata-readers.md) |
| Immediate-fix card "against RFC-BPR-0054 (provisional DRAFT-0010)" | [[BPR-1454]] |
| No authored cards; integrator decomposition by acceptance criterion and platform | [[BPR-1455]] to [[BPR-1464]] |
| No authored plan | [Source plan](git-metadata-readers-v1/milestone-plan.json), adopted as the [live plan](../plans/git-metadata-readers-v1/milestone-plan.json) |

RFC-BPR-0080, RFC-BPR-0081 and RFC-BPR-0082 were taken by concurrent imports of
the hashing, C++ parser and diff engine drafts, so this draft takes RFC-BPR-0083. The draft's
"provisional DRAFT-0010" label for the fix card does not name a live record; the
card is filed against RFC-BPR-0054 and this RFC.

Changes in the live RFC: front matter, an execution-status section recording the
owner's Q1 answer of 2026-10-13, the `Acceptance` heading renamed `Acceptance
criteria` for the documentation gate, and links rebased to `docs/rfcs/`.
Excluded: the other drafts under `docs/rfcs/drafts/` and the unrelated
`.gitignore` and MASK changes in the main checkout. Q2 and Q3 stay with the owner;
the reftable part of fix 1 and AC07 wait for Q2. Import supplies no measurement or
acceptance.
