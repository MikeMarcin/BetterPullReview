---
schema: "implement-rfc.card.v1"
id: "BPR-1122"
title: "WSLg refuses application window activation, so a reused Settings window can't take focus; recheck when WSLg ships the upstream fix (microsoft/wslg#1022)"
status: "Blocked"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["linux"]
blocked_by: "upstream-wslg"
metadata: {"origin":"owner verdict on attended candidate BPR-0975-wslg-choice-v001, session 7083474240820608cdf9ff551ddd298f, decision dce91e77-3dc2-49c4-b29e-fb81ca541b82, 2026-09-25","origin_card":"BPR-0975","related_cards":["BPR-0975","BPR-1024","BPR-0961"],"upstream":{"issue":"microsoft/wslg#1022","observed":"WSLg 1.0.73.2, microsoft/weston-mirror 04d436c7 (weston 9.0.0-213-g04d436c7), rdprail-shell","report":"not filed; the owner declined preparing one (candidate BPR-0975-upstream-report-v001, decision c5d0e284-d3f6-4563-ac30-abccbe01a6d2)"},"change_text_source":"docs/verification/BPR-0975-linux-native-settings.md at 2e6e64d4, section 'The upstream change that would fix it'","reservation":"docs/issues/_reservations/BPR-1122--BPR-1122--c71b4618324f.json"}
---
# BPR-1122: WSLg refuses application window activation, so a reused Settings window can't take focus; recheck when WSLg ships the upstream fix (microsoft/wslg#1022)

## Why

