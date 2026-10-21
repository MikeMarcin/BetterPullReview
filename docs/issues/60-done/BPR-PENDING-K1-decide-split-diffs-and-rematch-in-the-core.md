---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-K1"
title: "Decide force-push rematch offers and a split file's row in the core"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: ["BPR-1685"]
priority: "P1"
platforms: ["mac","win","linux"]
metadata: {"milestone":"RFC-BPR-0089 M3","related_cards":["BPR-PENDING-K2","BPR-PENDING-K3"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"dfea666ec9c7913b462e13f69169b81e769d548b","evidence":{"kind":"command","command":"cmake --preset core-only && cmake --build --preset core-only --target test_commit_filter test_generated_files && ctest --preset core-only -R '^core_(commit_filter|generated_files)$'","detail":"core_commit_filter: a hidden entry records its author and, once the chain's trees are read, what it changes (each path between its two blobs); a gone entry keeps what it recorded. rematch_offers offers a listed chain commit the filter does not hide whose changes equal the gone commit's (same change), and only when none does, one whose headline and author both match (a guess); a merge from the base, a hidden commit, or an entry with nothing recorded gets no offer; a path between other blobs is not the same change. apply_rematch hides the candidate in the gone entry's place and forgets it, and refuses anything not offered now, including an offer already taken (not_offered). combined_split_receipt: a split file's row is as far read as its least read per-commit diff, a diff with no receipt counting unread, and accepted only when every diff is. All under core-only, without Git or Qt. macOS."}}]
---

# BPR-PENDING-K1: Decide force-push rematch offers and a split file's row in the core

RFC-BPR-0089 M3, the decisions under the split diffs and the rematch offers
(RFC AC08, M3's part). The split itself (each path's per-commit segments with
the hidden commits after each) was already decided in M1 by `filter_paths`.

## Change

- `filter_commit` and `filter_commit_graph` keep each commit's author
  (`Authors`, bounded like headlines).
- `filter_hidden_commit` records its `Author` and, once the trees are read,
  `Changes`: every path its first-parent diff changes, with the blob on each
  side (`filter_path_change`). `record_hidden_changes` fills them for listed
  hidden entries; a gone entry keeps what it recorded.
- `rematch_offers` and `apply_rematch` (refusal `not_offered`): "the same
  change" is identical path and blob pairs. The RFC also allows identical diff
  text where it is computed within existing bounds; the chain's tree diffs
  compute no diff text, so only path and blob pairs are compared, and a
  headline and author match is offered only as a labelled guess.
- `combined_split_receipt`: a split file's row receipt from its per-commit
  diffs' own receipts (owner, Q2: each per-commit diff is its own material).

## Acceptance

- **AC1.** The rematch decisions and a split file's row receipt run under
  `core-only` without Git or Qt (RFC AC08, M3's part).
