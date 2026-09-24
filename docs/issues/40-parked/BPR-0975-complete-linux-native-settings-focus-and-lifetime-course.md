---
schema: "implement-rfc.card.v1"
id: "BPR-0975"
title: "Complete Linux native Settings focus and window-lifetime course"
status: "Parked"
rfcs: []
depends_on: ["BPR-0731"]
priority: "P2"
platforms: ["linux"]
metadata: {"origin_rfc":"RFC-BPR-0018","origin_card":"BPR-0593","mandate_revision":"r2","disposition":"owner-deferred standalone follow-up; not an RFC0018 closeout dependency","source_candidate":"2d3bdbdfdef1951c3c2732d5c20d6ba65be9e273","transferred_criteria":["AC-01 Linux full native menu/shortcut/window-lifetime course"],"related_cards":["BPR-0961","BPR-1024"],"station":"bpr-main-linux","candidate":"b651554ff50bdde1c5a41fd9a77f6c0e075b16c8","evidence":"docs/verification/BPR-0975-linux-native-settings.md","owner_decision":{"date":"2026-09-24","session":"cbd48b91000b00f1e72e1ad62c7c384c","candidate":"BPR-0975-wslg-activation-v001","decision":"35aa7cb0-1d37-4588-b8ad-3cdf2764a9e4","outcome":"reject","direction":"find why GUI activation does not work on WSLg and fix it"}}
parked_on: "human-review"
---

# Complete Linux native Settings focus and window-lifetime course

## Diagnosis, 2026-09-24: WSLg has no activation path to use; the owner decides between two options

