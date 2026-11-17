---
schema: "implement-rfc.card.v1"
id: "BPR-2247"
title: "What changed since my review says the reviewed code can't be read here"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","mandate_revision":"v0-5-0-prerelease-targets-r1","origin":"owner report, 2026-11-17","reservation":"docs/issues/_reservations/BPR-2247--BPR-2247--4e65a1072450.json","related":["BPR-2073","BPR-2167","BPR-2184","BPR-2246"],"acceptance":["BPR-2247:missing-earlier","BPR-2247:by-sha","BPR-2247:base-merges","BPR-2247:refusal"]}
---

# BPR-2247: What changed since my review says the reviewed code can't be read here

## Report

The owner, 2026-11-17: "I'm getting a lot of PRs I've reviewed before coming
back with new commits and getting "The code you reviewed can't be read here"
in my ask menu when I want to ask what changed. Seems like there are several
merge commits in the commits tab but there are a few real ones too and it
stinks that I can't do this"

Owner guidance, 2026-11-17: "We want if possible for things to just work."
Fetch what is missing and answer; refuse only when the reviewed code truly
cannot be got, and then say why.

## Scope

- What changed since my review is offered when the commit the reviewer
  reviewed is not among the review's own objects: it is fetched (by SHA when
  it is no longer within the head's history).
- Merges of the base branch since the review do not bring the base's changes
  into the change the agent is given.
- When the reviewed commit truly cannot be got, the refusal says so
  accurately.

## Acceptance

- `BPR-2247:missing-earlier`: a review whose objects do not hold the
  reviewed commit (a sparse review, which holds only its heads' commits and
  root trees) reads the change since the review.
- `BPR-2247:by-sha`: a reviewed commit outside the head's fetched history
  (beyond the depth, or force-pushed away) is fetched by its SHA.
- `BPR-2247:base-merges`: with merges of the base branch among the commits
  since the review, the change carries the real commits' own changes and
  none of the base's.
- `BPR-2247:refusal`: a reviewed commit GitHub no longer serves leaves the
  question unavailable with words that say the commit could not be fetched.

## Findings

- Root cause: most pull requests load sparsely
  (`object_loading_for` in `src/application/src/github_window_read_host.cpp`
  answers `selected_objects` whenever the shape is within the read limits),
  so the review's cache holds only its heads' commits and root trees. The
  window read the earlier revision through `resolve_changed_paths`
  (`src/application/src/epoch_host_review_reads.cpp`), which asks the cache
  alone: `inspect_commit_object(Left)` needs the commit with its closure and
  `find_blob_at_path` needs its subtrees, and it never fetches. The reviewed
  head was therefore unreadable on nearly every real pull request, and the
  window (`read_returning_material` in
  `src/app/src/github_application/window_returning_review.cpp`) marked the
  question `unavailable`: "The code you reviewed can't be read here". A
  force-pushed reviewed head was unreadable for every review.
- Merges of the base were a second, smaller fault: the change was the
  reviewed head to the head, restricted to the pull request's current files,
  so base changes to those files rode in it.

## Fix

- Core: `own_change_steps` (`agent_report.hpp`) is the reusable rule for
  any commit selection: every commit but a merge of the base
  (`merges_base`), as its first parent to it. `plan_returning_change`
  (`returning_review.hpp`) reads the change since a review as one
  comparison, or by commit when merges of the base are among the commits
  since; `bound_returning_commits` renders it under `Commit abc1234:` lines.
- Host: `resolve_returning_change` reads each step's changed files with both
  sides in one actor job. Commits the cache lacks are acquired through the
  review's own lease (`acquire_chain_trees`, to the generations that reach
  the reviewed head); blobs the cache lacks are read exactly, batched and
  checked.
- Transport: a chain request's `Outside` commit the depth clone did not
  bring is cloned by its SHA (one generation, trees only) into a second store
  the first borrows through alternates.
- The refusal, when a commit truly cannot be got, says the reviewed code
  could not be fetched from GitHub.

## Evidence

Lane commit `9cba49822`, 2026-11-17, macOS dev preset.

| Criterion | Check | Result |
|---|---|---|
| `BPR-2247:missing-earlier` | `application_epoch_host` (`sparse_returning_change_fetches_the_reviewed_head`, `returning_change_reads_each_step_from_the_cache`); `application_returning_review_latest_public_path` | Pass |
| `BPR-2247:by-sha` | `gh_cli_git_smart_transport` (the outside commit is fetched by its SHA and compared with the head in one store); live: on `MikeMarcin/BPR-Test` a branch commit force-pushed away was cloned by SHA with `gh repo clone -- --bare --revision=<sha> --depth=1 --filter=blob:none` (branch deleted afterwards) | Pass |
| `BPR-2247:base-merges` | `core_returning_review` (`own_changes_leave_out_merges_of_the_base`, `the_change_since_a_review_is_planned_around_base_merges`, `a_change_by_commit_keeps_its_commits_in_order`); host test diffs only the real commits | Pass |
| `BPR-2247:refusal` | `application_epoch_host` (`earlier_unavailable` on a refused fetch and with no lease); `application_returning_review_gone_public_path` ("could not be fetched from GitHub") | Pass |

Also passed: `core_agent_report`, `application_returning_review_addressed_public_path`,
`architecture_boundaries`; format check on every changed file; clang-tidy on
the changed translation units reports only findings on lines this card did not
touch.

## Remaining risks

- The overview's "changed since my last review" column still uses
  `resolve_changed_paths`, which has the same cache-only limit on sparse
  reviews.
- A merge of the base that resolved conflicts in the pull request's files
  carries that resolution only in the commit listing, not in the change.
- A rewritten (rebased) branch is compared whole, restricted to the pull
  request's files, so a base change to one of them can still appear.
