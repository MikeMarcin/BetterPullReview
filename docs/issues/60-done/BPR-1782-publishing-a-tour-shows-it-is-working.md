---
schema: "implement-rfc.card.v1"
id: "BPR-1782"
title: "Publishing a tour shows it is working, as publishing a review does"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1782--BPR-1784--53c6249f60af.json","kind":"defect","found_by":"owner chat, 2026-10-27","related_rfcs":["RFC-BPR-0039","RFC-BPR-0041"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "a472f01ec7002f053b613961c5bd60466e3e3b76", "evidence": {"kind": "command", "detail": "qt_review_tour_publication_working drives review_tour_editor::present_publication through admitted, publishing, stopping, published and attention and pins Publish's text (Publish tour / Publishing… / Published), enabled state of Publish, Preview, mode, description, Stop, Resume and Check status, the waiting panel's title, stages and status, the status line and the On GitHub link. Passes."}}, {"criterion": "AC2", "outcome": "pass", "revision": "a472f01ec7002f053b613961c5bd60466e3e3b76", "evidence": {"kind": "command", "detail": "All 9 application_review_tour_publication_* courses and application_review_tour_cli_ordinary_{native,stop} pass. Right after the press each course checks Publishing…, Publish and Preview disabled, Stop enabled and the panel's stages; stop also checks this while the fixture holds the first staged write, and Resume; native, compat and resumed stop check Published plus the returned link; conflict and description check the attention panel with Check status."}}, {"criterion": "AC3", "outcome": "pass", "revision": "a472f01ec7002f053b613961c5bd60466e3e3b76", "evidence": {"kind": "artifact", "detail": "Offscreen renders of in-flight and done, from the stop course (whole app, tour page) and the Qt test (editor), are saved outside the repository in the session scratchpad, bpr-1782/app-stop-in-flight.png, app-stop-done.png, qt-editor-in-flight.png, qt-editor-done.png, qt-editor-failed.png."}}, {"criterion": "AC4", "outcome": "pass", "revision": "aaa24c11c865068cc24b918dd9e4e7f7d61b2c99", "evidence": {"kind": "observation", "detail": "Attended review session 41f51349bc37e6c092d31dbf1739c5b1 (state ~/.attended-review-state/bpr-looks-2026-10-27, served over Tailscale, handed back 2026-10-27T21:11Z): candidate BPR-1782-look, owner verdict accept."}}]
---

# BPR-1782: Publishing a tour shows it is working, as publishing a review does

## Request

The owner, 2026-10-27, verbatim:

> publish tour needs to give feedback that it's working like publish a
> review does.

## Grounded current behavior (confirmed)

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

## Acceptance

- A Qt test drives the projection through admitted, in-flight stages,
  success and failure, and pins the button text, enabled state and status.
- The tour publication fixture course shows the working state between press
  and terminal.
- Renders of in-flight and done; the owner's look (attended review).

## Built

- **Application.** `tour_publication_presentation` gains `Phase` (idle,
  publishing, published, attention), `Stages` and `Link`.
  `staged_publication_stages(plan)` names what the captured plan sends, in
  order: the tour comment (or the index comment, or the update), each stop
  by number and title, then the description link. `staged_published_link`
  gives the review's, index comment's or updated root's returned address.
- **Composition** (`review_tour_publication_workflow`). Publish and Resume
  mark the active effect as publishing. A publish answered without
  publication is shown as attention until the next fresh preview. Its
  in-flight status says what is safe ("Stop keeps what GitHub already has.
  Nothing will be sent twice."). Grant, admission, host call, cancellation
  and terminal handling are unchanged. The round-trip composition digest is
  resealed.
- **Qt.** Compose review's waiting panel (`waiting_frame`, its mark,
  attention colour and quiet text) moves to `waiting_panel.hpp/.cpp` and is
  shared. The tour's On GitHub panel shows it while publishing: the title
  "Publishing the tour to GitHub…", "Sending, in order:" and the stages,
  then the status. Publish reads "Publishing…". Publish, Preview, the mode
  and the description choice are disabled, and Stop stays. On success,
  Publish reads "Published", and the status line plus "On GitHub: <url>"
  says where it is. On failure, the panel reads "Publishing didn't finish"
  with the reason, above Resume and Check status. Widgets perform no
  effects. `frontend-design:frontend-design` was loaded first.

## Not verified

- The running publication reports only its terminal, so the panel names
  the plan's stages but not which one is under way. A live per-stage
  marker needs a progress observation from the host through the completion
  lane; that work is not in this card.
- While stopping, the panel keeps the "Publishing…" title, and the status
  inside it says "Stopping."
- The published address is shown as selectable text, not a link. Opening
  it would need a composed open-link effect.
- The owner's look on a real display, light and dark (deferred, pending).
- No live GitHub run; fixtures only. Windows and Linux were not run.

