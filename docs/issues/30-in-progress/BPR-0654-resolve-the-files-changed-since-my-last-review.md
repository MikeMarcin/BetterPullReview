---
schema: "implement-rfc.card.v1"
id: "BPR-0654"
title: "Resolve the files changed since my last review for the overview"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
---

# Resolve the files changed since my last review for the overview

[BPR-0647](../60-done/BPR-0647-pull-request-overview-page.md) decides
the overview from the accepted snapshot, the private ledger and the retained
update rows. One column it cannot yet answer exactly is "changed since my
last review": the set of files that differ between the head I last reviewed
and the current head is resolved by no host today, so the overview carries
`overview_change_since_review::unknown` for every row while the head has
moved, the heading says the changes are not yet resolved, and Continue
review falls back to the first file not read.

## Scope

- Resolve the changed set in the host from material the epoch host already
  holds: the comparison between the last-reviewed update's head and the
  current head, resolved without disturbing the reviewer's selected
  comparison, or the per-epoch file records the catalog retains.
- Feed it to `overview_context::ChangedSinceReviewPaths`, which the
  projection builder and the page already honour: changed and new chips,
  the "Files changed" count, and Continue review landing on the first
  changed file.
- Owned paths: `src/app/src/github_application.cpp` (the join), any epoch
  host query it needs under `src/application`, and their focused tests.

## Acceptance

- With a last-reviewed head behind the current head, the overview marks
  exactly the files that differ between the two, counts them, and Continue
  review lands on the first of them.
- `application_pull_request_overview`, `qt_pull_request_overview_page` and
  the composition contracts pass with their digests repinned.
