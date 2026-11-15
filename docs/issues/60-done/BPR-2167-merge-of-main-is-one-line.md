---
schema: "implement-rfc.card.v1"
id: "BPR-2167"
title: "What changed is grouped by area, and a merge of main is one line"
status: "Done"
rfcs: ["RFC-BPR-0128"]
depends_on: ["BPR-2166"]
priority: "P2"
platforms: ["mac","linux","win"]
metadata: {"reservation":"docs/issues/_reservations/BPR-2163--BPR-2168--6bc64f9acd90.json","milestone_plan":{"id":"ask-v1","path":"docs/rfcs/plans/ask-v1/milestone-plan.json"},"criteria":["AC07"],"origin":"imported 2026-11-13 from the owner's Ask design (docs/design/ask-v1); cards cut at import"}
acceptance: [{"criterion":"AC07","outcome":"pass","revision":"38c727f03","evidence":{"kind":"command","detail":"core_agent_report, github_provider (test_merge_commits_count_what_they_brought); live compare check on BPR-Test 2026-11-14"}}]
---

# BPR-2167: What changed is grouped by area, and a merge of main is one line

Part of [RFC-BPR-0128](../../rfcs/RFC-BPR-0128-ask-quick-asks-three-scopes-reports-in-a-file-tab.md); milestone `reports-in-a-file-tab` of the [plan](../../rfcs/plans/ask-v1/MILESTONES.md).

## Scope

Core: commit listing folds a merge of the base branch (and a rebase onto it, Q11) into one line with its count and whether it changed this pull request's files; What changed groups by area. Applied wherever commits are listed for an agent answer or report.

## Acceptance

- **AC07:** Wherever commits are listed, a merge of the base branch is one line with its commit count and whether it changed this pull request's files, and the other commits are grouped by area.

## Progress (2026-11-14)

- Built (`d090ee1ae`, `5189a188c`): a merge of the base branch folds to one
  line saying whether it changed this pull request's files, the rest grouped
  by area, in What changed's request (`returning.commits`). The only other
  place commits are listed for an agent is none: Explain this file, Suggest
  the change and the review brief carry no commit list, so AC07's "wherever"
  is What changed.
- **Not built: the merge's commit count.** GitHub's pull request commit list
  holds the merge but not the base branch's commits it brought, and the
  review's cache holds only the pull request's own commits (a sparse review
  only its epochs'), so nothing local can count them. Plan: one GitHub
  compare read per merge (`compare/FIRST_PARENT...SECOND_PARENT`,
  `ahead_by`), bounded, through the GitHub read port, with a fixture whose
  pull request has a merge of main; `core::fold_commit_listing` already takes
  the counts (`aBrought`) and renders "Merged main (N commits)".

## Progress (2026-11-14): the merge's commit count

- Built: a merge in the pull request's commits carries what it brought in
  (`github_commit_record::Brought`). While reading the pull request, the
  GitHub provider makes one compare read per merge
  (`compare/FIRST_PARENT...SECOND_PARENT`), and `ahead_by` is the count. It
  asks for the newest eight merges a read and remembers each answer for the
  session, by host, profile, repository and the two parents. A compare that
  fails leaves that count and the rest unknown and the read whole. Lost
  authentication fails the read, as every optional read does. What changed's
  request passes the counts to `core::fold_commit_listing`, so a merge of
  main reads "Merged main (N commits)".
- Evidence: `github_provider` (`test_merge_commits_count_what_they_brought`:
  the committed github-com contract with a merge of main added; the merge
  asked once with its two parents, `ahead_by` 24 kept and not asked again on
  the next read, the head with one parent never asked, a failing compare
  leaving the count unknown); `core_agent_report` (the count rendered);
  `github_paged_read` and the returning-review courses unchanged.
- Live check on BPR-Test (2026-11-14, `X-RateLimit-Remaining` 4992 after):
  four unreferenced commits on the default branch's tip (two "main" commits,
  a feature commit and a merge of the second "main" commit into the feature).
  GitHub's compare of the merge's parents, feature...main, answered `ahead_by`
  2, `behind_by` 1, `base_commit` the first parent and the url ending in the
  second parent, which is the shape the adapter checks. No branch or pull
  request was made.
- [[BPR-2166]] is Done (2026-11-14), so this card is Done.
