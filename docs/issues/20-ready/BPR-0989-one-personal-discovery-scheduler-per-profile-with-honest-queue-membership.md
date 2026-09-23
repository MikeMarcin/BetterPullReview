---
schema: "implement-rfc.card.v1"
id: "BPR-0989"
title: "One personal discovery scheduler per profile with honest queue membership"
status: "Ready"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0988"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-02","AC-03","AC-04","AC-05","AC-06"]}
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
