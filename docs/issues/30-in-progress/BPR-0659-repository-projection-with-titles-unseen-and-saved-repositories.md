---
schema: "implement-rfc.card.v1"
id: "BPR-0659"
title: "Repository projection with titles, unseen counts and saved repositories"
status: "In Progress"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
---

# Repository projection with titles, unseen counts and saved repositories

The repositories drawer [BPR-0651](../60-done/BPR-0651-repositories-and-files-drawers.md)
built shows each repository the queue names with its pull requests of
interest, the one-word reason and the amber "needs me" state, from
`queue_landing_projection.Items`. Three things the design draws have no
projection: the pull request's title (a `core::queue_item` carries only its
ref, membership and bucket), the unread dot (`queue_activity_observation.UnseenComments`
is consumed by `bucket_for` and never retained), and the repositories the
profile's accounts have saved (no saved-repository record exists anywhere;
enrollment is per pull request and rule-derived, and Settings › Reviews &
queue under BPR-0652 needs the same record).

## Scope

- A durable saved-repository record per account (host, account, owner/name)
  with add/remove intents through the queue enrollment store, published with
  the queue landing projection so the tree lists a saved repository with no
  pull requests of interest as an empty row.
- The queue host retains, per item, the title, the author, the updated time
  and the unseen comment count it already observes, and publishes them on
  the item; the tree shows the title after the number and the unread dot.
- Owned paths: `src/core` queue item, `src/application` queue enrollment and
  landing projection, `src/adapters/sqlite` enrollment store, `src/adapters/qt`
  repositories tree, focused tests and the queue contracts.

## Acceptance

- The tree reads "#n title" with the unread dot for items with unseen
  comments, and lists every saved repository; adding and removing a saved
  repository from Settings and from the tree round-trips through the store.
- `core_review_queue`, `application_review_queue_enrollment`,
  `sqlite_queue_enrollment_storage`, `qt_repositories_drawer` and the queue
  composition contract pass.
