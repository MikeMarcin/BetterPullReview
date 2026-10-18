---
schema: "implement-rfc.card.v1"
id: "BPR-1586"
title: "Show a thread selected in Review in Threads, beside Show in Discuss"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner note on accepting BPR-1581 v001, 2026-10-17","owner_note":"We should also be able to go from rview to show in threads because why not","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-1293","BPR-1356","BPR-1567","BPR-1581","BPR-1582"],"reservation":"docs/issues/_reservations/BPR-1586--BPR-1586--61e6e8a52fa6.json","builds_on":"BPR-1581 branch bpr-1581-thread-navigation; lands with it in one integration"}
acceptance: [{"criterion":"BPR-1586:core","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"core_workbench_interaction: test_a_thread_shows_in_threads (a thread on a file, placed or outdated, routes to Threads; my draft has its row; a comment not on a file is unavailable)."}},{"criterion":"BPR-1586:window","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"qt_thread_moves_scale_{100,125,200}: the thread menu, a reply's own menu, the code's line menu, my draft's note and Option-Command-4 open Threads with the row as the cursor's at the named post and the keyboard on the list; a search hiding the row is cleared and said; qt_workbench_conversation_reveal: the draft menu offers Show in Threads."}},{"criterion":"BPR-1586:negative","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"Negative controls in qt_thread_moves: a pull request comment has no row and nothing moves; an unknown thread moves nothing; Show Thread in Threads is not offered in Discuss and steps aside while a reply is typed."}},{"criterion":"BPR-1586:catalogue","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"qt_keyboard_shortcuts_scale_100: Ctrl+Alt+4 in the catalogue with its live bindings and no other; qt_profile_management_window: the resident View menu mirrors Show Thread in Threads on Ctrl+Alt+4."}},{"criterion":"BPR-1586:gates","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"clang-format 22 clean; clang-tidy 22 reports nothing in the touched sources; DOCUMENTATION_OK; Full scripts/run_test_suite.py --preset dev on the merged tree 30f016602 (main 1afb7a6f9 merged): 767 of 777 passed, 1 skipped (application_large_review_cache_bytes, exit 77), 10 failed with the machine at load average 33-39 (13 agents building). Rerun alone: composed_application_hosts, application_private_review_native and application_private_review_retry pass; seven still miss time budgets or condition timeouts under that load, none in a path these cards change: application_large_review_{responsiveness,launch,active_work,pathological,history} (measured platform budgets, e.g. MainToFirstSourceRender 21 s), application_epoch_portable_occurrence_two_catalogues (composed read budget) and application_private_review_file (publish condition_timeout); BPR-1598 owns load-robust large-review rows. The earlier full run on 45cc87c97 passed 766 of 775 with only environment and load failures, each passing alone or after its test update. Final merge 74de8b4dd (main 1e72d4d60): the focused landing set passes. Landed without a new review, as the coordinator directed for a menu addition matching the accepted BPR-1581 pattern."}}]
---

# BPR-1586: Show a thread selected in Review in Threads

The owner accepted [[BPR-1581]] v001 on 2026-10-17 with the note "We should
also be able to go from rview to show in threads because why not". The
coordinator scoped it as a small follow-up that matches the accepted pattern
and lands without a new review once the tests prove it.

## Scope

- **Show in Threads** next to Show in Discuss wherever Show in Discuss appears
  in Review: the thread menu of an open card or closed note, a comment's own
  menu, the code's menu on a line with a thread, and my draft's note.
- **⌥⌘4** (`Ctrl+Alt+4`, next to ⌘4 = Threads), *Show the selected thread in
  Threads*: a View menu item, mirrored on the resident menu bar, listed in Help
  → Keyboard Shortcuts, stepping aside while typing, like ⌥⌘3 and ⌥⌘2.
- Threads opens with that thread's row (at the named post) as the cursor's,
  selected and scrolled into view, with the keyboard on the list. The core's
  shared move decision (`route_workbench_thread`, destination Threads) decides
  it; a comment that is not on a file has no row and nothing moves. My draft
  has its own row.

## Acceptance

- `BPR-1586:core`: `core_workbench_interaction` covers the route to Threads
  from Review: on a file, a draft and a comment not on a file.
- `BPR-1586:window`: `qt_thread_moves_scale_*` drive each entry point and
  ⌥⌘4 into Threads on that row and post, focused; a negative control (an
  unknown thread, and the key while typing) moves nothing.
- `BPR-1586:catalogue`: `qt_keyboard_shortcuts_scale_*` find the key bound
  once.
- `BPR-1586:gates`: format, tidy, documentation gate, and the full `dev`
  ctest shared with the [[BPR-1581]] landing.

## Closeout, 2026-10-17

Built on [[BPR-1581]]'s branch and landed with it on main as `9e27c40c5`, as
the coordinator directed: a menu addition matching the accepted Show in
Discuss pattern, landing without a new review once its tests passed.

- The core route gains the Threads destination (`core_workbench_interaction`:
  a thread on a file, placed or outdated, and my draft route to Threads; a
  comment not on a file has no row).
- **Show in Threads** sits beside Show in Discuss in the thread menu, a
  comment's own menu, the code's line menu and my draft's note; **Show Thread
  in Threads** is `Ctrl+Alt+4` (⌥⌘4), in the catalogue, the View menu and
  the resident menu bar, stepping aside while typing.
- `conversation_finder::reveal` puts the cursor on the row at the named
  post, with the keyboard on the list, and clears a search, filter or folded
  file that hides it, which the status bar says.

Verification at `74de8b4dd`: `qt_thread_moves_scale_{100,125,200}` drive
every entry point and the key, with negative controls (a pull request comment
has no row, an unknown thread moves nothing, the key is not offered in
Discuss or while typing); `qt_workbench_conversation_reveal`,
`qt_keyboard_shortcuts_scale_100`, `qt_profile_management_window` and
`core_workbench_interaction` pass. The full `dev` suite is shared with
[[BPR-1581]] (see its closeout).
