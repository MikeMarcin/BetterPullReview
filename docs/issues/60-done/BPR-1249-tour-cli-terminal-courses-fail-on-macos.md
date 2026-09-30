---
schema: "implement-rfc.card.v1"
id: "BPR-1249"
title: "Two tour CLI terminal courses fail on macOS in a BPR_BUILD_TOUR_CLI build"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"found by the main stabilization milestone while checking BPR-1243's script changes, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json","related_cards":["BPR-1243"]}
acceptance: [{"criterion":"tour_cli_terminal_publication and tour_cli_process_commands pass on macOS with -DBPR_BUILD_TOUR_CLI=ON","outcome":"pass","revision":"c3df92bf5","evidence":{"kind":"command","detail":"build/dev-tour-cli (dev preset + BPR_BUILD_TOUR_CLI=ON): ctest 6/6 tour CLI rows pass; both scripts also pass run directly with macOS's default TMPDIR, where they failed before the fix."}},{"criterion":"The cause of each is fixed where it lives","outcome":"pass","revision":"c3df92bf5","evidence":{"kind":"observation","detail":"Editor: product defect in run_tour_editor (projection folder under the non-canonical /var spelling refused by create_storage_root_owner_private after editing), fixed in tour_editor.cpp. Publication: course root under /var refused by the stores by design, fixed in the course by resolving the root."}}]
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

## Resolution (red-test backlog milestone, macOS arm64, 2026-09-30)

`c3df92bf5`. Both failures reproduce only when the scripts run outside the
test runner. Under `ctest` the runner supplies a canonical temporary root, and
all six tour CLI rows pass, both at `89e993254` (a scratch worktree build) and
at main. Run directly, as a person runs `bpr tour`, `TMPDIR` is macOS's
per-user `/var/folders/…/T`, and `/var` is a link to `/private/var`.

| Course | Cause | Fix |
|---|---|---|
| `tour_cli_process_commands` (`success`, exit 7) | **Product defect.** `run_tour_editor` makes its projection folder under `QDir::tempPath()` and, after the editor exits, checks it with `create_storage_root_owner_private`, which accepts only a path whose canonical spelling is its own. Every `bpr tour edit` on a Mac with the default `TMPDIR` was refused as "The private projection directory changed while editing" | The folder is made under the canonical temporary path (`QFileInfo::canonicalFilePath`) |
| `tour_cli_terminal_publication` (`publish`, SIGABRT at `public open`) | The course gave the tour store a root under the `/var` spelling, which the stores refuse by design | The course resolves its root, as other courses already do |

| Check | Result |
|---|---|
| Both scripts run directly, before the fix | Fail as recorded above (the failing control) |
| Both scripts, `pr_tour_skill_tests.py` and `tour_cli_tests.py` run directly, after | Pass |
| `ctest --test-dir build/dev-tour-cli -R 'tour_cli|skill_pr_tour'` (dev preset plus `-DBPR_BUILD_TOUR_CLI=ON`) | 6 of 6 pass |
| `application_review_round_trip_composition_contract` and the dev `tour` rows | 24 of 24 pass |

For the Windows station: `QFileInfo::canonicalFilePath` of the temporary path
is now the editor projection's parent there too. A Windows run of
`tour_cli_process_commands` is owed.
