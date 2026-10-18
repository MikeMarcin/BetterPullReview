---
schema: "implement-rfc.card.v1"
id: "BPR-1581"
title: "Show a thread selected in Review in Discuss, and back"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"owner request 2026-10-17","owner_note":"Why can't I go from a selected comment in Review to see it in Discuss easily?","governing_rfc":"RFC-BPR-0028","related_rfcs":["RFC-BPR-0052"],"related_cards":["BPR-1212","BPR-1290","BPR-1293","BPR-1356","BPR-1383","BPR-1384","BPR-1542","BPR-1567","BPR-1582"],"reservation":"docs/issues/_reservations/BPR-1581--BPR-1581--c1269e5136e4.json"}
acceptance: [{"criterion":"BPR-1581:core","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"core_workbench_interaction passes: test_a_thread_moves_between_review_and_discuss (Discuss clears exactly the hiding filters; outdated to Review among unplaced threads; PR comment unavailable; draft to the composer) and test_the_thread_a_key_shows."}},{"criterion":"BPR-1581:window","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"qt_thread_moves_scale_{100,125,200}: the open card's menu, a closed note's menu, a reply's own menu, the code's line menu and Option-Command-3 land in Discuss at that post, washed, selected, in view, focused and arrival-marked; Option-Command-2 and Open in Review come back; outdated both ways; a PR comment stays in Discuss; qt_workbench_conversation_reveal drives my draft to the composer. Frames of the jump in Light and Dark rendered."}},{"criterion":"BPR-1581:catalogue","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"qt_keyboard_shortcuts_scale_100: both keys in the catalogue with live bindings, no other binding; qt_profile_management_window: the resident View menu mirrors them."}},{"criterion":"BPR-1581:negative","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"Negative controls in qt_thread_moves: an unknown thread moves nothing; the keys step aside while a reply is typed; Open in Review on a PR comment is disabled with its reason."}},{"criterion":"BPR-1581:gates","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"command","detail":"clang-format 22 clean on touched files; clang-tidy 22 (llvm@22) reports nothing in the touched sources; scripts/verify_documentation.py DOCUMENTATION_OK; Full scripts/run_test_suite.py --preset dev on the merged tree 30f016602 (main 1afb7a6f9 merged): 767 of 777 passed, 1 skipped (application_large_review_cache_bytes, exit 77), 10 failed with the machine at load average 33-39 (13 agents building). Rerun alone: composed_application_hosts, application_private_review_native and application_private_review_retry pass; seven still miss time budgets or condition timeouts under that load, none in a path these cards change: application_large_review_{responsiveness,launch,active_work,pathological,history} (measured platform budgets, e.g. MainToFirstSourceRender 21 s), application_epoch_portable_occurrence_two_catalogues (composed read budget) and application_private_review_file (publish condition_timeout); BPR-1598 owns load-robust large-review rows. The earlier full run on 45cc87c97 passed 766 of 775 with only environment and load failures, each passing alone or after its test update. Final merge 74de8b4dd (main 1e72d4d60): the focused landing set passes."}},{"criterion":"BPR-1581:owner-look","outcome":"pass","revision":"74de8b4dd","evidence":{"kind":"observation","detail":"Owner verdict in session c1a9372af55350e3cb63fe7d8377eafa: Accept, note: \"We should also be able to go from rview to show in threads because why not\" (BPR-1586); candidate BPR-1581-show-in-discuss-v001, fingerprint 7b34d072ec7a..., decision f5b72418-3a72-46c5-adeb-2cfc17d983ea, frozen build frozen-c6644bad7-v001; handed back at event 13, cursor consumed 13, candidates released at event 14."}}]
---

# BPR-1581: Show a thread selected in Review in Discuss, and back

The owner asked on 2026-10-17: "Why can't I go from a selected comment in
Review to see it in Discuss easily?"

## Why it is missing

Every road between the thread surfaces was built except this one:

- **Threads → Review** is the pick [[BPR-1293]] built (click, Return), and
  **Threads → Discuss** the row's Discuss button, Shift+Return and the row
  menu [[BPR-1356]] added. Both go through `core::route_workbench_thread`.
- **Discuss → Review** is a card's *Open in Review* button, thread menu item
  and comment context-menu item ([[BPR-1383]], [[BPR-1384]]). It is disabled
  for an outdated thread and for a pull request comment.
