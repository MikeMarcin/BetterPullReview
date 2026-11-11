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

## Compile each physical host before its long suites

Compilation is an early prerequisite on each physical host, not a global wait
between independent hosts. After freezing the common source and passing cheap
tooling and source checks, configure and build every selected graph on that host:

1. The Release test graph and its selected test executables.
2. The required Debug test graph and its selected test executables.
3. The tests-disabled distribution graph, including every selected architecture
   and variant (for example, both architectures of the macOS universal build).

Record source, presets/configurations, toolchain, commands, actual exits and raw
build receipts in the phase ledger. A configure-only or focused target result
does not prove all selected targets compile. A Debug timing omission does not
waive compilation failures. `tools/release.sh` builds all three local graphs
before its long qualification suites, finding Debug/distribution compile errors
earlier instead of spending the Release suite first.

Mac compilation may run alongside PC compilation. Once its own graphs pass, Mac
qualification may start while the PC finishes compilation. Do not wait for an
unrelated physical host's receipt. On the shared PC, finish Windows and Ubuntu
Release/Debug/distribution compilation before either platform's long suites.
Do not schedule Windows build → Windows full suite → Ubuntu build: that delayed
discovery of a GCC warning-as-error until after hours of tests during v0.3.3.

A failure still rejects its candidate. Stop dispatching its unstarted downstream
stages, retain every running job's ownership and result, and reconcile all failed
rows before preparing a successor. Use the runner's [failure-first option](../../../../docs/development/release-test-profiles.md#prioritize-repaired-failures-without-dropping-coverage)
for successor qualification: prior failed fixture/dependency groups run before
the normal remainder, and both batches still collect all failures. Fixing only
the first failure serializes discovery.
Do not kill unrelated work. Starting independent native qualification earlier
can spend work on a candidate later rejected elsewhere; it does not grant any
old-source pass to corrected source. Reconcile unchanged passing receipts only
under the existing evidence policy.

All selected native compilation and qualification receipts remain required before
coordinated release acceptance/publication. Compilation is not test acceptance,
artifact audit or signing approval. Preserve the macOS script's gate ownership;
do not run a duplicate full suite for a packaging transcript. Keep any permitted
bridge to unchanged qualification explicit and input-bound.

## Run native risk checks before long suites

