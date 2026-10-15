---
schema: "implement-rfc.card.v1"
id: "BPR-1529"
title: "Draw Review's file-tree folders in Overview's folder colour"
status: "Parked"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"owner request 2026-10-15","owner_note":"For the file tree in review can we draw folders different to files, maybe the same color as they are in overview","governing_rfc":"RFC-BPR-0028","related_cards":["BPR-0658","BPR-1213","BPR-1331","BPR-1363"],"reservation":"docs/issues/_reservations/BPR-1529--BPR-1529--1cf250f53166.json","requested_skill":"frontend-design"}
parked_on: "human-review"
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"command","detail":"qt_files_tree test_folders_take_overviews_folder_ink: drawer folder names and Overview folder paths in PlaceholderText, no Text pixels, in Light, Dark, HC Light, HC Dark; file name keeps Text. Control with old Text ink fails."}},{"criterion":"AC2","outcome":"pass","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"command","detail":"qt_files_tree, qt_files_tree_folder_rows_125/200, qt_pull_request_overview_page and folder_rows_125/200, qt_workbench_window_scale_100/125/200, qt_composed_github_scale_100/125/200, application_github_public_path_nested_folders_v1: 13/13 pass."}},{"criterion":"AC3","outcome":"pending","revision":"0e4d1f89385ca1cbcd64113908d8717b3eda4324","evidence":{"kind":"freeform","detail":"Owner Accept/Reject pending in fresh session 9b4f7e98ea5592bf28d39c59d06f3949, candidate BPR-1529-folder-colour-v001, fingerprint bd812f36f7f9b22da4386d2ae80c6903bfd935066bb688f72bd0e7f57f0ce264, cursor 6; frozen root bpr1529-folder-look-v001.r0_305or."}}]
---

# BPR-1529: Draw Review's file-tree folders in Overview's folder colour

The owner asked on 2026-10-15: "For the file tree in review can we draw
folders different to files, maybe the same color as they are in overview".

The file tree in Review is the files drawer's tree ([[BPR-0658]],
`files_tree_view`). Its folder rows are DemiBold but draw their names in the
same ordinary text ink as file names, so the two read alike. Overview's flat
folder rows ([[BPR-1213]], `overview_row_delegate::paint_folder`) draw their
paths DemiBold in the palette's `PlaceholderText`, the muted ink the derived
appearance palette sets for every variant as the midpoint of text and base.

Governing design: [RFC-BPR-0028](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)'s
files drawer, recorded in the
[File drawer specification](../../ux/workbench-specification.md#file-drawer).
The tree's structure, order, compaction, counts, file marks, filter,
expansion, selection, focus, keyboard behaviour and accessible names are
unchanged; only a folder name's ink changes.

## Acceptance

- AC1: In the tree order, an unselected folder row draws its name in the
  palette's `PlaceholderText`, exactly the ink Overview's folder row draws its
  path in under the same appearance profile, and DemiBold as before. A file
  row keeps ordinary `Text` ink. This holds in Light, Dark, High Contrast
  Light and High Contrast Dark; a check against the old `Text` ink fails.
- AC2: Folder counts, file marks, operation and line-count colours, the unread
  dot, the selected row's `HighlightedText`, row geometry, the flat and
  changed-first orders, and the tree's accessible names and keyboard
  behaviour are unchanged.
- AC3 (human): The owner accepts the exact frozen candidate's Review file
  tree, shown before and after in offscreen Light and Dark renders beside an
  Overview render for comparison.

## Implementation

Load frontend-design:frontend-design before design work. Qt presents: the
change is `files_tree_delegate::paint`'s folder label ink, reading the same
palette role Overview reads; no core, host, provider, storage or new
appearance role. Never drive or capture the owner's desktop; renders are
offscreen. Visual judgement goes through a fresh short attended-review session
and a frozen build.

## Checkpoint — candidate v001, 2026-10-15

Product candidate `0e4d1f89385ca1cbcd64113908d8717b3eda4324` on branch
`bpr-1529-folder-colour` (worktree
`/Users/jaeger/.codex/worktrees/bpr1529-folder-colour/BetterPullReview`),
baseline `1c2fee23b`. The commit also carries [[BPR-1530]], which the owner
asked for on the same tree; each card has its own question. Not merged to
main until the owner accepts. Exact paths, commands and identities are in
[the checkpoint](../_checkpoints/BPR-1529/handoff.json).

Design (frontend-design loaded first): `files_tree_delegate::paint` draws
an unselected folder's name in the palette's `PlaceholderText`, read from the
same colour group Overview's `paint_folder` reads, and keeps DemiBold. File
names keep `Text`, selection keeps `HighlightedText`. No new colour, role or
setting. A folder glyph was considered and left out: the owner asked for a
colour, and a glyph would move every name and not match Overview.

That ink is Overview's existing folder ink, and as text on the tree's base it
reads 3.16:1 Light (`#8f9193`), 4.15:1 Dark (`#747679`), 4.0:1 High Contrast
Light and 5.24:1 High Contrast Dark (`#7f7f7f`): below 4.5:1 in three
variants, as Overview's folder rows already are. The owner's question states
this.

| Acceptance | Evidence at `0e4d1f893` | Result |
|---|---|---|
| AC1 | `qt_files_tree`: new `test_folders_take_overviews_folder_ink` paints the drawer and Overview under one profile in all four variants; folder names and Overview's folder paths have `PlaceholderText` pixels and no `Text` pixels, a file name has `Text`; control with the old `Text` ink fails "a drawer folder's name is not drawn in the file ink" | Pass |
| AC2 | Existing `qt_files_tree` row-ink, mark, selection, accessibility and gesture tests; `qt_files_tree_folder_rows_125/200`; `qt_workbench_window_scale_100/125/200`; `qt_composed_github_scale_100/125/200`; `qt_pull_request_overview_page`; `application_github_public_path_nested_folders_v1` (13/13) | Pass |
| AC3 human | Fresh session `9b4f7e98ea5592bf28d39c59d06f3949`, candidate `BPR-1529-folder-colour-v001`, fingerprint `bd812f36…`, ready and protected at cursor 6, Light and Dark before/after renders of the tree plus Overview, and the frozen build | Pending |

Gates: clang-format 22.1.8 clean on the three touched C++ files; clang-tidy
with warnings as errors passed on both touched translation units;
documentation gate `DOCUMENTATION_OK` and its 67 tests pass. Frozen root
`/Users/jaeger/.codex/attended-builds/bpr1529-folder-look-v001.r0_305or`;
its own `--check` reached `ATTENDED_FILE_SHOWN:src/engine/renderer.cpp`;
executable SHA-256
`840c9d325a6fef29d0b840d0f78b6fb472b9b9a8dcfc8413e77eec113e77b764`. The
review server (pid 48572) is handed to the coordinator. No owner desktop was
driven or captured; all renders were offscreen.

Next: on hand-back after cursor 6, an Accept (with BPR-1530's verdict)
merges the branch and finishes the card; a Reject's note becomes v002.
