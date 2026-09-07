---
schema: "implement-rfc.card.v1"
id: "BPR-0659"
title: "Repository projection with titles, unseen counts and saved repositories"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P1"
acceptance: [{"criterion":"The tree reads \"#n title\" with the unread dot for items with unseen comments, and lists every saved repository; adding and removing a saved repository from Settings and from the tree round-trips through the store","outcome":"pass","revision":"0a7fbf42 + this finish","evidence":{"kind":"observation","detail":"qt_repositories_drawer pins \"#14366 Preserve the DRCS font\", title_role, unseen_role and the tooltip count, a saved repository as an empty row reading saved, the saved mark on one with items, and Save/Forget on the current repository row leaving as add/remove intents; qt_workbench_window_scale_{100,125,200} pin the Settings editor on Reviews & queue off Home and out of the focus order, Save gated on the session's account and an owner/name, Remove on the selected saved repository, both leaving as intents, and the tree listing the projection's saved repositories; application_review_queue_enrollment pins that a save is durable before it is projected, that a refused write leaves the list, and no-op saves; sqlite_queue_enrollment_storage pins the round trip by host, account and name through the database and the application port, the version-three migration and canonical refusal of a tampered saved_repository table; the composition wires the window's dispatch to the queue host and the queue composition contract passes."}},{"criterion":"core_review_queue, application_review_queue_enrollment, sqlite_queue_enrollment_storage, qt_repositories_drawer and the queue composition contract pass","outcome":"pass","revision":"0a7fbf42 + this finish","evidence":{"kind":"observation","detail":"All named selectors pass at this head with the composition contracts (both application digests repinned), the UIforETW courses, check_boundaries, strict tidy on the touched translation units, clang-format --Werror and verify_documentation.py."}}]
---

# Repository projection with titles, unseen counts and saved repositories

The repositories drawer [BPR-0651](BPR-0651-repositories-and-files-drawers.md)
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

## Notes

- Slice (a), 2026-09-07, 0a7fbf42: `core::queue_item_facts` (title, author,
  unseen count) travels on every `queue_item`; `pull_request_observation`
  carries the title the composition read from the accepted snapshot; the
  queue host keeps the title and author with each activity report and
  projects them with the observation's unseen count. The repositories
  tree reads "#n title" (the view elides), carries `title_role` and
  `unseen_role`, and the delegate draws the accent dot before a pull
  request with unseen comments; the tooltip names title, author and count.
- Slice (b), 2026-09-07: `application::saved_repository` (host, account,
  "owner/name"), `saved_repository_intent` (add, remove) and `valid`;
  `durable_queue_state.SavedRepositories`; the store port's
  `apply_saved_repository_change`; queue schema version 4 with the
  `saved_repository` table (host, account, repository; names only), a
  3-to-4 migration, canonical validation of version 4 and legacy
  validation of version 3; the host's `apply(saved_repository_intent)`
  stores before it keeps and publishes `SavedRepositories` on the landing
  projection. The tree lists a saved repository with nothing of interest
  as an empty row reading "saved", marks a saved one that has items, and
  offers Save and Forget on the current repository row as its context
  menu; Settings > Reviews & queue has the saved-repositories editor (the
  list, an owner/name field, Save and Remove) for the account the queue
  session names. The composition carries both to the queue host.
- The queue's unseen count is what is newer than what I read: the ledger's
  read state (BPR-0656) for conversations that have a receipt. A
  conversation never read has every post unread in the cards but no cursor
  for a comment to be newer than, so the first open of a pull request is
  still quiet and notifies nothing (the queue composition course's claim);
  the count is taken when a pull request opens and again after each Mark
  read, and reaches the queue through the retained observation.
- Not carried: an updated time (the read model has none) and the Home
  action list's rows (BPR-0650). The unread dot's inset is 12 pixels.
- Verification at the finish: `core_review_queue`,
  `core_review_queue_attention`, `core_review_ledger`,
  `application_review_queue_enrollment`, `application_review_queue_notification`,
  `application_review_ledger_host`, `application_agent_invocation_host`,
  `sqlite_queue_enrollment_storage` (version-three migration, tampering
  with the new table, saved-repository round trip through the database
  and the port), `sqlite_review_ledger_storage`, `qt_repositories_drawer`,
  `qt_workbench_window_scale_{100,125,200}`, `qt_composed_github_scale_100`,
  `qt_review_landing_model`, `qt_conversation_cards_view`,
  `application_queue_composition_contract`,
  `application_composition_contract`,
  `application_review_round_trip_composition_contract` (digests repinned),
  `application_review_ledger_composition_contract`, the UIforETW course
  and selected-diff contract, `check_boundaries`, strict tidy on every
  touched translation unit, `clang-format --Werror` and
  `verify_documentation.py` pass.
