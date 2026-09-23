# RFC-0026 planning reconciliation — 2026-09-22

Run `main-closeout-20260922`, segment `rfc-0026-implementer`, milestone
`plan-rfc-0026`; sole non-delegating `rfc-implementer` engineering and
bookkeeping owner in the main checkout on `main` under the
coordinator-implementer recipe. Immutable RFC base
`e851eb8a851ab605cf5a85260a1317ecdf66773b`. The milestone was bound at
`70f6b16b` with the five earlier pack milestones accepted
(`build/bpr0598-bind.json`, definition fingerprint
`sha256:e95ca5bbd053f65b54b981f490e3365dbe05b65b11ae37d8907a42bdb791d5d7`).

The user mandate for this session, verbatim: "implement available unattended
work, focus on closing out rfcs. You can revise and update the cards and rfcs
if they have gone stale." It continues the ordered-pack mandate recorded in
the [RFC-0025 reconciliation](rfc-0025-planning-reconciliation.md) ("implement
RFC-BPR-0024 → 0025 → 0026 … Planning stage can revise/update the rfc if it's
gone stale"). As there, planning records dispositions; it removes, weakens or
accepts no criterion. A superseded row is recorded at closeout as superseded,
never as passed.

## What went stale

RFC-0026 was accepted as a design on 2026-09-04. Three owner decisions since
then govern its presentation:

- [RFC-BPR-0028](../../RFC-BPR-0028-review-workbench-direction.md), owner-locked
  2026-09-06, "supersedes … the Home presentation of RFC-BPR-0026 where they
  conflict": Home is a view with its button (⌘0), the grouped action list
  (needs me, waiting on others, following), and the Open bar.
- The owner's 2026-09-11 Home answers ([[BPR-0732]], [[BPR-0733]], accepted in
  the v007 attended session): a signed-out Home is one paste field that opens
  a public pull request in one gesture, and "the app no longer opens the last
  pull request by itself at startup; Home stays until a gesture", with a
  per-profile Recently viewed list whose entries resume their saved reading.
  RFC-0026's "Resume review" action on Home and its automatic restoration of
  other windows' views are superseded by that direction.
- [RFC-BPR-0032](../../RFC-BPR-0032-calm-home-and-accounts-surfaces.md)
  (Home state panels, Group, Filter, the strip's Home controls; owner accepted
  2026-09-16, closeout [[BPR-0781]] Done), which names RFC-0026's personal
  discovery a non-goal and leaves the refresh glyph's whole-queue meaning to
  [[BPR-0801]].

Browser-style tabs stay registered but unscheduled under RFC-0028, so
RFC-0026's "tab" wording reads as the receiving window's one review.

The canonical predecessor named by the plan, RFC-BPR-0010 ([[BPR-0244]]), is
`Auto-Verified` under owner amendment r4. Every RFC-0026 dependency
(RFC-BPR-0002, 0007, 0010, 0016, 0018, 0024, 0025) is `Auto-Verified`.

## Inventory at planning head `2722bd67`

- **Provider search.** The only pull request search is the repository-wide
  `is:open` REST search inside the full snapshot read (`github_paged_read.cpp`,
  `read_search_results`, bounded by `MaximumSearchResults` with a
  `result_limit` availability); `safe_search_terms` admits exactly `is:open`
  (`github_read_contract.cpp:398-401`). No read lists pull requests by author
  or requested reviewer, and none reads a pull request's requested
  reviewers.
- **Queue attention and enrollment.** The profile's
  `review_queue_application_host` (RFC-BPR-0010) retains observer rules,
  enrollments, preferences, saved repositories ([[BPR-0659]], per host and
  account) and saved servers ([[BPR-0660]]) in the SQLite queue store and
  projects one decided queue to every attached window. Its refresh port in
  the composition replays the snapshot of the pull request that was just
  opened (`composition_queue_refresh`), with `github_role::none` because the
  snapshot carries no queue role (`queue_observation_for`,
  `github_application.cpp:757-786`). So the queue holds only opened pull
  requests matched by a local rule; nothing is discovered. An enrollment
  without an activity report projects as `quiet`
  (`review_queue_enrollment.cpp:203-206`), which is RFC-0026's "unknown detail
  presented as quiet". Membership has a role and a rule, no discovery reason
  and no local follow; Home's Filter "Added by" has no Follow value
  (BPR-0781 A7 divergence 2).
- **Attention policy.** `core::bucket_for` already separates the four
  reviewer-facing buckets from `authored_awaiting_reviewer_action`, and Home
  maps them to Needs me / Waiting on others / Following ([[BPR-0650]],
  [[BPR-0776]]), so an authored pull request awaiting a reviewer never counts
  toward Needs me once its role and awaited count are known.
- **Viewer identity.** An accepted session carries the viewer record
  (`github_viewer_success`); the snapshot repeats it (`Viewer.Login`). One
  accepted session exists per BPR profile at a time; saved repositories are
  keyed by host and account.
- **URL routing.** `application::parse_setup_destination`
  (`setup_destination.cpp:41-127`, moved from the core by [[BPR-0689]])
  recognizes the canonical pull request, `/files`, `/commits`,
  `/commits/<oid>` and fragments, drops the query and refuses a repository
  commit URL or any other subpath. The location is kept as text and every
  non-empty one is reported unavailable after the open (`:260`). The setup
  open dispatches `open_pull_request_intent` even for the pull request the
  window already shows, which is a second snapshot read.
- **Pending setup.** [[BPR-0689]] retains the exact destination across Add
  server and sign-in, cancels safely and retires on authority replacement;
  [[BPR-0732]] opens a public link signed out in one gesture; [[BPR-0914]] and
  [[BPR-0915]] route a pasted link through a connected CLI session and one
  shared sign-in. No unaccepted authority receives a credential.
