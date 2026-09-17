---
schema: "implement-rfc.card.v1"
id: "BPR-0832"
title: "The poller liveness course uses a data root Windows refuses as not owner-private"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["win"]
metadata: {"found_by":"bpr-win task-20260917-win-hardening new defect","found_at":"711d3ddc5f8db9f237e4c4b84758d116b9e897af","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 application_poller_liveness passes on the Windows station, reaching its boundary line and the tick subtest","outcome":"pass","revision":"1c10d56616084f022e0cec6aaee6d1d429a6cc42","evidence":{"kind":"command","detail":"bpr-win: passes in 1.91 s printing github-application-boundary-tests:pass then github-application-liveness-tests:pass; reverted trial at the base still fails at application startup succeeds (card Windows section)."}},{"criterion":"AC2 it still passes on macOS","outcome":"pass","revision":"013d2036018ed608c54e1ae5dc1c19b904e327cc","evidence":{"kind":"command","detail":"application_poller_liveness passes on macOS at 013d2036 (build/r2-logs/dsp3-int5-ctest.log)."}}]
---

# BPR-0832: The poller liveness course uses a data root Windows refuses as not owner-private

## Defect

`application_poller_liveness` fails on Windows at "application startup
succeeds", so neither its BPR-0802 boundary line nor [[BPR-0827]]'s tick
subtest is reached. A reverted diagnostic on `bpr-win` showed
`startup_failure::registry`: the course's bare `QTemporaryDir` data root is
not owner-private on Windows, where a temporary directory inherits its
parent's access-control list. On macOS and Linux `mkdtemp`'s `0700` already
is owner-private, so the course passes there (Linux: dsp-002 confirmation).

Reported by `bpr-win` in `task-20260917-win-hardening` step 1
(`docs/verification/BPR-0802-windows-confirmation.md`), not carded there.

## Scope

- Give the course a data root that is owner-private by construction, using
  the product's own definition (`bpr::adapters::sqlite::make_storage_root_owner_private`
  or the course's existing private-root helper), not a Windows-only skip.
- [[BPR-0820]] narrows the automated-run private profile the same way; reuse
  that rule rather than add a second one.

## Acceptance

- `application_poller_liveness` passes on the Windows station, reaching its
  boundary line and the tick subtest.
- It still passes on macOS.

## Windows station, 2026-09-17 (task-20260917-win-startup-roots)

The base is `559377700d52f37d6c094cd646200c67ab20eeb5`.

**Change.** `with_fixtureless_application` in
`tests/end_to_end/github_application_liveness_tests.cpp` keeps the
`QTemporaryDir` and its canonical path. It then makes the root owner-private
by construction, using the stores' own rule:

- it narrows the temporary directory with
  `bpr::adapters::sqlite::make_storage_root_owner_private`;
- it creates `data` inside it and narrows that too;
- it starts the application on `<canonical temporary directory>/data`.

The root sits inside the narrowed directory rather than being it, because the
agent workspace neighbourhood is created beside the root and trusts only a
private parent. This station's `%TEMP%` grants other principals Modify.

This is the same rule the agent chat and finding courses use since
[[BPR-0812]]. [[BPR-0820]]'s private automated root has not reached
`windows-lane`, so it was not available to reuse.

**Results.**

| Run | Result |
|---|---|
| `ctest --preset windows-dev -R "^application_poller_liveness$" -V` with the change | **Passed**, 1.91 s: `github-application-boundary-tests:pass`, then `github-application-liveness-tests:pass`, i.e. the boundary line and [[BPR-0827]]'s tick subtest |
| **Reverted trial:** the HEAD version of the file, rebuilt at this base | **Fails**: `FAILED: application startup succeeds` |
| The change restored and rebuilt | Passes again |

**Portable sources touched:**
`tests/end_to_end/github_application_liveness_tests.cpp`. On POSIX the
narrowing is `chmod 0700` on two directories the course creates, and the root
becomes `<temporary directory>/data`. The macOS run is the integrator's.

## macOS run; finished (dsp-003)

Main merged `windows-lane` at `aedf471f` as `013d2036`. **macOS at main `013d2036`** after the merge (`build/r2-logs/dsp3-int5-ctest.log`):
`application_poller_liveness` passes (1.50 s), along with
`test_network_isolation_manifest` and `architecture_boundaries`.
