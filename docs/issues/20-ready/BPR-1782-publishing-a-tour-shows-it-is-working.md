---
schema: "implement-rfc.card.v1"
id: "BPR-1782"
title: "Publishing a tour shows it is working, as publishing a review does"
status: "Ready"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"defect","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0041"]}
acceptance: []
---

# BPR-1782: Publishing a tour shows it is working, as publishing a review does

## Request

The owner, 2026-10-27, verbatim:

> publish tour needs to give feedback that it's working like publish a
> review does.

## Grounded current behavior (confirm when the card starts)

`review_tour_editor.cpp`: **Publish tour** (`tourPublish`) triggers
`tour_editor_action::publish_publication`; the only progress is
`tourPublicationStatus`, a placeholder-coloured label set from the
projection's status text. The button does not change while the publication
runs, and nothing near it says the tour is being sent.

## Outcome

- From the press until GitHub answers, Publish tour reads as working in the
  same way the review's Publish does (find that treatment in
  `review_submission_editor.cpp` and its composition: a working label on
  the button or a progress line, the controls that cannot be used meanwhile
  disabled), and says what it is doing in the publication's own stages
  (publishing the comment, each stop, the description link) as the projection
  reports them.
- On success it says the tour was published, with the way to see it; on
  failure, why, with the existing Resume / Check status recovery.
- Stop stays available while it runs (existing `tourStop`).
- Load `frontend-design:frontend-design` first.

## Acceptance (draft)

- A Qt test drives the projection through admitted, in-flight stages,
  success and failure, and pins the button text, enabled state and status.
- The tour publication fixture course shows the working state between press
  and terminal.
- Renders of in-flight and done; the owner's look (attended review).
