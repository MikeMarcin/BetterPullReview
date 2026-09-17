---
schema: "implement-rfc.card.v1"
id: "BPR-0837"
title: "The architecture boundary contract's fixture lacks the Windows agent supervisor source"
status: "Ready"
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
