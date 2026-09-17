---
schema: "implement-rfc.card.v1"
id: "BPR-0842"
title: "The agent target gate cannot tell the adapter's Kernel32 link from the supervisor's"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P3"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 BPR-0837 slice","found_at":"866c5b906309f7b45cf9ed46b04d07662d8fcb75","run":"run-20260916-stations","dispatch":"dsp-003"}
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