[[BPR-0975]]'s Linux native Settings course runs on the JaegAW WSL2/WSLg host.
Its diagnosis, in the
[evidence note](../../verification/BPR-0975-linux-native-settings.md#why-wslg-refuses-activation-2026-09-24),
found that WSLg's compositor never activates an already-mapped window because
an application asked. Its X11 window manager drops `_NET_ACTIVE_WINDOW`
unread, puts X11 focus back when a client moves it, and offers no
`xdg_activation_v1` to Wayland clients. The shell activates a window only on a
click, when Windows activates the window's RAIL window, when a window maps, or
when the focused window goes away. The probe
([`wslg-activation-probe-d24223f2.json`](../../verification/BPR-0975-linux-native-settings/wslg-activation-probe-d24223f2.json))
saw every activation request refused, on X11 and on Wayland, whatever its
source indication or timestamp.

What that means in the product, on WSLg only:

- **Ctrl-comma, or View → Settings…, reuses a visible Settings window that is
  not active, but focus stays on the workbench.** The product asks for the
  window's activation, the way EWMH asks for it, and WSLg refuses.
- **Closing Settings cannot return focus to a sibling workbench used later.**
  Settings asks for the workbench that last opened or reused it, and WSLg
  refuses that too. WSLg then focuses whatever window is on top.

The owner chose to accept this behaviour with no product change, and to wait
for the upstream fix. Attended session `7083474240820608cdf9ff551ddd298f`,
candidate `BPR-0975-wslg-choice-v001`, decision
`dce91e77-3dc2-49c4-b29e-fb81ca541b82`, 2026-09-25T00:12:30Z, `actor: human`,
note verbatim:

> No product change, accept the current behavior and file a focused blocked
> card for the settings activation issue in WSL to recheck when the upstream
> ships a fix.

This is that card. BPR-0975 is Done by the owner's amendment. The owner also
declined to have an upstream report prepared (candidate
`BPR-0975-upstream-report-v001`, decision `c5d0e284`, "No: do not prepare
it"), so nothing is filed. microsoft/wslg#1022 (Emacs `other-frame` cannot
switch frames, open since 2023-03) reports the same defect.

## The upstream change that would fix it

Copied verbatim from the BPR-0975 evidence note at `2e6e64d4`, section
[The upstream change that would fix it](../../verification/BPR-0975-linux-native-settings.md#the-upstream-change-that-would-fix-it).
Line numbers are weston-mirror `04d436c7`'s.

The fix belongs in microsoft/weston-mirror, and in upstream Weston, which has
the same X11 window manager. Nothing has been filed.

1. `xwayland/window-manager.c`: in `weston_wm_handle_client_message`, add a
   branch for `wm->atom.net_active_window`. It hands the window's shell
   surface, the source indication (`data32[0]`) and the timestamp
   (`data32[1]`) to the shell. That makes the advertisement in
   `_NET_SUPPORTED` true.
2. `xwayland/xwayland-internal-interface.h`: add a `request_activate(shsurf,
   source, timestamp)` entry. `libweston-desktop/xwayland.c` (the table at
   462–480) implements it with a new optional `activation_requested` callback
   in `struct weston_desktop_api`
   (`include/libweston-desktop/libweston-desktop.h`), the way `set_minimized`
   reaches `minimized_requested` (`libweston-desktop.c` 239–244).
3. `rdprail-shell/shell.c`: register `.activation_requested` in
   `shell_desktop_api` (3403–3420). Implement it as `activate(shell,
   shsurf->view, seat, WESTON_ACTIVATE_FLAG_CONFIGURE)` for each seat, behind
   a focus-stealing rule. The rule grants a pager request (source 2), and an
   application request (source 1) when the requesting client owns the focused
   surface or its timestamp is no older than that surface's last user input.
   `activate()` already informs Windows, and the probe shows Windows following.
4. For Wayland clients: add an `xdg_activation_v1` global to the RAIL shell
   that issues a token for a recent input serial and calls the same
   `activate()`. Qt 6.11.2 uses it with no application change.

## What unblocks it

A WSLg release whose compositor grants an application's activation request.
That means the X11 window manager handles `_NET_ACTIVE_WINDOW`, the RAIL shell
offers `xdg_activation_v1`, or both. Watch microsoft/wslg#1022 and the WSLg
release notes. The installed version is what `wsl --version` reports as WSLg
and what the compositor's log names as its weston build. Until such a release
is installed on the station, the card stays Blocked on `upstream-wslg`. No
station setting can stand in for it; BPR-0975 found none that changes
activation.

## Acceptance (the recheck)

- AC1 The installed WSLg version and its weston-mirror commit are recorded, and
  the release is shown to carry an activation change (its release notes,
  commit or source).
- AC2 `wslg_activation_probe_course.py` on that WSLg shows an application's
  activation request granted while its sibling stays mapped. At least the Xlib
  `_NET_ACTIVE_WINDOW` request with source 1 and the key's time, and Qt's
  `activateWindow()` on xcb, move X11 focus and the Windows foreground to the
  requesting window. If they are still refused, the card stays Blocked and
  records the versions it saw.
- AC3 BPR-0975's full default course passes, without
  `--bound-refused-activation`, on a clean tests-disabled `app-release` built
  from a current `main` commit. That is View/Settings, Ctrl-comma reuse with
  focus, modeless workbench focus, singleton identity, Escape, same-profile
  sibling close and reopen, and native Quit with exit 0. BPR-0975 AC3's
  binding applies to the capture: source and build flags, executable digest,
  native environment, observed PID and image, and the seven-page inventory
  comparison.
- AC4 BPR-0975 AC4's isolation holds for every run: fresh mode-0700 XDG and TMP
  roots, the station lease and the live-state guard, all children waited. No
  WSLg, display, security or foreground-lock setting is changed.
- A product failure the full course exposes once focus works is fixed here or
  carded.

## Commands

The owned-root commands from BPR-0975's
[Reproducing](../../verification/BPR-0975-linux-native-settings.md#reproducing)
section, run under the station lease with the `guarded.sh` wrapper described
there:

```sh
W=/home/mmarcin/bpr0975-native
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/wslg_activation_probe_course.py --qt-root $W/cache/bprqt/6.11.2/6.11.2/gcc_64 --work-root $W/runs --output $W/runs/wslg-activation-probe.json
$W/guarded.sh /usr/bin/python3 $W/source/tests/scripts/linux_settings_course.py --build-dir $W/source/build/app-release --revision $(git -C $W/source rev-parse HEAD) --work-root $W/runs --output $W/runs/linux-full-course.json
python3 /home/mmarcin/BetterPullReview/tests/scripts/settings_inventory_comparison_tests.py
```

Rebuild the owned clean clone at the chosen `main` commit first, and record
its executable digest.

## Boundaries

- No product workaround. The owner chose none, so the WSLg-only remap
  (hiding and re-showing a reused Settings window) is not made.
- No upstream report is filed unless the owner asks for one.
- [[BPR-1024]]'s `xcb_activation_host` correction stays.
- Station-local, read-only checks only; the platform eligibility is Linux on
  the WSLg host. A native GNOME or KDE desktop would pass a different course
  and does not unblock this card.
