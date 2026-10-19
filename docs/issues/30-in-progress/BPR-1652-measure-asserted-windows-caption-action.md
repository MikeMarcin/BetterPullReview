---
schema: "implement-rfc.card.v1"
id: "BPR-1652"
title: "Measure the asserted Windows caption action"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["win"]
metadata: {"assigned_implementer":"/root/release_repairs_recovered","mandate":"release-v030-coordinated-r1","baseline":"93897e9c2cfce8b0ad674e6d311a3ed84eb70cde","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"},"owned_paths":["src/app/src/github_application.cpp","tests/end_to_end/application_review_round_trip_composition_contract.cmake"]}
---

# BPR-1652: Measure the asserted Windows caption action

The isolated synthetic UIforETW fatal is QList<QAction*>::at. Own-app RVAs resolve through capture_uiforetw_selected_diff_evidence using the exact diagnostic linker map. The Windows row asserts18 actions with caption index17, then its geometry observation incorrectly indexes19.

## Acceptance

- AC1: Use the caption action already asserted at lead+16 in the Windows-only BUILD_TESTING capture. Preserve exact title/review order, full rendered public composition, geometry/pixel/identity/navigation assertions, wire-cut negative control and all timeouts. Restore exact privacy source and rebuild the original app before candidate verification.
- AC2: Run the complete actual UIforETW selected-diff contract after native rebuild. Record raw source-input mismatch and separately prove BUILD_TESTING=OFF and Mac _WIN32 exclusions; root must explicitly reconcile Mac package/frozen candidate bindings.

- AC3: Review the exact pinned app-source range since last matching7697, reseal the full application/manifest digests for only1cdaf43b3, preserve the unchanged production mutation digest and original stale-pin refusal, and run the actual complete native round-trip composition contract.

## Preserved evidence and boundary

The complete780-row original Windows normal gate at93897 is retained:767 pass,12 fail,1 configured Debug Skip, CTest exit8. The evidence ZIP SHA-256 is `5d07e4e5deb82ac9f71c89c4c0424896db09cdac5952e3fdff95b96e53fe463f`. Baseline diagnosis and temporary observation logs are under ignored `.mask/sessions/release-v0.3.0/windows-diagnosis/`; temporary sources were restored exactly, both UIforETW diagnostic apps were rebuilt from original privacy source, and all diagnostic tasks were removed. No full normal rerun or release/native desktop acceptance is inferred.

## Source checkpoint

The bounded repair is implemented without changing product behavior, original
assertions or deadlines. Source self-review and diff checks pass; touched C++
uses clang-format22. Actual focused native verification remains pending.

## Exact composition seal review

The whole src/app diff from last matching76974f7d0 to1cdaf43b3 contains only
commit1cdaf43b3 and its one excluded fixture-index correction. The enclosing
BPR_BUILD_TESTING and _WIN32 guards exclude it from distributions and Mac
compilation respectively; the raw source hash still differs. No provider,
credential, grant, publication or mutation admission changes. The existing
print-only mechanism yields full application
`e6f152e5458fb2e80477828ef7e139bb7cf239bd4335bba97cbb1742e6a7ccc5`,
app manifest `8eabdc7c01467cced18d0a9019c07d5d3f346444157ed77fd94bdac97c67ce4f`
and unchanged mutation branch
`16d135419d39078bc3993b4e1cb80226e190edce50cd24d9456843066043a0e1`.
The stale full seal first refuses the measured drift, and its log is retained.
Only the two changed reviewed-scope pins are renewed. The actual native
round-trip contract remains pending; printing values supplies no verification.
