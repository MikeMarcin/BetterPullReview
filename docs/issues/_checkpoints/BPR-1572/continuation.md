# BPR-1572 engineering checkpoint

Branch `bpr-1572-stacked-merge-base` in the worktree
`/Users/jaeger/.codex/worktrees/bpr1572/BetterPullReview`, from `11c62b589`
(the allocation on main), main merged at `0c6d520ad`. Not merged to main: the
owner runs independent Fable and Astra review first.

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

## Pending

- Fable and Astra review of the branch again (owner).
- At landing: merge main again, reseal the round-trip composition contract if
  `github_application.cpp` or the production manifest moved, rerun the
  documentation gate, move the card to `60-done/` with its acceptance records.

## Next action

Owner's review. Re-run evidence with
`python3 tests/end_to_end/bpr1572_stacked_live_course.py --build build/dev --work <dir> --prefix <p> --variant merge|ff|rebase [--negative-control] --cleanup`.
