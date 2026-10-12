---
id: "RFC-BPR-0070"
title: "Keep authored pull requests waiting on others visible on Home"
status: "Proposed"
area: "BPR"
depends_on: ["RFC-BPR-0069", "RFC-BPR-0066"]
metadata: {"mandate_revision":"merge-gates-r1","rfc_base_sha":"9857d596769468c0585c475467175e3998a33530","milestone_plan":{"id":"merge-gates-and-review-waiting-v1","path":"docs/rfcs/plans/merge-gates-and-review-waiting-v1/milestone-plan.json"}}
---

# Keep authored pull requests waiting on others visible on Home

## Adoption

Adopted on 2026-10-11 under mandate `merge-gates-r1`. RFC_BASE_SHA: `9857d596769468c0585c475467175e3998a33530`. The [live import](assets/merge-gates-and-review-waiting-v1.LIVE_IMPORT.md) retains the authored pack and mapping. Independent critical review of the final pack is required by the implementation mandate and remains pending.

## Problem and grounded current behavior

The owner's PR has outstanding reviews on GitHub but appears in Following as
Quiet. [RFC-BPR-0066](RFC-BPR-0066-home-authored-pull-requests.md) already
keeps authored waiting-only PRs in Waiting on others; it moves other authored
PRs to My pull requests. That presentation change cannot repair absent facts.

The current [attention policy](../../src/core/src/review_queue_attention.cpp)
requires authored membership and a positive `ReviewersAwaited` count to choose
waiting. [Personal discovery](../../src/adapters/github/src/github_paged_read.cpp)
reads that count, but the [accepted-open composition](../../src/app/src/github_application.cpp)
constructs a default activity record without assigning it. The queue host's
[`remember_activity`](../../src/application/src/review_queue_enrollment.cpp)
replaces the prior record wholesale. That path can erase discovery's waiting
fact. This is a source-grounded defect mechanism, not a live reproduction of
the owner's unsupplied PR.

The same composition's `queue_observation_for` publishes role `none` even with
an accepted author in the snapshot. An authored PR enrolled only by Follow or
an observer rule can therefore lack authored membership, independently of the
count loss. The [queue facts](../../src/core/include/bpr/core/review_queue_attention.hpp)
also carry no reviewer names or merge-requirement explanation to Home.

## Proposed behavior and boundaries

### Preserve and combine observed facts

Use the accepted request roster and review-requirement observations defined
in [the requirements RFC](RFC-BPR-0069-reviewers-and-merge-requirements.md)
as the source of waiting facts shared by Overview and Home. Carry per-fact
availability and freshness: a read that did not observe requests cannot
replace a known roster/count with an empty one. Only a newer complete
observation can establish that no requests remain. Read-receipt, defect,
conversation and ordinary open updates preserve unrelated waiting facts.

Accept observations by exact profile, host, account and PR identity, relevant
base/head and generation. Completions from discovery, retained metadata and
open PR reads must not overwrite newer facts from another path. A new head
does not itself prove requests removed or reviews satisfied; obsolete
revision-dependent assessments become unknown/stale until refreshed.

For an already enrolled PR, establish authorship from accepted provider author
identity compared with its accepted viewer identity, whether enrollment came
from saved-repository discovery, Follow or a local observer rule. Authorship
does not require saving the repository. Do not infer it from a display string,
review history or another account's membership. This change supplies correct
facts for existing enrollment; opening a PR still does not automatically
follow it or add a repository. Team requests do not automatically enroll
every team member or claim that the current viewer was directly requested.

### Waiting includes required reviews

An authored PR has outstanding review work when a complete/usable observation
contains an unanswered user/team request, or GitHub reports an unmet approval
or code-owner review requirement. The latter remains waiting even when no
individual request is returned. Unknown detail is not evidence of no work.

Preserve RFC-BPR-0066's section order and precedence. With outstanding review
work and no higher-priority existing activity, the authored PR belongs in
**Waiting on others**. With unread discussion or another existing higher
priority activity, it belongs in **My pull requests**, retaining the waiting
explanation as well. When that activity is cleared and reviews are still
awaited, it returns to Waiting on others. It never duplicates itself into
Needs me or Following. Other people's enrollment and review-attention rules
retain their existing meaning.

