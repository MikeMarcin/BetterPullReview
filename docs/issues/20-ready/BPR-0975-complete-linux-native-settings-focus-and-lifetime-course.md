---
schema: "implement-rfc.card.v1"
id: "BPR-0975"
title: "Complete Linux native Settings focus and window-lifetime course"
status: "Ready"
rfcs: []
depends_on: ["BPR-0731"]
priority: "P2"
platforms: ["linux"]
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Linux full native menu/shortcut/window-lifetime course"],"related_cards":["BPR-0961"]}
---

# Complete Linux native Settings focus and window-lifetime course

## Resumption, 2026-09-23

The owner's mandate relayed by the Mac integrator ("implement available
unattended work, focus on closing out rfcs") resumes this card on a different
station: `bpr-main-linux` (`stn-5c7459ed`), checkout
`/home/mmarcin/BetterPullReview`, branch `main`, running directly on the same
JaegAW WSL2/WSLg host instead of through the Mac remote wrapper. AC1's bare
two-window control is re-run first; the card returns to Blocked with the new
evidence if native two-window focus still fails.

## Owner transfer, distinct from observations

Owner amendment r2 to run `rfc-0018-0026-closeout-20260920` transfers the remaining
Linux native focus/lifetime course from RFC-0018 terminal acceptance. This
standalone card remains **Blocked**, not accepted. The
[amended RFC](../../rfcs/RFC-BPR-0018-robust-settings-center.md#owner-amendment-r2--2026-09-21)
and [native evidence](../../verification/BPR-0593-native-settings-continuation.md)
retain the unchanged requirement, failed course and independent control. The
passing inventory-only course is not a focus, keyboard or lifetime pass.

## Exact limitation and dependency

Configured station `stn-3f7fcc92` / `bpr-linux` is `/home/mmarcin/bpr-linux`,
`linux-lane`, reached through `aito-main-win` and WSL Ubuntu. Its observed live
HEAD is `1aabc2f3bbd619a8e26938b8c2e909e4a7058f3e`; its existing dirty MASK lock
was preserved. The actual target is Ubuntu 26.04 x86_64 WSL2 / WSLg XCB
`DISPLAY=:0`, GCC 15.2, Qt 6.11.2. The clean owned source and Release/tests-OFF
binary are:

- `/home/mmarcin/bpr0018-native-280167ac/source` at the source above;
- `/home/mmarcin/bpr0018-native-280167ac/bgcc/src/app/better_pull_review`,
  SHA-256 `8234abccb4141bbf52c91c568b25917abd2e3125f46f14e1a0073ee285b7a51e`.

The full native course cannot retain focus on mapped workbench XID `6291474`
(`WM_STATE=[1,0]`); focus stays on Settings XID `6291484`. The observer refuses
wrong-target key injection. Bounded X11 focus/sync, WM activation, exact edge
hit-testing and an interactive WSLg RAIL bridge on the confirmed `Default`
input desktop all failed. No display or security policy was changed.

The separate checked-in two-QWidget control, with no BetterPullReview code,
reproduces this on the same SDK/display:

```text
BARE_QT_TWO_WINDOW_FOCUS:FAIL
BARE_QT_POST_CLOSE_FOCUS:PASS
BARE_QT_NATIVE_CLOSE_EXIT:0
```

This isolates a station/Qt/observer limitation, not a demonstrated Settings
composition bug. The exact layer is unresolved. The prerequisite to the full
course is a usable native two-window focus surface or a justified observer/
station correction demonstrated by this control. The station was actually
connected and executed; it was not assumed unavailable. BPR-0731 is the Done
baseline inventory prerequisite. BPR-0961's Ubuntu 24.04/RHEL 10 package matrix
is related release work, not a new prerequisite for this Settings course.

## Acceptance

- AC1: Attribute or bound the exact native focus failure and demonstrate that
  the bare two-window control can focus each mapped window while its sibling
  stays open, then close both cleanly. Preserve the old failure and identify any
  changed station, window manager, toolkit or observer. Do not relax owned-PID,
  exact-image or target-focus checks or use offscreen events as native evidence.
- AC2: On the actual tests-disabled application, complete View/Settings,
  Ctrl-comma, modeless workbench focus, singleton Settings identity, Escape,
  same-profile sibling close/reopen and native Quit with terminal process exit
  0. The full default course must pass; `--inventory-only` is not a substitute.
  Resolve any product failure exposed after focus becomes usable.
- AC3: Bind source/build flags, executable digest, native environment and
  observed image/PID to the full capture. Retain all seven category assertions
  and explicit capability differences. Reuse the existing current inventory
  only if its relevant inputs remain unchanged; rerun its comparison otherwise.
- AC4: Use fresh mode-0700 XDG data/config/cache/state/TMP roots, the station
  lease and unchanged-live-state guard; wait all children and release the lease.
  No personal profile, authentication, provider mutation, setting-change,
  maintenance operation or host security/display-policy change is authorized.

## Ready commands and retained evidence

Recheck the station and exact candidate before these retained absolute commands;
the guard creates fresh outputs and bounds observer lifetime to 180 seconds:

```sh
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py --linux /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_linux_control_guard.sh
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_remote.py --linux /Users/jaeger/.codex/worktrees/0152/BetterPullReview/build/rfc0018_linux_guard.sh
python3 /Users/jaeger/.codex/worktrees/0152/BetterPullReview/tests/scripts/settings_inventory_comparison_tests.py
```

The control wrapper's exit 0 accounts for cleanup; require the actual
`BARE_QT_TWO_WINDOW_FOCUS` observation to pass before calling AC1 complete.
Checked-in tools are
[`linux_settings_focus_control.cpp`](../../../tests/scripts/linux_settings_focus_control.cpp)
and [`linux_settings_inventory.py`](../../../tests/scripts/linux_settings_inventory.py).
The full guard calls
`/mnt/c/Users/mikem/bpr0018-native-280167ac/linux-course.sh` and the default full
`linux-launch.py` mode; do not replace it with `--inventory-only`.

The owned roots remain inert; the last independent control is in
`/home/mmarcin/bpr0018-native-280167ac/focus-control-h5ztktcb`. Local logs
`build/rfc0018-linux-{course21,focus-control-final,inventory-final,baseline-gate}.log`
retain the failures and separate inventory pass. That inventory has seven pages
and native close/exit 0; it compares six pages exactly with Mac and omits only
Mac's documented CLI button. The candidate retains one GCC
`-Wno-error=ignored-attributes` demotion, so no clean compiler-policy pass is
claimed. Full DEB/RPM qualification remains RFC-0038/BPR-0961. No native course
is newly run or accepted by this card's creation.
