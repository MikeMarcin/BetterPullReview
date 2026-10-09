---
schema: "implement-rfc.card.v1"
id: "BPR-1364"
title: "A single click on a Home row selects it; opening takes a double-click or Return"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","found_by":"Owner, BPR-1357 attended review, 2026-10-09","reservation":"docs/issues/_reservations/BPR-1364--BPR-1364--15f2a5b9a853.json","related_cards":["BPR-1357","BPR-1342","BPR-1338","BPR-0776","BPR-0736","BPR-1157"]}
acceptance: [{"criterion":"BPR-1364:select","outcome":"pass","revision":"57a08a877","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"qt_home_list_scale_{100,125,200} test_a_single_click_selects_and_a_double_click_opens (four arrangements, platform style and single-click-activation style) and qt_composed_github_scale_* test_home_side_lists_select_on_a_single_click (Recently viewed, Review Cycles): a single click makes the row current, focused and selected and opens nothing."}},{"criterion":"BPR-1364:open","outcome":"pass","revision":"57a08a877","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"qt_home_list_scale_*, qt_composed_github_scale_*, qt_workbench_open_review_from_home_scale_*: a double-click opens once, Return opens the current row, the row menu's Open Review opens it; Review Cycles opens on Return."}},{"criterion":"BPR-1364:same-turn","outcome":"pass","revision":"57a08a877","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"application_home_open_feedback_public_path (3 s fixture delay, light and dark on Home): a single click on #17 asks GitHub for nothing; a double-click says 'Opening #17...' with no event-loop turn after it; Return the same."}},{"criterion":"BPR-1364:accessible","outcome":"pass","revision":"57a08a877","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"qt_home_list_scale_* and qt_composed_github_scale_*: every Home list's accessible description says 'Double-click or press Return to open'."}},{"criterion":"BPR-1364:failing-before","outcome":"pass","revision":"57a08a877","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"With the source change set aside and the tests kept, qt_home_list_scale_100, qt_composed_github_scale_100, application_home_open_feedback_public_path and application_review_cycle_desktop_public_path fail; restored, they pass."}},{"criterion":"BPR-1364:owner","outcome":"pass","revision":"57a08a877","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1364-a-single-click-on-a-home-row-selects-it-and-opening-takes-a-double-click-or-return.md","detail":"Owner Accept, 2026-10-09T14:00:34Z: session a6f8821efdd9cd5b947d000e9eef319c, candidate BPR-1364-single-click-selects-v001 (frozen 57a08a877, fingerprint 91948414e63d...), decision fa5709d1-e47f-4d14-9bf4-1aa70df7086b at event 4, handed back at 5, released at 6. Note: 'Separate problem: the color of recent is the same as the color of selected.' -> BPR-1365."}}]
---

# BPR-1364: A single click on a Home row selects it; opening takes a double-click or Return

## Owner notes

Owner, BPR-1357 attended review (accepted), 2026-10-09, verbatim: "feedback:
I don't like a single click on home card opening the pr."

## Scope

A single click on any pull-request row or card on Home only selects it: the
row becomes current, takes focus and is highlighted. Opening takes a
double-click, Return, or the row menu's Open Review. BPR-1357's "Opening #N…"
still appears in the same turn as the opening gesture.

Every Home list is in scope: the queue in each arrangement (state panels, the
flat card, group panels and their state bands, which personal discovery's rows
also fill), Recently viewed, and the Review Cycles list beside them.

## Acceptance

- `BPR-1364:select`: a single click on a row of every Home list makes it
  current, focused and selected, and opens nothing, under the platform's own
  style and under a style that asks for single-click activation (as KDE does).
- `BPR-1364:open`: a double-click on a row opens it once; Return opens the
  current row; the row menu's Open Review opens it.
- `BPR-1364:same-turn`: through the composed application with the fixture
  delay, a double-click on `#17` says "Opening #17…" on that row with no
  event-loop turn after it, and a single click before it asked for nothing.
- `BPR-1364:accessible`: every Home list's accessible description says
  "Double-click or press Return to open".
- `BPR-1364:failing-before`: the new tests fail against the code before the
  change.
- `BPR-1364:owner`: the owner accepts single-click selection on a frozen
  build (one Accept/Reject question).

## What existed (inventory at `9df9fc891`)

- **Queue rows** (every arrangement: state panels, the one card, group panels
  and their state bands; personal discovery fills the same rows):
  `home_list::adopt_rows` connected both `clicked` and `activated` to
  `open_row`, so a single click opened the pull request. Return was already
  read by the list itself ([[BPR-0736]]).
- **Recently viewed** opened on `itemActivated`: a double-click on macOS and
  Windows, but a single click wherever the style asks for single-click
  activation (KDE). Return and ⇧Return were read by the window.
- **Review Cycles** resumed on `itemActivated` too, and on macOS Return is an
  item view's editing key, so the keyboard had no way to resume a cycle.
- **What depended on a single click:** `qt_workbench_open_review_from_home`
  ("a plain click still opens the row here"),
  `qt_composed_github`'s Home row course ("clicking a row opens its pull
  request") and the BPR-1357 course's clicks. Nothing in the product did.
  The Repositories drawer (not Home) opens on a current-row change and is
  unchanged.

## Implementation

The `frontend-design:frontend-design` skill was loaded first. This is an
interaction change to a Qt surface with no new visual design: the gesture
names stay the same everywhere ("Open Review", Return, double-click), and the
selection and focus painting are the style's own.

- `home_list::adopt_rows`: `doubleClicked` opens; neither `clicked` nor
  `activated` is taken. Each rows view's accessible description is
  `home_open_description()`, "Double-click or press Return to open".
- `workbench_window`: Recently viewed opens on `itemDoubleClicked` and says
  "Double-click or press Return to open; Shift and Return resume its saved
  reading". Review Cycles resumes on `itemDoubleClicked` or Return (read in
  `eventFilter`, `resume_home_cycle`), with the same description.
