---
schema: "implement-rfc.card.v1"
id: "BPR-2327"
title: "The Overview's conflict status names the base, the files when known, how to resolve, and auto-merge"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"ux","target":"0.5.0","origin":"owner request, 2026-11-20 (Overview Status drawer compared with GitHub's merge box)","reservation":"docs/issues/_reservations/BPR-2327--BPR-2328--f3b2d54d1f10.json","related_cards":["BPR-1405","BPR-1589","BPR-2314","BPR-2328"],"related_rfcs":["RFC-BPR-0085"],"acceptance":["AC1","AC2","AC3","AC4","AC5","AC6","AC7"]}
---

# BPR-2327: The Overview's conflict status names the base, the files when known, how to resolve, and auto-merge

## Asked

The owner, 2026-11-20, comparing GitHub's merge box with the pull request
Overview: "can we improve our status for conflicts? GitHub is much more
useful here."

GitHub's merge box says the branch has conflicts that must be resolved, lists
the conflicting files, links to its web editor and to command-line
instructions, and says when auto-merge is enabled. The Overview's Status says
"Conflicts with <base>" under Needs attention, then a red "Conflicts" heading
in Merge's details whose only line is an unrelated required-check sentence (a
merge-queue check that has not reported), cut off at the drawer's edge. It
lists no files, says nothing of auto-merge and offers no way to act.

## Acceptance

- AC1 Headline: when GitHub assesses the pull request as conflicting, Merge's
  heading names the conflict and the base ("Conflicts with <base>"), and a
  sentence under it says GitHub cannot merge until they are resolved.
- AC2 Files: when a conflicting-path observation is supplied, each path is a
  row that is itself a button (pointer, Space, Return) opening that file in
  the review. When none is supplied, the section says that GitHub does not
  report which files conflict and that Resolve on GitHub lists them. GitHub's
  API exposes no conflicting-file list (GraphQL `PullRequest` has `mergeable`
  and `mergeStateStatus` only; REST has `mergeable` and `mergeable_state`), and
  the review's Git cache holds no blobs for sparse reviews, so no producer
  exists yet; [[BPR-2328]] owns computing it locally.
- AC3 Actions: Resolve on GitHub opens the pull request's conflicts page
  (`…/pull/<n>/conflicts`) through the window's browser port, its address
  built from the accepted profile host and snapshot only. A copyable
  command-line hint names the head and base branches, and a Copy button puts
  it on the clipboard.
