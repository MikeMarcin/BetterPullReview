---
schema: "implement-rfc.card.v1"
id: "BPR-1129"
title: "The title row and the review bar replace the composition entry row"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-01","AC-02","AC-03","AC-10","AC-11"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 The chrome is the title row, the review bar, then the Updates line's timeline while open, and nothing else sits between them in any state or modality.","outcome":"pass","revision":"96a86433","evidence":{"kind":"command","detail":"macOS, build/dev Debug: qt_workbench_window_scale_{100,125,200} pass; test_the_epoch_row_spans_the_window_in_every_modality asserts the review bar directly under the title row, the Updates line the first row beneath it, only BPR-0795's readiness row allowed between (BPR-1130 owns it), the entry row gone, through every modality and drawer state. Control B (bar below the line) fails 'the review bar sits directly under the title row'. The fold does not exist until BPR-1130."}},{"criterion":"AC2 The title row holds only the listed controls, with the modality at its right; every control stays top level at each platform's width floor, swept again on Windows.","outcome":"pass","revision":"96a86433","evidence":{"kind":"command","detail":"qt_workbench_title_row_scale_{100,125,200} and qt_workbench_window_scale_* pass on macOS: exact 16-action title row, modality right of the spacer beside refresh and the comments toggle with its new-comment count, no navigation/progress/composition action in it, sweep 1920 to the 900 floor with no overflow and the name keeping its number. The Windows sweep to 902 (same rows, Windows branch) is owed to bpr-main-win with the command recorded on the card."}},{"criterion":"AC3 The review bar reads Updates and the comparison, then the active tab's controls, from its left, with the two icons on its right. Tooltips name each command and key; Compose review marks a kept draft; Review details opens from the comparison's menu.","outcome":"pass","revision":"96a86433","evidence":{"kind":"command","detail":"Exact 14-action review bar order asserted in the Qt rows and in application_uiforetw_selected_diff_contract (schema v5, four profiles); the Comparing button names the compared dates (qt_epoch_comparison_scale_*); progress tooltip names the key and the file's state; Compose review/Comment tooltips name command and key; test_compose_review_marks_a_kept_draft shows the amber mark with a kept summary and none for empty or discarded drafts (core_review_submission tests core::kept); Review details is in the comparison's menu."}},{"criterion":"AC4 Option-R, Option-C and Esc work, both commands are in the Pull Request menu, and no key sequence is used twice.","outcome":"pass","revision":"96a86433","evidence":{"kind":"command","detail":"test_composition_keys_open_the_pages_and_esc_returns delivers Alt+R, Alt+C and Esc through QWindowSystemInterface (the shortcut path, as Cocoa offers it on macOS) and opens/returns from both pages with focus restored; the Pull Request menu holds both actions; the focus-order course's unique-key check passes. Control A (Alt+R rebound) fails 'Option-R (Alt+R) opens the review page and focuses its summary'."}},{"criterion":"AC5 The height floors are re-taken from the measured stacks on each platform, with the Updates line open and folded.","outcome":"pass","revision":"96a86433","evidence":{"kind":"command","detail":"macOS measured stack: menu 20, title row 30, review bar 28, Updates line 50, status 15, margins 6, status bar 22 -> comparison 829; floor 829 in the capture and its course script, which pass; control C (floor 830) fails naming workbenchReviewBar@50+28. Linux 834 derived from BPR-1036's measured stack and Windows 850 kept: both stations' measurements are owed with the exact command on the card. The folded state is BPR-1130's, which re-takes the floors open and folded."}}]
---

# BPR-1129: The title row and the review bar replace the composition entry row

## Outcome

The title row keeps Home, the drawer toggles, the pull request's name, the
modality at its right, refresh, the comments toggle with the new-comment
count, account and settings. A full-width review bar directly under the title row
takes what leaves it and replaces the composition entry row:

- from its left, the Updates disclosure with the comparison and its dates,
  then previous and next change, previous and next file, and private progress
  as an icon;