- **A crash the change exposed.** A single click now leaves the keyboard in a
  group's rows, so changing Group retired a focused list. `~QWidget` sends
  that list's focus-out after its item-view part is gone, and the row button's
  filter (`home_row_action::eventFilter`) asked it for its rows (EXC_BAD_ACCESS
  in `home_row_action::refresh`). The filter now ignores a list that is no
  longer an item view. `retire_groups` moves the keyboard off a panel before
  deleting it. It also rescues BPR-1357's Cancel/Retry button, which lives in
  the open row's list and would have been deleted with it, and places it
  again. `set_grouping` and `set_state_headers` keep the keyboard on the same
  pull request's row.
- **A course bug found on the way.** `application_home_open_feedback_public_path`
  triggered the checkable Home action at the start of each appearance; in the
  second (dark) pass that toggled Home off and showed the review already open
  behind it, so the dark pass never ran on Home. It now uses
  `choose_home(true)` and asserts Home.
- Specification: "Opening a row" and "While a row opens" in
  `docs/ux/workbench-specification.md`.

## Verification (macOS, `build/dev` Debug, offscreen, 2026-10-09)

- `qt_home_list_scale_{100,125,200}`: new
  `test_a_single_click_selects_and_a_double_click_opens`. In each of the four
  arrangements a window-system single click on `#101` makes it current,
  highlighted and focused and opens nothing; a double-click opens it once;
  Return opens it; the description is the one above. With the keyboard on a
  group's row carrying Cancel, None and then State headers keep the keyboard
  on `#101`, and Cancel survives and moves to the new list. Under a style that
  makes a single click an activation (`activated` fires once), the click
  still only selects and a double-click opens.
- `qt_composed_github_scale_{100,125,200}`: the Home row course now asserts a
  single click opens nothing and selects and focuses the row (Open Review
  enabled); a double-click opens `#73`; the row menu's Open Review opens it.
  The Recently viewed course asserts a single click selects and opens nothing.
  New `test_home_side_lists_select_on_a_single_click`: Recently viewed and
  Review Cycles, under the platform style and the single-click-activation
  style, select on a single click, open on a double-click, and Review Cycles
  opens on Return.
- `qt_workbench_open_review_from_home_scale_{100,125,200}`: a plain click
  selects and opens nothing, and a double-click opens here, on a queue row
  and a Recently viewed row.
- `application_home_open_feedback_public_path` (3 s fixture delay, light then
  dark, both on Home): a single click on `#17` asks GitHub for nothing and
  stays on Home. A double-click says "Opening #17…" on that row with no
  event-loop turn after it. A second double-click is the same request.
  `#4001`'s failure and Retry, the bar and the status band are unchanged.
  Return on `#17` says "Opening #17…" in the same turn, then the pull request
  opens.
- `application_review_cycle_desktop_public_path` and
  `profile_cycle_handoff_public_path_tests.cpp` drive Home's cycles by
  `itemDoubleClicked`; `application_review_cycle_publish_public_path` (real
  window-system double-clicks) passes unchanged.
