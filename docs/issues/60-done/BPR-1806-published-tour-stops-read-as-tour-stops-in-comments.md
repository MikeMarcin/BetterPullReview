---
schema: "implement-rfc.card.v1"
id: "BPR-1806"
title: "Published tour stops read as tour stops in comments"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1803--BPR-1806--63af5d42316f.json","kind":"ux","found_by":"owner, 2026-10-28","related_cards":["BPR-1086","BPR-1222","BPR-1785","BPR-1781","BPR-1801","BPR-1802"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "6d7d70db60240b153d4017ab8d19389d3170d69b", "evidence": {"kind": "command", "detail": "core_review_tour_markdown published_stops: in LF and CRLF a middle stop reads back number 6, unescaped title with backticks and brackets, the exact narrative, pinned source span and Previous / Tour overview / Next destinations; a first stop without a source, a stop before BPR-1785's note, an overview-only stop and the legacy heading with trailing blank lines are stops; 23 other bodies are refused (ordinary, empty, no navigation, no or blank narrative, prose before, two sources, leading zero, zero, 257, non-decimal, empty title, hyphen, level two, missing blank lines, text after, wrong order or label, first stop with Previous, fenced, quoted, unclosed fence) and an oversized body is not read. Counts: a whole tour of three and a tour of one counted in order; a missing stop, a repeated number and a tour still being published stay unknown. Finder: by binding whatever the number, else by number and title; a renamed title or a conversation-comment prefix against a line binding finds nothing."}}, {"criterion": "AC2", "outcome": "pass", "revision": "6d7d70db60240b153d4017ab8d19389d3170d69b", "evidence": {"kind": "command", "detail": "qt_tour_stop_card: three published stops read as Stop 1/2/3 of 3 with unchanged bodies; the reply, an ordinary comment and a heading-only body are not stops. Open, stop 2 has its plate (named 'Tour stop: Why the width is clamped', described 'Stop 2 of 3'; post named 'octocat, Stop 2 of 3: ...'), renders only its narrative (no PR Tour, Tour overview, Previous, source path or note), View source and Copy source equal the published bytes, the reply reads as written, ordinary and broken bodies have no plate. qt_feedback_label_threads (BPR-1086 labelled stops), qt_discuss_comment_bodies, qt_workbench_overview, architecture_boundaries and 75 conversation/tour/feedback/markdown tests pass."}}, {"criterion": "AC3", "outcome": "pass", "revision": "6d7d70db60240b153d4017ab8d19389d3170d69b", "evidence": {"kind": "command", "detail": "qt_tour_stop_card: Open in tour appears only once offered with a dispatch, is named 'Open stop 2 in the tour', takes strong focus, and its click dispatches ('review-comment:stop-2', stop 2); withdrawn it goes. qt_workbench_overview: an unpublished draft offers nothing, a published tour in the window offers Open in tour, start_tour_at_published_stop opens Review's Tour tab at the stop bound to 'review-comment:PRRC_how' though its GitHub title differs, and an empty presentation withdraws the offer. The composition routes it through start_published_tour, importing an offered tour first as Start tour does (built; no separate composed course)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "6d7d70db60240b153d4017ab8d19389d3170d69b", "evidence": {"kind": "artifact", "detail": "With BPR_TOUR_STOP_RENDER_DIR set, qt_tour_stop_card writes tour-stop-notes-, tour-stop-open- and tour-stop-plate-{light,dark,high-contrast-light,high-contrast-dark}.png and checks the route marker's amber in each theme; copies kept outside the repository in the session scratchpad (tour-stop-card-renders). The owner's own look is not part of this card's acceptance."}}]
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

## Built

- `core::recognize_published_tour_stop`, `published_tour_stop_counts` and
  `find_published_tour_stop` in `review_tour_markdown`. Recognition is the
  exact inverse of `render_tour_stop`; there is no separate marker in the
  published body, so the envelope itself is the marker.
- `markdown_body_view::set_rendered_range`: the reader renders the
  narrative's byte range while source, copy and `original_body` keep the
  whole body; a labelled narrative's header still sets in semibold.
- `card_post::TourStop` and `TourStopCount`, filled for a thread's first post
  only; the closed note's badge and title line; `tour_stop_plate` in the open
  card; `conversation_cards_view::set_tour_open_offered` and
  `set_tour_open_dispatch`; `workbench_window::start_tour_at_published_stop`
  and `set_tour_stop_dispatch`; the composition's `start_published_tour` at a
  stop.
- Not changed: author labels ([[BPR-1803]]), the Tour tab, and permalink
  rendering ([[BPR-1802]]). The pinned source is hidden with the rest of the
  scaffolding; the plate does not yet show it.

## Owner review

Attended screenshot review session 00fef88b0f9a8d6133fc06b452010a90 (state /Users/jaeger/bpr-attended/tour-cards-v001/state, closed 2026-10-29) at efcaafa33: closed-note look accepted; open-thread plate rejected — keep tour previous/next navigation in a compact, polished form. Follow-up BPR-1810.
