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
The owner's later HTTPS follow-up below repairs the timeout observation race.

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

## Native HTTPS follow-up — 2026-10-18

The owner requested a direct fix for `release_update_native_https`, affected
checks and a push to main. Main was fast-forwarded to `560dd3799` first. The
fixture required a full origin request even when the native client's 500 ms
total deadline correctly expired during CONNECT or TLS setup. Controlled
setup delays reproduce that mismatch. The repair accepts timeout-related
disconnects at those boundaries, verifies observations after the proxy
drains, and retains exact checks for complete requests. Product code and
timeout budgets are unchanged.

The affected four-row CTest set passes, including the new six-test fixture
contract and network-isolation manifest. The expanded 26-scenario native
HTTPS course then passes eight consecutive runs (80.79 s). The disconnect
controls fail against the original fixture; both new native timeout cases
also reject an injected successful response. Focused format22/tidy22,
documentation and diff checks pass. The full diagnosis, commands and evidence
are in [the RFC-BPR-0035 ledger](../../verification/RFC-BPR-0035.md#https-fixture-timeout-follow-up--2026-10-18).


Do not touch RFC-BPR-0085 work BPR-1590–1595 or the drawer-redesign worktree.
BPR-1589 landed separately; the amended handoff assigns BPR-1601 here.

## BPR-1593 amended queue — 2026-10-18

The owner explicitly authorized item 7c as an exception to the stale
BPR-1590–1595/worktree exclusion. BPR-1594 and BPR-1603 remain untouched.
All earlier queue cards were skipped after checking their Finish commits on
origin/main; prior BPR-1599 WIP inspection and negative-control evidence above
remain authoritative.

Main a7a45e1c9 merged into bpr-1593-tours; the app delta is only the tests-on
attended tour course. All three printed composition pins match; a dated
merged-tree reseal note records the reviewed scope. All 44 focused tests pass
serially (373.20 s), including every tour row, all window scales, Notes,
submission, start-review and composition contracts. Format22, documentation
and diff checks pass. The inherited accepted design was landed without visual
edits; frontend-design is unavailable in this harness. MASK preflight reports
project.not_registered for the inherited worktree; existing direct execution
and board helpers were retained without registry changes.

Owner Accept at session sequence 4 and handback at 5 consumed. Protection
released at 6. Finish commit 545fce48e was produced by the board tool and
plain-pushed to origin/main. The recorded server PID was absent; the stop
command reported connection refused. Frozen root removed with the freeze
helper. The Tours worktree remains for Claude; only its 17 GB build/dev was
removed. Worktree prune completed; unrelated branches/worktrees were retained.

Final integrated configure and build pass on 545fce48e. One four-worker full
CTest is in progress. The scheduler was paused after the first few checks
because other sessions started builds/tests; that pause caused timeouts in
diagnostic_context_age and live_mutation_isolation, to rerun individually.
The owner then amended the mandate: ignore quiet-machine eligibility and
accept narrow timing misses until later release preparation. The same full
CTest process resumed; no second full suite was started. Final results and
lint gate are pending.

## BPR-1594 amended queue — 2026-10-18

The owner added accepted Agent tab item 7d while the pre-Agent integrated
suite was running. That partial suite was stopped by SIGINT to its own
CTest PID 63778 after 442 completed checks; it is not a completed final
suite. Its log is /tmp/bpr-pre1594-partial-20261018.log. It found two
scheduler-pause-induced timeouts and contained_temporary_usage_contract's
profile_recovery_course.py:92 direct tempfile.mkdtemp use. The final suite
below will decide the integrated result after the new landing.

BPR-1594 merged origin/main 3b68c4394 after BPR-1593. The app delta is only
the tests-on attended Agent fixture course; three printed pins matched and
a dated merged-tree reseal records the reviewed scope. All 16 prescribed
focused rows pass serially (235.57 s), plus the touched dif-context public
course (8.90 s). Format22/docs/diff pass. Owner Accept at sequence 4 and
handback at 5 consumed; protection released at 6. The inherited accepted
visuals were landed unchanged; frontend-design is unavailable in this
harness. RFC execution status records Tours and Agent without RFC closeout.
The main checkout's board tool finished the card (avoiding the worktree's
.agents symlink), and plain push landed f9ad0ffc4. Session stop succeeded,
PID 49516 exited, frozen root removed through the freeze helper, worktree
and its build removed, merged local branch deleted, and worktrees pruned.
The Tours worktree remains as explicitly requested.

