# Live import: pr-in-pane-suggestions-v1

Imported 2026-10-21 on the lane `worktree-agent-ab2406ef642d68509`, based on
local `main` at `07a9594c3a64dc9176a97335b04a6cead7b637cc`, which holds the
draft as committed in `653e2ff2d` and `8cdbace3a`.

## Mapping

| Draft | Live | Note |
|---|---|---|
| `RFC-DRAFT-0025` (r1) | [RFC-BPR-0090](../RFC-BPR-0090-in-pane-suggest-change-for-pull-requests.md) (r2) | Revised to the owner's answers to Q1 to Q5 before adoption; the draft is kept here unchanged as provenance. |
| Proposed M1 Core | BPR-PENDING-AH1 | Range decision, editable span, plan, body split, rebuild, convert, carry. |
| Proposed M2 Qt pane | BPR-PENDING-AH2 | |
| Proposed M3 Drawer and lifecycle | BPR-PENDING-AH3 | |
| (from M3) | BPR-PENDING-AH8 | Carry and Copy change at a newer head, split from AH3 during M3 because they need host facts. |
| Proposed M4 Live and attended | BPR-PENDING-AH4 | |
| (none) | BPR-PENDING-AH5 | Closeout card for RFC-BPR-0090. |
| (Q4 note) | BPR-PENDING-AH6 | Backlog follow-up: a stacked two-pane layout. |
| (Q3 note) | BPR-PENDING-AH7 | Backlog follow-up: a durable file-comment subject for a converted suggestion. |
| Design frames v1 | `docs/design/BPR-1723-in-pane-suggest-v1/` | Kept as the questions' evidence. |
| (none) | `docs/design/BPR-1723-in-pane-suggest-v2/` | Frames for the adopted selected-range design. |

Card IDs are provisional (`BPR-PENDING-AH1` to `AH8`). They are allocated on
`main` by the coordinator with `python3 scripts/allocate_issue_id.py`, which
renames the files and every reference together; this lane commits no
counter or reservation.

## Deliberate exclusions

- The draft's multi-run plan, run merges, "Part 2 of 3" and the insertion
  neighbour rule, which the Q1 rejection removed.
- The draft's Q3 option A as a standing rule: a marked suggestion is held back
  only until it is converted or deleted.
