---
schema: "implement-rfc.card.v1"
id: "BPR-1560"
title: "Whole PR compares from the merge base, as GitHub's Files changed does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-16","governing_rfc":"RFC-BPR-0029","related_rfcs":["RFC-BPR-0003","RFC-BPR-0013","RFC-BPR-0028"],"latent_since":"618c56419","related_cards":["BPR-0944","BPR-0943","BPR-1212","BPR-1553","BPR-1557"],"reservation":"docs/issues/_reservations/BPR-1560--BPR-1560--d3662cff3d94.json","acceptance":["BPR-1560:root-cause","BPR-1560:model","BPR-1560:adapter","BPR-1560:fixture","BPR-1560:live","BPR-1560:negative-control","BPR-1560:gates"]}
acceptance: []
---

# BPR-1560: Whole PR compares from the merge base, as GitHub's Files changed does

## Owner notes

Owner, chat, 2026-10-16, on the installed app (0.2.x), relayed by the
coordinator: "we're showing the wrong diffs relative to what GitHub shows. The
PR has gone through several iterations and got merges in from main along the
way. I'm seeing diffs that belong to unrelated changes that landed on main and
presumably got merged into the PR branch. And even more crucially I am NOT
seeing changes that are for this PR on some files — those files show 0 changes
when they have clear changes on GitHub, and both the left and right side of our
diff show our PR's changes."

The coordinator's mandate: the default comparison equals GitHub's Files changed
exactly (file set, statuses, renames, hunks) across merges from the base,
base-branch advances, force pushes and sparse fetches; an interdiff between
updates stays correct and separate; a merge base that cannot be determined
reliably never yields a silently wrong diff. Owner cross-review (an Astra
critical reviewer and a Fable reviewer) checks the root cause before the fix
and the fix with live parity evidence before landing.

## Root cause (confirmed 2026-10-16)

Whole PR compares **GitHub's recorded base commit** for the pull request
(GraphQL `baseRefOid`, REST `base.sha`) two-dot to the head. GitHub's Files
changed, and the file list this application shows (the provider's
`pullRequest.files` / `pulls/N/files`), compare the **merge base** of that
base and the head to the head (three-dot). Every file row comes from GitHub's
merge-base list, but both blobs of its comparison are read at the recorded
base and the head.

- `src/adapters/github/src/github_paged_read.cpp` reads `baseRefOid` into the
  snapshot's `PullRequest.BaseOid`.
- `src/application/src/github_window_read_host.cpp` (`make_epoch_handoff`)
  hands it to the epoch host as the accepted revision's `Base`.
- The live epoch records it as `review_epoch_summary::Base`; the Git cache
  retains exactly that commit's tree (sparse or complete).
- `src/core/src/epoch_transition.cpp` `select_pair`, `case
  named_comparison::whole_pr`: left is the latest live epoch's `epoch_base`
  anchor, that base commit.
- `docs/architecture/git-and-epoch-model.md`: "The friendly `Branch base`
  label resolves to the latest live epoch's accepted base-tip anchor. It is not
  a computed merge base."

`base.sha` is not the merge base. Measured on github.com (below), it is the
base branch's tip when the pull request opened, kept across ordinary pushes
and the base branch's advances, and reset only when a push changes the merge
base (a merge from the base). So whenever the base branch was ahead of the fork
point when the pull request opened, or moved past the merge base before a
later sync, the left side carries the base branch's own later changes:

- a base-branch change in a file the pull request also touches shows as a
  reversed hunk (the owner's "unrelated changes that landed on main");
- a file whose content on the base branch already equals the head's (the pull
  request's change also landed on main separately, or arrived through another
  branch) shows no change at all, with the pull request's text on both sides
  (the owner's "both the left and right side show our PR's changes"), while
  GitHub lists it as modified.

Not a regression: two-dot from the recorded base has been the Whole PR
contract since the epoch core (`618c56419`, `cac0f5169`, 2026-07-30). BPR-0944
recorded the same anomaly on 2026-09-20 and the owner deferred the decision;
this report supersedes that deferral. `0bfcbaa9a` (sparse loading for every
signed-in review) did not change the comparison basis; it did make the local
merge base underivable on a signed-in review (`prepare_sparse_revision` never
proves ancestry), so the only merge-base consumer, BPR-1212's LEFT-comment
placement, is unavailable there. No other defect was found on this path: on
the reproduction every left blob is exactly the recorded base's blob and
every right blob exactly the head's.

### Reproduction (MikeMarcin/BPR-Test, base branch `bpr1560-main`)

- **#76** (`bpr1560-feature2`): forked from `ffc2ef6d22ce`; the base branch had
  moved to `5476b9780a96` (it changes `a.txt` line 50, `shared.txt` lines
  40/55/58, and lands the PR's own `b.txt` line 3 change separately) when the
  pull request opened. GitHub: `base.sha=5476b9780a96`, compare merge base
  `ffc2ef6d22ce`; it stayed so after another ordinary push. Shipped
  composition, offscreen, signed in (sparse), Whole PR (`Comparing whole PR`),
  `live_second_account_course --course parity`:
  - `a.txt`: GitHub `+2 -2`; shown `+3 -3` with line 50 `a line 50 MAIN-2`
    reverted. Left blob `111c168f` is `5476b97:a.txt`, not the merge base's
    `aec6a2e4`.
  - `b.txt`: GitHub `+1 -1`; shown nothing. Left and right blob both
    `c965e09c` (the PR's text on both sides).
  - `shared.txt`: GitHub `+1 -1`; shown `+4 -4` with main's lines 40, 55 and
    58 reverted.
  - `bpr1560_live_parity.py --pull 76`: FAIL, 10 differences.
- **#75** (`bpr1560-feature`): four iterations, two merges from the base (one
  of an older base commit), main advancing in between. After each merge GitHub
  reset `base.sha` to the new merge base, so at the measured state the same
  course matches GitHub exactly (3/3 files): the defect is intermittent and
  depends on how `base.sha` relates to the merge base.

## Design (in progress)

- The comparison base of a live update is the merge base GitHub names for the
  pull request's recorded base and head (REST compare, `merge_base_commit`).
  Whole PR compares from it; the Updates line's base (BPR-1557) is that commit.
- When GitHub does not name it, no update is handed off on a guessed base: the
  comparison says it is unavailable rather than showing a two-dot diff.
- Interdiffs (update to update, last review to latest) compare heads and are
  unchanged.

## Acceptance

- `BPR-1560:root-cause`: the reproduction above, reviewed independently.
- `BPR-1560:model`: core and host tests show Whole PR's left anchor is the
  merge base GitHub named and never the recorded base when they differ.
- `BPR-1560:adapter`: the GitHub read contract issues the bounded compare read
  and normalizes `merge_base_commit`; malformed, absent and refused answers
  leave the merge base unavailable.
- `BPR-1560:fixture`: a deterministic composed fixture whose recorded base is
  ahead of the merge base shows GitHub's lines (no reversed base changes, the
  separately landed file still changed).
- `BPR-1560:live`: `bpr1560_live_parity.py` passes on BPR-Test #75 and #76 and
  a force-pushed course, signed in (sparse).
- `BPR-1560:negative-control`: the same live course and fixture fail on the
  build before the fix.
- `BPR-1560:gates`: one full `ctest --preset dev`, clang-format 22, tidy on
  changed files, the documentation gate and the composition contracts.
