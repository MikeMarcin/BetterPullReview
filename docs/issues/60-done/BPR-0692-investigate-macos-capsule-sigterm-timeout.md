---
schema: "implement-rfc.card.v1"
id: "BPR-0692"
title: "Investigate macOS capsule SIGTERM cleanup timeout"
status: "Done"
rfcs: ["RFC-BPR-0016"]
depends_on: []
platforms: ["mac"]
acceptance: [{"criterion":"Find the blocked cleanup operation using fixture-only observation","outcome":"pass","revision":"6818f222","evidence":{"kind":"observation","detail":"Post-SIGTERM window spent 10.06 s on the bounded process-group reap (term grace) and up to 17.6 s on the unbounded ordinary hdiutil detach under load; sample shows hdiutil idle in mach_msg and DiskArbitration log shows a 10.7 s eject. Over-30 s run not reproduced (worst 27.7 s); recorded as residual."}},{"criterion":"Preserve process-group reap and image detach requirements without merely raising timeouts or suppressing the case","outcome":"pass","revision":"6818f222","evidence":{"kind":"observation","detail":"Reap now asserted from the signal at production term+kill grace+10 s (tighter); detach keeps a 180 s hang bound; interrupted-by-signal and hdiutil info image-gone checks unchanged; SIGKILL-escalation mutation fails in 38.1 s rather than hanging."}},{"criterion":"Run the focused failing case and relevant capsule cleanup checks after correction and record native result and cleanup","outcome":"pass","revision":"6818f222","evidence":{"kind":"command","detail":"Focused case 3/3 pass (20.2/17.8/18.9 s); full capsule suite 44/44 in 26.3 s; 0 leftover named processes and 0 test capsule mounts after each run. build/r2-logs/bpr0692-fixed-*.log"}}]
---

# Investigate macOS capsule SIGTERM cleanup timeout

## Scope

Investigate the existing native capsule cancellation contract failure observed
while running the required core-only UX integration checks. This is independent
of profile/Settings source changes; it is not a reason to claim a green full
suite or to stop independent UX implementation.

## Evidence

At `2b40da44` plus ongoing UX changes, core-only ran 128 tests in445.72s.
125 passed; two URL-layer boundary failures were corrected and passed narrow
rechecks. The remaining failure is
`CapsuleImageTests.test_sigterm_reaps_blocked_child_group_and_detaches_real_image`
in `tests/scripts/designated_live_macos_capsule_tests.py`: child
`stage-zero --probe-launch` did not complete within30s after SIGTERM.
The full capsule suite ran44 cases in114.239s with this one error.

Direct isolated reproduction:
`python3 tests/scripts/designated_live_macos_capsule_tests.py CapsuleImageTests.test_sigterm_reaps_blocked_child_group_and_detaches_real_image`
failed with the same timeout in62.714s. No related production/script source was
changed by the UX work. Both attempts' named child processes were gone and
`hdiutil info` showed no test capsule mounts after completion. Root cause is
unresolved; neither timing noise nor a production defect is asserted.

## Acceptance

Find the blocked cleanup operation using fixture-only observation. Preserve
process-group reap and image detach requirements; do not merely raise timeouts
or suppress the case. Run the focused failing case and relevant capsule cleanup
checks after correction; record actual native result and cleanup. No live
credential, designated repository mutation or UX change is authorized here.

## Evidence, 2026-09-17 (run-20260916-stations, dispatch dsp-001-r2)

**Blocked operation.** The single `communicate(timeout=30)` window after
SIGTERM covered two operations with different clocks. The first is stage
zero's process-group reap, bounded by its own graces: it took 10.06 s at
every load level, 10 s of that being the term grace forced by the fixture's
SIGTERM-ignoring descendant. The second is the ordinary image detach, which
waits on DiskArbitration and which stage zero does not bound. Idle,
`hdiutil detach` takes 0.12 s. Under 28 spinning processes it took 4.20,
12.54 and 14.10 s; under 56, 17.64 and 14.71 s. The whole post-SIGTERM phase
reached 27.7 s. During the 14.1 s detach a `sample` showed `hdiutil` idle in
`CFRunLoopRun` → `mach_msg2_trap`. The DiskArbitration unified log for that
process shows the unmount queued at 21:35:09.586 and done at 21:35:12.986,
then the eject of `/dev/disk8` queued at 21:35:12.995 and succeeded at
21:35:23.667 (10.7 s). The detach starts only after the reap completes, and a
busy mount makes detach fail fast (exit 16 in 0.096 s) rather than hang, so
the reap-then-detach order is correct. The card's own traceback timed out
while still reading stage zero's pipes, before any reap, which fits this
account.

**Correction** (`6818f222`, test only; production stage zero unchanged). The
reap is now asserted from the signal at
`STAGE_ONE_TERM_GRACE_SECONDS + STAGE_ONE_KILL_GRACE_SECONDS + 10 s` using the
production constants, which is tighter and independent of load. The detach
keeps a 180 s hang bound instead of a promptness claim. The "stage zero
interrupted by signal" message and the `hdiutil info` check that the image is
gone are unchanged. The fixture helper and descendant loop for 300 s instead
of forever, as BPR-0454's helper already did. This is not a bare timeout raise:
the reap bound tightened and only the unbounded OS eject gained headroom.

| Check | Result | Revision |
|---|---|---|
| Focused case ×3 (`python3 tests/scripts/designated_live_macos_capsule_tests.py CapsuleImageTests.test_sigterm_reaps_blocked_child_group_and_detaches_real_image`) | pass: 1/1 each, 20.2 s, 17.8 s, 18.9 s | tree identical to `6818f222` |
| Full capsule suite | pass: 44/44 in 26.3 s | same |
| Mutation: SIGKILL escalation removed in a scratch copy | fails `process 22551 remained alive` in 38.1 s (exit 1) instead of hanging | scratch only |
| Cleanup after every post-fix run | 0 leftover named processes and 0 test capsule mounts in `hdiutil info` | — |

Ignored logs are under `build/r2-logs/bpr0692-*`. They include the load
observations, `bpr0692-load28-hdiutil-detach-sample.txt`,
`bpr0692-load28-3-detach-unified-log.txt` and `bpr0692-busy-detach.log`.

**Residual.** A run over 30 s was not reproduced on this Mac: the worst was
27.7 s under 56 spinners, and the card's failing run was under heavy C++
compile load. That the detach crossed the old window there is a
well-supported inference, not a reproduction, and the corrected case was not
re-run under load. A separate defect was observed. With several copies of the
suite running at once, one `hdiutil create` returned while its write-once
device was still attached, and that copy's stage-zero attach then failed and
left the device attached. It was cleaned up with an ordinary detach and is
recorded for its own card.
