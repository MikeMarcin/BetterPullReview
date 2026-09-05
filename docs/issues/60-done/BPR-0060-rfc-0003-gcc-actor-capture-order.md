---
schema: "implement-rfc.card.v1"
id: "BPR-0060"
title: "Fix GCC epoch-host actor capture ordering"
status: "Done"
rfcs: ["RFC-BPR-0003"]
depends_on: ["BPR-0045"]
completed: "2026-08-01"
---

# Fix GCC epoch-host actor capture ordering

Repair the cross-compiler ownership defect exposed by BPR-0050's fresh Ubuntu
26.04 x86_64 GCC 15.2 run. `accept_now` currently captures one repository
actor into a worker lambda while moving the same local actor into another
argument of the enclosing `submit_effect` call. The argument evaluation order
allows GCC to move first, so the worker later dereferences a null captured
actor during source-collision handling; Clang's observed ordering only hid the
defect.

Make the scheduling and worker ownership copies explicit without weakening
repository FIFO, PR-scoped cache-session reuse, cancellation, or quiescence.
Directly rerun the previously deterministic GDB reproduction and complete
`application_epoch_host` under GCC x86_64, Clang arm64, and Address/Undefined
Behavior Sanitizers. Inspect adjacent effect submissions for the same
capture-and-move pattern, run architecture gates and `git diff --check`, and
return BPR-0050 to `in-progress/` only after the remediation is committed.

## Result

`accept_now` now copies the retained actor into distinct worker and scheduler
owners before calling `submit_effect`. The worker capture then moves only its
own copy, so compiler-selected argument evaluation order cannot empty it.
Repository FIFO, exact cache-session reuse, cancellation, and quiescence are
unchanged.

The GCC rerun also exposed a false-positive stale-terminal test: after running
B's own preparation to a legitimate unavailable terminal, the assertion
dereferenced an empty optional. libc++ happened to leave readable storage,
while checked libstdc++ stopped at the invalid access. The load, preparation,
and comparison regressions now deliberately deliver B's offer before A's
queued terminal and assert the live optional before reading it, directly
exercising the intended stale-result rejection.

## Evidence

- Ubuntu 26.04 x86_64, Linux 7.0.0-28, GCC 15.2, and Debug libstdc++:
  `application_epoch_host` passed 1/1 after previously reproducing the null
  actor and optional assertion failures deterministically under GDB.
- macOS arm64 Clang and the ASan/UBSan build each passed
  `application_epoch_host` 1/1.
- The macOS and Ubuntu GCC architecture/dependency groups each passed 5/5:
  `core_dependency_boundary`, `core_boundary_contract`,
  `architecture_boundaries`, `architecture_boundary_contract`, and
  `git_protocol_boundary`. The QEMU target-native group completed in 464.71
  seconds.
- Every adjacent actor-backed `submit_effect` site was inspected. Other sites
  either capture a separately owned context or copy the actor before moving a
  different local; none captures and moves the same local in separate call
  arguments.
- `python3 scripts/verify_documentation.py --root .` reports
  `DOCUMENTATION_OK`; tracked and on-disk GitHub Actions workflow counts are
  both zero; `git diff --check` passes.
