---
schema: "implement-rfc.card.v1"
id: "BPR-1572"
title: "Stacked pull requests: Whole PR compares from the recorded base's merge base when the base branch takes the head's commits"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1560"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1560 follow-up (stacked pull requests), owner approved 2026-10-17","governing_rfc":"RFC-BPR-0029","related_cards":["BPR-1560"],"reservation":"docs/issues/_reservations/BPR-1572--BPR-1572--7334e3717c3a.json","acceptance":["BPR-1572:measurement","BPR-1572:provider","BPR-1572:publication","BPR-1572:wording","BPR-1572:fixture","BPR-1572:live","BPR-1572:negative-control","BPR-1572:gates"]}
---

# BPR-1572: Stacked pull requests compare from the recorded base's merge base

## Owner notes

BPR-1560 follow-up, owner approved 2026-10-17 (coordinator): a stacked pull
request B (base = feature branch A) whose base branch takes commits B already
has, while GitHub leaves B's `base.sha` unchanged. Then compare(A...head) and
compare(base.sha...head) name different merge bases, the provider answered
`refused` and Whole PR stayed unavailable until the next push moved
`base.sha`. Measure what GitHub's Files changed follows there; accept that
answer instead of refusing if it is reliable, keeping `refused` for genuine
movement and every BPR-1560 safety property. Later the same day the owner asked
for a lean run: essential variants, the minimal change, focused tests, one full
suite, then Astra (Codex) review only; restored the same day to the original
scope, with Fable and Astra review before merging.

## Measurement (github.com, MikeMarcin/BPR-Test, 2026-10-17)

Scratch main `M0`; A = `M0 + a1` (w.txt); B = `a1 + b1 (x.txt) + b2 (y.txt) +
b3 (z.txt)`, a pull request into A. After each step, once GitHub settled
(polled 45 s or more): `pulls/B` (`base.sha`, state, commits), `pulls/B/files`,
the public `pull/B.diff` (GitHub's web diff), and compare against the branch
and against `base.sha` (merge base and file list). Scripts and logs were kept
in the session scratchpad, not committed; the scratchpad was emptied when the
session restarted, so the numbers here are the record.

| Course, step | A tip | `base.sha` | merge base (A...head) | merge base (base.sha...head) | Files changed (REST, `.diff`) | follows |
|---|---|---|---|---|---|---|
| #92 open | a1 | a1 | a1 | a1 | x, y, z | both |
| #92 A advances on its own (a2) | a2 | a1 | a1 | a1 | x, y, z | both |
| #92 A merges b1 | merge | a1 | b1 | a1 | x, y, z | `base.sha` |
| #92 ordinary push b4 to B | merge | a1 | b1 | a1 | x, y, z | `base.sha` |
| #92 B merges A | merge | moved to A's tip | same | same | y, z | both |
| #92 A merges b2 | merge2 | unchanged | same | same | y, z | both |
| #92 A merges all of B | merge-all | unchanged | head (`behind`) | A's earlier tip | y, z; GitHub marked B merged (closed) in about 6 s | `base.sha` |
| #90 A fast-forwards to b1 | b1 | a1 | b1 | a1 | x, y, z | `base.sha` |
| #90 A fast-forwards to b2 | b2 | a1 | b2 | a1 | x, y, z | `base.sha` |
| #90 ordinary push b4 to B | b2 | a1 | b2 | a1 | x, y, z | `base.sha` |
| #91 A rebased onto a newer main (force push) | a1' | a1' (reset) | M0 | M0 | w, x, y, z | both |
| #91 B rebased onto A (force push) | b1' | b1' | b1' | b1' | y, z | both |
| #93 A force-pushed from a2 to b1 (a descendant of `base.sha`) | b1 | a1 for 5 s, then b1 | b1 | a1, then b1 | x, y, z, then y, z with `base.sha` | `base.sha` |

- In every state, GitHub's Files changed (REST `pulls/N/files`, the web
  `.diff`, and the web Files changed page fetched for #90) and its commit list
  follow **the merge base of `base.sha` and the head**. Where the branch's
  merge base differs, the branch's answer is not GitHub's basis.
- `LEFT` lines are numbered in that same merge base. On #90 (A at b2, merge
  base by branch b2, by `base.sha` a1), a `LEFT` comment on `x.txt` line 3 was
  accepted (`diff_hunk` `@@ -1,6 +1,6 @@`), though `x.txt` is absent from the
  branch's comparison.
