---
schema: "implement-rfc.card.v1"
id: "BPR-1365"
title: "Home paints the open review in the selection colour; only one Home list shows a selection"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"defect","found_by":"Owner, BPR-1364 attended review (accepted), 2026-10-09","reservation":"docs/issues/_reservations/BPR-1365--BPR-1365--b5afbbae5ee9.json","related_cards":["BPR-1364","BPR-0776","BPR-0733","BPR-1357","BPR-1157"],"requested_skill":"frontend-design"}
acceptance: [{"criterion":"BPR-1365:one-selection","outcome":"pass","revision":"78b1f21c5","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1365-home-paints-the-open-review-in-the-selection-colour.md","detail":"qt_composed_github_scale_{100,125,200} test_home_shows_one_selection_and_the_open_review_quietly and application_home_open_feedback_public_path: at most one Home list shows a selection, the one holding the keyboard; a click or Tab moves it; publishing Recently viewed or Review Cycles selects nothing outside the owner."}},{"criterion":"BPR-1365:reviewing","outcome":"pass","revision":"78b1f21c5","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1365-home-paints-the-open-review-in-the-selection-colour.md","detail":"qt_home_list_scale_{100,125,200} test_the_open_review_is_quiet_and_never_the_selection: the open review's row is on AlternateBase with a Reviewing tag, never Highlight unless selected, in every theme; its accessible description starts 'Reviewing in this window'."}},{"criterion":"BPR-1365:themes","outcome":"pass","revision":"78b1f21c5","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1365-home-paints-the-open-review-in-the-selection-colour.md","detail":"application_home_open_feedback_public_path --render-directory and qt_home_list_scale_* write light, dark, high-contrast light and high-contrast dark renders of the open review beside a selection."}},{"criterion":"BPR-1365:failing-before","outcome":"pass","revision":"78b1f21c5","evidence":{"kind":"command","path":"docs/issues/60-done/BPR-1365-home-paints-the-open-review-in-the-selection-colour.md","detail":"With the behaviour set aside and the tests kept, qt_composed_github_scale_100, qt_home_list_scale_100 and application_home_open_feedback_public_path fail; restored, they pass."}},{"criterion":"BPR-1365:owner","outcome":"pass","revision":"78b1f21c5","evidence":{"kind":"observation","path":"docs/issues/60-done/BPR-1365-home-paints-the-open-review-in-the-selection-colour.md","detail":"Owner Accept, no note, 2026-10-09T15:18:48Z: session 3a408f4db19329315edb27a85a768226, candidate BPR-1365-one-selection-reviewing-v001 (frozen 78b1f21c5, fingerprint 457146002de5...), decision adfe7958-1ef6-44e7-b3e0-6ee09f8b93e0 at event 4, handed back at 5, released at 6."}}]
---

# BPR-1365: Home paints the open review in the selection colour; only one Home list shows a selection

## Owner notes

Owner, BPR-1364 attended review (Accept), 2026-10-09, verbatim: "Separate
problem: the color of recent is the same as the color of selected."

The attachment (`/Users/jaeger/bpr-owner-att/BPR-1364-single-click-selects-v001-0.png`,
dark theme) shows Home over the open review of `octo-org/review-repository#17`.
Needs me `#17` is selected (blue fill), and Recently viewed `#17`, the review
open behind Home, has the identical blue fill. So "selected" and "the open
review" look the same.

## Scope

- Selection keeps the native selection colour, and only the current row of the
  Home list that holds the keyboard shows it.
- The open review gets a distinct, quieter treatment, never the selection fill.
- Only one Home list shows a selection at a time.

Every Home list is in scope: the queue in each arrangement (state panels, the
flat card, group panels and their state bands), Recently viewed and Review
Cycles. All four themes are in scope.

## Acceptance

- `BPR-1365:one-selection`: at most one Home list shows a selection, the one
  holding Home's keyboard. Clicking or tabbing into another list moves the
  selection there. Publishing Recently viewed or Review Cycles never selects a
  row in a list that does not hold the keyboard.
- `BPR-1365:reviewing`: the open review's row is drawn with a quiet treatment
  distinct from the selection fill, in every theme, and its accessible
  description says so.
- `BPR-1365:themes`: renders of light, dark, high-contrast light and
  high-contrast dark show this.
- `BPR-1365:failing-before`: the new tests fail against the code before the
  change.
- `BPR-1365:owner`: the owner accepts the change on a frozen build (one
  Accept/Reject question).

## What painted it (inventory at `1adeb4cfc`)

- **Recently viewed was selected, not marked.** `apply_recent_reviews` kept a
  current row with `setCurrentRow`, and in a single-selection list that also
  selects it. Every publication of Recently viewed therefore selected its
  first row, which after opening a review is the review open behind Home.
