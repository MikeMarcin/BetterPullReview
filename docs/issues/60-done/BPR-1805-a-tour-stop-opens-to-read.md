---
schema: "implement-rfc.card.v1"
id: "BPR-1805"
title: "A tour stop opens to read, rendered, with a pencil to edit"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"feature","found_by":"owner report, 2026-10-28","related_rfcs":["RFC-BPR-0039","RFC-BPR-0085"],"related_cards":["BPR-1593","BPR-1775","BPR-1783","BPR-1784"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "64825ac2fb65c45851062eec272bb400d7d197b1", "evidence": {"kind": "command", "detail": "core_tour_stop_view (tests/core/tour_stop_view_tests.cpp) pins decide_tour_stop_view: a written stop opens to read; an unwritten one (whitespace-only explanation) opens to edit; going back to a stop that was being edited reads it; presenting the open stop again keeps its mode; the pencil edits and Done reads; held text differing from the stop (title or body) or preedit keeps edit on Done and on open; a read-only draft is always read; no stop is nothing open; stable names. Passes."}}, {"criterion": "AC2", "outcome": "pass", "revision": "64825ac2fb65c45851062eec272bb400d7d197b1", "evidence": {"kind": "command", "detail": "qt_review_tour_stop_view (tests/adapters_qt/review_tour_stop_view_tests.cpp) drives review_tour_editor with core::apply. A written stop opens read with rendered Markdown (no ** or list markers), and its named, Tab-reachable pencil takes focus from show_stops. The pencil opens the title and source with focus in the source; typing saves and stays in edit. Moving to another stop and going back both read. Return on the reader edits; Escape reads with focus on the pencil; Command/Control+Return and Done read. A bounds refusal keeps the editor and its text on Done and Escape, and the next accepted edit then reads. A new empty stop opens in edit with title focus; Done on it reads 'Untitled stop' with the empty-explanation text. The tour page reads and edits the same way. A read-only draft reads with the pencil disabled. Passes."}}, {"criterion": "AC3", "outcome": "pass", "revision": "64825ac2fb65c45851062eec272bb400d7d197b1", "evidence": {"kind": "command", "detail": "After rebase onto main (1ff1314c4 or later), all 37 tests matching tour, qt_workbench_window_scale_100 and qt_drawer_surfaces_fit pass on macOS dev, as do qt_workbench_window_scale_125 and _200. They include application_review_tour_composition_{public_path,scale_125,scale_200}, application_review_tour_writing, all application_review_tour_publication_*, cli_ordinary_{native,stop}, qt_review_tour_steps and qt_drawer_surfaces_fit_scale_{100,125,200}. The composition course now opens the source through the pencil before keyboard undo and agent writing on a written stop. workbench_window_tests checks the read view on the route and the page."}}, {"criterion": "AC4", "outcome": "pass", "revision": "64825ac2fb65c45851062eec272bb400d7d197b1", "evidence": {"kind": "artifact", "detail": "Offscreen renders from qt_review_tour_stop_view, light and dark, saved outside the repository in the session scratchpad tour-read-renders/: route-read, route-edit, route-new, route-empty-read, page-read, page-edit. Offscreen forced-style renders are layout evidence only."}}]
---

# BPR-1805: A tour stop opens to read, rendered, with a pencil to edit

## Request

The owner, 2026-10-28, verbatim:

> Tour tabs default view of a stop seems to be edit when you go back to it
> but read with proper rendering and a pencil to edit would be much more
> useful

## Grounded current behavior

`review_tour_editor` has one presentation of the open stop, `StopFields`:
a title `QLineEdit` and the explanation's Markdown source in a
`scoped_prose_editor`. Every edit is dispatched to the core as it is typed
(`edit_stop`), so there is no save step. Choosing a stop on the route or on
the tour page binds that stop's document to the editor and shows it, so
going back to a written stop always shows its raw Markdown source. The
rendered form exists only in the Preview tab and in the publication column.

## Outcome

- A written stop opens to read: its title as text, its explanation rendered
  by the same `markdown_body_view` comments use (in place, sized to its
  content), and a pencil (the comments' Edit glyph) beside the title.
- The pencil, or Return in the read explanation, opens the title and source
  with the keyboard in them. Done, Escape or Command/Control+Return (as a
  comment's editor takes it) reads the stop again. Edits are still saved as
  they are typed.
- A stop with nothing to read yet (a new, empty stop) opens to be written,
  with the keyboard in its title as BPR-1783 has it.
- Text the draft has not taken (the local-limit refusal, or uncommitted
  input-method text) keeps the editor open with the text. Done says why.
- A read-only draft is read, with the pencil unavailable.
- The same in the drawer's route and on the tour page.
- Core decides the mode; Qt presents it and performs no effect.

## Acceptance

- Core tests pin the transitions: open, begin edit, finish edit, unsaved
  text, read-only, no stop.
- Qt tests drive `review_tour_editor`: read on open and on going back,
  rendered not raw, pencil named and reachable, keys, unsaved text kept, new
  stop in edit with title focus, page mode, read-only draft.
- Offscreen renders, light and dark, outside the repository.
- Existing tour tests still pass.

## Built

- **Core.** `bpr/core/tour_stop_view.hpp` adds two closed enums,
  `tour_stop_view_mode` (read, edit) and `tour_stop_view_event` (open,
  begin_edit, finish_edit), with counts pinned by `static_assert`. The pure
  `decide_tour_stop_view(current, input)` decides from the stop, the event,
  whether the draft can be written, the text the editors hold and whether
  input is being composed. `tour_stop_unwritten` and
  `tour_stop_text_unsaved` are public so tests can call them.
- **Qt** (`review_tour_editor`). The stop's fields gain a read title
  (`tourStopReadTitle`), a reader (`tourStopReader`), a pencil
  (`tourStopEdit`) and Done (`tourStopDone`). The reader is the comments'
  `markdown_body_view` shown in place and sized to its rendered height. The
  pencil uses the comments' Edit glyph and has the accessible name "Edit
  stop".
- **Read and edit.** Reading hides the title field, the source and the
  agent's Enhance or Generate button. Editing hides the reader and the
  pencil. The adapter asks the core on every presentation, on the pencil and
  on Done.
- **Keys.** They follow comment editing. Return in the reader opens the
  editor. Escape or Command/Control+Return in the title or source goes back
  to reading.
- **Focus.** Focus follows the mode. A read stop focuses the pencil. An
  edited stop focuses the source, or the title when it is untitled. Opening
  the tour page focuses the pencil when the stop is read.
- **Details.** Leaving edit cancels a pending agent writing request. The
  reader's height reports are queued, so placing the route card does not
  recurse. On the tour page the stop keeps to its content height. Widgets
  perform no effects. The Markdown renderer and author labels are unchanged.
  `frontend-design:frontend-design` was loaded first.
- **Docs.** RFC-BPR-0085 D4 describes the read view and pencil.

## Not verified

- The owner has not looked at it on a real display, light or dark. No
  attended review was prepared; the coordinator may add one.
- Windows and Linux were not run; macOS offscreen only.
- Return on the reader, Escape and Command/Control+Return were sent as
  synthetic key events, not through the native key map.
- `coding-tidy` ran on the changed translation units only, not the whole
  tree.

## Owner review

Attended screenshot review session 00fef88b0f9a8d6133fc06b452010a90 (state /Users/jaeger/bpr-attended/tour-cards-v001/state, closed 2026-10-29) at efcaafa33: edit view accepted; read view rejected as not judgeable — the renders carried no code or rich Markdown. A re-review with richer stop content is pending.
