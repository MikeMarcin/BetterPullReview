---
schema: "implement-rfc.card.v1"
id: "BPR-1936"
title: "Tour cards collapse, edit like the overview description, and lose their vestigial and misplaced controls"
status: "Parked"
rfcs: []
depends_on: []
priority: "P1"
parked_on: "human-review"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-08, coordinator chat (author view of the Tour tab)","related_cards":["BPR-1900","BPR-1925","BPR-1930"]}
acceptance: [{"criterion": "AC1 No overlap between any route label and the rail; the filename chip is removed or repositioned per the recorded decision", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup: a stop entering a new file is as tall as one in the same file (no divider band); the chip and its roles are removed (decision 1)", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC2 Clicking the number collapses and expands stops, Problem and Solution; a Qt test checks this, plus no scroll jump", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup: ring click, header empty space, Space on the route and Return on the ring toggle the card, scroll held within 4 px; Problem and Solution collapse by their headers; qt_tour_click_scroll passes", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC3 Ready, next works or is gone, per the recorded decision; no permanently disabled primary button remains", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup and qt_tour_chat_in_tab: offered only for a ready, not-yet-Ready read stop; unconfirmed lines show 'Lines not confirmed' and Show in code leads; never visible disabled (decision 3)", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC4 'Use selected lines' appears only in edit mode, as a secondary action, and still works (test)", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup and qt_review_tour_stop_view: hidden (and its menu item) while read, a quiet link while written; application_review_tour_composition_public_path still retargets through it", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC5 Editing keeps the title wrapped and unobscured, with no mode label; Cancel and Save at the bottom like the overview description (Qt test)", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup: the long title wraps over several lines, nothing overlaps it, no tourStopState or Done; Cancel then Save (default) below the explanation; qt_review_tour_stop_view: Esc and Cancel restore the text, Save and Cmd+Return read", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC6 A selected card has no empty top space (a Qt test on geometry)", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup: the open card's title is within 14 px of its row top and the card sits at its row's top", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC7 Owner look: Accept/Reject on a frozen build", "outcome": "pending", "revision": "b509893ff", "evidence": {"kind": "observation", "detail": "v001 rejected by the owner (session 31c583b9e1f98a618d85351ab5e23da9, relayed by the coordinator): 'Takes too much space from the title. Move the trashcan to the edit mode down to the left of cancel.' v002: the trash is in the editor's foot at the left, apart from Cancel and Save. Frozen build /Users/jaeger/bpr1936-trash-in-editor-v002.75gc0rft of b509893ff; new short session d35207841379edd8b4ba2fb7c035abf2, candidate BPR-1936-trash-in-editor-v002, one Accept/Reject question, ready and awaiting the owner.", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#attended-review-v002"}}, {"criterion": "AC8 The documentation gate passes", "outcome": "pass", "revision": "7006c9149", "evidence": {"kind": "command", "detail": "verify_documentation.py prints DOCUMENTATION_OK on the merge with main", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v001-focused"}}, {"criterion": "AC9 Every stop's context menu offers Remove, enabled, and the card's trash removes exactly its own stop (Qt test)", "outcome": "pass", "revision": "b509893ff", "evidence": {"kind": "command", "detail": "qt_tour_stop_card_cleanup: Remove enabled in each of four stops' menus with no row selected and after a refused command; v002: no trash on a read card, and written the trash sits at the foot's left, apart from Cancel and Save, and removes only its own stop", "path": "docs/issues/40-parked/BPR-1936-tour-stop-card-cleanup.md#verification-v002-focused"}}]
---

# BPR-1936: Tour cards collapse, edit like the overview description, and lose their vestigial and misplaced controls

## Owner report (2026-11-08)

Six items, from the author's view of the Tour tab:

1. "tour stop in review mode tab (as author) draws the filename over the
   rail for some reason. I'm not even sure why the filename is there at all."
2. "You also cant collapse a tour stop (or problem/solution for that
   matter). Seems like you can only click off them to another one. I think
   clicking at least the number should collapse the card."
3. "The (Ready, next) button seems vestigial now, it's always greyed out for
   me except when it's the 'Accept' button."
4. "'Use selected lines' is weird, I don't think it belongs outside of edit
   mode on a stop for sure, and right now it only exists outside of edit
   mode. I also wonder if it's too prominent of a button for a niche task."
5. "The current edit controls on a tour stop obscure a lot of the stops
   title, especially the word 'editing', I'm also not a big fan of the title
   going from word wrapped to not in edit mode, makes it quite hard to see
   and edit longer titles. I think ideally to me it doesnt change to editing
   and done at all, in our other places like the overview description the
   pencil goes away and we get cancel / save at the bottom."
6. "There's sometimes a lot of deadspace at the top of a selected tour stop
   card, I can't tell why yet."
7. (Relayed by the coordinator, same day.) "'Remove' for a stop appears
   inconsistently in the context menu. Maybe there needs to be another way to
   remove a stop like trash can button."

## Change

Load `frontend-design:frontend-design` first.

1. **Filename over the rail.** Find out why the route draws a filename chip
   (e.g. `main.cpp`, `renderer.cpp` between stops) over the rail, and what
   it's for (file grouping between stops?). Either remove it, or place it so
   it never overlaps the rail and reads as an obvious file header. Recommend
   which, with reasons.
2. **Collapse.** Clicking a card's number glyph (and the header's empty
   space) toggles that card collapsed or expanded, for stops, Problem and
   Solution. Keyboard accessible, and it doesn't scroll the tour (keep
   `qt_tour_click_scroll` green).