- **The style painted every selection alike.** `home_row_delegate::paint`
  leaves the ground to the style, which fills any selected row in an active
  window with the palette's Highlight whether or not its list has the
  keyboard. The app's palette sets Active and Inactive Highlight to the same
  colour. So Needs me `#17`, selected by the click, and Recently viewed
  `#17`, selected by the publication, showed the same blue.
- **One current row covered only the queue.** `home_list::adopt_current`
  clears the other queue views when a row becomes current, but Recently
  viewed and Review Cycles sat outside it, and nothing anywhere marked the
  open review.

## Implementation

The `frontend-design:frontend-design` skill was loaded first. Design choice:
on Home colour means state (blue is Waiting on others), and on macOS the
accent is the selection's own blue family, so the open review gets no hue.
It gets the theme's current-line tint (AlternateBase, the ground the code
canvas gives "where you are") and a **Reviewing** tag. That is the word Home's
title already uses ("Reviewing octo-org/… #17"); "Open" would read as GitHub's
pull-request state and as a command beside Open Review.

- **Application / composition.** `recent_review_entry::Open`. `publish_recent`
  marks the entry whose destination equals this window's `ReadingDestination`,
  and Recently viewed is published again when an accepted read or handoff sets
  that destination or an invalidation clears it. `github_application.cpp`
  changed, so the round-trip pins are resealed in the same commit.
- **Qt.** `home_row_role::reviewing`, with `home_reviewing_text()` ("Reviewing")
  and `home_reviewing_description()` ("Reviewing in this window"), which
  starts the row's accessible description. `review_landing_model::set_reviewing`
  marks the queue row with the window's exact `open_queue_ref()`; the window
  sets it in `update_pull_request_queue_actions`. `home_row_delegate` paints
  the tint under the style's ground only when the row is not selected, and the
  tag (secondary font, Text ink, PlaceholderText outline, 3-point radius so it
  never reads as an account chip) left of the time. A selected row keeps the
  selection and the tag takes HighlightedText. `reviewing_rect` says where the
  tag is.
- **One selection.** `workbench_window::home_selection_list` (queue, recent,
  cycles) replaces `HomeRecentRowLast`; `claim_home_selection` runs when focus
  enters a Home list and from both row menus. The claimed list's current row
  is selected; every other list keeps its current row and has its selection
  cleared (`home_hide_selection` / `home_show_selection`).
  `home_list::set_selection_shown` does this for every queue view, and a guard
  on each list's `selectionChanged` keeps any later programmatic current-row
  change (a refresh restoring the row, a right click, a publication) from
  selecting outside the owner. `apply_recent_reviews` sets its row with
  `NoUpdate` unless Recently viewed owns the selection. With a review cycle
  selected, Home's pull-request actions name no row.
- Specification: "One selection, and the open review" in
  `docs/ux/workbench-specification.md`.

## Verification (macOS, `build/dev` Debug, offscreen, 2026-10-09)

- `qt_home_list_scale_{100,125,200}`: new
  `test_the_open_review_is_quiet_and_never_the_selection` in light, dark,
  high-contrast light and high-contrast dark. The queue row and the Recently
  viewed row of the open review are on AlternateBase (pixel, ±6), and a row
  that is not open is not. The tag is inside the row and inked. The other row
  has no tag. The accessible description starts "Reviewing in this window".
  Selected, the open row is Highlight (±24). Hidden, the queue keeps its
  current row and selects nothing, even when `select(ref, false)` restores
  another row. Shown again, the current row is selected. It writes four
  renders, `home-list-<scale>-reviewing-<theme>.png`.
- `qt_composed_github_scale_{100,125,200}`: new
  `test_home_shows_one_selection_and_the_open_review_quietly` through the
  window. A publication makes Recently viewed's first row current without
  selecting it. With the queue's `#73` selected (window-system click) beside
  the open `#73`, the selected row is Highlight and the open row is
  AlternateBase, not Highlight. A republication takes no selection. A click
  moves the one selection to Recently viewed and then to Review Cycles, and
  the queue keeps its current row. The keyboard's return to the queue selects
  only it.
- `application_home_open_feedback_public_path` (composed application): after
  opening `#17` and returning Home, in all four themes, the composition marks
  Recently viewed's `#17` and only it. The queue's `#17` says so and `#4001`
  does not. A window-system click on the queue's `#17` is Home's one selection
  and Recently viewed selects nothing. Pixel checks: the selected row is
  Highlight and the open `#17` is AlternateBase, not Highlight. A click moves
  the selection to Recently viewed (then Highlight) and back. With
  `--render-directory` it writes `<theme>-reviewing.png` and
  `<theme>-reviewing-recent-selected.png` for light, dark, high_contrast_light
  and high_contrast_dark.
