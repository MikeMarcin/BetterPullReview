# Tests-enabled optimized qualification

BPR-1674 / `release-30min-portfolio-r1` adds ordinary Release qualification presets.
They preserve `NDEBUG`, Release Qt/CRT/STL and the test expectation mechanisms.
The owner subsequently authorized normal Release qualification plus focused patch
regressions and a five-command native Debug residual for bounded incremental
releases. Major, Qt/toolchain/platform, broad or unknown changes require
comprehensive native Release and Debug. Use the
[operational profile contract](../../../../docs/development/release-test-profiles.md)
for exact named heavy owners, scope declarations and commands. Normal PASS never
qualifies omitted heavy claims; BPR-1653 strict Ubuntu RSS remains a release
blocker, without extending the published v0.3.1 exception. No profile supplies
signing, native trust, legal or publication approval.

## Prepare one bounded Windows discriminator

Reuse the owned tests-enabled Visual Studio tree. Configure with
`cmake --preset windows-qualification-release`. Build only these targets first,
with bounded parallelism (normally four on a shared host, never more than eight):

```text
test_better_pull_review_qt_workbench_window_tests
test_better_pull_review_qt_github_tests
test_better_pull_review_diff_compute_core_tests
test_better_pull_review_application_diff_host_tests
test_better_pull_review_application_github_host_tests
test_qualification_assertion_probe
tool_better_pull_review_network_sandbox
```

The first configuration/build and cache condition are separate elapsed phases.
An existing optimized benchmark cache is not a warm complete test graph. A build
under documented foreign pressure supplies no uncontended build-speed claim.
Admit timing courses only on the leased quiet physical host; Windows and WSL share
that host. Preserve any wait or refusal and observe foreign starts during a course.
Run native Windows Qt through the existing qualified limited interactive context.
No live desktop driving or capture is involved.

Run the existing runner, with Python assertions enabled, a new output directory,
and one worker:

```text
python scripts/run_test_suite.py --preset windows-qualification-release --jobs 1 --tests-regex "^(qt_workbench_window_scale_100|qt_composed_inline_drawer_scale_100|core_diff_compute|application_diff_host|application_github_host)$" --output-dir ABSOLUTE_NEW_OUTPUT_DIRECTORY
```

Set `BPR_TEST_CASE_TRACE=1` in that process's environment. The runner validates the
full configuration's schedule, then runs the actual documentation structure,
documentation issue-graph contract and issue allocation contract preflight before
the bounded selection. Inspect its committed MASK pin and actual bootstrap result.
Do not repair bootstrap by switching agent roles or transplanting unrelated pins.
A failed preflight does not qualify the five commands.

Retain `Testing/Temporary/LastTest.log` before another CTest invocation replaces it.
Validate the actual manifest and log through
`scripts/qualification_case_inventory.py --configuration Release --platform Windows`
with absolute `--manifest`, `--log` and `--output` paths. It requires all 237 ordered
case occurrences and the exact configuration/STL/workload/latency/sweep evidence.
Run `scripts/qualification_assertion_controls.py` with that manifest, the actual
Release probe image under `build/windows-dev/tests/core/Release`, and an absolute
new `--output-dir`. It requires an ordinary expectation failure, the caught-failure
exit backstop and four actual drawer failures. The controls can only tighten a
named limit to zero; ordinary runs retain all four 100 ms limits. Keep negative
control output apart from behavioral pass evidence.

The portable `qualification-release` configure/build/test presets select the same
standard features with Release in a separate Ninja tree. Their full macOS/Linux
activation, actual Qt/runtime identity and native results remain M3 work. Never
project the Windows diagnostic cost onto another platform or hybrid-link Debug Qt.

## Current qualification selection

The runner owns the three bootstrap checks once per locked invocation. A passing,
complete receipt may satisfy their second occurrence only with the same live
invocation ID and equal source/manifest/environment/tool/deployed-helper seal.
Missing, stale, failed, skipped, interrupted or changed proof runs fresh instead.
Changed input across execution refuses complete qualification. Receipts identify
fresh rows and the reused same-invocation bootstrap rows separately. Unknown input
closure disables reuse. This is not a reusable native, security or release verdict.

The hostile architecture-tool corpus reads current native/product sources as
fixture inputs and executes the actual checker and CMake. Its complete native/tool
closure has not been accepted; cross-request reuse remains unready. Current-source
architecture and manifest checks, dependencies, actual artifacts, source/SPDX,
security, legal applicability, native performance and trust remain fresh.
The eight composition methods retain their distinct assertions. In particular the
generic tests-off Release image, tests-on optimized image and distribution image
are not interchangeable. No composition build was removed by this pilot.

## Debug residual and promotion boundary

The explicit incremental profile uses the five-command Debug residual. Comprehensive releases and the following owning changes require broader native Debug:
The following triggers must also invalidate any future narrower Debug proof:

| Trigger | Required evidence |
|---|---|
| Qt, compiler, CRT/STL, ABI, platform baseline, build flags or assertion infrastructure | Complete affected-platform Debug plus optimized qualification. |
| Container/iterator ownership, allocation, threading, cancellation, lifecycle, teardown, Qt models/widgets or composed callbacks | Known complete affected closure including public compositions; unknown closure means full Debug. |
| Test selection, fixture, assertion, scheduler, isolation or helper changes | Both configurations for affected cases and meaningful negative controls. |
| Broad integration, missing native input, unclassified change or failed/stale/interrupted proof | Complete native Debug; no inherited exemption. |

The owned `Q_ASSERT` in steady columns and Debug Qt DLL/CRT/STL diagnostics remain
covered by the full Debug requirement. Release cannot restore compiled-out Qt DLL
assertions. Both the 6,000-line Debug inputs and 40,000/30,000-line Release inputs
remain required; neither substitutes for the other. Windows Release additionally
runs two allocation sweeps that checked STL skips. Strict optimized memory/cache
budgets remain active, including the unresolved Ubuntu RSS failure. Do not waive
that failure or infer a new exception from the published v0.3.1 exception.

Promotion requires one complete stabilized native Release result per platform,
all optimized budgets and occurrence/configuration inventories, and an explicit
sound residual Debug closure. The M3 rehearsal measures the complete authorized
request-to-artifact route after that decision. Signing, publication and unresolved
M8/M9/security/legal acceptance retain their separate authority.
