# BPR-1617 handoff

Role: rfc-implementer | Model: gpt-6.1-sol | Effort: xhigh

Assignment: one nondelegating milestone in the main BetterPullReview checkout,
under the owner's deterministic platform crypto mandate r4 and chrono/random
amendment r5. No additional writers or agents were used. Parent audited read-only.

## Result and revisions

- Plan/reservation: `7e0c38ab5`; claim: `b9d5bf85e`.
- Exact implementation candidate: `45b0d3528fdf159ad9050debe3bd0f9f28d59bcf`.
- Integrated revision for final checkpoint validation (same product sources):
  `9ff31e0f4bd8c94b031641f144f585918481e9ce`.
- Board completion: `9ff31e0f4bd8c94b031641f144f585918481e9ce`.
- BPR-1617 is Done. The additive plan's five exits pass; original RFC plan,
  baseline and historical accepted cards are preserved.
- CommonCrypto on macOS; CNG on Windows; HACL elsewhere or with
  BPR_CORE_SHA256_PORTABLE=ON. Native initialization can fall back to HACL;
  established-stream failure terminates. Independent copies/noexcept retained;
  CNG allocates at construction/copy and moves transfer ownership. SHA-1 unchanged.
- Chrono arithmetic/time points and explicit seeded caller state permitted;
  clocks/random_device/global C RNGs refused. No new generator or RE2 migration.

## Verification

[Verification](verification.md), [acceptance](acceptance.json), and the
[RFC ledger](../../../verification/RFC-BPR-0080.md) bind the exact source.
Native core-only integration 11/11; configured forced-HACL composition 3/3;
Mac LLDB proves CommonCrypto; real Windows/MSVC courses pass CNG/HACL against
matching source hashes; executable CNG failure contracts pass. Core boundary
8 tests pass. Architecture contract's one new-fixture defect fixed and rechecked
with 157 unchanged passing checks reused. Documentation gate/67 tests and
clang-format 22.1.8 checks pass. Compact median7 Release bulk probe: Mac 5.68x,
Windows 2.03x native/HACL with identical digests. Native default restored and
its public SHA course passes after fallback verification.

## Review, ownership and commands

No human/native/independent acceptance remains for this bounded milestone and
no review server was started. Broader original Windows/Linux station, full dev,
window-thread, package and RFC closeout obligations remain on BPR-1448,
BPR-1449 and BPR-1453; no Linux/native universal-x86/full-dev/release claim.

All owned commands finished, including local builds, both architecture-contract
runs, LLDB child, SSH and remote MSBuild. The Windows GUID-named source/build
root was removed after its exact-source courses; no remote checkout changed.
Local core-only cache defaults to native selection (PORTABLE=OFF). No
protected attended candidate was touched. No live GitHub mutation or push.
Private evidence/selection remain in /tmp/bpr-platform-crypto for the coordinator.
Task-owned local probe binaries and transfer archives were removed; no process
or server is handed off. The implementation leaf releases repository writer
ownership after this checkpoint commit.

## Continuation

The coordinator can report the completed mandate and its separate audit findings.
No engineering successor is assigned by this milestone. Existing RFC station
cards remain eligible under their original requirements; do not infer RFC-wide
closure from BPR-1617's Done status.

## Changed paths

- `AGENTS.md`
- `CONTRIBUTING.md`
- `cmake/VerifyArchitectureBoundaries.cmake`
- `cmake/VerifyCoreBoundary.cmake`
- `docs/architecture/deterministic-functional-core-imperative-host-qt-presentation-adapter.md`
- `docs/architecture/system-architecture.md`
- `docs/development/build-and-test.md`
- `docs/issues/10-backlog/BPR-1453-close-rfc-0080.md`
- `docs/issues/20-ready/BPR-1448-run-the-hash-evaluation-on-windows.md`
- `docs/issues/60-done/BPR-1617-deterministic-platform-sha256.md`
- `docs/issues/NEXT_ID`
- `docs/issues/_checkpoints/BPR-1617/milestone-binding.json`
- `docs/issues/_reservations/BPR-1617--BPR-1617--8031beb5bea5.json`
- `docs/rfcs/RFC-BPR-0080-hashing-replacement.md`
- `docs/rfcs/amendments/RFC-BPR-0080-platform-crypto-r4.md`
- `docs/rfcs/plans/deterministic-platform-sha256-v1/milestone-plan.json`
- `docs/rfcs/plans/deterministic-platform-sha256-v1/source.json`
- `docs/verification/RFC-BPR-0080.md`
- `docs/verification/_BPR-1617-platform-sha256.json`
- `src/core/AGENTS.md`
- `src/core/CMakeLists.txt`
- `src/core/include/bpr/core/sha256.hpp`
- `src/core/src/sha256_cng.cpp`
- `src/core/src/sha256_commoncrypto.cpp`
- `src/core/src/sha256_hacl.cpp`
- `src/core/src/sha256_portable.cpp`
- `tests/core/CMakeLists.txt`
- `tests/core/cng_stub/bcrypt.h`
- `tests/core/cng_stub/windows.h`
- `tests/core/sha256_cng_contract_tests.cpp`
- `tests/core/sha256_tests.cpp`
- `tests/scripts/architecture_boundary_tests.py`
- `tests/scripts/core_boundary_tests.py`