Final integrated configure passes on f9ad0ffc4; full build is in progress.
The prior quiet-machine/timing amendment remains recorded above. No new
live GitHub course or owner design work was performed.


## BPR-1603 accepted landing — 2026-10-18

Direction A and the owner-authorized durable quoted replies implemented at
9eab5046a. Built owner review d28b9f71… accepted without note at decision 6,
handback 7 consumed, protection released at 8. Origin/main 937f96880 was
already merged and did not move before landing. Focused functional, restart,
safety and Qt checks, both composition contracts, reseal, format22, touched
tidy and docs passed. Live BPR-Test PR 108 reply course passed and cleaned
all its objects; minimum rate header 4970. RFC-BPR-0085 status updated;
BPR-1595 stays open for Claude. Finishing through board tool and plain push.
Final integrated suite has not yet run after this landing.


## Final integrated verification after BPR-1603 — 2026-10-18

Tested product landing: `6ca8aaccc` (Finish BPR-1603); BPR-1594
was already finished/pushed at `f9ad0ffc4`. BPR-1603's owner Accept was
session `d28b9f71…`, decision 6, handback 7, no note. Both cards finished
through the board helper, with plain pushes. RFC-BPR-0085 execution status
is updated; BPR-1595 stays open for Claude.

Ran exactly one uninterrupted integrated suite after the final landing:
`cmake --preset dev`, `cmake --build --preset dev`, then
`CTEST_PARALLEL_LEVEL=4 ctest --preset dev --output-on-failure`.
Configure/build passed. Machine was quiet at launch and checked again during
the run: no other build/test processes; CTest's own nested production build
and serial GUI rows were retained. Own CTest PID 998 was never paused.
Result: **789 passed, 8 failed, 1 skipped, 798 total**, 3748.64 seconds
(62 min 28.64 s). All eight failures were rerun individually with exact-name
filters and `-j 1`; each reproduced the same deterministic assertion. No
narrow timing failures remained. Diagnostic-context age and live-mutation
isolation, which the earlier interrupted run affected, both passed.

### Every remaining suite failure and cause

| Test | Cause |
| --- | --- |
| `contained_temporary_usage_contract` | A: direct tempfile call bypasses the required contained-temporary helper. |
| `qt_composed_github_scale_100` | B: legacy Agent tools stack assertion. |
| `qt_composed_github_scale_125` | B: same legacy Agent tools stack assertion. |
| `qt_composed_github_scale_200` | B: same legacy Agent tools stack assertion. |
| `application_review_submission_composition_public_path` | C: legacy Agent purpose/tab visibility assertion. |
| `application_review_submission_composition_scale_125` | C: same legacy Agent purpose/tab visibility assertion. |
| `application_review_submission_composition_scale_200` | C: same legacy Agent purpose/tab visibility assertion. |
| `application_thread_words_public_path` | D: review-thread wording scan also rejects legitimate Agent chat text. |

A. `tests/end_to_end/profile_recovery_course.py:92` calls
`tempfile.mkdtemp`; the policy course requires
`scripts/contained_temporary_support`. Introduced by `08d50fe93`, before
BPR-1603. The helper preserves usable inherited permissions for contained
Windows tests; this is a portability-policy violation in the fixture.

