# BPR-1665 Qt 6.12 upgrade checkpoint

Mandate `qt-6-12-r2`; assignment `/root/qt612`, single milestone
`qt612-upgrade`, branch `codex/qt-6-12`. Baseline:
`1a7d74f003b656469f5c6822c655a314fb58f663`. Tested source:
`b433d515f5a3a513bcecc0f520d8d04c38454c6a`; subsequent closure changes only
documentation. The owner approved macOS 14.4+ for new Qt 6.12 builds and the
full macOS suite, with three-platform v0.3.1 release assigned subsequently.

## Engineering result

Qt provider, source inventory, Linux companion archive, active policies and
fixtures use official Qt 6.12.0. Actual source/ICU downloads match the published
SHA-256 values; the three raw ICU 73.2 files are unchanged. Provider fresh-cache,
offline and refusal controls pass. Full dev build and composed app/runtime
checks pass, including a fresh tests-disabled Release composition build.
The Qt drag fixture uses the correct QPointF constructor for 6.12+ while
retaining earlier caller-installed Qt support. The recipient fixture uses the
established contained scratch-root helper after the full gate exposed its
existing policy violation. No product C++ or visual design changed.

The maintained full suite completed all 811 rows, exit 0, in 4407.875 seconds:
810 passed, one configured Debug skip, zero failures, no missing/unexpected
rows. Four independent jobs and 421 RUN_SERIAL rows were validated. The exact
skip and inner-platform/filesystem limits are recorded in the
[ledger](../../../verification/RFC-BPR-0088.md#completed-full-suite-evidence-and-skips).
Runtime evidence is external in `/tmp/bpr-qt612-run/full-suite-r2`; final
JUnit/timing reports are authoritative. The stale build LastTestsFailed.log
names the earlier dependency acquisition failure and is retained under an
explicit stale-prior name, not counted as a final failure.

The first full attempt stopped at268/811 after ENOSPC, exit130; it is retained
as incomplete evidence. Only a duplicate task SDK cache and1281 task object
intermediates (2.36GiB) were removed. The rerun passed with no threshold or
warning waiver. No build ran alongside the performance sections; the separate
bug owner paused compilation before the final PR-search course, which passed
14.91 seconds.

## Ownership and next action

All own commands, including full-suite session84214, have drained. No server,
attended candidate or background writer is retained. The coordinator requested
retaining this task's build tree (about15GiB), private SDK (about394MiB) and
installer environment (about35MiB) for the scrolling repair/release owner.
These are task-owned disposable resources after their consumers finish; they
must not be removed while the other implementer uses the private SDK. Shared
CPM cache, main/prior builds and release assets were preserved. External
reports and verified downloads remain in `/tmp/bpr-qt612-run`.

The only main mutation was the explicitly approved bookkeeping reservation
commit `c2aece8aa`; `25198b828` adopted its identical receipt here. Unrelated
main edits remain preserved. The parallel bug task subsequently advanced the
shared historical counter; the final documentation/finish guard correctly
refused this stale branch. Only its existing counter/receipt commit
`84e06ec28` was imported here as `cb4b944da`, without a new allocation or main
mutation. No integration, version bump, push or publication
was performed. The coordinator must reconcile dirty main, integrate the
upgrade and separate Threads scrolling/flicker fix, then conduct full native
three-platform and exact-artifact v0.3.1 qualification/publication. That bug is
not fixed or accepted by this milestone. Windows/Linux/Intel execution,
trust/vulnerability/native-device obligations and M8/M9 gaps remain explicit.
No owner desktop was driven or captured. No human criterion belongs to this
upgrade, so no attended-review session is required.

## Plan binding limitation

Target-only `milestone_checkpoint.py bind`/plan discovery refuses because an
unrelated existing shared-action-buttons plan lacks boundary fields. Explicit
`milestone_plan.py bind --plan docs/rfcs/plans/qt-6-12-v1/milestone-plan.json`
resolves this plan and its obligations. `checkpoint.json` uses that exact
binding and canonical schema/repository-state helpers. Final
`milestone_checkpoint.py check` verifies consistency against the explicit
binding; it does not grant engineering acceptance. The unrelated plan remains
unchanged; original refusal: external `checkpoint-bind-attempt.json`.

## Optimized release obligation

The Debug full suite configures `application_large_review_cache_bytes` to exit77
and report Skipped (BPR-1265), rather than pass. The subsequent release owner
must run this row from a tests-enabled **Release** build on the exact integrated
Qt6.12 candidate. The v0.3.0 RSS exception does not authorize v0.3.1. Native
Windows/Linux and whole-platform suite skip inventories remain release intake
obligations; this milestone supplies only actual macOS evidence.


Run the optimized-only row after integrating the final release source. These
commands use a new tests-enabled Release tree, separate from the distribution
graph; the latter disables tests. They are the required next release action,
not evidence already executed here. Run the test with no build alongside it.

```sh
cmake --preset dev -S /Users/jaeger/Documents/GitHub/BetterPullReview -B /Users/jaeger/Documents/GitHub/BetterPullReview/build/qt612-release-qualification -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4
cmake --build /Users/jaeger/Documents/GitHub/BetterPullReview/build/qt612-release-qualification --target bench_better_pull_review_large_review --parallel 2
ctest --test-dir /Users/jaeger/Documents/GitHub/BetterPullReview/build/qt612-release-qualification -R '^application_large_review_cache_bytes$' --output-on-failure
```

The registered CTest command retains the maintained network containment and
artifact paths. Native Windows Release uses its selected configuration
(`ctest --test-dir <absolute-tests-enabled-tree> -C Release -R
'^application_large_review_cache_bytes$' --output-on-failure`); each release
platform still needs its full native suite and complete configured-skip report.
