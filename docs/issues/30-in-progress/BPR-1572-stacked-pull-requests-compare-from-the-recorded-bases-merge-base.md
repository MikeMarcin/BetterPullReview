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
4. The pull request's metadata again: head, `base.sha`, base branch, state,
   changed-file count and, since review round one, the reported commit count
   must still be the snapshot's.

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
  base is asked as before. When it is not the comparison base, the same rule
  as the read path: `observe_recorded_merge_base` (compare `base.sha`...head,
  whose `ahead_by` must equal the `commits` the pull request reported) and
  `observe_base_ancestry` (compare `base.sha`...T, T the exact tip the
  base-branch answer resolved, compiled from `merge_base_ancestry_binding`)
  must name the comparison base and `base.sha`; then the pull request is read
  again and must be unchanged (identity, `base.sha`, base branch, head, state
  and commit count), or the write is `target_moved`
  (`left_lines_numbered_in_comparison_base`). Without this, every LEFT
  publication on a stacked pull request would have been refused. Review round
  one (Astra P1-1, P1-2): the first version asked the ancestry of the branch by
  name, did not read the pull request again after the answers, and ignored
  the commit count; a recorded base GitHub replaced with a descendant tip
  during those reads would have authorised stale LEFT coordinates.
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
  passing. Full `ctest --preset dev -j 6` on `df7c47f82`
  (main merged at `0c6d520ad`): 762/770 under load average 80-156 with other
  agents' builds; the eight failures, rerun alone, all pass:
  `documentation_structure`, `documentation_issue_graph_contract` and
  `issue_id_allocation_contract` (the worktree lacks the ignored `.agents`
  skills link the board model is imported from; passed with it linked),
  `coding_format_check` (`git_cache_contract_tests.cpp` lines this card added,
  reformatted with clang-format 22), `qt_composed_long_thread_scale_100` and
  `_200`, `release_update_native_https`, `application_line_draft_contract`
  (load).

## Review round one (Fable approve, Astra reject; on `d8d1f5671`)

- **P1-1** (publication reconfirmation and tip binding): fixed as above.
  Tests: `live_review_mutation_provider`
  (`a_left_comment_on_a_stacked_pull_request`: forward sent; rewritten,
  commit count differing and recorded base reset to the tip between the
  answers and the second pull request read: `target_moved`, nothing sent),
  `live_review_submission_provider` (the same four courses for a review),
  `staged_review_session` (a tour, and a resumed tour after a denied first
  write and after a Stop, with the reset), `github_mutation_contract` (an
  ancestry answer about the branch by name is refused; requests are exact
  commits). Negative controls: with the second pull request read ignored, the
  single-comment reset course sends its comment and the resumed-tour reset
  writes; with the commit count ignored, `count_differs` passes the rule.
- **P1-2** (publication commit count): the target observation reads
  `commits`, the compare answer `ahead_by` (unsigned; a negative or
  non-numeric count is malformed), and the rule requires them equal.
- **P2** (read confirmation): the confirming read compares the reported commit
  count; `github_provider` course `stacked_commits_drop` (2 to 1) is refused,
  and passes as available with the check removed.
- **Test notes.** The live course no longer sleeps: it waits only for the
  step's head, starts the app at once, and records GitHub's `pulls/N` and file
  list every second while it runs. A cold-start app reads the pull request
  tens of seconds in, past GitHub's 1-6 s reset window, so the window is
  covered deterministically: `application_merge_base_public_path` stacked
  reset course (every compare agrees with the recorded base, GitHub replaces
  it with the branch tip before the confirming read: Whole PR says GitHub's
  answers disagree, nothing compared) and the session tests above. The
  negative mode passes only when the app compared and differed, or said
  Whole PR has no base ("…where this pull request branched…").
