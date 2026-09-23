---
schema: "implement-rfc.card.v1"
id: "BPR-0990"
title: "Home refresh, discovery status, Follow and Save repository"
status: "In Progress"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0988","BPR-0989"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0026-planning-reconciliation","criteria":["AC-02","AC-04","AC-05","AC-10"],"answers":"BPR-0801"}
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
