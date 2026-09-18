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

## Windows: the launcher states the exit, and the uncontained set is measured (bpr-win, 2026-09-17)

The tip is `86b049014577bb02ec6952868215b8c1d31a5589`.

### The marker

`tests/network_isolation/windows_network_sandbox.cpp` prints
`TEST_RUNNER_CHILD_EXIT:<n>` after `GetExitCodeProcess`, and
`TEST_RUNNER_CHILD_EXIT_FAILED:<n>` when the status is not zero, then flushes,
before `CloseHandle`. Unsigned `%lu`: a Windows crash arrives as a status
rather than a small number — `0xC0000005` is 3221225477, and this run met
`0xC0000409` once already — and `%d` would print those negative. No CMake or
gate change; the inherited failure expression is on every row already.

### The measured gap: thirteen rows have no launcher, and it is currently empty

A `WINDOWS_UNCONTAINED` row clears the isolation prefix entirely
(`cmake/BprTestIsolation.cmake` ~702), so it runs with no launcher and
therefore carries no `TEST_RUNNER_CHILD_EXIT` marker at all. That set is
[[BPR-0875]]'s thirteen-row cluster — precisely where a shutdown fault would
hide.

**Measured on 2026-09-17 at `86b04901`: every green one of the thirteen exited
0. The one red row is red on both signals.** So the gap is real and, today,
empty.

| Rows | Result |
| --- | --- |
| `application_workbench_composition_contract`, `application_overview_since_review_contract`, `application_review_behaviour_contract`, `application_saved_server_contract`, `application_appearance_theme_customization_contract`, `application_epoch_persistence_github-com`, `application_epoch_persistence_ghes-3_18`, `application_epoch_maintenance_public_path`, `application_appearance_gesture_contract`, `application_appearance_store_fallback_contract` | green, exit 0 |
| `application_uiforetw_selected_diff_contract` | red on both signals: exit 1 **and** its pass expression does not match. [[BPR-0859]]'s capture fault |
| `application_native_notification_attended_contract`, `application_review_round_trip_composition_contract` | green, exit 0, and **judged by the exit status directly** — see below |

**The last two never needed this card's guard.** They carry a
`FAIL_REGULAR_EXPRESSION` but no `PASS_REGULAR_EXPRESSION`, and it is only a
pass expression that makes CTest ignore a non-zero exit. Their verdict is the
exit status, with the failure expression as an additional veto — the stronger
signal, not a weaker one. They are the part of the uncontained set this card's
defect cannot reach.

### The method, so this is re-runnable rather than re-invented

For each row in the set:

1. Take the row's own command, environment and `PASS_REGULAR_EXPRESSION` from
   `ctest --test-dir build/windows-dev --build-config Debug --show-only=json-v1`.
   The registry, never a card's prose: five of these thirteen names lacked
   their `_contract` suffix in a dispatch note, and a regex built from the prose
   quietly matched 8 of 13.
2. Run that command directly, with that environment, from the row's working
   directory.
3. Compare the exit status against whether the pass expression matched. A row
   that matches while exiting non-zero is the defect this card is about.

The driver used is `uncontained_exits.py`, kept with the station's task
scripts; it is thirty lines and the three steps above are all it does.

### When this measurement goes stale

**When a row is newly pinned `WINDOWS_UNCONTAINED`, re-run it.** That is the
only way the set grows, so it is the only trigger needed, and [[BPR-0872]]'s
rule is what moves a row into the set. A result that is empty today says
nothing about a set that has gained a member since.

### Deliberately not built: a no-container launcher mode

Running these rows under the launcher in a "report exit only" mode was
considered and declined. The question it would answer has been answered
without it, so the machinery would be justified only by the possibility that
the answer changes later — and that is better served by re-running the
measurement above than by a permanent second code path on the isolation seam.

### Two Windows defects found by running the contract, both fixed here

- `tests/scripts/verify_test_network_isolation_tests.py` pinned the cause
  vocabulary at five terms, so the contract failed the moment main landed the
  two this station measured. It now names `child-process-denied` and
  `repository-content-outside-checkout`.
- This card's own new row — the one proving CTest fails a marker-printing child
  that exits non-zero — wrote its generated `CTestTestfile.cmake` with a raw
  Windows interpreter path. CMake reads a backslash in a quoted argument as an
  escape, so the file listed no tests and the assertion counting them failed:
  a broken generator wearing a broken assertion's clothes. The interpreter path
  now uses forward slashes, as the generated script path beside it already did.
  This would fail on any Windows station, not only this one.

Both isolation gates pass at this tip: `test_network_isolation_structure_contract`
3.53 s and `test_network_isolation_manifest` 6.48 s.
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

### The census: Linux's shutdown tail is clean, and every row was measured

