---
schema: "implement-rfc.card.v1"
id: "BPR-0842"
title: "The agent target gate cannot tell the adapter's Kernel32 link from the supervisor's"
status: "Done"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0837 slice","found_at":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","run":"run-20260916-stations","dispatch":"dsp-003"}
acceptance: [{"criterion":"AC1 removing only the adapter's Kernel32 link fails the gate with the target contract message, and removing only the supervisor's link fails with a supervisor-specific message","outcome":"pass","revision":"afb15941e50f0b7e76a764749ea345b05c2a140c","evidence":{"kind":"command","detail":"cmake -DBPR_ROOT=<git archive copy> -P <copy>/cmake/VerifyArchitectureBoundaries.cmake on Linux: adapter-only removal rc 1 'Agent process adapter target contract is incomplete: target_link_libraries\\([ \\t\\r\\n]*better_pull_review_adapter_agent...'; supervisor-only removal rc 1 'Windows agent supervisor target contract is incomplete: target_link_libraries\\([ \\t\\r\\n]*tool_better_pull_review_agent_supervisor...'; the gate at b9b60b93 passed both (rc 0). New contract cases pin both and the supervisor's process/job API ownership, and fail against the pre-change gate and permission mutants (bpr-station-logs/task-20260917-lnx-portable-p3/bpr0842-*). Independent rfc-reviewer: no blocking findings."}},{"criterion":"AC2 architecture_boundaries and architecture_boundary_contract pass","outcome":"pass","revision":"afb15941e50f0b7e76a764749ea345b05c2a140c","evidence":{"kind":"command","detail":"ctest --preset dev -j1 -V -R '^architecture_boundaries$': Passed 8.84 s; ctest --preset dev -j1 -V -R '^architecture_boundary_contract$': Passed 134.54 s, Ran 134 tests, OK (Linux, bpr-linux)."}}]
---

# BPR-0842: The agent target gate cannot tell the adapter's Kernel32 link from the supervisor's

## Defect

`cmake/VerifyArchitectureBoundaries.cmake` checks that the agent process
adapter target links `Kernel32` by looking for that token anywhere in
`src/adapters/agent/CMakeLists.txt`. Since [[BPR-0622]] slice (a)
(`e11f19eb`), the Windows supervisor target has its own `Kernel32` link line.
If the adapter dropped its link and the supervisor kept its own, the gate
would still pass. [[BPR-0837]] had to make the contract case remove every
`Kernel32` token, because removing only the adapter's line no longer fails.

No contract case covers the supervisor's permission to use Windows process
and job APIs, or checks that its target is required.

## Scope

- Scope the gate's target-contract patterns to the target they describe.
- Add contract cases for the adapter link alone and for the supervisor's
  process and job API ownership.

## Acceptance

- Removing only the adapter's `Kernel32` link fails the gate with the target
  contract message, and removing only the supervisor's link fails with a
  supervisor-specific message.
- `architecture_boundaries` and `architecture_boundary_contract` pass.

## Fix (dsp-003, bpr-linux)

Commit `afb15941e50f0b7e76a764749ea345b05c2a140c` on `linux-lane`.
`src/adapters/agent/CMakeLists.txt` is unchanged.

- **The adapter's link.** In `cmake/VerifyArchitectureBoundaries.cmake`, the
  bare `Kernel32` pattern now requires `Kernel32` inside the adapter's own
  `target_link_libraries(better_pull_review_adapter_agent …)` call. `[^)]*`
  keeps the match inside one call. The message is unchanged:
  `Agent process adapter target contract is incomplete: <pattern>`.
- **The Windows supervisor's target.** A new check right after the adapter's
  requires `bpr_add_executable(tool_better_pull_review_agent_supervisor
  src/windows_agent_supervisor.cpp)` and `Kernel32` inside the supervisor's
  own `target_link_libraries` call. It fails with
  `Windows agent supervisor target contract is incomplete: <pattern>`.
