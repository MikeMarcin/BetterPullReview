---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-F1"
title: "Keep commit parents and decide commit filters in the core"
status: "Done"
rfcs: ["RFC-BPR-0089"]
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"milestone":"RFC-BPR-0089 M1","provisional_id":"issue IDs are allocated on main when the work lands","related_cards":["BPR-PENDING-F2","BPR-PENDING-F3"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"ctest --preset dev -R '^(github_paged_read|github_read_contract|github_contract_fixture_security)$'","detail":"GitHub's commit list keeps each commit's parents (optional, at most 16, every one a full commit ID); the scenarios commit_parents and commit_parents_malformed admit exact parents and refuse a malformed array; the recorded contract admits only an exact parents array of {sha}. macOS."}},{"criterion":"AC2","outcome":"pass","revision":"07920aeb4473341ec22c435eb76896ce08f3e7b5","evidence":{"kind":"command","command":"cmake --preset core-only && cmake --build --preset core-only && ctest --preset core-only -R '^(core_commit_filter|core_diff_model)$'","detail":"The first-parent chain and roles, the span, shown-set operations and their refusals, suspension, truncation at 250, gone commits after a force push and each path's diff (unchanged, exact, split) run without Git or Qt. A filtered key's digest and receipt file are domain-separated and leave every persisted known-answer vector unchanged. Negative control: with the split rule disabled core_commit_filter fails. macOS."}}]
---

# BPR-PENDING-F1: Keep commit parents and decide commit filters in the core

RFC-BPR-0089 M1, the decisions under the Commits tab.

## Before

GitHub's commit list was read without parents, so the application could not
tell a first-parent chain from a merge's side commits, and nothing decided a
commit filter.

## Change

- `github_commit_record::Parents` keeps GitHub's `parents` (optional, at most
  `maximum_github_commit_parents = 16`), read by the paged reader and the
  normalizer and admitted by the recorded contract only in its exact shape.
- `bpr/core/commit_filter.hpp` decides, as pure functions over host-supplied
  touch sets: the first-parent chain and each commit's role (`chain`,
  `chain_merge`, `base_merge`, `side`), why a pull request has no filter
  (truncated at 250, an invalid list, a missing head, duplicates, too many
  parents, unrelated history, unknown parents), the span a comparison covers
  and why a filter is suspended (reversed, empty, not a first-parent range),
  the operations (Show only selected, Hide selected, Show selected, Clear all
  filters) and their refusals, the summary the chip shows, gone hidden
  commits after a force push, and each path's diff: `unchanged` (the
  comparison's own), `exact` (from the parent of the first shown commit that
  changed it to the last), or `split` (a hidden commit changed it between two
  shown ones).
- A filter is the sorted, unique set of hidden commits, so a later push's
  commits are shown.
- `file_comparison_key::Filter` names a filtered diff's two commits. It enters
  the comparison digest only when present, and `filtered_receipt_file` keys
  the receipt of a filtered diff by file and endpoints, so receipts follow
  material.

## Acceptance

- **AC1.** Commit parents are kept from GitHub's commit list and validated in
  the provider, the normalizer and the recorded contract.
- **AC2.** The core decisions run under `core-only` without Git or Qt (RFC
  AC08, M1's part).

## Verification

See the acceptance evidence and the
[M1 checkpoint](../_checkpoints/RFC-BPR-0089-M1/handoff.md).
