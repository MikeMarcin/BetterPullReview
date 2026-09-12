---
schema: "implement-rfc.card.v1"
id: "BPR-0737"
title: "The epoch line and the comparison menu are one full-width row under the strip"
status: "Ready"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"8409ad43-a2a6-49e5-88c9-4317f141ebe1"}
---

# The epoch line and the comparison menu are one full-width row under the strip

The owner, 2026-09-11, rejecting `BPR-0646-workbench-chrome` in the v007
attended session (decision `8409ad43`, 23:43:16Z, two screenshots:
[Overview](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-epoch-overview-2026-09-11.png),
[Read](../../verification/reviews/rfc-bpr-0028-attended-v007/captures/owner-epoch-read-2026-09-11.png)),
verbatim:

> Epoc line appears, I think there's only one epoch on this pr so it's
> hard to tell if it's functional. But the epoch labels switch position in
> every mode and that is very disorienting. I think this is because epoch
> is sharing the same space as the drawers titles, but I think epoch and
> the comparing whole PR dropdown belong on a row above above those
> headers just below the title that is full app width.

## What the code says

The reading is right. `EpochSelectors` (the epoch comparison control with
"Updates", the line and the "Comparing …" menu) is the first widget of the
centre pane's `QVBoxLayout` (`src/adapters/qt/src/workbench_window.cpp:775`,
`:834`), and the centre pane is one of the four panes of the horizontal
`workbench_central_splitter` (repositories, files, centre, conversations;
`:1258-1261`). Its horizontal extent is therefore the centre pane's width,
which `central_sizes()` (`:6300-6326`) changes with every drawer width and
modality; the drawer headings (`workbench_drawer.cpp:377-416`) sit level
with it in their own panes. In the screenshots the one node "Sep 7, latest"
sits at about 720 of 1600 in Overview and about 500 in Read.

The owner's other remark, that microsoft/terminal#20607 has one epoch so a
handle cannot be seen to change the comparison, is the same limitation
BPR-0646's criterion has carried since v006; it is addressed under
"Acceptance" below, not by a code change.

## Scope

- The epoch control moves out of the centre pane to a full-window row
  between the strip and the central splitter: "Updates" at the left, the
  line, "pair ready" and the "Comparing …" menu at the right, spanning the
  window's width regardless of which drawers are open or how wide they are.
  The drawer headings stay in their panes beneath it.
- The row is shown exactly when the epoch control is shown today (a review
  is active; hidden on Home) and keeps every gesture, the review-details
  detail region and the status wording as they are.
- RFC-BPR-0028's chrome paragraph and AC-01 already say "the strip and the
  epoch line"; amend the paragraph so the line is stated to be full width
  above the drawers, not part of the centre.
- Owned paths: `src/adapters/qt/src/workbench_window.cpp` (and the epoch
  control's parent if it must change), `tests/adapters_qt/workbench_window_tests.cpp`,
  the composed tests and UIforETW probe entries that measure
  `review_details_rect_in_window` or the epoch line's geometry, RFC-BPR-0028.
  Both application digests are repinned if `github_application.cpp` moves.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}` pin that the epoch control's
  x-extent equals the window's client width in Overview, Read, Review and
  Discuss and does not move when a drawer is toggled or resized.
- `qt_composed_github_scale_100`, `application_composition_contract` and
  the UIforETW contract pass with any digest repinned.
- The owner sees the line hold its place across the four modalities on the
  signed bundle in an attended session. To make the handle criterion of
  BPR-0646 judgeable in the same session, the packet names a public pull
  request with more than one update (microsoft/terminal#20607 has one).

## Owner answer, 2026-09-11 (mandate r9)

Asked to name a public pull request with more than one update, for
BPR-0646's handle criterion and this card's attended ask, the owner
answered, verbatim: "https://github.com/NVIDIA/stdexec/pull/1846 looks to
have multiple updates". The v008 packet names NVIDIA/stdexec#1846 for both
asks; the update count the epoch line shows on it is confirmed when the
rebuilt bundle is smoked, not assumed from the answer.