Full stock sweep at **`88dc42fc4ecbe4a683994b07f22fba6fad7304c5`** — the pushed
lane tip — with `cmake --preset dev --fresh` (exit 0), `cmake --build --preset
dev --clean-first` (exit 0, 140.35 s) and `ctest --preset dev -V -j10`
(`build/r2-logs/dsp7-sweep-ctest.log`, 756 KB; census in
`dsp7-0890-census.log`).

**Every one of the 369 rows printed a `TEST_RUNNER_CHILD_EXIT` line. No row was
left unmeasured, and no second pass was needed** — on Linux every configured row
is registered through `bpr_add_network_isolated_test`, so every one goes through
the launcher. The macOS measurement needed a follow-up run for 144 rows; this
one did not need one at all.

The population that matters: **198 of the 369 rows carry a
`PASS_REGULAR_EXPRESSION`**, which is exactly the set where CTest ignores the
child's exit status outright. Property distribution at this tree: 367 rows carry
both fail entries, `rfc0017_release_absence_negative_control` carries the run
verdict alone (`EXPECTS_NONZERO_EXIT`), and `application_automated_run_budget`
carries neither (`EXPECTS_FAILED_RUN`). Linux has no equivalent of macOS's
enumerated `gh_cli_git_smart_transport` exception, so it is 367/1/1 against
macOS's 379/1/2.

**Two green rows carry a non-zero child exit, and both were already declared:**

| Row | Child exit | Why it is green |
| --- | --- | --- |
| `application_automated_run_budget` | **1** | `bpr_expects_failed_run` — the row whose subject is a failed run |
| `rfc0017_release_absence_negative_control` | **1** | `bpr_expects_nonzero_exit` — macOS found this one and declared it; the declaration carries to Linux unchanged |

**366 green rows exited 0. Zero green rows were hiding an undeclared non-zero
exit, and no row died of a signal.** That is the same measured zero macOS got,
reached here in one pass, and it is what makes this guard a regression net
rather than a bug fix on this platform too. The acceptance line "the card
records how many previously green rows were hiding a non-zero exit, on each
platform" now reads: macOS **one** (`rfc0017_release_absence_negative_control`,
by design, now declared), Linux **zero**.

### Sweep totals against the baseline

| | dsp-005 at `2b2e5549` | dsp-007 at `88dc42fc` |
| --- | --- | --- |
| Rows configured | 367 | **369** |
| Passed | 367 | **368** |
| Failed | 0 | **1** |
| Wall at `-j10` | 479.73 s | **484.61 s** |

Two rows were added to the graph between the two commits, and the wall time is
**+4.88 s** — so supervising every row instead of `execvp`ing costs nothing
measurable. The `fork`, `waitpid` and two `printf`s are lost in the noise of a
484-second suite. `application_composition_contract` at 471.69 s is the critical
path and essentially sets the wall time by itself.

**`sqlite_epoch_catalog_behavior_contract`, which the previous writer flagged:
Passed, 164.62 s.** It timed out at 180.03 s in both of that writer's
full-suite runs and passed in 122.65 s alone. Here it passes *inside the full
suite*, but with only **15.4 s of margin** against its 180 s budget, and it is
42 s slower than it is alone. So the row is not fixed, it is load-sensitive and
this sweep happened to land on the right side of the budget; it stays worth
watching rather than being called resolved.

### The one red row is not this card's, and not new work by this station

`test_network_isolation_structure_contract` fails, **deterministically and for a
reason that predates this change**. It carries no `PASS_REGULAR_EXPRESSION`
(confirmed against `ctest --show-only=json-v1`), so CTest has always decided it
on the child's exit status; running
`tests/scripts/verify_test_network_isolation_tests.py` directly, with no
launcher in the picture at all, exits **1** with the same assertion. It was
green at the `2b2e5549` baseline.

```text
FAIL: test_every_pinned_exemption_names_a_measured_cause (__main__.ConfiguredManifestTests.test_every_pinned_exemption_names_a_measured_cause)
AssertionError: Items in the first set but not the second:
'child-process-denied'
'repository-content-outside-checkout'
```

`scripts/verify_test_network_isolation.py` gained two new
`WINDOWS_UNCONTAINED_TESTS` causes — `child-process-denied` on twelve rows
(`:238-251`) and `repository-content-outside-checkout` on two (`:254-255`),
both documented at `:60` and `:66` — when bpr-win's containment measurements
merged at `d3b5a3ef`, and the expected set pinned at
`tests/scripts/verify_test_network_isolation_tests.py:573-583` was never
extended to match. The tripwire is doing its job: it exists so that a new
exemption cannot be added without someone recording that its cause was
measured.

**Not fixed here, deliberately.** It is outside this dispatch's three cards; the
gate half lives under `scripts/**`, which this station may not edit; and
mechanically pasting the two strings into the expected set would defeat the
assertion's whole purpose without anyone confirming bpr-win's measurements.
Reported to `bpr-main-mac` as an uncarded defect needing an ID.
