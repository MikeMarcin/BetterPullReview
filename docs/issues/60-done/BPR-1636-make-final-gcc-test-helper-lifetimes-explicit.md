---
schema: "implement-rfc.card.v1"
id: "BPR-1636"
title: "Make final GCC test helper lifetimes explicit"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"3ca343cbeb252b476b620a340cd821802ab08539","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","evidence":{"kind":"observation","detail":"Exactly3mac-only helper definitions scopedApple matchingexistingcaller; exact membershipvector/count equality retained with namedconst vector. No productpolicy/assertion/warningoverride;format22/diffpass."}},{"criterion":"AC2","outcome":"pass","revision":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","evidence":{"kind":"observation","detail":"Coordinator native Ubuntu r5 complete normal build passes under GCC15.2 unchangedWerror. Actual Mac Optiontyping1.14s and Githubprovider3.36s pass;per-TU scopedclang-tidy22 Werrorpasses actualcompileDB, isolatedObjectivefixturewarning separatelyfixed. Originalfailuresretained;selfreview/explicitcommits/docs complete;whole789suite remainsparent-owned."}}]
---

# BPR-1636: Make final GCC test helper lifetimes explicit

The coordinator's native Ubuntu keep-going build supplies exactly two failed
translation units: three macOS Option-key helpers are unused off Apple, and
GCC15 diagnoses a temporary expected membership vector in the provider test's
compound equality assertion. All other available build actions succeed.

## Acceptance

- AC1: Scope only the three actual macOS helpers to their existing Apple-only caller and name the provider expectation vector, preserving complete Option coverage and exact membership/count equality without warning overrides.
- AC2: Retain original raw failures, compile/run the affected native tests and scoped tidy, self-review/document/commit explicit paths, and hand full native gates to the coordinator.

## Verification

Exactly3mac-only helper definitions scopedApple matchingexistingcaller; exact membershipvector/count equality retained with namedconst vector. No productpolicy/assertion/warningoverride;format22/diffpass.

Coordinator native Ubuntu r5 complete normal build passes under GCC15.2 unchangedWerror. Actual Mac Optiontyping1.14s and Githubprovider3.36s pass;per-TU scopedclang-tidy22 Werrorpasses actualcompileDB, isolatedObjectivefixturewarning separatelyfixed. Originalfailuresretained;selfreview/explicitcommits/docs complete;whole789suite remainsparent-owned.