After the required graphs compile, run the maintained
[risk preflight](../../../../docs/development/release-test-profiles.md#early-native-risk-preflight)
on both Release and Debug before long qualification on that physical host.
The original Git cache ownership and profile-reset public-path courses belong
in this early set on every selected platform; a Mac pass cannot expose or
qualify Linux inode/generation behavior or MSVC's checked lifetimes.
`tools/release.sh` owns both early Mac runs, as well as its later final suites.
Do not add duplicate manual Mac preflights around it.

Prefer a verified independent Ubuntu 26.04 x64 native producer when one is
actually available and authorized. Confirm checkout/candidate, native toolchain,
dependency cache, output ownership and physical host before dispatch. An SSH
alias or Linux environment inside Windows does not establish independence.
Keep the selected Linux baseline and approved signer/key location; do not move
credentials or claim WSL/installation/native-trust evidence from another host.
Independent hosts may complete their own preflight and advance separately.

If Linux is available only as WSL on the signing PC, schedule Ubuntu discovery
first: build its Release/Debug/distribution graphs and run both short preflights,
then finish the Windows graphs and both Windows preflights. Complete all shared
host compilation and risk checks before either long suite, extended Windows
repair qualification or packaging. Keep all Windows/WSL builds, tests and package
work serialized through the existing host lease. Never interrupt a running
Windows job to insert Linux work; resume this ordering at its next safe owned
boundary. Existing live release jobs and frozen procedures remain unchanged.

A functional failure rejects the candidate's unstarted downstream work. Preserve
all native reports and actual exits, including failures from the other mode;
continue other independent discovery only when already safe and useful. A skipped
required row is incomplete early discovery, not a PASS. Use the existing Debug
timing policy for a classified mixed-course timing predicate and retain its raw
failure. Required final selected qualification, artifact and trust receipts
remain unchanged.

### Evidence reuse boundary

The early profile is a separate current-candidate observation. Final selected
rows execute fresh; no early native PASS is imported into the final runner.
The input closure needed for broader selective reuse has not been verified:
source/helper/fixture transitive inputs, generated configuration, native libraries,
compiler/CRT/STL/Qt, containment, storage filesystem capabilities and final artifact
bytes can change independently of a passing test's name. A docs-only-looking diff
or identical source file is not that proof. Do not infer that an old source PASS
qualifies a repaired candidate. Current `--failures-first` receipts only order
work, and the existing sealed same-invocation tooling bootstrap is the only
implemented duplicate-row credit. This change enables no new evidence bridge.
Collection-only recovery may still validate and collect the exact existing native
outputs under the existing policy; it does not fabricate missing native work.

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

## Interpret optimized timing without idle-host polling

Do not impose a quiet-host wait merely to attempt an upper-bound latency check.
A valid complete Release course that meets its unchanged maximum under load can
satisfy that course; it does not prove an uncontended speedup. Preserve the
course's fixture, workload, clocks, resource ownership and other validity
conditions. Minimum-throughput, cold-cache, RSS/heap and comparative performance
claims keep their own preconditions. Never infer those conditions from CPU
idleness or weaken CTest locks/serial declarations.

A failed timing predicate remains a failure. Retain its exact identity, binary,
configuration, workload, metric, limit and raw output. Diagnose functional errors
and collect available process-scoped CPU/memory/I/O observations from the same
interval before blaming contention; process names or a later quiet pass are not
causal evidence. Where the predicate permits it, plan at most one controlled,
identical-binary rerun after removing the suspected contention, retaining both
attempts and the changed conditions. Do not retry until green, average away a
failure, or call an unsupported result a pass. A repeated failure needs correction
or explicit disposition. Do not change an in-flight release's frozen procedure.

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

BPR-2057 makes build/qualification diagnostics automatic in future
`tools/release.sh` invocations. It prints a new external-to-source
`build/release-diagnostics/VERSION.XXXXXX/phases.jsonl` path before builds, with
`context.json` holding observed source commit/dirty state, candidate/version,
producer, platform/architecture and resolved tools. Each configure/build phase
retains UTC timestamps, monotonic elapsed seconds, cwd, argv, actual native exit,
child PID and separate raw stdout/stderr paths while streaming output normally.
The ledger's `summary` command includes failed and pending phase IDs, so callers
do not need to scrape transcripts. Qualification terminal records link the
existing manifest/selection/JUnit/timing/preflight reports and list failed,
skipped, missing, unexpected and not-run rows. Incomplete or malformed reports
are diagnostics, never acceptance. No environment or credential dump is made.

This is phase-level instrumentation, not per-translation-unit compiler profiling.
It covers configure/build/qualification, not package/sign/trust/publication.
An interrupted wrapper leaves an open phase until native state is reconciled.
A signaled child retains its native negative signal status in JSON and returns
128+signal to the shell. Raw logs stay private, outside published release assets;
inspect them before sharing, since a tool can print sensitive information.

A coordinated or resumed run passes existing absolute `--phase-ledger` and
`--phase-context` paths to `tools/release.sh`; it must not reset the original
request clock. A default ledger starts at this script invocation, not at an
unobserved earlier user request. Windows/Linux commands use the same portable
ledger wrapper with `run --log-dir /absolute/private-logs`. Contexts must retain
current source/configuration, toolchain, physical-host and candidate identity;
a diagnostic context is not an evidence-reuse certificate.

BPR-1675 adds `scripts/release_phase_ledger.py` as an append-only observation
helper around these existing commands. It does not select tests, schedule hosts,
reuse verdicts, sign or publish. Initialize one new external ledger at the actual
request start, with a nonsecret JSON context naming source/configuration, resolved
tools, cache condition, physical-host lease and evidence roots:

```sh
python3 scripts/release_phase_ledger.py --ledger /absolute/request.jsonl init \
  --request-start ACTUAL_UTC_INSTANT --context /absolute/context.json
python3 scripts/release_phase_ledger.py --ledger /absolute/request.jsonl run \
  --phase build --context /absolute/context.json --log-dir /absolute/private-logs -- cmake --build --preset PRESET
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
