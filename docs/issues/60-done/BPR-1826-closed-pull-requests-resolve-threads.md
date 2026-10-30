---
schema: "implement-rfc.card.v1"
id: "BPR-1826"
title: "Resolve and unresolve review threads on closed and merged pull requests"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1826--BPR-1826--ff96df541c32.json","kind":"feature","found_by":"BPR-1824 follow-up, owner approved 2026-10-29: GitHub resolves threads on a closed pull request; the application refused","related_cards":["BPR-0657","BPR-0879","BPR-1741","BPR-1824"],"frontend_design":"No layout, palette or control change; one refusal sentence removed"}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^core_review_round_trip$'", "detail": "core::pull_request_change_permitted: an open pull request takes every change; a closed or merged one takes only a thread's resolution (publication, reaction, description and comment edits and reviewer requests stay open-only). decide_thread_resolution no longer has a pull_request_closed refusal or a pull request state input: a closed pull request's thread resolves and unresolves, and without its connection it says no_write_connection."}}, {"criterion": "AC2", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_review_mutation_application_host$'", "detail": "application::valid stays open-only; valid_identity checks the binding whatever its state; valid_for(binding, change) asks the core. A host composed with a closed binding offers only ResolvesThreads (capabilities_for), resolves and unresolves through the provider bound to the closed target, and refuses a reaction, description edit and comment edit with a terminal and no effect."}}, {"criterion": "AC3", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^github_mutation_contract$'", "detail": "A closed binding compiles and revalidates resolve, unresolve, the thread read and the pull request read; a comment, reaction, description edit, comment edit, reviewer request and the publication's comment-collection and merge-base reads do not compile. The pull request read must say the state the binding was read in: a closed binding refuses a reopened pull request, an open binding a closed one, and an unknown state is refused."}}, {"criterion": "AC4", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^qt_conversation_cards_view$'", "detail": "No resolution refusal's words say the pull request is closed or send the reviewer to GitHub's page for it; the remaining refusals keep BPR-1824's tooltips."}}, {"criterion": "AC5", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "ctest --preset dev -R '^application_closed_thread_resolution$'", "detail": "Composed application over the public-path fixture, github.com and GHES 3.18, with the pull request read closed and then merged: a write connection is composed whose binding is not open, it resolves threads and offers no reaction, description or comment edit or reviewer request though the provider serves them; Edit is not offered on a comment; Resolve conversation, then Unresolve conversation, on the thread's open card in Discuss reach the port and the thread comes back resolved, then open; two resolutions and nothing else reach the provider."}}, {"criterion": "AC6", "outcome": "pass", "revision": "915a05b80", "evidence": {"kind": "command", "command": "python3 -I course.py (outside the repository) against MikeMarcin/BPR-Test", "detail": "Live, 2026-10-29, temporary PR #143 as MikeMarcin: a review thread was written, the pull request closed (REST state closed, merged false; GraphQL CLOSED), and the application's own ObserveThread, ResolveThread and UnresolveThread documents resolved and reopened the thread; viewerCanResolve was true. The branch was deleted and the pull request left closed. X-RateLimit-Remaining stayed at 4989 or above."}}]
---

# BPR-1826: Resolve and unresolve review threads on closed and merged pull requests

## Goal

The owner approved letting a thread be resolved or unresolved on a closed or
merged pull request, as GitHub allows ([[BPR-1824]] checked it live). The
application refused: the write connection was composed only for an open pull
request, and every publication path relied on that.

## Design

The core decides, the host performs, Qt presents.

- **Core** (`review_round_trip.hpp`): `pull_request_change` names each change
  the application writes (thread resolution, publication, reaction,
  description edit, comment edit, reviewer request), and
  `pull_request_change_permitted(change, open)` says an open pull request
  takes all of them and a closed or merged one only a thread's resolution.
  `thread_resolution_refusal::pull_request_closed` and
  `thread_resolution_input::PullRequestOpen` are gone.
- **Application** (`review_mutation_port.hpp`, `review_mutation_contract.cpp`):
  the target binding already carried `Open`. `valid(binding)` stays the
  open-only contract every publication path checks. `valid_identity` checks
  the identity alone; `valid_for(binding, change)` adds the core's rule;
  `capabilities_for` masks a provider's capabilities to what the binding
  takes. The mutation host's grant binding accepts a closed binding; its
  thread grant, `issue_thread` and `resolve` ask `valid_for(thread_resolution)`.
  Every other host path, grant and issuer still asks `valid`, so a reaction,
  edit, reviewer request or publication on a closed binding is refused
  before any effect.
- **GitHub adapter**: the live connection's configuration is admitted and
  performs on `valid_identity`; each request's compiler stays the gate.
  Only the thread resolution, its thread read and the pull request read
  compile for a closed binding, and the outbound check recompiles each
  request, so nothing else reaches the wire. The pull request read
  (`parse_target_observation`) now requires the state the binding was read
  in instead of `open`: a reopened pull request is refused under a closed
  binding, and an open binding still refuses a pull request closed since.
  Repairs of pending publications keep the strict configuration check, so
  none runs on a closed pull request.
- **Composition** (`github_application.cpp`): `compose_review_mutation`
  selects a closed or merged pull request too
  (`select_observed_review_mutation(..., true)`), with the binding's `Open`
  from the accepted read. For a closed one it tells the window the provider
  resolves threads, presents the reviewer, description and comment-edit
  offers (all off, from the masked capabilities) and stops: no shared drafts,
  no editor target. The account and pull request checks are unchanged, and
  review submission, source observation and off-hunk facts still ask for an
  open pull request. `thread_resolution_connection_refusal` no longer names a
  closed pull request.
- **Qt**: the "This pull request is closed…" refusal is removed; no other
  wording changed.

No change was made to comments, replies, reviews, edits, reactions or
reviewer requests: GitHub allows some of them on a closed pull request, but
none was obviously right to open here.

## Fixture

`github_public_path_fixture_options::PullRequestState` reads the fixture's
pull request closed or merged (REST `state` "closed", with `merged`), and
drops it from the read's open-pull-request search, as GitHub's search would
(the read refuses a search result whose state disagrees with the pull
request's). The fixture's mutation wire answers the pull request read with
the binding's own state. `thread_resolution_evidence` reports the composed connection's binding
and capabilities and the resolution gestures' counts.

## Verification

Focused, macOS Debug (offscreen, built `-g0` for disk): `core_review_round_trip`,
`application_review_mutation_application_host`, `application_review_mutation`,
`qt_conversation_cards_view`, `github_mutation_contract`, `gh_cli_transport`,
`native_https_transport_contract`, `native_https_transport_fixture`,
`live_review_mutation_provider` and its two recovery courses,
`application_closed_thread_resolution`, `application_thread_resolution_refresh`,
`application_threaded_reply_contract`, `application_description_edit_contract`,
`application_comment_edit_contract`, `application_reviewer_request_contract`,
`application_review_round_trip_composition_contract` (resealed) and
`test_network_isolation_structure_contract` pass; `coding-format-check` passes.
With the composition's closed-pull-request selection reverted, the new course
fails at "a write connection is composed for the closed pull request".

`test_network_isolation_manifest` fails on this base for two registrations
this card does not touch (`application_agent_chat_custom_agent_contract` has
no Windows-uncontained entry; `gh_cli_git_smart_transport`'s executable was
not built); the new registration raises no finding.

## Not covered

- A merged pull request was checked only through the fixture; the live course
  closed a pull request without merging it, to leave BPR-Test's main alone.
- The live course used the application's documents through `gh`, not the
  application's own live provider; the REST answer's fields the target read
  checks (state, ids, refs, base) were read from the same pull request.
- macOS offscreen only; Windows and Linux were not run. The new registration
  is listed Windows-uncontained and unmeasured, as its sibling
  `application_thread_resolution_refresh` is.
- No owner look was requested: the change removes a refusal and adds no
  surface.
