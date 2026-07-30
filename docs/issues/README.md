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

RFC-BPR-0001 is `Auto-Verified`, and every linked card is in `done/`.
RFC-BPR-0002 is the active RFC in canonical order. Its planning slice is
complete; its architecture, deterministic-core, and normalized-contract
slices plus host-profile/capability probing and the portable credential-vault
contract plus the native macOS Keychain and Linux Secret Service adapters are
verified. `BPR-0023` is in `done/` with its Windows implementation evidence,
while direct Windows x64 execution remains in `BPR-0036`. `BPR-0025` is ready,
and completion of `BPR-0024` newly unblocks `BPR-0026`, which is also ready.
The open Windows task does not block implementation, tests, documentation,
security review, or simplification on available targets; it blocks only the
affected Windows claims and RFC-BPR-0002 closeout.

GitHub Actions is not a BetterPullReview build, test, package, release, or RFC
verification surface. Platform-specific claims require local or target-native
evidence.
