---
schema: "implement-rfc.card.v1"
id: "BPR-2131"
title: "Keep the wrong-volume ownership fixture consistent across journal phases"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "linux", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2131--BPR-2131--7e001f66280f.json","source_baseline":"af1eaedf78d276ec7a9a7d12dc55b28e8143ed20","mandate_revision":"bpr2131-workspace-fixture-r1","related_cards":["BPR-2118","BPR-2125"],"milestone_plan":{"id":"bpr-2131-workspace-volume-fixture","path":"docs/rfcs/plans/bpr-2131-workspace-volume-fixture/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"f9442ef78bb02f0e556ff381713f89eb3962073e","evidence":{"kind":"artifact","detail":"Only runtime delta is tests/scripts/agent_workspace_ownership_tests.py, 21 insertions/two deletions in check_remount_and_record_refusals. NonMac all-line wrong-volume identity retains exact moved/other-volume and byte-preservation; independent final-line-only alteration retains exact malformed and byte-preservation. Existing macOS wrong-volume construction unchanged. AST/source-scope proof and exact SHA99568d9a in external evidence root."}},{"criterion":"AC2","outcome":"pass","revision":"f9442ef78bb02f0e556ff381713f89eb3962073e","evidence":{"kind":"artifact","detail":"Windows Release+Debug followed by Linux Release+Debug: all four full corrected ownership courses exit0 at actual configured120s bound using unchanged compiled60 helper/source6ef and stock support; both preservation markers, finalOK and17 retained-edit/same-inode migration receipts each. Exact configured environment/boundary/working directory retained (Windows normal uncontained; Linux normal sandbox wrapper). Protected source/helper/build/plumbing seals unchanged, all remaining owned forests empty, PClease returned. Separate corrective proofs preserve original comprehensive failure/unreached tail and give no new-source or finalrelease qualification credit. See docs/verification/BPR-2131-workspace-volume-fixture.md and external windows/linux fullreceipts."}},{"criterion":"AC3","outcome":"pass","revision":"f9442ef78bb02f0e556ff381713f89eb3962073e","evidence":{"kind":"command","detail":"Python compile and AST-only changed-definition/source-scope proof, gitdiffcheck, DOCUMENTATION_OK and explicit own pinned965 plancheck pass. Global targeted discovery actualexit2 on existing foreign release-v041 unknown amendments field, preserved without repair or globalreadiness claim. No product/app pin/CMake/registration/profile/normalR/W/nativegraph/helper/library/distribution mutation, runtime upgrade, nativeMac command or build."}},{"criterion":"AC4","outcome":"pass","revision":"f9442ef78bb02f0e556ff381713f89eb3962073e","evidence":{"kind":"command","detail":"Own allocation/plan/card Main6f60900b and fixture Mainf9442ef78 explicitly land on preserved canonicalMain, then own scoped proof/plan amendment and Done closure through maintained issue_board finish. No broadmain merge/push or frozen60intake/allocation/sign/package/publication. All own executed diagnostic commands terminal and nativeforests empty; PCexecutionlease returned before metadata closure. Fresh own Windows task inert Ready/result0/no trigger; evidence retained; no reviewserver or humancriterion."}}]
---

# Keep the wrong-volume ownership fixture consistent across journal phases

Frozen Build60 Windows Release reached the BPR-2118 wrong-volume course and
reported `record-malformed`, where the fixture expected `record-moved` with
`other-volume`. The non-Mac fixture changes Registry volume only in the final
phase7 line. The immutable identity then differs from phase3, so the existing
phase-transition validation correctly refuses the chain before live-volume
checking. Linux takes the same fixture branch. The macOS wrong-volume generator
already changes identities consistently across its chain.

## Correction

Change Registry volume in every legacy line for the non-Mac wrong-volume case.
Keep its precise moved/other-volume and byte-preservation expectations.
Retain the final-line-only mutation as a separate malformed-chain negative with
its exact refusal and byte-preservation expectation. No parser or product change.

## Acceptance

- AC1: Only the ownership Python fixture changes: the non-Mac wrong-volume chain stays internally consistent and still requires moved/other-volume without journal mutation; the single-final-line identity alteration separately requires malformed without mutation. Existing macOS wrong-volume construction and other course checks remain unchanged.
- AC2: Run the focused corrected ownership course on native Windows and Linux, Release and Debug, serially against the unchanged compiled Build60 helper in its actual configured context, with normal environment/boundary and disposable private roots. Record source/helper/build-input seals before and after; preserve original failure and do not claim next-candidate qualification.
- AC3: Cheap syntax/source/documentation and the explicit owned pinned965 plan checks pass. No app/pin, production C++, registration/profile, normal R/W/native graph or helper change.
- AC4: Land only the own test correction and tracker/proof paths on current main, preserve foreign work/schema drift, and return exact commits plus terminal command/host-lease state. No broad main intake, release allocation, signing or publication.

## Verification

The correction is on main at `f9442ef78bb02f0e556ff381713f89eb3962073e`
(private source `d30c640f1d71507426db225f24d0d5a654027ec0`). All four
corrected full ownership courses passed against unchanged compiled Build60
helpers. The original frozen comprehensive failure remains a recorded failure.
See [the bounded proof](../../verification/BPR-2131-workspace-volume-fixture.md)
for exact source, fixture, native helper and external receipt identities.
The PC execution lease has been returned; no native Mac work was performed.
