---
schema: "implement-rfc.card.v1"
id: "BPR-1510"
title: "Remove redundant Cmd+J button covered by Threads tab"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Cmd+j button can go away, threads tab already covers the behavior","milestone_plan":{"id":"bpr1510-remove-cmd-j","path":"docs/rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json"}}
---

# BPR-1510: Remove redundant Cmd+J button covered by Threads tab

Remove the visible Cmd+J button because the Threads tab already provides its behavior. Preserve the existing keyboard shortcut and Threads tab interaction; the owner requested removal of the button.

## Acceptance

- AC1: The redundant Cmd+J button is absent from the workbench and its former space does not leave an awkward layout gap.
- AC2: The Threads tab still provides the behavior previously reachable through the button.
- AC3: The existing Cmd+J keyboard shortcut continues to work, with no focus or accessibility regression from the removed widget.
- AC4: Verify actual offscreen rendering and exercise the Threads tab and keyboard action through the composed public path.

## Coordination

Load frontend-design:frontend-design before UX edits, as required by AGENTS.md. Coordinate shared tab/layout files with BPR-1509 (Files/Search tab styling), while preserving that card’s scope. Never drive or capture the owner’s live desktop. Any required human visual judgement uses a short attended session and frozen candidate.

## Milestone plan and checkpoint (2026-10-14)

- Assignment: BPR-1510 only, owner testing note 2026-10-14 r1; branch `bpr1510-remove-cmd-j`, original baseline `e0db7fb17`. Coordinator-authorized tracker imports through `9c80010a0` were copied unchanged; claim `4c99101a5`. No independent allocation.
- [Adopted plan](../../rfcs/plans/bpr1510-remove-cmd-j/milestone-plan.json): remove the redundant Comments-header button, retain Threads and Cmd+J, verify render/interaction/focus, then present AC1's visual judgment as one short attended candidate.
- Skills: implement-rfc, manage-agent-skills (exact registration plan `sha256:0452f63d3420c2a0c3c3f3ca8f5c4ffbf1e28094cfcd94a3aae88a980290ecf0` applied; preflight `ready_pinned`, host loading unverified), frontend-design:frontend-design.
- Design: preserve the native palette, typography, tab row and one-pixel tool spacing. Remove the finder widget outright so remaining comment tools occupy its former space.
- Scope: `workbench_window.cpp/.hpp`, existing window and composed application courses, and the owning UX specification. BPR-1509 confirmed its Files/Search styling hunks are independent.
- Documentation impact: UX specification updated for the removed entry. No policy, host, provider, persistence, publication, build or release behavior changes.
- Current state: removal and focused assertions implemented; dev build pending. No known blocker in product behavior. Automated AC2–AC4 and visual AC1 remain unaccepted until their evidence arrives.
- Runtime evidence and milestone binding: `/var/folders/k7/43dlvy3n3v969dd3_xqz8w4m0000gn/T/bpr1510-ashc70r3`. Task-owned build command uses the isolated `build/dev`, four compile jobs, shared read-only dependency inputs. No other product writer or protected candidate yet.
- Next: complete focused public-path checks, inspect offscreen images, freeze the exact passing source using the repository helper, prepare a fresh attended session, and release engineering ownership to the coordinator.
