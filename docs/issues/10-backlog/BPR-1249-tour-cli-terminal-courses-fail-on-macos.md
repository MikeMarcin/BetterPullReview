---
schema: "implement-rfc.card.v1"
id: "BPR-1249"
title: "Two tour CLI terminal courses fail on macOS in a BPR_BUILD_TOUR_CLI build"
status: "Backlog"
rfcs: []
depends_on: []
metadata: {"origin":"found by the main stabilization milestone while checking BPR-1243's script changes, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-1243"]}
---

# BPR-1249: Two tour CLI terminal courses fail on macOS in a BPR_BUILD_TOUR_CLI build

## Observation

The dev preset leaves `BPR_BUILD_TOUR_CLI` off, so these rows are not in the
dev suite. In a scratch Debug build of 89e993254 with `-DBPR_BUILD_TOUR_CLI=ON`
on macOS arm64:

- `tests/scripts/tour_cli_publication_tests.py` (`tour_cli_terminal_publication`)
  fails: `('publish', -6, b'FAILED: public open …expectation_failed')`.
- `tests/scripts/tour_cli_commands_tests.py` (`tour_cli_process_commands`)
  fails: `('success', 7, b'… The private projection directory changed while
  editing. Candidate retained at …/T/bpr-tour-edit-…/field.md …')`.
- `pr_tour_skill_tests.py` (`skill_pr_tour_package`) passes.

Both failures are the same with the scripts as they were before BPR-1243
(control run), so they predate it.

## Outcome

Both courses pass on macOS in a tour CLI build, with the cause of each fixed
where it lives.

## Direct acceptance

- `tour_cli_terminal_publication` and `tour_cli_process_commands` pass on
  macOS with `-DBPR_BUILD_TOUR_CLI=ON`.
