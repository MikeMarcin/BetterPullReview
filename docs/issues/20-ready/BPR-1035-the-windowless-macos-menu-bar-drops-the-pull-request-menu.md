---
schema: "implement-rfc.card.v1"
id: "BPR-1035"
title: "The windowless macOS menu bar drops the Pull Request menu that BPR-0990 added to the window's bar"
status: "Ready"
rfcs: ["RFC-BPR-0033"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"found_by":"RFC-BPR-0033 closeout recheck on the Mac integrator, 2026-09-23","found_at":"1de0ca9a","introduced_by":"0d56cee5 (BPR-0990)","related":["BPR-0796","BPR-0782","BPR-0983"]}
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
