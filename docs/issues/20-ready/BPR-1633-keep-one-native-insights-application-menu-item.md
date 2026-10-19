---
schema: "implement-rfc.card.v1"
id: "BPR-1633"
title: "Keep one native Insights application-menu item"
status: "Ready"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0086"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"f6ba8f43e4477f60941dd24cdac4450ca68de911","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1633: Keep one native Insights application-menu item

The frozen Mac baseline fails the exact windowed/resident application-menu
parity assertion: after sibling windows and last-window close, two Insights
items remain in Apple's application menu. Both per-workbench and recreated
resident actions use `ApplicationSpecificRole`. Existing native update-menu
code records the same Qt retention/duplicate hazard. RFC0086 and accepted
BPR1606 require Insights in the macOS application menu; moving it to Help is
not an eligible repair.

## Acceptance

- AC1: Establish source-backed cause and preserve the original exact menu failure; load frontend-design and retain approved title, application-menu location, shortcut, active-profile dispatch and disabled no-window behavior.
- AC2: Repair only the measured menu lifetime/ownership defect; meaningful focused offscreen or isolated native evidence proves one item, correct dispatch, sibling/close/reopen lifetime and no-window disabled state without driving or capturing the owner's live desktop. Retain human/native observations that cannot safely run unattended as pending.
- AC3: Pass focused format/tidy/architecture/documentation checks, self-review and commit explicit paths, reconcile commands and report candidate identities and any remaining native acceptance.

## Design boundary

The canonical official frontend-design plugin skill is read before UI edits.
This repair follows the established native platform palette, system menu type,
existing command order and label; it adds no visual tokens, copy or layout.
The design is the single existing Insights command, with ownership matching
its application-menu lifetime and existing per-profile action semantics.
