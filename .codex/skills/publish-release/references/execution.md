# Coordinated execution and recovery

Use the existing native producers and publisher. This reference schedules their
work and recovery; it adds no release path or acceptance waiver. The
[v0.3.1 retrospective](../../../../docs/verification/release-v031-procedure-findings.md#retrospective-and-adopted-workflow-2026-10-20)
records the observed costs behind these changes.

## Establish the run once

Keep one durable record outside source for the selected version/platforms,
source commit, producer roots and contexts, exact owned task/process identities,
stage receipts, result paths and next action. On resumption, reconcile it with
actual native state before dispatch. An existing running stage is something to
wait for; a completed stage is something to validate. A handoff alone invalidates
neither. Do not create a second suite or signing job to obtain a new transcript.

A new producer checkout also needs the repository's ignored portable test
tooling. Use the normal MASK register/repair procedure at the committed lock,
inspect its owned-checkout plan and readiness receipt, then run the actual
[three cheap tooling checks](../../../../docs/development/build-and-test-reference.md#complete-suite-timing-and-focused-work)
before expensive configuration/builds. The complete-suite runner runs these
configured checks as a fresh prerequisite preflight. Its second occurrence in the
selected graph may reuse only the same invocation's complete passing proof with
an unchanged input seal; the report marks those rows separately. A stale or
changed receipt runs fresh. No cross-request native/tool verdict reuse is active.
A failed preflight means the full graph was not started; preserve that result
and repair the prerequisite before dispatch. Neither an early check nor the
recorded lock declaration is complete qualification or host-loading evidence.

Before long work, check the actual producer's tool PATH, canonical dependency
cache paths, free space, native signing configuration and result locations.
Exercise the same dispatch/collection route with harmless stderr plus exit 0
and stderr plus a chosen nonzero exit. Preserve both streams and the true native
exit. A PowerShell `NativeCommandError` from a CMake warning is not a test result.
For Windows/WSL, avoid the observed `Start-Process -Wait` wrapper stall; use a
native process bridge that drains both streams and observes the actual child
exit, rather than treating an outer task state as completion.

Use the approved native Windows producer context. v0.3.1 GUI courses refused
the direct SSH context with `STATUS_UNTRUSTED_MOUNT_POINT` and passed in the
existing interactive producer context without relaxing containment. Resolve
tools and configuration inside the process that will run them. WSL needs its
configured login/tool PATH and an accessible Windows working directory; see
[Linux dispatch](linux.md). Reuse existing credentials through supported helpers.

## Put early feedback before the common freeze

Finish blocker engineering, then run a small set of affected public interaction
courses on each relevant host. Choose them from the changed behavior rather
than making a second broad suite. For asynchronous UI changes, include the
composed navigation/persistence path and immediate subsequent interaction;
feature-only checks missed v0.3.1's active-reader/menu race. Wait on the actual
public completion in fixtures; elapsed sleeps cannot establish that completion.

Before expensive native builds/gates, run the composition driver's existing
cheap source-manifest/inventory method from the selected source checkout:

```sh
python3 tests/end_to_end/composition_contract_tests.py --cmake cmake \
  CompositionContractTests.test_async_mutation_host_inventory_rejects_every_direct_bypass
```

Resolve Python/CMake to the approved native tools (use their absolute paths in
a dispatched command). This selects the actual owning digest and adversarial
source-tree checks without configuring/building the nested Release application.
A failure rejects this candidate; follow the recorded source-review/reseal
procedure rather than merely replacing a digest. This is early feedback only:
the final full suite must still run all eight composition methods, including
the real tests-disabled build and binary/runtime checks. The driver stops on a
failed method and explicitly reports remaining methods as not run.

Commit one common release candidate before the final native gates. If a failure
requires a source correction, mark the old candidate rejected and prevent its
unstarted downstream stages. Retain any running job's ownership and raw result;
do not stop unrelated jobs. Validate affected evidence for the new source under
the release policy. Do not silently reuse old-source acceptance.

## Schedule by physical host and prerequisites

- Run Mac production independently from the signing PC. Windows and its Ubuntu
  WSL instance share a physical host: serialize builds, normal suites, optimized
  performance courses and production there. Preserve the maintained suite's
  four-job scheduling, locks and exclusive timing/native/global-resource rows.
  Windows may overlap only the maintained explicit isolated-correctness
  allowlist; preserve its audited output/executable locks and default serial
  classification. Follow the [suite policy](../../../../docs/development/build-and-test-reference.md#complete-suite-timing-and-focused-work),
  not a new release-specific concurrency override.
- On Mac, `tools/release.sh` already runs the normal gate. Do not run a duplicate
  full suite first. Preserve separate tests-enabled optimized and tests-disabled
  package graphs, and reuse canonical build/dependency caches after current
  configuration and identity checks.
- Prepare notes, source/material comparisons, native download-verification
  commands and review packet structure while eligible work runs. Finalize each
  packet against actual immutable outputs before review; fresh scans and native
  trust remain bound to the delivered artifacts.
- Start each required human criterion as soon as all agent-owned work for that
  criterion is complete. Do independent work during its wait. Carry forward
  existing authorization and policy-permitted evidence with their precise scope;
  do not ask again merely because an agent or phase changed. A changed Qt/legal
  material decision and a factual security applicability disposition remain
  distinct obligations. Follow the attended-review handback boundary.

## Recover the failed layer

Require actual native exit, expected source, terminal stage and complete result
inventory before advancing. A successful configure or zero outer wrapper does
not mean a suite completed. Preserve configured skips and failed predicates;
reconcile an existing exception separately and only within its explicit scope.

| Observation | Next action |
|---|---|
| Native job still running | Wait on the owned job; do not redispatch. |
| Native stage passed; transfer/parser/collector failed | Repair collection and rehash/reparse the existing outputs. |
| Wrapper stalled; native result appears complete | Verify the receipt, exact inputs, exit and native quiescence before retiring only owned wrapper processes and continuing. |
| Product gate failed | Retain failure, diagnose and correct it; revalidate affected inputs. |
| Signing/upload outcome uncertain | Inspect actual artifact or remote state before any retry. |

Reuse a passing result only when policy permits and its source, configuration,
toolchain, selected inputs and outputs still match. Record that bridge explicitly.
An orchestrator repair does not by itself require rebuilding, rerunning a
159-minute suite or signing again. A source/dependency/package change may.

## Measure and close

Record UTC start/end and elapsed time by build, test, optimized course,
package/sign/audit, transfer, review handoff/handback, publication and downloaded
trust stage. Include attempts, dependency waits, actual exit, source/input
identity and raw receipt locations. Distinguish native work, wrapper failure,
collection failure and human handoff intervals. Summed test durations and
overlapping platform runs are not elapsed critical-path time.

Once accepted, keep signed assets and source fixed through publication and
download verification. Make latest-link, ledger and retrospective edits afterward
in a separate administrative commit. Report measured savings only after a later
comparable run; instruction changes alone establish no speedup.

## Request phase ledger

BPR-1675 adds `scripts/release_phase_ledger.py` as an append-only observation
helper around these existing commands. It does not select tests, schedule hosts,
reuse verdicts, sign or publish. Initialize one new external ledger at the actual
request start, with a nonsecret JSON context naming source/configuration, resolved
tools, cache condition, physical-host lease and evidence roots:

```sh
python3 scripts/release_phase_ledger.py --ledger /absolute/request.jsonl init \
  --request-start ACTUAL_UTC_INSTANT --context /absolute/context.json
python3 scripts/release_phase_ledger.py --ledger /absolute/request.jsonl run \
  --phase build --context /absolute/context.json -- cmake --build --preset PRESET
python3 scripts/run_test_suite.py --preset PRESET --jobs 4 \
  --output-dir /absolute/new-results --phase-ledger /absolute/request.jsonl \
  --phase-context /absolute/context.json
python3 scripts/release_phase_ledger.py --ledger /absolute/request.jsonl summary
```

Initialization refuses an existing ledger. Failed and launch-failed children keep
their true status; interrupted phases stay pending until the existing native
receipt/process is reconciled. Never restart the request clock after setup,
repair, build, cache warming, transfer or review. Record external native receipts
with their original UTC interval, monotonic duration, exact child PID/exit,
source and configuration; retain the raw receipt. A resumed wrapper is not a
new request. Source/config/tool/cache/lease facts belong to each phase; changed
facts never rewrite prior phases. Do not put credentials or authenticated review
URLs in contexts. Include explicit not-run reasons for signing, publication,
trust, human review or a gate blocked by failed prerequisites. Summed overlapping
phase work is distinct from elapsed request time. One-time workflow engineering
and a later reproducible release request remain separate reported clocks.

## Incremental normal and heavy qualification

The owner-authorized [profile contract](../../../../docs/development/release-test-profiles.md)
selects normal tests-on Release plus named patch regressions and native Debug
residual for bounded patches. Use `--profile normal --change-scope incremental
--include-tests-regex REGEX` with the existing runner, then `--profile
debug-residual` on the Debug preset. The baseline is retained; a focused regex
is an addition. Request relevant `--heavy-group` owners when their inputs change.
Major/Qt/toolchain/platform/broad/unknown changes request comprehensive Release
and Debug. Missing scope is refused. Existing generic runner callers remain
comprehensive. Source, actual distribution configuration, artifact integrity,
SPDX/corresponding-source/notices and security checks remain fresh.

Inspect `selection.json` before calling a normal result complete: it names every
selected and omitted obligation. Normal PASS is not heavy/resource qualification
and cannot supply BPR-1653 resource evidence or extend v0.3.1's one-release RSS
exception. The separate owner-authorized RSS criterion and complete course are
recorded on that card. No profile
change grants a version bump, signing, publication, installation or trust approval.
