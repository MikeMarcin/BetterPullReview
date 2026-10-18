# Codex ordered handoff progress — 2026-10-17

Mandate: `codex-handoff-2026-10-17.md`, direct execution, no delegated agents.
The resolved selection is `codex-handoff-2026-10-17.selection.json`.
Existing writers were stopped before handoff; process inspection found no builds
or test runners active. Existing worktrees and WIP remain authoritative.

The installed MASK readiness check reports `project.identity_changed` for the
main checkout; no global registry or skill configuration was changed. The
installed direct recipe and board helpers are usable. No authored JSON milestone
plan is attached to BPR-1577; discovery reports absent and bind refuses it. The
owner’s ordered Markdown handoff remains the execution and exit contract.

## Frontier

1. BPR-1577: Done and pushed in `376a4190b`. Six Overview rows and switch
   checks at 100%, 125% and 200% pass, as do format22/docs/diff checks. Session
   released through event 6; server PID absent; frozen root, worktree and branch
   removed. The stop request returned connection refused; process inspection
   established the recorded PID was no longer alive before removing resources.
2. BPR-1578: Done and pushed in `c38432adf`; 15 focused rows pass at
   `49b3c0f57`, with the later main allocation-only update merged. Format22,
   documentation and diff checks pass; existing composition pins match.
   All sessions released/stopped; frozen roots, worktree and branch removed.
3. BPR-1597/BPR-1568: Done and pushed in `19a3392d2` and `93ebf4eef`.
   All 28 focused rows pass at `c6c4eeff3`; format22/docs/diff pass.
   No schema addition. The accepted owner verdict closes BPR-1568 into
   BPR-1597, without changing the earlier Rejects. Session released, server PID
   absent, frozen root, worktree and branch removed.
4. BPR-1600: inherited WIP/card committed at `324653f18`, current main merged
   at `db2cd82d1` without conflict. Three affected targets build. All 14 focused
   key/catalogue/typing/Home/composed/refresh-feedback tests pass (30.69 s),
   as do format22/docs/diff. Focused clang-tidy22 passes all four touched translation units. Removing
   only `apply_refresh_key(home)` made the key test fail; exact restoration,
   rebuild and rerun pass. Finished/pushed in `ca55c0a71`; worktree, build and
   branch removed. No src/app delta.
5. BPR-1572: current main merged and composition resealed at `ce38d487e`.
   Round-four approvals and completed conditions confirmed. All 35 touched C++
   files pass format22; documentation and diff checks pass. Affected targets
   build and all 20 focused rows pass (239.48 s + 343.22 s), including both
   composition contracts. Prior unchanged tidy/live/negative controls retained.
   Finished/pushed in `57f335411`; worktree, build and branch removed.
6. BPR-1598: main `57f335411` merged at `e569d67aa`. Inherited timing
   implementation and negative controls are present; affected benchmark and
   replay targets build. The native tidy helper now selects documented LLVM 22
   and supports bounded read-only concurrency (`f579a02ca`); its CTest contract
   and documentation gate pass. The 899-unit inventory is complete; 121 runs reported findings. Fixes
   are in place, 74 changed test targets plus focused consumers build, full
   format22 and documentation checks pass, and replay’s five-second CPU
   negative control fails as intended. The initial complete native gate exposed follow-on findings; those are
   fixed and six direct rechecks pass. Source slices are `9e73d212d` and
   `b1b416c17`, with final residual fixes in `0bea4e26d`. All large-review rows
   pass (15 executed, one documented Debug skip), with the bounded retry used
   by responsiveness and history. All 84 focused rows pass (1,212.24 s). Nine final
   checks pass on `0bea4e26d`, covering the follow-on edits and composition
   reseal (201.40 s). The final full tidy gate passes all 899 translation
   units, and format passes all 1,628 files. Finished/pushed in `4305b7210`;
   worktree, branch and 12 GB build removed.
   No full-suite run per card.
7. BPR-1599: inherited fix preserved at `5721c7061`, main through BPR-1598
   merged at `68c701ea5`. The product loses a modality chosen during loading
   when arrival reapplies the opening preference. Source history identifies the
   older arrival reset and the loading view introduced by `9c670d886`;
   BPR-1588 is absent from the original failing base. The current-main negative
   control fails the deterministic Qt assertion at the lost Review choice;
   the native course passes on that timing (7.09 s). Exact source restoration
   is verified by SHA-256. Positive rebuild passes; all seven focused rows
   pass on `69a477a05` (96.21 s), as do focused tidy22, format22,
   documentation and diff checks. Composition pins match BPR-1598.
   The new regression also covers cached arrival and a later load.
   Finished/pushed in `2e37e282e`; worktree, branch and 1.1 GB build removed.
