---
schema: "implement-rfc.card.v1"
id: "BPR-2329"
title: "Auto-merge is shown on the Overview and can be enabled or disabled there"
status: "In Progress"
rfcs: []
depends_on: ["BPR-2327"]
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner request, 2026-11-20, while BPR-2327 was under way","reservation":"docs/issues/_reservations/BPR-2329--BPR-2329--7f57d17cb2c6.json","split_from":"BPR-2327","related_cards":["BPR-1852","BPR-2327"],"related_rfcs":["RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"]}
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

## Plan (2026-11-20, not started)

Work stopped before any code: the task's build disk reached its 5 GB floor
after BPR-2327 landed. The change mirrors Ready for review's path
([[BPR-1852]]) layer by layer:

- Read: `GetMergeAssessment` (`github_read_contract.cpp`) adds
  `viewerCanEnableAutoMerge viewerCanDisableAutoMerge` and
  `baseRepository{autoMergeAllowed mergeCommitAllowed squashMergeAllowed
  rebaseMergeAllowed}` inside `pullRequest` (the `repository` object is
  exact-key checked; `pullRequest` takes required keys only), parsed in
  `apply_merge_fields` into `github_merge_observation`, absent meaning
  unknown. BPR-2327 already reads the method and enabler.
- Core: an `auto_merge` header beside `draft_state.hpp`: offer, gesture
  decision, pre-write decision (already, not open, not permitted, method not
  allowed) and an overlay held until a read agrees.
- Port and host: `review_auto_merge_request`/`receipt` in
  `review_mutation_port.hpp`, `ChangesAutoMerge` capability, coordinator
  call, `review_mutation_application_host` record, queue, user-authority
  issuer and execution-authority check, as `change_draft_state`.
- GitHub mutation contract: `ObserveAutoMerge`,
  `enablePullRequestAutoMerge(input:{pullRequestId,mergeMethod})` and
  `disablePullRequestAutoMerge`, observe first, write once, read back a lost
  answer; GitHub's "clean status" refusal reads as "it can merge now".
- Diagnostics: an `operation_diagnosis_kind` for enable and disable through
  `review_mutation_diagnostics.cpp`, Standard detail, outcome only.
- Composition (`github_application.cpp`): offer, gesture, terminal and
  retirement as the draft state's; Qt: Merge's auto-merge line gains Enable
  (method menu) and Disable with a status line.
- Fixtures: the public-path fixture provider answers both mutations.

## Checkpoint (2026-11-20, round 2)

Work stopped at the coordinator's 3 GB free-disk line (2.4 GB free after the
Qt build) with every layer but composition written and built. Branch
`bpr-2329` (not on `main`) holds it as one WIP commit.

Done and passing in the task tree:

- Core `bpr/core/auto_merge.hpp`: offer, change decision (not open, not
  allowed, already, not permitted, method not allowed), overlay.
  `core_auto_merge` passes.
- Read: `GetMergeAssessment` also asks `viewerCanEnableAutoMerge
  viewerCanDisableAutoMerge baseRepository{autoMergeAllowed
  mergeCommitAllowed squashMergeAllowed rebaseMergeAllowed}` (inside
  `pullRequest`); parsed into `github_merge_observation::AutoMergeSettings`,
  absent meaning unknown. Field names checked live on BPR-Test.
- Port, coordinator, application host (`change_auto_merge`, one-shot
  `issue_auto_merge` grant, `permits_auto_merge`, terminal
  `publish_auto_merge`), capability `ChangesAutoMerge`,
  `pull_request_change::auto_merge`.
- GitHub mutation contract: `ObserveAutoMerge`, `EnableAutoMerge`
  (`mergeMethod`), `DisableAutoMerge`; observe first, write once, read back a
  lost answer; live provider forwards it. `github_mutation_contract` and
  `live_review_mutation_provider` pass (new
  `test_auto_merge_goes_by_graphql_exactly`,
  `test_auto_merge_is_observed_then_written_once`, including GitHub's live
  refusal body).
- Diagnostics: `change_draft_state`, `enable_auto_merge`,
  `disable_auto_merge` actions (review-mutation lines, outcome and flags
  only). **Found:** `with_review_mutation_diagnostics` never forwarded
  `change_draft_state`, so with a diagnostic log (production) Ready for
  review answered `unsupported`; the decorator now forwards both.
- Projection: `overview_auto_merge` (line, pending, core offer), context
  `ProjectedAutoMerge` and `AutoMergeControl`, `apply_auto_merge_state`.
  `application_pull_request_overview` passes (existing courses).
- Qt: Merge's auto-merge line, Enable auto-merge (menu of allowed methods),
  Disable auto-merge, status line; `present_auto_merge_state`,
  `workbench_window::publish_auto_merge`. `qt_pull_request_overview_page`
  passes (existing courses). `architecture_boundaries` passes.

Next action, in order:

1. Composition (`github_application.cpp`, `window_composition.hpp`,
   `window_construction.cpp`, `window_review_mutations.cpp`): mirror the
   draft-state flow (`change_draft_state`, `publish_draft_state`,
   `retire_draft_state_change`, `observe_draft_state_change`,
   `present_overview_draft_state`) with an `auto_merge_overlay`; set
   `AutoMergeControl` from `capabilities().ChangesAutoMerge`; wire
   `overview_page()->set_auto_merge_dispatch`; refusal words per
   `core::auto_merge_refusal` and `review_mutation_failure`.
2. New courses: projection (view on/off, offer, not allowed, overlay in
   place), Qt controls (menu methods, dispatch, busy, status), host
   (`review_mutation_application_host_tests`: grant exclusivity and
   terminal), and rerun `core_operation_diagnosis`, `github_paged_read`,
   `coding_format_check`.
3. Live: BPR-Test has `autoMergeAllowed: false` and its PRs are `CLEAN`;
   GitHub refuses enable there ("Auto merge is not allowed for this
   repository", checked live, nothing changed). A live enable-then-disable
   needs the owner to turn on "Allow auto-merge" and a required check on a
   BPR-Test branch (a settings change agents may not make).
