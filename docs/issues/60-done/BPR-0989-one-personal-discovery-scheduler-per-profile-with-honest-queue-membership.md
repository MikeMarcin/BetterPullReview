---
schema: "implement-rfc.card.v1"
id: "BPR-0989"
title: "One personal discovery scheduler per profile with honest queue membership"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0988"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-02","AC-03","AC-04","AC-05","AC-06"]}
acceptance: [{"criterion":"Core tests: reason union and withdrawal only on a complete current answer; reviewed and followed retention; the unknown-detail state; the authored-awaiting case outside Needs me","outcome":"pass","revision":"b34542ed","evidence":{"kind":"command","detail":"core_review_queue_attention passes with test_discovery_reasons_and_unknown_detail (requested reason is initial review whatever is unknown; authored awaiting is Waiting on others only when detail read; unknown is detail_unknown never quiet and never notifies; discovered never observed); application_review_queue_enrollment's register tests pin union, complete-only withdrawal to undiscovered, and follow/reviewed retention. Negative control letting an incomplete answer withdraw fails 'an incomplete or limited answer establishes no removal'."}},{"criterion":"Application host tests with the manual executor: two attached windows share one scheduler and one request per repository and reason; a stale generation is refused; primary and secondary limits, offline and authentication failure keep cached rows and back off; an incomplete answer removes nothing; a saved repository removed mid-flight publishes nothing for it; an account switch cancels and republishes nothing old; the explicit refresh intent and activation freshness behave as stated","outcome":"pass","revision":"b34542ed","evidence":{"kind":"command","detail":"application_review_queue_enrollment passes seven discovery tests: two windows / one scheduler / one read per repository and reason / two in flight / foreign host and account never read; partial and limited remove nothing; primary limit with retry-after, refused Refresh inside backoff, secondary limit default 60 s, authentication, offline, one unreadable repository, cached rows kept; account switch cancels and old answer refused by host and queue; earlier generation of the same account refused (negative control dropping the generation check fails); refused store write rolled back and reported unavailable (negative control fails); forgotten repository mid-flight publishes nothing; activation freshness 300 s. Focused set 20/20 in build/bpr0989-focused.log."}},{"criterion":"sqlite_queue_enrollment_storage migrates a current-schema file intact and refuses a future schema before writing","outcome":"pass","revision":"b34542ed","evidence":{"kind":"command","detail":"sqlite_queue_enrollment_storage passes: test_version_six_gains_discovery_reasons_and_they_round_trip migrates a canonical version-six file to seven with every enrollment, departure and preference intact and no reason invented, then round-trips the four reasons, undiscovered and detail_unknown through store_all; historical fixtures rebuilt at their own shapes still migrate; test_a_newer_database_fails_closed refuses user_version 99 untouched."}},{"criterion":"Changed-surface upkeep: boundaries, format, tidy, documentation","outcome":"pass","revision":"b34542ed","evidence":{"kind":"command","detail":"core_boundary_contract, core_dependency_boundary, architecture_boundary_contract pass; architecture_boundaries passes on a clean export; core-only builds and passes except the stray-worktree structure tests and BPR-0854's msvc_secure_crt_contract; clang-format check clean; strict tidy clean on changed lines; docs/architecture/review-queue.md and docs/ux/workbench-specification.md updated; verify_documentation.py DOCUMENTATION_OK; git diff --check clean."}}]
---

# One personal discovery scheduler per profile with honest queue membership

## Scope

The decision and host half of RFC-BPR-0026's personal discovery, over the
port [[BPR-0988]] adds. At planning head `2722bd67` the profile's
`review_queue_application_host` retains enrollments, rules, saved
repositories and preferences and projects the decided queue to every
window, but its only refresh replays the snapshot of the pull request that
was just opened (`composition_queue_refresh`), an enrollment without an
activity report projects as quiet (`review_queue_enrollment.cpp:203-206`),
and membership has a GitHub role and an observer rule but no discovery
reason and no local follow.

- **Membership.** Queue membership gains the discovery reasons (authored,
  directly requested) and an explicit local follow, kept beside the
  existing role and rule so no reason overwrites another. The core decides
  what a discovery answer does: a complete answer for the current session
  generation adds and refreshes reasons and may withdraw a reason it no
  longer finds; a partial, failed or stale answer adds but never removes. A
  pull request the reviewer already reviewed, or follows, stays when its
  request disappears; closed, merged, access-lost and gone keep their
  distinct departures.
