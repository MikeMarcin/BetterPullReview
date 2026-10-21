---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-B2"
title: "Closing the diagnostic export re-runs a Home refresh that just ended"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report, 2026-10-20: exporting a diagnostic log after Home finishes a refresh causes another Home refresh (GHE environment)","provisional_id":"Allocate on main at landing and rename this card and its [[BPR-PENDING-B2]] references","related_cards":["BPR-PENDING-B1","BPR-0990","BPR-1679"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"this card's commit on df2244530","evidence":{"kind":"command","detail":"application_review_queue_enrollment's new test_activation_after_a_refresh_that_ended_with_a_failure: a refresh with one unreadable repository ends stale; activation inside five minutes reads nothing; after five minutes it refreshes, retained metadata first; a refresh a network failure stopped is retried on the next activation. With activate() measuring from LastSuccess again the test fails on 'activation inside the freshness window of that ended refresh reads nothing'; with the fix it passes."}},{"criterion":"AC2","outcome":"pass","revision":"this card's commit on df2244530","evidence":{"kind":"command","detail":"application_personal_discovery_public_path's new export course (real composition, offscreen): the restarted session's first refresh ends stale with three refused repositories; the window's Export anonymized diagnostics action opens diagnosticExportDialog, which takes the window's activation; rejecting it and reactivating the window ran refresh 3 and four more reads before the fix (export refreshes=2:3 submitted=24:28, on 15bb7f8a4) and none after. Passes again rebased onto df2244530 with BPR-1679's yield."}},{"criterion":"AC3","outcome":"pass","revision":"this card's commit on df2244530","evidence":{"kind":"command","detail":"On df2244530: application_review_queue_enrollment, application_personal_discovery_public_path, application_personal_discovery_cli_public_path, application_home_waiting_public_path, application_home_waiting_cli_public_path, application_home_open_feedback_public_path and architecture_boundaries pass; format_check and clang-tidy (LLVM 22, warnings as errors) on the touched sources pass; the documentation gate passes with the provisional IDs replaced by allocated-form IDs."}}]
---

# Closing the diagnostic export re-runs a Home refresh that just ended

The owner reported that exporting a diagnostic log right after Home finished
a refresh started another refresh. On a GitHub Enterprise server, where one
refresh takes minutes, that is minutes of reads in the serialized `gh` lane.

## Cause

Nothing in the export reaches discovery. The export opens
`diagnosticExportDialog` and calls `activateWindow()` on it, so the workbench
window loses activation. When the dialog (and the save panel) closes, the
window gets it back. `workbench_window::changeEvent(ActivationChange)` then
sends `queue_discovery_intent::activate` while Home shows ([[BPR-0990]]).

`personal_discovery_host::activate()` refreshed whenever there was no
*successful* refresh in the last five minutes. A refresh that runs to its end
with any repository- or pull-request-scoped failure (a saved repository whose
search is refused, one retained pull request whose metadata read fails) is
not a success: `finish_cycle` leaves `LastSuccess` unset and the status
`stale`. So after such a refresh, **every** return to the window re-ran the
whole refresh: closing the export, Settings, Insights or a file dialog, or
switching back from another application. The failure that made it stale is
usually persistent, so the retry fails the same way and the cycle repeats on
the next activation.

The anonymized export cannot show which repository or pull request failed:
personal discovery records no diagnostic events (see [[BPR-PENDING-B1]]). The
mechanism was reproduced through the real composition (AC2). A refresh that
ends `current` or `partial` does not re-run on activation for five minutes, so
if the owner still sees the export start a refresh after a refresh that ended
without "Not refreshed", the remaining cause is the five-minute window itself.

## Change

`personal_discovery_host` keeps `LastEnded`, the time the last refresh ran to
its end, and `activate()` measures freshness from it instead of from
`LastSuccess`. A refresh that only failed for a repository or pull request
counts as ended. One a session-wide failure stopped (offline, authentication,
rate limit, process start; `CycleStopped`) does not, so activation still
retries it once any retry-after has passed. The status, its problem and
`LastSuccess` are unchanged, and explicit Refresh, reconcile and adoption
behave as before. This does not touch the `gh` lane or request priority.
[[BPR-1679]]'s `yield` composes with it: a read cancelled to give way is asked
again and does not fail the refresh, so it never leaves the refresh stale.

`github_personal_discovery_fixture::OtherRepositoriesRefused` lets the
public-path course refuse the searches of every saved repository but the
fixture's own as unreadable.

## Acceptance

- AC1: After a refresh that ended with a repository- or pull-request-scoped
  failure, activation inside the freshness window reads nothing; after it,
  activation refreshes; a refresh a session-wide failure stopped is retried
  on the next activation. A host test fails on the old rule.
- AC2: Through the composed application, closing the diagnostic export's
  dialog after such a refresh starts no refresh. The course fails before the
  fix.
- AC3: Focused discovery suites, format, tidy and documentation checks pass.

## Files

- `src/application/src/personal_discovery_host.cpp`
- `src/application/include/bpr/application/personal_discovery_host.hpp`
- `docs/architecture/review-queue.md`
- `src/app/src/github_application_fixture_access.hpp`,
  `src/app/src/github_public_path_fixture.cpp` (fixture knob)
- `tests/application/review_queue_enrollment_tests.cpp`
- `tests/end_to_end/personal_discovery_public_path_tests.cpp`
