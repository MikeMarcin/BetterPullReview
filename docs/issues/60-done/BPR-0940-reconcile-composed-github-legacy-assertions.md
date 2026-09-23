---
schema: "implement-rfc.card.v1"
id: "BPR-0940"
title: "Reconcile legacy composed GitHub assertions with current reading and sign-in contracts"
status: "Done"
rfcs: []
depends_on: []
priority: P2
metadata: {"origin":"r20-usable-review-amendments integration verification","scope":"Broader pre-existing composed Qt contract drift; not part of BPR-0935 through BPR-0939 acceptance","origin_rfc":"RFC-BPR-0013","mandate_revision":"r4","disposition":"owner-deferred follow-up; not an RFC0013 closeout dependency"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f839e26a","evidence":{"kind":"observation","detail":"Every remaining failure in the composed GitHub and workbench-window suites was traced, course by course, to the change that governs it: BPR-0915 shared sign-in (cbd50b74) for five composed courses, BPR-0929/BPR-0795 review-surface entry rows for the epoch row, eecdda2a for the empty-history wording. Six are stale entry paths or superseded wording; one is a product regression (QDialog clears Open browser's default, against BPR-0773/RFC-BPR-0032 A2). Exact-profile activation, bound-profile, cancellation (counted through the sign-in cancel sink) and no-other-effect checks are kept.","path":"docs/issues/60-done/BPR-0940-reconcile-composed-github-legacy-assertions.md#ac1-each-remaining-assertion-against-the-current-approved-behavior"}},{"criterion":"AC2","outcome":"pass","revision":"f839e26a","evidence":{"kind":"command","detail":"Courses follow the shared-dialog entry path; device_authorization_panel re-asserts Open browser as default while a code is live. Failing-before controls: re-assertion removed -> qt_composed_github_scale_100 fails at the dialog's filled-control row; enter_read removed -> application_large_review_benchmark fails at its Read render row; Compose review treated as foreign -> qt_workbench_window_scale_100 fails at the epoch row. No test suppressed; authority checks kept.","path":"docs/issues/60-done/BPR-0940-reconcile-composed-github-legacy-assertions.md#ac2-fixture-entry-paths-updated-one-regression-repaired"}},{"criterion":"AC3","outcome":"pass","revision":"0f734801","evidence":{"kind":"command","detail":"ctest build/dev at 0f734801: qt_composed_github_scale_100/125/200 pass (3.59/3.01/3.40 s, reaching 'All GitHub Qt presentation tests passed'); qt_workbench_window_scale_100/125/200 pass (19.49/19.13/19.59 s); application_large_review_benchmark passes (22.00 s) with a Read render inspected; 18 neighbouring sign-in, device and settings rows pass. qt_anonymous_public_preview_scale_100 fails at its pre-existing row, filed as BPR-1023.","path":"docs/issues/60-done/BPR-0940-reconcile-composed-github-legacy-assertions.md#ac3-evidence"}}]
---

# Reconcile legacy composed GitHub assertions

## Owner-deferred follow-up — 2026-09-21

Owner amendment r4 transfers this remaining work out of RFC-BPR-0013 terminal
acceptance. This card keeps its ID, path, status, dependencies and all historical
evidence; only the RFC0013 association is removed. The
[amended RFC](../../rfcs/RFC-BPR-0013-performance-resilience-resource-limits.md#owner-amendment-r4--2026-09-21)
and [ledger](../../verification/RFC-BPR-0013.md#owner-amended-closeout-r4--2026-09-21)
retain its origin and continuation.

Pending: Complete composed GitHub and workbench-window layout reconciliation at the existing scales; known failures remain open.
No owner decision, native observation or repair is supplied by this transfer.

The broad `qt_composed_github_scale_100` suite is not green during the r22
integration pass. Focused new loading, visual, CLI, core/host and actual
production-composed public courses pass; do not describe the broader suite as
passing on their strength.

The initial failures asserted retired contracts: adjacent different root
conversations despite the current selected-root reader; Home sign-in opening
Settings instead of the existing shared dialog; loading remaining on Home
despite BPR-0795; and an inline-comment action without an exact code anchor.
The r22 test-only upkeep corrects those precise fixtures/expectations without
changing product behavior. The next reached failure is:

`with GitHub CLI chosen the card speaks for it: the kind line says so, Connect is its retry, the warning stays and Sign in is not filled`

It occurs in `test_one_card_per_server_holds_its_ways_in`. The card's current
shared sign-in setup mode and the legacy multi-host Settings expectation need
independent reconciliation. Do not simply weaken action/authority assertions
or change shipped sign-in behavior to satisfy an obsolete test. BPR-0933 owns
the separate owner-requested CLI warning cleanup.

## Acceptance

- AC1: Identify each remaining assertion against current approved UI behavior and preserve account, exact-anchor, cancellation and no-effect boundaries.
- AC2: Update stale fixture entry paths or repair an actual regression with a failing-before control, without suppressing tests or reducing authority checks.
- AC3: Run the complete composed GitHub Qt suite at its existing scale registrations and record the exact revision and results.

## Evidence

At r4 source `3efe61dfcf6874d35400bf777a7e86dbbf0a4693`, the ordinary
`application_large_review_benchmark` passes its exact source-document,
navigation, 1,000-wheel and stale-selection assertions, but direct inspection
of its new `large-review-window.png` shows the current Overview page. The
hidden documents being exact does not prove readable on-screen source. Extend
AC1–AC3's current-entry-path reconciliation to this benchmark render and assert
the intended visible Read surface without weakening its existing data checks.
The separate unavailable-diff render correctly shows Read and the typed limit.
No whole-window or visible selected-source acceptance is inferred from the
ordinary benchmark's passing process result.

See the [r22 bounded evidence](../../verification/rfc-bpr-0013-usable-review-20260920.md).
The current failure is durable and remains open; no full-suite acceptance is claimed.

The r23 follow-up also reaches the related workbench-window layout assertion at
all three existing scales: `the epoch row is the first row of the client area
and as wide as it`. The measured row top is 72 versus client top 50. A temporary
control restoring the old, non-wrapping comparison status still fails identically
(2.10 seconds); that control was removed and the unchanged test rebuilt. The
new endpoint-failure presentation is not exercised by this layout setup. Preserve
the existing failure until the current compose/readiness/epoch-row layout
contract is reconciled; do not count this suite as green. Include these window
scale registrations in this card's contract reconciliation and verification.

## Resolution — 2026-09-23, Mac integrator

Code at `f839e26a` (authored as `89c67c7f`, rebased over two BPR-0975 commits
from the Linux station). Fixture and Qt-level only.

### AC1: each remaining assertion against the current approved behavior

Each suite stops at its first failure, so after reconciling the first course
of each, a run with `expect` made non-fatal (not committed) listed the rest at
`8918d226`: 115 failed claims in four more composed courses, and two in one
more window course. All seven courses are below; every failure was in how a
course reached or read the surface, none in its data:

| Course | What it asserted | What governs now | Classification |
|---|---|---|---|
| `test_one_card_per_server_holds_its_ways_in` (composed) | After Connect, the enterprise card's GitHub CLI offer is clicked, then Sign in stays shown and the trust warning stays | [[BPR-0915]] (`cbd50b74`; RFC-BPR-0032 owner amendment): a card's Connect, Sign in and GitHub CLI offer open the shared sign-in dialog, whose setup presentation hides the card's CLI offer during native sign-in and shows only Connect during GitHub CLI sign-in. The course clicked a hidden control. | Stale entry path |
| `test_accounts_page_says_each_fact_once`, "signing in" onwards (composed) | Inventory of the Settings page while a code is live, then three later states | Same: Sign in moves the cards into the dialog and closes Settings; the page held no cards for any later state because nothing closed the dialog | Stale entry path |
| `test_one_sign_in_gesture_shows_the_code_in_the_card` (composed) | The row's own Cancel shown; whole row inside the Settings viewport; Open browser filled | Dialog's own Cancel stands for the row's (BPR-0915 hides the duplicate). **Open browser was no longer filled**: `QDialog::setVisible` makes the button box's auto-default Cancel the default when it has none and clears every other push button's default, and the panel set Open browser's default only at construction | Stale path for Cancel and viewport; **product regression** for Open browser, against [[BPR-0773]] / RFC-BPR-0032 A2 |
| `test_terminal_and_token_rows_stay_in_the_card` (composed) | The row's Cancel emits the device cancel | The dialog's Cancel leaves through `set_sign_in_cancel_dispatch`, which the composition turns into the device cancel (`github_application.cpp`) | Stale entry path |
| `test_a_finished_sign_in_is_said_only_on_its_host` (composed) | Connect on the enterprise card while GitHub.com's code is live; GitHub.com's Connect dispatches an activation | In the dialog the enterprise card is reachable the way the composition reaches it for a link on that host (`open_sign_in`); Connect on a device-capable card begins device sign-in (BPR-0915's `begin_selected_sign_in`) | Stale entry path |
| `test_the_epoch_row_spans_the_window_in_every_modality` (window) | Epoch row top equals the client area's top | [[BPR-0929]]'s Compose review / Draft comment or reply entries (owner-accepted native candidate `rfc0037-compose-pages`) and [[BPR-0795]]'s readiness line sit above it on the review surface. [[BPR-0737]]'s contract (a full-width row between the strip and the drawers, unmoved by drawers and modality) still holds | Superseded "first row" wording |
| `test_review_history_state_is_explicit_in_workbench_text` (window) | An empty loaded catalog says "No review epochs have been recorded." in the inactive status | `eecdda2a` says it only once history is ready, so setting up is not reported as empty; the control's own test was updated there, this one was not | Superseded fixture |

Account, exact-anchor, cancellation and no-effect boundaries are kept: every
activation still names its exact profile and the profile chosen and bound
when it left; every dialog close is counted through the sign-in cancel sink
and nothing else leaves; the device begin names its exact profile.

### AC2: fixture entry paths updated, one regression repaired

- The composed courses now follow the reviewer's path: the dialog opens, its
  facts are checked there (`expect_signing_in_dialog`: only the signing-in
  server's card, its host, transport and Signing in once each, and Open
  browser the one filled control), and closing it returns the cards to
  Settings, where the legacy Settings expectations hold unchanged, including
  "with GitHub CLI chosen the card speaks for it: … the warning stays and Sign
  in is not filled". The dead `SigningIn` page-inventory state is removed.
- Repair: `device_authorization_panel::apply_commands` re-asserts Open
  browser as the default whenever a code is live
  (`src/adapters/qt/src/device_authorization_panel.cpp`). Failing-before:
  with the re-assertion removed, `qt_composed_github_scale_100` fails at
  "the code's Open browser is the dialog's one filled control while signing in
  on GitHub.com".
- The epoch row must sit directly beneath the review surface's entry rows,
  and nothing else may come between the strip and it. Control: treating the
  Compose review entry as foreign fails the course at the reworded row.
- The window's empty-history row publishes the ready status, as the control's
  own test does.
- Benchmark render: the ordinary `application_large_review_benchmark` course
  now enters Read (the existing `enter_read`) before selecting the large file,
  so its measurement, navigation and wheel run on the visible comparison, and
  before the capture it requires Read, both panes visible, the large file
  selected and `int sample_00000 = 0;` / `= 1;` as the first line in each
  pane. The data checks are unchanged. Control: without `enter_read` the
  course fails at "the full-window render shows Read with the large file's
  first source lines in both panes". The render was inspected: Read with
  lines 1–55 of `large/comparison.cpp` in both panes (the baseline showed the
  Overview page).

### Found, not fixed here

`qt_anonymous_public_preview_scale_*` fails on the same drift, in a suite this
card does not name, and carries an authority question: with the activation
command refused, a click on GitHub.com's hidden Connect still begins device
sign-in. Filed as [[BPR-1023]] with the diagnostic run.

### AC3: evidence

macOS 26 arm64, `build/dev`, Qt 6.11.2, offscreen. Re-run at `0f734801`
(the rebased `f839e26a` plus BPR-1023's allocation; no other source
difference) after the rebase brought a BPR-0975 change to
`github_application.cpp`:

- `qt_composed_github_scale_100` 3.59 s, `_125` 3.01 s, `_200` 3.40 s: pass,
  each reaching "All GitHub Qt presentation tests passed".
- `qt_workbench_window_scale_100` 19.49 s, `_125` 19.13 s, `_200` 19.59 s: pass.
- `application_large_review_benchmark` 22.00 s: pass; its render is Read.
- Neighbours on the same surfaces, all pass: `qt_composed_loading_scale_100`,
  `qt_composed_resident_drawer_filters`, `qt_device_authorization_scale_{100,125,200}`,
  `qt_settings_window`, `qt_settings_effect_boundary`,
  `application_setup_destination`, `application_setup_destination_public_path`
  (48.04 s), `application_device_authorization_{public_path,unavailable,registered_course,refresh_course,orphan_course,browser_handoff,browser_absent,panel_course,panel_absent}`,
  `composed_application_hosts`.
- `qt_anonymous_public_preview_scale_100` fails at its pre-existing row; see
  [[BPR-1023]].
- Controls, each restored afterwards: Open browser re-assertion removed → the
  composed suite fails at the dialog's filled-control row; `enter_read`
  removed → the benchmark fails at its render row; Compose review treated as
  foreign → the window suite fails at the epoch row.
- clang-format 23.1.1 passes the four touched C++ files; clang-tidy on them
  reports only findings on untouched lines (the benchmark's glaze member names,
  `std::endl`, casts and statics; `wait_for_frame` in the window test;
  `home_view_state.hpp`; `dif_runtime.hpp`).
