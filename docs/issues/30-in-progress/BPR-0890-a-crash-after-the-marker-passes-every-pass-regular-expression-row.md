---
schema: "implement-rfc.card.v1"
id: "BPR-0890"
title: "A crash after the marker is printed passes every row that has a PASS_REGULAR_EXPRESSION"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"r5 whole-project critical review, Claude/fable src/app and gates partition","found_at":"20418bfe","run":"run-20260916-stations","dispatch":"dsp-005"}
---

# BPR-0890: A crash after the marker is printed passes every row that has a PASS_REGULAR_EXPRESSION

## Defect

CTest ignores exit status when a row has a pass expression —
`cmake/BprTestIsolation.cmake:741-748` records this deliberately. **117 rows
carry one**: 79 in `tests/end_to_end/CMakeLists.txt` and 38 in
`tests/adapters_qt/CMakeLists.txt`.

`report_verdict` prints `PUBLIC_PATH_OK` **before** `QApplication::exec()`
returns, and before `desktop`, `QApplication` and the static destructors run
(`src/app/main.cpp:8735-8741`). So an abort anywhere in that tail is invisible
to CTest: the marker is already on stdout and the row is green.

That tail is not hypothetical. `github_application_liveness_tests.cpp:84-92`
documents exactly this class of shutdown fault for [[BPR-0215]], [[BPR-0217]]
and [[BPR-0827]].

[[BPR-0552]] chose the pass-expression trade-off knowingly, but did not close
this gap. It is worth closing now because this dispatch has just spent a great
deal of effort on shutdown behaviour — [[BPR-0875]]'s thirteen-row Windows
cleanup cluster is precisely a family of shutdown faults, and a green row is
currently no evidence that shutdown succeeded.

## Scope

- Pair the marker with the child's exit status.
  `scripts/run_network_isolated_test.py:117-136` already has the child's code
  and could print `TEST_RUNNER_CHILD_EXIT:<n>`, which a second
  `FAIL_REGULAR_EXPRESSION` entry can catch. That is the macOS launcher; the
  Linux `execvp` launcher
  (`tests/network_isolation/linux_network_sandbox.cpp:126`) and the Windows
  launcher need the same, or the gap just moves platform.
- Whatever shape is chosen, it has to compose with [[BPR-0875]]'s work on the
  non-append `FAIL_REGULAR_EXPRESSION` property, and with the existing
  `bpr_expects_failed_run` rows that are supposed to fail.
- Then check whether any currently green row is hiding a non-zero exit. That
  is the interesting part, and it should be done before anything is
  celebrated as fixed.

## Acceptance

- A row whose application prints its marker and then aborts fails, shown by a
  deliberate abort in the shutdown tail.
- The existing expected-failure rows still behave.
- The card records how many previously green rows were hiding a non-zero exit,
  on each platform.

## The fail-closed verdict is not pinned on ordinary rows either

From the same review, and the same machinery, so it belongs here rather than
in its own card.

`cmake/BprTestIsolation.cmake:770-773` sets
`FAIL_REGULAR_EXPRESSION "PUBLIC_PATH_FAILED"` with a **non-append**
`set_property`. Any later row-level
`set_tests_properties(... FAIL_REGULAR_EXPRESSION ...)` silently replaces it.

The gate does not catch that. `scripts/verify_test_network_isolation.py:641-659`
checks the property only for the fourteen `EXIT_VERDICT_TESTS`, and
`configured_failures` (`:903-1003`) never asserts its **presence** for a
`bpr_network_isolated` row that lacks the `bpr_expects_failed_run` label.

Today no row overrides it — grep confirms — so nothing is broken. But the
comment at `BprTestIsolation.cmake:753-756` claims "a row added later inherits
it and the guarantee cannot drift", and nothing enforces that claim. The
reviewer verified the gap directly: adding `FAIL_REGULAR_EXPRESSION "x"` to one
row and running `ctest --show-only=json-v1` still passes
`test_network_isolation_manifest`.

