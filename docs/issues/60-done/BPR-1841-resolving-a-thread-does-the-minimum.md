---
schema: "implement-rfc.card.v1"
id: "BPR-1841"
title: "Resolving a review thread asks GitHub only for the change and updates only that thread"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"kind":"bug","found_by":"Owner, 2026-11-03: \"Resolving a conversation thread needs to be much much faster; it seems like we refresh more than the absolute minimum necessary right now on that action.\"","related_cards":["BPR-0657","BPR-1741","BPR-1824","BPR-1826"],"id_allocation":"Provisional. scripts/allocate_issue_id.py refuses off main ('only the configured main integrator may reserve IDs'); the worktree branch could not reserve. The integrator confirms or renumbers BPR-1841 and commits the counter and receipt on main.","frontend_design":"No layout, palette, wording or control change; existing rows, bubbles and cards change state sooner"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"796a7e4dd","evidence":{"kind":"command","command":"ctest --preset dev -R '^application_threaded_reply_contract$'","detail":"Composed application over the public-path fixture, github.com and GHES 3.18. From the Resolve press until the reopen settles: 0 pull request reads (18 before), exactly 2 provider resolutions, 0 conversation-model resets, 2 changed rows (6 before), 0 Threads-view rebuilds, no refresh in flight at either answer, and the Overview's unresolved count fell by one and rose back in place. The contract requires THREAD_RESOLUTION_MINIMAL_OK and 'reads=0 resolutions=2 modelResets=0'."}},{"criterion":"AC2","outcome":"pass","revision":"796a7e4dd","evidence":{"kind":"command","command":"ctest --preset dev -R '^(qt_github_conversation_panel|qt_conversation_finder_scale_100)$'","detail":"The panel lays the core's projected resolution over its read: one dataChanged on the resolved thread's row, no reset, kept across a re-adopted read that has not caught up, and reversed by a reopen. The Threads view changes the row in place, Hide resolved drops and restores it, and nothing is rebuilt; a later rebuild lays the projection over the forest."}},{"criterion":"AC3","outcome":"pass","revision":"796a7e4dd","evidence":{"kind":"command","command":"ctest --preset dev -R '^(core_conversation_reactions|application_pull_request_overview)$'","detail":"core::project_thread_resolution_permission swaps GitHub's state-bound viewerCanResolve/viewerCanUnresolve when the projected state differs from the read, so Unresolve is offered on a thread just resolved without a re-read. application::apply_thread_resolution moves a file's unresolved count and mine exactly as project_pull_request_overview does with overview_context::ProjectedResolutions."}},{"criterion":"AC4","outcome":"pass","revision":"796a7e4dd","evidence":{"kind":"command","command":"ctest --preset dev -R '^(application_thread_resolution_refresh|application_closed_thread_resolution|application_description_edit_contract|application_comment_edit_contract|application_reviewer_request_contract|qt_conversation_cards_view|qt_composed_github_scale_100|qt_workbench_window_scale_100)$'","detail":"An admitted resolve across an accepted refresh still settles exactly once with one write; closed and merged pull requests still resolve and reopen; the courses that share the threaded course and the composed window suites pass."}},{"criterion":"AC5","outcome":"pass","revision":"796a7e4dd","evidence":{"kind":"command","command":"cmake --build --preset dev --target coding-format-check; _coding-standard/scripts/tidy.py --warnings-as-errors --paths <touched translation units>; python3 scripts/verify_documentation.py --root .","detail":"clang-format 22.1.8 clean; clang-tidy 22 reports no finding on any changed line (the remaining findings in workbench_window.cpp and github_application.cpp predate this card at 248607543); documentation gate passes."}}]
---

# BPR-1841: Resolving a review thread asks GitHub only for the change and updates only that thread

## Goal

The owner found resolving a conversation slow. A resolve should send the
targeted mutation and update only what depends on that thread: its card,
its row in the thread list and Threads view, its inline marker and the
counts. Unresolve takes the same path.

## Root causes

