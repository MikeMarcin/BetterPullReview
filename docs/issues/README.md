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
complete; its architecture, deterministic-core, normalized-contract,
host-profile/capability, portable/native credential, and hardened GitHub CLI
broker slices are verified. `BPR-0023` is in `done/` with its Windows
implementation evidence, while direct Windows x64 execution remains in
`BPR-0036`. `BPR-0025` is in `done/`; its production CLI path remains
fail-closed with zero launches until backlog card `BPR-0037` proves the
separate trust and identity contract. `BPR-0026` is in `done/` with its
macOS/Linux native HTTPS trust, proxy, redirect, cancellation, bounds, and
runtime evidence. `BPR-0027` is in `done/` with the application provider,
serialized host, pagination, rate/retry, credential-lifecycle, and staging
surface verified on macOS by the focused 9/9, fresh `dev` 35/35, fresh
`core-only` 17/17, sanitizer 3/3, and tests-disabled Release gates; its final
Ubuntu 26.04 x86_64 full-system QEMU build passed under GCC 15.2 and its
focused matrix passed 9/9 under Qt 6.10.2. `BPR-0028` is in `done/` with the
provider-free atomic projection, accessible GitHub Qt window, stable
incremental conversation model, three-scale rendering, fresh macOS 38/38 and
core-only 17/17 matrices, sanitizer 4/4, and Ubuntu x86_64 host/scale 4/4
evidence. `BPR-0038` is in `done/` with the fail-closed decision to keep
query-bearing HTTP redirect targets rejected. `BPR-0029` is in `done/` with
explicit GitHub.com/GHES profiles and provenance, multi-profile provider
routing, native HTTPS and platform-vault selection, Qt-owned sensitive input,
hostname-bound v2 authority, orderly exact credential deletion, visible
zero-launch CLI unavailability, public GitHub.com/GHES application fixtures,
and a tests-disabled Release seam-absence contract. Its current-tree Linux v2
matrix also passed in an Ubuntu 26.04 x86_64 full-system QEMU guest, including
the native vault, both public fixtures, Release seam absence, exact cleanup,
and dependency/runtime/ELF audits. `BPR-0030` is in `done/` with exact
recorded-method and bounded GraphQL operation policy, canonical-body
backstop, exhaustive provider-failure mapping, a checked-in deterministic Qt
property corpus, and the current 32-scenario native HTTPS verifier. Its final
macOS matrices passed 44/44 and 17/17 plus sanitizer refreshes; its Ubuntu
26.04 x86_64 GCC 15.2 build completed 83/83 targets and the complete
non-native matrix passed 41/41. `BPR-0031` is in `in-progress/` as the current
slice and owns presentation polish, native system appearance, live appearance
changes, rendered UX, and accessibility verification.

The open Windows task and CLI production-trust task do not block later
implementation, tests, documentation, security review, or simplification on
available targets. BPR-0036 blocks only its affected Windows claims and final
RFC-BPR-0002 Auto-Verified closeout; BPR-0037 blocks only CLI production
enablement and the final RFC closeout.

The public application has no durable credential selector or enumeration
path. Orderly shutdown exact-deletes the retained credential before worker
teardown; an abrupt crash can leave an item that a later process cannot
recover. No restart-recovery claim is made.

GitHub Actions is not a BetterPullReview build, test, package, release, or RFC
verification surface. Platform-specific claims require local or target-native
evidence.
