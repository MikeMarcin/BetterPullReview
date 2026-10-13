# RFC-BPR-0080 owner answers and collision amendment, 2026-10-13

Mandate `hashing-replacement-r2`. The owner answered the adopted RFC's open
questions through the coordinating session on 2026-10-13. This record follows
the author-rfc amendment convention; the integrator published it into the live
RFC in the same change.

## Live sources amended

| Source | Content identity before the change |
|---|---|
| `docs/rfcs/RFC-BPR-0080-hashing-replacement.md` at `4967b9abd` | SHA-256 `948187c9270b3d114f6e98b772167a3a59c4747dbe499e403762f4183cb1264c` |
| `docs/issues/50-blocked/BPR-1450-adopt-sha1collisiondetection-for-sha1-identities.md` | SHA-256 `516c8a42b61e072be6962cd22b47313bedd943fef0e4a8ce341fd009102c67d1` |
| `docs/issues/10-backlog/BPR-1453-close-rfc-0080.md` | SHA-256 `0d4c99445083197d8fe941c8b9eafd8d13a4251f1d5faa089897e26f64b14e40` |
| `docs/rfcs/plans/hashing-replacement-v1/milestone-plan.json` | SHA-256 `77f22af6fd44f82bc73921eee8cdab55efaa159aa59fd9cb750c7e95cefdc3a7`; unchanged |

## Owner answers (verbatim as relayed, 2026-10-13)

- **Q1, Accept:** add sha1collisiondetection (MIT) to core's approved libraries.
- **Q2, Accept:** add HACL*'s generated SHA-2 C (MIT) to core's approved libraries.
- **Amendment, Accept:** the collision requirement is met by detection proven
  on raw byte streams (SHAttered and SHA-mbles), plus the same library at every
  object-ID construction, verification and publication path, shown by code
  inspection and test. A Git-header collision vector is not required.

Q3 was not asked: the Linux backend has not been measured.

## Rationale

[[BPR-1446]] found that the public collision pairs neither collide nor are
detected once a Git object header precedes them, so the four object-level
refusal points could not be exercised with any public vector. The owner
accepted proof at the raw stream plus proof that every object-ID path uses the
same detecting library.

## Before and after

**Evaluation, collision inputs.** Before: "Each must be refused at core
identity construction, host blob verification, transport verification and
Review Cycle publication." After: each must be detected when hashed as a raw
byte stream; every one of those four points must be shown, by code inspection
and test, to compute its SHA-1 with that same detecting library.

**Decision rule, sha1dc.** Before: "every collision input is refused at every
listed point". After: "every collision input is detected on raw byte streams,
and every listed point is shown by code inspection and test to use the same
detecting library".

**AC03.** Before: "With sha1dc adopted, every collision input is refused at
each listed point while the plain-SHA-1 control collides. Ordinary objects, and
pack checksums of valid packs, are still accepted." After: the text in the
live RFC's Acceptance criteria, marked amended 2026-10-13.

**Owner questions.** Q1 and Q2 change from unanswered to Accept.

## Preserved obligations

- The correctness gate, every other decision rule and AC01, AC02 and AC04 to
  AC07 are unchanged.
- The throughput conditions are unchanged, including the idle machine. Both
  adopt rules need W1 to W3 "on every measured target", measured under those
  conditions; BPR-1446's measurements were taken on a loaded machine and its
  AC4 stays open. The owner has not waived that condition.
- An Accept puts a library on core's approved list. It is not an adopt
  decision.

## Consequences and reconciliation

- `cmake/VerifyCoreBoundary.cmake` admits `BetterPullReview::Sha1dc` and
  `BetterPullReview::HaclSha2`; the root and core `AGENTS.md` files and the
  system architecture name both libraries.
- BPR-1450 and BPR-1451 lose the `owner-core-library-approval` blocker. Their
  dependency on BPR-1446 remains, so they move to Backlog until BPR-1446 is
  Done.
- BPR-1450's AC2 and BPR-1453's AC03 checklist row carry the amended text.
- BPR-1452 runs only on a Q1 Reject; with Q1 accepted it is not needed and
  stays in Backlog until closeout records that.
- The adopted plan's milestones, work selectors and exits are unchanged.