- **Unknown is not quiet.** An enrolled item whose activity detail has not
  been observed is its own labeled attention state, never `quiet`; an
  authored pull request awaiting another reviewer stays in Waiting on
  others and adds nothing to Needs me.
- **One scheduler per profile.** A discovery host owned beside the profile's
  queue host serves every window of the profile: it searches the saved
  repositories of the accepted session's server and account only, tags
  every request with that session generation, runs on activation subject to
  a freshness interval and backoff from retry-after, runs on an explicit
  refresh intent, bounds its concurrency, cancels on session change and
  shutdown, and never restarts because a window opened a file or a pull
  request. Opening a pull request never waits on discovery.
- **Status.** Per server and account: last successful refresh, refreshing,
  partial with its reason, stale with its failure and retry, or unavailable
  with a sign-in action when there is no viewer. Cached rows stay visible
  through every failure.
- **Durability.** The queue store migrates in place: rules, preferences,
  saved repositories, saved servers and enrollments survive intact; the new
  reasons, follow and each partition's last successful refresh persist; a
  refused write is reported as session-only, never answered from another
  profile's store.

Owned surfaces: `src/core` queue membership and attention, the
application queue enrollment and a discovery host with its ports,
`src/adapters/sqlite` queue store and migration, their tests.

## Acceptance

- Core tests: reason union and withdrawal only on a complete current
  answer; reviewed and followed retention; the unknown-detail state; the
  authored-awaiting case outside Needs me.
- Application host tests with the manual executor: two attached windows
  share one scheduler and one request per repository and reason; a stale
  generation is refused; primary and secondary limits, offline and
  authentication failure keep cached rows and back off; an incomplete
  answer removes nothing; a saved repository removed mid-flight publishes
  nothing for it; an account switch cancels and republishes nothing old;
  the explicit refresh intent and activation freshness behave as stated.
- `sqlite_queue_enrollment_storage` migrates a current-schema file intact
  and refuses a future schema before writing.

## Evidence

Not run at allocation (planning head `2722bd67`).

## Implementation and verification — 2026-09-23

Source commit `b34542ed` on `main`, claimed at `62e604a3`.

- **Membership.** `core::queue_membership` gains `Authored`, `Requested`,
  `Followed` and `Reviewed` beside `Role` and the rule; `enrolled()` holds on
  any of them and `discovered()` on the two discovery reasons.
  `queue_enrollment_register::observe_discovery` adds and refreshes a reason
  for every pull request an answer finds (reviving a departed record, since
  the search reads open pull requests only) and, only for a complete answer,
  withdraws the reason from the rest of that repository; a record that
  nothing else holds then departs as the new `undiscovered`. `follow` and
  `reviewed` are the local reasons; the queue refresh marks `Reviewed` only
  for a discovered (or already reviewed) record the viewer has reviewed, and
  no longer departs a discovered, followed or reviewed record.
- **Unknown is not quiet.** `attention_bucket::detail_unknown` follows
  `quiet`; `queue_activity_observation::Known` defaults true and a missing
  report projects as `Known = false`. `bucket_for` puts a direct request in
  initial review whatever else is unknown, an authored pull request in
  Waiting on others only when its requested reviewers were read and someone
  is awaited, and a discovered item is never `observed`. Detail unknown does
  not notify; Qt shows it in Following with the word Unknown and "Nothing
  about it has been read yet", and the Quiet-only filter leaves it out.
- **Scheduler.** `application::personal_discovery_host` (new), with a clock
  port and `personal_discovery_session` naming the queue session exactly.
  `adopt` cancels and starts a refresh under a new session; `activate`
  refreshes past a 300 s freshness window; `refresh` refreshes now or is
  refused inside a rate-limit backoff (retry-after, else 60 s) and says how
  long remains. It reads the saved repositories of the session's host and
  account only, both reasons each, two in flight, and drops a completion of
  an earlier generation or refresh or of a repository no longer saved. A
  session-wide failure (limits, authentication, offline) cancels the rest of
  the refresh and decides it at once; a repository-specific one does not.
  `queue_discovery_status` (per host and account: refreshing, current,
  partial with why, stale with the failure and retry-after, unavailable with
  no account or no saved repository) is published in every window's queue
  projection; the queue also refuses an answer for another session.
