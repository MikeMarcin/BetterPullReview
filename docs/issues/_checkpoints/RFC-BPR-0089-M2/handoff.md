# RFC-BPR-0089 M2 checkpoint: generated files and file filters

Assignment: milestone M2 of RFC-BPR-0089 (revision r3), from the RFC
implementation coordinator, after M1 landed on main at `26ea30e7b`. Branch
`worktree-agent-ac4be454b6444a133`, rebased onto main `1204a7194` (after
BPR-1680). Tested code candidate:
`8e296033dd8586fa127a1994f3f03e1ca8fe2697`. The commit that adds this
checkpoint and the cards changes only documentation. Issue IDs are
provisional (`BPR-1689` to `J5`); the coordinator allocates them on main.

`frontend-design:frontend-design` was loaded before the generated row, the
bulk label, the file-menu gestures and the chip's file-filter text were drawn.

## Measured first

GitHub's Files changed honours the head commit's `linguist-generated`, in the
whole pull request and in every per-commit view; the base, merge base, merge
commit and each commit's own rules are not read. BPR-Test #112 (closed, both
branches deleted), six REST calls, `X-RateLimit-Remaining` above 4,990. The
RFC's engineering questions record the probe in full.

## BPR-1692 round (2026-10-21)

The owner answered **Accept**, no note (decision
`62bc634e-549e-4956-9f52-2ebbc6a0d99f`): `linguist-generated` is read with the
core's existing first-party wildmatch and attribute reader, and no library is
adopted. The session is closed (event cursor 8: an observation recording the
decision's use, protection released) and its host is stopped through the
script's own `stop`. [[BPR-1692]] is done on branch
`worktree-agent-a5993382e3cca9089`, code candidate
`879fda2fbbb0281459db0093fa15386278ac126f` over main `21fae3b25`, docs on the
commit after it; not landed on main. It was tested as `f3cabb334` over
`5e4c77757`; the rebase onto `21fae3b25` brought in only two card files, so the
code tree is byte-identical.

- Verification on that code tree (lean `-g0` dev build, removed afterwards):
  `core_generated_files`, `core_commit_filter`, `core_review_cycle_manifest`,
  `application_commit_filter_model`, `application_pull_request_overview`,
  `application_epoch_host`, `application_epoch_contract`,
  `application_commit_filter_public_path` (three courses; negative control:
  without the composition's attribute read it fails at opening),
  `git_metadata_harness` (C6-generated agrees with `git check-attr` on 193
  questions), `commit_filter_fixture_contract`,
  `github_contract_fixture_security`, `github_recorded_fixture_contract`,
  `qt_pull_request_overview_page` and its four scale rows, `qt_files_tree`
  (three rows), `qt_commit_filter_view` at 100 and 200,
  `qt_workbench_window_scale_100`, `architecture_boundaries`,
  `command_correctness_registry_contract`, `test_network_isolation_manifest`
  and `application_review_round_trip_composition_contract`. `core-only`:
  `core_generated_files`, `core_commit_filter`, `core_review_cycle_manifest`.
  `coding-format-check` (clang-format 22.1.8) and clang-tidy with warnings as
  errors on all 13 changed translation units pass. The command-correctness and
  round-trip composition pins are resealed, with a note at the top of the
  contract.
- Live: BPR-Test #113 (closed, both branches deleted), the owner's shape with
  C2 regenerating 320 files and the head marking 319; every view matches
  `git check-attr --source C3`; Viewed unchanged; `X-RateLimit-Remaining`
  stayed above 4,870.

## Owner question (answered 2026-10-21, Accept)

AGENTS.md asks before a general-purpose matcher is written in core. The core
already holds one: `bpr::core::wildcard_match` and `workspace_attribute_rules`
(`review_cycle_ignore.cpp`), kept for C5 and C6 by the
[RFC-BPR-0083 decision record](../../../verification/RFC-BPR-0083-decision.md)
after its Git differential harness. The question asks Accept/Reject on reusing
it, extended by one attribute, with no new library; the alternatives named are
Git's GPL-2.0-only `wildmatch.c` (incompatible with the MIT binary, the
Apache-2.0 Abseil and LGPL-3.0-only Qt), libgit2's `wildmatch.c` or libgit2
behind an adapter (GPL-2.0 with the linking exception plus Rich Salz's
acknowledgement; exponential on stars and divergent per RFC-BPR-0083), RE2 by
translation (a new hand-written translator and an unapproved use), and
fnmatch(3), Qt wildcards or gix-glob.

- Session `e081a94ec80b52c1ecb067710c8983cc`, candidate
  `RFC-BPR-0089-M2-attribute-matcher`, revision `rfc-bpr-0089-m2-matcher-q1`,
  fingerprint `f8d128d460f3bc415da6bf3ba8dc52d4854042018565ea46d0f416bb14995629`.
- External state
  `/Users/jaeger/.mask/attended-review/owner-review-20261021-rfc0089-m2-matcher-v001`
  (checkout root: the main checkout, no paths protected). The authenticated
  URL is kept only there.
- Event cursor 3 at the first handoff; answered and handed back by event 6,
  closed at event 8.

## Result

| RFC criterion | M2 status | Where |
|---|---|---|
| AC04, marks and gestures | pass, fixture through the shipped composition: Review anyway, Show as normal and Treat as generated work and persist across restart; Clear all filters leaves generated files behind their row; nothing is hidden before a rule or gesture | [[BPR-1691]] AC1 |
| AC04, Hide files these commits changed | pass, fixture: C2's 96 paths hidden in C3 as well | [[BPR-1691]] AC2 |
| AC04, `linguist-generated` and `-linguist-generated` | pass, fixture through the shipped composition: 95 of 98 set aside, unprepared, under every filter and after Clear all filters; the nested `-linguist-generated` returns one; the reader agrees with `git check-attr` | [[BPR-1692]] |
| AC04, live on BPR-Test | pass for the attribute half: #113, 319 of 325 set aside, every view matches `git check-attr --source C3`, Viewed unchanged; the no-attribute gestures are covered by the fixture course | [[BPR-1692]] |
| AC05 (M2 gestures) | pass: no review mutation terminal after file-filter and generated gestures | [[BPR-1691]] AC4 |
| AC08 (M2 decisions) | pass under `core-only` | [[BPR-1689]] |
| Bulk label (RFC Commits tab) | pass, model and Qt; the fixture's 96-file C2 is correctly unlabelled | [[BPR-1691]] AC3 |
| Generated row "opening one shows its diff on request" | not delivered; Review anyway reads it today | [[BPR-1693]] |

## Verification on the candidate

- 30 focused dev rows pass on `8e296033d`: `core_generated_files`,
  `core_commit_filter`, `application_commit_filter_model`,
  `application_review_ledger_host`, `application_agent_invocation_host`,
  `application_pull_request_overview`, `sqlite_review_ledger_storage`,
  `sqlite_private_review_storage`, `sqlite_private_store_open`,
  `sqlite_window_review_restoration_storage`, `qt_commit_filter_view` at 100
  and 200, `qt_files_tree` (three rows), `qt_pull_request_overview` (five
  rows), `qt_workbench_window` at 100, 125 and 200, `qt_drawer_parts` (three
  rows), `application_commit_filter_public_path`,
  `command_correctness_registry_contract`, `test_network_isolation_manifest`
  and `architecture_boundaries`. `architecture_boundary_contract` passed
  before the rebase (no boundary-relevant change since).
- `core-only`: `core_generated_files` and `core_commit_filter` pass (built
  before the rebase; the rebase changed no file of mine).
- Negative control: without adopting the ledger's marks on reload, the
  composed course fails at "after a restart the file is still treated as
  generated".
- `coding-format-check` passes with clang-format 22.1.8. clang-tidy with
  warnings as errors passes on all 21 changed translation units.
- The command-correctness pins of `github_application.cpp` were resealed after
  the rebase.
- The documentation gate passes apart from the provisional IDs.

## Changed paths

Core: `generated_files.hpp/.cpp` (new), `commit_filter.hpp/.cpp`. Application:
`commit_filter_model`, `commit_filter_projection.hpp`,
`pull_request_overview.hpp`, `review_ledger_port.hpp`,
`review_ledger_application_host`, `observing_review_ledger_store`. SQLite:
`review_ledger_database` (schema 11), `review_ledger_store.cpp`. Qt:
`generated_files_view.hpp/.cpp` (new), `commit_filter_view`, `files_tree`,
`pull_request_overview_page`, `workbench_window`. Composition:
`github_application.cpp`. Tests: core, application, sqlite, Qt and the
composed course; `command_correctness_registry.json`. Docs: the RFC, the
[review ledger](../../../architecture/review-ledger.md), these cards.

None of the concurrent agent's areas (discovery, the github adapter, the
process_support lane) is edited.

## Open obligations and next action

- [[BPR-1692]] is done; land its branch on main.
- [[BPR-1693]]: open a set-aside file's diff on request (the last M2 gap).
- [[BPR-1688]] also covers restoring file filters with the window.
- Windows and Linux are unmeasured.

## Command state

No task-owned build or test process is running. The attended-review host for
session `e081a94e…` (pid 60888) was stopped by the BPR-1692 round through the
script's `stop`, at the coordinator's request; the session is closed. Build
directories are removed at the end of each task.
