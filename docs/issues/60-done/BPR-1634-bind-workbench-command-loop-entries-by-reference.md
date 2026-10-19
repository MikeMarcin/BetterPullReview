---
schema: "implement-rfc.card.v1"
id: "BPR-1634"
title: "Bind workbench command loop entries by reference"
status: "Done"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"613f33524f92e844e8996126f86c5f64e54b1103","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"4b73e70461c316483ea92d9e4710f1149db13a96","evidence":{"kind":"observation","detail":"Exactly two const auto& bindings; callback [id] capture retained;clang-format22/diffpass;no UI/warningchanges."}},{"criterion":"AC2","outcome":"pass","revision":"123410f4a","evidence":{"kind":"observation","detail":"Coordinator actual Ubuntu r3 and full r5 normal build compiles repairedTU under GCC15.2 unchangedWerror. Actual Macqt_change_end_keys passes0.52s; focusedworkbenchclang-tidy22 Werrorpasses exactDB. Raworiginalfailureretained;docs/selfreview/explicitcommitscomplete;parentownsfullnormalgates."}}]
---

# BPR-1634: Bind workbench command loop entries by reference

Ubuntu normal gate-r2 fails before tests because GCC15.2 treats copying the
structured binding pairs in two workbench command loops as
`-Werror=range-loop-construct`. The coordinator supplies the actual raw native
failure and delegates the two reference changes, retaining all warning flags.

## Acceptance

- AC1: Bind only the two entry/id loop pairs by const reference, preserving copied callback capture, commands and product UX; pass format and diff checks with no warning override.
- AC2: Retain original failure, supply focused native compile/tidy evidence, self-review/document/commit explicit paths and return command ownership to the coordinator. Full normal native gate remains coordinator-owned.

## Verification

The coordinator's Ubuntu r3 and complete r5 native builds compile the repaired
workbench under unchanged GCC15.2 warnings as errors. The exact loop changes
are committed at `4b73e70461c316483ea92d9e4710f1149db13a96`; callback `id`
capture stays by value. Mac focused `qt_change_end_keys` passes (0.52 seconds),
exercising construction, both commands and their state descriptions. Focused
clang-tidy22 with warnings as errors passes on the actual workbench compilation
database; clang-format22 and diff checks pass. The parent retains full suites.