- **Fable.** Docs rewritten (`git-and-epoch-model.md`); the stale "target is
  the last observation" comment corrected; negative `ahead_by` refused (both
  parsers, tested); measurement output committed
  (`docs/issues/_checkpoints/BPR-1572/live-2026-10-17.jsonl`). A LEFT publish
  from the app itself needs the owner's attended designated-live approval
  (`--attended-designated-live-approval`), so instead `--left-wire` performs
  live the exact reads the publication makes and posts a LEFT comment through
  `gh` on `x.txt` line 3, a line only the recorded base's comparison has:
  #102 (A merged b1) and #103 (A fast-forwarded to b2) both satisfy the rule
  and GitHub accepted the LEFT line each time (comment deleted).
- **Criss-cross** (Astra): measured live on #106 (#101 was the probe's first
  run, whose record compared only file counts and was superseded). Git names two merge
  bases X1 and Y1 whose comparisons with the head change different lines of
  `c.txt`; Git's recursive virtual base changes neither. compare named X1, and
  GitHub's Files changed, patches included, was exactly the comparison from
  X1; the app matched it line for line. Documented in the architecture doc:
  the review follows the one merge base compare names.

## Gates, review round one

On `1745f83bf` (main merged at `79a4652ad`): clang-format 22 on every changed
C++ file; clang-tidy on the changed translation units passes, and the
findings it reported in this round's new test code were fixed (the rest are on
lines this card does not touch); `DOCUMENTATION_OK`; round-trip composition
contract resealed (full application digest `16c45b7b…`, production manifest
`450c878e…`) and passing. One full `ctest --preset dev -j 6`: 762/772 under
load average up to 47 with other agents' builds; the ten failures rerun alone
all pass: `qt_workbench_window_scale_200`, `application_composition_contract`,
`application_offline_inventory` and `application_terminal_diagnostics`
(timeouts under load), the five `application_large_review_*` timing budgets
(`large_file_selection` 13 s under load; `application_large_review_launch`
once more failed alone with "fixture exit code 1" and passed twice after),
and `rfc0016_production_identity_security` (failed while the ignored
`.agents` skills link the documentation rows need was present; passes in the
clean worktree).

## Review round two (Fable approve, Astra reject; on `6bee33df2`)

- **P1** (tours read after the confirmation): `verify_staged` now does every
  other read (review list, review, its comments, description) first and
  observes the pull request (and, for a LEFT stop, the merge base and the pull
  request again) last, so the pull request is the last network read before
  every tour write, first and resumed. Audited the other paths: a single
  comment (legacy route and the source-backed child, `execute_publication_child`)
  and a review observe the target immediately before the POST.
  `staged_review_session`: GitHub replacing the recorded base during the
  baseline review-list read of a first publication, and during the review or
  review-list read of a resumed tour after a denied first write and after a
  Stop, writes nothing. `github_review_comment_permalink`
  (`test_left_source_backed_child_on_a_stacked_pull_request`): a LEFT comment
  through the source-backed child is sent after two complete observations;
  a reset at the admission's second read, the child's first read or the
  child's last read sends nothing.
- **P2** (the agreeing path skipped the final confirmation): the pull request
  is read again after an agreeing branch answer too. `live_review_mutation_provider`
  and `live_review_submission_provider`: a head that moved during that compare
  is `stale_head`, a 500 on the second read is `unavailable`, nothing sent.
- **Inherent limit**, now stated in the architecture doc: a read and the POST
  are not atomic, and GitHub's review and review-comment requests carry
  `commit_id` and coordinates but no base precondition, so a recorded base
  replaced after the last read cannot be ruled out; the publication is then
  reconciled from what GitHub stored.
- **Fable.** The second read is classified as the first: a transport failure
  or non-200 as the first read is (`unavailable`), a malformed answer through
  `map_contract_failure` (submission: `undesignated_target`), a moved head
  `stale_head`, and only an answer that is the same pull request with
  another recorded base (`target_recorded_base_moved`) `target_moved`. The
  live course's rule check now says it relies on B forking at a1. Criss-cross
  cites #106 only.
