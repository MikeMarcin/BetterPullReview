---
schema: "implement-rfc.card.v1"
id: "BPR-1356"
title: "Threads opens a thread in Discuss as well as in Review"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","origin":"Owner chat, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1356--BPR-1356--8c7bfcbb3a0c.json","related_rfcs":["RFC-BPR-0028","RFC-BPR-0052"],"related_cards":["BPR-1293","BPR-1341","BPR-1342","BPR-1328","BPR-1354","BPR-1355"],"implementation_note":"Designed with the frontend-design skill; owner look required"}
acceptance: [{"criterion":"BPR-1356:owner-decision","outcome":"pass","revision":"160a726c0","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"Owner chat 2026-10-09, relayed: 'Do it' to the proposed design, then the row menu (Open in Review bold default, Open in Discuss, keys shown, menu key and Shift+F10); recorded under Owner decision."}},{"criterion":"BPR-1356:core","outcome":"pass","revision":"160a726c0","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"core_workbench_interaction covers route_workbench_thread: Review with Comments, Discuss clearing exactly the hiding filters, my draft routed to Review with draft_opens_in_review. Pass on macOS (f77a95cb3), Linux (b0ef8d7b9) and Windows (7672d5a8b)."}},{"criterion":"BPR-1356:finder","outcome":"pass","revision":"160a726c0","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"qt_conversation_finder_scale_{100,125,200}: Discuss button on hover and cursor rows only (never header or draft, left of the people at every width), Shift+Return, row menu and its bold default, menu key, draft's menu. Pass on macOS, Linux and Windows."}},{"criterion":"BPR-1356:window","outcome":"pass","revision":"160a726c0","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"qt_workbench_window_scale_{100,125,200} through the real core: Shift+Return, the button and Open in Discuss land on the washed, selected, focused card in Discuss with Review's file unchanged; hiding filters cleared and said, others kept; menu by right click, menu key, Shift+F10; no window command holds the keys. Pass on macOS, Linux and Windows."}},{"criterion":"BPR-1356:fails-before","outcome":"pass","revision":"710e06796","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"qt_workbench_window_scale_100 built against the unchanged sources failed 'Shift+Return opens the thread in Discuss'; core_workbench_interaction does not compile without route_workbench_thread."}},{"criterion":"BPR-1356:docs","outcome":"pass","revision":"160a726c0","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"docs/ux/workbench-specification.md describes the Discuss button, Shift+Return and the row menu; verify_documentation.py printed DOCUMENTATION_OK and verify_documentation_tests.py ran 67 tests OK."}},{"criterion":"BPR-1356:owner-look","outcome":"pass","revision":"160a726c0","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1356-threads-opens-a-thread-in-discuss.md","detail":"Owner Accept, no note, 2026-10-09T13:00:47Z: session e157660d93be79fc09c9139915076c11, candidate BPR-1356-open-in-discuss-v001 (fingerprint 16045f81aeb3..., frozen-160a726c0-v001), decision d05ec0be-9dbe-4495-b49b-cd722240d3f1 at event 4, handed back at 5, released at 6."}}]
---

# BPR-1356: Threads opens a thread in Discuss as well as in Review

## Owner decision

Owner, chat, 2026-10-09, relayed by the coordinator: from the Threads
modality ([[BPR-1293]], renamed by [[BPR-1341]]), add a way to go directly to
Discuss on a thread, in addition to the existing pick that opens it in
Review. The coordinator proposed the design below and the owner approved it,
verbatim: "Do it".

The owner then added, relayed by the coordinator: each thread row also gets a
right-click context menu with both destinations, "Open in Review" (the
default, shown in bold) and "Open in Discuss", each showing its key (Return,
⇧Return), opened from the keyboard too (the menu key, Shift+F10).

## Scope

- A click or Return keeps opening the thread in Review, unchanged.
- A small **Discuss** button on each thread row, shown on the row under the
  pointer and on the cursor's row, never on a file's header or on my draft.
  It takes the end of the row's words, so the people never move. Its tooltip
  is anchored right below it ([[BPR-1083]]'s `anchored_tooltip`): "Open in
  Discuss (⇧↩)".
- Shift+Return (keypad Enter too) opens the cursor's thread, or the post
  under the cursor, in Discuss.
- A right click on a row, or the menu key or Shift+F10 on the cursor's row in
  the list, opens the row menu: **Open in Review**, the default and drawn
  bold, then **Open in Discuss**, each showing its key. The search keeps its
  own menu. On my draft, Open in Discuss is disabled.
- Discuss opens with the thread's card open at the picked post, washed amber
  as the Review pick is, selected, scrolled into view and focused. Review's
  file does not change.
- If Discuss's filters would hide the card (File scope on another file, Only
  new on a thread with nothing new, Hide resolved on a resolved thread, an
  Overview count's list that leaves it out), exactly those are turned off and
  Discuss's notice says so: "To show this thread, Discuss turned off …". The
  notice goes when the reviewer changes a filter or picks again.
- The routing is the core's: `core::route_workbench_thread` takes the
  destination the gesture asked for, whether the row is my draft and which
  filters would hide the thread, and returns the modality, whether Comments
  opens, the filters to clear and a reason. My draft is not in Discuss's list,
  so Discuss asked for it routes to Review with
  `draft_opens_in_review`, said in the status bar.
- Keys are bound once: the finder reads Return, Shift+Return, the menu key
  and Shift+F10 itself; the menu's shortcuts are bound to the menu
  (`Qt::WidgetShortcut`) and no window command holds those keys.

## Acceptance

- `BPR-1356:owner-decision`: the owner's approval and addition are recorded
  above.
- `BPR-1356:core`: `core_workbench_interaction` covers the route: Review with
  Comments, Discuss clearing exactly the hiding filters, the draft's reason.
- `BPR-1356:finder`: `qt_conversation_finder_scale_*` cover the button's
  placement (hover, cursor row, never a header or my draft, left of the
  people at every rendered width), Shift+Return, the row menu and its default,
  the menu key, and the draft's menu.
- `BPR-1356:window`: `qt_workbench_window_scale_*` drive the window through
  the real core: Shift+Return, the button and the menu's Open in Discuss land
  in Discuss on the card, washed, selected, in view and focused, Review's file
  unchanged; the hiding filters are cleared and said, a filter that does not
  hide it is kept; the menu by right click, the menu key and Shift+F10; Open
  in Review and Return open Review; no window command holds the keys.
- `BPR-1356:fails-before`: the window course's Shift+Return check fails on
  the unchanged sources, and the core course does not compile without the
  route.
- `BPR-1356:docs`: the workbench specification describes it and the
  documentation gate passes.
- `BPR-1356:owner-look`: the owner accepts the frozen build in a new short
  attended session.

## Verification (macOS, offscreen, 2026-10-09)

Designed with the `frontend-design` skill: a quiet text pill that appears
only with intent (pointer or keyboard cursor), matching the strip segment's
word, and a menu that follows Home's row menu ([[BPR-1342]]).

- **Fails before.** The window course's first part, built against the
  unchanged sources at `710e06796`, failed with `FAILED: Shift+Return opens
  the thread in Discuss` (`qt_workbench_window_scale_100`). The core course
  does not compile without `route_workbench_thread`.
- **Passes after**, on `f77a95cb3` (rebased onto `a0eb15f5e`), 20 rows:
  `core_workbench_interaction`, `qt_conversation_finder_scale_100/125/200`,
  `qt_workbench_window_scale_100/125/200`, `qt_composed_github_scale_100/125/200`,
  `qt_github_conversation_panel`, `qt_workbench_title_row_scale_100/125/200`,
  `architecture_boundaries`, `application_large_review_benchmark`,
  `application_review_round_trip_composition_contract` (pins unchanged: no
  `src/app` source moved), `application_overview_feedback_public_path`,
  `application_attended_recent_replay`,
  `application_uiforetw_selected_diff_contract`.
  `architecture_boundary_contract` passed on the previous base `bfec328be`.
- `application_workbench_composition_contract` fails
  (`WORKBENCH_PROJECTION_WIRE_FAILED`, `WORKBENCH_SELECTED_DIFF_REQUEST_FAILED`
  and six more) identically on the base `bfec328be` without this change, so
  it is not counted here.
- clang-format 22.1.8 clean on every touched file. clang-tidy 22 reports no
  diagnostic on a line this change touches; the six it reports in these files
  (`compact_below`, `menu_at`, `native_caret`, `all_enabled`,
  `selected_after_review`, a copied `command`) predate it.
- `python3 scripts/verify_documentation.py --root .` prints
  `DOCUMENTATION_OK`; `tests/scripts/verify_documentation_tests.py` runs 67
  tests OK.
- Renders (offscreen, 1.0x): the button on the hovered and cursor rows, the
  row menu, Discuss on the opened thread, and Discuss after three filters
  were turned off with the notice; the finder at four palettes and the
  narrowest width with the cursor row's button.

## Owner look on v001 (accepted, below)

One new short session, one question, Accept or Reject.

- Session `e157660d93be79fc09c9139915076c11`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1356-v001`,
  repo root the main checkout, candidate `BPR-1356-open-in-discuss-v001`
  (revision `frozen-160a726c0-v001`, fingerprint `16045f81aeb3…`), criterion
  `BPR-1356:owner-look`, ready at event cursor 3. The host runs detached; the
  offline page is `review.html` in the state directory. The three 1.0x
  renders above are inline.
- Frozen root `/Users/jaeger/bpr1356-open-in-discuss-v001.or24x9ef`, made by
  `scripts/freeze_attended_build.py freeze` from `160a726c0` (fixture
  `inline-drawer-v1`, file `src/ui/conversation_cards_view.cpp`), executable
  SHA-256 `ce141c46e798…`. Its own `--check` reached `ATTENDED_FILE_SHOWN` on
  head `89df5e59…`.
- Launch: `python3 /Users/jaeger/bpr1356-open-in-discuss-v001.or24x9ef/launch.py`.

## Owner verdict on v001

Accept, 2026-10-09T13:00:47Z, decision `d05ec0be-9dbe-4495-b49b-cd722240d3f1`
at session event 4, no note (candidate `BPR-1356-open-in-discuss-v001`,
fingerprint `16045f81aeb3…`, revision `frozen-160a726c0-v001`). The owner
handed session `e157660d93be79fc09c9139915076c11` back at event 5, read from
cursor 0.

This satisfies `BPR-1356:owner-look`. The other criteria stand on the
verification above (`f77a95cb3`, landed as `160a726c0`), and the Linux and
Windows station runs below pass the core, finder and window rows. The card is
Done.

The host (pid 72837) was stopped and the candidate released with
`resume-development --all` at event 6. The frozen root
`/Users/jaeger/bpr1356-open-in-discuss-v001.or24x9ef` was removed with
`scripts/freeze_attended_build.py remove`. The session's state folder keeps
the record.

## Linux station run (bpr-main-linux, 2026-10-09)

At `b0ef8d7b9`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner: `core_workbench_interaction`, `qt_conversation_finder_scale_{100,125,200}` and `qt_workbench_window_scale_{100,125,200}` pass.

## Windows station run (bpr-main-win, 2026-10-09)

At `7672d5a8b`, Windows 11, MSVC 19.50, `build/windows-dev-tour-cli` (Debug), offscreen,
network-isolated runner, `-j4`:

- **Pass:** `core_workbench_interaction`, `qt_conversation_finder_scale_{100,125,200}`,
  `qt_workbench_window_scale_{100,125,200}` and, after `ca01e7bf3`,
  `application_workbench_composition_contract`.
