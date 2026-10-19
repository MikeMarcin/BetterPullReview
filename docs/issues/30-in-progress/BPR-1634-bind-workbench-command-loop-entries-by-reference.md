---
schema: "implement-rfc.card.v1"
id: "BPR-1634"
title: "Bind workbench command loop entries by reference"
status: "In Progress"
rfcs: ["RFC-BPR-0038"]
depends_on: []
priority: "P1"
platforms: ["linux"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"613f33524f92e844e8996126f86c5f64e54b1103","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1634: Bind workbench command loop entries by reference

Ubuntu normal gate-r2 fails before tests because GCC15.2 treats copying the
structured binding pairs in two workbench command loops as
`-Werror=range-loop-construct`. The coordinator supplies the actual raw native
failure and delegates the two reference changes, retaining all warning flags.

## Acceptance

- AC1: Bind only the two entry/id loop pairs by const reference, preserving copied callback capture, commands and product UX; pass format and diff checks with no warning override.
- AC2: Retain original failure, supply focused native compile/tidy evidence, self-review/document/commit explicit paths and return command ownership to the coordinator. Full normal native gate remains coordinator-owned.
