---
schema: "implement-rfc.card.v1"
id: "BPR-2131"
title: "Keep the wrong-volume ownership fixture consistent across journal phases"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["win", "linux", "mac"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2131--BPR-2131--7e001f66280f.json","source_baseline":"af1eaedf78d276ec7a9a7d12dc55b28e8143ed20","mandate_revision":"bpr2131-workspace-fixture-r1","related_cards":["BPR-2118","BPR-2125"],"milestone_plan":{"id":"bpr-2131-workspace-volume-fixture","path":"docs/rfcs/plans/bpr-2131-workspace-volume-fixture/milestone-plan.json"}}
acceptance: []
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
- AC2: Run the focused corrected ownership course on native Windows and Linux serially against the unchanged compiled Build60 helper in its actual configured context, with normal environment/boundary and disposable private roots. Record source/helper/build-input seals before and after; preserve original failure and do not claim next-candidate qualification.
- AC3: Cheap syntax/source/documentation and the explicit owned pinned965 plan checks pass. No app/pin, production C++, registration/profile, normal R/W/native graph or helper change.
- AC4: Land only the own test correction and tracker/proof paths on current main, preserve foreign work/schema drift, and return exact commits plus terminal command/host-lease state. No broad main intake, release allocation, signing or publication.
