---
schema: "implement-rfc.card.v1"
id: "BPR-1572"
title: "Stacked pull requests: Whole PR compares from the recorded base's merge base when the base branch takes the head's commits"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1560"]
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"BPR-1560 follow-up (stacked pull requests), owner approved 2026-10-17","governing_rfc":"RFC-BPR-0029","related_cards":["BPR-1560"],"reservation":"docs/issues/_reservations/BPR-1572--BPR-1572--7334e3717c3a.json","acceptance":["BPR-1572:measurement","BPR-1572:provider","BPR-1572:live","BPR-1572:negative-control","BPR-1572:gates"]}
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
suite, then Astra (Codex) review only.

## Measurement (github.com, MikeMarcin/BPR-Test, 2026-10-17)

Scratch main `M0`; A = `M0 + a1` (w.txt); B = `a1 + b1 (x.txt) + b2 (y.txt) +
b3 (z.txt)`, a pull request into A. After each step, once GitHub settled
(polled 45 s or more): `pulls/B` (`base.sha`, state, commits), `pulls/B/files`,
the public `pull/B.diff` (GitHub's web diff), and compare against the branch
and against `base.sha` (merge base and file list). Scripts and logs were kept
in the session scratchpad, not committed.

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

Rule (open pull request; a closed one is unchanged and asks only `base.sha`):

1. compare(`base.sha`...head): the merge base GitHub compares from (exact
   pair, remembered with its `ahead_by`).
2. compare(base branch...head). The same merge base: accepted as today.
3. Another merge base: accepted, as the recorded base's, only when
   - GitHub's commit count for the pull request (`pulls/N` `commits`) equals
     step 1's `ahead_by`, so GitHub's own commit list is the recorded base's;
     and
   - compare(`base.sha`...T), T being the tip step 2 resolved, names `base.sha`
     itself as merge base: the branch only moved forward, so GitHub keeps
     `base.sha`.
   Otherwise `refused`: GitHub is replacing `base.sha` (a force-pushed base
   branch), or its answers disagree. The extra compare is asked only here.
4. The pull request's metadata again, unchanged (as today).

`refused` is kept for genuine movement only. Unchanged: lost authentication
fails the whole read, any other failure is `failed` and the only state a
window carries a merge base across, a refusal is never overridden, and LEFT
publication rechecks run before any write.

## Change

- `src/adapters/github/src/github_paged_read.{hpp,cpp}`: the merge-base read
  carries `ahead_by` (absent allowed; malformed refused).
- `src/adapters/github/src/github_provider.cpp`: the rule above; the remembered
  exact pair keeps its `ahead_by`.
- `src/application/include/bpr/application/github_read_model.hpp`: the
  availability documentation.
- `tests/end_to_end/bpr1560_live_parity.py`: GitHub's basis is
  compare(`base.sha`...head) for open pull requests too.
- `tests/end_to_end/bpr1572_stacked_live_course.py`: the live stacked course.
- `docs/architecture/git-and-epoch-model.md`: the measured rule.

## Evidence

- `github_provider` (`test_merge_base_failure_and_refusal_are_distinct`):
  agree is available from the tip; a forward-moved base branch (count matches,
  ancestor) is available from the recorded base's merge base, asked three
  compares; a rewritten base branch (not an ancestor), a count that differs,
  a missing count and the pull request moving are `refused`; a failing
  ancestry read is `failed`. `github_paged_read`: `ahead_by` parsed, absent
  allowed, malformed refused.
- Live and gates: see below.

## Follow-ups (not changed, lean run)

- **LEFT publication on a stacked pull request.** The publication recheck
  still asks the base branch's merge base only, so in the stacked state a LEFT
  comment, a review with one, or a tour with a LEFT stop is refused as
  `target_moved` before any write. Safe, but those publications fail until
  the base branch and `base.sha` agree again. Fix: the same rule in
  `review_mutation_session` and `review_submission_session` (recorded-base and
  base-ancestry compares).
- **Refusal wording** (BPR-1560 follow-up): `refused` still shows the
  `merge_base_unavailable` text; it is now rarer (genuine movement only).
- **Fixture.** No stacked fixture tree was added. The provider unit tests and
  the live course cover the rule.
- **Endpoint lag.** For up to about 5 s after a push, GitHub's files endpoint
  can answer a newer state than `pulls/N`. The snapshot's file-count, commit
  count and last-commit checks and the confirming read catch it when the
  counts differ. This predates this card.

## Acceptance

- `BPR-1572:measurement`: the table above.
- `BPR-1572:provider`: `github_provider` and `github_paged_read` as above.
- `BPR-1572:live`: `bpr1572_stacked_live_course.py` passes: open; A merges b1
  (stacked, merge bases differ; Whole PR equals GitHub's Files changed from
  `base.sha`'s merge base); A merges all of B (merged, closed).
- `BPR-1572:negative-control`: `github_provider`'s stacked course fails with
  the provider change reverted.
- `BPR-1572:gates`: one full `ctest --preset dev`, clang-format 22, tidy on the
  changed sources, the documentation gate.
