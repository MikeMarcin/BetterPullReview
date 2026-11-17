---
schema: "implement-rfc.card.v1"
id: "BPR-2220"
title: "The prompt library always has a prompt selected"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2179"]
acceptance: [{"criterion": "AC-a", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "qt_prompts_page; owner Accept BPR-2220-always-selected (look prompt-library-v002, decision 2aa571a0)"}}, {"criterion": "AC-b", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "qt_prompts_page; owner Accept BPR-2220-always-selected (look prompt-library-v002, decision 2aa571a0)"}}, {"criterion": "AC-c", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "qt_prompts_page; owner Accept BPR-2220-always-selected (look prompt-library-v002, decision 2aa571a0)"}}, {"criterion": "AC-d", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "qt_prompts_page; owner Accept BPR-2220-always-selected (look prompt-library-v002, decision 2aa571a0)"}}, {"criterion": "AC-e", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "prompts_page.cpp and .hpp only; no core change"}}, {"criterion": "AC-f", "outcome": "pass", "revision": "47cd76bc8", "evidence": {"kind": "command", "detail": "qt_prompts_page at QT_SCALE_FACTOR 1.0, 1.25, 2.0 and native light/dark; owner Accept BPR-2220-tools-on-filter-row (look prompt-library-v002, decision 3fd3d8bb)"}}]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-2220--BPR-2220--ed478f0bfc51.json","origin":"owner, 2026-11-16, Settings › Prompts"}
---

# The prompt library always has a prompt selected

## Origin

The owner, 2026-11-16: "Also the bottom of prompt library looks like a broken
menu when no prompt is selected... either always have one selected or hide
that bottom area until one is."

The bottom area is Settings › Prompts' editor ([[BPR-2179]]): beside the
library in a wide window, under it in a narrow one. With nothing selected it
showed every field disabled and empty.

Coordinator's decision: always have a prompt selected; hide the editor only
when the list is truly empty.

## Scope and acceptance

- **AC-a.** When the library opens, its first prompt is selected and the
  editor shows it.
- **AC-b.** When the selected prompt is deleted, its neighbour in the list is
  selected (the next row, else the one before).
- **AC-c.** A filter or search that still shows the selected prompt keeps it;
  one that hides it selects the first prompt shown.
- **AC-d.** A filter or search that shows nothing hides the editor and the
  list says so; clearing it shows the editor again with the selection it had.
- **AC-e.** Presentation only: the selection policy lives in the Qt page; the
  core's prompt editor is unchanged.
- **AC-f.** The owner, 2026-11-16, on the same page: "we can probably move the
  restore built-ins and new prompt to the same row as the filter for the
  prompt library." New prompt and Restore built-ins sit on the filter's row,
  at its right; the filter takes the room left. The row fits the page's
  narrowest width at 100, 125 and 200 percent without clipping: as the page
  narrows the title, then the filter choices, go on their own lines.

The owner's look on a frozen build (Fixture agent only, a fresh short session,
one Accept/Reject for each of AC-a to AC-d and AC-f) is filed, not waited on.

## Intended verification

`qt_prompts_page` asserts AC-a to AC-d and AC-f (run at 100, 125 and 200
percent); native-look renders light and dark, before and after.

## Look prompt-library-v002 (filed 2026-11-16, closed)

Session `1df4dc88417b17eabe9b916084c4b6dc`, state
`/Users/jaeger/.attended-review-state/prompt-library-v002-2026-11-16/state`
(URL in `serve.log` beside it), served on the tailnet. Root
`/Users/jaeger/prompt-library-v002.tzl2onjk`, frozen from `e203eadb5`
(tests-on, Fixture agent; its check opened the pull request and started no
real agent). Two questions, one Accept/Reject each:
`BPR-2220-always-selected` (AC-a to AC-d) and `BPR-2220-tools-on-filter-row`
(AC-f), with native off-screen renders from `qt_prompts_page`, before and
after. A partial root from an earlier freeze that the machine's memory
exhaustion cut off, `/Users/jaeger/prompt-library-v001.m59svv_j`, has no
marker, so `freeze_attended_build.py remove` refuses it.

Verification at `47cd76bc8`: `qt_prompts_page` passes, and passes run directly
at `QT_SCALE_FACTOR` 1.0, 1.25 and 2.0 and in the native look, light and dark.
The old page fails ten of its new checks.

### Look prompt-library-v002 handed back (owner, 2026-11-16)

Accepted, no note: `BPR-2220-always-selected` (decision `2aa571a0`) and
`BPR-2220-tools-on-filter-row` (decision `3fd3d8bb`), read from the session's
status at sequence 11. The host is stopped and the frozen root removed.