- Also passing: `qt_review_landing_model`,
  `qt_workbench_open_review_from_home_scale_{100,125,200}`,
  `qt_workbench_open_on_github_scale_100`, `qt_workbench_window_scale_100`,
  `qt_composed_home_sign_in`, `qt_composed_home_refusal_scale_100`,
  `qt_surface_capture_course_scale_100`, `core_home_open`,
  `application_home_open_host`, `application_attended_recent_replay`,
  `application_recent_account_isolation`,
  `application_profile_window_review_restoration_public_path`,
  `application_document_tabs_restoration_public_path`,
  `application_profile_multi_window_public_path`,
  `application_personal_discovery_public_path`,
  `application_review_cycle_desktop_public_path`,
  `application_review_cycle_publish_public_path`,
  `test_network_isolation_manifest`, `architecture_boundaries` and every
  `*_composition_contract` (round-trip pins resealed:
  full digest `498510165f41…`, manifest `26ccd144c16d…`).
- `qt_workbench_home_discovery_scale_100` fails ("the empty Needs me lane is
  not shown while nothing is saved; Review Cycles stays") with this card's
  source set aside too, at `1adeb4cfc`. It predates this card and is not
  touched by it.
- clang-format 22.1.8 on every changed source. clang-tidy 22:
  `home_row_delegate.cpp` and `review_landing_model.cpp` clean. The findings
  on `home_list.hpp` (`portability-template-virtual-member-function` on
  `home_fitted_view`) and `workbench_window.cpp:13073` (`compact_below`, from
  `1cd9f15e4b`) are on main as well.

**Failing-before.** With the behaviour set aside and the tests kept (no tag
or tint, `reviewing` false, `set_selection_shown` and the guards no-ops,
`claim_home_selection` only recording the list, `apply_recent_reviews`
selecting), `qt_composed_github_scale_100` ("publishing Recently viewed makes
its first row current without selecting it"), `qt_home_list_scale_100` ("the
open pull request's queue row is the one this window is reviewing, and says
so (light)") and `application_home_open_feedback_public_path` ("Recently
viewed says which of its entries is the open review") fail. Restored, they
pass.

## Owner look — prepared 2026-10-09

- Landed on main as `78b1f21c5`. Frozen build of `78b1f21c5` with
  `scripts/freeze_attended_build.py freeze` (source export and tests-on build
  inside `/Users/jaeger/.mask/attended-review/bpr1365-home-reviewing-v001.x8t3w3e7`;
  `better_pull_review` SHA-256
  `01f124c93a47d2e9d6c46cabd33ce97b3ab64c88c6bc0b56cd12aa70e312b322`), the
  BPR-1364 launcher pattern without the slow network: fixture `github-com`,
  `--attended-home-look`, which signs in, opens `#17` once from the Home bar,
  saves its repository and shows Home over the open review. The launcher's
  own offscreen check reached `ATTENDED_HOME_SHOWN:rows=2:delay=0ms`. Started
  with
  `python3 /Users/jaeger/.mask/attended-review/bpr1365-home-reviewing-v001.x8t3w3e7/launch.py`.
- New short session `3a408f4db19329315edb27a85a768226`, state
  `/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1365-home-reviewing-v001/state`,
  host detached (pid 88807). One Accept/Reject candidate, ready at event
  cursor 3: `BPR-1365-one-selection-reviewing-v001` (`BPR-1365:owner`). It
  carries the four theme renders from the composed-application course and
  names the design choice: no accent colour and "Reviewing" rather than "Open".
- Next: on hand-back, read `status --after 3`. Accept closes `BPR-1365:owner`
  and the card. A Reject's note (for example an accent bar, or another word)
  is the next round's change.

## Owner verdict and closeout — 2026-10-09

Session `3a408f4db19329315edb27a85a768226` (state
`/Users/jaeger/.mask/attended-review/owner-review-20261009-bpr1365-home-reviewing-v001/state`),
handed back at event 5, read from cursor 3. Candidate
`BPR-1365-one-selection-reviewing-v001` (revision `frozen-78b1f21c5-v001`,
fingerprint `457146002de5…`): **Accept**, no note, 2026-10-09T15:18:48Z,
decision `adfe7958-1ef6-44e7-b3e0-6ee09f8b93e0` at event 4. This satisfies
`BPR-1365:owner`.

The candidate was released with `resume-development --all` at event 6, the
host (pid 88807) was stopped with the session's own `stop` and is gone, and
the frozen root
`/Users/jaeger/.mask/attended-review/bpr1365-home-reviewing-v001.x8t3w3e7`
was removed with `scripts/freeze_attended_build.py remove`. The session's
state folder keeps the record. The card is Done.

## Linux station run (bpr-main-linux, 2026-10-09)

At `9b1603193`, WSL2 Ubuntu 26.04, GCC 15.2.0, `build/dev` (Debug), offscreen, network-isolated runner, all pass: `qt_home_list_scale_{100,125,200}`,
`qt_composed_github_scale_{100,125,200}` and
`application_home_open_feedback_public_path`.
