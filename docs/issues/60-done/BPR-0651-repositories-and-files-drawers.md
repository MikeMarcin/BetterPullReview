---
schema: "implement-rfc.card.v1"
id: "BPR-0651"
title: "Repositories and files drawers"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"RFC-BPR-0028 AC-09 at 1280x852 and 1680x1000: the repositories drawer","outcome":"pass","revision":"f227072e + this finish","evidence":{"kind":"observation","detail":"The repositories drawer is a third core-decided drawer end to end: core state, delta and command; host persistence with a version-3 SQLite schema and migrations from versions one and two (sqlite_workbench_settings_storage pins both); the window's third panel left of the files with its toggle, glyph, Alt+Shift+R binding, rail and focus stops. qt_repositories_drawer pins the tree from the queue in Home's order with needs-you-of-total, the reason words and the amber state; the composed repositories test pins the drawer's place, the rail count and open-on-Overview; the window test pins toggle-through-core and collapse to the rail. The files tree is carded as BPR-0658 (the flat list is read by about thirty probes) and titles, unread dots and saved repositories as BPR-0659 (no projection carries them); AC-09 remains open for the files tree."}},{"criterion":"Selecting a pull request row opens it on Overview; selecting a file row lands on its first change","outcome":"pass","revision":"f227072e + this finish","evidence":{"kind":"observation","detail":"qt_composed_github_scale_100: choosing a pull request row dispatches open_pull_request_intent with the owner, name and number and the pull request arrives on Overview. The file-row landing is the flat list's existing selection (select_workbench_file_intent) and moves with the files tree under BPR-0658."}},{"criterion":"Focused model and drawer tests; the composed courses that pin the files drawer pass with their evidence repinned","outcome":"pass","revision":"f227072e + this finish","evidence":{"kind":"observation","detail":"qt_repositories_drawer (new), core_workbench_interaction, application_workbench_host, sqlite_workbench_settings_storage, qt_workbench_window_scale_{100,125,200}, qt_composed_github_scale_{100,125,200}, the public-path courses, application_workbench_composition_contract, application_uiforetw_selected_diff_contract (the course collapses the third drawer and its contract pins the rails' order), the queue and ledger composition contracts, application_composition_contract and the round-trip contract with its digests repinned; check_boundaries, strict tidy on every touched translation unit, clang-format --Werror and verify_documentation.py. Recorded in the card's notes."}}]
---

# Repositories and files drawers

RFC-BPR-0028 Drawers, from the Settings-and-drawers asset sections 4 and 4b.
Replace the current files drawer and queue landing with two drawers: the
repositories tree (saved repositories per account, pull requests of interest
in Home's order with reason and unread dot, open on select) and the files tree
(compacted nesting folders, chevrons and guide lines, read-state dots, counts,
three orders, parent-keeping filter). Each has its own strip toggle, shortcut
and rail; they collapse independently. Rows come from the queue, ledger and
snapshot projections; any missing projection is added in the host.

## Acceptance

- RFC-BPR-0028 AC-09 at 1280x852 and 1680x1000.
- Selecting a pull request row opens it on Overview; selecting a file row
  lands on its first change.
- Focused model and drawer tests; the composed courses that pin the files
  drawer pass with their evidence repinned.

## Dependency

- 2026-09-06: `depends_on` cleared from `["BPR-0646"]`. BPR-0646's chrome, epoch line,
  modalities and rails are integrated at f01f34ad; only its attended capture is
  owed and it is parked under human-review (fbc64dad). The board counts only
  Done as satisfying a dependency, and the attended capture is planned as one
  session at the RFC-BPR-0028 pack head, so this card builds on the integrated
  chrome rather than waiting for that session.

## Notes

- Delivered: the repositories drawer, end to end. The core knows a third
  drawer (`workbench_drawer::repositories`, its state, its delta field and
  `toggle_repositories_drawer` as the eighth command; the two-way ternaries
  that named a drawer became switches, so a fourth would be a compile
  error rather than an alias); the workbench host persists its state and
  width (`workbench_layout_settings::Repositories`/`RepositoriesWidth`,
  default 300 as the asset draws it) and publishes the width in the layout
  delta; the SQLite workbench settings store is at schema version 3 with
  `repositories_state` and `repositories_width`, and a version-one or
  version-two database is rebuilt onto it with the drawer open at 300; the
  window carries a third `drawer_entry` (`Alt+Shift+3`, the toggle after
  Home and before Files in the strip with its own glyph, a rail, its
  presented state shown while the core offers no drawer command, four
  splitter columns, its own focus stops) and the width observer names the
  drawer by switch, which is where the first run went wrong: the
  repositories panel's width had been recorded as the conversations'. Two
  more of the same shape followed: the drawer panel and rail named
  themselves by a two-way choice, so the third rail answered to the
  conversations' accessible name, and the workbench host rebuilt the
  drawer delta from two fields, so the core's collapse of the third drawer
  never reached the window and the UIforETW course waited on it forever.
  All four are switches or complete literals now.
- The tree (`repositories_tree_view` over `repositories_tree_model`,
  `src/adapters/qt/repositories_drawer.*`) is built from the queue landing's
  items in Home's order (`core::ordered_queue`): each repository is a bold
  row reading "needs you of total" (or the total alone), its pull requests
  beneath with "#number" and a one-word reason from the bucket (requested,
  changed, replied, verify, waiting, following, quiet), amber when the
  bucket is one that needs me; the rail counts the ones that need me.
  Choosing a pull request row dispatches the same `open_pull_request_intent`
  the locator sends and the pull request arrives on Overview. Pinned by
  `qt_repositories_drawer`, the composed repositories test and the window
  drawer test.
- Not delivered, carded with the findings: the files tree
  ([BPR-0658](../10-backlog/BPR-0658-files-tree-in-the-files-drawer.md); the
  flat list is read by about thirty probes and courses, so its replacement
  is its own slice) and the titles, unread dots and saved repositories
  ([BPR-0659](BPR-0659-repository-projection-with-titles-unseen-and-saved-repositories.md);
  no projection carries them). AC-09 is therefore met for the repositories
  drawer and open for the files tree.
- Pins moved by the third drawer: the strip's row (Repositories between Home
  and Files; the UIforETW probe's twenty-two entries), the splitter's four
  columns and the size lists the drawer tests set, the width floor (900),
  and the collapse test's window, widened to 1600 so three open drawers and
  the centre's minimum leave the collapse arithmetic exact. The binding is
  `Alt+Shift+R`: Alt+Shift+3 to 5 were already the line numbers, review
  controls and review details. The UIforETW course collapses the third
  drawer before its capture and its contract pins the repositories rail at
  the splitter's left edge with the files rail after it; the version-two
  settings store migration is pinned in the SQLite contract.

