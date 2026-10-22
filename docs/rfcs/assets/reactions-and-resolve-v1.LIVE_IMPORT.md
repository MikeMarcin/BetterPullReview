# Live import: reactions-and-resolve-v1

Imported 2026-10-22 on the lane `worktree-agent-aab81f7a773163494`, based on
local `main` at `3ddfbc5f8ea8b4df418a7b355bcbe4ae6101b5bf`; the
draft was committed on this lane in `ae9dbedb1` and built as written.

## Mapping

| Draft | Live | Note |
|---|---|---|
| `RFC-DRAFT-0027` (r1) | [RFC-BPR-0091](../RFC-BPR-0091-comment-reactions-and-resolve-conversation.md) (r2) | r2 adds the owner's look and closes the RFC; the draft is kept here unchanged as provenance. |
| Plan steps 1 to 6 | BPR-PENDING-AR1 | One card for the whole plan; it also closes RFC-BPR-0091. |
| Draft decisions D1 to D6 | Accepted by the owner's look | Attended-review session `3b59571c0c371e211a07e6a1c88e5711`, closed. |

The card ID is provisional (`BPR-PENDING-AR1`). It is allocated on `main` by
the coordinator with `python3 scripts/allocate_issue_id.py`, which renames the
file and every reference together; this lane commits no counter or
reservation.

## Deliberate exclusions

- Reactions on the pull request description, which is not a conversation card
  here (draft D4).
- Who resolved a thread (`resolvedBy`), which would change every recorded
  review-thread read; the card shows Resolved without a name.