- **Contract cases** (`tests/scripts/architecture_boundary_tests.py`):
  - `test_native_windows_agent_target_owns_selected_backend` removes only
    the adapter's link line. It first checks that the line occurs once. This
    replaces [[BPR-0837]]'s workaround, which removed every `Kernel32`.
  - `test_windows_agent_supervisor_target_is_required` removes only the
    supervisor's `bpr_add_executable` line in one subtest, and only its
    `Kernel32` in the other. Each expects the supervisor message.
  - `test_windows_agent_supervisor_owns_process_and_job_apis`: the fixture
    passes with the supervisor's real source. The same source under another
    file name is refused as `native Windows process creation API ownership
    violation`. With `CreateProcessW` renamed, it is refused as `native
    Windows agent containment API ownership violation`.
  - `test_unix_agent_supervisor_target_is_required` is unchanged. It still
    gets the adapter message, because the adapter check runs first.

## Verification (Linux, bpr-linux)

Logs are in `/home/mmarcin/bpr-station-logs/task-20260917-lnx-portable-p3/`
(`bpr0842-*`). None is committed. An rfc-worker made the change and the
integrator committed it unchanged.

- **Before the change.** The gate at `b9b60b93` was run from a copy of the
  real tree (`git archive`). It passes the unchanged agent target file, a
  copy without only the adapter's `Kernel32` and a copy without only the
  supervisor's, all rc 0 (`bpr0842-pre-change-control.log`).
- **After the change,** on the same three copies
  (`bpr0842-post-change-tree.log`):
  - unchanged: rc 0, `Application, provider, and Qt architecture boundaries verified`;
  - adapter's `Kernel32` removed: rc 1,
    `Agent process adapter target contract is incomplete: target_link_libraries\([ \t\r\n]*better_pull_review_adapter_agent…`;
  - supervisor's `Kernel32` removed: rc 1,
    `Windows agent supervisor target contract is incomplete: target_link_libraries\([ \t\r\n]*tool_better_pull_review_agent_supervisor…`.
- **The cases fail without the fix.** Run against the gate at `b9b60b93`,
  the adapter-only case and both supervisor-target subtests fail
  (`bpr0842-tests-vs-pre-change-gate.log`, 3 failures in 4 tests). The
  ownership case fails against a gate with the supervisor removed from the
  process-creation allow list, and again against one with it removed from the
  containment allow list (`bpr0842-supervisor-permission-mutant-*.log`).
- **Rows.** `ctest --preset dev -j1 -V -R '^architecture_boundaries$'`:
  pass, 8.84 s. `ctest --preset dev -j1 -V -R '^architecture_boundary_contract$'`:
  pass, 134.54 s, `Ran 134 tests`, `OK` (132 before, plus the 2 new cases).
- **Independent review** (rfc-reviewer, read-only, on `afb15941`): no
  blocking findings. The reviewer ran the gate on 18 layouts: LF and CRLF,
  `PRIVATE` on its own line, `Kernel32` last before `)`, target-name
  prefixes, `Kernel32` in a neighbouring call, `MyKernel32`, and a generator
  expression before `Kernel32`. Every verdict was correct. The reviewer also
  confirmed that each new case fails against the pre-change gate and against
  targeted gate mutants. Two limits apply to every pattern in this gate and
  are not regressions. A commented-out matching call satisfies a pattern. A
  quoted argument containing `)` before `Kernel32` would refuse a real link.
- **Limits.** These are text checks, run on Linux. Nothing here builds the
  supervisor or shows that it links `Kernel32` on Windows. Neither criterion
  asks for that. As before, the Unix supervisor's pattern reports under the
  adapter message.

**Integrator confirmation (dsp-003).** Main merged `linux-lane` at `e70b9112`
as `0c111e34`. On macOS at `0c111e34`, `architecture_boundaries` and
`architecture_boundary_contract` pass (`build/r2-logs/dsp3-int9-ctest.log`),
and the station's Done is confirmed.
