---
schema: "implement-rfc.card.v1"
id: "BPR-1844"
title: "Discuss, Threads and Review Comments open with the filters last chosen, across pull requests and launches"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"feature","found_by":"owner request, 2026-11-03: persistent Discuss, Threads and Review Comments filters across PRs and launches, windows and tabs still free to diverge","reservation":"docs/issues/_reservations/BPR-1842--BPR-1844--749b2d50ac34.json","related_cards":["BPR-0661","BPR-1356","BPR-1808"],"frontend_design":"not loaded: no layout, wording or colour changes; only which filters a view opens with"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "ae892ba94", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_view_filter_memory$'", "detail": "Opening takes each view's memory or nothing, Review's Comments the first-filter preference; a choice changes only its view and is remembered; present_view_filter is not; per-view last-writer-wins merge with two simulated windows."}}, {"criterion": "AC2", "outcome": "pass", "revision": "ae892ba94", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^sqlite_view_filter_memory_store$|^sqlite_profile_data_reset_files$|^sqlite_storage_diagnostics$'", "detail": "Round-trip across reopen; an unknown value degrades its view only and is repaired; version 0 reads as nothing and is rewritten; version 2 reads as nothing, its write is refused and the row is left as it was; a non-database file reads as nothing; Reset settings removes view-filters.sqlite3; storage lines name it."}}, {"criterion": "AC3", "outcome": "pass", "revision": "ae892ba94", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_view_filter_memory_host$'", "detail": "One memory for every window; at most one write running and one waiting; the latest memory is written; shutdown posts the waiting write; failing and newer-build stores keep the session memory; a relaunch reads the written memory."}}, {"criterion": "AC4", "outcome": "pass", "revision": "ae892ba94", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_view_filter_memory_window$|^qt_github_conversation_panel$|^qt_workbench_window_scale_100$|^qt_shared_snapshot_navigation$|^application_profile_(multi_window|window_restart)_public_path$'", "detail": "Two workbench_window instances over one memory: Review Comments, Discuss and Threads presses are remembered and never reach the other window; a later pull request takes each view's latest choice from either window; set_filters is not remembered; the Settings first filter seeds and is remembered. Existing window, panel, finder and composed multi-window and restart courses pass."}}, {"criterion": "AC5", "outcome": "pass", "revision": "ae892ba94", "evidence": {"kind": "command", "command": "cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors on changed TUs; python3 scripts/verify_documentation.py --root .", "detail": "Format passed; tidy clean on every new TU and the panel and finder; the remaining tidy findings in workbench_window.cpp, github_application.cpp and operation_diagnosis.cpp are on lines unchanged from main (finish_choice, closing, go_on_to_reviewers, unsigned_digits); DOCUMENTATION_OK."}}]
---

# BPR-1844: Discuss, Threads and Review Comments open with the filters last chosen

## Request

The owner, 2026-11-03: "I want my Discuss and Threads view and Review
Comments view filter settings to be persistent across PRs and launches
(although still allow windows/tabs to diverge active filters)."

## Interpretation

- **What is remembered.** Only new and Hide resolved, separately for each of
  three views: Discuss, Threads (the finder) and Review's Comments beside the
  code. A view's search, the Threads label filter, Discuss's scope and chosen
  file, and an Overview count's listing name things in one pull request and
  are not carried. Overview's Comments ([[BPR-1808]]) keeps its own filters
  as before.
- **Where.** One memory per profile, in its own
  `state/v1/view-filters.sqlite3`: one row with a version and a nullable
  value per view (NULL: never chosen there). It is a separate file so the
  workbench settings schema is untouched and losing it loses nothing else.
  Reset settings removes it; storage diagnostics name it.
- **When it is read.** When a window opens a pull request (a new window, a
  new pull request in a window, or a relaunch's restored one). A refresh of
  the same pull request (`PreserveWindowNavigation`) keeps the window's own.
- **When it is written.** On a reviewer's own press of Only new, Hide
  resolved, Clear filters (Discuss, Review's Comments) or Show all threads
  (Threads). A change the window makes itself (a reveal clearing a hiding
  filter, a count listing, Show new threads) is shown but not remembered.
- **Windows diverge.** The memory changes at once (last writer wins, per
  view), but is never pushed into a window that is already open. Each window
  keeps its own filters until it opens another pull request. Tabs share their
  window's filters as before; per-tab filter state is not added here.
- **The Settings first filter ([[BPR-0661]]).** Until Review's Comments has a
  remembered choice it opens with that preference. Choosing the preference
  in Settings is remembered as Review's Comments' choice, so the next pull
  request opens with it.
- **Damage and versions.** An unknown stored value leaves only its view
  unremembered (reported as recovered) and the next write repairs it; a
  version older than any known reads as nothing remembered and is rewritten;
  a newer build's row reads as nothing and is never overwritten; a file that
  is not a database reads as nothing.

## Layering

- Core, `core/view_filter_memory.hpp`: the window's filter state
  (`view_filter_state`), `open_view_filters`, `choose_view_filter`,
  `present_view_filter`, the per-view `remember` merge, and the stored row's
  encoding and reading.
- Application, `view_filter_memory_host`: one memory for the profile's
  windows; writes on the workbench effect lane, one at a time carrying the
  latest memory, flushing a waiting write at shutdown; reports durability.
- SQLite adapter, `view_filter_memory_store`: the file and its row.
- Qt: the conversation panel and the finder tell only `clicked` gestures; the
  window applies the opened state and forwards choices. The composition loads
  the memory once at profile start, before any window opens a pull request.

## Acceptance

- AC1: Core transitions: opening takes each view's memory or nothing (Review's
  Comments the first-filter preference); a choice changes only its view and
  is remembered; a window's own change is not; the merge is per view, last
  writer wins.
- AC2: Persistence: the memory round-trips through the SQLite store across a
  relaunch; damaged values, an obsolete version, a newer build's row and a
  foreign file degrade as described without overwriting the newer row.
- AC3: Host: one memory for every window, writes off the decision lane with
  at most one waiting, a waiting write flushed at shutdown, a failing store
  keeping the session's memory.
- AC4: Two composed workbench windows over one memory: choices in each are
  remembered and do not reach the other; a pull request opened afterwards
  takes the latest choice of every view, made in either window; the Settings
  first filter seeds Review's Comments and is remembered when chosen.
- AC5: Format, tidy and documentation gates pass.