- Compose review and Comment as icons on its right, with tooltips naming the
  command and its key, and Compose review marking a kept review draft.

Review details moves into the comparison's menu. Option-R (Alt+R), Option-C
(Alt+C) and Esc back from a composition page are bound, and both composition
commands join the Pull Request menu.

## Seams

- `build_central_surface` and the strip builder in
  `src/adapters/qt/src/workbench_window.cpp` and `workbench_strip.cpp`.
- The epoch comparison control's menu, for Review details.
- The Pull Request menu in `workbench_window.cpp`.
- The review-submission workflow's draft state, projected for the kept-draft
  mark.
- `composeReviewAction` and `composeInlineDraftAction` keep their object names
  and become window-owned, so their keys work without a visible button.

## Tests that change with it

- BPR-0940's allow-list in `test_the_epoch_row_spans_the_window_in_every_modality`.
- The declared focus order and its unique-shortcut check.
- The UIforETW capture's exact strip row and its height floors, in
  `github_application.cpp` and in
  `tests/end_to_end/application_uiforetw_selected_diff_contract.cmake`, with
  the digests `application_review_round_trip_composition_contract` pins.
- The Windows width sweep at the 902 floor.

## Acceptance

- The chrome is the title row, the review bar, then the Updates line's
  timeline while open, and nothing else sits between them in any state or
  modality.
- The title row holds only the listed controls, with the modality at its
  right; every control stays top level at each platform's width floor, swept
  again on Windows.
- The review bar reads Updates and the comparison, then the active tab's
  controls, from its left, with the two icons on its right. Tooltips name each command and key; Compose review marks
  a kept draft; Review details opens from the comparison's menu.
- Option-R, Option-C and Esc work, both commands are in the Pull Request menu,
  and no key sequence is used twice.
- The height floors are re-taken from the measured stacks on each platform,
  with the Updates line open and folded.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-01, AC-02, AC-03, AC-10, AC-11 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.

## Execution record

What was built, how it was verified, and what is owed elsewhere. The
acceptance above is unchanged by it.

## Done on macOS, 2026-09-25 (main, `96a86433`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order (see
the RFC's status section), so this slice started without waiting for
RFC-BPR-0028 to RFC-BPR-0038.

### What changed

- **The title row** (`workbenchPrimaryToolbar`, accessible name "Title row")
  holds, in this exact order, the platform inset, Home, the repositories and
  files toggles, the pull request's name and a spacer, then the modality,
  Home's own Group, State headers and Filter (Home only), refresh, Home's
  discovery refresh (Home only), the comments toggle, a rule, account and
  settings; on Windows the menu control leads and the caption buttons end it.
  No navigation, progress or composition control sits in it. The comments
  toggle carries the count of threads with new comments as an accent pill
  (`set_strip_badge`) and says it in its accessible description; Next new
  comment keeps Alt+N, window-owned, with no title-row presenter. The name
  takes the room the navigation left: the whole name with its title elided at
  the right, then owner/repository and number, then the repository and
  number, then a fragment and the number. The modality keeps its shortcuts
  from 1100 pixels, as before, and drops them first when the name is short.
- **The review bar** (`workbenchReviewBar`, a `workbench_strip` in its new
  `review_bar` role: 28 pixels, and its bare surface drags nothing) sits full
  width directly under the title row, in this exact order: an inset, Updates,
  the comparison's menu and a rule, previous and next change with the position,
  previous and next file with the position, private progress, a spacer,
  Compose review and Comment. The epoch control releases its Comparing button
  to the bar (`release_comparing_button`) and keeps it current; its text now
  names the compared updates' dates ("Comparing whole PR, Feb 12 → Feb 15",
  from `epoch_line::selection_dates`). Review details is the last item of
  that menu, with Alt+Shift+3 unchanged. Private progress's tooltip names the
  key, this file's state ("This file is Read.") and the four actions.
