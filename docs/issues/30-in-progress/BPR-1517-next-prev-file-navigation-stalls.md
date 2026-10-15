---
schema: "implement-rfc.card.v1"
id: "BPR-1517"
title: "Investigate Next/Previous File stalling while file tree navigation works"
status: "In Progress"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"I haven't figured out how yet but I've gotten into a state where next/prev file do nothing although I can still navigate through files using the file tree","reproduction":"Trigger unknown; owner observed stalled Next/Previous File with working file tree","dispatch":"Fresh dedicated implementer queued pending an available agent slot","milestone_plan":{"id":"bpr1517-file-navigation","path":"docs/rfcs/plans/bpr1517-file-navigation/milestone-plan.json"}}
---

# BPR-1517: Investigate Next/Previous File stalling while file tree navigation works

The owner observed a state where Next File and Previous File do nothing, while choosing files in the file tree still navigates successfully. The triggering sequence is not yet known. Investigate and fix the discrepancy without assuming a reproduction or treating tree navigation as a workaround that completes the card.

## Acceptance

- AC1: Diagnose the relevant action admission, selection identity and navigation sequence; establish a representative reproduction of the reported stalled state or record precise evidence and remaining uncertainty if not reproducible.
- AC2: Next/Previous File navigate when an eligible adjacent file exists, including after tree selection, epoch/comparison changes, refresh, filtering and in-flight file loads as applicable to the diagnosis.
- AC3: Action enabled state and actual admission agree; stale completions and unavailable material do not strand navigation. Legitimate endpoints remain correctly disabled without wrapping unless already specified.
- AC4: Verify a regression sequence through actual composed actions and file-tree navigation, asserting exact selected file identity and visible material, not only QAction activation.

## Coordination

Coordinate shared navigation/selection paths with BPR-1506 (automatic Seen) and BPR-1507/BPR-1515 (Accept control/hotkey). Preserve core navigation authority. Load frontend-design:frontend-design before any UX changes per AGENTS.md. Never operate or capture the owner’s live desktop. Use controlled fixtures and deterministic host delay where useful; no live remote mutation required.
