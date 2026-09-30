---
schema: "implement-rfc.card.v1"
id: "BPR-1243"
title: "Three repository contract rows are red at main: format, dependency modernization and contained temporaries"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"full dev run of the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1121","BPR-0653"]}
acceptance: [{"criterion":"The three rows pass on macOS","outcome":"pass","revision":"94250ab9c","evidence":{"kind":"command","detail":"coding_format_check, dependency_modernization_contract and contained_temporary_usage_contract pass after the pull that brought BPR-1231; application_review_round_trip_composition_contract passes after the reviewed reseal."}},{"criterion":"The courses whose scripts changed still pass","outcome":"pass","revision":"94250ab9c","evidence":{"kind":"command","detail":"application_private_review_native/file/retry pass; skill_pr_tour_package script passes in a BPR_BUILD_TOUR_CLI scratch build; the two tour CLI terminal courses fail identically with their unchanged scripts (control), filed as BPR-1249; full dev build at 625a74087 compiles the formatted sources."}},{"criterion":"Documentation gate","outcome":"pass","revision":"94250ab9c","evidence":{"kind":"command","detail":"python3 scripts/verify_documentation.py --root . -> DOCUMENTATION_OK"}}]
---

# BPR-1243: Three repository contract rows are red at main: format, dependency modernization and contained temporaries

## Observation

The full dev run at `e9e65eab2` plus the milestone's first fixes, macOS arm64,
failed three repository contract rows whose causes are unrelated to each other
and to the product:

- `coding_format_check`: seven files committed between 2026-09-26 and
  2026-09-29 are not clang-formatted (`private_review_workflow.cpp`,
  `review_comment_placement.cpp`, `review_tour_import.hpp`,
  `review_cycle_feedback_tests.cpp`, `gh_cli_transport_tests.cpp`,
  `live_review_{mutation,submission}_provider_tests.cpp`), all declaration
  alignment.
- `dependency_modernization_contract`: "the validator's package list is gone".
  d62902a86 made `bpr_validate_qt_prefix` build its package list
  (`set(bpr_qt_packages …)`, the Gui/Widgets pair appended under
  `BPR_BUILD_QT`) for the non-GUI tour runtime; the test still looks for the
  old `foreach(package IN ITEMS …)` literal.
- `contained_temporary_usage_contract`: five Python courses added since the
  rule create their roots with `tempfile.TemporaryDirectory`
  (`private_review_public_path.py`, `verify_logging_disk_full.py`,
  `pr_tour_skill_tests.py`, `tour_cli_commands_tests.py`,
  `tour_cli_publication_tests.py`).

## Outcome

Each row passes with its cause fixed where it lives: the files formatted, the
modernization test reading the validator's list in its current form while
still requiring all six packages for a Qt build, and the five courses using
`scripts/contained_temporary_support`.

## Direct acceptance

- The three rows pass on macOS.
- The courses whose scripts changed still pass.
- `python3 scripts/verify_documentation.py --root .` passes.

## Resolution (macOS arm64, 2026-09-30)

`94250ab9c`.

| Row | Fix | Result |
|---|---|---|
| `coding_format_check` | clang-format 23.1.1 on the seven files; every hunk is declaration alignment. `private_review_workflow.cpp` is in the pinned production manifest, so the commit carries a reviewed reseal (formatting only; `github_application.cpp` and the mutation pin unchanged) | Pass; `application_review_round_trip_composition_contract` passes |
| `dependency_modernization_contract` | reads `set(bpr_qt_packages …)`, the `list(APPEND …)` under `if(BPR_BUILD_QT)` and `foreach(package IN LISTS bpr_qt_packages)`; still requires `Qt6 Qt6Core Qt6Network Qt6CoreTools` plus `Qt6Gui Qt6Widgets` for a Qt build | Pass |
| `contained_temporary_usage_contract` | four courses use `contained_temporary_directory`; [[BPR-1231]] had already converted `private_review_public_path.py` on the Windows station, so this card keeps its version | Pass |

The full `cmake --build --preset dev -- -k 0` at `625a74087` compiles the
formatted sources with no failure. Of the changed scripts,
`application_private_review_{native,file,retry}` pass, `pr_tour_skill_tests.py`
passes against a `BPR_BUILD_TOUR_CLI` scratch build, and
`verify_logging_disk_full.py` imports and binds the helper (it is a manual
Linux disk-full course, not registered). `tour_cli_publication_tests.py` and
`tour_cli_commands_tests.py` fail in that scratch build exactly as they do
with their pre-change scripts; that is [[BPR-1249]].