- AC4 Auto-merge and merge queue: when GitHub reports auto-merge enabled, Merge
  says so on its own line, apart from the conflict. (Viewing auto-merge off
  and controlling it was split to [[BPR-2329]] on the owner's later request.) When the pull request is
  in a merge queue, Merge says so with its position and state. The merge
  assessment read asks for `isInMergeQueue` and `mergeQueueEntry{state
  position}` in the same `GetMergeAssessment` query: no new request. A read
  without them leaves the queue unknown and shows nothing.
- AC5 Heading reflects the blockers: Merge's heading is decided from every
  blocker the core sees (conflicts, behind, draft, changes requested, review
  required, required checks failed or not reported). A required check that has
  not reported is never labelled Conflicts: alone, the heading reads "Waiting
  for 1 required check"; with a conflict, the heading reads "Conflicts with
  <base>, and 1 more requirement", and the check is listed under "Also
  required".
- AC6 Long reasons wrap: a condition naming a long check (no spaces, only
  slashes and hyphens) wraps at the drawer's width; no line is cut off.
- AC7 Architecture: the blocker order and headline are decided by a pure core
  function, the projection maps them to words, the GitHub adapter reads the
  new fields, and Qt draws them and emits intents only (open a file, open the
  conflicts page, copy text).

## Not in scope

- Computing conflicting files from the local cache ([[BPR-2328]]).
- Enabling or disabling auto-merge, or enqueueing, from the Overview.

## Change

- Core: `core::order_merge_blockers` and `core::decide_merge_headline`
  (`src/core/include/bpr/core/merge_blockers.hpp`). The most serious kind
  leads, blockers of that kind are counted together, the rest are "more
  requirements".
- Application: `project_pull_request_overview` maps the decision to Merge's
  heading, fills `MergeConflict` (base, conflicting paths from
  `overview_context::ConflictingPaths`, the command hint) and `MergeNotes`
  (auto-merge, merge queue); `apply_draft_state` moves them in place.
  `github_window_read_host::open_pull_request_conflicts_page` opens
  `…/pull/<n>/conflicts` from the accepted profile host and snapshot.
- GitHub adapter: `GetMergeAssessment` asks
  `autoMergeRequest{enabledAt mergeMethod enabledBy{login}} isInMergeQueue
  mergeQueueEntry{state position}` in its one query. Home's shorter
  `autoMergeRequest{enabledAt}` and an answer without the queue fields still
  parse; an unknown enum is refused.
- Qt: Merge's details show the conflict's sentence, file rows
  (`drawer_link_row`, open the file), the not-reported line, Resolve on
  GitHub, Copy commands and the commands, Also required, and the notes.
  Condition text gets zero-width break opportunities after `/ - _ . :`.
- RFC-BPR-0085 owner amendment (2026-11-20) records it.
- Conflicting files: GitHub exposes no list (GraphQL `mergeable` and
  `mergeStateStatus`; REST `mergeable`, `mergeable_state`). A local
  `git merge-tree` needs the base tip's and head's blobs, which a sparse
  review's cache does not hold (BPR-1873), and a new effect lane; no producer
  was added and BPR-2328 owns it. Today every conflict shows the
  not-reported line and Resolve on GitHub.
- Design: `frontend-design:frontend-design` was loaded. The Merge details
  stay in the drawer's existing type and inks: the heading keeps its
  demi-bold defect tone, the commands use the system fixed font (they are
  code), file rows wear the link ink only because they are links (BPR-2314
  AC5).

## Evidence

Candidate: `9d0dc3f1c` and `89c6caa75` on `main`, built in a task worktree with its own `build/dev` tree (dev preset,
jobserver).

- `core_merge_blockers`: pass (new).
- `application_pull_request_overview`: pass, with the new
  `test_merge_names_the_conflict_and_what_else_blocks` (conflict without and
  with file data, waiting check alone is not a conflict, auto-merge and queue
  notes, Home's short read, draft in place).
- `qt_pull_request_overview_page`: pass, with the new
  `test_merge_shows_the_conflict_and_how_to_resolve_it` (heading, sentence,
  not-reported line, Also required, auto-merge line, Resolve on GitHub
  dispatch, Copy commands clipboard, file rows by pointer and Return, long
  reason wraps, no conflict). Negative control: with the break opportunities
  removed, it fails at "the long check name has places to break".
- `github_paged_read`: pass, with the new
  `test_merge_assessment_reads_auto_merge_and_the_queue` (query fields,
  details, short answer, refused unknown queue state).
- `github_read_contract`, `gh_cli_transport`,
  `github_anonymous_read_contract`, `github_provider`,
  `github_provider_content_policy`, `native_https_anonymous_read_contract`:
  pass. `native_https_anonymous_read_fixture` fails with
  `unavailable-executable` (its helper executable was not built in this
  partial tree); not run.
- `architecture_boundaries`, `architecture_boundary_contract`,
  `coding_format_check`: pass.
- `window_construction.cpp` (composition) and
  `overview_summary_public_path_tests.cpp` compile; the end-to-end course
  `overview_summary_public_path` was not linked or run (free disk was about
  6 GB; the app library does not fit), its heading expectation was updated to
  "1 required check failed, and 2 more requirements".
- Not done: no live GitHub check (nothing here writes, and the read adds
  fields to an existing query); no attended look.
