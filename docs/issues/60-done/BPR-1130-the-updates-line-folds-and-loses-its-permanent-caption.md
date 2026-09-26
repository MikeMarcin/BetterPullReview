---
schema: "implement-rfc.card.v1"
id: "BPR-1130"
title: "The Updates line folds and loses its permanent caption"
status: "Done"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1129"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-01","AC-04","AC-11"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json","milestone_plan":{"id":"rfc-bpr-0052-review-workbench-scaffolding","path":"docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
acceptance: [{"criterion":"AC1 The fold works from the review bar and by keyboard. A new window starts folded, and each window restores its own state.","outcome":"pass","revision":"fb4b8364","evidence":{"kind":"command","detail":"macOS 26.6.2 arm64, build/dev Debug: qt_workbench_window_scale_{100,125,200} pass test_the_updates_line_folds_from_the_review_bar (new window folded with the panes under the bar; click, Space on the focused disclosure and the View menu's Updates Line each send one toggle intent; open is one 34-pixel row under an unmoved bar; nothing folds through the modalities) and BPR-0940's course, now starting folded; core_workbench_interaction (fold decided in the core, accepted with no workbench, changed by nothing else); application_workbench_host (kept in the window's own record, held and merged before load, restored on load without a workbench, never a profile default); sqlite_workbench_settings_storage (schema v10 per-window column, v9 migration reads folded). Controls B (timeline shown whatever the fold) and C (host does not keep the fold) fail. The owner's attended look is pending on BPR-1135."}},{"criterion":"AC2 There is no permanent caption. The tooltip carries the history statement, and an incomplete or ambiguous history shows a mark on the line in every modality.","outcome":"pass","revision":"fb4b8364","evidence":{"kind":"command","detail":"qt_epoch_comparison_scale_{100,125,200} pass test_the_line_marks_incomplete_history_without_a_caption: the line is 34 pixels (nodes and dates); complete history has no mark and its statement is the tooltip away from a node and the accessible description; incomplete, ambiguous, unresolved and local-records-only histories are marked with ink in the ring and the track after it. The window course asserts the mark on the line and on the review bar's disclosure (with the statement in both tooltips) in Overview, Read, Discuss and Review. The line's 'pair ready' words left the row; their sentence is the comparison menu's tooltip (owner ask 4)."}},{"criterion":"AC3 The height floors hold with the line open and folded on each platform.","outcome":"pass","revision":"fb4b8364","evidence":{"kind":"command","detail":"application_uiforetw_selected_diff_contract passes on macOS in four appearance profiles: the capture window starts folded, opens the line through its own command, and measures folded 879 and open 845 of 1000 (stack menu 20, title 30, bar 28, diff status 15, margins 6, status bar 22; open adds the timeline at 78+34), asserted as floors in the capture and the course script (schema v6). Control A (folded floor 880) fails naming both stacks. Linux 884/850 is derived from BPR-1036's stack and Windows keeps the owner's 850 open (884 folded): both station runs are owed with the exact command on the card."}}]
---

# BPR-1130: The Updates line folds and loses its permanent caption

## Outcome

The Updates line folds from the review bar's disclosure. Open, its timeline
runs alone across the whole window just below the bar; folded, it leaves no
row. The bar does not move either way, and it names the comparison and its
dates in both states. A new window starts folded, and each
window restores its own choice through the existing window restoration.
Nothing folds or opens on its own.

The permanent caption goes. The history statement becomes the line's tooltip
and accessible description, and a visible mark on the line says when history
is incomplete or ambiguous, the statement RFC-BPR-0029 requires the line to
make.

## Ask first

Before it changes the readiness line, this card asks the owner whether its
words should take the Updates line's track, or the review bar's summary when
folded, so that no row comes or goes above the comparison. The rounds drew
that from v2 on, but it was never asked separately. Without an answer,
[[BPR-0795]]'s row stays.

## Acceptance

- The fold works from the review bar and by keyboard. A new window starts
  folded, and each window restores its own state.
- There is no permanent caption. The tooltip carries the history statement,
  and an incomplete or ambiguous history shows a mark on the line in every
  modality.
- The height floors hold with the line open and folded on each platform.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-01, AC-04, AC-11 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
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

## Done on macOS, 2026-09-25 (main, `fb4b8364`)