- **Negative controls.** With the old tour order (observe first), the resumed
  tour with a reset during the review reads writes; with the agreeing path
  returning before the second read, the single-comment and review agree
  courses send; with the child's second read ignored, the source-backed reset
  course sends.

## Review round three (Fable approve, Astra reject; on `d4ee19341`)

- **P1** (the GitHub CLI account wrapper reads after the final confirmation):
  `cli_observed_account_transport::perform_review_mutation` checks the
  account (a `/user` read) before and after every exchange, so a write went
  out after the session's last pull-request read, its trailing account check
  and the write's leading one. The wrapper now reads the pull request once
  more after its pre-write account check, for every witnessed write bound to
  a pull request (`written_target`: a review comment or reply, a review, a
  tour write), and refuses the write, not sent, when the head or recorded base
  moved; the account binding is unchanged (checks before and after). The
  sequence for a write is: account, pull request, POST, account. Audited the
  other transports: the native HTTPS transport (github.com and GHES) adds no
  read before a write; the GitHub CLI transport's operation scope reads its
  credential locally; a signed-out session cannot write; no retry or
  rate-limit wrapper re-reads before a POST. `github_provider`
  (`test_cli_wrapper_reads_the_pull_request_last_before_a_write`, through the
  production wrapper): unchanged, the exact read order; GitHub replacing the
  recorded base during the session read's trailing account check or the
  write's leading one, the head moving, or both, sends nothing. With the
  wrapper's read removed, the test fails.
- **Fable.** The stale comment in `observe_exact` is corrected. Both the
  recorded base and the head moving on the second read is `target_moved` (the
  pull request moved; a moved head alone is `stale_head`): new courses in
  `live_review_mutation_provider` and `live_review_submission_provider`, and
  in the wrapper test. Deliberate: a tour whose plan has any LEFT stop asks
  the merge-base reads before every write of the tour, its RIGHT stops'
  writes included: the stops form one pending review, so its LEFT
  coordinates are carried by every later write that edits it and become
  public only with the final submit, which each write leads up to.

## Review round four (Fable approve, Astra approve with conditions; on `1206adfca`)

