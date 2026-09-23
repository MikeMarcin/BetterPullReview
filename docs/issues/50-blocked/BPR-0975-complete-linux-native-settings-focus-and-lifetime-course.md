---
schema: "implement-rfc.card.v1"
id: "BPR-0975"
title: "Complete Linux native Settings focus and window-lifetime course"
status: "Blocked"
rfcs: []
depends_on: ["BPR-0731"]
priority: "P2"
platforms: ["linux"]
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Linux full native menu/shortcut/window-lifetime course"],"related_cards":["BPR-0961","BPR-1024"],"station":"bpr-main-linux","candidate":"b651554ff50bdde1c5a41fd9a77f6c0e075b16c8","evidence":"docs/verification/BPR-0975-linux-native-settings.md"}
blocked_by: "window-manager-activation"
---

# Complete Linux native Settings focus and window-lifetime course

## Current state, 2026-09-23

The owner's mandate relayed by the Mac integrator ("implement available
unattended work, focus on closing out rfcs") resumed this card on station
`bpr-main-linux` (`stn-5c7459ed`), checkout `/home/mmarcin/BetterPullReview`,
branch `main`, directly on the same JaegAW WSL2/WSLg host. The card is
**Blocked** again, on `window-manager-activation`; nothing here is accepted by
a human or qualifies a package. Evidence:
[BPR-0975 Linux native Settings](../../verification/BPR-0975-linux-native-settings.md).

