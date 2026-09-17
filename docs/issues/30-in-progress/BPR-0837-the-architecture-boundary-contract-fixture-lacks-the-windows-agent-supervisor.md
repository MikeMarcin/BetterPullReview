---
schema: "implement-rfc.card.v1"
id: "BPR-0837"
title: "The architecture boundary contract's fixture lacks the Windows agent supervisor source"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "linux", "win"]
metadata: {"found_by":"dsp-003 integration pass at 8a03a5be (red since BPR-0622 slice (a))","found_at":"8a03a5bee813fc285e1c0be91f6a5c070436364f","run":"run-20260916-stations","dispatch":"dsp-003"}
---

# BPR-0837: The architecture boundary contract's fixture lacks the Windows agent supervisor source

## Defect

`architecture_boundary_contract` (`tests/scripts/architecture_boundary_tests.py`)
fails nearly every case on macOS. Each case builds a fixture tree and expects
one named refusal from `cmake/VerifyArchitectureBoundaries.cmake`, but the
gate stops first with

    The selected agent process source is required:
    src/adapters/agent/src/windows_agent_supervisor.cpp

(`VerifyArchitectureBoundaries.cmake:223`, the required list at ~121). The
source was added by [[BPR-0622]] slice (a), `e11f19eb`, and the gate requires
it, but the contract's fixture tree was not given it. The same failure is in
dsp-002's logs (`build/r2-logs/dsp2-0802-focused-final.log`, `dsp2-0644-focused.log`)
and at `8a03a5be` (`dsp3-int1-ctest.log`). `architecture_boundaries` itself
passes: only the contract's own fixtures are stale.

## Scope

- Add the supervisor source to the contract's fixture inventory the way the
  other selected agent process sources are supplied, and check whether any
  other required source added since is missing in the same way.
- Do not relax the gate's required-source rule.

## Acceptance

- `architecture_boundary_contract` passes on macOS, and each case again fails
  on its own named refusal (spot-check two cases' messages).
- `architecture_boundaries` still passes.

## Cause and fix (dsp-003)

Both breaks come from `e11f19eb` ([[BPR-0622]] slice (a)), the only change to
the gate since `a5667197`.

1. **The missing source.** The gate requires
   `src/adapters/agent/src/windows_agent_supervisor.cpp`, and the contract's
   fixture tree never had it, so every case stopped at that refusal. The
   contract now reads the real source into the fixture, the same way it
   supplies `windows_agent_process.cpp`.
2. **A stale mutation hidden behind it.** `e11f19eb` also gave the supervisor
   target its own `Kernel32` link. The gate looks for `Kernel32` anywhere in
   the agent CMake file, so the case that removed only the adapter's indented
   `Kernel32` line still passed the gate (`0 == 0` once the first fix was in).
   The case now removes every entry, and first asserts that the text changed.

The gate itself is unchanged (`tests/scripts/architecture_boundary_tests.py` only).

**Verification (macOS)**, logs in `build/r2-logs/`:

- The full contract passes, 132 tests (`dsp3-0837-contract-final.log`).
  After the first fix only, the Kernel32 case failed (`dsp3-0837-contract.log`).
- Spot checks through the contract's own `run_check` (`dsp3-0837-spotcheck.log`):
  - the unmodified fixture passes;
  - the Kernel32 case fails on `Agent process adapter target contract is incomplete: Kernel32`,
    while the old one-line mutation returns 0;
  - `test_native_windows_agent_process_owner_is_exact` fails on
    `native Windows process creation API ownership violation in src/adapters/agent/src/other.cpp`;
  - `NSTask` added to the supervisor fixture is refused.
- `architecture_boundaries` passes on the real tree (`dsp3-0837-gate.log`).

**Limits, not criteria of this card.**

- The gate's `Kernel32` check cannot tell the adapter's link from the
  supervisor's.
- No contract case yet covers the supervisor's permission to use Windows
  process and job APIs.