7b. BPR-1601: added by the amended handoff at `ede68f51c`, implemented
   directly at `39d056d37` and `4c2d600f3` over main `887feb0ed`. Singleton
   receipts reuse the core and existing table. Discuss, Threads' Not on a file
   group, Status and next-new navigation share the read state. The native
   lookup is bounded before indexing file threads, and clearing a PR resets
   the conversation count. All 14 native focused rows pass across the affected
   checks, including the public read/unread/restart/edit/menu course and both
   composition contracts (664.58 s, with a production Release build). Three
   core-only rows pass; format22, documentation and its 67 tests pass.
   Finished and pushed in `1abb3f581`. The managed worktree is archived,
   its source/build trees removed and its merged local branch deleted.

8. Task-owned housekeeping: complete for items 1–7 and 7b. All handed-off branches/worktrees and
   frozen roots are absent. The audited BPR-1568, BPR-1577, BPR-1578 and
   BPR-1597 candidates are unprotected; all recorded host PIDs are absent.
   Historical decisions and session files are retained. The older BPR-1568
   mockup session was already unprotected and has no host or frozen build.
   `git worktree prune` completes; unrelated and paused work is retained.
9. The single final integrated configure/build/full CTest ran on main
   `e9b1cee35` after the other project's Release build and ThreadSanitizer run
   ended and a second process preflight found the machine quiet. Configure and
   the full build pass. Debug uses `-g0` for disk use, retaining assertions and
   test behavior; build concurrency is six and CTest concurrency four, with
   serial rows respected. CTest ran from 05:20:27 to 06:30:01 UTC on
   2026-10-18: 778 passed, 12 failed and one skipped, 4,174.18 seconds. No
   second full suite was run. The documentation-only BPR-1602 allocation
   `4d6b63e86` arrived during the run; it changes no tested source.

   All twelve failed rows were rerun in separate CTest invocations. Nine now
   pass after the BPR-1601 integration follow-up described below; three
   out-of-queue inventory failures remained at the original handoff close.
   The owner's 2026-10-18 follow-up below resolves all three. The
   strengthened conversation-read public path also passes. The affected
   targets and final application relink pass, as do documentation and its
   67 regression tests. Full format22 passes all 1,632 files. The full tidy22
   pass checks 902 native translation units with four workers: 900 pass and
   two report the mechanical findings below. Both repaired units pass their
   focused tidy22 recheck. All native units are covered; 91 platform/configuration
   units absent from this Mac's compilation database are explicitly skipped,
   not claimed passing. Validation and task-owned cleanup are complete.

   The BPR-1601 integration follow-up is `1c4ffbee0`; the final BPR-1598 lint
   repair is `5095c91f4`. These commits contain the tested source changes.
   The final documentation gate passes on their combined tree and this report.

## Integrated follow-up and original inventory failures

The broader workbench courses still expected file-only Threads rows. They now
verify that a PR comment moves to its Not on a file row with selection and
focus, retaining the unknown-identity and pathless-to-Review refusals. The
next-new tooltip had also renamed every file thread a conversation. It now
counts threads and standalone comments separately, preserving the unchanged
current-file badge test. The terminology scan allows the PR-conversation
phrase only on the two named Status controls. The new public course verifies
mixed navigation counts and their return to threads only after marking the
standalone comments read. BPR-1601's required Windows registration is added as
`unmeasured (registered on macOS)`; no native Windows result is claimed.

These nine formerly failing rows pass individually: `qt_thread_moves_scale_100`,
`qt_thread_moves_scale_125`, `qt_thread_moves_scale_200`,
`qt_workbench_conversation_reveal`, `qt_workbench_window_scale_100`,
`qt_workbench_window_scale_125`, `qt_workbench_window_scale_200`,
`qt_current_file_unread_badge` and `application_thread_words_public_path`.
`application_conversation_read_public_path` passes again with the added count
assertions. The thirteen isolated invocations took 143.00 seconds in total.

These three failures reproduced alone before the inventory follow-up:

