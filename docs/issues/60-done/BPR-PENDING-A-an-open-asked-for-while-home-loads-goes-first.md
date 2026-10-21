---
schema: "implement-rfc.card.v1"
id: "BPR-PENDING-A"
title: "An open asked for while Home loads goes first"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"kind":"bug","found_by":"Owner chat, 2026-10-20","provisional_id":"BPR-PENDING-A: the integrator allocates the real ID on main at landing and renames every reference","related_cards":["BPR-0989","BPR-1357","BPR-1576"],"related_rfcs":["RFC-BPR-0026"]}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"7a96faade","evidence":{"kind":"command","detail":"Working tree over 7a96faade, committed as this card's slice. core_home_open's new test_home_loading_yields_to_a_window_read: no read and no window keep Home loading; an open in the second of two lanes gives way at once; loaded, a failed reread, one window's cancel beside another's read, and a sign-out that abandons the read each end or keep the wait as stated. Passes."}},{"criterion":"AC2","outcome":"pass","revision":"7a96faade","evidence":{"kind":"command","detail":"application_review_queue_enrollment's new test_discovery_gives_way_to_a_window_read_and_resumes and test_discovery_gives_way_across_a_session_change: both reads in flight cancelled in the same turn and once only, nothing submitted while yielding, the raced answer applied, the cancelled read resubmitted first under a new request identity, the refresh ends current with no failure or give-up counted; a refresh started while yielding waits and says it is loading; a stopped read of generation 6 is refused (2 stale, 0 resubmitted) and generation 7's reads go out on resume. Passes (57 s, with every existing case)."}},{"criterion":"AC3","outcome":"pass","revision":"7a96faade","evidence":{"kind":"command","detail":"application_home_open_feedback_public_path's new section: Home's searches held 10 s each on the fixture network, Home veiled as refreshing, a double-click on #4001 opens it in 61 ms (47 ms on a second run); both reads in flight give way in the gesture's turn, both are asked again, Home's refresh ends current with nothing failed and discovery quiescent. Negative control with the composition's yield_discovery disabled: the course fails at 'in the same turn, Home's two reads in flight give way', and with that check made a printout it fails at 'without waiting behind Home's searches' after the open took 19641 ms, two held searches back to back. Restored, it passes."}},{"criterion":"AC4","outcome":"pass","revision":"7a96faade","evidence":{"kind":"command","detail":"cmake --preset dev; cmake --build --preset dev (full). After rebasing onto 7a96faade and rebuilding, these pass: architecture_boundaries, core_home_open, application_home_open_host, application_review_queue_enrollment, github_provider, qt_composed_github_scale_100, qt_workbench_home_discovery_scale_100, application_workbench_composition_contract, application_profile_multi_window_public_path, application_personal_discovery_public_path, application_personal_discovery_cli_public_path, application_home_waiting_public_path, application_home_open_feedback_public_path, application_pending_states_public_path, application_refresh_feedback_public_path and test_network_isolation_manifest. Before the rebase these also passed: github_provider_content_policy, application_github_contract, application_home_waiting_cli_public_path and qt_workbench_open_review_from_home_scale_100. application_queue_composition_contract failed once in a four-way parallel run under load and passed in four other runs: once before the rebase in that parallel set, and three times alone after it. The github_application.cpp source pin and the production manifest pin are resealed (application_review_round_trip_composition_contract.cmake, command_correctness_registry.json); with them, application_composition_contract passes and the round-trip contract's pins verify. The round-trip contract's composed run was not repeated after the reseal, because the build tree was removed when the disk filled. The ID rename at landing leaves github_application.cpp and both pins untouched: that file carries no card tag. clang-format 22.1.8 over every touched C++ file; clang-tidy 22 with warnings as errors over every touched translation unit; python3 tests/scripts/verify_documentation_tests.py passes. python3 scripts/verify_documentation.py --root . reports only this card's provisional ID and main's own NEXT_ID high-water error (BPR-1678 below BPR-1679 on another branch)."}}]
---

# BPR-PENDING-A: An open asked for while Home loads goes first

## Owner report

Owner, chat, 2026-10-20, verbatim: "When I'm on the home and it's still
loading prs, if i double click to open a PR, even though its greyed out, it
should give that action priority. Right now it seems like it queues behind the
home load before it goes to open the PR"

## Cause

Every GitHub read of a profile runs on one provider lane: the composition hands
both providers one `worker_executor` (`ProviderEffects`), a single thread that
runs posted work first in, first out. Home's loading is the personal discovery
scheduler ([[BPR-0989]]): each refresh reads up to 30 retained pull requests
and then two searches per saved repository, two at a time, and each read is one
lane task holding several requests (with the GitHub CLI, an operation
credential, the viewer checks, the search and up to 30 detail reads). A
double-click on a greyed row reaches the core in the same turn ([[BPR-1357]]),
but its read was posted behind the read running and the one already queued, so
it started only after both had finished, and nothing told discovery to wait.

## Change

- Core: `core::home_loading_yields` (`home_open.hpp`) decides that Home's
  loading gives way while any window of the profile has a pull request read
  pending, an open and a reread alike.
- Application: `personal_discovery_host::yield` holds the scheduler. Giving way
  cancels the current refresh's reads in flight and submits nothing new; a read
  answered by that cancellation is asked again, ahead of the rest and under a
  new request identity, once discovery resumes; a read that answered first is
  applied. The refresh keeps running, so Home keeps saying it loads, and ends
  current rather than failed. Statistics count `Preempted` and `Resubmitted`.
- Adapter: a discovery read cancelled while it waited for its turn launches
  nothing, and one cancelled while its operation began reports cancellation
  rather than a failure.
- Composition: each window's read update asks the core in the gesture's turn
  and gives way at once; a profile presentation and a closed window ask again;
  resuming is posted to its own decision-lane turn and asks again there.

## Acceptance

- AC1: The core says Home gives way while any window's read is pending and
  resumes when every read has loaded, failed, been cancelled or abandoned.
- AC2: The scheduler gives way and resumes with request identity intact: the
  reads in flight are cancelled in the same turn, a cancelled read is asked
  again under a new identity, a raced answer is kept, a stopped read of an
  earlier generation is refused, and the refresh ends current.
- AC3: Through the composed application on a slow fixture network, a
  double-click on a row while Home's searches are held opens the pull request
  well inside one search's hold, and Home's refresh resumes and finishes with
  nothing failed. The course fails without the change.
- AC4: The focused suites, format, tidy and documentation checks pass.
