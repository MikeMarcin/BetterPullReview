---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-K2"
title: "Read a split file's per-commit diffs, each with its own Seen and Read"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-PENDING-K1","BPR-1686"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"milestone":"RFC-BPR-0089 M3","owner_decision":"Q2 (504cfd1b): one row, one exact diff per shown commit, the hidden one named","frontend_design":"frontend-design:frontend-design loaded before the strip over a split file's diff was drawn","related_cards":["BPR-PENDING-K1","BPR-PENDING-K3","BPR-1693"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|application_commit_filter_model|qt_commit_filter_view_scale_(100|200))$'","detail":"Fixture, shipped composition, offscreen: commit-filter-split-v1 (scripts/build_commit_filter_fixture.py --split; src/version.txt changed by C1, C2 and C3). Hide C2 leaves 5 files, the Commits tab says 1 file has a hidden commit between two shown ones, and the Files drawer and the walk list src/version.txt once. Opening it shows a strip over the diff, '2 diffs, one per shown commit', naming 91bd0b2 Change the generator input, then '798cde7 Regenerate also changed this file; hidden', then 968f892 Follow-ups. The first diff is git diff C1^:P C1:P (merge base to C1 blobs; version 1 -> version 2 only), the second, chosen from the strip, is git diff C3^:P C3:P (C2 to C3 blobs; version 2 -> version 3; C2's stamp in neither change). Every shown blob is the file at a commit of the chain; negative control: the blob of the tree a cherry-pick of C1 and C3 would make ('version 3 / stamp none') is in no commit and the course's check refuses it. Negative control: without the composition's segments the course fails at opening the file. Full-window render inspected at 1280x860: the strip's wording shortens to fit the diff's width. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_commit_filter_public_path|qt_commit_filter_view_scale_(100|200))$'","detail":"Same course: presenting the first diff marks only it seen; Mark read on the second reads only it, and the Overview row is not read while one diff is only seen; once both are read the row is read. Each diff's receipt is keyed by the file and its own two commits (filtered_receipt_file), so neither is Whole PR's: after Clear all filters the file reads its Whole PR diff, with no strip, and is not read. The split file stays in the walk until the reviewer leaves it (its actionability is not taken from one diff's receipt). Qt: the strip names each diff and how far it is read, checks the one on screen only when the host says so, and shortens its wording instead of widening the window. macOS."}},{"criterion":"AC3","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"python3 tests/end_to_end/rfc0089_commit_filter_live_course.py --build build/dev --work DIR --prefix rfc0089-m3-split1 --cleanup","detail":"Live, MikeMarcin/BPR-Test #114 (closed, both branches deleted), signed in through gh as MikeMarcin, sparse review: the owner's shape (C1 source, C2 regenerating 320 files and src/version.txt, C3 mixed, 325 files). Under Hide C2 src/version.txt kept one row and read 2 per-commit diffs, C1's and C3's, each with endpoints C^..C, both blobs and every removed and added line equal to git diff C^ C -- P, every blob in a commit of the chain, and C2 named between them; Show only C1, C2 and C3 matched git as before. GitHub's Viewed state was unchanged for all 325 files (AC05). X-RateLimit-Remaining 4966 before, 4870 after. Run on this tree before two clang-tidy renames (a local name in the strip and in the course) and one string build in the course binary, none of which changes behavior; the composed courses ran on the committed tree."}}]
---

# BPR-PENDING-K2: Read a split file's per-commit diffs, each with its own Seen and Read

RFC-BPR-0089 M3, "Each file's diff is exact", owner decision Q2: a file a
hidden commit changed between two shown commits keeps one row and shows one
exact diff per shown commit that changed it, in chain order, with the hidden
commit named in place, each its own material for receipts. Until now such a
file read nothing and said why. The `frontend-design:frontend-design` skill
was loaded before the strip was drawn.

## Change

- **Model.** A split `commit_filter_file` carries its `Segments`: each shown
  commit's own diff of the path (the commit's parent to the commit), its own
  change kind from that commit's tree diff, and the hidden commits after it.
  `split_parts` names them for the strip.
- **Workbench host.** A split `workbench_file_observation_entry` carries its
  segments (`workbench_split_segment`). The host reads the chosen one
  (`select_split_segment`), with the segment's endpoints as the request's
  filter and the segment's own change; another file opens at its first
  diff, and preparation prepares the first. The epoch host reads a split
  file's diff when the request names a segment, through the endpoints the
  chain's touch job authorized; without one it still reads nothing.
- **Receipts.** Each diff's receipt is the file and its two commits, so Seen,
  Read and Accept are each diff's own. The Overview's row combines them with
  `combined_split_receipt`; a split file stays in the walk (its actionability
  is not one diff's receipt).
- **Qt.** `split_diff_strip`, over the diff and under the document tabs,
  names each diff ("91bd0b2 Change the generator input (seen)") and each hidden
  commit in place ("798cde7 Regenerate also changed this file; hidden", quieter
  and slanted so it never reads as a button). The host's projection says which
  diff is on screen. On a narrow diff the wording shortens (elided headlines,
  hidden commits by ID, then IDs only); the strip never widens the window. It
  takes no room while hidden, so every other diff keeps its geometry.
- **Fixture.** `commit-filter-split-v1` (`--split`): `src/version.txt` changed
  by all three commits; 99 files.
- **Live course.** `--course commit-filter` now reads each segment of a split
  file and the script checks each against `git diff C^ C`.

Shared files touched: `github_application.cpp` (pins resealed),
`workbench_window`, `workbench_application_host`, `epoch_application_host`,
`workbench_runtime_ports.hpp`, `commit_filter_view`.

## Acceptance

- **AC1.** RFC AC03 on the fixture: one row, `git diff C1^:P C1:P` and
  `git diff C3^:P C3:P` with C2 named between them, no shown blob absent from
  every commit of the chain (negative control: a synthesized tree's blob).
- **AC2.** Each per-commit diff has its own Seen and Read (owner, Q2).
- **AC3.** RFC AC03 live on BPR-Test, through the shipped composition.
