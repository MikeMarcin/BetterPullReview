---
schema: "implement-rfc.card.v1"
id: "BPR-2327"
title: "The Overview's conflict status names the base, the files when known, how to resolve, and auto-merge"
status: "In Progress"
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
  says so on its own line, apart from the conflict. When the pull request is
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
