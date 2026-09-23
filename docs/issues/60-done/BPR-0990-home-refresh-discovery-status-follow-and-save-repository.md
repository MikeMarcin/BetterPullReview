---
schema: "implement-rfc.card.v1"
id: "BPR-0990"
title: "Home refresh, discovery status, Follow and Save repository"
status: "Done"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0988","BPR-0989"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-02","AC-04","AC-05","AC-10"],"answers":"BPR-0801"}
acceptance: [{"criterion":"A composed public-path course over a recording provider with at least 2,000 unrelated open pull requests in a saved repository: discovery issues only the bounded personal searches and requested-reviewers reads, no snapshot, file or diff read; Home lists the authored and requested rows in Waiting on others and Needs me with their reasons; a second window adds no discovery request; a pull request opened by link is under Recently viewed and neither enrolled nor saved until Follow or Save repository; no request mutates GitHub","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"application_personal_discovery_public_path passes 26.7 s: 2,404 open pull requests (2,400 unrelated); Save Repository runs one refresh of exactly 2 personal searches (author:/review-requested:octo-reviewer) and 4 requested-reviewers reads, no other account read; #4003 Needs me (Requested), #4001 Waiting on others, team-only #4004 and unrelated absent; #17 opened by link is Recently viewed, unenrolled and unsaved until Save Repository/Follow; second window (after its first read, BPR-1017) shows the same queue and sentence with no discovery request; its Refresh runs the same six reads only; zero review mutations. Negative control without the composed reconcile fails."}},{"criterion":"qt_composed_github_scale_{100,125,200} (and qt_home_list_* where the row is drawn) pin the refresh glyph's enabled state and intent, the status sentence per state, the unknown reason word, Follow and Save repository, and their places in the declared focus order","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"Refined on the card: qt_composed_github_scale_* stops at BPR-0940's pre-existing assertion before Home, so the pins run directly in qt_workbench_home_discovery_scale_{100,125,200} (glyph place/name/description/focus stop/enablement/intent, every sentence, Home arrival activation, Follow/Save Repository in the Pull Request menu with check and enablement, none for a signed-out read) and qt_workbench_focus_order_scale_{100,125,200}; qt_review_landing_model pins the Unknown word; qt_home_list_scale_* pass. Two negative controls fail as named. Follow and Save Repository are menu commands, not strip stops (recorded refinement)."}},{"criterion":"Changed-surface upkeep: boundaries, format, tidy, documentation, composition digest reseal with review","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"Focused set 34/34; restoration public path 563.89 s; composed Qt suites stop only at known BPR-0940/Connect assertions; application_composition_contract only BPR-0983's three; core-only green except stray-worktree rows and BPR-0854; architecture_boundaries on a clean export; clang-format clean; tidy clean on changed lines; review-queue.md and workbench-specification.md updated; DOCUMENTATION_OK; git diff --check clean; reviewed BPR-0990 reseal (mutation-branch pin unchanged)."}}]
---

# Home refresh, discovery status, Follow and Save repository

## Scope

The composed half of RFC-BPR-0026's personal discovery. The Home the owner
accepted under RFC-BPR-0028 and RFC-BPR-0032 is unchanged in shape; this
card gives its existing controls the discovery they stand for and adds the
two local gestures the RFC names.

- **Refresh.** Home's refresh glyph sends the profile's discovery refresh
  intent from [[BPR-0989]] whenever a session with an account exists, and
  its enabled state says so. This answers [[BPR-0801]]'s question with
  RFC-BPR-0026's explicit Refresh action; that card is finished from this
  one's evidence.
- **Status.** One sentence on Home, where Home already reports refusals:
  when the queue was last refreshed, that a refresh is running, that the
  list is partial and why, or that it is stale and when it retries; with
  no viewer, a sign-in action instead. Cached rows stay.
- **Unknown detail** reads as its own reason word, not as Following's quiet.
- **Follow and Save repository.** For the pull request open in the window:
  Follow or Unfollow (local only) and, when its repository is not saved
  for this account, Save repository. Opening a pull request by link
  records it under Recently viewed and enrolls or saves nothing by itself.
  No gesture here reaches GitHub.
- **Composition.** The discovery port from [[BPR-0988]] is composed over the
  accepted session's account transport; a recording fixture serves a saved
  repository with thousands of unrelated open pull requests.

Owned surfaces: `src/adapters/qt` Home, strip and the open review's
actions; `src/app/src/github_application.cpp` discovery composition and
fixture; `tests/adapters_qt` composed and Home courses; a new composed
public-path course under `tests/end_to_end`;
`docs/ux/workbench-specification.md`.

## Acceptance

- A composed public-path course over a recording provider with at least
  2,000 unrelated open pull requests in a saved repository: discovery
  issues only the bounded personal searches and requested-reviewers reads,
  no snapshot, file or diff read; Home lists the authored and requested
  rows in Waiting on others and Needs me with their reasons; a second
  window adds no discovery request; a pull request opened by link is under
  Recently viewed and neither enrolled nor saved until Follow or Save
  repository; no request mutates GitHub.
- `qt_composed_github_scale_{100,125,200}` (and `qt_home_list_*` where the
  row is drawn) pin the refresh glyph's enabled state and intent, the
  status sentence per state, the unknown reason word, Follow and Save
  repository, and their places in the declared focus order.

## Evidence

Not run at allocation (planning head `2722bd67`).

## Implementation and verification — 2026-09-23

Source commit `0d56cee5` on `main`, claimed at `3e12138a`.

- **Composition.** `github_profile_runtime` owns one
  `application::personal_discovery_host` beside its queue host, over the
  authenticated `github_provider` and a system clock, shut down before the
  queue host and counted in quiescence. After every GitHub presentation it
  adopts `personal_discovery_session_of(Host->accepted_profile_session())`
  (new accessor; the same session again changes nothing, public preview and
  signed out adopt none). After every queue projection it posts one
  coalesced `reconcile()`, so a repository saved from any window is searched
  at once. Each window sends Home's Refresh and Home's activation as
  `queue_discovery_intent` and the Pull Request menu's Follow as
  `queue_follow_intent`; Save Repository reuses the saved-repository intent.
- **Host additions.** `personal_discovery_host::reconcile` (a newly saved
  repository refreshes now, or when a running refresh ends; a forgotten one
  is only no longer searched) and the queue host's withdrawal of a forgotten
  repository's discovery reasons (`undiscovered` unless a role, rule, follow
  or review still holds the pull request).
- **Qt.** On Home the refresh glyph is its own `refreshQueueAction`, the same
  glyph and place after Filter, disabled while a refresh runs or without an
  account session and saying why; the review strip keeps the pull request's
  refresh and its shortcut. `homeDiscoveryStatus` under the bar says one
  sentence from the projected status (`qt::home_discovery_sentence`): found
  at a clock time from N saved repositories, finding, partial with why,
  stale with the failure, what it still shows and the retry-after, or "Save
  a repository to find the pull requests in it that need me." Arriving at
  Home and activating a window showing Home send the activation. The
  **Pull Request** menu holds Follow (checked from the queue) and Save
  Repository (offered while the repository is not saved); a pull request
  read signed out offers neither.
- **Fixture.** `github_personal_discovery_fixture` makes the public-path
  fixture server answer the two personal reads by evaluating their
  qualifier over a repository list, after production wire validation, and
  reports the searches, their terms and every account read by operation.

Refinements recorded against the Scope and Acceptance above:

- *Follow and Save Repository are menu commands, not strip stops.* They sit
  in a new Pull Request menu on the menu bar (and the Windows strip's
  application menu), reachable by keyboard there, so the declared focus
  order gains only Home's refresh glyph. The accepted strip is unchanged.
- *"With no viewer, a sign-in action instead."* Without an account session
  the sentence is empty: the signed-out Home already is the sign-in card.
- *`qt_composed_github_scale_*` stops at [[BPR-0940]]'s assertion* before
  any Home course, so its updated Home strip pin does not run there; the
  glyph, sentence, menu and focus pins run directly in
  `qt_workbench_home_discovery_scale_{100,125,200}` (`--home-discovery`),
  as [[BPR-0981]] and [[BPR-0985]] did, and `qt_workbench_focus_order_*`.
- *A new window shows the sign-in card until it reads* (found here,
  recorded as [[BPR-1017]] under RFC-BPR-0024). The composed course opens a
  pull request in the second window before it looks at its Home.

Checks at `0d56cee5`:

- `application_personal_discovery_public_path` (new, 26.7 s): sign-in with
  nothing saved reads nothing and says to save a repository; opening #17 by
  link lists it under Recently viewed and enrolls, saves and searches
  nothing; Save Repository from the Pull Request menu runs one refresh of
  exactly two personal searches (`author:octo-reviewer`,
  `review-requested:octo-reviewer`) and four requested-reviewers reads over a
  repository of 2,404 open pull requests (2,400 unrelated), and no other
  account read; #4003 is in Needs me as Requested, #4001 waits on others,
  the team-only #4004 and every unrelated pull request are absent; the
  sentence says when and from how many repositories; Follow keeps #17 as
  followed only; a second window shows the same queue and sentence and adds
  no discovery request; its Refresh runs one more refresh of the same six
  reads and nothing else; zero review mutations. Negative control removing
  the composed reconcile fails "saving the repository runs one discovery
  refresh" (`build/bpr0990-negative-reconcile.log`).
- `qt_workbench_home_discovery_scale_{100,125,200}` (new): the glyph's
  place, name, description, focus stop, enablement and intent, every
  sentence, Home arrival sending one activation, Follow/Save Repository
  intents and their check and enablement, nothing for a signed-out read.
  Negative controls: keeping the pull request's refresh on Home fails the
  A7 strip pin (`build/bpr0990-negative-glyph.log`); dropping the arrival
  activation fails "arriving at Home asks discovery once to catch up"
  (`build/bpr0990-negative-activate.log`).
- `application_review_queue_enrollment` adds
  `test_the_discovery_session_of_an_accepted_session` and
  `test_discovery_follows_the_saved_repositories`; withholding the forget
  withdrawal fails "what was found only there is no longer found"
  (`build/bpr0990-negative-forget.log`).
- Focused set 34/34 (`build/bpr0990-focused.log`), including the focus
  order at three scales, `qt_home_list_scale_*`, `qt_review_landing_model`,
  the queue suites, `application_setup_destination_public_path`,
  `application_profile_multi_window_public_path`,
  `application_profile_two_account_isolation_public_path`, the provider
  contract suites and the boundary contracts.
  `application_profile_window_review_restoration_public_path` passes
  (563.89 s). `qt_composed_github_scale_100` and
  `qt_anonymous_public_preview_scale_100` stop at their known pre-existing
  assertions ([[BPR-0940]], Connect); `application_composition_contract`
  fails only on [[BPR-0983]]'s three recorded assertions.
- `core-only` builds and passes except the stray-worktree structure tests and
  [[BPR-0854]]'s `msvc_secure_crt_contract`; `architecture_boundaries` passes
  on a clean export. The network-isolation manifest on a clean export lists
  no failure for the new row; `application_personal_discovery_public_path`
  is pinned Windows-uncontained with the multi-window row's cause.
- `clang-format` check clean; strict tidy reports nothing on changed lines.
  `verify_documentation.py` `DOCUMENTATION_OK`; `git diff --check` clean.

Composition digest reseal review (the `BPR-0990 reseal` note in
`application_review_round_trip_composition_contract.cmake`, full
`8a20d055…`, manifest `f1bb7571…`): the `github_application.cpp` changes are
the discovery clock, the runtime's discovery member, construction, shutdown
and quiescence, `adopt_discovery` after each presentation, the coalesced
reconcile post after each queue projection, the two window dispatches, the
window alias and the fixture-only evidence accessor. The discovery reads use
the accepted session's own account transport through the provider's existing
exact-lease admission; no credential, vault, transport, capability or
mutation path changed, and the mutation-branch pin is unchanged.

No attended check was run or is claimed. Whether the new glyph behaviour,
sentence and Pull Request menu read well to the owner is RFC-BPR-0026's
subjective UX, left to the existing attended-review workflow.
