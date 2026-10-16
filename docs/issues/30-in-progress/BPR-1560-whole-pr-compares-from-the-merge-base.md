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

`base.sha` is not the merge base. Measured on github.com (BPR-Test #75, #76,
#78; below), GitHub sets it to the base branch tip when the pull request opens
and again on every force push, keeps it across ordinary pushes and across the
base branch's own advances, and moves it forward on an ordinary push only when
that push merges a newer base commit (to that commit, the new merge base). So
whenever the base branch was ahead of the fork point when the pull request
opened, or when the branch was last force-pushed (a squash or a rebase onto an
older base commit), `base.sha` is ahead of the merge base and the left side
carries the base branch's own later changes:

Owner data, 2026-10-16 (relayed; no hashes kept): on the owner's pull request
`base.sha` and the compare merge base for `base.sha...head` differ, so it is in
exactly that state. The pull request now shows one commit after earlier merges
from main: it was squashed (possibly onto a newer main) and force-pushed, which
on github.com sets `base.sha` to the base tip at the push (course #78, steps 3
and 5). Whichever rule the owner's server follows, the fix does not depend on
it: it never compares from `base.sha`.

Further owner evidence (relayed 2026-10-16): the file whose right pane was not
syntax-highlighted ([[BPR-1558]], fixed on main at `ec097d8dc`: identical blobs
on both sides only coloured the left) was one of the 0-change files, that is
identical blobs on both sides, which is this card's second symptom.

Not a regression: two-dot from the recorded base has been the Whole PR
contract since the epoch core (`618c56419`, `cac0f5169`, 2026-07-30). BPR-0944
recorded the same anomaly on 2026-09-20 and the owner deferred the decision;
this report supersedes that deferral. `0bfcbaa9a` (sparse loading for every
signed-in review) did not change the comparison basis; it did make the local
merge base underivable on a signed-in review (`prepare_sparse_revision` never
proves ancestry), so the only merge-base consumer, BPR-1212's LEFT-comment
placement, is unavailable on every signed-in review within the read limits. No other defect was found on this path: on
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
- **`base.sha` rule, measured** (github.com, 2026-10-16): after a push to the
  base branch then an ordinary push to #76's head, `base.sha` stayed at the
  tip from when #76 opened (not the new tip); a merge of the base into #75
  moved `base.sha` together with the head in one observation (polled every
  1-3 s, no window seen) to the merged commit; on a squash course (#78) a
  force push set `base.sha` to the base tip at the push whether the squash sat
  on the fork point (merge base two commits behind), on the tip, or on an older
  base commit. In every state measured, compare's merge base against `base.sha`
  equalled compare's merge base against the base branch tip.
- **#75** (`bpr1560-feature`): four iterations, two merges from the base (one
  of an older base commit), main advancing in between. After each merge GitHub
  reset `base.sha` to the new merge base, so at the measured state the same
  course matches GitHub exactly (3/3 files): the defect is intermittent and
  depends on how `base.sha` relates to the merge base.

## Design

- **Source of the merge base: GitHub.** After each pull request read the
  GitHub provider asks REST compare (`get_merge_base`, page 2 of one commit
  per page, which carries the merge base but no file list). An open pull
  request asks for its current head against its base branch by name; the
  answer names the tip GitHub resolved (`base_commit`) and the merge base
  (`merge_base_commit`) together, so tip and merge base come from one
  response, and nothing is remembered under a branch name that can move. A
  closed pull request, whose branch may have moved on or taken the head, asks
  against its recorded base; that exact pair's answer is remembered (bounded,
  by host, profile, repository, base and head), so a poll asks nothing. The
  answer must name a full commit of the head's format for both, and for an
  exact pair the base asked; anything else is a failure. The application never
  computes the merge base itself (several local merge bases on a criss-cross
  history, or none on a sparse review, would disagree with GitHub) and never
  substitutes `base.sha`.
- **The snapshot** carries `MergeBaseOid`, `MergeBaseTipOid` and
  `MergeBaseAvailability` (`unavailable`: not asked; `available`; `failed`:
  not found, no common ancestor, refused, timed out, malformed).
- **The handoff** (`github_window_read_host.cpp`) gives the epoch host the
  merge base as the revision's `Base` and keeps GitHub's `base.sha` as
  `RecordedBase` for the publication staleness check. Live epochs therefore
  record, fetch (sparse or complete) and compare from the merge base; Whole
  PR, the Updates line's base ([[BPR-1557]], which reads the same
  `pull_request_base`), source search and the cached overview's source follow
  it. A merge from the base, a rebase or a squash onto another base commit
  moves the merge base and appends an epoch whose cause is a base change; an
  ordinary push does not. A catalogue written before this change holds
  `base.sha` on its earlier epochs; the first read afterwards appends one
  base-change epoch.
