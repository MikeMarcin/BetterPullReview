---
schema: "implement-rfc.card.v1"
id: "BPR-1250"
title: "Closing the last window takes Help off the macOS menu bar"
status: "Parked"
rfcs: []
depends_on: []
platforms: ["mac"]
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1250--BPR-1250--34ad467c0959.json","related_cards":["BPR-1035","BPR-1121","BPR-1157"]}
parked_on: "human-review"
acceptance: [{"criterion":"application_profile_empty_desktop_native_menu_bar passes in the owner's Aqua session","outcome":"pending","revision":"5db77be71","evidence":{"kind":"command","detail":"Passes on macOS arm64 at 5db77be71 with the screen locked (QPA key-window path), 7.3 s; the unlocked Aqua run in the owner's session is owed."}},{"criterion":"Owner judgment of which commands windowless Help offers","outcome":"pending","revision":"5db77be71","evidence":{"kind":"freeform","detail":"Attended item BPR-1250-help-v001, session cbd48b91000b00f1e72e1ad62c7c384c, ready and protected; no server started."}}]
---

# BPR-1250: Closing the last window takes Help off the macOS menu bar

## Observation

At `main` 580589740 on macOS arm64, in an unlocked Aqua session where AppKit
made the window key, `application_profile_empty_desktop_native_menu_bar`
fails at `closing the last window takes no menu off the bar`
(`tests/end_to_end/profile_empty_desktop_native_menu_bar_tests.cpp:415`). The
windowed top-level menus are Apple, File, Profiles, Pull Request, View and
Help; windowless they are Apple, File, Profiles, Pull Request and View. The
trace shows the resident desktop's Help as `~Help[]`, empty and hidden. (An
earlier run in the same pass, with the screen locked, failed the same way.)

The windowed Help is the workbench window's diagnostic menu
(`workbench_window.cpp`, RFC-BPR-0027's exports, 75700f4dd and e0a870b34 on
2026-09-26). The resident menu's Help is `release_update_menu`, whose only item,
Check for Updates…, is hidden on native macOS, so the Mac drops the empty
menu. The trace also shows File losing Open Project… windowless
([[BPR-1157]]); the course compares titles only.

## Outcome

The windowless bar keeps every top-level menu the windowed bar has, with
windowless Help offering what applies without a window. Which diagnostic
commands belong there is a UX choice to settle before implementing.

## Direct acceptance

- `application_profile_empty_desktop_native_menu_bar` passes in the owner's
  Aqua session.

## Candidate (red-test backlog milestone, macOS arm64, 2026-09-30)

`5db77be71`. The resident `desktop_menu_bar` now offers the window's Help
(Diagnostics…, Export anonymized diagnostics…, Export with identifying
details…) under the object name `releaseUpdateMenu`, so
`release_update_menu::attach` puts Check for Updates… into the same menu, as it
does in a window. All three are disabled: Diagnostics opens a window's Settings
and an export is a profile's (`present_diagnostic_export`), and with no window
neither has an owner. This follows the accepted pattern for View
([[BPR-0796]]) and Pull Request ([[BPR-1035]]).

With Help back, the course reached its next check, that the bar reads the same
before and after the close, and found three more window commands the resident
bar never mirrored, all added after the course last changed on 2026-09-25:
File's Open Project… (acebc2089, [[BPR-1157]]) and Pull Request's Prepare review
tour and Add tour stop (791decea3). They are mirrored disabled too. The
window's disabled Comment names its reason in its own title ("Comment
unavailable — Select a current-head…", fa194e6bc), which is the window's state,
so the course compares the commands with that reason removed from both sides.

| Check | Result |
|---|---|
| `qt_profile_management_window` | Pass; with the Help menu not added to the bar it fails, as a control |
| `application_profile_empty_desktop_native_menu_bar` | Pass, 7.3 s, with the screen locked (the course's QPA key-window path). The acceptance names the owner's unlocked Aqua session: owed |
| Review-cycle, profile management and menu rows | 48 of 48 pass |

Which commands a windowless Help should offer was left as a UX choice on this
card. The disabled mirror is the candidate; the owner's Accept or Reject is
attended item `BPR-1250-help-v001` in session
`cbd48b91000b00f1e72e1ad62c7c384c` (no server started).

Published: `git pull --rebase` renamed the candidate. `5db77be71` above is `9605cb7da` on main; the attended item names `9605cb7da`.