The owner activated RFC-BPR-0052 on 2026-09-25 ahead of canonical order (see
the RFC's status section), so this slice started without waiting for
RFC-BPR-0028 to RFC-BPR-0038.

### What changed

- **The disclosure.** The review bar's first control after its inset is the
  Updates disclosure (`reviewBarUpdates`, `qt::updates_disclosure`): a
  triangle and the word Updates, painted whole because the bar frames its
  presenters and a disclosure is a line of text. It replaces [[BPR-1129]]'s
  "Updates" label in the same place, so the bar's exact order is unchanged.
  The View menu's **Updates Line** (`toggleUpdatesLineAction`, checkable)
  sends the same gesture, and the disclosure takes focus and answers Space.
  The fold takes no key of its own: the RFC decided the window's keys and none
  is the fold.
- **The fold is the core's.** `core::workbench_updates_line` (folded, open)
  is part of the workbench interaction state, with
  `set_workbench_updates_line_intent`, `toggle_workbench_updates_line_intent`,
  its own delta, and a tenth command, `toggle_updates_line`, always offered as
  a drawer toggle is (the line is the review's chrome whether or not a
  workbench is active, so a pull request whose comparison failed can still
  open it). No other intent or observation changes it: nothing folds or opens
  on its own. The window re-asserts the core's state after each click, as it
  does for the drawers.
- **Each window keeps its own.** `workbench_layout_settings` and
  `workbench_window_layout` carry `UpdatesLine`. The workbench host keeps it on
  Home's terms ([[BPR-0783]]): only in the window's own record, never in the
  profile's defaults (so every new window starts folded), and a fold chosen
  before the record loads is held and merged into it, and drained with it at an
  orderly close. It is restored as soon as the record loads, without waiting
  for a workbench. The SQLite settings store's version ten adds
  `updates_line` to the per-window table only; version-nine rows read folded,
  keeping their native placement (the migration now carries the geometry
  columns), and a value outside the vocabulary is refused without writing.
- **Folded, no row; open, one row.** Folded, the Updates line's control is
  hidden and the panes sit directly under the bar; open, the timeline is one
  34-pixel row across the window under the bar. The bar never moves. Review
  details still open under the bar while the line is folded (the control shows
  them and not the timeline).
- **No permanent caption.** `epoch_line::height_pixels` is 34: the nodes and
  their dates. The history statement ("Known updates · display order is not
  push order") is the line's tooltip away from a node and its accessible
  description. When history is incomplete or ambiguous as RFC-BPR-0029
  defines it (incomplete or cut-off, a provisional current snapshot, ambiguous
  occurrences, unresolved local records, or local records alone), a mark, a
  ring with an exclamation point in the text colour, sits at the line's start
  and the track begins after it; the disclosure carries the same mark after its
  name, with the statement in its tooltip and "Updates, history incomplete" as
  its accessible name, so a folded line still says so, in every modality.
- **The line runs alone.** RFC-BPR-0052 draws the open timeline alone on its
  row, so the line's "pair ready" words ([[BPR-0676]]) left it with the
  caption: they stay the control's fact, their sentence is the comparison
  menu's tooltip together with the history statement, and loading or an
  unreadable pair remains the readiness line's to say.
- **The readiness line is unchanged.** [[BPR-0795]]'s row still sits between
  the bar and the line while a pull request loads; the owner has not answered
  where its words go (below), and the card's "Ask first" keeps the row until
  then.
- **Floors, both ways.** The UIforETW capture's window starts folded (a fact
  it records); it opens the line through the window's own command, so the core
  decides and the host publishes, measures the open stack and folds it again,
  and asserts a folded and an open floor. Its failure line names both stacks.
  Capture schema v6 records the initial fold, the open line's rectangle and the
  open comparison's height; the course script asserts the same floors and that
  the open line is 34 pixels at the bar's bottom across 1280, and that folding
  gives exactly that row back. The round-trip pins are resealed with the review
  recorded in the contract (`96a86433..fb4b8364`, capture only).
- **Specification.** [workbench-specification.md](../../ux/workbench-specification.md)
  describes the fold, the caption-less line, the mark and the floors under
  "Primary layout".

### Measurements (1280x1000, offscreen, every drawer collapsed, Review)

| Platform | Folded stack | Folded | Open | Floors (folded, open) |
| --- | --- | --- | --- | --- |
| macOS 26.6.2 arm64, measured at `fb4b8364`, all four appearance profiles | menu bar 0+20, title row 20+30, review bar 50+28, diff status 78+15, 6 of margins, status bar 978+22; open adds the timeline at 78+34 | 879 | 845 | 879, 845 (were 829 with the 50-pixel line) |
| Linux, derived | [[BPR-1036]]'s stack gave 834 with the captioned 50-pixel line | 884 | 850 | 884, 850 |
| Windows, unmeasured | no menu bar row | — | — | 884, and the owner's 850 open |

### Verification (macOS 26.6.2 arm64, Qt 6.11.2, `build/dev` Debug, at `fb4b8364`)

| Check | Result |
| --- | --- |
| `core_workbench_interaction` | Pass. `test_updates_line_state` (new): starts folded; the toggle is accepted with no workbench; the fold publishes only itself; set and toggle; no modality, drawer, numbering, navigation or workbench coming and going changes it; the tenth command is offered. The command table agrees with the transitions for all ten commands |
| `application_workbench_host` | Pass. `test_the_updates_line_folds_persists_and_restores_per_window` and `test_orderly_shutdown_drains_a_held_fold` (new): held before the load and merged into the record with its drawers and widths intact; kept at once after; a restarted window restores the record's open line without a workbench and writes nothing back; the defaults record answers folded and never stores a fold; a held fold drains at an orderly close |
| `sqlite_workbench_settings_storage` | Pass. `test_the_updates_line_is_window_local_and_migrates_from_nine` (new): two windows keep opposite folds, a window with no row and the defaults read folded, an invalid fold is refused before the write, a version-nine file migrates with geometry kept and reads folded, a hostile value is refused without modifying the file |
| `qt_workbench_window_scale_{100,125,200}` | Pass. `test_the_updates_line_folds_from_the_review_bar` (new): a new window is folded with the panes under the bar; click, Space on the focused disclosure and the View menu command each send one gesture and show nothing the core did not decide; open is one 34-pixel row under a bar that has not moved, alone on its row; the statement is the tooltip and description; the mark on the line and the disclosure in every modality; folded again; Review details under the bar while folded; the width floor. BPR-0940's course (`test_the_epoch_row_spans_the_window_in_every_modality`) starts folded, then asserts the open line through every modality, drawer state and width, and folds back with the bar unmoved |
| `qt_epoch_comparison_scale_{100,125,200}` | Pass. `test_the_line_marks_incomplete_history_without_a_caption` (new): complete history carries no mark and its statement is the tooltip and description; incomplete, ambiguous and unresolved histories and local records alone are marked, with ink in the ring and the track starting after it |
| `application_uiforetw_selected_diff_contract` | Pass, four profiles: folded 879 and open 845, both floors, the fold facts |
| `application_review_round_trip_composition_contract`, `application_composition_contract` | Pass with the pins resealed |
| `qt_workbench_focus_order_scale_*`, `qt_workbench_title_row_scale_*`, `qt_composed_github_scale_*` (the composed Home course now expects the review bar with the line folded), `application_workbench_preferences_consumers`, `application_profile_window_layout_public_path`, `application_profile_window_restart_public_path`, `test_network_isolation_manifest`, `architecture_boundaries`, `core_dependency_boundary` | Pass; 27 of 27 at the pre-rebase candidate, and the capture, window, epoch, host, store and core rows again at `fb4b8364` (9 of 9) |
| `core-only` preset | Builds; 163 of 163 pass |
| Control A, scratch: the capture's macOS folded floor at 880 | Fails: `UIFORETW_CAPTURE_LAYOUT_FAILED:985:879:open=845 … stack=workbenchMenuBar@0+20,workbenchPrimaryToolbar@20+30,workbenchReviewBar@50+28,diffWorkbenchStatus@78+15,QStatusBar@978+22 … open_line=78+34 open: … stack=…,epochComparisonControl@78+34,diffWorkbenchStatus@112+15,…` |
| Control B, scratch: the window shows the timeline whatever the fold | Fails: `a new window starts with the Updates line folded, and the panes sit under the review bar` (and the capture: `UIFORETW_UPDATES_LINE_FOLD_FAILED:initially_folded=0`) |
| Control C, scratch: the host does not keep the fold | Fails: `the held fold is merged into the loaded record, which keeps its drawers and widths` |
| clang-format 23.1.1 | Clean on the twenty touched C++ files (their `HEAD` versions were clean) |
| `_coding-standard/scripts/tidy.py --changed` | `clang-tidy passed`; the five findings it prints are on lines this card did not touch |
| `python3 scripts/verify_documentation.py --root .` | `DOCUMENTATION_OK` |

The controls were scratch edits, restored and rebuilt. The renders are at
`build/dev/tests/adapters_qt/workbench-window-{100,125,200}-updates-{open,folded}-{1280x852,floor}.png`
(not committed).

### Owed elsewhere: the Windows and Linux runs

At or after `fb4b8364`, each station builds its dev preset and runs:

```sh
ctest --test-dir <build> --output-on-failure -R "^(core_workbench_interaction|application_workbench_host|sqlite_workbench_settings_storage|qt_workbench_window_scale_(100|125|200)|qt_epoch_comparison_scale_(100|125|200)|qt_workbench_focus_order_scale_100|qt_composed_github_scale_100|application_uiforetw_selected_diff_contract|application_review_round_trip_composition_contract|test_network_isolation_manifest)$"
```

- **Linux (bpr-main-linux): owed.** Expect every row to pass. Record the
  measured folded and open heights from
  `<build>/tests/end_to_end/uiforetw-selected-diff-contract/data/evidence/light/capture-facts.json`
  (`comparison_rect_in_window.height` and `updates_line_open_comparison_height`).
  If they are not 884 and 850, the floors in `src/app/src/github_application.cpp`
  and `tests/end_to_end/application_uiforetw_selected_diff_contract.cmake`
  take the measured values, with the pins resealed.
- **Windows (bpr-main-win): owed.** The same command, plus
  `qt_workbench_strip_menu` and `application_windows_title_row`. Record both
  heights. The open floor stays the owner's 850; if the open line takes Windows
  below it, the failure line names the stack, and that is a decision for the
  owner, not a floor to lower here. Folded is the open height plus 34 by
  construction.

A failure on either is this card's defect.

### Owner asks (for the attended look BPR-1135 needs; no session prepared)

Each ask is one question about one criterion.

1. **Ask first: where the readiness words go** (the card's own question,
   before [[BPR-0795]]'s row changes). While a pull request loads, should the
   readiness words take the Updates line's track when it is open, or the review
   bar's summary when it is folded, so that no row comes or goes above the
   comparison? Until you answer, the readiness row stays between the bar and
   the line.
2. **The fold works from the review bar.** At 1280x852 on macOS and on Windows,
   open and folded: does "▸ Updates" at the bar's left read as the disclosure
   you chose, with the 34-pixel timeline appearing below the bar and the bar
   not moving?
3. **Incomplete history shows a mark.** With history incomplete, a ring with an
   exclamation point sits at the line's start and after "Updates" on the bar,
   and the statement is the tooltip. Is that mark clear enough to say
   "history incomplete"?
4. **The line carries no permanent caption.** The line's "pair ready" words
   left its row with the caption, and their sentence is now the comparison
   menu's tooltip; loading and an unreadable pair stay on the readiness line.
   Is that acceptable, or do you want those words back on the bar?

[[BPR-1129]]'s fourth ask (the comparison 6 pixels shorter until this card) is
answered by the measurement above: 879 folded and 845 open, against 829.

## Owner verdicts — 2026-09-25 (attended session 06ad5734, macOS)

Packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md),
session `06ad5734ed9f7c2fd6d0a62c8328fa0a`, hand-back at sequence 54. The
questions were carried verbatim, with identical fingerprints, from session
`60bd1ed5`, which still lists them unanswered. They bind the frozen m2 app:
source `ba9d2723`, executable `a2e5128a…8a83`. Every decision is `actor:
human`, and its fingerprint equals the prepared one. This card's acceptance
above is unchanged; the work the verdicts ask for is on new cards.

| Ask | Candidate, decision | Verdict, verbatim | Disposition |
|---|---|---|---|
| 1. Where the readiness words go | `BPR-1130-readiness`, `5b349ec3`, seq 44 | "Move to the track when open and summary when folded" | Answered before [[BPR-0795]]'s row changes; [[BPR-1201]] builds it |
| 2. The fold | `BPR-1130-fold`, `74ee4e1c`, seq 41 | "Keep this fold and disclosure" | Accepted on macOS |
| 3. The incomplete-history mark | `BPR-1130-history`, `2edcf8e2`, seq 42 | "The history mark is clear enough" | Accepted on macOS |
| 4. Pair-ready words | `BPR-1130-pair-ready`, `ccba3a15`, seq 43 | "Keep pair-ready words in the tooltip" | Accepted on macOS |

The note on ask 3, verbatim, is about the review itself: "Metafeedback: Most
of these attended review questions seem like simple accept/reject review
critera would be sufficient, not sure why they are being presented as multiple
option select responses." It is recorded as a rule for preparing attended
questions in `AGENTS.md`.