The Mac integrator reported that `bpr-main-win` took the Windows foreground
during the committed runs. Repeats with nothing else taking it gave the same
control results, the same Ctrl-comma failure and the same diagnosis pass
([clear-window reproduction](../../verification/BPR-0975-linux-native-settings.md#clear-window-reproduction)).
The table below is unchanged. [[BPR-1024]] carries the key-routing consequence
of a refused activation as its own defect.

| Criterion | Outcome at `b651554f` |
|---|---|
| AC1 | **Pass.** The failure is attributed. WSLg's `Weston WM` returns X11 focus to the compositor-focused window and ignores `_NET_ACTIVE_WINDOW`; a toolkit-free Xlib control shows the same, so Qt is not the cause. Both the Qt and Xlib controls focus each window through Windows' own RAIL foreground activation while the sibling stays mapped, focus A after B closes, and exit 0. The X11-only failure is preserved in the same capture. |
| AC2 | **Fail on this platform.** The full default course stops at Ctrl-comma. The app reuses the one Settings window and asks for its activation, and the window manager refuses. Qt then applies the refused request to itself, so its keyboard routing can diverge from X11 focus. A labelled diagnosis that records each refusal and continues passes every other step, native Quit and exit 0. It is not acceptance. |
| AC3 | **Bound, except the full capture.** Clean tests-disabled `app-release` at `b651554f`, no `-Wno-error`, digest `13c59e94…b984`; environment, PID and image are recorded. The seven pages are identical to the 2d3bdbdf Linux inventory, and the Mac comparison was rerun (16 tests). The full capture itself waits on AC2. |
| AC4 | **Held for every run.** Fresh mode-0700 XDG/TMP roots, station-writer lease, live-state guard with 8/8 files unchanged. All children waited. No sign-in, setting, maintenance, provider, display, security or foreground-lock change. |

To unblock AC2, supply one of these:

- a native Linux desktop whose window manager honours EWMH application
  activation, such as GNOME or KDE on Ubuntu 24.04 (the [[BPR-0961]] baseline
  would serve), on which to run the unchanged full course;
- an owner amendment that records WSLg's refusal of application activation as
  an explicit Linux/WSLg capability difference, with the diagnosis as the
  evidence for the other steps.

Re-mapping Settings to force focus on WSLg would work around the compositor,
not fix a product defect, so it is not done here.

## Owner transfer, distinct from observations

Owner amendment r2 to run `rfc-0018-0026-closeout-20260920` transfers the remaining
Linux native focus/lifetime course from RFC-0018 terminal acceptance. This
standalone card remains **Blocked**, not accepted. The
[amended RFC](../../rfcs/RFC-BPR-0018-robust-settings-center.md#owner-amendment-r2--2026-09-21)
and [native evidence](../../verification/BPR-0593-native-settings-continuation.md)
retain the unchanged requirement, failed course and independent control. The
passing inventory-only course is not a focus, keyboard or lifetime pass.

## Recorded limitation, 2026-09-21

The 2026-09-23 attribution above resolves the layer this record left open.

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

On a Linux desktop that satisfies AC2's prerequisite, build a clean
tests-disabled `app-release` from the exact commit in an owned root, then run
the checked-in courses there. Use the system `/usr/bin/python3`, which has
PyGObject, with the AT-SPI typelib available, under the station lease and the
live-state guard:

```sh
/usr/bin/python3 tests/scripts/linux_settings_focus_control_course.py --qt-root QT_SDK --work-root OWNED_ROOT --output OWNED_ROOT/linux-focus-control.json
/usr/bin/python3 tests/scripts/linux_settings_course.py --build-dir OWNED_BUILD --revision EXACT_SHA --work-root OWNED_ROOT --output OWNED_ROOT/linux-full-course.json
python3 tests/scripts/settings_inventory_comparison_tests.py
```

A desktop that is not WSLg uses `--focus-surface x11`, the default there. The
exact WSLg commands and their `guarded.sh` wrapper are in the
[evidence file](../../verification/BPR-0975-linux-native-settings.md#reproducing).
The full course fails on a refused application activation. Its
`--bound-refused-activation` and `--inventory-only` modes are diagnoses, never
AC2 acceptance.

The 2026-09-21 Mac-driven commands and their logs are retained in the
[BPR-0593 continuation](../../verification/BPR-0593-native-settings-continuation.md#verification-and-reproducing-the-remaining-work).
The owned roots `/home/mmarcin/bpr0018-native-280167ac` and
`/home/mmarcin/bpr0975-native` are inert. The earlier candidate's GCC demotion
is gone: `b651554f` builds with no `-Wno-error`. Full DEB/RPM qualification
remains RFC-0038/BPR-0961.

## GCC 15 prerequisite and composition pin review, 2026-09-23

The dev preset did not build at `4380f310` under GCC 15.2 `-Werror`, and the
tests-disabled `app-release` graph did not either. Both now build with no
demoted diagnostic: `f863ce1b` (stateless `closedir` deleter and three test
warnings) and `233e1ebd` (three [[BPR-0822]]-shape aggregate members named;
the aggregate survey over `build/dev` and the `app-release` database reports no
gated `src/` site).

`f863ce1b` also restructured `cli_read_offered_here` in
`src/app/src/github_application.cpp` so GCC would not reject
`cli_read_offered` as unused in a non-Apple tests-enabled graph. That was
wrong twice over: it changed pinned code without resealing
`application_review_round_trip_composition_contract` (red on main, reported by
bpr-main-win), and it broke the CLI consent gate shape that
`application_composition_contract` pins ([[BPR-0797]], [[BPR-0982]]).

Pin review for the reseal: the only change to the pinned scope since the
BPR-0990 reseal (`0d56cee5`, whose pins matched at `f863ce1b^`) is this
card's. The correction restores `cli_read_offered_here` byte for byte and
instead marks `cli_read_offered` `[[nodiscard, maybe_unused]]` with a comment
saying why. Net change against `f863ce1b^`: one attribute and two comment
lines. Every graph's answer is unchanged — true on shipped macOS,
`!inside-boundary` in tests-enabled macOS (widened only by fixture-discovered
accounts), false elsewhere. No credential, vault, provider, transport or
mutation path changes. Full application
`dfd9605e84e9ccf83abc2db375ec75d407028962bec44a81e2bbddfa035f5723`; production
manifest `9fff94699a1dfe815afaeda9d6f25db19ffc4f404d13750956b6837381e08559`;
mutation branch unchanged at
`a7e2081e57d4344efdf0caf8d6ad63568ba5717a6a9b0a61d6e57ddf053d1e8a`.

- `application_review_round_trip_composition_contract` fails before the
  reseal (full digest drift) and passes after it.
- `cli_consent_gate_findings` over the corrected file returns no finding;
  with `f863ce1b`'s shape it reported the `cli_read_offered_here` finding.
- `application_composition_contract` after the correction fails only on
  [[BPR-0983]]'s recorded assertions: the three vault-selection subtests, the
  boundary-reader test and the release-graph scan. The CLI gate finding is
  gone (`build/bpr0975/composition-contract-after.log`).
- `conditional_branch_identifiers` reports two findings in
  `setup_destination_public_path_tests.cpp`'s Apple-only
  `check_denied_read_retains_session`; the same two are reported on an
  export of `f863ce1b^`, so they predate this card.
- The `core-only` preset configures and builds 676/676 under GCC 15.2. 151 of
  its 154 tests pass. Two failures predate this card:
  `conditional_branch_identifiers`, above, and `msvc_secure_crt_contract`,
  which flags `std::getenv` in the 2026-09-20 `release_update_*` end-to-end
  tests. The third, `contained_temporary_usage_contract`, was this card's: the
  two new course scripts used `tempfile.mkdtemp`. They now use
  `scripts/contained_temporary_support`, and the contract passes.
