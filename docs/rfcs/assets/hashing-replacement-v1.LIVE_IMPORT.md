# Hashing replacement live import

Mandate `hashing-replacement-r1` adopts the
[draft](hashing-replacement-v1/RFC-DRAFT-0020-hashing-replacement.md)
(SHA-256 `77f418c38647532599f82e5f11acbafe50dd39b47e2e24c5aee8f65253be920f`).
RFC-DRAFT-0020 maps to RFC-BPR-0080; newly allocated BPR-1446 to BPR-1453
carry its seven acceptance criteria. Immutable RFC_BASE_SHA:
`5620a9f15a391dde7750112294f7562f2f64a32f`. The engineer authored the absent
six-milestone [plan](hashing-replacement-v1/milestone-plan.json), adopted as
the live plan.

Owner decisions at import: core may link only owner-approved, pinned, pure
libraries (glaze, utf8proc, zlib and fmt; 2026-10-13). The draft's Q1
(sha1collisiondetection in core), Q2 (HACL* SHA-2 in core) and Q3 (Linux links
the system libcrypto) are unanswered. BPR-1450 and BPR-1451 are blocked on
them; BPR-1452 runs only on a Q1 Reject. Windows and Linux measurements are
station cards (BPR-1448, BPR-1449) by platform metadata; no result is claimed
for them at import.

The adopted RFC and the retained copy each replace one link to the
uncommitted RFC-DRAFT-0018 draft with its name; the digest above is the
original draft's. The original draft path and unrelated dirty configuration in the
main checkout are excluded. No owner, native or package acceptance is supplied
by import.
