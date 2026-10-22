# Live import: agent-first-run-setup-v1

Imported 2026-10-22 on the lane `worktree-agent-a7c9be9edf974a7e8`, based on
local `main` at `92162efed`, which holds the draft as committed in
`c47ad93e6`, `996f560ba` and `cb22d35be`.

## Mapping

| Draft | Live | Note |
|---|---|---|
| `RFC-DRAFT-0026` (r2) | [RFC-BPR-0091](../RFC-BPR-0091-agent-first-run-setup.md) (r3) | Revised to the owner's answers to Q1 to Q9 and to [[BPR-1738]]'s sparse workspace before adoption; the draft is kept here unchanged as provenance. |
| Proposed card `BPR-PENDING-AN` | `docs/issues/60-done/BPR-PENDING-AN-set-up-an-agent-the-first-time.md` | The phase 1 card, done with the answers recorded. |
| Plan card 1 (core and Codex) | BPR-PENDING-AQ1 | |
| Plan card 5 (application setup host) | BPR-PENDING-AQ2 | Ordered second, as the coordinator directed. |
| Plan card 2 (clone discovery) | BPR-PENDING-AQ3 | |
| Plan card 3 (agent worktrees) | BPR-PENDING-AQ4 | |
| Plan card 4 (brief and skill) | BPR-PENDING-AQ5 | With the Settings editor from plan card 7. |
| Plan card 6 (Qt setup) | BPR-PENDING-AQ6 | With the remaining Settings rows from plan card 7. |
| Plan card 8 (documentation) | BPR-PENDING-AQ7 | |
| Plan card 8 (frozen build and look) | BPR-PENDING-AQ8 | |
| (none) | BPR-PENDING-AQ9 | Closeout card for RFC-BPR-0091. |
| Design frames v1 r2 | `docs/design/BPR-PENDING-AN-agent-setup-v1/` | Kept as the questions' evidence and the adopted design. |

Card IDs are provisional (`BPR-PENDING-AN`, `AQ1` to `AQ9`). They are
allocated on `main` by the coordinator with
`python3 scripts/allocate_issue_id.py`, which renames the files and every
reference together; this lane commits no counter or reservation.

## Sessions

- `29049098deb24f47c839771a46b05c4c` (r1's Q1 to Q6): no answers, superseded
  by r2, released and stopped.
- `349862f0cb991e14b6cb57cf80653c5f` (r2's Q1 to Q9): Q1 A and Q3 A saved by
  the page; Q2 and Q4 to Q9 given in chat because the page's Accept/Reject
  and Next controls failed. Stopped through the script and released.

## Deliberate exclusions

- The draft's Q8 B (the clone's own remote): the head never comes through the
  reviewer's remote or credentials. A sparse review offers the fetch command
  to copy instead.
- The draft's Q9 B and C (writing the skill into worktrees or harness
  folders).