3. **Ready, next.** Work out what "Ready" still means since BPR-1925 v003
   and BPR-1900's head chip, and why it's always disabled for the author
   (lines never confirmed? the gate never satisfiable from the card?). Either
   make it a working, meaningful step or remove it and the state it gates,
   keeping publication's real requirements. Explain the decision in the
   report and on the card.
4. **Use selected lines.** Move it into edit mode only, as a quieter
   secondary action (e.g. a small link or menu item near the location line).
   It sets the stop's lines from the current code selection. Keep its
   function and its test.
5. **Editing like the overview description.** Remove the "Editing" or
   "Done" mode label and the controls that cover the title. Entering edit
   (pencil, or click into the text) hides the pencil. The title stays
   word-wrapped and fully visible while editing (a wrapping editor, not a
   single line). Cancel and Save sit at the bottom of the card, matching the
   overview description's editor. Reuse that pattern or component.
6. **Dead space.** Find and remove the empty space at the top of a selected
   stop card: hidden or zero-content widgets, a stale proposal or chip slot,
   margins.
7. **Remove.** Find why Remove is sometimes missing from a stop's context
   menu, and make it always present for a stop the author can remove, acting
   on that card's stop. Add a small trash button among the card header's
   icon buttons (tooltip "Remove stop", accessible name). Removal stays
   undoable.

## Acceptance

- AC1. No overlap between any route label and the rail. The filename chip is
  removed or repositioned per the recorded decision.
- AC2. Clicking the number collapses and expands stops, Problem and
  Solution. A Qt test checks this, plus no scroll jump.
- AC3. Ready, next works or is gone, per the recorded decision. No
  permanently disabled primary button remains.
- AC4. "Use selected lines" appears only in edit mode, as a secondary
  action, and still works (test).
- AC5. Editing keeps the title wrapped and unobscured, with no mode label.
  Cancel and Save are at the bottom, matching the overview description (Qt
  test for wrap and controls).
- AC6. A selected card has no empty top space (a Qt test on geometry).
- AC7. Owner look: Accept/Reject on a frozen build.
- AC8. The documentation gate passes.
- AC9. Every stop's context menu offers Remove, enabled, and the card's trash
  removes exactly its own stop (Qt test).

## Decisions and implementation (v001, 2026-11-08)

`frontend-design:frontend-design` was loaded before any of this was designed.
The direction is subtraction: the card keeps one primary action at a time, its
title keeps its row, and quiet affordances (the number, a link, an icon) do
the niche work. Everything is in `src/adapters/qt/src/review_tour_editor.cpp`
and its header; Qt presents, and the gestures are the existing semantic tour
commands (edit, remove, complete, retarget).

1. **Filename over the rail: removed.** The route drew a file chip
   (`main.cpp`, `renderer.cpp`) in a 26-pixel band above every stop whose file
   differed from the stop before. It was a file grouping header from
   [[BPR-1593]]. It sat at the grip's left edge, so the rail ran through it,
   and every stop already names its file in its own source chip
   (`renderer.cpp:21–23`), so it said nothing new. Repositioning it would keep
   a header for a grouping the route does not otherwise show and still cost a
   band per file change. `divider_role`, `file_role` and `divider_band` are
   gone, and the chat toggle's hit box no longer allows for the band.
