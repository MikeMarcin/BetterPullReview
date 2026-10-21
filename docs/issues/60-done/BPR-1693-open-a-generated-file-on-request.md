---
schema: "implement-rfc.card.v1"
id: "BPR-1693"
title: "Open a generated file's diff from the generated row on request"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1691"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1689--BPR-1693--627bb0b13945.json","milestone":"RFC-BPR-0089 M2","origin":"RFC-BPR-0089 'Generated files are set aside': 'Expanding it lists them, and opening one shows its diff on request.'","related_cards":["BPR-1691"],"frontend_design":"frontend-design:frontend-design loaded before the generated rows gained Open diff and open-on-activation"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"51106f759ac954c4e72d3b8580fd9869541cc48e","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|qt_commit_filter_view_scale_(100|200))$'","detail":"Shipped composition, offscreen, commit-filter-attributes-v1 (95 files set aside by the head's linguist-generated). In Review, Open diff, first in the Files drawer generated row's menu, on gen/item_000.txt shows its Whole PR diff (generated from value 1 -> 2, checksum 1000 -> 2000), read on request; the Files drawer walks the same three ordinary files, the drawer and Overview counts are unchanged (3 of 98), and the row still lists it as linguist-generated, not reviewed anyway: no mark is written. From the Overview, activating gen/item_001.txt in its generated row enters Review on that file's diff and leaves it set aside. The earlier preparation check still finds no set-aside head blob read. Qt: the menu offers Open diff first when an open dispatch is set, activation opens, a single click opens only where the row opens on click (the Files drawer). Negative control: with the composition's open dispatch disconnected the course fails at 'Open diff shows the set-aside file's diff'. macOS. Tested as 8000ed698 over 111235987; the rebases onto ef5939561 and e9d5297ac brought only the v0.3.2 version and release notes, CLI and Home fixture tests, and main's Qt-only render block in commit_filter_view_tests.cpp, merged with this card's assertions; the rows above pass again on the rebased tree."}},{"criterion":"AC2","outcome":"pass","revision":"51106f759ac954c4e72d3b8580fd9869541cc48e","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_commit_filter_public_path$'","detail":"From the opened set-aside file, Next file shows one of the review's own three files, the walk is unchanged and the file is still set aside behind the generated row. The file is retained outside the walk as a Find in files result is, so it never enters the actionable order, the counts or preparation. macOS."}}]
---

# BPR-1693: Open a generated file's diff from the generated row on request

RFC-BPR-0089 says the generated row lists the files set aside, "and opening one
shows its diff on request", while they stay out of Next file, the counts and
preparation.

## Today

The generated row ([[BPR-1691]]) lists each file set aside and why, and
its context menu offers Review anyway, Show as normal and Follow
.gitattributes. Activating a listed file opens nothing: the workbench reads
only files in its actionable order, and a set-aside file is not in it. Review
anyway is the way to read one now; it returns the file to the walk.

## Change

- **Qt.** The generated row's list, in the Overview and the Files drawer,
  opens a file on activation, and its menu offers Open diff first; in the
  Files drawer a single click opens it, as a click in its tree does. From the
  Overview, Review is entered first.
- **Composition.** `open_set_aside_file` builds the file's workbench entry from
  the commit filter's own record (`application::workbench_file_entry`, the
  snapshot observation's own mapping, with the record's filtered endpoints),
  retains it outside the walk through the workbench host's existing
  source-file path, as a Find in files result is, and opens it in the active
  tab. The diff is read through the epoch host's ordinary selected-diff path.
- The file stays set aside: the actionable order, the counts and preparation
  never contain it, and no generated mark is written. Seen is recorded for the
  diff the reviewer looked at, as for any diff.

Shared files touched: `github_application.cpp` (pins resealed),
`workbench_window`, `generated_files_view`, `workbench_snapshot_observation`.

## Acceptance

- **AC1.** Activating a file in the generated row shows its diff, read on
  request, without adding it to Next file, the remaining counts or
  preparation ahead, and without writing a mark.
- **AC2.** Leaving the file (or the row's list) returns the review to its set;
  the file is still set aside.