- **No merge base named.** The handoff carries the recorded base under
  `comparison_base_basis::recorded_base`; the epoch is appended so updates stay
  comparable, the core resolves Whole PR with its base anchor unavailable and
  `merge_base_unavailable` ("GitHub did not say where this pull request
  branched, so Whole PR cannot match its Files changed. Compare two updates,
  or refresh."), and the epoch host refuses that pair to the diff pipeline,
  though the recorded base is in the cache. Interdiffs and exact pairs are
  untouched.
- **LEFT lines.** GitHub numbers a `LEFT` line in the merge base. The mutation
  target keeps `BaseOid` (GitHub's `base.sha`, checked against GitHub before
  publishing) and gains `ComparisonBaseOid` (set only when it differs), and
  every LEFT source check uses `comparison_base(target)`: staged comments and
  tour stops, reading staged comments back, the anchor gate, the fixture
  server. The submission journal writes version 3 only for such a target, so
  every earlier record keeps its bytes and fingerprint, and the submission
  identity ignores a moved merge base as it ignores a moved base, so a draft or
  a pending attempt survives it. BPR-1212's numbering answers "same" without
  the cache when the left commit is the merge base GitHub named, which
  restores LEFT placement on signed-in (sparse) reviews.
- **Interdiff on a signed-in review.** Previous upload's left side, the
  preceding update's retained head, is proved from its sparse retained refs
  exactly as an explicitly chosen anchor is; before, every sparse review showed
  Previous upload as unavailable.
- **Anonymous public preview** admits the read on github.com; GHES signed out
  refuses it before any request, as every anonymous GHES read.

## Evidence (fix branch `bpr-1560-merge-base`)

Kept in the worktree's ignored `build/dev/bpr1560-evidence/` (the session
scratchpad was emptied once mid-run, losing the first runs' logs; their
numbers are kept here).

- **Fixture.** `application_merge_base_public_path` passes: a.txt, b.txt,
  new.txt and shared.txt show exactly GitHub's lines; with compare failing,
  Whole PR says "GitHub did not say where this pull request branched…" and
  no comparison from the recorded base is shown. Negative control (the
  handoff given the recorded base): fails at a.txt, `removed=3 added=3`,
  "the removed lines are GitHub's: no line of the base branch's own is
  reverted".
- **Live #76** (`base.sha` ahead of the merge base): `bpr1560_live_parity.py`
  PASS, 3/3 files, blobs from the merge base; the three LEFT comments posted by
  MikeMarcinTest (a.txt 2, b.txt 3, shared.txt 5) are placed on the left at
  "Line 2", "Line 3" and "Line 5". Negative control on the same build with the
  recorded base: 10 differences, b.txt's blobs equal ("both sides show the
  same blob"), line 50/40/55/58 reverts, and no LEFT thread placed.
