# dsp-007 Windows full sweep at the lane tip

Station `bpr-win` (JaegAW, `windows-native`, branch `windows-lane`), task
`task-20260917-win-0890-0877-0876`. The sweep ran at
**`86b049014577bb02ec6952868215b8c1d31a5589`**, serial,
`ctest --preset windows-dev --output-on-failure --timeout 900`.

**364 rows, 338 passed, 26 failed, 3668.80 s.** The previous full Windows
sweep, dsp-005's at `2b2e5549`, was 362 rows with 53 failures.

## The three questions this sweep was run to answer

### 1. Was any row green at `2b2e5549` and is now red? **No. None.**

Every one of today's 26 failures appears in the `2b2e5549` record
(`docs/verification/dsp4-windows-merged-head-sweep.md`), compared by name
against that file rather than from memory. Twenty-seven rows went green and
nothing regressed.

### 2. Does any green row carry a non-zero exit? **No, and for contained rows the class is now closed.**

- **Contained rows.** `TEST_RUNNER_CHILD_EXIT_FAILED` is an inherited failure
  expression (`cmake/BprTestIsolation.cmake` ~834), so a contained row whose
  child exits non-zero is now necessarily red. Sixteen of the 26 failures
  printed it — and, per question 1, **not one of them is a row that used to be
  green**. The marker exposed no hidden crash; it agreed with signals those
  rows were already failing on.
- **Uncontained rows.** Those thirteen clear the isolation prefix and carry no
  marker at all, so they were measured directly instead; the method, the result
  and the re-measurement trigger are recorded on [[BPR-0890]]. Every green one
  exits 0.

So the answer across the whole suite is zero, by two different means for the
two kinds of row.

### 3. [[BPR-0901]]'s two rows: **both passed on Windows.**

| Row | Result |
| --- | --- |
| `application_overview_since_review_contract` | Passed, 44.42 s |
| `application_review_behaviour_contract` | Passed, 44.37 s |

**`RUN_BUDGET_EXPIRED` does not appear anywhere in this sweep.** So the wrong
budget (`bpr_fixture_run_budget_ms`, 40000, where seventeen sibling sites use
`bpr_epoch_phase_budget_ms`, 120000) is carried here too, and this host stayed
inside it under full-sweep contention where macOS went over by 35 and 52
milliseconds. There is no `elapsed_ms` to report because nothing expired. Not
fixed here: the fix lands on main from the Mac, in a file this lane also edits.

One disclosure about that margin: while rows in the 150-250 range were running,
this station also ran the documentation and issue-board validators and a git
commit on the same host. Both rows above ran later, at 328 and 329, with the
machine otherwise idle.

## The 26 failures, with the first failing line

`child-exit` marks a row whose output carried
`TEST_RUNNER_CHILD_EXIT_FAILED:1` — every one of which was already failing on
its own signal.

| Row | First failing line | child-exit |
| --- | --- | --- |
| `rfc0017_release_absence_negative_control` | `rfc0017-test-inventory:fail:string present in release binary: --network-denied` | yes |
| `rfc0016_production_course_security` | `FAIL: test_published_evidence_is_admitted_by_the_verifier` | yes |
| `test_network_isolation_child` | `Traceback (most recent call last):` — the counterexample probe, [[BPR-0903]] | yes |
| `sqlite_epoch_catalog_behavior_contract` | `FAIL: a copy of the previous file migrates to v6` | |
| `qt_composed_github_scale_100` / `_125` / `_200` | `FAILED: the code is in a fixed-pitch font at 1.6 times the row's size` — [[BPR-0869]] | yes |
| `qt_syntax_pane_composition` | `FAILED: the line that opened the block comment has scrolled off the top` — [[BPR-0870]] | yes |
| `github_cli_process_runner` | `FAILED: opt-in preserves only exact bounded stdout and completed exit code` | |
| `live_review_submission_restart_recovery` | `Traceback (most recent call last):` | |
| `epoch_kill_recovery` | `Traceback (most recent call last):` | |
| `epoch_transaction_kill_recovery` | `Traceback (most recent call last):` | |
| `application_device_authorization_refresh_course` | the rotation fault, [[BPR-0902]]: one rotation where two are expected | |
| `application_large_review_benchmark` | `large-review-benchmark-failed: application startup succeeds` — [[BPR-0856]] | yes |
| `application_unavailable_diff_public_path` | same | yes |
| `application_approximate_diff_public_path` | same | yes |
| `application_uiforetw_selected_diff_contract` | `PUBLIC_PATH_FAILED`, then `UIFORETW_SELECTED_DIFF_EVIDENCE_FAILED` — [[BPR-0859]] | |
| `application_review_ledger_composition_contract` | `STARTUP_FAILED` | |
| `application_defect_composition_contract` | `STARTUP_FAILED` | |
| `application_device_binding_launch_renewal` | `FAILED: private desktop test root under the canonical storage policy` | yes |
| `application_device_binding_relaunch` | `SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable` | yes |
| `application_review_submission_composition_public_path` | fails in 0.06 s with no output before its marker | |
| `application_profile_owner_admission_public_path` | `SEED_REGISTRY_REFUSAL:0` | yes |
| `application_profile_window_restart_public_path` | `seed failed (3)` | yes |
| `application_profile_window_layout_public_path` | `seed failed (3)` | yes |
| `application_profile_window_review_restoration_public_path` | `contained temporary: could not remove …\AC\Temp\bpr-0709-restart-53…` | yes |

### What the named refusals are worth reading together

Five of these now name themselves because of this dispatch's naming work, and
they name **one family**: `SEED_STARTUP_PROFILE_REFUSAL:registry_unavailable`,
`SEED_REGISTRY_REFUSAL:0` (`profile_registry_failure::unavailable`), the two
`seed failed (3)` aborts and the two `STARTUP_FAILED` composition rows are all
a store refusing a root whose parent does not carry the private descriptor.
[[BPR-0875]]'s fix gave the CMake courses an owned parent; these courses root
under the container temp and have not had that treatment. The last row in the
table is [[BPR-0868]]'s report doing its job: the removal failure names the
path instead of leaking quietly.
