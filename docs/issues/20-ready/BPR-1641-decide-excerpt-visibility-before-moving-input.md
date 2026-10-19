---
schema: "implement-rfc.card.v1"
id: "BPR-1641"
title: "Decide excerpt visibility before moving input"
status: "Ready"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux", "mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"8eef1ff495f291a125c15b1a824431a8f8a9fe3c","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1641: Decide excerpt visibility before moving input

The actual Ubuntu application_composition_contract fails its nested Release
build under GCC15.2 at -O3 with unchanged warnings as errors. Inlining the
absent-excerpt refresh into excerpt_view::present diagnoses the stored optional
vector's begin/end as maybe uninitialized after the optional move assignment.
The ordinary Debug build passed; it does not establish optimized compilation.

## Acceptance

- AC1: Compute the existing engaged-and-nonempty visibility predicate from the initialized input before transferring that input into Shown. Preserve optional contents, widget update order, absent/empty hiding, populated visibility and all compiler warning flags; no UI behavior change.
- AC2: Retain the exact original optimized native compile failure; compile the affected translation unit with the original GCC15 Release flags, run the affected public composition/tour evidence and scoped tidy/format/documentation checks, then intentionally commit and reconcile commands. Pending native evidence stays explicit until the coordinator releases the host.

## Diagnosis

The original path assigns Shown from the value parameter and then reads
Shown->Lines behind has_value(). The diagnostic is on vector storage in the
widget, specifically the nullopt refresh path. Computing the same guarded
predicate on the incoming value before moving it removes that post-assignment
read without changing the resulting stored excerpt or visibility. A vector
moved into Shown retains the incoming line sequence. No undefined behavior is
asserted from the original guarded expression; the observed defect is the
optimized GCC build failure.

Evidence: ignored ubuntu-gate-r5.partial5 raw/normalized logs, diagnostics at
lines4952–4981. Both native heavy slots remain coordinator-owned; no build,
tidy, render or native probe is launched while its suites run.
