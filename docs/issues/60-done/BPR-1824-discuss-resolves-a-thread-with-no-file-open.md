---
schema: "implement-rfc.card.v1"
id: "BPR-1824"
title: "Discuss resolves a thread with no file open, and says why when it cannot"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1824--BPR-1824--8eb153c2e234.json","kind":"defect","found_by":"owner report, 2026-10-29: Resolve in Discuss said resolving was unavailable; the same thread resolved at once on GitHub's page","related_cards":["BPR-0657","BPR-1741","BPR-1745","BPR-1746"],"frontend_design":"frontend-design:frontend-design loaded before the refusal and failure words; no layout or colour change"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "4acc4f253", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_review_round_trip$'", "detail": "core::decide_thread_resolution names a closed or merged pull request, a missing write connection and a conversation that is no review thread, in that order after sign-in; a thread another account wrote resolves and unresolves when nothing refuses; the earlier four refusals keep their order."}}, {"criterion": "AC2", "outcome": "pass", "revision": "4acc4f253", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_conversation_cards_view$'", "detail": "The thread menu's disabled Resolve carries the core's reason as its tooltip for sign-in, closed, no connection and server capability, never 'not available'; GitHub's viewerCanResolve / viewerCanUnresolve false says it takes write access; a pull request comment says only review threads resolve; with nothing refusing, another account's thread offers Resolve and a resolved one Unresolve."}}, {"criterion": "AC3", "outcome": "pass", "revision": "4acc4f253", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_threaded_reply_contract$'", "detail": "Through the composed application and the fixture server (github.com and GHES 3.18): the course now clears the diff selection before Resolve in Discuss, so no file is open, and the thread resolves through the port, reopens, and the reaction, description, comment-edit and reviewer phases that follow still pass. With the old grant gate restored the same course fails at Resolve (phase 9, resolution refused before GitHub was asked)."}}, {"criterion": "AC4", "outcome": "pass", "revision": "4acc4f253", "evidence": {"kind": "command", "command": "python3 -I course.py (outside the repository) against MikeMarcin/BPR-Test", "detail": "Live, 2026-10-29, temporary PR #142, closed and its branch deleted afterwards. MikeMarcin (admin) read viewerCanResolve true on both its own thread and MikeMarcinTest's; resolved and unresolved MikeMarcinTest's thread with the application's own ResolveThread / UnresolveThread documents; did the same after the pull request was closed. MikeMarcinTest (read only) read viewerCanResolve false on both threads, its own included, and GitHub answered FORBIDDEN. The live capability probe documents on github.com still parse to supported (ResolveReviewThreadInput gained an optional resolutionReason, which the probe accepts). X-RateLimit-Remaining stayed above 4980."}}]
---

# BPR-1824: Discuss resolves a thread with no file open, and says why when it cannot

## Report

The owner pressed Resolve on a review thread in Discuss. The application
said it could not: resolving was unavailable. On GitHub's page the same
thread resolved at once. The repository and server were the owner's own.

## Root cause

Every gesture went through `review_mutation_gesture_grant`, which issued a
grant only when a diff was selected (`SelectedDiffPublication`), or for a
reply bound to its thread ([[BPR-1745]]). A resolution needs no diff. From
Discuss with no file open in Review, Resolve was offered, the core allowed it,
and the grant came back empty. The mutation host then refused before asking
GitHub, with `review_mutation_failure::unavailable`, and the panel said
"The thread could not be resolved: unavailable." Reactions, description and
comment edits and reviewer requests had the same gate.

The fixture course passed because it replied on a line first, so a diff was
always selected when it pressed Resolve.

## Every condition that refused or hid Resolve

- **Grant** (`github_application.cpp`): the selected-diff gate above. Fixed:
  only an inline comment publication needs the selected diff; every other
  gesture binds to the accepted pull request (`LastAcceptedHandoff`), whose
  account and pull request must match the accepted read as before.
- **Composition**: the write connection (`compose_review_mutation`) is built
  only for an open pull request at the handed-off base and head, signed in.
  Without it the menu said "Resolution is not available for this thread." and
  the open card hid the button. It now names the reason: sign in, closed or
  merged, or not connected yet (refresh).
- **Server capability** (`github_capability_probe.cpp`): `viewerCanResolve`,
  `viewerCanUnresolve` and both mutations. Checked against today's github.com
  schema; supported. A server that does not claim them is named: use the pull
  request's page on GitHub.
- **Thread permission** (`viewerCanResolve` / `viewerCanUnresolve`, read from
  GraphQL, `denied` only on an explicit false). Correct. The menu used to
  offer Resolve anyway and refuse after; it now disables it with GitHub's
  reason (write access), the same as the open card hiding it.
- **Kind**: a pull request comment or review summary is no review thread. A
  signed-out REST read yields no review threads, so Resolve is not offered;
  sign-in is named first.
- **Thread author, outdated, stale snapshot**: none refuses. GitHub allows
  resolving another account's thread and an outdated one; the live course
  confirmed the first.
- **Host failures after GitHub is asked** each say what happened and what to
  do (`thread_resolution_failure_text`), instead of the failure's enum name.

## Built

- **Core** (`review_round_trip.hpp`): `thread_resolution_refusal` gains
  `pull_request_closed`, `no_write_connection` and `not_a_review_thread`;
  `thread_resolution_input` gains `ReviewThread`, `PullRequestOpen` and
  `WriteConnection`. No selected-diff input exists.
- **Qt**: `thread_resolution_refusal_text`; `conversation_cards_view` holds
  the pull request's refusal (`set_resolution_refusal`) and its menu asks the
  core per card; `workbench_window::set_thread_resolution_connection`
  combines the composition's reason with the server's capability.
- **Composition**: `thread_resolution_connection_refusal`, presented before a
  write connection is composed and replaced when it is; the Resolve gesture
  passes the same facts to the core and words its refusal from it.

## Design

Loaded `frontend-design:frontend-design` before the words. No layout,
palette or control changes. Each refusal says what stops it and what to do,
in the existing sentence-case voice; "unavailable" alone is gone.

## Not reproduced or not verified

- The owner's exact server was not known. On github.com every probe fact is
  supported today; a GitHub Enterprise Server whose schema lacks the fields
  now says so by name, but no GHES instance was reached live (the GHES 3.18
  fixture passes).
- Resolving on a closed or merged pull request still does not go: GitHub
  allows it (checked live), but the write connection is bound to an open pull
  request (`review_mutation_target_binding::Open`), which every publication
  relies on. The menu now says so. Allowing thread-only changes on a closed
  pull request is a separate decision for the owner.
- macOS offscreen only; Windows and Linux were not run. No owner look was
  requested: the change is wording and a gate, with no visual change.
