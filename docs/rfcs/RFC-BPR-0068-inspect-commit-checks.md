---
id: "RFC-BPR-0068"
title: "Inspect commit checks and their failures in Overview"
status: "Proposed"
area: "BPR"
depends_on: []
metadata: {"mandate_revision":"merge-gates-r1","rfc_base_sha":"9857d596769468c0585c475467175e3998a33530","milestone_plan":{"id":"merge-gates-and-review-waiting-v1","path":"docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Inspect commit checks and their failures in Overview

## Adoption

Adopted on 2026-10-11 under mandate `merge-gates-r1`. RFC_BASE_SHA: `9857d596769468c0585c475467175e3998a33530`. The [live import](assets/merge-gates-and-review-waiting-v1.LIVE_IMPORT.md) retains the authored pack and mapping. Independent critical review of the final pack is required by the implementation mandate and remains pending.

## Problem and grounded current behavior

The owner reports an Overview with **16 passed, 1 failed, 7 skipped**, matching
GitHub, but no way to identify or inspect the failing **Branch Freshness**
check. The individual PR was not supplied or inspected; these are reported
observations, not a reproduced live test.

The current [check record](../../src/application/include/bpr/application/github_read_model.hpp)
retains only an ID, name, status and conclusion. The production
[check parser](../../src/adapters/github/src/github_paged_read.cpp)
binds a run to the accepted head but discards its description and destinations.
The [Overview projection](../../src/application/src/pull_request_overview.cpp)
reduces the records to outcome totals, and the
[Qt page](../../src/adapters/qt/src/pull_request_overview_page.cpp)
renders those totals as text. Its operation catalog reads check runs, with no
commit-status collection. Thus both presentation and normalized observations
need changes; making the existing label clickable is insufficient.

GitHub documents [check-run output and detail links](https://docs.github.com/en/rest/checks/runs)
and separate [commit status contexts](https://docs.github.com/en/rest/commits/statuses).
These give the proposal a supported read path without running CI or loading a
third-party check page inside BPR.

## Proposed behavior and boundaries

### Readable results inside Overview

Keep the compact checks totals and make their details a clearly labelled,
keyboard-operable disclosure in Overview. On the first accepted load, expand
it when a check has failed or requires action; preserve a subsequent user
collapse while the same PR remains open. The collapsed summary still exposes
the failure and offers an obvious way to inspect it. Do not require a hover,
browser visit or a second navigation mode to identify the failure.

List check runs and commit-status contexts with their names, producer when
known, actual state/conclusion, and a readable bounded provider description
or output summary where supplied. A missing description says that no details
were supplied. Keep failure, timeout, cancellation and action-required outcomes
distinguishable even when the compact summary groups them. Order failures and
action-required results first, waiting/running results next, then settled
results, with stable identity ordering within a group. Passed, skipped and
neutral results remain inspectable.

Each result with an admitted destination has an **Open check details** action
through the existing host-owned external-link path. A missing or refused
destination leaves the result readable and the unavailable action explained.
Provider text is untrusted content; reuse the existing safe text/Markdown
presentation and resource restrictions. Downloading logs, embedding external
websites, rerunning jobs and executing repository code are outside this pack.

### Check outcome is separate from merge requirements

The rows and totals describe the same accepted current result set. Handle
repeat attempts and duplicate names using provider identities and producer
context, not the name alone; an old failed attempt must not make a current
successful result fail. Keep check runs and commit statuses distinct when both
exist under the same name. Historical attempts, if offered, are labelled and
excluded from current totals.

Use the commit whose checks GitHub evaluates for this PR, labelling whether it
is the PR head or its test merge commit. Never mix the two sets or silently
present head-only evidence as a complete merge verdict. If that selection
cannot be established, show the known commit and the limitation. GitHub's
[required-check guidance](https://docs.github.com/en/pull-requests/how-tos/merge-and-close-pull-requests/troubleshooting-required-status-checks)
distinguishes those commits and distinguishes a reported skipped/neutral result
from a required workflow that has not produced a result. Preserve those
distinctions: a skipped row alone establishes neither a failed gate nor
overall merge readiness.

Show **Required**, **Not required**, or **Requirement unknown** only according
to accepted requirement evidence. GitHub's
[check requirement field](https://docs.github.com/en/graphql/reference/checks#checkrun)
can answer requiredness for an exact PR. Without that observation, leave it
unknown; a failing optional check is not automatically a merge blocker.
[The merge-requirements RFC](RFC-BPR-0069-reviewers-and-merge-requirements.md)
owns the overall verdict and expected required checks that have not reported.
The first milestone remains useful with unknown requiredness.

### Bounded, current observations

Extend the existing provider read, identity acceptance and projection paths.
Account, host, repository, PR, accepted base/head, evaluated commit and request
generation must agree before results become current. Head replacement must not
leave the former head's green results looking current. An unavailable,
rate-limited, cancelled or incomplete read is distinct from zero checks; keep
any retained observation visibly stale and identify partial totals as partial.
An optional detail read must not make otherwise readable PR content disappear.

Reuse existing page, record, text and rate budgets, with explicit availability
for any newly bounded detail collection. Additional reads participate in the
same cancellation, account lease and scheduler policies on native HTTPS and
supported CLI profiles. Core decides semantic state and stale acceptance; the
host performs reads and browser handoff; Qt receives a bounded projection.
[The proposed amendment](assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md) identifies the narrow
GraphQL expansion beyond the original provider contract.

### Native presentation

Use the existing Overview summary region and its resizable boundary; preserve
the files table, review progress and Start/Continue review. Use native theme
roles, application font, text labels alongside state icons, wrapped names and
summaries, visible focus, and incremental row updates. A result expansion or
refresh retains selection and reading position where its identity survives.
This direction applies the loaded frontend-design skill within the existing
Qt design language; it proposes no new palette, web surface or visual theme.

## Acceptance criteria

- **AC01:** A composed PR containing the reported 24-result example still
  shows the matching totals and exposes Branch Freshness by name, its failed
  outcome, and its supplied explanation from ordinary Overview controls.
- **AC02:** Check-run and commit-status examples retain distinct producer,
  identity and outcome semantics. Repeat attempts, identical names, skipped
  results and a head/test-merge disagreement produce the current-set behavior
  defined above, without claiming merge readiness from totals.
- **AC03:** An available result destination opens through the existing
  admitted host action. Missing/refused destinations and absent output remain
  readable; unsafe content cannot create an effect or embedded remote load.
- **AC04:** Late responses, head/account changes, pagination limits and
  failed detail reads cannot present old or incomplete results as current and
  complete, and do not prevent reading otherwise available PR content.
- **AC05:** The rendered disclosure, result details and actions work by
  keyboard and accessible names at narrow widths and enlarged text in existing
  appearances. Accepted updates preserve usable Overview navigation and the
  user's selection/disclosure state.

## Proposed implementation

[Normalize observations](../issues/60-done/BPR-1390-check-observations.md),
[compose Overview](../issues/60-done/BPR-1391-check-details-overview.md), and
[reconcile closeout](../issues/30-in-progress/BPR-1392-close-rfc-0068.md) implement this
scope under the [canonical plan](plans/merge-gates-and-review-waiting-v1/milestone-plan.json). See the
[decision record](assets/merge-gates-and-review-waiting-v1/DECISIONS.md#checks-and-confirmation) for draft readiness.
