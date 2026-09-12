---
schema: "implement-rfc.card.v1"
id: "BPR-0737"
title: "The epoch line and the comparison menu are one full-width row under the strip"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"8409ad43-a2a6-49e5-88c9-4317f141ebe1"}
acceptance: [{"criterion":"qt_workbench_window_scale_{100,125,200} pin that the epoch control's x-extent equals the window's client width in Overview, Read, Review and Discuss and does not move when a drawer is toggled or resized","outcome":"pass","revision":"4ebb0009","evidence":{"kind":"command","detail":"test_the_epoch_row_spans_the_window_in_every_modality: the row's window rectangle equals the client area's left, top and width, sits above the splitter, and is identical through the four modalities, both drawers opened and collapsed, and a 120-pixel drawer resize; passes at the three scales at 4ebb0009."}},{"criterion":"qt_composed_github_scale_100, application_composition_contract and the UIforETW contract pass with any digest repinned","outcome":"pass","revision":"4ebb0009","evidence":{"kind":"command","detail":"11 of 11: qt_workbench_window_scale_{100,125,200}, qt_epoch_comparison_scale_{100,125,200}, qt_composed_github_scale_100, application_uiforetw_selected_diff_contract, application_composition_contract, architecture boundaries at 4ebb0009; github_application.cpp unchanged by this slice, no digest moved."}},{"criterion":"The owner sees the line hold its place across the four modalities on the signed bundle in an attended session, on a public pull request with more than one update","outcome":"pass","revision":"35438244","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0737-epoch-row, revision app-release-e4fdb0ea-2026-09-11.4 (bundle built at 35438244, executable sha256 e4fdb0ea8e250f42da2bfa3a61c501fbdbb6ffd6d92d929912f6b25b44b2757f), fingerprint 85bf31e19884fae76278548ab576187170631e3f45e474f018128d417e7101f9, decision 34c79345-1d3a-4db6-811b-340869d3e7b7, actor human, 2026-09-12T04:33:10Z, no note, no attachment: accept. The candidate's criterion as reviewed was the row's placement and that it holds through the four modalities and every drawer gesture, which is the v007 BPR-0646 finding closed. Limitation, recorded: the bundle showed one update on NVIDIA/stdexec#1846 because a first open yields one epoch (BPR-0743), so the 'more than one update' clause of this criterion's wording was not met; that clause was preparation for BPR-0646's handle criterion and does not bear on where the row sits. The owner's three captures in this round show the row spanning the client width above both panes and the drawer."}}]
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

## Notes

- Slice, 2026-09-11 (d16). The central stack's review page is now a
  `workbenchReviewSurface` widget: a zero-margin column holding the epoch
  comparison control as its first row and the central splitter beneath it,
  so the row is as wide as the client area whatever the drawers do, and
  Home still replaces the whole surface (the control's visibility rules,
  gestures, details region and status wording are untouched; only its
  parent changed). The centre pane keeps the overview/comparison stack.
  `central_sizes()` still measures the splitter, which is unchanged in
  width. The epoch control's parent was never named by the app or the
  probes: `review_details_rect_in_window` measures the control itself.
- `qt_workbench_window_scale_{100,125,200}` gain
  `test_the_epoch_row_spans_the_window_in_every_modality`: the row's
  rectangle in window coordinates equals the client area's left, top and
  width, sits above the splitter, and is identical through Overview, Read,
  Review and Discuss, through both drawers opened and collapsed, and after
  a 120-pixel drawer resize; the composed-placement pin now names the
  review surface and the splitter's parent.
- RFC-BPR-0028's chrome paragraph now states the row is full width above
  the drawers, never inside the centre. `github_application.cpp` did not
  move, so no digest changed for this slice.
- Verification at this head: `cmake --build --preset dev` clean; 11 of 11
  in `qt_workbench_window_scale_{100,125,200}`,
  `qt_epoch_comparison_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_uiforetw_selected_diff_contract`,
  `application_composition_contract` and the architecture boundary tests;
  strict tidy and `clang-format --Werror` clean on the touched files.
- Found red at HEAD, not this slice: `application_profile_window_layout_public_path`
  fails "exact public review opens" with the BPR-0737 edits stashed as
  well (it last changed at ad3abf50, BPR-0708; the Home open path it
  drives changed under BPR-0732 and BPR-0733). Carded separately.

## Attended candidate, 2026-09-11 (v008)

The pending criterion is candidate `BPR-0737-epoch-row` in
[rfc-bpr-0028-attended-v008](../../verification/reviews/rfc-bpr-0028-attended-v008/operator-packet.md)
on the bundle built from the source at 35438244 (executable sha256
`e4fdb0ea…`, tests disabled, codesign clean), candidate revision
`app-release-e4fdb0ea-2026-09-11.4`; prepared and protected at sequence
341, undecided, no host running until the owner says so. The pull request
is NVIDIA/stdexec#1846, the owner's answer of mandate r9.

## Attended verdict, 2026-09-11 (v008): accepted

Session `cbd48b91…`, revision `app-release-e4fdb0ea-2026-09-11.4` (bundle
built at 35438244, executable sha256 `e4fdb0ea…`), actor human; handed back
at sequence 348 with all six candidates decided. Candidate
`BPR-0737-epoch-row`, decision `34c79345`, 2026-09-12T04:33:10Z, no note and
no attachment: **accept**.

The candidate's criterion, as reviewed: "The epoch line and the Comparing
menu are one row the full width of the window directly under the strip, above
the drawers, and hold their place through Overview, Read, Review and Discuss
and through opening, collapsing and resizing either drawer." That is the
v007 finding on [BPR-0646](../10-backlog/BPR-0646-fixed-workbench-header-with-sign-in-and-settings-window.md)
closed: the row now belongs to the review surface above the splitter, not to
the centre pane.

One limitation, recorded rather than glossed. This card's AC3 says "on a
public pull request with more than one update"; the bundle showed one update
on NVIDIA/stdexec#1846, because a first open yields one epoch
([BPR-0743](BPR-0743-the-epoch-line-shows-only-the-updates-this-install-has-observed.md)).
The multi-update condition was preparation for BPR-0646's handle criterion
and carries no weight here: what this criterion observes is where the row
sits and whether it holds through the four modalities and the drawer
gestures, which the owner exercised and accepted. The owner's own three
captures in this round, taken in Read on that pull request, show the row
spanning the client width above both panes and the drawer.

RFC-BPR-0028's chrome paragraph already carries the amendment this card
made. AC-01 stays BPR-0646's.
