---
schema: "implement-rfc.card.v1"
id: "BPR-2324"
title: "A pull request opened again brings back the tabs it had"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-20: \"It would be nice to remember what tabs we had open when we go back to a PR.\"","reservation":"docs/issues/_reservations/BPR-2324--BPR-2324--6174a9a6ce71.json","related_cards":["BPR-1131","BPR-1132","BPR-1475","BPR-1573","BPR-2166"],"related_rfcs":["RFC-BPR-0052","RFC-BPR-0025","RFC-BPR-0128"],"frontend_design":"loaded: frontend-design:frontend-design; no new surface, the existing tab strip shows what comes back"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^core_document_tabs$'", "detail": "test_a_reopened_pull_request_keeps_order_active_pairs_and_readings: the same comparison brings every file tab back as remembered (order, active tab, own pair, readings). Report tabs are not persisted (core::persisted, RFC-BPR-0128 Q10, unchanged)."}}, {"criterion": "AC2", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_document_tabs|application_document_tab_host|application_reopened_tabs)$'", "detail": "Core: a newer accepted comparison is taken by tabs that followed the window, their readings left behind; an own pair keeps pair and reading in its catalog, takes the accepted comparison in another; a file chosen first activates its tab or opens after the active one (at the bound, the active tab shows it). Host: outside Review nothing loads and entering Review loads the active tab; in Review the chosen file loads at once. Composed course (profile desktop, profile_window_restoration_v1): three tabs, the middle active, Close review, opened again by the bar; the strip holds review.cpp, secondary.cpp*, secondary.cpp, and in Review secondary.cpp is shown; after a restart on the same data root the same. Control: with the window's hook disabled the course fails at 'the reopened pull request brings its tabs back once its files are known'."}}, {"criterion": "AC3", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(core_document_tabs|application_document_tab_host|application_reopened_tabs)$'", "detail": "Core: a tab whose file is gone is dropped, the dropped active tab's next neighbour (or previous, when it was last) is active, nothing left counts all dropped. Composed course: a fourth remembered tab on gone-since.cpp is dropped with no message (restored 3, dropped 1); after the restart it stays dropped (restored 3, dropped 0)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(application_reopened_tabs|core_window_review_restoration|application_window_review_restoration|application_profile_window_review_restoration_public_path)$'", "detail": "The tabs live on the recently viewed entry in the existing window restoration store (no schema change): sixteen tabs a pull request, ten pull requests. The window holds the entry's tabs (RememberedTabs) and retain_tabs saves nothing over them until they have come back; the restart leg of the composed course reads them back. Resume's restart course passes unchanged (145 s)."}}, {"criterion": "AC5", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(application_document_tab_host|core_operation_diagnosis|quill_logging|quill_logging_content_policy|diagnostic_controls|application_release_maintenance)$'", "detail": "One Standard workbench.tabs succeeded line per reopening: 'action=tabs_restore failure=none process=none exit=none duration_ms=0 count=2 written=0 read_back=0 tabs_dropped=1'; nothing when nothing was remembered or the reopening is refused. The line parses only exactly; a tabs line on another operation or with anything after its counts is dropped by the logger; the export keeps the counts-only line under category workbench under either policy and drops a line carrying a path."}}, {"criterion": "AC6", "outcome": "pass", "revision": "367e96972", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(architecture_boundaries|core_dependency_boundary|architecture_boundary_contract)$'; clang-format 22.1.8 --dry-run --Werror on the 22 touched C++ files; python3 scripts/verify_documentation.py --root .", "detail": "architecture_boundaries and core_dependency_boundary pass at 367e96972, architecture_boundary_contract passed once on this task's pre-rebase tree (424 s); this card changes none of its inputs (cmake/VerifyArchitectureBoundaries.cmake, tests/scripts/architecture_boundary_tests.py); format clean; DOCUMENTATION_OK. The coding-format-check target was not used: its file walk skips every path under a build/ folder, where this task's worktree lived, so it checks nothing there."}}]
---

# BPR-2324: A pull request opened again brings back the tabs it had

## Outcome

The owner, 2026-11-20: "It would be nice to remember what tabs we had open
when we go back to a PR."

Today only Resume on a Recently viewed row brings a pull request's tabs back
([[BPR-1131]], [[BPR-1132]]). Every other way back (the bar, a link, Open on a
Recently viewed row, opening it again after Close review) starts with no tabs,
and the first comparison the window accepts then saves that empty set over
the remembered one, so the tabs are lost for Resume too.

With this card, each pull request remembers its tabs. When the reviewer comes
back to it by any path, in the same session or after a restart, its tabs
return in their order, with the same tab active.

## Acceptance

- AC1 Per pull request, the window remembers its open document tabs: every
  file tab, in order, which one is active, each tab's comparison when it has
  its own, and each file tab's reading position (the line it was on). Report
  tabs stay out, as RFC-BPR-0128 Q10 has it: a report comes back through its
  card. Overview, Discuss and Threads are pages, not tabs, and the drawer's
  tabs are not per pull request, so neither is remembered here.
- AC2 Opening the pull request again by any path, in the same session or
  after a restart, brings the remembered tabs back in their order with the
  same tab active. A tab that followed the window's comparison takes the
  comparison the reopened pull request accepts. It keeps its reading position
  only when that comparison is the one it was remembered with. A tab with its
  own pair keeps that pair and its position. A file the reviewer opens before
  the tabs return is not lost: it becomes the active tab.
- AC3 A remembered tab whose file is not in the pull request's current
  comparison is dropped without a message. The active tab then falls to its
  next surviving neighbour, or the previous one when it was last. The drop is
  logged.
- AC4 The tabs are stored with the pull request's local state: the profile's
  window restoration store, on its Recently viewed entry. That is bounded:
  sixteen tabs a pull request and the ten most recently viewed pull requests.
  Opening the pull request does not overwrite the remembered tabs with an
  empty set before they are brought back.
- AC5 Each return of remembered tabs writes one Standard diagnostic line,
  `workbench.tabs` with `action=tabs_restore`. It carries the counts restored
  and dropped and nothing else: no path, file, repository or comparison. It
  therefore reaches an anonymized export as counts only.
- AC6 Tests: the core policy (order, active tab, dropped tabs, comparison and
  position rules, a file chosen first, refusals); the host (the restore, the
  diagnostic line, no load while the window is not in Review); persistence
  across a restart through the composed application; and a Qt course checking
  the strip's order and active tab after the pull request is opened again,
  including a dropped tab. `architecture_boundaries`, the format check and
  `python3 scripts/verify_documentation.py --root .` pass.

## Boundaries

The core decides which tabs come back (`src/core`). The application host
restores them and writes the line (`src/application`). The composition holds
the remembered set until the file list is known and leaves it unchanged in
the store until then (`src/app`). Qt shows what the core published. Resume
keeps its exact course ([[BPR-1131]]), which restores the remembered comparison
itself. No new store, table or schema version is needed.

## Execution record

Done on macOS 26 arm64, `build/dev` Debug, at `367e96972` (rebased onto main
after BPR-2322 and BPR-2325).

### What changed

- **Core.** `document_tabs.hpp`: `reopen_tabs` decides what a pull request
  opened again brings back, and `restore_reopened` restores it, loading the
  active tab only where a tab is shown. `operation_diagnosis`: a
  `workbench_tabs` family and a `tabs_restore` action whose line carries
  `count` (restored) and `tabs_dropped`.
- **Application.** `document_tab_host::reopen` runs the decision, publishes
  and persists the result and writes the line. The host takes the profile's
  diagnostic log. `workbench.tabs` (category `workbench`) joins the logged
  operations, and the logger admits the line only exactly.
- **Composition.** A window adopting a pull request it was not showing holds
  the tabs its recently viewed entry remembers. It brings them back from the
  tab observation once the accepted comparison's file list is known, with a
  file the reviewer opened first as the active tab. Until then it saves
  nothing over them. Resume and Close review let them go.
- **Not here.** Report tabs (RFC-BPR-0128 Q10 stands); the page (Overview,
  Discuss, Threads) the pull request opens on, which stays the bar's
  Overview; drawer tabs. A pull request that has left the ten most recently
  viewed has nothing remembered.

### Verification

| Check | Result |
| --- | --- |
| `core_document_tabs`, `application_document_tab_host`, `core_operation_diagnosis` | Pass |
| `quill_logging`, `quill_logging_content_policy`, `diagnostic_controls`, `application_release_maintenance` | Pass |
| `application_reopened_tabs` (new composed course: same session and restart) | Pass (15 s) |
| Control: the window's hook disabled | `application_reopened_tabs` fails at the reopening |
| `core_window_review_restoration`, `application_window_review_restoration`, `application_profile_window_review_restoration_public_path`, `application_close_review` | Pass |
| `architecture_boundaries`, `core_dependency_boundary`; `architecture_boundary_contract` (once, on the pre-rebase tree; this card changes none of its inputs) | Pass |
| clang-format 22.1.8 on the touched files; `verify_documentation.py` | Clean; `DOCUMENTATION_OK` |

Found on main and not this card's: `test_helper_tour_round_diagnostics` does
not compile (`application::cycle_store_failure` left with BPR-2274), and
`test_network_isolation_manifest` fails on six other cards' rows with no
Windows pin and on `gh_cli_git_smart_transport`. This card's
`application_reopened_tabs` carries its pin.

### Owed elsewhere

Windows and Linux runs of `core_document_tabs`, `application_document_tab_host`,
`core_operation_diagnosis` and `application_reopened_tabs`. The code has no
platform branch. The owner's own look at a reopened pull request in the app
was not asked for.
