# BPR-1572 engineering checkpoint

Branch `bpr-1572-stacked-merge-base` in the worktree
`/Users/jaeger/.codex/worktrees/bpr1572/BetterPullReview`, from `11c62b589`
(the allocation on main). The owner’s ordered 2026-10-17 handoff accepts
Fable and Astra round four and authorizes landing after focused verification.
Current main `ca55c0a71` is merged and composition resealed at `ce38d487e`.

## State

- Measured on BPR-Test (#89-#100, all closed, every `bpr1572-*` branch
  deleted): GitHub's Files changed, commit list and LEFT numbering follow the
  merge base of `base.sha` and the head; `base.sha` stays while the base branch
  only moves forward and is reset to the branch's tip 1-6 s after the branch is
  force-pushed. The card holds the table; the raw logs were lost when the
  session scratchpad was emptied on restart.
- Implemented: provider rule (accept the recorded base's merge base when the
  base branch names another, only with GitHub's commit count matching and
  `base.sha` an ancestor of the tip), the same rule before LEFT publication,
  `disputed_base` / `merge_base_disputed` wording for genuine refusals, the
  stacked-merge-base-v1 fixture, and the sparse head-lineage fix the rebase
  course exposed.
- Verified: focused rows, negative controls (unit, fixture, live, lineage),
  live merge / fast-forward / rebase courses, clang-format 22, tidy on changed
  translation units, documentation gate, composition contracts (round-trip
  resealed), one full `ctest --preset dev` with every failure rerun green.

## Review round one

Fable approved; Astra rejected (P1-1 publication reconfirmation and tip
binding, P1-2 publication commit count, P2 read confirmation commit count,
live-course notes, criss-cross). All addressed on this branch; see the card's
"Review round one" and "Live round two". Live records:
`live-2026-10-17.jsonl` beside this file (one JSON line per step: GitHub's
state, timeline, the app's lines, the verdict).

## Review round two

Fable approved; Astra rejected (tours read after the confirmation; the
agreeing path skipped the final confirmation; state the inherent GET/POST
limit). Fixed on `bdd64ad72` and `0bdb5673b`; see the card's "Review round
two" and "Gates, review round two".

## Review round three

Fable approved; Astra rejected (the GitHub CLI account wrapper's account
checks read after the final confirmation). Fixed on `3b79ec5a1`: the wrapper
reads the pull request once more after its pre-write account check. Three
large-review timing budgets still to rerun on a quiet machine.

## Review round four and ordered landing

Fable approved; Astra approved with a propagation condition. The condition and
Fable’s three follow-ups are implemented in `c8a7ec0d7`: refused review, tour
and source-backed comment writes keep `target_moved`/`stale_head` and unsent
status; the wrapper has 500 and cancellation courses; its comment states the
head/recorded-base limit; the architecture wording covers every witnessed
write bound to a pull request. The owner’s handoff accepts the reviews and
requires no further review or live course. Existing negative controls stand.

Main `ca55c0a71` was merged at `ce38d487e`. The round-trip contract’s full
application and production-manifest pins were recomputed on that merged tree;
the mutation-production pin remains unchanged. A dated note records the scope.
All 20 focused landing rows pass (239.48 s + 343.22 s); all affected targets
build, all 35 C++ files pass format22, and docs/diff checks pass. The handoff moves the full suite and
large-review timing reruns to its final integrated pass (item 9).

## Next action

Finish the card on main, push, then remove this
worktree and its build. Do not restart owner review or the completed live course.
