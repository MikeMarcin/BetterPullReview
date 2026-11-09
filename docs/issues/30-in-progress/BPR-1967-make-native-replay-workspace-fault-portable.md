---
schema: "implement-rfc.card.v1"
id: "BPR-1967"
title: "Make the native replay workspace fault portable"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"kind":"test","found_by":"BPR-1964 current-image native Windows wrapper baseline","mandate":"Coordinator approved one test translation unit on 2026-11-09: withhold the exact owned required workspace marker, restore it on every path, and preserve original semantic oracles and limits.","milestone_plan":{"id":"bpr-1967-native-replay-marker","path":"docs/rfcs/plans/bpr-1967-native-replay-marker/milestone-plan.json"}}
---

# BPR-1967: Make native replay's workspace fault portable

## Outcome and acceptance

- AC1. The transcript replay course temporarily withholds the exact owned
  `.git/bpr-owned-workspace` marker and restores it on every exit path. Named
  errors replace an unhandled file operation. Production guards and the live
  native backend remain intact; the fault means missing required ownership
  evidence, without claiming the entire directory disappeared.
- AC2. The normal six-course wrapper completes on the corrected native
  image with its original 400/180-second limits and unchanged assertions.
  Removing only an irrelevant canary state marker in a separate negative
  variant must fail the original no-retry/private-storage oracle.
- AC3. Exact source/image/context evidence, focused format/documentation
  checks and normal main integration are complete, with quiescent commands.

## Cause and scope

The fresh current-main Windows Release image passes the first four sessions
then aborts with native code 3 during `transcript-replay`. An original-image
Win32 debug observer sees an unhandled `std::filesystem_error`: renaming the
workspace is refused because another process uses it. The native fixture
intentionally waits for further input between turns, so turn quiescence is
not process completion. Under the debugger the exception remains unhandled
and exits with its exception status; that engineering observation does not
replace the original native abort or establish a passing course.

Only `tests/end_to_end/agent_invocation_composition_public_path.cpp` changes.
The production workspace inspection already requires the exact marker and
refuses before launch when it is missing. Keep the same failed question,
`private_storage_failure`, unchanged process-start count, no implicit replay,
retained native state and explicit successful transcript replay afterward.
No cached backend is killed to permit the fault. Frozen producers remain
untouched. BPR-1964's containment measurement and all other release failures
remain separate obligations.
