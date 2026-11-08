---
schema: "implement-rfc.card.v1"
id: "BPR-1936"
title: "Tour cards collapse, edit like the overview description, and lose their vestigial and misplaced controls"
status: "Backlog"
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
