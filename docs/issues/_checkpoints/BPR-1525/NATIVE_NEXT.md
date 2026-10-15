# BPR-1525 native continuation

Resume SAME `/root/bpr1525_implementer` at the checkpoint commit. BPR-1525
remains In Progress. Root preserves this worktree's source ownership. Commands
below are pending, not passing evidence. Root explicitly authorized this
quiescent checkpoint while original full gates keep their stations.

## Reconcile the station first

Through SSH alias `aito-main-win`, read:

```powershell
Get-ScheduledTask -TaskName BPR-v021-native-production-8f0d2541 | Select-Object State
Get-Content C:/Users/mikem/AppData/Local/BetterPullReviewRelease/v021-windows-production.receipt.json
Get-Content C:/Users/mikem/AppData/Local/BetterPullReviewRelease/v021-windows-production.log -Tail 40
```

The task must actually have exited. Collect full failed-test inventory and
`ctest.log`, `junit.xml`, `timings.json` under
`C:/Users/mikem/bpr-v021/build/test-results/windows-dev/20261015T053241.959016Z`.
Do not stop/restart it. Root must release the native slot; do not compete with
another Windows/WSL writer. Mac compilation likewise waits for root's explicit
release from `mac_release_recovery`.

## Shortest native reproduction before runtime ACL mutation

Replay the manifest environment, then preserve failure of:

```powershell
C:/Users/mikem/bpr-v021/build/windows-dev/Debug/better_pull_review_network_sandbox.exe C:/Users/mikem/AppData/Local/Python/pythoncore-3.14-64/python.exe C:/Users/mikem/bpr-v021/tests/scripts/network_isolation_counterexamples.py child windows-appcontainer
```

Capture `sys.executable`, canonical selected executable/runtime and its ACL.
Program Files Python is a different installation. Compare captured versus
inherited stdio if the image diagnosis is insufficient. The configure probe
uses captured stdout/stderr as the actual counterexample does. Grant only the
named test-container SID RX on the canonical selected runtime, after a failed
probe, and re-probe; no broad package ACE or network capability. The actual
child counterexample still must prove grandchild AppContainer and WSAEACCES10013.

## Isolated corrected source/build

Mac root `/Users/jaeger/.codex/worktrees/v021-windows-gates/BetterPullReview`,
branch `codex/v021-windows-gate-fixes`. Transfer exact checkpoint branch as a
Git bundle against prerequisite `8f0d25418cd4278065d5a043dabaa728feb202f1`.
Use a separate absent owned checkout, e.g. `C:/Users/mikem/bpr1525-repair`;
never edit the original producer or lease-owned `C:/code/git/BetterPullReview`.
Reconcile cache writers before reusing verified CPM sources. Keep build separate.

Use the normal MSVC environment from
`C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat`,
normal discovered CMake/Python/Git, and the original Interactive Limited task
session/token. No old-Git/interpreter substitution. No signing/auth/publication.
From the repair checkout:

```powershell
cmake --preset windows-dev
cmake --build --preset windows-dev --parallel 8 --target test_better_pull_review_application_diff_host_tests test_review_cycle_git_readers test_git_metadata_harness test_better_pull_review_gh_cli_transport_tests test_better_pull_review_github_provider_tests tool_better_pull_review_network_sandbox
ctest --test-dir C:/Users/mikem/bpr1525-repair/build/windows-dev -C Debug --output-on-failure -R '^(github_recorded_fixture_contract|test_network_isolation_(child|external|loopback|concurrent_launch)|application_diff_host|review_cycle_git_readers|git_metadata_harness|gh_cli_transport|github_provider)$'
```

Repeat `application_diff_host` on an idle station with unchanged10ms assertion.
`/tmp/bpr1525-matcher-wakeup.cpp` is the previous worker's disposable race
harness: still needs correct core linkage, never executed. Verify250 abandon
races, normal completion, pre-stopped calls, or meaningful existing equivalents.

The provider30.04s timeout remains unproven. Trace a disposable test candidate
with main-case progress markers, identify last case, then diagnose actual
contained refusal/deadlock/other cause. Keep normal30s acceptance bound intact.
Do not pin uncontained without an actual measurement under the existing contract.

The original run also timed out `qt_diff_workbench_scale_100` after120.01s
(row418/705). Scale125 was starting at checkpoint. Collect all subsequent rows
before deciding scope. No UI source or acceptance timeout has been changed;
apply the required frontend-design skill before any later UX/visual change.

## Nine contained measurements and final obligations

`/tmp/bpr1525-measure-native.py` is a prepared disposable script (syntax checked
only). Copy it to an owned external Windows evidence directory. It reads the
original manifest, replays ENVIRONMENT and ENVIRONMENT_MODIFICATION, prepends
the sandbox to each of the9 currently uncontained commands, and retains actual
stdout/stderr/status/timing. Supply absolute `--manifest`, `--launcher`,
`--output` and `--cwd`. Run serially only after quiescence and interpreter proof.
Inspect every refusal before replacing a pin. A contained pass means fix the
registration; never invent a refusal. Then run `test_network_isolation_manifest`
and `tests/scripts/verify_test_network_isolation_tests.py`.

Verify literal-prefix behavior with native Git2.55 and local Apple Git2.50.1;
retain conservative K exclusion. In a disposable build change/remove the new
classification's expected answer and prove unknown/reversed findings still fail;
restore and recheck exact candidate. Unknown differences remain fatal.

When Mac slot is released, build focused portable host/reader/harness/CLI checks
in this worktree, run relevant architecture gates, changed-source tidy,
format22 and documentation. No compilation has run here yet. Self-review,
record every AC and exit against actual revisions and canonical finish only
after all pass. Integrate to main only with root coordination. Corrected final
source still needs all release-platform gates and packages; baseline packages
are not final-source evidence.