B. `tests/adapters_qt/composed_github_tests.cpp:116` still selects
`privateConversationTabs`' old Agent-tools page. Its assertion at 5970
requires the original launcher, side-chat and finding-queue widgets to be
visible in their old vertical order. BPR-1594 intentionally leaves that
legacy column hidden and presents the new `agent_tab_view`, borrowing its
controls. `workbench_window.cpp:3023-3027` constructs the new tab and hides
the old tabs. The three tests are out of sync with the accepted presentation;
the current Agent-tab/window tests pass at all three scales.

C. `tests/end_to_end/review_submission_composition_public_path.cpp:380`
requires `workbenchNotesPurpose` to be visible and compares its dimensions
with `privateConversationTabs`. BPR-1594 deliberately hides both legacy
widgets (`workbench_window.cpp:3026-3027`) in favor of the accepted Agent
tab. All three course variants abort at this stale UI assertion, before the
remaining publication checks. Existing focused submission/safety checks pass.

D. `tests/end_to_end/setup_destination_public_path_tests.cpp:1295-1308`
exempts only the literal `AI conversations`, then rejects any remaining
`conversation` anywhere in the collected UI/accessibility strings. The
accepted Agent UI now says `Agent conversations, findings and private
history` in its action/tab tooltips and `Back to the conversation` in
`agentHistoryBack`. These refer to Agent chat, not a review thread. The
scan's Agent-context exemption is stale; its own comment explicitly keeps
agent conversations distinct from review threads.

### Deliberate skip and gates

`application_large_review_cache_bytes` skipped with code 77 because the dev
build is Debug. `large_review_benchmark.cpp:2263` explicitly restricts that
byte-pressure benchmark to optimized builds. This is not a passing benchmark.

`cmake --build --preset dev --target coding-format-check coding-tidy`
completed: format22 passed all 1652 files; full tidy checked 914 translation
units with six analyzers and failed on the following three findings. Each
unit was rerun individually with warnings-as-errors and reproduced exactly.

| Location | Diagnostic and cause |
| --- | --- |
| `src/adapters/qt/src/private_notes_panel.cpp:793` | `performance-no-automatic-move`: `const auto label` (line 789) is returned by value; constness prevents the automatic move. From c2487b2585 (BPR-1592). |
| `src/adapters/qt/src/review_tour_editor.cpp:289` | `performance-unnecessary-copy-initialization`: local `doc` copies `excerpt` and is never modified before use. From eb4990c98d (BPR-1593). |
| `tests/end_to_end/review_submission_composition_public_path.cpp:323` | `bugprone-optional-value-conversion`: dereferences `Attachment->Line` only to wrap it back into an optional. From c2487b2585 (BPR-1592). |

The isolated tidy logs are `/tmp/bpr-final1603-tidy-rerun-{1,2,3}.log`;
`/tmp/bpr-final1603-tidy-reruns.json` maps each to its source path. These
remaining findings and suite failures are reported rather than folded into
an unrelated repair/card. The new conversation read/reply public path and
round-trip composition contract passed in the full suite (4.35/2.65 s).

Concurrent main commits 0378ab977 (issue allocations) and 6a7073240
(Insights RFC pack) changed documentation only. Checked the diff from the
tested product: no src, tests or build-configuration change. Final documentation
gate and all 67 documentation tests passed on this report and current main.

Evidence retained outside Git: `/tmp/bpr-final1603-configure.log`,
`/tmp/bpr-final1603-build.log`, `/tmp/bpr-final1603-full.log`,
`/tmp/bpr-final1603-reruns.json`, each exact-name rerun log and
`/tmp/bpr-final1603-gates.log`. No raw test output or build artifacts committed.

Cleanup completed: stopped the BPR-1603 review host, released protection,
removed its frozen root with the freeze helper, removed its worktree/build
and merged local branch, and pruned worktrees. BPR-1594's server/root/worktree
were already cleaned after landing. Removed the final main `build/dev`
tree after all tests/analyzers exited. The shared dependency cache is
preserved. The explicitly retained BPR-1593 worktree remains for Claude.