- **Compose review and Comment** are window-owned actions with the object
  names the composition pages already used (`composeReviewAction`,
  `composeInlineDraftAction`), Option-R (Alt+R) and Option-C (Alt+C), icons on
  the bar (a pencil and a speech balloon) whose tooltips name the command and
  its key, and entries in the Pull Request menu. Esc returns from either
  composition page to the invoking surface and focus. Compose review carries
  an amber mark, the colour of my last review's tick, while the review
  workflow's presented draft is kept; what "kept" means is the core's
  (`core::kept`: a summary or a verdict while drafting, or a submission not yet
  settled), and the editor reports each presentation to the window.
- **The composition entry row is gone.** The bar yields text as the window
  narrows, the dates first and then the positions' words ("3/40"), and never a
  control.
- **Home has no review bar**: the bar belongs to the review surface.

### What this slice leaves to the next ones, deliberately

- The Updates disclosure that folds the line, the line's permanent caption,
  and the readiness row ([[BPR-0795]]) are [[BPR-1130]]'s. Until it lands the
  bar names "Updates" as a label, the timeline keeps its 50-pixel row with
  its caption and its "pair ready" words, and the readiness line may sit
  between the bar and the line while a pull request loads; the window course
  allows exactly that and nothing else.
- Comment's arrow and the target its menu names are [[BPR-1133]]'s; Comment
  opens the inline composition page, as "Draft comment or reply" did.
- The height: the bar is 28 pixels where the entry row was 22 on macOS and
  20 on Linux, so until [[BPR-1130]] takes the caption off the line the
  comparison is 6 pixels shorter on macOS than it was (829 of 1000 against
  835). With the line folded and uncaptioned the design's figures are 710
  open and 744 folded at 1280x852 against 707 today; [[BPR-1130]] re-takes
  the floors then.

### Measurements (1280x1000, offscreen, every drawer collapsed, Review)

| Platform | Stack | Comparison | Floor |
| --- | --- | --- | --- |
| macOS, measured at `96a86433` | menu bar 0+20, title row 20+30, review bar 50+28, Updates line 78+50, diff status 128+15, 6 of margins, status bar 978+22 | 829 | 829 (was 835) |
| Linux, derived | [[BPR-1036]]'s measured stack (menu bar 19, strip 30, entry row 20, epoch line 50, status 14, margins 6, status bar 19) with the 28-pixel bar for the 20-pixel entry row | 834 | 834 (was 842) |
| Windows, unmeasured | no menu bar row | — | 850, the owner's budget, unchanged |

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug)

