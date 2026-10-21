---
schema: "implement-rfc.card.v1"
id: "BPR-1682"
title: "The \"+\" and Return open their draft on a pull request without threads"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner bug report 2026-10-20","owner_note":"i'm no longer able to get the add comment mouse over column next to line number control to come up or click on a line I have selected.","owner_followup":"there is no thread on that line","regressing_commit":"a90419791","reservation":"docs/issues/_reservations/BPR-1682--BPR-1682--5e96c8488820.json","related_cards":["BPR-1212","BPR-1214","BPR-1335"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"c0e06d464","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_workbench_line_draft$'","detail":"test_the_plus_on_a_pull_request_without_threads_opens_its_draft: a pull request adopted with no conversation, a 41-line head with an added line 14; every pointer and key event goes to the window through QWindowSystemInterface, so Qt's own hit-testing picks the code, the gutter or the button. A click on line 15's code selects it; the pointer in its \"+\" column shows the \"+\"; a click on the \"+\" opens 'Your draft on line 15' in the drawer with its editor focused and the line's writing mark; Esc drops the empty draft and its mark, the drawer goes back to its empty page, and line 15 shows its \"+\" again. Failing-before control: with github_conversation_panel.{hpp,cpp} at 2085a4d6e (unchanged since 3fdbc3949, where it was run) the case fails at 'on a pull request without threads, clicking the \"+\" opens a draft on line 15 in the drawer, with focus' (writing=1, composer=0, notes hidden). macOS offscreen at 1x."}},{"criterion":"AC2","outcome":"pass","revision":"c0e06d464","evidence":{"kind":"command","command":"ctest --preset dev -R '^(command_correctness_registry_contract|qt_workbench_line_draft|qt_workbench_conversation_reveal|qt_workbench_window_scale_100|qt_github_conversation_panel|qt_conversation_cards_view|qt_diff_workbench_scale_(100|125|200)|qt_composed_(github|loading|inline_drawer|long_thread)_scale_100|qt_composed_resident_drawer_filters)$'","detail":"A snapshot with no conversation and no draft still shows the empty page (test_discard_and_back_keep_the_comments_drawer's negative control, the composed available-empty, unknown and unsupported thread states); a snapshot with threads composes in the drawer as before (test_compose_in_the_drawer); the \"+\" hover, column, tooltip and drag checks in the diff workbench pass at 1x, 1.25x and 2x; the registry's workbench source pin and the case inventory are resealed with the new --line-draft case. coding-format-check (clang-format 22.1.8) passes; clang-tidy --warnings-as-errors on github_conversation_panel.cpp and workbench_window_tests.cpp reports only the two existing performance-avoid-endl findings in the test runner's case trace (b059676cc), none in this change. macOS, a Debug configuration of the dev preset built without debug information (-g0) to fit the disk."}}]
---

# BPR-1682: The "+" and Return open their draft on a pull request without threads

## Owner note

The owner, 2026-10-20, verbatim:

> i'm no longer able to get the add comment mouse over column next to line
> number control to come up or click on a line I have selected.

and, asked whether a thread was on the line: "there is no thread on that
line". A photo of the screen showed line 15 selected under an added line 14,
its number in a dashed outline, a thin blue bar at the boundary between the
number and the code, the I-beam on that boundary, and no "+".

## Cause

The dashed outline is the gutter's mark for my draft, and the blue bar is the
bracket of a draft being written ([[BPR-1212]]). A line with a mark offers no
"+" ([[BPR-1335]]), by design. So line 15 had a draft being written, yet no
composer was on screen.

The Comments drawer's notes live on the loaded page of the conversation
panel's stack. Since [[BPR-1214]] that page shows only while the snapshot has
a conversation; a pull request no one has commented on shows the empty page.
[[BPR-1212]] slice 2 (a90419791, 2026-10-08) moved a new comment from the full
composer into the drawer's notes. On a pull request without conversations the
"+", Return and Add comment then began the draft on the hidden notes:
`conversation_cards_view::begin_draft` marked the line as being written, but
the hidden view built no open card, so no editor opened and the keyboard
stayed in the code. Esc went to the code, so the draft stayed and its mark
kept the line's "+" away until another line was selected.

Earlier probes used the conventional-comments fixture, which has threads, so
the notes' page was showing and the "+" worked there.

## Change

`github_conversation_panel` shows the notes' page while the snapshot has a
conversation or the notes hold a note, my draft included, once a snapshot is
adopted. The notes' layout observer, which already refreshed the summary,
now refreshes the page too, so beginning a draft shows the notes before
`begin_draft` looks for the open card, and dropping an empty draft returns
to the empty page. A cleared panel (no snapshot) keeps its empty page.

The diff workbench's "+" hit-testing, hover and painting are unchanged.

## Acceptance

- **AC1.** On a pull request without conversations, with every event sent to
  the window: a click selects line 15, the pointer in its "+" column shows
  the "+", and a click on it opens its draft in the drawer with focus and the
  writing mark. Esc drops the empty draft and its mark, the drawer shows its
  empty page again, and the line offers its "+" once more. The case fails
  before the change.
- **AC2.** A snapshot with no conversation and no draft keeps the empty page.
  Composing on a pull request with threads, the diff workbench's "+" checks
  and the composed drawer courses are unchanged.

## Verification

- `qt_workbench_line_draft` runs the new
  `test_the_plus_on_a_pull_request_without_threads_opens_its_draft`.
  Failing-before control: with the panel at 2085a4d6e (as at 3fdbc3949, where it was run) the case fails when the
  "+" is clicked (`writing=1 composer=0`).
- `qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100`,
  `qt_github_conversation_panel`, `qt_conversation_cards_view`,
  `qt_diff_workbench_scale_{100,125,200}`, and the composed GitHub,
  loading, inline drawer, long thread and resident drawer filter courses
  pass.
- The new case joins `--line-draft`, so `tests/command_correctness_registry.json`
  and `tests/workbench_case_inventory.json` carry the new source pin and the
  inventory lists the case; `command_correctness_registry_contract` and
  `scripts/workbench_case_inventory.py --execute-lists` pass.
- `coding-format-check` (clang-format 22.1.8) passes; clang-tidy on the
  changed units as recorded in AC2.

macOS offscreen only. The change is Qt widget state with no platform branch;
Windows and Linux station runs are not gating.
