---
schema: "implement-rfc.card.v1"
id: "BPR-1035"
title: "The windowless macOS menu bar drops the Pull Request menu that BPR-0990 added to the window's bar"
status: "Done"
rfcs: ["RFC-BPR-0033"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"RFC-BPR-0033 closeout recheck on the Mac integrator, 2026-09-23","found_at":"1de0ca9a","introduced_by":"0d56cee5 (BPR-0990)","related":["BPR-0796","BPR-0782","BPR-0983"]}
acceptance: [{"criterion":"AC1 application_profile_empty_desktop_native_menu_bar passes with its windowed and windowless snapshots equal, and fails on the menu comparison without the fix","outcome":"pass","revision":"871c1185","evidence":{"kind":"command","detail":"At 1de0ca9a it failed with 'closing the last window takes no menu off the bar': windowed Apple, Profiles, Pull Request, View against windowless Apple, Profiles, View. At 871c1185 it passes (6.1-6.2 s) with both snapshots Apple, Profiles, Pull Request, View and a 0 ms stretch without the resident bar. The screen was locked, so the activation gesture was not run, as the course reports.","path":"docs/issues/60-done/BPR-1035-the-windowless-macos-menu-bar-drops-the-pull-request-menu.md#evidence"}},{"criterion":"AC2 The windowless Pull Request menu mirrors the window's titles and order, and its items are disabled and connected to nothing","outcome":"pass","revision":"871c1185","evidence":{"kind":"command","detail":"qt_profile_management_window requires P&ull Request with &Follow (checkable) and &Save Repository, both disabled, and the bar's order File, Profiles, Pull Request, View. desktop_menu_bar.cpp connects neither action, so the bar holds no window state and performs no effect."}},{"criterion":"AC3 qt_profile_management_window and the profile desktop courses still pass","outcome":"pass","revision":"871c1185","evidence":{"kind":"command","detail":"18/18 in 741.07 s at zero pending build actions (build/rfc0033-close-int-pass-871c1185.log): the four application_profile_* host courses, qt_profile_management_window, profile_window_restoration_fixture_contract, application_agent_crash_recovery and the eleven profile public paths including the native menu-bar course."}}]
---

# BPR-1035: The windowless macOS menu bar drops the Pull Request menu

## Defect

[[BPR-0990]] (`0d56cee5`, 2026-09-23) added a **Pull Request** menu, with
Follow and Save Repository, to the workbench window's menu bar, between
Profiles and View. The windowless desktop bar (`qt::desktop_menu_bar`) was not
given one. On macOS, closing the last window therefore takes a menu off the
bar, which is what the owner rejected on 2026-09-16 ("Closing last window
closes menu bar.") and what [[BPR-0796]] fixed for View. RFC-BPR-0033 B1
promises that "the menu bar remains (File, Profiles, View and the rest)".

`application_profile_empty_desktop_native_menu_bar` catches it. At `1de0ca9a` it
fails with `closing the last window takes no menu off the bar`, and its own
snapshot names the difference:

- windowed top-level menus: Apple, Profiles, Pull Request, View
- windowless top-level menus: Apple, Profiles, View

[[BPR-0983]] saw the same failure with the screen locked and attributed it to
the console. The course already works around a locked session: it delivers
key-window changes through the QPA entry point when macOS refuses activation.
It then fails on the menu comparison, which is deterministic.

## Outcome

With no window, the macOS menu bar carries the same menus a window's bar
carries, including Pull Request, offered and disabled as View is, so the close
subtracts nothing.

## Acceptance

- `application_profile_empty_desktop_native_menu_bar` passes, with its
  windowed and windowless snapshots equal, and it fails on the menu comparison
  without the fix.
- The windowless Pull Request menu mirrors the window's titles and order, and
  its items are disabled and connected to nothing. The desktop bar stays free
  of window state and effects, as BPR-0777 and BPR-0796 require.
- `qt_profile_management_window` and the profile desktop courses still pass.

## Boundaries

No GitHub or GHES mutation. No queue, follow or saved-repository behaviour
changes. The core decides, the host performs, Qt presents.

## Resolution — 2026-09-23, Mac integrator (Mac batch 2)

`qt::desktop_menu_bar` offers a **Pull Request** menu between Profiles and
View, as the window's bar has it: "P&ull Request", with "&Follow" (checkable)
and "&Save Repository", both disabled and connected to nothing, as BPR-0796
offers View. With no window no pull request is open, so there is nothing
either gesture could act on. The bar still holds no window state and performs
no effect. `pull_request_menu()` exposes it for the component test (`871c1185`).

[[BPR-0983]]'s reading of the same failure, that the course needs an active,
unlocked console, was wrong about the cause. The course runs with the screen
locked: it reports "the session refused activation (screen locked: yes,
frontmost: loginwindow)" and delivers key-window changes itself. What it
cannot do locked is run the activation gesture, and it says so.

## Evidence

macOS 26.6.2 arm64, `build/dev`, screen locked:

- **Before** (`1de0ca9a`, the RFC-BPR-0033 recheck pass,
  `build/rfc0033-close-int-pass.log`):
  `application_profile_empty_desktop_native_menu_bar` fails with `closing the
  last window takes no menu off the bar`, and its snapshots read windowed
  "Apple, Profiles, Pull Request, View" against windowless "Apple, Profiles,
  View".
- **After** (`871c1185`): it passes (6.2 s). Both snapshots read "Apple,
  Profiles, Pull Request, View", with a 0 ms stretch without the resident bar
  through the close. It also reports "the activation gesture was not run",
  which the locked screen prevents.
- `qt_profile_management_window` passes. Its desktop-bar test now requires Pull
  Request with its two items disabled, and the bar's order File, Profiles, Pull
  Request, View.
- clang-format 23.1.1 passes all three files, and their `HEAD` versions were
  clean. The project tidy script reports only `home_view_state.hpp:35`, a header
  this change does not touch.
- The RFC-BPR-0033 recheck pass at `871c1185` ran with zero pending build
  actions before and after, on app sha256 `1659f055…00bdd`
  (`build/rfc0033-close-int-pass-871c1185.log`). All 18 courses passed in
  741.07 s, including the native course, `qt_profile_management_window` and
  every profile desktop public path.
