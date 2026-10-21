---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-Q"
title: "A read file the reviewer leaves returns to the Files drawer"
status: "Done"
rfcs: ["RFC-BPR-0007"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Observed by the BPR-1701 implementer, 2026-10-21: on nested-folders#21, with the third file read, the Files drawer listed 16 files and not docs/api/v2/endpoints.md, and Enter on that file's Overview row left Review on the file already open.","governing_rfc":"RFC-BPR-0007","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028"],"related_cards":["BPR-0228","BPR-0972","BPR-1339","BPR-1701"],"regressed_by":"78d4fa5ca93b88e107d7c31c86a8d8f98d459ae4"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "93da02e4adc19a8549f1026d1802ecc4bf61ada0", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^application_accepted_file_in_overview$'", "detail": "ACCEPTED_OVERVIEW_PUBLIC_PATH_OK through the shipped composition, ledger host and SQLite store on nested-folders#21. After the third file (docs/api/v2/endpoints.md) is read and the second is opened by Enter on its Overview row, the flat Files list and the tree each hold all 17 files and endpoints.md; Enter on endpoints.md's Overview row opens it in Review, read; its checked Read step leaves it seen (Overview: seen, 1 of 17 done) and checks it read again (Overview: read, 2 of 17); opening the first file returns it to both lists again. BPR-1701's workaround is gone: the course takes Read off the read third file, as it first meant to, instead of reading the second. Failing-before control: on the same tree without the composition change the course fails at 'the Files drawer lists the read third file once another file is open'; a probe on that tree showed the flat list at 16 rows and Enter on endpoints.md's row leaving Review on docs/api/reference.md. macOS 26 arm64, Qt 6.12 offscreen, dev preset Debug built -O0 -g0, on the branch rebased onto main 1694a7f37."}}, {"criterion": "AC2", "outcome": "pass", "revision": "93da02e4adc19a8549f1026d1802ecc4bf61ada0", "evidence": {"kind": "command", "command": "ctest --test-dir build/dev -R '^(application_header_source_navigation|application_header_source_navigation_large|application_source_pr_find|application_review_ledger_composition_contract|application_workbench_composition_contract|application_review_round_trip_composition_contract|application_profile_multi_window_public_path|application_active_changed_scaling|application_review_navigation_entry|command_correctness_registry_contract|architecture_boundaries)$'", "detail": "All pass. application_header_source_navigation had asserted the bug: an excluded header stayed out of the order after another file was opened. It now requires the header back in the order once src/widget.cpp is shown, and the jump to it to keep the excluded receipt and withhold the shown header again. The round-trip contract's full (ac79ebb3f3e4) and manifest (4d7a78e2bc56) pins are resealed for the one src/app change, with a note at the top; the mutation pin 16d135419d39 is unchanged. The command-correctness registry follows github_application.cpp."}}, {"criterion": "AC3", "outcome": "pass", "revision": "93da02e4adc19a8549f1026d1802ecc4bf61ada0", "evidence": {"kind": "command", "command": "cmake --build build/dev --target coding-format-check; python3 _coding-standard/scripts/tidy.py --build-dir build/dev --warnings-as-errors --paths src/app/src/github_application.cpp tests/end_to_end/accepted_file_in_overview_public_path_tests.cpp tests/end_to_end/header_source_navigation_public_path_tests.cpp; python3 scripts/verify_documentation.py --root .; python3 tests/scripts/verify_documentation_tests.py", "detail": "All pass. clang-format 22.1.8 passes on all 1719 files. clang-tidy 22 --warnings-as-errors passes on the three changed translation units. Both documentation gates pass with the card under a stand-in allocated ID (BPR-1702, the counter moved to BPR-1703 for the run); with the provisional ID the gate refuses only the card's id format, as it did for BPR-PENDING-P."}}]
---

# BPR-PENDING-Q: A read file the reviewer leaves returns to the Files drawer

## What was seen

The [[BPR-1701]] implementer, on `bpr-fixtures/nested-folders#21`: with the
third file read, the Files drawer listed 16 files and not
`docs/api/v2/endpoints.md`, and Enter on that file's Overview row left Review
on the file already open. BPR-1701's course opened another file instead.

## Diagnosis

It is a product bug, not a fixture artefact. The pack's diff
(`c58160c2f..b20cb4434`) has 17 files, endpoints.md among them, added; the
drawer listed all 17 until the file was read. Folder compaction, sorting,
filters and caps played no part.

The Files drawer lists the workbench's actionable order. Since [[BPR-0228]]
the order withholds the file whose retained receipt needs nothing (read or
excluded), and only that file: the observation supplier answers only for the
retained receipt, and unknown or unselected material stays conservatively
actionable ([review ledger](../../architecture/review-ledger.md)). When the
receipt moves to another file, the composition's `publish` sees the file
change and asks the host to restore the withheld one ([[BPR-0972]]'s
`retain_actionability`).

`78d4fa5ca` ("Share accepted private-ledger projections across review
windows", 2026-09-08) made `clear_diff_selection` drop the retained receipt.
That runs whenever a new file's diff begins, so by the time the next file's
receipt is published there is no earlier receipt to compare with, and an
actionable next file asks for nothing. The read file stayed withheld until
something else reobserved the order. Enter on its Overview row resolves the
path through the drawer's list (`workbench_window::file_for_path`), found
nothing, and dispatched no selection, so Review stayed on the file already
open. Found with `git log -S "ReviewLedgerReceipt.reset();"`: before
`78d4fa5ca` only ledger retirement dropped the receipt.

## Fix

`clear_diff_selection`, when the receipt it drops had withheld its file, asks
the workbench host's existing `retain_actionability` to restore it, as an
observation without that receipt would answer. The host applies it as a file
delta, so the list and the tree each take one inserted row. Nothing else
changes: the shown file, once read or excluded, still leaves the order while
it is shown, as BPR-0228 designed.

## Acceptance

- AC1: Through the shipped composition, a read file the reviewer leaves is
  listed again in both Files views and Enter on its Overview row opens it.
  BPR-1701's course uses the read file it first meant to.
- AC2: The courses that withhold files, the ledger and workbench composition
  contracts, the resealed round-trip contract and the boundaries pass.
- AC3: Format, tidy and documentation gates pass.

## Checkpoint

Product commit `93da02e4a` on branch `worktree-agent-a1c375a5fb7648c41`,
rebased onto main `1694a7f37`; this card follows in its own commit. Not landed
on main: the coordinator allocates the ID and renames `BPR-PENDING-Q` in
`src/app/src/github_application.cpp`, the two courses,
`docs/architecture/review-ledger.md` and the round-trip contract's note, then
reseals that contract and the command-correctness registry for the rename.

Changed paths: `src/app/src/github_application.cpp`,
`tests/end_to_end/accepted_file_in_overview_public_path_tests.cpp`,
`tests/end_to_end/header_source_navigation_public_path_tests.cpp`,
`tests/end_to_end/application_review_round_trip_composition_contract.cmake`,
`tests/command_correctness_registry.json`,
`docs/architecture/review-ledger.md`.

Left for the owner: whether a read or excluded file should leave the Files
drawer at all while it is shown. That is BPR-0228's design and this card keeps
it. No attended review is pending. The build tree was removed at the end.
