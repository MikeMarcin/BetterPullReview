---
schema: "implement-rfc.card.v1"
id: "BPR-1250"
title: "Closing the last window takes Help off the macOS menu bar"
status: "Done"
rfcs: []
depends_on: []
platforms: ["mac"]
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1250--BPR-1250--34ad467c0959.json","related_cards":["BPR-1035","BPR-1121","BPR-1157"]}
acceptance: [{"criterion":"application_profile_empty_desktop_native_menu_bar passes in the owner's Aqua session","outcome":"pass","revision":"4c8bcda0b","evidence":{"kind":"command","detail":"ctest -V -R ^application_profile_empty_desktop_native_menu_bar$ in /Users/jaeger/bpr1250-review-v003.pDiQ9g/source/build/cand (4c8bcda0b), owner's logged-in unlocked Aqua session on macOS arm64: Passed 4.31 s; 'AppKit activated this process and made the window key', 'key windows were native throughout'. A first non-verbose run passed in 7.00 s."}},{"criterion":"Owner judgment of which commands windowless Help offers","outcome":"pass","revision":"4c8bcda0b","evidence":{"kind":"observation","detail":"Owner Accept of BPR-1250-help-v003 on the frozen build of 4c8bcda0b (sha256 bf212edf...4b82a5), session f18e0aba4e53e518cd4f57a4dbcfc475, event 8, decision a2b6a4dd-6865-4c03-bc4b-c87b3f5c2012, fingerprint 51ea8a00...725895, note: 'It works, I'm not sure why but dif doesn't look great to me once it's actually there in the menu bar.' Windowless Help lists Diagnostics..., Export anonymized diagnostics..., Export with identifying details..., disabled without a window. The glyph's look is follow-up BPR-1271. Released by name at event 11."}},{"criterion":"The menu bar status item stays from launch until Quit, through the last window's close","outcome":"pass","revision":"4c8bcda0b","evidence":{"kind":"observation","detail":"Automated part passes (the Aqua course). The owner's launch command for v003 run unchanged: after the window's close button the process stays with 0 windows, menu bar 2 'status menu' 24 x 24, and Apple, File, Profiles, Pull Request, View, Help; Quit exits 0. Owner Accept of BPR-1250-help-v003, event 8 ('It works')."}},{"criterion":"The status item is a monochrome Dif template image macOS tints","outcome":"pass","revision":"a4723f817","evidence":{"kind":"observation","detail":"Owner accepted BPR-1250-menubar-dif-v001 (fingerprint cb3a79372079, decision 00e0e86f-6c5e-4453-a29a-7d77cc178ae3, 2026-10-03T18:50:24Z, session cbd48b91000b00f1e72e1ad62c7c384c). Automated part: qt_menu_bar_status_item(_cocoa) and dif_status_item_template_contract pass at a4723f817. The owner's later note on the look (help-v003) is follow-up BPR-1271, not a rejection of this criterion."}}]
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

## Windows (bpr-main-win, 2026-10-02)

At `93ade9102`, which contains `a4723f817`. Windows 11 Home 10.0.26200, MSVC
14.51, Qt 6.11.2, `build/windows-dev` Debug. The 19 executables the selection
runs were deleted first, and the build rebuilt every one. It printed no
`warning C` or `warning LNK` line.

```powershell
ctest --preset windows-dev -j4 --output-on-failure -R "^qt_system_tray_notification_sink$|^qt_menu_bar_status_item|^dif_status_item_template_contract$|notification|^application_profile_|^application_review_round_trip_composition_contract$|^architecture_boundaries$|^test_network_isolation_manifest$"
```

22 of 23 pass, in 306.49 s:

- `qt_system_tray_notification_sink` (Windows keeps one tray per profile);
- `dif_status_item_template_contract`;
- `application_native_notification_attended_contract`,
  `application_review_queue_notification` and
  `application_notification_delivery`;