- **Live #75** (two merges from the base): PASS, 3/3, before and after the fix.
  After merging #75 (closed, merged): compare against `base.sha` names
  `017af0a` (GitHub's basis, files unchanged), compare against the branch
  names the head itself (`behind`, an empty diff), which is why a closed pull
  request asks against its recorded base.
- **Squash course** (`bpr1560_live_course.py --prefix bpr1560-sq5`, #83,
  signed in, sparse, one data root across all steps): every step passes
  (`summary: 1-open=ok, 2-merged=ok, 3-squash-fork-point=ok,
  4-squash-on-tip=ok, 5-squash-on-older-base=ok`). Whole PR shows GitHub's
  status and lines on every file (modified, added, renamed with no change,
  removed), including the squash on the fork point (`base.sha` = tip, merge
  base two commits behind) and the squash onto an older base commit
  (`base.sha` = tip, merge base one behind); the interdiff from the previous
  update shows both heads' blobs for every file from step 2 on, a file both
  updates lack reported as such. An earlier run (#82) failed step 3 once in
  Git acquisition (transport `response_limit` while the composition contracts
  ran beside it); forcing the same head again on the same data root passed.
  Not reproduced since; recorded.
- **Squash course negative control** (`--negative-control`, #84, the handoff
  given the recorded base): the mismatch is reproduced exactly where GitHub's
  `base.sha` is ahead of the merge base, the squash on the fork point (step 3)
  and the squash onto an older base commit (step 5), the owner's state; where
  `base.sha` is the merge base (steps 1, 2, 4) the two agree.
- **Merged #75** (closed, merged): Whole PR PASS 3/3 against GitHub, asked
  against the recorded base.
- **Gates.** One full `ctest --preset dev` on `d8cdf5970` (main merged at
  `ec097d8dc`): 746/754 passed. The eight failures and their dispositions:
  `coding_format_check` (the merge's `github_provider.cpp` alignment,
  reformatted), `github_contract_fixture_security` and
  `test_network_isolation_manifest` (the new fixture tree and test row
  registered), `application_logging` (an unknown `GetMergeBase` name dropped
  the read's log record; added to both name tables),
  `application_epoch_portable_occurrence_two_catalogues` (the rewritten-base
  course's base shares no history with the head, so GitHub's compare finds no
  merge base and Whole PR is now honestly unavailable; the course accepts that
  state with its reason), and `git_process_contract` (timeout under load),
  `application_label_authoring_contract`, `application_terminal_diagnostics`
  (load). All eight pass on `2aade62c1`, as do the nine composition contracts
  after the reseal. clang-format 22 clean; clang-tidy clean (warnings as
  errors) on every changed source except four findings in `src/app/main.cpp`
  on lines this change does not touch (identifier naming at 2194 and 9664, an
  optional conversion at 10143, a use after move at 10151, from `bf04e3918f`
  and earlier); `DOCUMENTATION_OK`.

## Not changed, deliberately

- `review_queue_facts.cpp` and the projection's `BaseOid` keep GitHub's
  recorded base: they describe the pull request, not a comparison.
- Designated live targets (BPR-Test #2) and their bootstrap name one base;
  there it is the merge base too.
- An imported historical epoch's base (an update GitHub recorded, imported on
  demand) is the current comparison base: in complete mode Git derives the
  merge base of it and the imported head, in sparse mode it is taken as is.
  "Show this update alone" on such an imported pre-squash update can therefore
  differ from GitHub's view of that update; Whole PR and interdiffs (head to
  head) do not depend on it. Recorded as a follow-up below.

## Acceptance

- `BPR-1560:root-cause`: the reproduction above, reviewed independently
  (Fable: confirmed; Astra critical: confirmed, mechanism reproduced on #76).
- `BPR-1560:model`: `core_epoch_policy` shows Whole PR compares from the
  merge base GitHub named, says `merge_base_unavailable` with its base
  unavailable when GitHub named none, and leaves interdiffs and exact pairs
  alone; the same revision losing its merge base is accepted again.
- `BPR-1560:adapter`: `github_paged_read` asks compare once on the page with no
  file list (branch-encoded for an open pull request, exact commits for a
  closed one), binds the answer, and refuses a missing, abbreviated,
  other-format or other-base answer; not found, server error and timeout leave
  no merge base; GHES asks under `/api/v3`; signed out it is admitted on
  github.com and refused on GHES before any request. The contract counts
  (`github_read_contract`, `github_anonymous_read_contract`,
  `native_https_anonymous_read_contract`) name the twentieth read.
- `BPR-1560:publication`: `github_mutation_contract` refuses a staged LEFT line
  on the recorded base when GitHub numbers LEFT in a distinct merge base and
  accepts it on the merge base; `sqlite_review_submission_recovery_storage`
  keeps a distinct comparison base across a restart (version 3, with and
  without line comments) and the submission identity across a merge-base move;
  live, existing LEFT comments on #76 are placed on the merge base's lines.
- `BPR-1560:fixture`: `application_merge_base_public_path` opens the
  merge-base-v1 fixture through the shipped composition: every file shows
  exactly GitHub's removed and added lines (no base-branch line reverted,
  `b.txt` changed), and with GitHub's compare failing Whole PR says so and no
  comparison from the recorded base is shown.
- `BPR-1560:live`: `bpr1560_live_parity.py` passes on BPR-Test #75 and #76, and
  `bpr1560_live_course.py` passes every step of a fresh course: open, merge
  from the base, squash on the fork point, squash onto the base tip, squash
  onto an older base commit (statuses including a rename and a removal, Whole
  PR lines, and the interdiff from the previous update), signed in (sparse).
- `BPR-1560:negative-control`: the fixture test, the parity course on #76 and
  the squash course fail on the comparison from the recorded base.
- `BPR-1560:gates`: one full `ctest --preset dev`, clang-format 22, tidy on
  changed files, the documentation gate and the composition contracts.

## Follow-ups

- An imported historical update's own base on a sparse review (see "Not
  changed, deliberately"): give a sparse import its own merge base from GitHub,
  or mark "Show this update alone" unavailable for it.
- The unavailable Whole PR leaves the centre pane at "No file open"; the reason
  is in the Updates control and the status bar. [[BPR-0944]] AC4/AC5 own the
  pane's explanation.

## Coordination

[[BPR-1557]] draws the Updates line's base from the core's
`pull_request_base`, the function Whole PR takes its left side from, so its
anchor is the merge base GitHub named with no change on its side. A base change
now means a merge from the base, a rebase or a squash onto another base commit
(or this change's one-time upgrade epoch), not the base branch merely moving.