**Deliberately not taken as a quick fix in dsp-005.** The check itself is a few
lines in `configured_failures`, but it has to get the live lane, the
`bpr_expects_failed_run` rows and any legitimate exception right, and an
assertion that is wrong in the strict direction turns the manifest gate red for
every platform at once. It wants the same pass as this card's exit-status work,
which touches the same property on the same rows.

Additional acceptance: a row that replaces the inherited
`FAIL_REGULAR_EXPRESSION` fails `test_network_isolation_manifest`, shown by
adding one deliberately.

## Findings, dsp-006 (macOS)

### The measurement, first

Every one of the **208** rows this build configures with a
`PASS_REGULAR_EXPRESSION` was run with the macOS launcher printing the child's
exit status, and every one of them was measured — 64 in the full-suite sweep
(`build/r2-logs/dsp6-0890-sweep.log`, 382 rows, 1584 s) and the remaining 144
in a follow-up run of exactly the rows the first sweep had not marked
(`build/r2-logs/dsp6-0890-rerun.log`, 424 s). No pass-expression row was left
unmeasured.

**One green row was hiding a non-zero exit, and it was hiding it by design:**

- `rfc0017_release_absence_negative_control` — green, child exit **1**. It runs
  the shipped-artifact scan against the tests-enabled binary, where the scan
  *must* fail; the row pins the refusal text and the scanner exits 1. Nothing
  anywhere said so. It now says so at the registration, with
  `EXPECTS_NONZERO_EXIT`.

One further green row exits non-zero and was already declared:
`application_automated_run_budget` (exit 1, `bpr_expects_failed_run`).

Four rows exited non-zero and were **already red**, so they hid nothing:
`application_large_review_cache_bytes`, `application_large_review_loaded_close`
and `application_large_review_resources` ([[BPR-0893]]'s known load-sensitive
footprint rows, and this sweep ran six-wide), and
`application_overview_since_review_contract`. No row died of a signal.

So on macOS the shutdown tail is clean today: **zero green rows hiding an
undeclared fault**. That is a measured zero rather than an assumed one, and it
is what makes the guard below a regression net rather than a bug fix.

### The mechanism

`scripts/run_network_isolated_test.py` prints `TEST_RUNNER_CHILD_EXIT:<n>`
after every run — the evidence, in every log — and
`TEST_RUNNER_CHILD_EXIT_FAILED:<n>` only when that code is non-zero, which is
what `bpr_add_network_isolated_test` now matches as a second
`FAIL_REGULAR_EXPRESSION` entry. A literal rather than a negated digit class,
because CTest's regular expressions have no lookahead. A negative value is
death by that signal.

Both entries are written with `APPEND`, because `set_property(... PROPERTY
...)` replaces the whole property and a second non-append write would have
silently deleted the first — the hazard this card folds in, met from the inside.

Two exemptions, each written at the row rather than inferred:
`EXPECTS_FAILED_RUN` (unchanged: the row whose subject is a failed run, which
inherits neither entry) and the narrower `EXPECTS_NONZERO_EXIT` (the row whose
child exits non-zero as the answer it asserts, which keeps `PUBLIC_PATH_FAILED`
and drops only the exit entry). The two keywords refuse to be combined.

Configured result: 379 rows carry both entries, 1 carries the run verdict
alone, 2 carry neither — `application_automated_run_budget` and the enumerated
macOS-unsandboxed row `gh_cli_git_smart_transport`, which is registered outside
the seam and is now held to the stronger rule instead (no pass expression at
all, so its exit status is the only thing that decides it).

### Evidence

- **A row that prints its marker and then aborts now fails.** A row registered
  through the real seam, whose child printed `PUBLIC_PATH_OK` and then raised
  `SIGABRT`: `***Failed  Error regular expression found in output.
  Regex=[TEST_RUNNER_CHILD_EXIT_FAILED]`, child exit -6
  (`build/r2-logs/dsp6-0890-proof.log`). The same fixture under the property
  set this card starts from **passes**
  (`build/r2-logs/dsp6-0890-ctest-semantics.log`), and a clean exit still
  passes, so the difference is the guard and not the fixture.