- Also passing: `qt_workbench_window_scale_100`,
  `qt_workbench_open_on_github_scale_100`, `qt_composed_home_refusal_scale_100`,
  `qt_surface_capture_course_scale_100`, `qt_anchored_tooltips`,
  `application_setup_destination_public_path`,
  `application_attended_recent_replay`,
  `application_profile_multi_window_public_path`,
  `application_personal_discovery_public_path`,
  `application_profile_window_review_restoration_public_path`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` and
  `application_workbench_composition_contract` (no pin moved:
  `github_application.cpp` is unchanged), `architecture_boundaries`.
  clang-format 22.1.8 on every changed source; clang-tidy 22 on
  `home_list.cpp` clean.

**Failing-before.** With the source change set aside and the tests kept,
`qt_home_list_scale_100` (no description), `qt_composed_github_scale_100`
("a single click on a row opens nothing"),
`application_home_open_feedback_public_path` ("a single click on a row asks
GitHub for nothing and stays on Home") and
`application_review_cycle_desktop_public_path` ("resumed from Home") all fail;
with it restored they pass.

## Owner look — prepared 2026-10-09

- Landed on main as `57a08a877`. Frozen build of `57a08a877` with
  `scripts/freeze_attended_build.py freeze` (source export and tests-on build
  inside `/Users/jaeger/.mask/attended-review/bpr1364-home-single-click-v001.qsxvzbe2`;
  `better_pull_review` SHA-256
  `6ccffa367e91c916902c764f8e72ff691399ab9a6ab899e1a5f4ddacc7c201ec`), the
  BPR-1357 launcher pattern: fixture `github-com`, `--attended-home-look
  --fixture-read-delay-ms 4000 --fixture-read-fail-number 4001`. The
  launcher's own offscreen check reached
  `ATTENDED_HOME_SHOWN:rows=2:delay=4000ms`. Started with
  `python3 /Users/jaeger/.mask/attended-review/bpr1364-home-single-click-v001.qsxvzbe2/launch.py`.
- New short session `a6f8821efdd9cd5b947d000e9eef319c`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1364-home-single-click-v001/state`,
  host detached. One Accept/Reject candidate, ready at event cursor 3:
  `BPR-1364-single-click-selects-v001` (`BPR-1364:owner`).
- Next: on hand-back, read `status --after 3`. Accept closes `BPR-1364:owner`
  and the card; a Reject's note is the next round's change.

## Owner verdict and closeout — 2026-10-09

Session `a6f8821efdd9cd5b947d000e9eef319c` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1364-home-single-click-v001/state`),
handed back at event 5, read from cursor 3. Candidate
`BPR-1364-single-click-selects-v001` (revision `frozen-57a08a877-v001`,
fingerprint `91948414e63d…`): **Accept**, 2026-10-09T14:00:34Z, decision
`fa5709d1-e47f-4d14-9bf4-1aa70df7086b` at event 4. This satisfies
`BPR-1364:owner`.

The owner's note, verbatim: "Separate problem: the color of recent is the
same as the color of selected." The attached dark-theme capture (kept as
`/Users/jaeger/bpr-owner-att/BPR-1364-single-click-selects-v001-0.png`)
shows Needs me `#17` selected and Recently viewed `#17`, the review open
behind Home, in the identical blue fill. It is a separate problem and is
[[BPR-1365]]; it does not hold this card.

The candidate was released with `resume-development --all` at event 6, the
host (pid 83554) was stopped by its process ID and the frozen root
`/Users/jaeger/.mask/attended-review/bpr1364-home-single-click-v001.qsxvzbe2`
was removed with `scripts/freeze_attended_build.py remove`. The session's
state folder keeps the record. The card is Done.

## Linux station run (bpr-main-linux, 2026-10-09)

At `fc89f79e3`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner, all pass:
- `application_attended_recent_replay`, the Home open feedback public path
  and the personal discovery, multi-window, review restoration and setup
  destination public paths;
- `application_review_cycle_desktop_public_path` and
  `application_review_cycle_publish_public_path`;
- `application_workbench_composition_contract`, `application_composition_contract`
  and the round-trip contract;
- `qt_anchored_tooltips`, the composed GitHub, Home refusal, Home list, open
  on GitHub and open review from Home scale rows, and `qt_surface_capture_course_scale_100`.