| Check | Result |
| --- | --- |
| `qt_workbench_window_scale_{100,125,200}` | Pass. The exact title-row (16 actions) and review-bar (14) orders; BPR-0940's course (the bar directly under the title row, nothing else above the Updates line but the readiness row, the entry row gone, the line and the bar as wide as the client area through every modality and drawer state); Option-R, Option-C and Esc delivered through the window system's shortcut path; the kept-draft mark appearing and going with the presented draft; review details in the comparison's menu; the comments toggle's count; the chrome renders |
| `qt_workbench_title_row_scale_{100,125,200}` | Pass. Both rows' orders, the name's ladder and the modality at every width from 1920 to the 900 floor with nothing folding, the bar's yield order in a doubled font, and the title row's drag and zoom |
| `qt_workbench_focus_order_scale_{100,125,200}` | Pass. The bar's eight stops follow the title row's in its order; no key sequence is claimed twice |
| `application_uiforetw_selected_diff_contract` | Pass, four profiles, 70 s: both rows asserted exactly (capture schema v5), 829/1000 height |
| `application_review_round_trip_composition_contract` | Pass with the pins resealed for the `BPR_BUILD_TESTING` capture change (review recorded in the contract, covering `0114d6a8..96a86433`) |
| `qt_composed_github_scale_{100,125,200}`, `qt_composed_loading_scale_100`, `qt_composed_home_sign_in`, `qt_composed_cli_read_consent`, `qt_composed_resident_drawer_filters`, `qt_surface_capture_course_scale_{100,125,150,200}`, `qt_epoch_comparison_scale_{100,125,200}`, `qt_review_submission_editor_scale_{100,125,200}`, `qt_workbench_{home_discovery_scale_*,conversation_reveal,overview}`, `qt_shared_snapshot_navigation`, `qt_native_window_geometry`, `qt_github_conversation_panel`, `qt_conversation_cards_view`, `core_review_submission`, `application_github_public_path_uiforetw_74`, `application_diff_workbench_public_path`, `application_workbench_composition_contract`, `application_setup_destination`, `test_network_isolation_manifest`, `architecture_boundaries`, `core_dependency_boundary` | 44 of 44 pass at the pre-rebase candidate; the rows above re-run at `96a86433` |
| Control A, scratch: Compose review bound to Alt+Shift+Y | Fails: `Option-R (Alt+R) opens the review page and focuses its summary` |
| Control B, scratch: the review bar laid out below the Updates line | Fails: `the review bar sits directly under the title row` |
| Control C, scratch: the capture's macOS floor at 830 | Fails: `UIFORETW_CAPTURE_LAYOUT_FAILED:985:829 … stack=workbenchMenuBar@0+20,workbenchPrimaryToolbar@20+30,workbenchReviewBar@50+28,epochComparisonControl@78+50,…` |
| clang-format 23.1.1 | Clean on the seventeen touched C++ files (their `HEAD` versions were clean) |
| `_coding-standard/scripts/tidy.py` | `clang-tidy passed` on the eight changed translation units |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The controls were scratch edits, restored and rebuilt. The renders are at
`build/dev/tests/adapters_qt/workbench-window-{100,125,200}-review-bar-{1280x852,floor}.png`
(not committed). The workbench specification describes the built chrome.

### Owed elsewhere: the Windows and Linux runs

At or after `96a86433`, each station builds its dev preset and runs:

```sh
ctest --test-dir <build> --output-on-failure -R "^(application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract|qt_workbench_title_row_scale_(100|125|200)|qt_workbench_window_scale_100|qt_workbench_focus_order_scale_(100|125|200)|qt_composed_github_scale_100|qt_epoch_comparison_scale_100|core_review_submission|test_network_isolation_manifest)$"
```

- **Linux (bpr-main-linux): owed.** Expect every row to pass and the capture
  at or above 834. Record the measured comparison height from
  `<build>/tests/end_to_end/uiforetw-selected-diff-contract/data/evidence/light/capture-facts.json`
  (`comparison_rect_in_window.height`); if it is not 834, the floor in
  `src/app/src/github_application.cpp` and
  `tests/end_to_end/application_uiforetw_selected_diff_contract.cmake` takes
  the measured value, with the pins resealed.
- **Windows (bpr-main-win): owed.** The same command, plus
  `qt_workbench_strip_menu` and `application_windows_title_row`, which only
  Windows builds. The Windows title row is swept to its 902 floor and must show
  the name and the whole modality words there. Record the measured comparison
  height; the floor stays the owner's 850. If the 28-pixel bar takes Windows
  below 850 (it had about 857 with the entry row), the failure line names the
  stack; that is a decision for the owner, not a floor to lower here.

### Owner visual asks (for the attended look BPR-1135 needs; no session prepared)

The card's boundary keeps the owner's look at the built chrome on macOS and
Windows outside what it can supply. When that session is prepared, these are
the questions, one per candidate criterion:

1. The title row at 1280x852: Home, the drawer toggles and the name at the
   left, the modality at the right beside refresh, the comments toggle with
   its count, account and settings. Is this the row you chose?
2. The review bar at 1280x852 and at the width floor: Updates, the comparison
   with its dates, the change and file navigation with their positions,
   progress, and Compose review and Comment at the right. Is this the bar you
   chose?
3. Compose review's amber mark while a review draft is kept: is it visible
   and clear enough?
4. The comparison is 6 pixels shorter than before (829 of 1000 on macOS)
   until BPR-1130 folds the Updates line and takes its caption away. Is that
   acceptable in the meantime?
