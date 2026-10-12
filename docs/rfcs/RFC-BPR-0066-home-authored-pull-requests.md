---
id: "RFC-BPR-0066"
title: "Separate authored pull requests on Home"
status: "Proposed"
area: "BPR"
depends_on: ["RFC-BPR-0010","RFC-BPR-0026"]
metadata: {"mandate_revision":"home-authored-r1","milestone_plan":{"id":"home-authored-v1","path":"docs/rfcs/plans/home-authored-v1/milestone-plan.json"}}
---

# RFC-BPR-0066: Separate authored pull requests on Home

## Adoption and implementation

Adopted on 2026-10-11 under mandate `home-authored-r1`, including the accompanying Home amendment and the settled Q1 waiting placement. RFC_BASE_SHA: `b87b7e4ae8f62696c3af88abe3c943eaef65c6f5`. The [import record](assets/home-authored-v1.LIVE_IMPORT.md) retains the original draft and amendment. [[BPR-1388]] owns all five criteria and final reconciliation; historical RFC0032 A8 human/native obligations remain unchanged.

## Problem and grounded current behavior

The owner asked on 2026-10-11: “I want the reviews where i am the author to
be separate from Needs Me on home”. The same session's answer below settles
the placement of authored PRs waiting on reviewers.

The queue already knows authorship through `queue_membership.Role` and
`queue_membership.Authored` in [the core membership model](../../src/core/include/bpr/core/review_queue.hpp).
However, [the attention policy](../../src/core/src/review_queue_attention.cpp)
gives unread discussion and other actionable activity precedence over waiting
for reviewers. [Home's group mapping](../../src/adapters/qt/src/review_landing_model.cpp)
uses only that attention bucket. An authored PR with unread comments therefore
lands in **Needs me**; an authored PR merely waiting for reviewers lands in
**Waiting on others**; quiet or unknown authored PRs land in **Following**.
The core's [authored attention tests](../../tests/core/review_queue_attention_tests.cpp)
explicitly preserve unread activity over waiting. Separating authorship must
retain that information.

[Personal Home](RFC-BPR-0026-personal-review-home-pr-url-navigation.md)
and [calm Home](RFC-BPR-0032-calm-home-and-accounts-surfaces.md)
currently specify the old placement. The accompanying
[proposed amendment](assets/home-authored-v1/proposed-home-amendment.md) identifies the affected live
contracts and historical evidence; it does not modify them.

## Proposed behavior and boundaries

### Separate ownership from attention

An enrolled PR authored by the account represented by its exact queue identity
never appears in Home's **Needs me** section or contributes to that section's
count or summary. Recognize both accepted author-role membership and authored
discovery membership, even when other enrollment reasons coexist. Authorship
for one host/account must not classify another account's row, and missing
authorship evidence must not be guessed from display text.

Other people's PRs retain their existing Needs me or Following placement.
Each exact queue identity appears in one active section. Recently viewed stays
a separate history and resume surface and may still contain the same PR.

### Section membership

**Decision Q1 — resolved on 2026-10-11.** The owner answered: “Keep waiting
PRs in ‘Waiting on others’”. This settles the waiting placement; the rest of
this document remains a proposal. No material design questions remain open.

Preserve **Waiting on others** for authored rows whose current core bucket is
`authored_awaiting_reviewer_action`. Put all other authored rows in **My pull
requests**, including actionable, quiet and unknown rows. The existing core
precedence still applies: an authored PR with unread comments belongs in My
pull requests even if reviewers are also awaited; it returns to Waiting on
others when its current bucket becomes waiting-only.

Home's section order is **Needs me → My pull requests → Waiting on others →
Following → Recently viewed**.

### Keep activity visible

My pull requests uses the existing row anatomy. Its reason sentence identifies
the PR as mine and retains the observed activity, including unread counts,
quiet or unknown detail. Its rows keep the existing state words and colours;
authorship is not a new attention state. The section header uses neutral theme
roles because its rows can have different states. Collapsed, it shows its row
count and a concise summary of actionable and unread activity, with unknown
detail never counted as quiet. The core's current bucket remains the source of
whether a row is actionable; moving a row does not mark anything read or
change its review state.

### Fit the existing Home controls

Apply the section order above inside every Organization, Repository or
Account group. With State headers off, keep each section's rows contiguous
and keep authorship readable in row text and accessible descriptions. Existing
account chips, filters and sorting operate over the same eligible rows;
section order comes first and the chosen sort applies within each section.
Counts and summaries describe the rows admitted by those filters.

Use the existing sticky, collapsible panels and keyboard behavior. My pull
requests starts expanded, including on upgrade: a saved collapse of Waiting
on others must not silently hide newly separated actionable PRs. Thereafter
its top-level collapse persists per window. Preserve other saved view choices
and the [existing session-only collapse of named groups and their bands](../issues/60-done/BPR-0783-add-group-state-headers-account-chips-and-filter-to-home.md).
Waiting on others retains its own saved collapse independently.

Empty sections stay absent. A Home containing only authored rows still shows
those rows; an empty Needs me section does not trigger the whole-queue empty
message. Existing filtered-empty, saved-repository onboarding, signed-out and
public-preview behavior continues under [the current Home specification](../ux/workbench-specification.md#home).
Accepted refreshes update rows by stable identity and preserve the current
selection and opening target when that identity remains present.

### Scope and ownership

The core supplies the deterministic authorship/section decision alongside
unchanged attention facts; the host carries accepted projections and persists
view state; Qt presents them using existing theme, layout and interaction
conventions. Concrete types and storage mechanics belong to implementation.

This changes Home's active GitHub PR sections. Discovery scope, enrollment and
departure, freshness reporting, notifications, repository-drawer attention
counts, Review Cycles and remote effects retain their existing contracts.
In particular, the repository drawer may still count activity on authored PRs;
Home's Needs me count describes only its own section. No additional provider
read or full PR load is needed to arrange already accepted queue rows.

## Acceptance

- **AC01:** With an authored PR and another person's PR both carrying unread
  activity, only the latter appears or counts in Needs me. Author-role and
  authored-discovery inputs produce the same separation, scoped to each row's
  account identity.
- **AC02:** Changing an authored row between actionable, waiting, quiet and
  unknown activity produces the placement defined under Section membership,
  with one active row and no loss or fabrication of its activity explanation.
  In particular, unread activity moves it from Waiting on others to My pull
  requests while preserving its unread count. Its Home placement
  does not change notification eligibility or repository-drawer attention.
- **AC03:** The rendered Home preserves the separation, accurate filtered
  counts and readable activity through every Group value, both State headers
  settings, collapse and keyboard navigation. An authored-only queue remains
  visible; empty sections add no empty panels.
- **AC04:** After upgrading and restarting two windows with different saved
  Home views, My pull requests initially appears expanded and subsequently
  retains each window's own collapse choice. Unaffected view choices and the
  documented lifetime of named group collapse remain intact.
- **AC05:** Through the composed application's accepted refresh and ordinary
  row actions, the same exact PR remains selected and opens in the intended
  window after its activity changes; section arrangement adds no provider
  work. Existing Recently viewed, onboarding and signed-out courses remain
  valid for their unchanged behavior.
