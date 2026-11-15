---
schema: "implement-rfc.card.v1"
id: "BPR-2180"
title: "Other panes load with the shape of their content"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-11-14","owner_note":"Hey can we do more graphics like the main pane loading for other parts that are loading in?","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-1576","BPR-1842","BPR-2151"],"reservation":"docs/issues/_reservations/BPR-2180--BPR-2180--cfc3f23d5086.json"}
---

# BPR-2180: Other panes load with the shape of their content

The owner asked on 2026-11-14: "Hey can we do more graphics like the main
pane loading for other parts that are loading in?" Their screenshot showed the
comparison pane's code skeleton beside a Comments drawer that said its wait in
words alone.

[[BPR-1576]] made one loading-state pattern, `qt::pending_veil`: placeholders
in the content's own shape, then the turning mark and words. The comparison
pane draws code; the Comments drawer and Threads draw plain bars; the
Overview's people, the private tour and Finish review's pending comments say
"Loading…" in text only, and the status bar says "Preparing file
comparisons…" without the mark the Updates line has.

Governing design: the review workbench of
[RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md) and the
loading-state pattern recorded in the
[workbench specification](../../ux/workbench-specification.md). This extends
the accepted pattern; it adds no shimmer and no new motion.

## Acceptance

- AC01: The pattern gains a `cards` shape (a thread card: feedback rail, face,
  name and age, two lines of text) and a `people` shape (faces in a row where
  the place is one line high, a roster of face and name where it is taller),
  drawn from the same tint, theme, high-contrast and reduced-motion rules as
  the other shapes. A pane whose wait a heading beside it already says can
  draw its placeholders in the words' place.
- AC02: The Comments drawer (and Discuss, which shows the same panel) and
  Threads wait as thread cards.
- AC03: The Overview's People section waits as faces while reviewers are still
  coming, after the title and files have shown.
- AC04: The private tour waits as stop cards under its head while it loads.
- AC05: Finish review holds a place for its pending comments while they are
  read, with comment rows under its "Loading pending comments…" heading.
- AC06: The status bar's line turns the pattern's mark while it says the pull
  request or its comparisons are loading.
- AC07: Qt and composed courses cover AC01 to AC06, with renders in Light and
  Dark; format, tidy and documentation gates pass.
- AC08 (human): The owner accepts a frozen build opening a slow pull request,
  one Accept/Reject per surface group.

The Agent tab's load is deferred: its view belongs to [[BPR-2151]] while that
card is open.

## Implementation

`qt::pending_veil` gains `pending_shape::cards`, `pending_shape::people`,
`set_words_beside` and `find`; `qt::pending_mark` gains `lead(QLabel&)`, which
keeps the mark at the start of a label's line. The window's Comments and
Threads veils take `cards`; the Overview adds `People` and `Roster` veils and
shows the People section in its place while the people are the part still
coming (before, the section stayed hidden until they arrived, so its
"Loading participants…" was never on screen); the tour editor lays a `cards`
veil over its route; Finish review shows its pending place with three rows,
words beside; the status bar's line carries a `pending_mark` and is refreshed
whenever readiness is decided, so it no longer keeps a stale sentence while a
read runs. Discuss shows the Comments panel itself, so it shares that veil.

## Progress

- 180b79fab, 209d124b6: implementation, courses and the UX specification.
- `qt_pending_veil`, `application_pending_states_public_path`,
  `qt_pull_request_overview_page`, `qt_workbench_window_scale_100`,
  `qt_review_submission_editor_scale_100/125/200`,
  `qt_refresh_feedback_window`, `qt_drawer_surfaces_fit_scale_100`,
  `qt_review_tour_steps`, `qt_review_tour_publication_working` and
  `application_start_review_contract` pass on the lane's dev build; format
  check and clang-tidy clean on the changed translation units (two tidy
  findings in `review_tour_editor.cpp:2575` and
  `workbench_window_defects_and_home_rows.cpp:547` predate this card);
  documentation gate passes.
- AC08 pending: look `bpr-2180-loading-shapes-v001`, attended session
  `8d3a3be6af32e5b2071a1de01e1b71da` (state
  `/Users/jaeger/.mask/attended-review/bpr-2180-loading-shapes-v001`, served on
  loopback, cursor 15), five candidates, one Accept/Reject each: comments
  cards, Overview people, status mark, tour, Finish review. Frozen root
  `/Users/jaeger/bpr-2180-loading-shapes-v001.2g5nxy8y` from 209d124b6
  (tests-on, fixture `promo-v1` opened on its Overview, read delay 1500 ms,
  page delay 1200 ms, fixture agent only). The tour and Finish review read
  local storage and rarely stay on screen, so their candidates carry the
  course's offscreen renders from the same commit.
