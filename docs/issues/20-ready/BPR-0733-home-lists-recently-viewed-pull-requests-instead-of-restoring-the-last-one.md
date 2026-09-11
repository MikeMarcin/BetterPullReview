---
schema: "implement-rfc.card.v1"
id: "BPR-0733"
title: "Home lists recently viewed pull requests instead of restoring the last one"
status: "Ready"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11"}
---

# Home lists recently viewed pull requests instead of restoring the last one

The owner, 2026-09-11, on what is "very messy" about Home
([BPR-0716](../10-backlog/BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md)):
"App seems to automatically, but very slowly, auto-load the last PR on app
restart which is disorienting; a recently viewed list or something would
be better."

## What is established

- Each window persists one restoration row
  (`restoration_records(window_id, mode, hostname, owner, repository,
  number, position)`, `src/adapters/sqlite/src/window_review_restoration_store.cpp:22-24`)
  through `application::window_review_restoration_host`. At startup
  `select_startup` (`src/app/src/github_application.cpp:3794-3831`)
  restores the device profile or activates public preview, waits for an
  accepted session (`observe_restoration_session`, `:3840-3854`), and only
  then `restore_review` (`:5474-5497`) says "Restoring the previous
  review…" and dispatches `open_pull_request_intent`, a network snapshot
  read; with a saved position `advance_reading_restoration`
  (`:5364-5395`) then waits on the catalogue, the comparison, the file
  list and the diff. Home stays up until `loading_pull_request`
  (`workbench_window.cpp:3955-3956`), so the whole session bootstrap runs
  behind a Home that looks idle, and then the review replaces it without
  a gesture. That is the "very slowly" and the "disorienting". Any Open
  bar submit or GitHub intent cancels the restoration (`:5651, :8864`).
- Nothing persists a history of opened pull requests; the one row per
  window is all there is. RFC-BPR-0026 "Home and attention" asked for a
  prominent Resume action bound to the exact retained destination, not an
  automatic open; RFC-BPR-0028's Home paragraph (an action list, open by
  URL in the bar) supersedes the presentation and does not mention
  restoration.

## Scope

- The app no longer opens the last pull request by itself at startup.
  Home is shown and stays until a gesture. The saved row becomes the
  first entry of a recently viewed list.
- A per-profile recently viewed list (the pull request, its title where
  a snapshot was seen, the host, when it was last viewed, and the saved
  reading position where one exists), bounded (ten is enough), most
  recent first, persisted through the same restoration store (a schema
  step from the one-row table) and owned by the core's restoration
  policy: the core decides what is recorded and evicted, the host writes,
  Qt presents. Signed out, it lists the public-preview entries only;
  entries of an account are listed once that account's session exists.
- On Home, between the paste field and the queue: a "Recently viewed"
  list whose rows open the pull request on Overview (as a queue row does,
  `open_pull_request_item`, `workbench_window.cpp:5040-5058`) and, when a
  saved position exists, offer to resume it there. Empty, the section is
  not shown. The list is keyboard reachable and does not depend on colour.
- Per-window drawer layouts (BPR-0708) and the saved reading position are
  untouched; only the automatic open goes. "Resume review" of RFC-BPR-0026
  AC-01 is satisfied by the first entry's resume action; the RFC row on
  [BPR-0599](../10-backlog/BPR-0599-close-rfc-0026.md) records that.
- Owned paths: `src/core` restoration policy, `src/application`
  window-review restoration host, `src/adapters/sqlite/src/window_review_restoration_store.cpp`,
  `src/adapters/qt/src/workbench_window.cpp` and a small recently-viewed
  widget beside the landing, `src/app/src/github_application.cpp`
  startup wiring, their tests, RFC-BPR-0028's Home paragraph.

## Acceptance

- The sqlite store test migrates a one-row schema to the list schema,
  keeps the old row as the newest entry, and refuses a malformed or
  future schema before writing.
- `profile_window_review_restoration_public_path` (or its successor)
  starts with a saved review, asserts Home stays with no
  `loading_pull_request` and no snapshot request until a row is chosen,
  and that choosing the row opens it and resumes the saved position.
- `qt_composed_github_scale_{100,125,200}` pin the list's rows, order,
  keyboard reach and the empty case.
- The owner restarts the signed bundle with a review saved, sees Home at
  once with the entry, opens it by hand; accepted in an attended session.