- **Home, Resume, recents, window filters.** Home button and Home-only strip
  ([[BPR-0646]]); Recently viewed with Resume, per profile, persisted by the
  restoration store ([[BPR-0733]]); Group, State headers and Filter per window
  ([[BPR-0783]], `home_view_state`); keyboard, VoiceOver and 100–200 %
  renders accepted on RFC-0032 A5–A8.
- **Refresh.** Home's refresh glyph acts on the open pull request and is
  disabled with none open; no whole-queue refresh intent exists
  ([[BPR-0801]]).

## Criterion map

| Criterion | Disposition |
|---|---|
| AC-01 Home reachable; profile/window startup; Resume restores the exact retained review/tab without destroying other sessions | No slice. Home button and Home-only chrome ([[BPR-0646]]); Recently viewed with Resume ([[BPR-0733]], owner accepted v007) is the Resume action. Superseded by the owner's 2026-09-11 direction: no window opens a review by itself at startup (RFC-0026's automatic restore of other windows' views), and the tab wording by RFC-0028. Closeout re-runs `application_profile_window_review_restoration_public_path`, `application_profile_multi_window_public_path`, `qt_composed_github_scale_*` (Home button, Recently viewed) and `qt_home_list_scale_*`. |
| AC-02 dedupe; attention, waiting and quiet explained; authored-awaiting outside attention count; unknown not quiet | Presentation shipped and owner-accepted (RFC-0028 AC-06, RFC-0032 A6/A7). Discovery-backed roles and awaited counts: [[BPR-0988]]; unknown-detail state and membership: [[BPR-0989]]; its rendered reason: [[BPR-0990]]. |
| AC-03 authored and directly requested discovered in saved repositories without opening; reviewed and followed retained across request changes and restart | [[BPR-0988]] (read) and [[BPR-0989]] (membership, retention, persistence). |
| AC-04 separate ownership of saved repositories, recents, follow and window filters; external open enrolls nothing and mutates no subscription | Saved repositories, recents and window filters exist ([[BPR-0659]], [[BPR-0733]], [[BPR-0783]]). Local Follow: [[BPR-0989]] (membership) and [[BPR-0990]] (gesture); Save repository for an externally opened pull request and the zero-mutation course: [[BPR-0990]]. |
| AC-05 discovery independent of PR loading, paginated, bounded, shared across windows; thousands of unrelated PRs need no broad fetch or hydration | [[BPR-0988]] (bounded read, no hydration), [[BPR-0989]] (one scheduler per profile), [[BPR-0990]] (composed course with ≥ 2,000 unrelated open pull requests and two windows). |
| AC-06 rate limits, incomplete pages, offline, auth failures, removed repositories, stale completions: honest availability, cached rows, no cross-profile/account publication or false removal | [[BPR-0988]] (typed failures) and [[BPR-0989]] (manual-executor host courses). |
| AC-07 canonical and supported PR subpages resolve in the receiving window, reuse the open PR, navigate exact supported locations | Parser and one-field Open exist ([[BPR-0669]], [[BPR-0689]], [[BPR-0732]]). Typed locations, exact mapping and reuse: [[BPR-0987]]. |
| AC-08 unavailable sublocations explained; malformed or non-PR never guessed | Refusal of malformed and non-PR input exists ([[BPR-0689]]); per-location exactness and explanations: [[BPR-0987]]. |
| AC-09 unknown-server and signed-out navigation keep the destination, cancel safely, send no credentials to unaccepted authorities | No slice. [[BPR-0689]], [[BPR-0732]], [[BPR-0914]], [[BPR-0915]]. Closeout re-runs `core_setup_destination`, `application_setup_destination` and `application_setup_destination_public_path`. |
| AC-10 composed Home, filtering, Refresh, Resume, Open PR and onboarding keep keyboard, accessibility and responsive layout without Qt effects | Existing flows: RFC-0032 A5–A8 (owner accepted) and [[BPR-0981]]'s focus order. New Refresh, status, Follow and Save repository controls: [[BPR-0990]]'s focus-order and scale courses. |

## Allocation and plan amendment

The main integrator allocated BPR-0987 to BPR-0990 with
`python3 scripts/allocate_issue_id.py --count 4 --reserve-for
rfc0026-planning-reconciliation` (`2722bd67`). [[BPR-0598]] remains the entry
card and closes with this reconciliation; [[BPR-0599]] gains the four slices
as dependencies.

As in the two earlier reconciliations, `check-amendment` refuses a new work ID
on its own ("amendment must preserve every obligation; reconcile changed
requirements separately"), both directly and when registering the IDs as
unplanned work. Under this authorized reconciliation the IDs were registered
as unplanned work on a partial plan, and the regrouping into
`close-rfc-0026` was then checked: partial-plan fingerprint
`sha256:16c96d3ad7aa3c5047cc2c0afdb61473726129d476f74403b78868a5401a5cd8` to
complete-plan fingerprint
`sha256:6637b906e9ff68e64dce1b34c1d25fbefd6e7e16a946ea5479af3d253d453289`,
affected milestone `close-rfc-0026` only
(`build/rfc0026-plan-amendment-partial-to-complete.json`). The published plan
before this amendment had fingerprint
`sha256:820fe9d9339bf9356ee7a2bdce3a10e2950c1b6bfcf2e095e863b0f0cde8ab8b`.
`close-rfc-0026`'s outcome and boundary now name the four slices and the
superseding decisions; both exit criteria are unchanged. The bound
`plan-rfc-0026` definition is unchanged.

No live GitHub/GHES mutation, credential, push, or change to a protected
station checkout is authorized. No product code changed in this milestone.