- `base.sha` stays while the base branch only moves forward, including
  fast-forwards to the head's commits and merges of them. It moves on a head
  push that merges the base (BPR-1560's rule), and, measured here, to the
  branch's new tip one to six seconds after the base branch is force-pushed,
  even onto a descendant of `base.sha` (#93). In that window GitHub's file
  list still followed the old `base.sha` (#93), or for about 5 s led `pulls/N`
  (#91, #92 "B merges A": the files endpoint answered the new state while
  `pulls/N` still named the old head and base).
- The pull request's `commits` equals compare(`base.sha`...head)'s `ahead_by`
  in every state measured, and differs from the branch's when the merge bases
  differ.

## Decision

GitHub's Files changed does **not** follow the branch's merge base. It follows
the recorded base's merge base, reliably, in every state measured. So the
right answer in the stacked state is neither the branch's merge base nor a
refusal: it is compare(`base.sha`...head)'s merge base. This is a third
outcome, beyond the two the mandate named (accept the branch's answer, or keep
the refusal with accurate wording); it is flagged for the owner's review.
`refused` is kept, with its own wording, for genuine movement.

Rule (open pull request; a closed one is unchanged and asks only `base.sha`):

1. compare(`base.sha`...head): the merge base GitHub compares from (exact
   pair, remembered with its `ahead_by`).
2. compare(base branch...head). The same merge base: accepted as before.
3. Another merge base: accepted, as the recorded base's, only when
   - GitHub's commit count for the pull request (`pulls/N` `commits`) equals
     step 1's `ahead_by`, so GitHub's own commit list is the recorded base's;
     and
   - compare(`base.sha`...T), T being the tip step 2 resolved, names `base.sha`
     itself as merge base: the branch only moved forward, so GitHub keeps
     `base.sha`.
   Otherwise `refused`: GitHub is replacing `base.sha` (a force-pushed base
   branch), or its answers disagree. The extra compare is asked only here.
4. The pull request's metadata again, unchanged (as before).

Unchanged BPR-1560 properties: lost authentication fails the whole read; any
other failure is `failed`, the only state a window carries a merge base across;
a refusal is never overridden and a failed read after one brings nothing back;
the recorded base is never compared from; publication rechecks run before any
write.

## Change

- **Provider** (`github_provider.cpp`, `github_paged_read.{hpp,cpp}`): the rule
  above; the merge-base read carries `ahead_by` (absent allowed, malformed
  refused); the remembered exact pair keeps it.
- **Publication** (`github_mutation_contract.{hpp,cpp}`,
  `review_submission_session.cpp`): before anything on a LEFT line is written
  (single comment, review, tour stop, resumed tour), the base branch's merge
  base is asked as before; when it is not the comparison base, two new reads,
  `observe_recorded_merge_base` (compare `base.sha`...head) and
  `observe_base_ancestry` (compare `base.sha`...base branch), must name the
  comparison base and `base.sha`, or the write is `target_moved`
  (`left_lines_numbered_in_comparison_base`). Without this, every LEFT
  publication on a stacked pull request would have been refused.
- **Wording** (BPR-1560 follow-up): `refused` hands over
  `comparison_base_basis::disputed_base`; the core resolves Whole PR as under
  `recorded_base` with the reason `merge_base_disputed`, shown as "GitHub's
  answers about where this pull request branched disagree, so Whole PR cannot
  match its Files changed yet. Refresh in a moment, or compare two updates."
  `failed` keeps "GitHub did not say where this pull request branched…".
- **Fixture**: `stacked-merge-base-v1`, built by
  `scripts/build_stacked_merge_base_fixture.py` (recorded base A1 is the merge
  base, the base branch fast-forwarded to B1, the head B2; Files changed
  `x.txt`, `y.txt`). `build_recorded_pull_request_contract.py` accepts a
  construction that declares `packed_history` (every listed commit is in its
  own pack) and serves its earlier commits as they are; every other fixture
  regenerates unchanged. The fixture server answers a stacked descriptor's
  compares from the graph, with `ahead_by`, and a `StackedBaseRewritten`
  option answers that `base.sha` is not an ancestor of the tip.
- **Live**: `bpr1560_live_parity.py` takes GitHub's basis from
  compare(`base.sha`...head); `tests/end_to_end/bpr1572_stacked_live_course.py`
  runs the merge, fast-forward and rebase variants with a negative control.
- **Sparse lineage for an unchanged head** (`installed_git_cache.cpp`, found by
  the rebase course): on a selected-objects (signed-in) review the Git cache
  reported head lineage `unknown` whenever there was a prior head, even when
  it was the same commit, and the core refuses an unchanged head with any
  lineage but `not_applicable` as an invalid observation. So a revision whose
  merge base alone moved (the base branch rebased, GitHub resetting
  `base.sha` with the head unchanged) left review history "could not be
  updated. The review-history observation is invalid." and no comparison.
  Predates this card (BPR-1560's courses always moved the head). The sparse
  path now claims lineage only when the head changed, as the complete path
  already did.
- **Live course tool**: `live_second_account_course --course parity` prints
  what the review shows when the first file's comparison does not settle.
- **Composition**: the round-trip composition contract is resealed over main
  at `0c6d520ad` (full application digest `ec46df28…`, production manifest
  `c16d0693…`); `github_application.cpp` only serves the new fixture.
- **Docs**: `docs/architecture/git-and-epoch-model.md`.

## Evidence (branch `bpr-1572-stacked-merge-base`)

- **Unit.** `github_provider`: agree is available from the tip; a base branch
  moved forward (count matches, `base.sha` an ancestor) is available from the
  recorded base's merge base after three compares; a rewritten branch, a count
  that differs, a missing count, and the pull request moving are `refused`; a
  failing ancestry read is `failed`. `github_paged_read`: `ahead_by`.
  `github_mutation_contract`: the two new GETs are canonical, cannot be
  relabelled, bind base commit and echoed head; the decision function.
  `live_review_submission_provider` and `staged_review_session`: a review with
  a LEFT comment and a tour with a LEFT stop are published when the branch
  only moved forward and refused before any write when it was rewritten.
  `core_epoch_policy`, `application_github_window_read_host`,
  `qt_epoch_comparison_scale_*`: disputed basis, reason and text.
- **Fixture.** `application_merge_base_public_path`: on stacked-merge-base-v1
  Whole PR shows exactly GitHub's lines for `x.txt` and `y.txt`; with the
  branch rewritten it says GitHub's answers disagree and compares nothing from
  the recorded base; the race course now shows the disagreement text;
  merge-base-v1 unchanged. `merge_base_fixture_contract` regenerates both trees
  byte for byte and checks the stacked descriptor.
- **Negative control.** With the stacked acceptance reverted (always refuse)
  and the publication rule reverted (branch only): `github_provider`,
  `github_mutation_contract`, `live_review_submission_provider`,
  `staged_review_session` and `application_merge_base_public_path` (stacked
  Whole PR shows nothing) all fail.
- **Live** (shipped composition offscreen, signed in, one data root per course,
  `bpr1572_stacked_live_course.py`):
  - merge, #94: open, A merges b1 (branch merge base b1, recorded a1),
    A merges all of B (merged): PASS 3/3 files at every step.
  - fast-forward, #95: open, A fast-forwarded to b1, then b2, ordinary push b4
    to B (`base.sha` stays a1): PASS at every step.
  - rebase, #96, #98, #99 before the lineage fix: open PASS; A rebased onto a
    newer main (GitHub resets `base.sha` to the new tip, both merge bases
    agree on the fork point, the head is unchanged): no comparison, "Review
    history could not be updated. The review-history observation is invalid."
    (#99, with the new diagnostic line); B rebased onto A: PASS.
  - rebase, #100, with the lineage fix: open, A rebased (w.txt, x.txt, y.txt,
    z.txt from the fork point), B rebased: PASS at every step.
  - negative control, #97, a build with the stacked acceptance reverted:
    step 1 agrees with GitHub; step 2 (A merges b1) reproduces the defect,
    Whole PR shows nothing ("negative control: reproduced the mismatch").
- **Lineage negative control.** `git_sparse_cache_contract` fails ("sparse
  witness binds exact operation objects without ancestry claim") with the
  fix reverted and passes with it.
- **Gates.** clang-format 22 on every changed C++ file. clang-tidy on the
  changed translation units passes; the remaining warnings are on lines this
  card does not touch (`github_paged_read.cpp` 4401, older test helpers);
  three string-concatenation findings in the new test code were fixed.
  `DOCUMENTATION_OK` and `verify_documentation_tests` OK on the branch merged
  with main at `0c6d520ad`. Composition contracts: the round-trip contract
  resealed and passing, `application_composition_contract` and the other nine
  passing. Full `ctest --preset dev -j 6` on `b39c5a8c0`-equivalent tree
  (main merged at `0c6d520ad`): 762/770 under load average 80-156 with other
  agents' builds; the eight failures, rerun alone, all pass:
  `documentation_structure`, `documentation_issue_graph_contract` and
  `issue_id_allocation_contract` (the worktree lacks the ignored `.agents`
  skills link the board model is imported from; passed with it linked),
  `coding_format_check` (`git_cache_contract_tests.cpp` lines this card added,
  reformatted with clang-format 22), `qt_composed_long_thread_scale_100` and
  `_200`, `release_update_native_https`, `application_line_draft_contract`
  (load).

## Follow-ups

- **Endpoint lag.** For up to about 5 s after a push, GitHub's files endpoint
  can answer a newer state than `pulls/N` (#91, #92). The snapshot's
  file-count, commit-count and last-commit checks and the confirming read
  catch it when the counts differ. Predates this card; unchanged.
- **A base force-pushed onto a descendant of `base.sha`** (#93): for the 1-6 s
  before GitHub resets `base.sha`, the provider accepts the recorded base's
  merge base, which is what GitHub's Files changed still shows in that window;
  the next read follows the reset.
- **Residual carry-forward window** (BPR-1560): unchanged; a failed read keeps
  the merge base named for the same head, recorded base and base branch.

## Acceptance

- `BPR-1572:measurement`: the table above.
- `BPR-1572:provider`: `github_provider` and `github_paged_read`.
- `BPR-1572:publication`: `github_mutation_contract`,
  `live_review_submission_provider`, `staged_review_session`.
- `BPR-1572:wording`: `core_epoch_policy`, `application_github_window_read_host`,
  `qt_epoch_comparison_scale_*`, `application_merge_base_public_path`.
- `BPR-1572:fixture`: `application_merge_base_public_path` on
  stacked-merge-base-v1 and `merge_base_fixture_contract`.
- `BPR-1572:live`: `bpr1572_stacked_live_course.py`, merge, fast-forward and
  rebase variants, every step PASS.
- `BPR-1572:negative-control`: the unit, fixture and live controls above.
- `BPR-1572:gates`: one full `ctest --preset dev`, clang-format 22, tidy on the
  changed sources, the documentation gate, the composition contracts.

