---
schema: "implement-rfc.card.v1"
id: "BPR-1776"
title: "Tour stop actions look like labels, and the stop's More button is awkward"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1776--BPR-1776--a6905583b2f2.json","kind":"defect","found_by":"owner photograph and chat, 2026-10-26: the tour editor's stop card in dark mode","related_cards":["BPR-1593","BPR-1090"]}
acceptance: [{"criterion": "renders", "outcome": "pass", "revision": "a45cbce8654e24d4afc4def0c9aefaa0df31fb4a", "evidence": {"kind": "command", "detail": "Offscreen tour-route renders before (73e90cbd0: bare Enhance text, Use selected lines, Show in code, Suggest an outline; '⋯' with Qt's arrow) and after (raised chips; one More glyph), light, default drawer width. Dark and narrowest width not rendered."}}, {"criterion": "kinds", "outcome": "pass", "revision": "a45cbce8654e24d4afc4def0c9aefaa0df31fb4a", "evidence": {"kind": "command", "detail": "qt_workbench_window_scale_100 pins each stop action as a non-flat quiet_push_button and More as an icon with no menu, no instant popup and no text that opens tourStopMoreMenu on a press; tour composition, publication (native, stop), scoped prose editor and drawer fit tests pass."}}, {"criterion": "look", "outcome": "pass", "revision": "aaa24c11c865068cc24b918dd9e4e7f7d61b2c99", "evidence": {"kind": "observation", "detail": "Attended review session 41f51349bc37e6c092d31dbf1739c5b1 (state ~/.attended-review-state/bpr-looks-2026-10-27, served over Tailscale, handed back 2026-10-27T21:11Z): candidate BPR-1776-look, owner verdict accept."}}]
---

# BPR-1776: Tour stop actions look like labels, and the stop's More button is awkward

## Observed

The owner, 2026-10-26, with a photograph of a tour stop being written
(dark appearance), verbatim:

> These tour buttons for enhanced text and showing code don't even appear to
> be buttons. They look more like labels. And then this thing that is a
> button with the ellipsis and the Chevron looks very awkward.

The photograph shows, under the stop's explanation field: **Enhance text**
followed by a grey square holding "⋯" and a down chevron, then **Show in
code** on the next row beside the blue primary button; under the card,
**Add stop** (outlined, reads as a button) and **Suggest an outline**
(plain text).

## Cause (observed at `73e90cbd0`)

`src/adapters/qt/src/review_tour_editor.cpp`:

- **Enhance text** (`add_writing_button`, object `tourStopMagic`, and the
  problem and solution fields' `tourProblemMagic` / `tourSolutionMagic`) is a
  `quiet_push_button` with `setFlat(true)`: white text with no frame, fill or
  hover cue, the same as a label beside it. Its words change with the
  selection (Enhance selection, Generate comment, Recover explanation).
- **Show in code** (`tourReturn`) is a flat button made the same way.
- **Suggest an outline** appears to be the same kind; confirm.
- The **More** button (`tourStopMore`, [[BPR-1593]]) is a `QToolButton`
  whose text is "⋯" with `InstantPopup`, so Qt adds its own menu-indicator
  arrow beside the ellipsis and draws the auto-raised button as a grey
  square: two "more" signs in one control.

## Outcome

- Every stop action reads as something to press, in light and dark, at the
  drawer's narrowest width, using the drawer kit's existing button kinds
  (RFC-BPR-0085, the shared action buttons of RFC-BPR-0087) rather than a
  new style: a secondary or quiet-with-affordance kind for Enhance text, Show
  in code and Suggest an outline, beside the one primary action.
- More is one sign: the ellipsis without Qt's arrow (or the kit's own more
  glyph), the same size and hover as the other drawer tools.
- Keyboard focus is visible on each; accessible names unchanged.
- UX work: load `frontend-design:frontend-design` first and say so.

## Acceptance (draft)

- Offscreen renders of the stop card, light and dark, before and after, at
  the default and narrowest drawer widths.
- A Qt test pins each control's kind (not flat; More has no menu indicator).
- The owner accepts the look on a frozen build (attended review, one
  question for the buttons, one for More).

## Done and pending

Done at the recorded revision: every stop action listed under Cause is the
raised shared chip; More is the conversation cards' More glyph, its menu
opened under it on a press, its glyph redrawn on a palette change.

Pending: the owner's look on a frozen build (attended review), dark and
narrowest-width renders. Left as they were, outside the request: the stop
header's Previous and Next, and the publication row's Check status and Stop,
which are flat too.
