---
schema: "implement-rfc.card.v1"
id: "BPR-1560"
title: "Whole PR compares from the merge base, as GitHub's Files changed does"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-16","governing_rfc":"RFC-BPR-0029","related_rfcs":["RFC-BPR-0003","RFC-BPR-0013","RFC-BPR-0028"],"latent_since":"618c56419","related_cards":["BPR-0944","BPR-0943","BPR-1212","BPR-1553","BPR-1557"],"reservation":"docs/issues/_reservations/BPR-1560--BPR-1560--d3662cff3d94.json","acceptance":["BPR-1560:root-cause","BPR-1560:model","BPR-1560:adapter","BPR-1560:fixture","BPR-1560:live","BPR-1560:negative-control","BPR-1560:publication","BPR-1560:regeneration","BPR-1560:round-three","BPR-1560:gates"]}
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

- **Source of the merge base: GitHub, bound to the snapshot.** After each
  pull request read the GitHub provider reads, in order (round 2, Astra B1):
  1. compare against the recorded base (`base.sha`...head, exact commits,
     remembered per exact pair so a poll asks nothing);
  2. for an open pull request, compare against the base branch by name, whose
     answer names the tip GitHub resolved; it must name the same merge base,
     or GitHub's two bases disagree and its file list's basis is not
     knowable;
  3. the pull request's metadata again, which must still be the snapshot's
     (head, recorded base, base branch, state, changed-file count), so the
     file list and the merge base describe one state of the pull request.
  A closed pull request, whose branch may have moved on or taken the head
  (measured on merged #75), uses the first only. Every compare page is page 2
  of one commit per page, which carries the merge base but no file list
  (measured: a one-commit compare's page 2 has no commits and the same merge
  base), and its answer must echo exactly the head asked about (`url`), name
  full commits of the head's format, and for an exact pair the base asked.
  Lost authentication on any of these reads fails the whole read, as on every
  optional read (Astra B3); any other failure leaves the merge base
  unavailable. The application never computes the merge base itself (several
  local merge bases on a criss-cross history, or none on a sparse review,
  would disagree with GitHub) and never substitutes `base.sha`.
- **The snapshot** carries `MergeBaseOid`, `MergeBaseTipOid` and
  `MergeBaseAvailability` (`unavailable`: not asked; `available`; `failed`:
  not found, no common ancestor, refused, timed out, malformed; `refused`:
  GitHub answered and the answers prove there is no single merge base for
  this snapshot, because the base branch names another merge base than the
  recorded base does, or the pull request moved before the confirming read).
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
- **A failed read after a good one.** For the same head, recorded base and
  base branch, a merge base GitHub already named outlives a later `failed`
  read (like the snapshot's other carried observations), so one transient
  failure neither hides Whole PR nor appends a base-change epoch and another
  back (Fable N1). A `refused` read is never overridden: the base branch can
  take commits the head shares and move the merge base with `base.sha` and
  the head unchanged, and the earlier merge base is then stale (Astra round
  three, P1-A). After a refusal a failed read brings nothing back, since the
  accepted snapshot it would carry from has no merge base.
- **No merge base named.** The handoff carries the recorded base under
  `comparison_base_basis::recorded_base`; the epoch is appended so updates stay
  comparable, the core resolves Whole PR with its base anchor unavailable and
  `merge_base_unavailable` ("GitHub did not say where this pull request
  branched, so Whole PR cannot match its Files changed. Compare two updates,
  or refresh."), and the epoch host refuses that pair to the diff pipeline,
  though the recorded base is in the cache. Any other comparison from that
  recorded base, the newest update shown alone included, is refused the same
  way (Fable N1). Interdiffs and exact pairs between heads are untouched. No
  cached overview is retained from such a read, so the offline source reader
  never reads the recorded base as the comparison's.
- **LEFT lines.** GitHub numbers a `LEFT` line in the merge base. The mutation
  target keeps `BaseOid` (GitHub's `base.sha`, checked against GitHub before
  publishing) and gains `ComparisonBaseOid` (set only when it differs), and
  every LEFT source check uses `comparison_base(target)`: staged comments and
  tour stops, reading staged comments back, the anchor gate, the fixture
  server. The submission journal writes version 3 only for such a target, so
  every earlier record keeps its bytes and fingerprint, and the submission
  identity ignores a moved merge base as it ignores a moved base, so a draft or
  a pending attempt survives it. Before anything on a LEFT line is written (a
  single comment, a whole review with a LEFT comment, a tour with a LEFT
  stop), the session asks GitHub's compare for the target's base branch and
  head once more; a merge base other than the one the lines were numbered in
  is `target_moved` and nothing is sent (Astra B2, Fable N2: a stacked pull
  request's base branch can take commits the head shares and move the merge
  base with `base.sha` and the head unchanged). A tour with a LEFT stop asks
  before every write, the first and each one a resumed journal makes after a
  denied authorization, a Stop or a restart (Astra round three, P1-B). BPR-1212's numbering answers "same" without
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
- **Round three, on `b5b2d4c04`** (snapshot-bound confirmation, carried-forward
  merge base, proven bases only, publication recheck). Squash course
  `--prefix bpr1560-r3`, #86: every step passes, the same five as above; steps
  3, 4 and 5 are single-commit pull requests (GitHub's compare on page 2 of
  one commit per page still names the merge base). #76 on a fresh data root:
  PASS 3/3, LEFT threads at "Line 2", "Line 3" and "Line 5" (the interdiff is
  unavailable there because that root has seen one update only, as before).
  Negative control (the handoff given `base.sha` as a merge base), `--prefix
  bpr1560-r3m`, #88: steps 1, 2 and 4 agree with GitHub; steps 3 and 5
  reproduce the owner's state (`b.txt` shown `+0 -0` against GitHub's `+1 -1`,
  `shared.txt` likewise at step 3, `a.txt` reverting a base line at step 5). An
  earlier negative run (#87, load average above 7) timed out at step 3 waiting
  for the first file and at step 4 waiting for GitHub CLI discovery; the course
  reports both steps failed rather than as a reproduction, and #88 repeated it
  cleanly. Zero commits: #85 above (GitHub closes it).
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
- **Gates, round three.** One full `ctest --preset dev` on `b5b2d4c04` (main
  merged at `c9ff3bf50`): 761/761 passed (`application_large_review_cache_bytes`
  skipped as on main). Before it, `application_github_window_read_host` (its
  snapshot named no merge base, so the offline reader kept nothing; the test
  now names one), `application_attended_recent_replay` and
  `application_label_authoring_contract` failed: the fixture answered the
  confirming pull request read only after a compare on the wire, so a
  remembered merge base (no compare) sent the confirmation into the strict
  recording. The fixture now answers a snapshot read's second pull request
  read with the one that read was served, by its parent. clang-format 22
  clean; clang-tidy clean on the changed sources (two identifier-naming
  findings in `github_window_read_host_tests.cpp` at 375 and 379 are from
  `ca000e791a`); the composition contracts pass; `DOCUMENTATION_OK`.

## Not changed, deliberately

- `review_queue_facts.cpp` and the projection's `BaseOid` keep GitHub's
  recorded base: they describe the pull request, not a comparison.
- Designated live targets (BPR-Test #2) and their bootstrap name one base;
  there it is the merge base too.
- An imported historical epoch's base (an update GitHub recorded, imported on
  demand): in complete mode Git derives the merge base of the current
  comparison base and the imported head; on a selected-objects review it is
  the current comparison base taken as is, unproven for that head, so that
  base is no anchor to compare from ("Show this update alone" is unavailable
  for it; Astra P2-4). Its head stays comparable.
- The snapshot reader refuses a pull request with no commits. Measured: GitHub
  closes a pull request whose head is forced onto its base tip (#85: state
  closed, 0 commits, 0 files; compare `identical`), so this concerns closed
  pull requests only; unchanged here.

## Operator notes

- **Upgrade.** A catalogue written before this change holds `base.sha` on its
  earlier epochs; the first read afterwards appends one base-change epoch per
  affected review (the base moves from `base.sha` to the merge base). Earlier
  epochs keep their bases; "Show this update alone" on them compares from
  what they recorded.
- **Tour drafts.** A tour draft written before this change with a LEFT stop on
  a pull request whose merge base is not its `base.sha` names `base.sha` as the
  stop's commit; it no longer passes the LEFT source check, so its publication
  is refused before any write (not re-anchored; the stop's line may not exist
  in the merge base). Drafts on pull requests whose merge base is `base.sha`
  are unaffected. Not verified on a real pre-upgrade draft.
- **Recovery journal version 3.** Written only for a review whose LEFT lines
  are numbered in a merge base other than `base.sha`. An older build cannot
  read it and fails closed: its decoder refuses any version but 1 and 2, so the
  pending attempt is not reconciled by that build (it reports the local record
  unavailable) and is not lost; the newer build reads it again.


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
  other-format, other-base or other-head answer; not found, server error and
  timeout leave no merge base; GHES asks under `/api/v3`; signed out it is
  admitted on github.com and refused on GHES before any request. The contract
  counts (`github_read_contract`, `github_anonymous_read_contract`,
  `native_https_anonymous_read_contract`) name the new read.
  `application_merge_base_public_path` also runs the race (GitHub records
  another base between the snapshot and the confirming read: Whole PR
  unavailable) and a refused credential on compare (the read fails).
- `BPR-1560:publication`: `github_mutation_contract` refuses a staged LEFT line
  on the recorded base when GitHub numbers LEFT in a distinct merge base and
  accepts it on the merge base; `sqlite_review_submission_recovery_storage`
  keeps a distinct comparison base across a restart (version 3, with and
  without line comments) and the submission identity across a merge-base move;
  `github_mutation_contract`, `live_review_submission_provider` and
  `staged_review_session` show a moved merge base refuses a LEFT comment, a
  review with one and a tour with a LEFT stop before any write, and RIGHT-only
  content asks nothing; live, existing LEFT comments on #76 are placed on the
  merge base's lines.
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
- `BPR-1560:round-three`: `github_provider` shows a compare that fails is
  `failed`, and a base branch naming another merge base or a pull request
  moving under the read is `refused`; `application_github_window_read_host`
  shows a merge base carried across a failed read, never across a refusal,
  and not brought back by a failed read after one; `staged_review_session`
  shows a resumed tour with a LEFT stop (after a denied first write, and after
  a Stop with two writes made) asks GitHub's merge base again, writes nothing
  when it moved and completes once it is back. Each was seen failing with its
  fix reverted.
- `BPR-1560:regeneration`: `merge_base_fixture_contract` rebuilds merge-base-v1
  byte for byte and checks the descriptor names its commits (Fable N7).
- `BPR-1560:gates`: one full `ctest --preset dev`, clang-format 22, tidy on
  changed files, the documentation gate and the composition contracts.

## Follow-ups

- An imported historical update's own merge base on a sparse review, from
  GitHub, would make "Show this update alone" available for it again.
- The unavailable Whole PR leaves the centre pane at "No file open"; the reason
  is in the Updates control and the status bar. [[BPR-0944]] AC4/AC5 own the
  pane's explanation.

## Coordination

[[BPR-1557]] draws the Updates line's base from the core's
`pull_request_base`, the function Whole PR takes its left side from, so its
anchor is the merge base GitHub named with no change on its side. A base change
now means a merge from the base, a rebase or a squash onto another base commit
(or this change's one-time upgrade epoch), not the base branch merely moving.
