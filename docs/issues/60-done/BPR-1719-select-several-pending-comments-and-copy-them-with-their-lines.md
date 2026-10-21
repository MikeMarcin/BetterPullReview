---
schema: "implement-rfc.card.v1"
id: "BPR-1719"
title: "Select several pending comments on Finish your review and copy them with their lines"
status: "Done"
rfcs: []
depends_on: ["BPR-1345", "BPR-1590", "BPR-1715"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1719--BPR-1719--618929565a78.json","kind":"ux","origin":"owner request 2026-10-21 (relayed by the coordinator)","owner_note":"On the finish review page want to be able to multi select comments and copy to clipboard to get the file line and full comment for all selected","related_cards":["BPR-1345","BPR-1590","BPR-1712","BPR-1715"],"related_rfcs":["RFC-BPR-0008","RFC-BPR-0085"],"frontend_design":"frontend-design:frontend-design loaded before the selected rows' look and the copy wording"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"d6c0aa176aa3abe1283d71d800aca69e237008b1","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_review_submission_editor_scale_(100|125|200)$'","detail":"A page with three pending comments (renderer.cpp line 431; renderer.cpp base lines 434-435 with a body over 1,000 characters; scale.hpp lines 12-14 holding a suggestion block and a trailing line break), through mouse and key events on the rows: a plain click selects one row; Shift-click selects through to the clicked row; Command-click (Qt's Control) takes a row out and puts it back; a plain click selects one row again; Shift+Down extends to the next row and moves focus there; Shift+Up twice extends upwards from the anchor; a plain Down selects the next row alone; Command-A selects every row and its ShortcutOverride is accepted. Return on the focused row still opens only that comment in Review. Failing-before control: with review_submission_editor.cpp as on main 00aeb9513 (unchanged on main ae867c2db), the row fails at 'a plain click selects one row, and Command-C copies its file, line and whole comment'."}},{"criterion":"AC2","outcome":"pass","revision":"d6c0aa176aa3abe1283d71d800aca69e237008b1","evidence":{"kind":"command","command":"ctest --preset dev -R '^qt_review_submission_editor_scale_(100|125|200)$'","detail":"Each copy is compared with the exact clipboard text: 'src/engine/renderer.cpp:431' then the comment's two lines; 'src/engine/renderer.cpp:434-435 (LEFT)' then the long comment whole, trailing breaks dropped; 'src/engine/scale.hpp:12-14' then the suggestion block; a blank line between comments, none after, in the page's order. Command-C's ShortcutOverride is accepted, ahead of any window shortcut. The menu key and Shift+F10 open the row menu (Open in Review, Open in Discuss, Show in Threads, a separator, Copy); its Copy shows the platform's Copy key and copies the selection it opened inside; a right-click outside the selection selects and copies that row alone. With the review submitting (GitHub's answer unknown), the rows still offer no Delete and Command-A then Command-C copy all three held comments with their text. A relisted page (a comment's body and revision changed) keeps each comment's selection by its draft and copies the new words. Failing-before control as AC1; the existing row-menu check also fails on main, which has no Copy."}},{"criterion":"AC3","outcome":"pass","revision":"d6c0aa176aa3abe1283d71d800aca69e237008b1","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_review_submission_editor_scale_(100|125|200)|qt_workbench_window_scale_100)$'; python3 _coding-standard/scripts/format.py --dry-run <changed files>; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths <changed units>; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py","detail":"Selecting and copying send no editor intent, call no Delete, open no question and leave the shown draft as given; the earlier BPR-1345, BPR-1590 and BPR-1715 courses in the same row (a click opens nothing, double click and Return open, Shift for Discuss, Show in Threads, Delete asks first, held rows offer no Delete) still pass. qt_workbench_window_scale_100 passes: the shortcut catalogue matches every live binding and no window-wide key is held twice; the sheet's pending rows still select on a click and open nothing. The three new catalogue entries are read by the rows only (Place 'In Finish your review's comments'): no window, menu-bar or desktop binding holds Copy, Select All, Shift+Up/Down or the menu key. clang-format 22 passes on the changed files; clang-tidy (LLVM 22) with warnings as errors passes on review_submission_editor.cpp, keyboard_shortcuts.cpp and review_submission_editor_tests.cpp. Both reseal checks report nothing to reseal (no src/app change). Both documentation gates pass with the card under a stand-in allocated ID; with the provisional ID the gate refuses only the card ID format. macOS, the dev preset built -O0 -g0."}}]
---

# BPR-1719: Select several pending comments on Finish your review and copy them with their lines

## Owner request

The owner, 2026-10-21, verbatim:

> On the finish review page want to be able to multi select comments and copy
> to clipboard to get the file line and full comment for all selected

The main use is a backup. A review that GitHub has not confirmed
([[BPR-1715]]) holds its comments, and its rows offer no Delete. Copying them
keeps the reviewer's words somewhere safe whatever happens to the attempt.

## Before

- Finish your review's pending rows ([[BPR-1345]], [[BPR-1590]]) took one row
  at a time. A click focused a row, a double click or Return opened it, and
  the row menu offered Open in Review, Open in Discuss and Show in Threads.
- A row showed only the first line of its comment. The whole comment was in
  the tooltip, which cannot be copied. Up and Down moved focus through the tab
  chain, onto the row's Delete when it had one.

## Change

- **Selecting.** The rows take an extended selection, as Find in files'
  results do ([[BPR-1712]]):
  - a plain click selects one row and opens nothing;
  - Shift-click selects from the last row picked to the clicked one;
  - Command-click (Ctrl-click elsewhere) adds a row or takes it out;
  - Up and Down go row to row and select the row they reach, and stop at the
    ends; with Shift they select from the anchor;
  - Command-A (Ctrl+A) selects every row.

  A selected row has a firmer wash than the pointer's and a highlight bar at
  its leading edge, so it reads apart from the focus ring in Light and Dark.
- **Copying.** Command-C (Ctrl+C), and **Copy** after a separator in the row
  menu (right-click, the menu key or Shift+F10), write plain text in the
  page's order. With nothing selected they copy the focused row's comment.
  Each comment is a header line, then the whole comment as written with its
  trailing line breaks dropped, and a blank line comes between comments:

  ```text
  src/engine/renderer.cpp:431
  question: Where does 0.01 come from?
  It looks arbitrary.

  src/engine/renderer.cpp:434-435 (LEFT)
  nit: scale could be constexpr.
  ```

  The header is the full path and the one-based line, or `start-end` for a
  range. A comment on the base side gets " (LEFT)". A review carries only
  line drafts (each has an anchor and a head), so a row is never a file
  comment or a reply, and neither mark is ever written. The summary is not a
  row and is copied from its own field.
- **The menu.** A right-click inside the selection keeps it; outside, it
  selects that row alone. The openings act on the row the menu is on.
- **Keys.** The rows claim Copy and Select All in `ShortcutOverride`, so no
  window shortcut takes them. The shortcut catalogue gains "Select more
  comments" (Shift+Up, Shift+Down), "Select every comment" and "Copy the
  selected comments with their lines", read in Finish your review's comments.
  A row's accessible description says "Selected." when it is selected, then
  how to open, select, copy and reach the menu. Its accessible name, which
  screen readers announce, is unchanged.
- **Unchanged.** Return, double click, Shift for Discuss, Show in Threads,
  Delete and its question, the held rows' missing Delete, and Tab order. A
  relisted page keeps each comment's selection by its draft.

The clipboard write is the page's one effect, a Qt presentation effect. No
core, application-host or composition code changes, and the selection never
reaches a draft.

## Acceptance

- **AC1.** Shift-click, Command-click (Ctrl-click), Shift+arrow and
  Command-A (Ctrl+A) select pending comments; a plain click and arrow select
  one; Return still opens the focused comment. The cases fail before the
  change.
- **AC2.** Command-C (Ctrl+C) and the menu's Copy write the exact text above
  in the page's order, a range and a base-side comment included. The menu
  opens from the menu key and Shift+F10. A review awaiting GitHub's answer
  copies its held comments. The cases fail before the change.
- **AC3.** Selecting and copying change no draft, send no command and open
  no question. The earlier Finish your review courses, the workbench window's
  catalogue check, format, tidy and the documentation gates pass.
