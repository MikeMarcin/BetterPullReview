---
schema: "implement-rfc.card.v1"
id: "BPR-2329"
title: "Auto-merge is shown on the Overview and can be enabled or disabled there"
status: "Done"
rfcs: []
depends_on: ["BPR-2327"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner request, 2026-11-20, while BPR-2327 was under way","reservation":"docs/issues/_reservations/BPR-2329--BPR-2329--7f57d17cb2c6.json","split_from":"BPR-2327","related_cards":["BPR-1852","BPR-1828","BPR-2327"],"fixes_shipped_bug":"BPR-1852 draft-state changes refused as unsupported by the diagnostics wrapper","related_rfcs":["RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"]}
---

# BPR-2329: Auto-merge is shown on the Overview and can be enabled or disabled there

## Asked

The owner, 2026-11-20, after asking for a better conflict status
([[BPR-2327]]): "It also doesn't look like we can view or control
auto-merge." Split from BPR-2327, which shows auto-merge's line when it is
on; this card adds the control.

## Acceptance

- AC1 View: Merge says whether auto-merge is on (with the merge method and who
  enabled it, when GitHub says) or off, whenever the repository allows
  auto-merge. A repository that does not allow it shows no auto-merge line and
  no control.
- AC2 Offer: Enable auto-merge is offered only when the repository allows
  auto-merge, GitHub says the viewer can enable it, the pull request is open
  and auto-merge is off. Its menu lists only the merge methods the repository
  allows. Disable auto-merge is offered only when auto-merge is on and GitHub
  says the viewer can disable it. The core decides both.
- AC3 Mutation: each is an explicit gesture, written once through the
  application's review-mutation host under a one-shot user grant bound to
  the accepted pull request and the acting account, as Ready for review is
  ([[BPR-1852]]). GitHub is asked first; nothing is sent when auto-merge is
  already in the state asked, the pull request is closed, or the viewer may
  not change it. A lost answer is read back, never sent twice.
- AC4 Refusals: each refusal says why in words (not permitted, not allowed
  for this repository, method not allowed, closed, already on or off, GitHub
  refused, connection cannot), and nothing changes.
- AC5 Reflect: GitHub's answer moves Merge in place at once and is held until
  a read agrees.
- AC6 Log: one Standard diagnostic event per enable and per disable with its
  outcome and no content (no names, branches, logins or messages).
- AC7 Evidence: fixture courses for view, enable, disable and a refusal at
  the core, GitHub contract, application host and Qt layers. Any live check
  only on MikeMarcin/BPR-Test, disabling anything it enables.

## Change

- Core `bpr/core/auto_merge.hpp`: `auto_merge_offered`,
  `decide_auto_merge_change` (not open, not allowed, already, not
  permitted, method not allowed), `auto_merge_overlay` and
  `project_auto_merge`.
- Read: the merge assessment's one query also asks
  `viewerCanEnableAutoMerge viewerCanDisableAutoMerge
  baseRepository{autoMergeAllowed mergeCommitAllowed squashMergeAllowed
  rebaseMergeAllowed}`; absent fields leave the settings unknown.
- Application: `review_auto_merge_request`/`receipt`, capability
  `ChangesAutoMerge`, `pull_request_change::auto_merge`, coordinator and
  host `change_auto_merge` under a one-shot `issue_auto_merge` grant that
  permits only that exact change, terminal `publish_auto_merge`.
  Projection `overview_auto_merge` (line, pending, core offer) with
  `overview_context::ProjectedAutoMerge`/`AutoMergeControl` and
  `apply_auto_merge_state`.
- GitHub adapter: `ObserveAutoMerge`, `EnableAutoMerge` (`mergeMethod`),
  `DisableAutoMerge`; observed first, written once, a lost answer read back
  once; the live provider reports the capability.
- Qt: Merge's auto-merge line, Enable auto-merge (a menu of the allowed
  methods in GitHub's words), Disable auto-merge, and a status line that
  holds the buttons while GitHub answers.
- Composition (`github_application.cpp`): gesture, terminal, read
  reconciliation and retirement, as the draft state's; refusals in words.
- Log: `enable_auto_merge` and `disable_auto_merge` review-mutation lines
  (Standard detail; outcome, written and read-back flags only).

## Shipped bug fixed here

`with_review_mutation_diagnostics` ([[BPR-1828]]) wrapped the live provider
but never forwarded `change_draft_state`, so with the diagnostic log in place
(every production window) Ready for review and Convert to draft
([[BPR-1852]]) answered `unsupported` and the window said "The connection
could not change the draft state (unsupported)". The wrapper now forwards it
and logs a `change_draft_state` line, and forwards `change_auto_merge`.

## Evidence

Candidate: the BPR-2329 commits on `main` (see the landing commits named in
the closing commit), built in a task worktree with its own `build/dev`.

- `core_auto_merge` (new), `core_operation_diagnosis`, `core_merge_blockers`:
  pass.
- `application_pull_request_overview`: pass, new
  `test_auto_merge_is_shown_and_offered` (off and on lines, offer only with
  the connection's capability and the allowed methods, not allowed shows
  nothing, unknown settings keep BPR-2327's note, the viewer's change in
  place equals the projection).
- `application_review_mutation_application_host`: pass, new
  `test_an_auto_merge_change_needs_the_capability_and_a_permitting_grant`,
  and the closed-binding course covers `auto_merge`.
- `qt_pull_request_overview_page`: pass, new `test_merge_offers_auto_merge`
  (menu methods, enable and disable dispatch, busy holds the buttons, a
  refusal is said, in-place update, another pull request refused).
- `github_mutation_contract` (new
  `test_auto_merge_goes_by_graphql_exactly`), `live_review_mutation_provider`
  (new `test_auto_merge_is_observed_then_written_once`, including GitHub's
  refusal bodies), `github_paged_read`: pass.
- Composition translation units compile (`github_application.cpp`,
  `window_read_state.cpp`, `window_review_mutations.cpp`,
  `window_construction.cpp`); the application was not linked or launched.
- `architecture_boundaries`, `coding_format_check`: pass.

Live, MikeMarcin/BPR-Test (owner approved and the coordinator turned on
"Allow auto-merge"; left on). `X-RateLimit-Remaining` stayed above 4,960.

- With the setting off, `enablePullRequestAutoMerge` answered
  `{"data":{"enablePullRequestAutoMerge":null},"errors":[{"type":"UNPROCESSABLE",…,"message":"Auto merge is not allowed for this repository"}]}`.
- With it on: every open pull request is `CLEAN` and reports
  `viewerCanEnableAutoMerge: false`. A disposable pull request (#186, a
  pending commit status on its head so it was not clean) read through the
  product's `ObserveAutoMerge` document as `UNSTABLE`,
  `viewerCanEnableAutoMerge: false`, `autoMergeAllowed: true`, all three
  methods allowed; the product's `EnableAutoMerge` document was refused
  with "Pull request is in unstable status". Nothing was enabled, so nothing
  needed disabling; #186 is closed and its branch deleted.
- So the Overview correctly offers no Enable on BPR-Test today. A live
  enable-then-disable needs a pull request GitHub reports `BLOCKED`, which on
  BPR-Test needs a branch rule with a required check or review: a further
  settings change for the owner to approve. The success path is covered by
  the fixture courses above.
