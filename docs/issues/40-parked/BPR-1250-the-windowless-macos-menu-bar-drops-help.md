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
acceptance: [{"criterion":"application_profile_empty_desktop_native_menu_bar passes in the owner's Aqua session","outcome":"pending","revision":"a4723f817","evidence":{"kind":"command","detail":"Passes on macOS arm64 at a4723f817 in a clean worktree build with the screen locked (QPA key-window path), 7.15 s, now with the status item assertions; the unlocked Aqua run in the owner's session is owed."}},{"criterion":"Owner judgment of which commands windowless Help offers","outcome":"pending","revision":"a4723f817","evidence":{"kind":"freeform","detail":"help-v001 (9605cb7da) rejected at event 868 with 'It does but the to right menu bar icon is gone': the Help part was as described, the rejection is the missing status item. Asked again with the item restored as BPR-1250-help-v002, session cbd48b91000b00f1e72e1ad62c7c384c."}},{"criterion":"The menu bar status item stays from launch until Quit, through the last window's close","outcome":"pending","revision":"a4723f817","evidence":{"kind":"command","detail":"Automated part passes: application_profile_empty_desktop_native_menu_bar asserts the desktop's item is visible from startup, is the profile's notification tray and AppKit's one NSStatusBarWindow, and stays through every close and reopen; controls fail at the item and at the close. Owner's confirmation is BPR-1250-help-v002."}},{"criterion":"The status item is a monochrome Dif template image macOS tints","outcome":"pending","revision":"a4723f817","evidence":{"kind":"command","detail":"Automated part passes: qt_menu_bar_status_item(_cocoa) (mask, 18 px 1x and 36 px 2x, black with alpha; three controls fail) and dif_status_item_template_contract (byte-exact from the accepted neutral master). Owner's visual judgment is BPR-1250-menubar-dif-v001."}}]
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

## The owner's rejection of help-v001: the menu bar icon (2026-10-02)

Event 868, session `cbd48b91000b00f1e72e1ad62c7c384c`: "It does but the to
right menu bar icon is gone. Also the menubar icon needs to be a
monochrome/greyscale Dif avatar so it can be colored by macos -- I could have
sworn we did that already." `help-v001` is released by name.

### Why the icon went

The menu bar icon was the review-queue notification sink's own
`QSystemTrayIcon`, created in `system_tray_notification_sink`'s default
constructor and shown at once. The sink belongs to the profile runtime
([[BPR-0705]], 826fb871e, 2026-09-08). Until 48aa547fc (2026-09-15,
RFC-BPR-0033 B1, [[BPR-0777]]) closing the last window presented the chooser;
since then the process stays resident with no runtime, so the icon leaves with
the profile. It was never the application's. Observed on the frozen BPR-1213
build (0f6e7b292): System Events reports menu bar 2 ("status menu", 24 x 24)
with the window open and only menu bar 1 after the close, the process still
running with Apple, File, Profiles, Pull Request, View and Help.

### Why there was no template image

The owner directed it on 2026-09-13: mandate revision 5 of the Dif pack
(f4fe865ac), folded into BPR-0767 with its native check in BPR-0766. The
amendment and a docs-only draft (8ee34d1f4) exist only on
`claude/bpr-dif-integration-v1-d932e2`, whose session stopped before the glyph
or code was made. The integrated BPR-0766, BPR-0767 and BPR-0769 never carried
it, and RFC-BPR-0031 on main did not record it. Nothing was lost from main;
it was never built.

### Candidate a4723f817

- `profile_desktop` owns one status item for the life of the process on macOS
  (`native_menu_bar_status_item`, the one place the platform mapping is) and
  lends it to each runtime as `github_application_config::StatusItem`; the
  runtime composes its sink over it. A click on the shared item reaches only
  the sink that submitted last. Linux and Windows keep a tray per profile.
- `qt::menu_bar_status_icon`: Dif's head from the accepted neutral master,
  black with alpha, 18 points at 1x and 2x, `setIsMask(true)`. The pair is in
  `src/adapters/qt/assets/status-item/`, made by
  `scripts/dif_status_item_template.py` (standard library, reproducible) and
  compiled into macOS graphs only.
- RFC-BPR-0031 records the restored owner amendment; the round-trip pins are
  resealed with the review.

| Check, at a4723f817 in a clean worktree build | Result |
|---|---|
| `application_profile_empty_desktop_native_menu_bar` | Pass, 7.15 s, screen locked (QPA key-window path). New: the item is visible from startup, is the profile's tray, is AppKit's one `NSStatusBarWindow`, stays through every close and reopen |
| Control: the profile's sink keeps its own tray | Fails "the profile's notifications go through the desktop's item, not a tray of their own" |
| Control: the empty desktop hides the item | Fails "closing the last window leaves Dif's status item in the menu bar" |
| `qt_menu_bar_status_item`, `qt_menu_bar_status_item_cocoa` | Pass: mask; 18 px at 1x, 36 px at 2x; black with alpha. Control without `setIsMask` fails; a coloured and an empty image are refused |
| `qt_system_tray_notification_sink` | Pass, with two sinks sharing one item: the click reaches the latest submission only. Control without the routing fails "and no other sink sharing the item" |
| `dif_status_item_template_contract` | Pass: both images byte-exact from the master; four controls |
| `application_review_round_trip_composition_contract` | Pass after the reseal |
| Notification and profile rows (20) | 19 pass. `application_profile_window_review_restoration_public_path` fails at "failed reading restoration preserves exact recorded identities and sibling", offscreen where the change composes nothing: the known [[BPR-1236]] |
| Frozen app, sha256 `8d9e09a2…` | System Events: menu bar 2, "status menu", 24 x 24, at launch |

### Owner review

Two items in session `cbd48b91000b00f1e72e1ad62c7c384c`, one question each,
on the frozen build in `/Users/jaeger/bpr1250-review-v002.bthToZ` (launch:
`/opt/homebrew/bin/python3 /Users/jaeger/bpr1250-review-v002.bthToZ/launch.py`):

- `BPR-1250-help-v002`: with no window, Dif stays in the menu bar beside the
  windowless menus.
- `BPR-1250-menubar-dif-v001`: the icon is Dif's head in the menu bar's own
  colour.

The unlocked Aqua run of the native course is still owed.

## Linux: bpr-main-linux, 2026-10-02

At `93ade9102` (includes `a4723f817`), on WSL2 with GCC 15.2 and `build/dev` Debug,
offscreen. `cmake --build --preset dev` exits 0 with 0 `warning:` lines. The
requested `ctest --preset dev -j4` selection runs 22 rows and passes **21 of
22** in 87.26 s. The pattern `^qt_menu_bar_status_item` matches no Linux row;
those rows are macOS-only.

- **Passes:** `qt_system_tray_notification_sink` (Linux keeps one tray per
  profile), `dif_status_item_template_contract`,
  `application_native_notification_attended_contract`,
  `application_notification_delivery`,
  `application_review_queue_notification`, every other `application_profile_*`
  row (`multi_window` 10.70 s, `two_account_isolation` 13.05 s,
  `window_layout` 24.44 s, `window_restart`, `activation`, `startup`,
  `management`, `owner_admission` and the profile unit rows), the
  round-trip contract and `architecture_boundaries`.
- **The one failure** is [[BPR-1236]]'s
  `application_profile_window_review_restoration_public_path`, at its known
  check after `unavailable-anchor failed (1)`: `failed reading restoration
  preserves exact recorded identities and sibling` (68.80 s). This change did
  not cause it.

No remote mutation.
