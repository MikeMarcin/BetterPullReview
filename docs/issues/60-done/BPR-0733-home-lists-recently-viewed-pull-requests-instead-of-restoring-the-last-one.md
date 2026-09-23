---
schema: "implement-rfc.card.v1"
id: "BPR-0733"
title: "Home lists recently viewed pull requests instead of restoring the last one"
status: "Done"
rfcs: ["RFC-BPR-0028", "RFC-BPR-0026"]
depends_on: []
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11"}
acceptance: [{"criterion":"AC1 sqlite store migrates the one-row schema to the list schema, keeps the old row as the newest entry, refuses malformed or future schema before writing","outcome":"pass","revision":"e7c8a81cf110fa50b5ee6748ff9f784a3cd7fb83+working","evidence":{"kind":"command","detail":"ctest -R sqlite_window_review_restoration_storage: Passed. recent_reviews table at user_version 3; v1/v2 one-row files migrate with the old row as ordinal 0 (title '', viewed_at 0, its position); 11 rows, duplicate destination, 513-byte title, user_version 4 refused with file bytes unchanged."}},{"criterion":"AC2 profile_window_review_restoration_public_path starts with a saved review, Home stays with no loading_pull_request and no snapshot request until a row is chosen, choosing the row opens it and resumes the saved position","outcome":"pass","revision":"e7c8a81cf110fa50b5ee6748ff9f784a3cd7fb83+working","evidence":{"kind":"command","detail":"ctest -R application_profile_window_review_restoration_public_path: Passed (571 s). home_lists_recent checkpoint (Home, status not loading, no accepted snapshot, AccountReadCount and PublicPreviewRequests unchanged over 400 ms, rows most-recent-first) before every restart gesture; resume_recent restores the exact position in auth-advance/auth-restore/reading races; unavailable-* still report an explicit failure; anonymous-restore opens the public row by Return."}},{"criterion":"AC3 qt_composed_github_scale_{100,125,200} pin the list's rows, order, keyboard reach and the empty case","outcome":"pass","revision":"e7c8a81cf110fa50b5ee6748ff9f784a3cd7fb83+working","evidence":{"kind":"command","detail":"ctest -R qt_composed_github_scale_(100|125|200): 3/3 Passed. test_home_lists_recently_viewed_pull_requests pins row texts and order, focus_order stops for the resume button and list, Resume enabled only on a resumable row, Return/activation and Resume dispatch {row,resume}, empty projection hides the section and drops its stops, signed-out arrival still focuses the bar."}},{"criterion":"AC4 the owner restarts the signed bundle with a review saved, sees Home at once with the entry, opens it by hand; accepted in an attended session","outcome":"pass","revision":"2cdc0e57","evidence":{"kind":"observation","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0733-recently-viewed, revision app-release-64d9e46a-2026-09-11.3 (bundle built at 2cdc0e57, executable sha256 64d9e46a0fa7cff7e37d778d329a8e71bcf6ff13547bbc82eadb547ebb252827), fingerprint 39fa48b4b48f69ab068ef95c77383319b1f2dbd4136e8f41f6b20fbf8a774502, decision 76d1917a-f3d7-44c3-b96e-6679191a9ace, actor human, 2026-09-11T23:48:33Z: accept. Relaunch with a review saved showed Home at once with the entry under Recently viewed; opened by hand; accepted without a note, including the d13 change shown for the verdict that a refused startup authority is now silent while a refused destination or store still speaks."}}]
---

# Home lists recently viewed pull requests instead of restoring the last one

The owner, 2026-09-11, on what is "very messy" about Home
([BPR-0716](BPR-0716-read-without-signing-in-is-offered-from-every-signed-out-home.md)):
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
  [BPR-0599](BPR-0599-close-rfc-0026.md) records that.
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

## Attended candidate, 2026-09-11 (v007)

The pending criterion above is candidate `BPR-0733-recently-viewed` in
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md)
on the bundle built from the source at 2cdc0e57 (executable sha256
`64d9e46a…`), candidate revision `app-release-64d9e46a-2026-09-11.3`;
prepared and protected at sequence 311, undecided, no host running until
the owner asks for the session. The ask: relaunch with a review saved shows Home at once with the entry, opened by hand, Resume reading returns to the position; the third criterion is the d13 change shown for the owner, that a refused startup authority is silent while a refused destination or store still speaks.

## Attended verdict, 2026-09-11 (v007): accepted

Session `cbd48b91…`, revision `app-release-64d9e46a-2026-09-11.3` (bundle built at 2cdc0e57, executable sha256 `64d9e46a…`), actor human; the session was handed back at sequence 322 with all ten candidates decided. Candidate `BPR-0733-recently-viewed`, decision `76d1917a`, 23:48:33Z: **accept**.
Accepted without a note, including the judgement made while landing the card (a refused startup authority is silent; a refused destination or store still speaks). Done.
