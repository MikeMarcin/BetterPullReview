---
schema: "implement-rfc.card.v1"
id: "BPR-1687"
title: "Acquire a sparse review's first-parent chain trees on demand"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685"]
priority: "P1"
platforms: ["mac"]
metadata: {"milestone":"RFC-BPR-0089 M1","reservation":"docs/issues/_reservations/BPR-1685--BPR-1688--a54310676f44.json","related_cards":["BPR-1686"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"ctest --preset dev -R '^(gh_cli_transport|gh_cli_git_smart_transport|application_epoch_host)$'","detail":"compile_gh_cli_git_clone_request emits --depth=N only in sparse mode and refuses depth 0, above 251, and other than 1 outside sparse mode. acquire_chain_trees clones at the chain's depth with --filter=blob:none into a transient quarantined store, checks every requested commit and its tree, and answers each commit's first-parent changes (diff-tree --find-renames=100%, bounded) and a blob at a literal path. A missing commit is refused as a response limit. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"observation","detail":"Live, MikeMarcin/BPR-Test #111, signed in through gh: the Commits tab acquired the three-commit chain through gh and read 321, 4 and 8 touched files; every filtered diff read its blobs through the review's normal one-file reads. The course took 67 s; the run spent 95 REST calls including setup, Viewed reads and checks (X-RateLimit-Remaining 4809 before, 4714 after). macOS."}}]
---

# BPR-1687: Acquire a sparse review's first-parent chain trees on demand

RFC-BPR-0089 M1. A review through the GitHub CLI on macOS is sparse: it holds
only the epoch commits. A filter needs the first-parent chain's trees.

## Change

- `git_smart_transport_port::acquire_chain_trees` takes the head, the chain's
  depth (chain length plus one, at most 251) and the commits it needs. The gh
  transport clones at that depth with `--filter=blob:none` into a transient
  store under the existing quarantine and process supervision, then checks
  every requested commit and its tree. Nothing is added to the durable
  catalogue.
- `gh_cli_chain_tree_store` answers each commit's first-parent changes through
  `diff-tree --find-renames=100%` within the existing 10,000-entry bound, and
  a blob at a path through `ls-tree --literal-pathspecs`.
- The epoch host runs the touch job on the review's own objects when it has
  them, and falls back to the chain store for a sparse review. Its filtered
  blob reads use the same store and are authorized only for the endpoints the
  touch job resolved.

## Acceptance

- **AC1.** The transport compiles, bounds and checks the chain acquisition.
- **AC2.** A sparse review on BPR-Test reads its chain's trees through gh and
  filters on them.

## Open engineering question

The RFC's question on the cost of long chains in large repositories stays
open: this store is transient and re-acquired per window, and only a
three-commit chain was measured.