- `github_contract_fixture_security` (1.47 s): the expected fixture inventory
  omits the eleven committed `status-attention-v1` files added separately by
  BPR-1589 in `2e86be6e5`. The exact-tree assertion reports those extra files.
- `test_network_isolation_structure_contract` (8.60 s): `cmake_sources` walks
  unrelated nested checkouts under `.claude/worktrees`, so the raw-registration
  check counts their CMake registration owners as duplicates. No unrelated
  checkout was removed or changed to conceal this result.
- `test_network_isolation_manifest` (10.36 s): the same nested-checkout scan
  duplicates the designated-live and Windows registration counts. It also
  reports BPR-1589's separate `application_status_attention_public_path` row
  missing from `WINDOWS_UNCONTAINED_TESTS`. BPR-1601's registration omission is
  fixed and absent from this rerun.

The sole skipped row is the documented Debug-only
`application_large_review_cache_bytes` case. All other large-review rows,
including history, timing control and replay, pass. Both composition contracts,
the native and file-backed private-review courses and the portable two-catalogue
epoch course pass. The previously flaky `release_update_native_https` passes
this run (9.33 s); this does not establish that its intermittency is fixed.

The full lint command exits 1 for two findings introduced by separately landed
BPR-1589 commit `39afb7162`: local `most_teams` violates the naming rule in
`github_provider.cpp`, and `get_team_membership` duplicates the following
`return false` switch branch in `github_read_contract.cpp`. Renaming the local
and grouping the identical cases preserve behavior. Both affected targets and
the final application relink pass; `github_read_contract` and `github_provider`
pass in 3.96 seconds. Format22 on the two files and their focused tidy22 command
both exit 0. The unchanged 900 passing units are reused; a second complete lint
pass was not run or claimed. No lint findings remain on the checked native
compilation graph.

Raw full-suite, isolated rerun, build and lint evidence is outside Git at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-codex-handoff-20261017-vwyr19ur`.
`full-LastTest.log` and `full-LastTestsFailed.log` preserve the original suite
before the individual reruns; `final-run.json` records timestamps and exits.

## Inventory follow-up — 2026-10-18

The owner requested direct fixes for the three inventory failures and a push
to main. This follow-up starts from `902f3afaf`, including the separately
landed BPR-1602. The fixture inventory now binds `status-attention-v1` to its
exact synthetic identity and populated thread, review, comment and check field
sets. Its eleven files pass the existing exact-tree, manifest, field, byte
sanitization and artifact checks without relaxing those checks.

The CMake source walk prunes nested repository roots, recognizing both a
`.git` directory and a worktree's `.git` file. Both registration inventories
use that walk. It keeps scanning ordinary hidden directories and applies the
existing generated-directory exclusions relative to the requested root. Three
new regression tests cover nested repositories, unapproved registrations in
an unmarked hidden directory, and a checkout beneath a parent named `build`.
Before the fix, the nested-repository cases and the parent-directory case
fail; the hidden-directory control passes. All 72 structure tests now pass.
No unrelated checkout was removed or changed.

The missing BPR-1589 Windows registration is recorded as
`unmeasured (registered on macOS)`. Reconfiguring the current main checkout
also exposed BPR-1602's three missing entries:
`sqlite_profile_data_reset_files`,
`application_profile_data_reset_public_path` and
`application_profile_recovery_public_path`. They receive the same unmeasured
marker; native Windows qualification remains open.

Validation on this tree:

- `cmake --preset dev -DCMAKE_CXX_FLAGS_DEBUG=-g0` passes and refreshes the
  configured inventory to 796 tests.
- The focused three-row CTest run passes `github_contract_fixture_security`
  (1.39 s) and the structure contract (0.70 s), and identifies the three
  BPR-1602 entries above. After adding them, the affected structure contract
  (0.63 s, 72 tests) and manifest (0.70 s) both pass. The manifest reports
  `configured=796:registrations=727:backend=macos-sandbox-exec`.
- `python3 scripts/verify_documentation.py --root .` and `git diff --check`
  pass. No product code, fixture bytes or CMake registration changed; the
  existing C++ build, formatting and lint evidence is reused. No additional
  full suite was run.

All three original inventory failures are resolved. Raw follow-up output is
outside Git at
`/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr-inventory-followup-20261018-mvxiidz7`.


Do not touch RFC-BPR-0085 work BPR-1590–1595 or the drawer-redesign worktree.
BPR-1589 landed separately; the amended handoff assigns BPR-1601 here.
