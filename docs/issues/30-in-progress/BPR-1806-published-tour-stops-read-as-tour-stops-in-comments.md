---
schema: "implement-rfc.card.v1"
id: "BPR-1806"
title: "Published tour stops read as tour stops in comments"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"ux","found_by":"owner, 2026-10-28","related_cards":["BPR-1086","BPR-1222","BPR-1785","BPR-1781","BPR-1801","BPR-1802"]}
acceptance: []
---

# BPR-1806: Published tour stops read as tour stops in comments

Owner, 2026-10-28: "I'm kind of disappointed that my tour stop render in the
review tab like any other comment rather than getting recognized and treated
specially to reduce the noise that is necessary for the website. It would
also be nice if they just looked special."

A published stop is a review comment (or, in compatibility mode, a
conversation comment) whose body `core::render_tour_stop` writes: an
optional pinned source line, `### PR Tour · N — Title` (legacy
`### Author's tour · `), the narrative, and the Previous / Tour overview /
Next paragraph with BPR-1785's note. The website needs that scaffolding; a
reader in the app does not.

## Acceptance

- AC1. Core recognizes exactly the published envelope, in either newline
  style, and nothing looser: number, unescaped title, narrative span,
  pinned source span and the navigation's destinations. Ordinary comments,
  a heading without the rest of the envelope, prose before or after it, a
  leading zero, zero, more than `maximum_tour_stops`, another heading level,
  missing blank lines, wrong link order or labels, a first stop with
  Previous, and fenced, quoted or unclosed-fence bodies are not stops. Core
  counts a tour's stops only when the stops at hand prove it (1 to M once
  each, Previous and Next exactly where they belong, one Tour overview), and
  finds the window tour's stop for a comment by its remote binding, else by
  number and title.
- AC2. In the conversation cards (Review's Comments drawer, Discuss and
  Overview's Comments), a thread whose first post is a recognized stop shows
  as one: closed, a route dot and "Stop 2 of 3" beside the author and the
  title on its own line before the narrative; open, a plate with the tour's
  numbered route marker (its route line running up to a stop before and
  down to one after), the title, "Stop 2 of 3", and Open in tour while the
  window has a published tour; then the narrative alone. Replies, ordinary
  comments and unrecognized bodies read as before. View source, Copy source
  and the edit baseline keep every byte. Plate, post and button have
  accessible names; the button takes keyboard focus.
- AC3. Open in tour opens Review's Tour tab at the stop bound to that
  comment (importing an offered tour first through the composition, as
  Start tour does); it is offered only while a published tour is in the
  window or on offer.
- AC4. Offscreen renders of the notes and the open stop in every built-in
  theme exist outside the repository.

## Design

Loaded `frontend-design:frontend-design`. The tour already has a visual
language: Review's Tour tab draws stops as numbered amber rings on a route.
The plate reuses it, in the notes' route amber (`note_colours::Route`), so a
stop in the conversation reads as a place on the same route: a quiet amber
wash and hairline, the numbered ring, the title in semibold, the badge in
secondary ink, and a dotted route line up and down from the ring when a
stop comes before or after. Picked and arriving posts keep their own amber
wash and edge on the whole post, which the plate does not imitate.
