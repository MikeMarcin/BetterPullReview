# BetterPullReview Issue Board

Issue IDs use the `BPR-NNNN` sequence allocated by `NEXT_ID`.

## Lanes

| Folder | Status | Meaning |
|---|---|---|
| `backlog/` | Backlog | Planned but blocked by dependencies |
| `ready/` | Ready | Unblocked and available to start |
| `in-progress/` | In Progress | The current implementation slice |
| `done/` | Done | Verified terminal state |

Folder placement is authoritative. Front-matter `status` must agree with the
folder. Move a card rather than copying it, and update dependencies when new
work is discovered.

Each card links one RFC, names its dependencies, defines completion evidence,
and is committed with the implementation slice it tracks. RFC closeout is
blocked while any required card remains outside `done/`.

## Current RFC

RFC-BPR-0001 is the only active RFC. Its pack-intake/planning card is complete,
and `BPR-0002` is the next ready slice.