- every `application_profile_*` row but one: activation, startup and its
  admission, owner admission, the window layout and its drain, restart,
  multi-window, second-window close, two-account isolation, management and
  its host, registry and storage authority;
- `application_review_round_trip_composition_contract`;
- `architecture_boundaries`;
- `test_network_isolation_manifest`.

The `qt_menu_bar_status_item*` rows are not registered in the Windows graph.

The one failure is the parked
`application_profile_window_review_restoration_public_path` (242.10 s):
`unavailable-anchor failed (1)`, `FAILED: failed reading restoration preserves
exact recorded identities and sibling`. That is [[BPR-1236]], unchanged. Nothing
else needed mapping.

## Owner hand-back at event 899 (2026-10-03)

Session `cbd48b91000b00f1e72e1ad62c7c384c`:

- `BPR-1250-menubar-dif-v001`: **Accept** (18:50:24 UTC). The template image
  criterion passes.
- `BPR-1250-help-v002`: **Reject**, "It closes instantly and nothing is in
  the dock or menu bar so I can't tell", with a screenshot of the launcher
  printing "BPR-1250 v002 candidate closed." straight away.

Both items, and the accepted `BPR-1241-retarget-key-v002`, are released by
candidate name.

### Why v002 closed instantly: the frozen build was not self-contained

Reproduced by running the owner's command unchanged: it exits in 0.4 s, and
`owner-app.log` in the root holds seven earlier launches, each printing
`STARTUP_FAILED`, `startup_failure=unavailable` and
`STARTUP_FAILURE_SOURCE:profile-runtime-construction: [omitted:unclassified-exception]`.
There was no crash report and no other BetterPullReview process, and the
root's own home held no profile, so no single-instance handoff happened.
Under lldb, the exception that escapes is
`read_all` in `deterministic_git_fixture` (`github_public_path_fixture.cpp`),
reading the `nested-folders-v1` pack. A tests-on build compiles
`BPR_GITHUB_PULL_REQUEST_FIXTURE_ROOT` and the other fixture roots as
absolute paths into its source tree (`src/app/CMakeLists.txt`), and the
agent supervisor and MCP helper paths into its build tree. The v002 binary
names `/Users/jaeger/bpr1250-wt`, the clean worktree it was built in. That
worktree was removed after the build was frozen and checked, so every later
launch failed before the first window. (The earlier `__cxa_throw` inside
`SecKeychainCopyDefault`, because the redirected home has no keychain, is
caught inside Security and is harmless; BPR-1241's frozen build takes the
same path and works.)

This is not a product defect in a4723f817 or in the status item. The
procedure is fixed: `AGENTS.md` now requires a frozen attended build to keep
its source and build tree inside its own root, and its launcher to check
the fixture tree first and to report an exit within seconds rather than
close silently.

A related product gap, not caused here: a startup refusal prints to stdout
and quits, so a person who starts the app from Finder sees nothing at all.
That is a separate UX question and is not changed on this card.

### Candidate v003: main 4c8bcda0b, self-contained

`/Users/jaeger/bpr1250-review-v003.pDiQ9g`: `source/` is `git archive` of
4c8bcda0b, built there (`cmake --preset dev -B source/build/cand`, shared
dependency cache only), executable sha256
`bf212edf12993adce408562317c5f3de229cb58dff97dc295e09b3864b4b82a5`. Every
compiled absolute path names the root, apart from the Qt runtime in the
main checkout's shared `build/.cpm-cache/bprqt`, as for BPR-1241's working
build. Launch:
`/opt/homebrew/bin/python3 /Users/jaeger/bpr1250-review-v003.pDiQ9g/launch.py`.

