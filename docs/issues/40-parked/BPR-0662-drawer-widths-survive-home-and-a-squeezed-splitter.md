---
schema: "implement-rfc.card.v1"
id: "BPR-0662"
title: "Drawer widths survive Home and a squeezed splitter"
status: "Parked"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
parked_on: "human-review"
acceptance: [{"criterion":"Focused suites pass; a squeeze reports no width and a floor layout opens at the design widths; a handle drag reports exactly the dragged drawer","outcome":"pass","revision":"7506c87b","evidence":{"kind":"observation","detail":"At this head: qt_workbench_window_scale_{100,125,200}, qt_composed_github_scale_100, application_composition_contract, application_review_round_trip_composition_contract, application_github_public_path_uiforetw_74 and application_uiforetw_selected_diff_contract pass. Reporting a width from the squeeze fails 'a squeeze reports no width to the host'; dropping the lift fails 'a width at the host's floor opens at the drawer's design width'."}},{"criterion":"Observed in the signed bundle: the workbench opens with the drawers at usable widths after the clobbered layout","outcome":"pending","revision":"7506c87b","evidence":{"kind":"freeform","detail":"Owed. The signed app-release bundle is rebuilt at this head (BetterPullReview.app, executable sha256 dba2d952133ee18b28e458fb282fe8d9ad3930c68a76d1fd5c3f601d86fc88cf) and the owner's workbench-settings.sqlite3 still holds the clobbered 64-pixel widths, so the observation is one launch away. It needs a pull request opened in the bundle, which needs typing into the locator; this session must not inject input (System Events automation revoked the terminal's permissions once before), so it is queued for the attended session with BPR-0646 and BPR-0663."}}]
---

# Drawer widths survive Home and a squeezed splitter

Opening a pull request in the signed `app-release` bundle at the pack head
(bc551240, 2026-09-07, 1280x820) showed the review workbench with all three
drawers 64 pixels wide and the centre taking the rest. The owner's
`workbench-settings.sqlite3` held `files_width`, `conversations_width` and
`repositories_width` at 64, the host's floor, written on 2026-09-07 00:11
local by the Home build.

Cause. `workbench_drawer_panel::resizeEvent` reported every width change as
the reviewer's, and the window only suppressed reports while it was itself
applying a published layout. Since BPR-0646 the workbench splitter is a
hidden page of the centre stack while Home shows, so it is laid out at its
default size, its drawers squeezed to their 64-pixel minimum, and those
widths were reported and persisted before the page was ever shown. A
window narrower than the persisted widths did the same. Neither is a width
a reviewer chose, and RFC-BPR-0028 AC-03 says the drawers never change on
their own.

## Scope

- A width reaches the host only from a drag of a splitter handle
  (`QSplitter::splitterMoved`); the drawer panel reports nothing from its
  own resize.
- Whenever the splitter itself is resized or shown, the window re-applies
  the persisted widths and the centre absorbs the difference, so a squeeze
  is temporary and never written back.
- A persisted width beneath the presented floor (160 pixels) was written by
  a squeeze; the drawer opens at its design width (files 320, conversations
  375, repositories 300) and nothing is reported until the reviewer drags.
- The UIforETW probe and the window tests drive the handle the way a
  reviewer does, through the splitter's own move.
- Owned paths: `src/adapters/qt` workbench window and drawer panel, the
  UIforETW probe in `src/app/src/github_application.cpp`, their tests and
  the application digests they pin.

## Acceptance

- `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `application_composition_contract` and the UIforETW course pass; a
  published layout applied while the window is hidden, a window narrower
  than the persisted widths, and a return to a wider window report no width
  and end at the persisted widths; a handle drag reports exactly the dragged
  drawer's width; a layout persisted at the floor opens at the design
  widths.
- Observed in the signed bundle: the workbench opens with the drawers at
  usable widths after the clobbered layout.

## Notes

- Fix, 2026-09-07. `workbench_central_splitter` (a `QSplitter` subclass in
  `workbench_window.hpp`, no Q_OBJECT) is the centre's splitter; the window
  connects `splitterMoved` to `observe_handle_drag`, which reads the
  splitter's sizes and reports each expanded drawer whose width changed.
  `workbench_drawer_panel` no longer reports from its own `resizeEvent`
  and has no width dispatch. `central_sizes` grants the drawer's design
  width for a persisted width beneath the 160-pixel presented floor
  (`minimum_presented_drawer_width`); the host's 64-pixel floor and the
  SQLite check are untouched, so no migration. `move_handle(index, pos)`
  is the splitter's own `moveSplitter`, the call a handle drag ends in; the
  window tests and the UIforETW probe drive it instead of `setSizes` or a
  panel resize.
- A re-application of the persisted widths on the splitter's own resize
  was drafted and removed: Qt's splitter keeps the sizes it was given and
  restores them itself once there is room, and a mutation that dropped the
  re-application left every test green. What the tests do catch: reporting
  a width from the squeeze (the old path) fails "a squeeze reports no width
  to the host", and dropping the lift fails "a width at the host's floor
  opens at the drawer's design width".
- Verification at the fix: `qt_workbench_window_scale_{100,125,200}`,
  `qt_composed_github_scale_100`, `application_composition_contract`,
  `application_review_round_trip_composition_contract` (both application
  digests repinned), `application_github_public_path_uiforetw_74`,
  `application_uiforetw_selected_diff_contract`, `check_boundaries`,
  strict tidy on the three touched translation units, `clang-format
  --Werror` and `verify_documentation.py` pass.

## Attended verdict, 2026-09-07: not judged

The owner stopped the session before this candidate. The drawers cannot be
judged while the comparison is empty
([BPR-0667](../10-backlog/BPR-0667-the-epoch-refuses-any-pull-request-whose-base-branch-moved.md)),
so the observation is re-offered with the others once a pull request opens
completely.