2. **Collapse.** The open stop's card is a `stop_card` whose number is a real
   button (`tourStopCollapse`, named "Collapse stop"): a click, Return on it,
   or a press in the header's empty space collapses the card. The stop stays
   current, its row drawn as any other's (`card_role`). A click on the
   collapsed row or Space on the route opens it again, and choosing another
   stop opens that one expanded. Collapsing a stop being written saves and
   reads it first (it is not Cancel). Problem, Solution and Stops collapse by
   their own step headers the same way (`StepCollapsed`). Every toggle holds
   the scroll (`hold_scroll`), so the tour does not move.
3. **Ready, next: kept as a working step, shown only when it can act.** Since
   [[BPR-1925]] v003 "Ready" means the author has confirmed a stop for
   publication. Publication still needs every stop's lines confirmed
   (`core::ready`) and every outline-suggested stop marked Ready, so the state
   cannot go. It was always disabled for the owner because an agent-drafted
   stop's lines are a destination to look at (`Mapping` unavailable, no
   `Original`) until the author confirms them with Use selected lines; the
   card offered a primary button it knew could not act. Now the button shows
   only for a read stop that is ready and not yet Ready; a Ready stop needs no
   button, and while written Save is the one primary action. When the lines
   are not confirmed the location line says "Lines not confirmed" (its
   tooltip says how) and Show in code leads.
4. **Use selected lines.** A quiet link (`step_change`, the accent words that
   Change uses) on the card's action row after the drafting help, shown only
   while the stop is written; in a narrow card it waits behind More. Its
   route and More menu item (`tourRetarget`) is offered only while written
   too. It still sets the stop's lines from the code's selection. The first
   placement, beside the location line, made a long location wrap and moved
   the explanation 12 pixels when the editor opened, which
   `qt_tour_chat_in_tab` ([[BPR-1925]] v002) refuses.
5. **Editing like the overview description.** The "Editing" state word and
   Done are gone from the title row. The pencil hides while writing. The
   title is `stop_title_editor`, a wrapping plain-text editor as tall as its
   lines with the explanation's quiet box, Return going on to the
   explanation and pasted line breaks becoming spaces. Cancel (quiet) and
   Save (default) sit at the foot, right-aligned, as
   `overviewDescriptionCancel`/`overviewDescriptionSave` do (the description
   builds its own foot; there is no shared component to reuse, so the layout
   and button kinds are copied). Text is still saved in the private draft as
   it is typed: Save closes the editor (Command/Control+Return), Cancel (Esc)
   restores the title and explanation the editor opened on, as one edit, and
   reads the stop. Lines set with Use selected lines while editing are not
   undone by Cancel (Undo does).
6. **Dead space.** It was the file chip's band: a selected card in a new file
   got `setContentsMargins(0, 26, 0, 0)` above its title. With the band gone
   the title sits at the ring's height.
7. **Remove.** It was there but disabled: `update_stop_actions` enabled Remove
   (and Duplicate) only while a route row was *selected*, and a stop opened by
   the host, the keyboard or Next is current without being selected. It was
   also disabled after any refused command, such as a refused Ready. Now it is
   enabled whenever there is a stop and the draft can be written (only unsaved
   text past the limit holds it back), and an empty selection means the open
   stop. The open card has the stop menu as its own context menu, the read
   explanation's menu has Remove stop, and the title row has the comments'
   trash (`tourStopRemove`, "Remove stop"), which removes exactly its card's
   stop and says "Removed stop N. Command+Z brings it back." Undo is the
   tour's structural undo.

`--attended-tour-cards` (with `--attended-tour-drafting`) drafts the tour with
the stub agent, accepts it, gives stop 2 a long title through its editor, saves
and leaves the card open: `ATTENDED_TOUR_CARDS_SHOWN`.

### Verification (v001, focused)

On `5f4851e21` and again on the merge with main `7006c9149` (BPR-1937), dev
build:
- `qt_tour_stop_card_cleanup` (new, `tests/adapters_qt/tour_stop_card_cleanup_tests.cpp`):
  AC1 to AC6 and AC9.
- `qt_review_tour_stop_view` (updated for Cancel/Save), `qt_tour_chat_in_tab`
  (updated for Ready, next), `qt_tour_click_scroll`, `qt_review_tour_reaffirm`,
  `qt_review_tour_steps`, `qt_tour_agent_session_view`, `qt_tour_side_chat`,
  `qt_drawer_surfaces_fit_scale_100/125/200`: pass.