- **The gate now catches a replaced property.** A deliberate row-level
  `set_tests_properties(application_public_path PROPERTIES
  FAIL_REGULAR_EXPRESSION "NEVER_PRINTED_SENTINEL")` turns
  `test_network_isolation_manifest` red with
  `TEST_NETWORK_ISOLATION_FAILED:application_public_path must carry the
  inherited fail-closed verdict ['PUBLIC_PATH_FAILED',
  'TEST_RUNNER_CHILD_EXIT_FAILED']; found ['NEVER_PRINTED_SENTINEL']`. The
  reviewer's original demonstration — add an override, gate still green — no
  longer reproduces.
- The expected-failure rows still behave: `application_automated_run_budget`
  and `rfc0017_release_absence_negative_control` both pass, alongside the gate
  rows and a sample of ordinary rows
  (`build/r2-logs/dsp6-0890-final-verify.log`, 12/12).

### Not closed here

The acceptance line "on each platform" is answered for macOS only. The Linux
launcher `execvp`s, so it has no exit status to state and must become a
supervising parent before it can; the Windows launcher already waits on the
child and only needs to print, but its `WINDOWS_UNCONTAINED` rows run with no
launcher at all and stay unguarded — which is exactly [[BPR-0875]]'s cluster.
Both platforms already carry the property, so the launcher is the only thing
left to change on each.

## The Linux launcher now supervises instead of dissolving (bpr-linux, dsp-007)

Base `019bab8f`. One file changed,
`tests/network_isolation/linux_network_sandbox.cpp`. No CMake and no gate
change: `cmake/BprTestIsolation.cmake` already appends
`TEST_RUNNER_CHILD_EXIT_FAILED` to every Linux row, where it has been inert
because `::execvp` left nothing to report.

After `enable_loopback()` succeeds the launcher forks. The child sets
`prctl(PR_SET_PDEATHSIG, SIGKILL)`, re-checks `getppid()` to close the window
between the fork and that call, and then `execvp`s exactly as before. The parent
installs forwarding handlers for SIGINT, SIGTERM and SIGHUP, `waitpid`s,
translates the status the way the macOS runner does, prints
`TEST_RUNNER_CHILD_EXIT:<n>` always and `TEST_RUNNER_CHILD_EXIT_FAILED:<n>` when
non-zero, `fflush`es and `_exit`s with the child's own number.

**The two platforms now print the same number for the same event.** `_exit`
keeps only the low eight bits, so passing the negative signal number through
reproduces macOS's status exactly: a SIGABRT prints `-6` and exits **250** on
both. Measured here — `-6`/250, `-15`/241 (forwarded SIGTERM), `-2`/254
(forwarded SIGINT), `3`/3, `0`/0.

**The boundary did not move.** Namespaces are inherited across `fork`, so the
child runs in the same `CLONE_NEWUSER|CLONE_NEWNET` pair; the identity mapping
argued at lines 47-55 is written *before* the fork and is untouched. Verified
from inside the child: one interface (`lo`, up, 127.0.0.1), `uid_map`
`1000 1000 1`, uid/gid 1000 — self-mapped, not root.

### Evidence, through the real seam

Three rows were registered temporarily through `bpr_add_network_isolated_test`
in `tests/CMakeLists.txt`, each given a `PASS_REGULAR_EXPRESSION` the child
does print, run, and then **reverted before the commit**. All three inherited
`FAIL_REGULAR_EXPRESSION = ['PUBLIC_PATH_FAILED', 'TEST_RUNNER_CHILD_EXIT_FAILED']`,
confirmed against `ctest --show-only=json-v1`.

- **A row that prints its marker and then aborts now fails**
  (`build/r2-logs/dsp7-0890-seam-proof.log`):

  ```text
  1/2 Test #54: dsp7_probe_abort_after_marker ....***Failed  Error regular expression found in output. Regex=[TEST_RUNNER_CHILD_EXIT_FAILED]  0.00 sec
  PUBLIC_PATH_OK
  TEST_RUNNER_CHILD_EXIT:-6
  TEST_RUNNER_CHILD_EXIT_FAILED:-6
  ```