- **Durability.** SQLite queue schema version seven stores the four reasons,
  the new departure and the new bucket filter value; version six migrates in
  place and earlier versions step through it. A discovery answer is stored
  in one transaction (`store_enrollments`) before it is shown.

Refinements recorded against the Scope above, both consistent with the RFC's
"storage failure reports precisely which updates are session-only or
unavailable":

- *Last successful refresh is session-scoped, not persisted.* The rows it
  found persist as enrollments and return at restart; the timestamp does
  not, and the refresh that adoption starts supplies it again. Persisting it
  would need another table for a value the first refresh replaces.
- *A refused discovery write is reported unavailable, not kept
  session-only.* It follows every other queue write: the update is rolled
  back, rows already stored stay, and every window is told the store refused
  it (`queue_landing_status::unavailable` with the failure). Nothing is read
  from another profile's store.

Checks at `b34542ed`:

- `application_review_queue_enrollment`: six discovery tests —
  `test_one_scheduler_discovers_the_saved_repositories_of_its_account` (two
  windows, one scheduler, one read per repository and reason, two in
  flight, foreign host and account never read, both windows see the same
  status), `test_discovery_membership_is_honest_about_absence` (partial and
  limited answers remove nothing and say why; a complete answer withdraws
  and departs as `undiscovered`; a follow is kept),
  `test_discovery_failures_keep_rows_and_back_off` (primary limit with
  retry-after, refused refresh inside it, secondary limit with the default,
  authentication, offline and one unreadable repository, cached rows kept
  throughout), `test_discovery_refuses_stale_and_foreign_answers` (account
  switch cancels; old answer refused by host and queue; no session is
  unavailable), `test_discovery_refuses_an_earlier_generation_of_the_same_account`,
  `test_a_refused_discovery_write_is_reported_not_kept` and
  `test_discovery_freshness_and_forgotten_repositories` (activation and
  Refresh during a refresh add nothing; a repository forgotten mid-flight
  publishes nothing; freshness window).
- `core_review_queue_attention` (`test_discovery_reasons_and_unknown_detail`),
  `sqlite_queue_enrollment_storage`
  (`test_version_six_gains_discovery_reasons_and_they_round_trip`, historical
  fixtures rebuilt at their own shapes; `test_a_newer_database_fails_closed`
  unchanged) and `qt_review_landing_model` (the Unknown word, sentence, group
  and Quiet filter) pass.
- Negative controls fail their named assertions: letting an incomplete
  answer withdraw (`build/bpr0989-negative-partial.log`), dropping the
  generation check (`build/bpr0989-negative-stale.log`) and dropping the
  rollback on a refused write (`build/bpr0989-negative-store.log`).
- The focused set passes 20/20 (`build/bpr0989-focused.log`): the above plus
  `core_review_queue`, `application_review_queue_refresh`,
  `application_review_queue_notification`, `qt_review_queue`,
  `qt_repositories_drawer`, `qt_home_list_scale_100/125/200`,
  `application_queue_composition_contract`,
  `application_native_notification_attended_contract`,
  `application_agent_finding_queue_construction_cut_contract`,
  `application_setup_destination_public_path`,
  `application_review_round_trip_composition_contract` (after `a9984645`
  corrected [[BPR-0987]]'s pins, which never matched its committed file),
  `core_boundary_contract`, `core_dependency_boundary` and
  `architecture_boundary_contract`.
- `core-only` builds and its suites pass except `architecture_boundaries` and
  the two network-isolation structure tests, which stop on the stray
  `.claude/worktrees/agent-a17cae88115dbbb4e`, and `msvc_secure_crt_contract`,
  recorded on [[BPR-0854]]; `architecture_boundaries` passes on a clean
  export (`build/bpr0989-architecture-boundaries.log`).
- `clang-format` check passes on all touched C++ files; strict tidy reports
  nothing in the changed lines (the remaining findings are in unchanged
  `dif_runtime.hpp` and `home_view_state.hpp`). `verify_documentation.py` is
  `DOCUMENTATION_OK`; `git diff --check` clean.

Not composed yet: no window adopts a session, activates or refreshes
discovery, and Home shows no discovery status. [[BPR-0990]] composes it.
