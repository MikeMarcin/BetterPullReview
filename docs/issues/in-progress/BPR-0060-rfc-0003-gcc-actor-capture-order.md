---
id: BPR-0060
title: Fix GCC epoch-host actor capture ordering
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0045]
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
