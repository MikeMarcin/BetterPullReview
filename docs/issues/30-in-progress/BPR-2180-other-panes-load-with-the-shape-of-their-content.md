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
- AC03: The Overview's side column waits section by section: Needs
  attention, My review, Checks and People each stand in their place under
  their heading and wait in their own shape for their own part of the read
  (amended after the owner's v001 rejection).
- AC04: The private tour waits as stop cards under its head while it loads.
- AC05: Finish review holds a place for its pending comments while they are
  read, with comment rows under its "Loading pending comments…" heading.
- AC06: The status bar's line turns the pattern's mark while it says the pull
  request or its comparisons are loading.
- AC09: Once the Overview lists the files GitHub reported, Review's Files
  drawer lists them, uncovered (the owner's v001 note).
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
  `fb9faa4cfc98dcf7ec58cb025cc4746f` (state
  `/Users/jaeger/.attended-review-state/bpr-2180-loading-shapes-v001-2026-11-14`,
  served on the tailnet and listed on the review hub, cursor 15; it replaces
  the loopback-only session `8d3a3be6af32e5b2071a1de01e1b71da`, stopped and
  released with no decisions), five candidates, one Accept/Reject each: comments
  cards, Overview people, status mark, tour, Finish review. Frozen root
  `/Users/jaeger/bpr-2180-loading-shapes-v001.2g5nxy8y` from 209d124b6
  (tests-on, fixture `promo-v1` opened on its Overview, read delay 1500 ms,
  page delay 1200 ms, fixture agent only). The tour and Finish review read
  local storage and rarely stay on screen, so their candidates carry the
  course's offscreen renders from the same commit.

## AC08 verdicts, look v001

Session `fb9faa4cfc98dcf7ec58cb025cc4746f`, handed back at sequence 39
(2026-11-15T04:05:25Z), every decision by the human actor, frozen build of
209d124b6:

- `BPR-2180-comments-cards`: Accept, with the note "It looks good but
  something broke because going to review the file list should be loaded
  becuase it's already loaded on overview but now we see loading there
  instead of the file list." (decision aac1629b-21ec-4f6b-aa72-0c837d98f6a1)
- `BPR-2180-finish-review`: Accept, no note (f5dbad1d-0b12-4181-b028-d5c88a47da66).
- `BPR-2180-overview-people`: Reject, "It feels like there should be more that
  one section on the status drawer with separate loading."
  (45fc4513-a403-484f-a87b-098ff99e28df; an earlier Accept, a7e22b3d, was
  replaced by it).
- `BPR-2180-status-mark`: Accept, no note (12a65641-61a7-4807-a6ff-afcb61ceb005).
- `BPR-2180-tour`: Accept, no note (9e42d3c3-ff91-4a8e-83b0-7c7aed742541).

The Files note: while the Overview listed the files GitHub had reported
([[BPR-1842]]), Review's Files drawer was still covered by its first-load
veil. That veil asked only whether the accepted review had arrived, and the
reported files went only to the Overview. Reproduced in
`application_pending_states_public_path` (the drawer held rows under a
`loading` veil). The rejection asks for each side-column section to wait
separately in its own shape.

## v002

- The Files drawer lists the reported files once the Overview does (path rows,
  as [[BPR-0668]] builds them) and says on its plate that they open once the
  review has loaded; its veil no longer covers them.
  `application_pending_states_public_path` goes Overview, then Review, and
  fails if the Files veil covers the drawer once the Overview has the files,
  or if the drawer is empty.
- The side column waits per section (`overview_pending_veils::Merge`,
  `MyReview`, `Checks`, `People`, `Roster`); the column-wide veil is only the
  plate at its foot. `qt_pull_request_overview_page`, `qt_composed_github`
  and `application_progressive_open_public_path` assertions that sections
  stay hidden while loading now assert that they wait in place with nothing
  of the previous pull request shown.
- Look `bpr-2180-loading-shapes-v002`, attended session
  `ab148169e2487a6d3fec30a40e58d336` (state
  `/Users/jaeger/.attended-review-state/bpr-2180-loading-shapes-v002-2026-11-14`,
  served on the tailnet, cursor 6), two candidates, one Accept/Reject each:
  `BPR-2180-review-files-v002` (AC09) and `BPR-2180-side-sections-v002`
  (AC03). Frozen root `/Users/jaeger/bpr-2180-loading-shapes-v002.0zw6l_19`
  from 2ab3d74bd, same fixture and delays as v001. The v001 session's host is
  stopped and its candidates released (its verdicts stay recorded above); the
  v001 root is removed.

## AC08 verdicts, look v002

Session `ab148169e2487a6d3fec30a40e58d336`, handed back at sequence 19
(2026-11-15T06:23:48Z), both decisions by the human actor, frozen build of
2ab3d74bd:

- `BPR-2180-side-sections-v002`: Accept, no note
  (be5e216a-f5d1-4224-82b8-49c60fed784a). AC03 is accepted.
- `BPR-2180-review-files-v002`: Reject, "The irony is the initial thing I
  asked you to replicate, the main diff file pane, no longer shows the
  loading." (7bd9a238-c83a-4f0b-9d43-0992be9f6a62)

The cause: v002 listed the reported files in the Files tree only. A click on
one looked the file up in the drawer list, which never received them, so the
click was dropped: no file was opened, and the comparison pane never said it
was loading one ([[BPR-1871]]'s code veil). Before v002 the drawer was covered
while the read ran, so nothing could be clicked. No course clicked a file in
Review while a first read ran: `application_pending_states_public_path`
checked only the drawer's rows and veil, and the comparison veil's courses
(`qt_pending_veil`, `qt_diff_workbench`) drive the comparison alone.

## v003

- Architecture review N2: the read lane (`core::github_window_read`) decides
  what a first read has shown, part by part (`github_loading_readiness`); the
  read host publishes it with every transition and admitted observation, and
  publishes the reported files as provisional file rows (identities minted in
  the host), withdrawing them when a read ends with no review. Qt maps the
  readiness to each pane's wait and shows the rows through the ordinary
  file-row path.
- A click on a reported file is remembered, said on the Files plate, and
  opened once the review lists it; from the click on the comparison pane
  waits for it in the shape of code.
- Architecture review N6: `status_line()` returns its words and whether they
  say a load, and one `present_status_line()` sets both.
- Courses: `core_github_window_read` (readiness through open, observations,
  failure and cancellation); `qt_pending_veil` (reported files are the
  drawer's rows, a click is remembered and the comparison waits as code, a
  read with no review withdraws them; the status mark agrees with its
  words); `application_pending_states_public_path` (Overview then Review
  under a read delay: the drawer lists the files uncovered, a click shows the
  code veil without a gap until the file arrives, and a superseded read that
  fails leaves only the shown review's files).
- Look `bpr-2180-loading-shapes-v003`, attended session
  `856f3dec1b0b67a634a60a0ce3e586e0` (state
  `/Users/jaeger/.attended-review-state/bpr-2180-loading-shapes-v003-2026-11-14`,
  served on the tailnet, cursor 6), two candidates, one Accept/Reject each:
  `BPR-2180-diff-skeleton-v003` and `BPR-2180-review-files-v003` (AC09).
  Frozen root `/Users/jaeger/bpr-2180-loading-shapes-v003.1rcim4tx` from
  1887d3281, same fixture and delays as v001 and v002. The v002 host is
  stopped, its candidates released, its root removed.
