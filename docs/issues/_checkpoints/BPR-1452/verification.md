# BPR-1452 conditional fallback reconciliation

Assignment `/root/obsolete_hash`, local main; run `bpr-closeout-20261018`,
segment `obsolete-hash-1452`, mandate `owner-ready-backlog-20261018-r2`.
Inspected source revision: `2b09f74f0287b1bd0a282b40cb31805a0973d293`.
Immutable RFC_BASE_SHA: `5620a9f15a391dde7750112294f7562f2f64a32f`.

## Evidence and disposition

- RFC-BPR-0080 records Q1 Accept, 2026-10-13, and already stated that
  BPR-1452 was not needed after that answer.
- The unmodified authored plan selects `adopt-sha1dc`; its boundary is
  "Starts only after the owner answers Q1; only the card matching the answer
  runs." BPR-1450 is the accepted branch, with its four original passing
  records at `23c6b345f`. Its acceptance is reused, not re-certified as a new
  native or full-package run.
- Current `src/core/CMakeLists.txt` selects `sha1_collision_detecting.cpp`
  and links `BetterPullReview::Sha1dc`; `src/core/src/sha1.cpp` is absent.
  The selected implementation calls SHA1DCInit/Update/Final and returns
  `sha1_refusal::collision_detected` on detection. This is inspection of the
  composed public SHA-1 implementation, not a new runtime test result.
- The public header and wrapper match `23c6b345f` byte for byte. An initial
  overly broad byte-identity assertion also covered the provider and failed:
  its approval comment lost the word "pure" under the later architecture
  amendment. Direct diff establishes that this is its only change; the
  executable CMake content, pin, source set and configuration are unchanged.
- Authored plan SHA-256:
  `77f22af6fd44f82bc73921eee8cdab55efaa159aa59fd9cb750c7e95cefdc3a7`.
  The plan/source receipt and imported packet are unchanged. No claim is
  inferred from a source configuration file about installed loading.

Original AC1–AC3 remain intact and are not applicable to the unselected
Q1-Reject branch. Closure accepts the branch disposition only. No fallback
implementation, correctness, throughput or boundary-test run is claimed.

## Verification commands

- Scoped source/decision consistency assertions: owner answer, BPR-1450's
  recorded acceptance, authored branch boundary, deleted first-party source,
  selected target/refusal path, unchanged wrapper/header, exact comment-only
  provider diff, authored-plan digest and immutable baseline: pass.
  Runtime receipt: `/tmp/bpr-closeout-20261018/hash-relevance.json`.
- `python3 scripts/verify_documentation.py --root .`: DOCUMENTATION_OK.
- Scoped issue-board validation: valid, 1,610 cards, no errors.
- Milestone checkpoint check: `action: stop`. The conditional Reject exit's
  passing record means its applicability was resolved by Q1 Accept; it is
  not a claim that fallback tests passed. The helper explicitly reports
  `engineering_acceptance: not-evaluated`.
- `git diff --check`: pass.

This documentation-only reconciliation changes no product inputs. Existing
BPR-1450 acceptance and BPR-1617's later focused Git-object/collision course
remain evidence for their exact candidates; no new build or runtime course
is required to determine that the conditional fallback was not selected.
RFC-wide station, window-thread, Q3 and package obligations remain on
BPR-1448, BPR-1449 and BPR-1453.
