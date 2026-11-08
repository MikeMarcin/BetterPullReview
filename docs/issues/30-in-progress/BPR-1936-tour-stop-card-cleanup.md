---
schema: "implement-rfc.card.v1"
id: "BPR-1936"
title: "Tour cards collapse, edit like the overview description, and lose their vestigial and misplaced controls"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "linux", "windows"]
metadata: {"kind":"defect","found_by":"owner, 2026-11-08, coordinator chat (author view of the Tour tab)","related_cards":["BPR-1900","BPR-1925","BPR-1930"]}
acceptance: []
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
