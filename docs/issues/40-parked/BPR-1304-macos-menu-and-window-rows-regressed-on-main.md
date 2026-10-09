---
schema: "implement-rfc.card.v1"
id: "BPR-1304"
title: "macOS menu and window rows regressed on main"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac"]
parked_on: "human-review"
metadata: {"found_by":"macOS Debug serial run of origin/main at ebb2cf11e, 2026-10-07","reservation":"docs/issues/_reservations/BPR-1304--BPR-1304--c2a0556d07b5.json","related_cards":["BPR-1295","BPR-1293","BPR-1089","BPR-0796","BPR-1303"],"acceptance":["BPR-1304:desktop-view","BPR-1304:title-row","BPR-1304:native-key-locked","BPR-1304:native-courses"]}
---

# BPR-1304: macOS menu and window rows regressed on main

## Observation

A serial macOS Debug run of `origin/main` at `ebb2cf11e` failed five rows that
[[BPR-1295]] and [[BPR-1293]] had left passing in their focused sets:

- `qt_profile_management_window`: "with the window menu's own commands, in the
  window menu's own order".
- `application_profile_management_public_path`: "and carries the window menu's
  View, offered and disabled, so the close takes no menu off the bar".
- `application_profile_empty_desktop_native_menu_bar`: "View and its commands
  are still offered with no window open" (cocoa).
- `qt_hide_drawers_native_menu_key`: "Command-B is sent through AppKit" (cocoa).
- `application_uiforetw_selected_diff_contract`: "compact navigation omitted
  title_row_exact_order".

`application_label_authoring_contract` failed in the same run; it is
[[BPR-1303]]'s and not this card's.

## Root causes and first bad commits

Each boundary was built and run in a scratch worktree, the parent passing and
the named commit failing.

1. **`9446afe42` ([[BPR-1295]]), the tests were wrong.** It added Hide Drawers
   to the window's View and to the windowless desktop bar alike, but the Qt row
   pinned the desktop View at seven items and the public path counted seven.
   At `b078d8bdf` both pass; at `9446afe42` both fail with the messages above.
2. **`9446afe42` ([[BPR-1295]]), the capture was wrong.** The title row gained
   the Hide Drawers button after the Comments toggle, but the selected-diff
   capture in `src/app/src/github_application.cpp` still required sixteen
   title-row actions, so `title_row_exact_order` came out false. At
   `b078d8bdf` the row passes; at `9446afe42` it fails. The owner's
   `29a55ef70` ("Align selected-diff capture with the accepted Hide Drawers
   control"), committed on local `main` for [[BPR-1305]] and not pushed, is
   the fix. It also drifts the round-trip full-application pin, which
   BPR-1305 was resealing, uncommitted, when this card was pushed; so this
   card verifies `29a55ef70` and neither carries nor changes it.
3. **`7778886c9` ([[BPR-1293]]), the product was wrong.** It added
   Conversations (Ctrl+J) to the window's View menu and not to the desktop
   bar that mirrors it ([[BPR-0796]]), so closing the last window took
   Conversations off the macOS menu bar. The native course compares the two
   bars and failed there. With this card's Qt row applied, `5d5de1c02` (its
   parent) passes the window comparison, and `7778886c9` fails it.
4. **Not a commit: the native key course could not send in a locked
   session.** The failing run's log says "the session refused activation
   (screen locked: yes, frontmost: loginwindow)". The course hands Qt the
   focus window through the QPA entry point in that case, as it says it does,
   but it then sent Command-B only to `[NSApp keyWindow]`, which is nil in a
   session that refused activation. It had passed for [[BPR-1295]] in an
   unlocked session. [[BPR-1089]] played no part in any of the five.

## Change

- `src/adapters/qt/src/desktop_menu_bar.cpp`: the desktop bar's View carries
  Conversations, disabled, on Ctrl+J, after Hide Drawers as in the window.
- `tests/adapters_qt/profile_management_window_tests.cpp`: the desktop View is
  compared item by item (text, separator, checkable) with a real
  `workbench_window`'s View, so a window item added later without a desktop
  counterpart fails here, offscreen. The explicit pins now name Hide Drawers
  on Ctrl+B and Conversations on Ctrl+J, and the later key and trigger checks
  use the shifted indices.
- `tests/end_to_end/profile_management_public_path_tests.cpp`: the resident
  View is compared with the closed window's View, read before the close,
  instead of a count.