- **Astra P2** (callers lost the wrapper's refusal): a whole review refused
  unsent by a transport now reports the transport's `target_moved` or
  `stale_head` (`review_submission_session`), definitely not published; a
  tour write refused unsent for those reasons is a `conflict` carrying the
  failure (or `description_attention` once published), not a ready tour that
  invites a Resume (`staged_review_session`); a source-backed comment's child
  never sent returns the transport's failure with no attempt
  (`execute_publication_child`), and the legacy route reports no attempt
  likewise. Tests from the session level through the production CLI account
  wrapper (`make_cli_observed_account_transport_for_testing`, which builds the
  wrapper exactly as an account lease does, over
  `tests/github_contract/cli_account_wrapper_support.hpp`): a review, a tour
  and a LEFT source-backed comment, each with the recorded base replaced (and
  for the review, the head moved) before the wrapper's last read, are refused
  unsent with the reason. With the propagation reverted all three fail.
- **Fable.** Wrapper-path courses for a 500 on the wrapper's pull-request read
  (`unavailable`, definitely not sent, no write) and for a cancellation inside
  it (`cancelled`, definitely not sent, no write); the wrapper's comment now
  says it compares the head and recorded base only and does not see a base
  branch rewritten after the session's compares before GitHub replaces the
  recorded base; the architecture doc says "every witnessed write bound to a
  pull request".

## Gates, review round three

On `3b79ec5a1` (main already merged): clang-format 22 on the changed C++
files; clang-tidy reports nothing in the changed regions;
`DOCUMENTATION_OK`; no `src/app` change, the composition contracts unchanged.
One full `ctest --preset dev -j 6`: 764/772. Rerun one at a time,
`rfc0016_production_identity_security`, `live_mutation_isolation` (a timeout),
`release_update_native_https`, `native_git_smart_transport_fixture` and
`application_terminal_diagnostics` pass. `application_large_review_responsiveness`,
`_pathological` and `_history` failed again alone, each on a timing budget
("responsiveness budget exceeded", "pathological fallback completion/input
budget exceeded", "epoch refresh exceeded its measured platform budget") with
the machine's load average at 56-66 from other agents; they are fixture read
courses over the fixture transports, which the CLI account wrapper changed
here is not installed on, and they passed alone in rounds one and two. Still
to rerun on a quiet machine. Main then moved with code (`83ed4d8fd`, merged in
`21ff72ccc`): the round-trip composition contract is resealed (full
application digest `ee1795ef…`, production manifest `bfb21e55…`), all eleven
composition contracts and the thirteen BPR-1572 rows pass on the merged tree.

## Gates, review round two

On `0bdb5673b` merged with main (`9117cdf60`, then the docs-only `77e67000e`):
clang-format 22 on the changed C++ files; clang-tidy reports nothing on lines
changed this round; `DOCUMENTATION_OK`; the round-trip composition contract
passes unchanged (main did not move `github_application.cpp` or the manifest).
One full `ctest --preset dev -j 6` under load average 85-131 from other
agents: 750/772. The 22 failures rerun one at a time all pass: the three
documentation rows (no `.agents` skills link in the worktree during the run;
`documentation_structure` also needed main's newer `NEXT_ID`, merged in
`77e67000e`), and timeouts or timing budgets under load
(`qt_composed_long_thread_scale_125`/`_200`, `qt_github_conversation_model_property`,
`qt_diff_workbench_scale_200`, the four `application_large_review_*`,
`large_review_replay_contract` (timed out once more alone, then passed),
`application_setup_destination_public_path`,
`application_personal_discovery_cli_public_path`, the three
`application_overview_summary_*`, `application_epoch_ordinary_update_recreated`,
`application_terminal_diagnostics`, `application_private_review_native`/`_file`,
`application_source_pr_search_performance`).

## Live round two (`ba735233a`, records in `live-2026-10-17.jsonl`)

| Course | Steps | Result |
|---|---|---|
| merge #102 | open; A merges b1 (+ LEFT wire); A merges all (merged) | PASS 3/3, wire rule holds, LEFT accepted |
| fast-forward #103 | open; A to b1; A to b2 (+ LEFT wire); push b4 to B | PASS 4/4, wire rule holds, LEFT accepted |
| rebase #104 | open; A rebased (base.sha reset to the new tip within 0.9 s of the push); B rebased | PASS 3/3 |
| force #105 | open; A advances; A force-pushed back to b1 (base.sha reset to b1) | PASS 3/3 |
| criss-cross #106 | open | PASS, GitHub = comparison from compare's merge base |
| negative control #107 (stacked acceptance reverted) | open; A merges b1 | step 1 agrees; step 2 shows "GitHub's answers … disagree", no comparison: reproduced |

## Follow-ups

- **Endpoint lag.** For up to about 5 s after a push, GitHub's files endpoint
  can answer a newer state than `pulls/N` (#91, #92). The snapshot's
  file-count, commit-count and last-commit checks and the confirming read
  catch it when the counts differ. Predates this card; unchanged.
- **A base force-pushed onto a descendant of `base.sha`** (#93, #105): before
  GitHub resets `base.sha` the provider accepts the recorded base's merge
  base, which is what GitHub's Files changed still showed in that window
  (#93); a reset between the answers and the confirming read is refused (the
  stacked reset fixture course); the next read follows the reset.
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
- `BPR-1572:live`: `bpr1572_stacked_live_course.py`, merge, fast-forward,
  rebase and force variants with the LEFT wire course, and
  `bpr1572_criss_cross_probe.py`, every step PASS (round two).
- `BPR-1572:negative-control`: the unit, fixture and live controls above.
- `BPR-1572:gates`: one full `ctest --preset dev`, clang-format 22, tidy on the
  changed sources, the documentation gate, the composition contracts.