- `application_review_tour_composition_public_path` and
  `application_review_tour_publication_native` pass (on `5f4851e21`).
- `application_review_round_trip_composition_contract` passes, its app source
  digest resealed from its own output (`0113c751…` after the merge).
- `coding-format-check` passes; `verify_documentation.py` prints
  `DOCUMENTATION_OK`.
- The offscreen look on the dev build prints
  `ATTENDED_TOUR_CARDS:wrapped=1:read=1:card=1:disabled_ready=0:trash=1` and
  `ATTENDED_TOUR_CARDS_SHOWN`, light and dark.
- Not run: `test_profile_tour_handoff_public_path` is not built in this
  configuration; its change is the same mechanical title-editor rename.

### Attended review (v001)

- Frozen build `/Users/jaeger/bpr1936-tour-stop-cards-v001._sl3fspd` of
  `7006c9149` (executable SHA-256
  `7ec10ecd07a73c934b0a0a46d78e810087800d66ab5dd991c1cc7c9031b03a27`). Its
  launcher's own offscreen check reached `ATTENDED_TOUR_CARDS_SHOWN`.
- Launch: `python3 /Users/jaeger/bpr1936-tour-stop-cards-v001._sl3fspd/launch.py`.
- New short session `31c583b9e1f98a618d85351ab5e23da9`, state
  `/Users/jaeger/.codex/attended-review/bpr1936-v001/state`, host started with
  `serve --tailscale`.
- One candidate, `BPR-1936-tour-stop-cards-v001` (revision
  `7006c9149-v001`), with one Accept/Reject question.
- Status: rejected by the owner (see below); host stopped and frozen root removed.

### Owner verdict on v001 (2026-11-08)

Rejected (session `31c583b9e1f98a618d85351ab5e23da9`, relayed by the
coordinator after v001 merged as `00de17641`): "Takes too much space from the
title. Move the trashcan to the edit mode down to the left of cancel."

### v002 (2026-11-08)

`frontend-design:frontend-design` loaded again. The trash (`tourStopRemove`,
"Remove stop", the same tooltip and Cmd+Z notice) leaves the title row: a read
card's title has the row but for the chat toggle. While the stop is written
the trash is the first thing on the foot's row, at its left, before Show in
code; Cancel and Save stay at the right, so a stretch separates the trash from
them and it is not hit for Cancel. Remove stays in the stop's right-click
menus (route row, open card, read explanation).

### Verification (v002, focused)

On the dev build, branch `bpr-1936-v002` from `00de17641`:
- `qt_tour_stop_card_cleanup`: a read card has no trash; written, the trash is
  on the foot's row, in its left third, more than 40 pixels before Cancel, and
  the title reaches within 50 pixels of the row's end; the trash still removes
  only its own stop; Remove in every stop's menu.
- `qt_review_tour_stop_view`, `qt_tour_chat_in_tab`, `qt_tour_click_scroll`,
  `qt_review_tour_reaffirm`, `qt_review_tour_steps`,
  `qt_tour_agent_session_view`, `qt_tour_side_chat`,
  `qt_drawer_surfaces_fit_scale_100/125/200`: pass.
- `application_review_round_trip_composition_contract` passes unchanged (no
  app source change); `coding-format-check` passes; `verify_documentation.py`
  prints `DOCUMENTATION_OK`.
- The offscreen look prints `ATTENDED_TOUR_CARDS:...:trash=0` (no trash on
  the read card) and `ATTENDED_TOUR_CARDS_SHOWN`, light and dark.

### Attended review (v002)

- Frozen build `/Users/jaeger/bpr1936-trash-in-editor-v002.75gc0rft` of
  `b509893ff` (executable SHA-256
  `17c0b59d3f8118ebee915a82a848415e51bfe18b7a6ceb2d311ebf0ee84c6745`). Its
  launcher's own offscreen check reached `ATTENDED_TOUR_CARDS_SHOWN`.
- Launch: `python3 /Users/jaeger/bpr1936-trash-in-editor-v002.75gc0rft/launch.py`.
- New short session `d35207841379edd8b4ba2fb7c035abf2`, state
  `/Users/jaeger/.codex/attended-review/bpr1936-v002/state`, host started with
  `serve --tailscale`.
- One candidate, `BPR-1936-trash-in-editor-v002` (revision `b509893ff-v002`),
  with one Accept/Reject question.
- The v001 host is stopped and its frozen root removed.
- Status: ready, awaiting the owner.
