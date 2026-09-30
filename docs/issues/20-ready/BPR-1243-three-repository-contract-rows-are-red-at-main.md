---
schema: "implement-rfc.card.v1"
id: "BPR-1243"
title: "Three repository contract rows are red at main: format, dependency modernization and contained temporaries"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"full dev run of the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1240--BPR-1243--20fe98015ceb.json","related_cards":["BPR-1121","BPR-0653"]}
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
