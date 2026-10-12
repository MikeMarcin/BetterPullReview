---
id: "RFC-BPR-0069"
title: "Show pending reviewers, code-owner teams and merge requirements"
status: "Proposed"
area: "BPR"
depends_on: ["RFC-BPR-0068"]
metadata: {"mandate_revision":"merge-gates-r1","rfc_base_sha":"9857d596769468c0585c475467175e3998a33530","milestone_plan":{"id":"merge-gates-and-review-waiting-v1","path":"docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Show pending reviewers, code-owner teams and merge requirements

## Adoption

Adopted on 2026-10-11 under mandate `merge-gates-r1`. RFC_BASE_SHA: `9857d596769468c0585c475467175e3998a33530`. The [live import](assets/merge-gates-and-review-waiting-v1.LIVE_IMPORT.md) retains the authored pack and mapping. Independent critical review of the final pack is required by the implementation mandate and remains pending.

## Problem and grounded current behavior

The owner reports that GitHub shows a code-owner review requirement naming
three teams, a blocked merge and an approving-review requirement, while BPR
shows none of those obligations. Overview omits people who have not acted.
The owner's follow-up explicitly asks whether the named code-owner teams can
be obtained; it does not accept replacing them with a generic warning.

The [participant projection](../../src/application/src/pull_request_overview.cpp)
adds the author, commenters and submitted review authors. It never adds
outstanding requests. The [PR snapshot model](../../src/application/include/bpr/application/github_read_model.hpp)
has neither requested reviewers nor merge/review decision facts. Personal
discovery's [requested-reviewer parser](../../src/adapters/github/src/github_paged_read.cpp)
counts users plus teams but retains no roster or code-owner origin.

There is a documented route for the missing names: GitHub
[ReviewRequest](https://docs.github.com/en/graphql/reference/pulls#reviewrequest)
has `asCodeOwner` and `requestedReviewer`; the latter can identify a user or a
team. The same [PR API](https://docs.github.com/en/graphql/reference/pulls#pullrequest)
exposes review decision, merge status and an auto-merge request. These are
distinct facts, not one combined website message. Named code-owner requests
are therefore required scope, not an assumed API impossibility.

## Proposed behavior and boundaries

### A complete pending-review roster

Overview includes every outstanding user and team request returned by a
complete bounded read, even when the actor has never commented or reviewed.
Use provider identity for deduplication. A user who has reviewed and is asked
again shows both the latest submitted disposition and the current request;
do not let the old approval erase the new request. A pending unpublished
review, an outstanding review request, and a submitted review have different
labels and cannot substitute for one another.

Keep teams as teams, with organization-qualified identity and readable name
where supplied. Do not expand them into guessed members or count a team as a
known number of people. Label requests flagged by the provider as code-owner
requests. Show the requested code-owner team names directly in a visible
**Waiting on code-owner review from …** explanation when review is required.
This must name team A, team B and team C in the owner's representative case.
Ordinary requested reviewers remain visible alongside them with distinct
wording. A null, hidden or unsupported actor has a visible unavailable identity
and contributes to completeness reporting rather than silently disappearing.

Current requests are the waiting roster. The latest submitted review remains
review history; neither an ordinary comment nor a disappeared request is
proof that a requirement was approved. Accepted removals and re-requests
update the roster without leaving stale waiting entries.

### Explain merge requirements in Overview

Add a concise **Merge requirements** section in Overview, visible with the
summary rather than hidden behind check totals. It presents GitHub's last
accepted merge assessment and the known unmet conditions together. For the
reported scenario it exposes the blocked merge, failed Branch Freshness
result, required approval and named code-owner requests at the same time.
The reader can determine whom to contact without first opening GitHub.

Requirements come from rules applying to the actual target branch, including
inherited active rulesets, and applicable classic branch protection where
readable. GitHub's [branch-rules endpoint](https://docs.github.com/en/rest/repos/rules#get-rules-for-a-branch)
selects active applicable rules across configuration levels. Do not show every
repository ruleset as an obligation on this PR, or treat evaluate/disabled
rules as blocking. Preserve the origin of an observed requirement for a
small details disclosure and link to its admitted GitHub destination when
available. Failure to read policy details cannot turn a blocked server
assessment into ready.

Represent required approvals, code-owner review, specifically required team
reviews, required checks and branch freshness when their applicability is
observed. Show other returned obligations, such as conversation resolution,
without pretending to evaluate conditions the read cannot establish. An
expected required check with no reported result is visible as **Waiting for
check result**, distinct from the observed-result totals. Use the check
detail surface defined in the [checks RFC](RFC-BPR-0068-inspect-commit-checks.md)
for a reported failure; a check named Branch Freshness does not itself prove
GitHub's separate behind-base condition.

State a configured approval minimum as a minimum. State a remaining approval
count only when accepted evidence establishes which approvals satisfy the
applicable rules. Do not subtract the number of historical approvals or
requested actors: stale/dismissed reviews, review permissions, code owners and
last-push restrictions can change what counts. Likewise, a list of requested
code-owner teams is not proof that every team must independently approve.
GitHub's [ruleset semantics](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/available-rules-for-rulesets)
distinguish alternative code owners from separately required team reviews.
Reflect that distinction without building a local CODEOWNERS/rules engine.

An all-green checks list is not a ready-to-merge assessment. Distinguish
blocked, checking/unknown, draft, conflicts and the positive server assessment
using ordinary language. A positive assessment describes GitHub's observation,
not a guarantee that the current viewer may merge. An observed auto-merge
request may be shown as enabled and waiting on the visible requirements;
missing reviews alone never imply auto-merge is enabled or scheduled. BPR
does not invent or copy an unconditional “one approval will merge this” claim
when another condition remains unresolved.

### Capability and freshness are part of the answer

Read supported public APIs through the selected host/account transport. Extend
the existing capability-shaped REST/GraphQL contract as described in
[the amendment](assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md). Complete the bounded reviewer
connection, retaining `asCodeOwner` and typed actor identities; the REST roster
can supply known requests when that richer observation is unavailable, but
cannot establish their code-owner origin by itself.

On a supported, authorized path, omitted named code-owner requests fail this
RFC. A real permission, host-version, redacted-identity or completeness limit
must instead be visible beside the affected field while retaining other
known facts. This is error handling, not permission to ship a generic warning
in place of an available roster. Unsupported rule details do not hide the
aggregate review requirement or known teams. Never claim complete website
parity from an incomplete observation.

Bind policy facts to the accepted target branch/base and review/check facts
to the accepted PR/head and exact authority. A base change, new push,
re-request, dismissal or later approval refreshes the affected facts; late or
failed completions cannot mark them satisfied. Preserve known observations
with explicit stale status while refreshing. Policy and team lookups share
the existing bounded scheduler/rate budget; do not fetch every team's
membership or require repository administration just to open a PR.

### Native and read-only scope

Use existing Overview typography, native palette, summary splitter and
incremental models, with readable wrapped names and state text. Keep blockers
and waiting identities legible without hover and expose the same explanation
to accessibility. Load details through semantic intents; Qt performs no
provider or browser effects. The frontend-design skill was loaded for this
information hierarchy, which preserves the current review workspace.

The product remains a review client. This pack introduces no merge, branch
update, reviewer-request, reminder/message, ruleset editing or auto-merge
mutation. Opening GitHub is an explicit secondary action; identifying a
supported blocker and its waiting reviewers works inside BPR.

## Acceptance criteria

- **AC01:** A composed PR with requested users and teams that have never
  acted displays every returned request. A prior approval followed by a
  re-request retains both facts; removal, dismissal and replacement update
  the roster by identity without confusing pending reviews with requests.
- **AC02:** With a blocked review requirement and three provider-flagged
  code-owner team requests, Overview visibly names all three teams and the
  requirement. It neither replaces them with an aggregate warning nor claims
  that every listed team must separately approve without such evidence.
- **AC03:** The reported failure/approval scenario exposes its simultaneous
  blockers, applicable rule origins and check details. A missing required
  result, an optional failure, classic protection, inherited active rules and
  inactive rules remain distinguishable as defined above.
- **AC04:** Required-minimum, remaining-approval and auto-merge statements
  follow the evidence boundary. Historical approvals or request counts cannot
  produce a false remaining count, satisfied gate or enabled auto-merge claim.
- **AC05:** Permission limits, unsupported host fields, partial connections,
  new heads/targets and late completions retain truthful field availability
  and usable PR content across native and supported CLI transports. A known
  merge block cannot disappear because its detailed explanation is unavailable.
- **AC06:** The rendered requirements and roster are readable and operable
  through ordinary Overview controls, keyboard and accessibility with long
  team names, narrow widths and enlarged text. Inspecting them performs only
  admitted reads and explicitly requested external-link handoffs.

## Proposed implementation

[Normalize requests and requirements](../issues/60-done/BPR-1393-review-requirement-observations.md),
[compose Overview](../issues/60-done/BPR-1394-reviewers-and-merge-overview.md), and
[reconcile closeout](../issues/10-backlog/BPR-1395-close-rfc-0069.md) implement
this scope under the [canonical plan](plans/merge-gates-and-review-waiting-v1/milestone-plan.json). See
[the decision record](assets/merge-gates-and-review-waiting-v1/DECISIONS.md#checks-and-confirmation) for the actual
owner clarification and validation limits.