A simultaneous failed check does not displace the reported waiting-only
example from Waiting on others. Show the check/merge block alongside the
waiting reason. With no outstanding review work, an authored PR with a known
merge/check problem stays in My pull requests and exposes that problem; it
must not say Quiet or “nothing is waiting” merely because its review-request
count is zero. A known problem also must not be hidden by the Quiet-items
filter as if it were settled. This adds truthful row state, not a new section
or a new notification category.

### Explain who and what is awaited

Home's existing row reason identifies named code-owner teams or requested
reviewers and any concurrent known block. For example, **Waiting on code-owner
review from @org/team-a, @org/team-b and @org/team-c; Branch Freshness failed**.
Use readable wrapping and bounded summaries with an ordinary route to the
complete Overview roster when necessary. An aggregate requirement without
names says **Required review outstanding**; it does not invent people or
counts. Counts distinguish requested people, requested teams and required
approvals. A team is one requested team, not one person or one approval.

Opening the Home row leads to the same accepted requirement explanation in
Overview. Section counts, collapsed summaries, Group, State headers, My role,
Added by and sorting use the corrected projection without losing selected
identity or the row's opening target. Preserve the native appearance and
keyboard behavior; accessible row text includes waiting and blocker facts.
This extends existing Home anatomy under the loaded frontend-design skill.

### Refresh without opening every PR

Populate the minimal waiting/requirement summary through bounded discovery
and retained-metadata refresh, including unopened authored PRs. Home does not
load whole PR snapshots, files, conversations or diffs for this purpose.
Reuse the current discovery limits: 100 search rows, at most 30 discovery
detail reads, 30 retained PRs per cycle and 400 submitted-review records per
retained PR. Any added requirement reads consume the existing shared page,
byte, concurrency and rate budgets, with explicit partial status when the
budget is exhausted; they are not an unbounded multiplier on those limits.

Retain the bounded last accepted waiting summary with existing private Home
metadata so a restart or an offline refresh can present it as last known.
An older stored row with no such observation starts unknown, never known
empty. Freshness and admission still belong to the host; storage does not
establish current server authority. Complete newer observations of approval,
request removal, changed requirements or closure update the row normally.
Leave genuine unrelated blockers visible after the last review request is
resolved.

Core derives section, state and explanation from typed observations. The host
merges observations, refreshes and persists bounded metadata. Qt only presents
the resulting deltas. Preserve existing notification eligibility for existing
activity and private notification content; this pack introduces no automatic
pings or new blocker notifications. Follow/unfollow, observer exclusions,
Recently viewed and public-preview access retain their established contracts.

## Acceptance criteria

- **AC01:** An unopened authored PR discovered with pending reviews appears
  in Waiting on others. Opening, refreshing and marking its discussion read
  cannot replace its known requests with zero; Home and Overview agree.
- **AC02:** An authored PR enrolled only by Follow or an observer rule gets
  the same waiting placement without saving its repository. An aggregate
  unmet-review requirement with no named request still produces a truthful
  waiting row; an unrelated account or team membership cannot supply that role.
- **AC03:** The mixed Branch Freshness/code-owner example remains in Waiting
  on others with its team names and failure visible. Unread activity and its
  clearance follow the defined My pull requests precedence. Removing the last
  request does not hide an independently remaining review requirement or check
  problem behind Quiet.
- **AC04:** Interleaved discovery/open/retained reads, head/account changes,
  incomplete or failed refresh, restart and old stored rows follow the
  availability rules. A complete newer removal or approval clears only the
  facts it actually settles; Home refresh stays bounded without full PR loads.
- **AC05:** Composed Home preserves exact row selection, opening destination,
  accurate filtered/grouped counts, collapsed summaries and accessible waiting
  text. The Quiet-items filter, existing notification policy, unrelated
  enrollment and Recently viewed behavior follow the boundaries above.

## Proposed implementation

[Preserve and classify waiting facts](../issues/30-in-progress/BPR-1396-home-waiting-facts.md),
[compose Home explanations](../issues/10-backlog/BPR-1397-home-waiting-presentation.md), and
[reconcile closeout](../issues/10-backlog/BPR-1398-close-rfc-0070.md) implement this
scope under the [canonical plan](plans/merge-gates-and-review-waiting-v1/milestone-plan.json).
[The proposed amendment](assets/merge-gates-and-review-waiting-v1/PROPOSED_AMENDMENTS.md) preserves the prior authored
Home decision while extending the facts it consumes.