Station `bpr-main-linux`, claimed at `87ef6dfe`. The diagnosis is in the
evidence note, section
[Why WSLg refuses activation](../../verification/BPR-0975-linux-native-settings.md#why-wslg-refuses-activation-2026-09-24),
and its capture is
[`wslg-activation-probe-d24223f2.json`](../../verification/BPR-0975-linux-native-settings/wslg-activation-probe-d24223f2.json).
The probe ran in one desktop hold that bpr-main-win acknowledged: settle from
2026-09-24T20:41:58Z, run 20:43:58–20:44:34Z. The guard passed with 8/8 live
files unchanged, and every control exited 0.

**Why.** WSLg's compositor (weston-mirror `04d436c7`, WSLg 1.0.73.2) never
activates a window because an application asked. Its X11 window manager drops
`_NET_ACTIVE_WINDOW` unread (`xwayland/window-manager.c` 2217–2247), whatever
the source indication or timestamp. It also sets X11 focus back when a client
moves it (2606–2631). The compositor offers no `xdg_activation_v1` to Wayland
clients. Its shell activates a mapped window only on a click, or when Windows
activates the window's RAIL window (`rdprail-shell/shell.c` 3969–4029). It
also activates a window when it maps (2587–2622), and it focuses the top window
when the focused one goes away (919–960). Qt's request is already the one EWMH
asks for (`qxcbwindow.cpp` 1462–1513). No setting changes this, and neither
weston-mirror `working` nor upstream Weston `main` is different.
microsoft/wslg#1022 reports the same defect.

**Probe results.** B held focus and had just received a key, and A asked for
activation each time.

- **Refused, with no focus event on X11** (Xlib control): `_NET_ACTIVE_WINDOW`
  with source 0, 1 or 2 and any timestamp (no time, the key's time set as
  `_NET_WM_USER_TIME`, or a fresh server time), and `XRaiseWindow` followed by
  the request.
- **Undone** (Xlib control): `XSetInputFocus` gave FocusIn(A), then
  FocusIn(B) again.
- **Refused, bare Qt control:** `activateWindow()`, both on xcb and on
  Wayland.
- **Granted, the only way:** unmapping and mapping A again moved X11 or Qt
  focus to A, and the Windows foreground followed, in the Xlib control, in
  Qt on xcb and in Qt on Wayland.

Windows is not what blocks it. The Windows foreground followed the compositor
each time the compositor activated a window, with no RAIL activation by the
observer.

So no supported activation request works here, and there is no product fix
that uses one. Switching the application to Wayland would not help. The
upstream change that would fix it, in microsoft/weston-mirror and upstream
Weston, is written out in the evidence note, under
[The upstream change that would fix it](../../verification/BPR-0975-linux-native-settings.md#the-upstream-change-that-would-fix-it).
In short: handle `_NET_ACTIVE_WINDOW` in the X11 window manager, pass it to
the shell through a new activation entry, and grant it under a
focus-stealing rule; and add `xdg_activation_v1` to the RAIL shell. Nothing
has been filed.

### Decision for the owner

AC2 cannot pass on WSLg as the product and WSLg stand. Choose one:

- **A. A WSLg-only workaround in the product.** It is not an activation
  request. It relies on WSLg activating a window when it is mapped.
  - When Ctrl-comma or the menu reuses a visible Settings window that is not
    active, the product would hide and show it before raising it.
    - Windows would destroy the Settings window and create it again: its
      taskbar button would blink, and the window would flicker.
    - The Qt window and its X11 identity would not change.
  - Settings would also return focus, when it closes, to the workbench that
    last opened or reused it. WSLg focuses that workbench anyway, because it
    is the top window left. Today the product asks for the first opener,
    which WSLg refuses when a sibling was used later.
  - Both changes would be limited to xcb under WSLg. The composition would
    detect WSLg; if that touches pinned composition code, the pins are
    resealed and the review recorded here.
  - Expected, not yet shown: the full default course would then pass. That
    takes an implementation, the focused Settings suites, and another desktop
    hold for AC2, AC3 and AC4.
- **B. No product change; wait for WSLg.** The card goes to Blocked until a
  WSLg release ships the upstream change above.
  - AC2 stays failing on WSLg until then.
  - Whether to file the change with microsoft/wslg or microsoft/weston-mirror
    (for example on #1022) is the owner's call. The change text is ready.
  - The earlier alternatives, another Linux desktop or an amendment, were
    rejected on 2026-09-24. B leaves the card waiting unless one of them is
    reconsidered.

A and B can be combined: make the workaround now and also report the upstream
change. Nothing is implemented until the owner chooses. The card is Parked on
`human-review` until then; AC1 to AC4 are unchanged, and nothing here is
accepted.

## Owner direction, 2026-09-24: find out why WSLg refuses it, and fix it (attended session cbd48b91)

Source: attended session `cbd48b91000b00f1e72e1ad62c7c384c`, packet
[owner-decisions-20260924-v001](../../verification/reviews/owner-decisions-20260924-v001/operator-packet.md),
hand-back at sequence 589. Candidate `BPR-0975-wslg-activation-v001`, revision
`b651554f`, fingerprint `292bd12e…`, decision
`35aa7cb0-1d37-4588-b8ad-3cdf2764a9e4`, sequence 578, 2026-09-24T19:02:41Z,
`actor: human`. The question offered (a) a real Linux desktop or (b) an
amendment that accepts WSLg's refused activation. The owner chose
**reject**, turning down both, with this note, verbatim:

> Seems like we should be able to run GUI apps there. Figure out why it's not
> working and fix it.
>
> https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps

What that settles:

- **No amendment.** AC2 stands as written, and WSLg's refusal is not accepted
  as a platform difference.
- **No other desktop was offered.** The owner did not choose to supply a
  GNOME or KDE desktop. The course is to pass on the JaegAW WSLg host.
- **The work is to find out why activation is refused there, and fix it.** The
  note is direction, so the earlier position below, that re-mapping Settings
  "would work around the compositor, not fix a product defect, so it is not
  done here", no longer bounds the work. The fix goes in whatever layer the
  diagnosis names: the WSLg or Weston configuration on the station, how the
  application or Qt requests activation (for example the `_NET_ACTIVE_WINDOW`
  source indication and timestamp, or `_NET_WM_USER_TIME`), or the
  application's own window handling. It is recorded with a failing-before
  control.
- The AC4 limits still hold. Any change to the station's WSLg or display
  configuration is recorded as a station change and reverted afterwards, or
  justified as a documented prerequisite, rather than done quietly.
- [[BPR-1024]]'s correction (`xcb_activation_host`) stays in place, and the
  owner declined an upstream Qt report (`BPR-1024-upstream-qt-report-v001`).

The card leaves `window-manager-activation` for **Ready** on the Linux
station.

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