- **The control separates the guard from the fixture.**
  `dsp7_probe_clean_after_marker` — same seam, same pass expression, clean exit
  — **Passed**. So the difference is the guard.

- **A CTest TIMEOUT still kills the child and leaves no orphan**
  (`build/r2-logs/dsp7-0890-timeout-orphan.log`). This is the single regression
  forking can introduce, so it was measured rather than argued.
  `dsp7_probe_timeout_orphan` (`TIMEOUT 5`, child `exec sleep 313`) reports
  `***Timeout 5.01 sec`; the census of live `sleep 313` processes is empty
  before the run, immediately after it, and again three seconds later. CTest
  SIGKILLs the launcher pid only, and `PR_SET_PDEATHSIG` takes the child down
  with it. Killing the launcher with SIGKILL by hand gives the same result
  (`dsp7-0890-signal-probe.log`).

- **Signal forwarding works** (`dsp7-0890-signal-probe.log`,
  `dsp7-0890-sigint-probe.log`): SIGTERM to the launcher reaches the child,
  which dies of it — `TEST_RUNNER_CHILD_EXIT:-15`, launcher exit 241; SIGINT
  likewise, `-2` and 254.

- **The four isolation rows and both declared exemptions are green**
  (`build/r2-logs/dsp7-0890-isolation-rows.log`):
  `test_network_isolation_identity` 0.03 s, `_external` 0.03 s, `_child`
  0.06 s, `_loopback` 0.04 s, plus `_manifest` 1.28 s and `_application`
  0.08 s. `rfc0017_release_absence_negative_control` (the
  `EXPECTS_NONZERO_EXIT` row, child exit 1) **Passed, 1.63 s** — so macOS's
  exemption carries on Linux — and `application_automated_run_budget` (the
  `EXPECTS_FAILED_RUN` row) **Passed, 5.32 s**.

### Independent review, and what it changed

An independent reviewer worked the diff for process and signal handling and
returned **no critical and no major findings**, clearing signal safety, wait
correctness, exit fidelity, orphan/timeout behaviour, boundary preservation,
stdio and verdict effects. Three of its findings were taken:

1. **The reaped pid was still forwardable.** Between `waitpid` returning and
   `_exit`, a signal would have `kill`ed a pid already released and possibly
   reassigned. `ChildPid` is now cleared at the reap — the same window the
   macOS runner guards with `child.poll() is None`.
2. **An inherited `SIGCHLD = SIG_IGN` produced no exit line at all.** Ignored
   dispositions survive `execve`; under one, the child is auto-reaped,
   `waitpid` fails with `ECHILD`, and the launcher exited 125 having printed
   neither line — a row with a pass expression would have been green again with
   no exit evidence, which is precisely this card's defect. The launcher now
   sets `SIGCHLD` back to `SIG_DFL` before forking and is self-sufficient
   rather than relying on CTest resetting dispositions. Verified: the same case
   now prints `TEST_RUNNER_CHILD_EXIT:7` / `..._FAILED:7`.
3. **A comment misstated `PR_SET_PDEATHSIG`'s clearing rule** (dumpability
   rather than set-id / file capabilities / credential change, and about the
   wrong process). Corrected.

One finding was adjudicated and **not** taken: the reviewer called `extern "C"`
on a handler inside an unnamed namespace odd. It is kept, because a signal
handler is entered from outside the program and C linkage is what the language
guarantees for that; a comment now says so. One observation is recorded rather
than fixed: a **grandchild** the test spawns still outlives the timeout kill.
That is unchanged from the `execvp` launcher rather than newly introduced, and
the comment no longer implies otherwise.

### Still open on Linux

The census — how many previously green rows were hiding a non-zero exit — is
measured in the full sweep and recorded below it, not here.
