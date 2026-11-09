---
schema: "implement-rfc.card.v1"
id: "BPR-1967"
title: "Make the native replay workspace fault portable"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "mac", "linux"]
metadata: {"kind":"test","found_by":"BPR-1964 current-image native Windows wrapper baseline","mandate":"Coordinator approved one test translation unit on 2026-11-09: withhold the exact owned required workspace marker, restore it on every path, and preserve original semantic oracles and limits.","milestone_plan":{"id":"bpr-1967-native-replay-marker","path":"docs/rfcs/plans/bpr-1967-native-replay-marker/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"3dc816125b7c7b82b1c2113f9067f57ee795d0c9","evidence":{"kind":"artifact","detail":"Single test TU temporarily withholds exact required .git/bpr-owned-workspace marker using named error_code failures and restores every controlled path; original private_storage_failure/no-process-start/no-fallback/native continuation/replay oracles preserved. Original-image filesystem_error evidence retained, cached backend not killed."}},{"criterion":"AC2","outcome":"pass","revision":"8396d01c3fedc31c5fbcaa9ef7d33eb7a8791406","evidence":{"kind":"artifact","detail":"Current accepted8f+1967 image575b normal six-course wrapper PASS45.94s at original400/180limits. Irrelevant-canary-marker negative image7362 rejected by exact missing-native-no-retry Finished: success oracle, CTest8/84.49s; exact-line reconciliation excludes standalone wrapperPASS. Corrected b606 source restored and normalbuild0; no restored-image PASS reuse."}},{"criterion":"AC3","outcome":"pass","revision":"107d346834f7ad2c316580c634fccbce314d6ad0","evidence":{"kind":"artifact","detail":"Source and shared evidence integrated and verified on main/origin107d preserving7321/NEXT1969 allocations. Format22,74 isolation contracts,67 documentation contracts,DOCUMENTATION_OK and scoped board validation passed. Native courses/guards quiescent; exact images retained including About dirty-source metadata library delta. No full common-source qualification credit."}}]
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

## Native result and main integration

Source `3dc816125` changes only this test TU. The corrected file SHA is
`b6061e0bf6d3074922b69e3d3948f477c75b2705527c99c7c4fd0840b74d8a27`.
On current accepted 8f code, the normal six-course wrapper passes in 45.94
seconds with all original assertions and limits. The negative changes only
the withheld file to the canary's unrelated state marker. The ownership
marker then remains valid, native continuation succeeds, and the unchanged
`missing-native-no-retry` oracle rejects it (CTest 8 / 84.49 seconds).

The operator postcheck mistakenly matched CTest's printed pass-regex header;
the raw assertion and exact-line reconciliation are retained. The corrected
source is restored and normally rebuilt. About's normal dirty-source build
identity causes one composition-library hash change during the variants;
there is no all-libraries-unchanged claim or reused pass for the restored
image. The passing and negative image/PDB evidence remains separately bound.

Source and evidence are pushed through main `107d34683`, preserving every
allocation. Format 22, 74 isolation contracts, 67 documentation contracts,
documentation structure and scoped board validation pass. The
[shared handback](../_checkpoints/bpr1964-1967-native-windows/handoff.md)
retains the original abort, later c4 failure, exact corrected/native controls,
ownership and limits. BPR-1964 containment registration is separate; no
full-release qualification is supplied by this fixture correction.