| Check | Result |
|---|---|
| The owner's command, run unchanged, observed through System Events | Window opens with Apple, File, Profiles, Pull Request, View and Help, and menu bar 2. After its close button: process running, 0 windows, the same six menus, menu bar 2's "status menu" 24 x 24; windowless Help lists Diagnostics…, Export anonymized diagnostics…, Export with identifying details…, all disabled. Quit from the app menu: exit 0, "candidate closed" |
| Control: launcher with the fixture tree pointed at the removed v002 worktree | Refuses: "cannot start: … is missing", nothing launched |
| Control: launcher started on the v002 executable | Reports "exited after 0.4 s with code 1 … tell the agent" and the `STARTUP_FAILED` lines |
| `application_profile_empty_desktop_native_menu_bar`, at 4c8bcda0b in the v003 tree, in the unlocked Aqua session | Pass, 4.31 s: "AppKit activated this process and made the window key", "key windows were native throughout" (and 7.00 s in a first run) |

The Aqua course criterion passes. The owner's confirmation that the app stays
running with Dif and Help after the window closes is attended item
`BPR-1250-help-v003` in the same session, served on port 59998.

### Moved to a new short session (2026-10-03)

The owner asked for the next review to hold only the recent questions, so
`BPR-1250-help-v003` moved, unchanged, from `cbd48b91000b00f1e72e1ad62c7c384c`
to session `f18e0aba4e53e518cd4f57a4dbcfc475`:

- The new session's state is
  `/Users/jaeger/.mask/attended-review/owner-review-20261003-recent-v001`,
  and every command on it takes `--state` with that path. `serve` printed
  port 55410.
- Title, revision, criterion, content, launch command, paths and resource
  version are the same, and so is the fingerprint, `51ea8a00…725895`. The
  item was ready at sequence 6.
- Its frozen root `/Users/jaeger/bpr1250-review-v003.pDiQ9g` is untouched,
  and the shared install claim is now held by the new session only.
- In the old session the item was released by name at event 923, undecided.
  That session's host on port 59998 is stopped.

The launch command is unchanged:
`/opt/homebrew/bin/python3 /Users/jaeger/bpr1250-review-v003.pDiQ9g/launch.py`.

## Owner verdict at hand-back 9: help-v003 accepted (2026-10-04)

Read with `status --after 6` in session `f18e0aba4e53e518cd4f57a4dbcfc475`.
`BPR-1250-help-v003` was **accepted** at event 8, decision
`a2b6a4dd-6865-4c03-bc4b-c87b3f5c2012`, fingerprint `51ea8a00…725895`, on the
frozen build of `4c8bcda0b` (executable sha256 `bf212edf…4b82a5`). The
owner handed back at 9. The note, verbatim:

> It works, I'm not sure why but dif doesn't look great to me once it's actually there in the menu bar.

The decision carries a screenshot of the owner's dark menu bar with Dif at
the left of the status items. It stays in the session's event log and is
not committed, because it shows other applications' items.

"It works" answers both pending criteria: after the window closes the app
stays running with Dif in the menu bar until Quit, and windowless Help
offers Diagnostics…, Export anonymized diagnostics… and Export with
identifying details…, disabled without a window. The second sentence is
about the glyph's look. The template-image criterion passed at
`BPR-1250-menubar-dif-v001` and is not reopened; the look is the Backlog
follow-up [[BPR-1271]], which quotes the note and points at
`scripts/dif_status_item_template.py` and the committed pair in
`src/adapters/qt/assets/status-item/`.

| Criterion | Outcome | Evidence |
|---|---|---|
| `application_profile_empty_desktop_native_menu_bar` passes in the owner's Aqua session | Pass | 4.31 s at `4c8bcda0b` in the v003 tree (above) |
| Owner judgment of which commands windowless Help offers | Pass | Owner Accept, event 8 |
| The status item stays from launch until Quit, through the last window's close | Pass | Owner Accept, event 8, with the System Events observation above |
| The status item is a monochrome Dif template image macOS tints | Pass | Owner Accept of `BPR-1250-menubar-dif-v001`, at `a4723f817` |

The item was released by candidate name at event 11; the consumed cursor is
11. Linux and Windows ran their rows at `93ade9102` and on bpr-main-win
(above). The card is Done.
