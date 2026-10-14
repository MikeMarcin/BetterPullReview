---
schema: "implement-rfc.card.v1"
id: "BPR-1518"
title: "Navigate to the next change after the clicked cursor position"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"If we click a line below a change hunk next change goes to the first change in the file. Should be the next change after the cursor probably","dispatch":"Fresh dedicated implementer queued pending an available agent slot"}
---

# BPR-1518: Navigate to the next change after the clicked cursor position

Clicking a source line below a change hunk and invoking Next Change currently returns to the first change in the file. Use the current clicked cursor position as the navigation origin so Next Change selects the next change after it. Treat the owner’s suggestion as the intended direction; inspect existing endpoint/wrap semantics and make them explicit rather than silently inventing a new policy.

## Acceptance

- AC1: Reproduce clicking a line between hunks and invoking Next Change; select the next hunk after the cursor rather than the first hunk in the file.
- AC2: Navigation uses the correct current file, side and semantic source position, including unchanged lines outside a hunk and unequal left/right mappings.
- AC3: Before-first, inside-hunk and after-last positions have coherent documented behavior consistent with the existing navigation contract; inspect Previous Change for the same stale-origin problem and preserve directional symmetry where appropriate.
- AC4: Verify actual source-line click then composed Next Change action, asserting selected hunk/source identity and visible position; cover cursor replacement after prior hunk navigation and stale comparison changes.

## Coordination

BPR-1517 tracks stalled Next/Previous File actions as a separate issue; preserve distinct scope while sharing navigation findings. Coordinate any diff cursor/splitter overlap with BPR-1516. Load frontend-design:frontend-design before UX edits per AGENTS.md. Never operate or capture the owner’s live desktop. No remote mutation needed.