Traced end to end: card action (`workbench_window`), composition
(`resolve_review_thread`), core decision and overlay
(`decide_thread_resolution`, `begin_resolution_change`), mutation host,
provider (`review_mutation_session::resolve_thread`), terminal
(`publish_resolution`), and back to Qt.

1. **A full pull request refresh after every answer.** `publish_resolution`
   triggered the window's Refresh action on success. That re-read the whole
   pull request (9 paged reads per resolve in the fixture, more on a large
   pull request), then re-adopted every projection: pull request panel,
   conversation panel and cards, Threads view, defect surface, Overview and
   feedback counters. The resolve was not finished, as the reviewer saw it,
   until all of that was.
2. **Only the cards knew the answer.** The core overlay
   (`conversation_overlay`) already showed the asked state at once and kept a
   confirmed change until a read agreed, but only the cards drew it. The
   conversation model's row, the Threads view and the Overview counted from
   the read, so they changed only when the refresh arrived. Removing the
   refresh alone would have left them stale.
3. **State-bound permissions.** GitHub grants `viewerCanResolve` only on an
   open thread and `viewerCanUnresolve` only on a resolved one. Without a
   re-read, a thread just resolved still carried the read's "may not
   unresolve", so Unresolve conversation was disabled and the gesture
   refused.

## Design

The core decides, the host performs, Qt presents.

- **Core** (`conversation_reactions.hpp`):
  `project_thread_resolution_permission(observed, projected, permission)`
  swaps the read's permission when the projected state differs from it.
- **Application** (`pull_request_overview`): `overview_context` takes
  `ProjectedResolutions` (GitHub thread id to state) and counts them so;
  `apply_thread_resolution` moves one thread's file count and "my unresolved
  threads" in place, as projecting again would.
- **Composition** (`github_application.cpp`): `publish_resolution` no longer
  refreshes; GitHub's confirmed answer settles the overlay and the next read
  reconciles it, as reactions already did. The presentation and the gesture
  use the projected permission. `present_conversation_state` also moves the
  Overview counts in place (`present_overview_resolutions`), and a full
  Overview projection includes the overlay.
- **Qt**: `workbench_window::present_conversation_state` hands the projected
  resolutions to the conversation panel and the Threads view. The panel lays
  them over its read when it makes the shown forest, so the model's own
  incremental apply changes the one row; the Threads view changes its row
  and filters again without a rebuild. The Overview's file model already
  updates only changed rows.

Error and rollback behaviour is unchanged: a refused, failed or unconfirmed
answer abandons the overlay change and every surface shows the read again;
the provider still binds the thread to the pull request before writing and
confirms an ambiguous outcome with one read of the thread, never a second
write.

## Measurement

`--threaded-reply-probe --reaction-probe` over both fixtures, from the
Resolve press until the following reopen settled (two resolutions):

| | Before | After |
|---|---|---|
| Pull request reads | 18 | 0 |
| Provider resolutions | 2 | 2 |
| Conversation model resets | 0 | 0 |
| Conversation model rows changed | 6 | 2 |
| Threads view rebuilds | 0 | 0 |
| Card layouts | 16 | 5 |
| Refresh in flight at an answer | no | no |

The course prints `THREAD_RESOLUTION_COST:` and
`THREAD_RESOLUTION_MINIMAL_OK`; `application_threaded_reply_contract`
fails when a resolve or reopen reads the pull request, resets the model,
asks for other than the two resolutions, or leaves the Overview count
unmoved. One whole Overview projection may follow the read receipt that
opening the thread writes; no resolution causes one.

Per resolve on the wire, a live connection sends two GraphQL requests: the
thread's binding read (the designated-target check before any write, which
this card keeps) and the mutation.

## Not covered

- The Overview's feedback-label chips counted in Open mode still count from
  the read until the next one; they come from the label buckets the read
  fills, not from the thread facts.
- macOS offscreen only; Windows and Linux were not run. No live BPR-Test
  course was needed: the change removes a read and adds no wire request.
- No owner look was requested: no surface, wording or layout changed.