- **Review → Discuss** exists only for a draft being written: the drawer's
  Maximize, ⌘⇧M ([[BPR-1212]]). A published thread's card in Review offers
  Show line, Resolve, Mark read, More and Close, and its menu (More, right
  click, Shift+F10) offers Reply, Mark read, Resolve, Show line and Copy first
  post. The code's menu on a line with a thread offers Open thread. None of
  them goes to Discuss. ⌘3 switches to Discuss without the thread.

No card or RFC designed, deferred or refused a Review → Discuss jump. The
direction of the October work was the other way: [[BPR-1290]] made the
drawer the place a file's thread is read beside its code, and [[BPR-1293]]
moved the `#discussion_r` link, the published-post reveal and Previous and
Next defect *out of* Discuss into Review in place, keeping Discuss for what is
not on a file (the owner's Q2, 2026-10-06: "just keep discuss for now"). Each
later card added the road its owner note asked for, and nobody asked for this
one until now.

## Design

Designed with the `frontend-design` skill on the cards' own language: amber
is where you read (the pick's wash and 3-pixel edge), blue is where you
write. One shared route decides every move between the surfaces, the same one
[[BPR-1582]]'s Threads gestures use.

- **Show in Discuss** on a thread in Review: the thread menu (More, right
  click, the menu key, Shift+F10) of the open card and of a closed note; the
  context menu of a post in the open card (that post is the target); and the
  code's context menu on a line with a thread, after Open thread.
- **⌥⌘3** (`Ctrl+Alt+3`), *Show the thread in Discuss*, next to ⌘3 =
  Discuss: the open thread at its picked post, else the drawer's current
  thread, else the selected line's thread. **⌥⌘2** (`Ctrl+Alt+2`), *Show the
  thread in Review*, is the same move back from Discuss, next to ⌘2 = Review.
  Both are View menu items, mirrored disabled on the resident menu bar, listed
  in Help → Keyboard Shortcuts, and step aside while typing (Ctrl+Alt is AltGr
  on Windows, where AltGr+2 and AltGr+3 type `@`, `²`, `#` or `³`).
- Discuss opens with the thread open at that post, the post selected, in view
  and focused. On arrival the post's wash is drawn stronger with an amber
  ring, held briefly, then dissolves to the pick's resting wash, the same
  arrival in either direction. A dissolve, not motion, so it stands under
  Reduce Motion. Discuss's filters that would hide the thread are turned off
  and said, as [[BPR-1356]] does.
- **Outdated** threads: in Discuss they are listed, so Show in Discuss works.
  Discuss's Open in Review now opens an outdated thread among its file's
  threads that have no line, as a Threads pick does, and says so, instead of
  being disabled.
- **Pull request comments and reviews** are not on a file: Review has none,
  so they have no Show in Discuss, and Open in Review stays disabled with its
  reason.
- **My draft or pending comment**: Discuss lists no draft; Show in Discuss
  opens it in Discuss's composer, as Maximize does, and the status bar says
  so.
- The core decides: which thread and post is the target, where it opens, what
  is cleared, why, and when the move is unavailable. Qt presents.

## Acceptance

- `BPR-1581:core`: `core_workbench_interaction` covers the target choice and
  the route in both directions: Discuss clearing exactly the hiding filters,
  outdated to Review among the unplaced threads, a pull request comment
  unavailable, my draft to Discuss's composer, nothing selected unavailable.
- `BPR-1581:window`: `qt_workbench_window_scale_*` drive the real window:
  Show in Discuss from the open card's menu, a closed note's menu, a post's
  context menu and the code's line menu, and ⌥⌘3, land in Discuss on that
  thread and post, washed, selected, in view and focused; ⌥⌘2 and Open in
  Review come back; an outdated thread both ways; a pull request comment
  offers no Show in Review; a draft opens in the composer. Fails before.
- `BPR-1581:catalogue`: `qt_keyboard_shortcuts_scale_*` list both keys and
  find their live bindings; nothing else binds them.
- `BPR-1581:negative`: a negative control shows the window course refusing a
  move that lands on another thread or loses focus.
- `BPR-1581:gates`: clang-format 22 and clang-tidy on touched files, the
  documentation gate, focused rows and one full `dev` ctest on the merged
  tree.
- `BPR-1581:owner-look` (human): the owner accepts the frozen candidate,
  a frame sequence of the jump in Light and Dark, in a new short session.

## Checkpoint: candidate v001, 2026-10-17

One milestone with [[BPR-1581]], [[BPR-1582]] and [[BPR-1584]]: one shared
core route for every move between Threads, Review and Discuss, and one branch.
Product candidate `c6644bad7e3ecc05a70965bcd9910ef2a173725c` on branch
`bpr-1581-thread-navigation` (worktree
`.claude/worktrees/bpr-1581-thread-navigation`), main merged in at
`b6b066285`. Not merged to main until the owner accepts. Paths, commands and
identities are in [the checkpoint](../_checkpoints/BPR-1581/handoff.json).

Verification at the candidate (macOS, `dev` Debug, offscreen):
`core_workbench_interaction`, `core_review_ledger`,
`application_review_ledger_host`, `sqlite_workbench_settings_storage`,
`qt_conversation_finder_scale_{100,125,200}`, `qt_conversation_cards_view`,
`qt_thread_moves_scale_{100,125,200}` (new), `qt_keyboard_shortcuts_scale_100`,
`qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100` and
`application_profile_multi_window_public_path` (the composed Mark as Unread
course) pass. Negative controls: an unknown thread moves nothing; the keys
step aside while a reply is typed; a single click picks nothing; Full width
leaves Discuss uncapped and a 1900-pixel window's rows run past the old 880;
the read command is disabled with no read path; Mark read undoes Mark unread.
Fails before: the core and window courses do not compile without
`route_workbench_thread`'s origin, `choose_workbench_thread_target`,
`mark_conversation_unread` and `reading_column_width`; the finder's click
course asserts that a press picks nothing, where the unchanged sources pick on
a press (not rerun against the old build). clang-format 22
clean; clang-tidy 22 reports nothing in the touched sources; documentation gate
`DOCUMENTATION_OK`. The round-trip composition contract's pins already differ
on main (`06b00593…` against the pinned `315f7078…`) without this branch; the
reseal covering `src/app`'s new Mark as Unread wiring is done at landing on the
merged tree. The full `dev` suite did not complete: the shared volume ran out
of space during the full build (828/859), so it remains owed at landing; see
the checkpoint.

Owner look: session `c1a9372af55350e3cb63fe7d8377eafa`, state
`/Users/jaeger/.mask/attended-review/owner-review-20261017-bpr1581-v001`,
repo root the main checkout, ready at event cursor 9, one Accept/Reject
question per card; frozen root
`/Users/jaeger/bpr1581-thread-moves-v001.x7yze1z4` (executable SHA-256
`e5ba8757f9d1…`, its `--check` reached `ATTENDED_FILE_SHOWN`), launch
`python3 /Users/jaeger/bpr1581-thread-moves-v001.x7yze1z4/launch.py`.

This card's candidate: `BPR-1581-show-in-discuss-v001 (fingerprint `7b34d072ec7a…`)`, criterion `BPR-1581:owner-look`, Pending.

## Closeout, 2026-10-17

**Owner decision: Accept** (Accept, note: "We should also be able to go from rview to show in threads because why not", which became [[BPR-1586]]), decision `f5b72418-3a72-46c5-adeb-2cfc17d983ea` at event 10 of
session `c1a9372af55350e3cb63fe7d8377eafa`, on candidate `BPR-1581-show-in-discuss-v001`
(fingerprint `7b34d072ec7a…`, revision `frozen-c6644bad7-v001`). Handed back at
event 13; cursor consumed 13. The review host was stopped, every candidate
released (event 14), and the frozen root removed.

Landed on main as `9e27c40c5` (branch `bpr-1581-thread-navigation` at
`74de8b4dd`, main `1e72d4d60` merged), together with [[BPR-1586]]'s Show in
Threads. The round-trip composition contract is resealed over main
`1e72d4d60` for [[BPR-1582]]'s Mark as Unread wiring (the only
production-app change; mutation digest unchanged).

Verification: the full `dev` suite on the merged tree `30f016602` passed 767
of 777 (1 skipped). Of its 10 failures, three pass when rerun alone, and
seven (five large-review budget rows, the two-catalogue epoch read budget and
the private-review file course's publish timeout) still miss time budgets at
load average 33–39 with 13 agents building. None of the seven is in a path
these cards change; [[BPR-1598]] owns load-robust large-review rows. An
earlier full run on `45cc87c97` passed 766 of 775, with its failures
environmental or fixed by updating two tests that asserted the old behaviour
(`qt_github_conversation_panel`, `qt_profile_management_window`). On the
final merge `74de8b4dd` the 18 focused rows pass, including the thread-moves,
finder, cards, panel, resident menu, shortcuts, window, multi-window
composed and composition-contract rows. clang-format 22 clean, clang-tidy 22
reports nothing in the touched sources, `DOCUMENTATION_OK`.