- `tests/end_to_end/hide_drawers_native_menu_key_tests.cpp`: when activation
  was refused and there is no key window, Command-B is sent through
  `-[NSApplication sendEvent:]` with no window, which AppKit resolves against
  the main menu, the path [[BPR-0796]]'s course already uses for Command-N. An
  active session takes exactly the path it did before.

## Acceptance

- **BPR-1304:desktop-view.** The desktop bar's View matches the window's,
  offered and disabled; both offscreen rows pass, and fail with the product
  change removed.
- **BPR-1304:title-row.** `application_uiforetw_selected_diff_contract` passes
  with `29a55ef70` ([[BPR-1305]]'s to publish).
- **BPR-1304:native-key-locked.** `qt_hide_drawers_native_menu_key` sends its
  key in a session that refused activation.
- **BPR-1304:native-courses.** The two cocoa courses pass in the owner's Aqua
  session.

## Verification — 2026-10-07 (macOS arm64, Debug)

A clean worktree build of the candidate, run serially (`-j1`):
`qt_profile_management_window`, `application_profile_management_public_path`,
`application_uiforetw_selected_diff_contract`, `qt_workbench_window_scale_{100,125,200}`,
`application_attended_recent_replay`, `application_release_maintenance_public_path`,
`application_profile_multi_window_public_path`,
`application_setup_destination_public_path` and `architecture_boundaries`:
11 of 11 pass. Both cocoa course executables build.

**Failing-before control.** With `desktop_menu_bar.cpp` reverted and the new
tests kept, `qt_profile_management_window` fails "with the window menu's own
commands, in the window menu's own order" and
`application_profile_management_public_path` fails "and carries the window
menu's View, offered and disabled, so the close takes no menu off the bar".
Both pass restored.

**Not run here.** The two cocoa courses
(`application_profile_empty_desktop_native_menu_bar`,
`qt_hide_drawers_native_menu_key`) put windows on the live desktop, which
implementers must not do (AGENTS.md). They are owed to the owner's session:

```sh
ctest --preset dev -j1 --output-on-failure -R '^(application_profile_empty_desktop_native_menu_bar|qt_hide_drawers_native_menu_key)$'
```

The empty-desktop course's failing assertion is the same View comparison the
offscreen rows now pin. The locked-session path of the key course has not
been observed to pass.

## Owner session prepared, 2026-10-09 (main, Mac)

`BPR-1304:native-courses` and `BPR-1304:native-key-locked` are owner runs in his Aqua session, so they are a wizard, not an implementer run.

- Frozen root `/Users/jaeger/.mask/attended-review/bpr-owner-20261009-mac._suhca5k`: `cf476d62b` (main) exported and built there (optimized, tests on) by `scripts/freeze_attended_build.py`, executable SHA-256 `cdaab5033db8463f9544e8cde6d61d1277212723b9b59f0eb981b52daf805db6`. Its own `--check` printed `CHECK_OK (first head): ATTENDED_PULL_REQUEST_OPENED` on `overview-labels-v1`. Launch: `python3 /Users/jaeger/.mask/attended-review/bpr-owner-20261009-mac._suhca5k/launch.py`. Everything goes to the fixture server; nothing reaches GitHub. The freeze also built the two course executables (`--target test_profile_empty_desktop_native_menu_bar --target test_hide_drawers_native_menu_key`). ctest needs to write its own `Testing/` scratch, so only `src/build/app-release-tests/Testing` was made writable after the freeze. The sources and built products stay read-only. `ctest -N` lists both rows.
- Wizard `bpr-mac-frozen-20261009-v001` (session `2de8e578-5311-43e6-8fd5-9d168bc0f156`, 12 steps), state `/Users/jaeger/.mask/wizard/bpr-mac-frozen-20261009-v001`, source root this checkout; its loopback host runs detached. The page's URL carries a token and is handed to the owner directly, not committed. Steps 1 and 2 are this card's: the two courses with the Mac unlocked, then `qt_hide_drawers_native_menu_key` started 20 seconds after the owner locks the screen. Output goes to `/Users/jaeger/.mask/wizard/bpr-mac-frozen-20261009-logs/bpr1304-{unlocked,locked}.log`. The last step is the agent's reading of both logs for `profile-empty-desktop-native-menu-bar:pass` and `hide-drawers-native-menu:pass`.

Both criteria stay pending until the logs are read here.
