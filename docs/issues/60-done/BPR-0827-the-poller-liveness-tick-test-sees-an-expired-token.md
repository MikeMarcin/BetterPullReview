---
schema: "implement-rfc.card.v1"
id: "BPR-0827"
title: "The poller liveness tick test finds the liveness token already expired"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac"]
metadata: {"found_by":"dsp-002 BPR-0802 slice (pre-existing at HEAD)","found_at":"7fc0e498014c5266507fe909a468508053d962df","run":"run-20260916-stations","dispatch":"dsp-002"}
acceptance: [{"criterion":"application_poller_liveness passes on macOS, and the cause is named here","outcome":"pass","revision":"6db403bc2757a53454236d21b53de9a96d2bd285","evidence":{"kind":"command","detail":"Passes at d9ea1cbc, 10f30f04 and the integrated head (dsp2-int7/8/9 logs). Cause on the card: first bad 826fb871, BPR-0705 moved Liveness.reset() after the destructor's processEvents drain; fix clears Liveness first and gives OnClosed its own SessionCloseLifetime; restoring BPR-0705's order fails the test again. Multi-window, shutdown, crash-recovery and restart courses pass."}}]
---

# BPR-0827: The poller liveness tick test finds the liveness token already expired

## Defect

`application_poller_liveness` fails on macOS. Its subtest
`test_a_tick_armed_before_destruction_reaches_nothing`
(`tests/end_to_end/github_application_liveness_tests.cpp`) reports "the
liveness token has expired by the time that tick runs". HEAD's own test
source failed the same way before [[BPR-0802]] changed that file, with the
marker set or unset (`build/r2-logs/dsp2-0802-baseline-head-liveness.log`).
After BPR-0802, the file's new boundary subtest passes first, and then the
tick subtest fails as before.

The last recorded pass is `build/logs/bpr0054/ctest-dev.log`, 2026-08-28
(main was near `70e5d8c5`; that is not a proven build revision). The test
file last changed in `47de7ae8` ([[BPR-0695]], 2026-09-08).

The test guards the BPR-0215/BPR-0217 crash class: a tick armed before
destruction must reach nothing. A red guard for that crash class is itself
a risk.

## Scope

- Find the first bad revision and the mechanism. Either the application now
  expires its token earlier, which may be a change to shutdown or liveness
  semantics, or the test's precondition drifted.
- Keep the assertion's meaning: a token born expired would make every
  production poller a no-op. Do not weaken it.

## Acceptance

- `application_poller_liveness` passes on macOS, and the cause is named here.

## Cause and fix (dsp-002)

**First bad revision: `826fb871`** ([[BPR-0705]]), the same commit as
[[BPR-0826]] but a different mechanism. Parent `061dfcdf` passes (0.86 s);
the child fails with this card's message
(`build/r2-logs/dsp2-0826-{parent,child}-*.log`).

**A product regression.** Before BPR-0705, `~github_application` cleared
`Liveness` first, before anything pumped events. BPR-0705 added
`begin_shutdown()` and a `processEvents` drain to the destructor, and moved
`Liveness.reset()` after that drain. A poller tick armed before destruction
was then delivered while the token was still live, and re-entered windows
already shutting down: the BPR-0215/0217 crash class this test guards. The
token had moved after the drain because the window-session `OnClosed`
callback shared it, and that callback must run during the drain; clearing
the token first would have made the destructor loop forever.

**Fix `d9ea1cbcec2dc8a3109fae91405f8d0c2b311198`.** `Liveness.reset()` is the first line of the destructor
again. A private `SessionCloseLifetime` token guards only `OnClosed`, and is
cleared after the drain. The assertion's meaning is unchanged. Putting back
BPR-0705's order fails the test again with the original message
(`dsp2-0826-negative-controls-*`).

**macOS at `6db403bc2757a53454236d21b53de9a96d2bd285`.** `application_poller_liveness` passes
(`dsp2-int7-ctest.log`, `dsp2-int8-ctest.log`, `dsp2-int9-courses.log`).
So do the courses that exercise window close and the quit drain:
`application_profile_multi_window_public_path` (80.64 s),
`application_diff_workbench_shutdown_public_path`, both crash-recovery
courses and `application_device_binding_restart` (`dsp2-int7-ctest.log`).
