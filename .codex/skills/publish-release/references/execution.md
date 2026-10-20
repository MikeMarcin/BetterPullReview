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
  classification. Follow the [suite policy](../../../../docs/development/build-and-test.md#complete-suite-timing-and-focused-work),
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
