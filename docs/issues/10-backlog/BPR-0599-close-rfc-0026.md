---
schema: "implement-rfc.card.v1"
id: "BPR-0599"
title: "Verify and close RFC-BPR-0026"
status: "Backlog"
rfcs: ["RFC-BPR-0026"]
depends_on: ["BPR-0598","BPR-0987","BPR-0988","BPR-0989","BPR-0990"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0026"]
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"planning_reconciliation":"docs/rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md"}
---

# Verify and close RFC-BPR-0026

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 4 of 4.
Planned cards: BPR-0598, BPR-0599, BPR-0987, BPR-0988, BPR-0989, BPR-0990 (allocated
by the 2026-09-22 planning reconciliation). Allocate additional implementation cards only
when the later bounded-slice planning pass needs them, and update this list and
closeout dependencies before implementation. The RFC index remains the order
authority; this requested subset waives no intervening eligibility/closeout gate.

**Design accepted; registration only; implementation requires a later request.**
This unstarted backlog card records the 2026-09-04 registration, not an execution
grant or completed product work. The immutable base is the pre-registration
commit for this approved workstream; registration commits after it are planning
history, not evidence of interrupted product implementation.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Home is reachable and opens according to the profile/window startup contract, with Resume restoring the exact retained review/tab without destroying other sessions. | No slice ([reconciliation](../../rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md#criterion-map)). Re-run `application_profile_window_review_restoration_public_path`, `application_profile_multi_window_public_path`, `qt_composed_github_scale_*` (Home button, Recently viewed) and `qt_home_list_scale_*`. Automatic restore of other windows superseded by the owner's 2026-09-11 direction ([BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md)); tabs by RFC-BPR-0028 | Resume is the first entry's "Resume reading" action on Home's recently viewed list; startup no longer opens a review by itself ([BPR-0733](../60-done/BPR-0733-home-lists-recently-viewed-pull-requests-instead-of-restoring-the-last-one.md), `application_profile_window_review_restoration_public_path`) | None |
| AC-02: Home deduplicates PRs and explains attention, waiting, and quiet states; authored PRs merely awaiting another reviewer do not inflate the user's attention count, and unknown detail is not presented as quiet. | [[BPR-0989]] core and host attention/unknown-detail tests; [[BPR-0988]] roles and awaited counts; [[BPR-0990]] rendered reason; presentation re-run `qt_home_list_scale_*` (RFC-0028 AC-06, RFC-0032 A6/A7) | Not run; implementation deferred | None |
| AC-03: Authored and directly requested PRs are discovered in saved repositories without prior PR opening, while previously reviewed and explicitly followed PRs retain meaningful membership across request changes and restart. | [[BPR-0988]] recorded search and requested-reviewers exchanges; [[BPR-0989]] membership retention and `sqlite_queue_enrollment_storage` restart | Not run; implementation deferred | None |
| AC-04: Saved repositories, recents, follow membership, and window filters preserve their separate ownership, and external PR opening does not silently enroll its repository or mutate GitHub subscriptions. | [[BPR-0990]] composed course (external link enrolls and saves nothing; Follow; Save repository; zero GitHub mutation); [[BPR-0989]] follow membership; window filters re-run from [[BPR-0783]] | Not run; implementation deferred | None |
| AC-05: Discovery is independent of PR loading, paginated, bounded, and shared across profile windows; a fixture with thousands of unrelated open PRs requires no broad all-PR fetch or full-review hydration. | [[BPR-0990]] composed public-path course with at least 2,000 unrelated open pull requests and two windows; [[BPR-0988]] bounded read; [[BPR-0989]] one scheduler per profile | Not run; implementation deferred | None |
| AC-06: Rate limits, incomplete pages, offline state, auth failures, removed repositories, and stale completions preserve honest availability and cached results without cross-profile/account publication or false membership removal. | [[BPR-0989]] manual-executor host courses; [[BPR-0988]] typed provider failures | Not run; implementation deferred | None |
| AC-07: Canonical PR URLs and supported PR subpages resolve in the receiving profile/window, reuse an existing PR there, and navigate exact supported locations without manual owner/repository/number entry. | [[BPR-0987]]: `application_setup_destination` table, core resolution test, `application_setup_destination_public_path` | Not run; implementation deferred | None |
| AC-08: Unsupported or unavailable sublocations open the resolved PR with an explanation, while malformed or non-PR targets never guess a review, line, or revision. | [[BPR-0987]]: refusal and unavailable-explanation rows of the same three selectors | Not run; implementation deferred | None |
| AC-09: Unknown-server and signed-out navigation preserve the pending destination through explicit server setup and supported sign-in, cancel safely, and send no credentials to unaccepted authorities. | No slice. Re-run `core_setup_destination`, `application_setup_destination`, `application_setup_destination_public_path` ([[BPR-0689]], [[BPR-0732]], [[BPR-0914]], [[BPR-0915]]) | Signed-out navigation now opens a public host without an account ([BPR-0732](../60-done/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md), `application_setup_destination_public_path`); the sign-in continuation is kept for a host that refuses preview | None |
| AC-10: The composed Home, filtering, Refresh, Resume, Open PR, and onboarding flows preserve keyboard/accessibility and responsive layout behavior without adding Qt effects or weakening existing review/preview authority. | [[BPR-0990]] focus-order and scale courses for Refresh, status, Follow and Save repository; existing flows re-run from RFC-0032 A5–A8 and [[BPR-0981]] | The signed-out Home is the card and the bar, focused; the queue and filters appear with an account ([BPR-0732](../60-done/BPR-0732-a-signed-out-home-is-one-paste-field-that-opens-a-public-pull-request.md), `qt_composed_github_scale_*`) | None |

## Completion rules

- Replace planned checks with exact executed commands/courses, results, and
  producing commits as implementation completes; retain not-run rows honestly.
- Inspect all cards linked to this RFC, not just the initial planned pair.
  Every required card must be done and every canonical criterion satisfied.
- Exercise the selected public path and check critical changed-surface findings.
  Use focused evidence; do not infer a full matrix requirement from this card.
- Record target-native or attended evidence in the project's existing workflow.
  Park pending attended work and continue independent work; no approval or live
  GitHub authority comes from registration.
- Run the documentation structure gate and `git diff --check`, reconcile RFC
  and index status, and close only when canonical order and all dependencies
  permit Auto-Verified. No status changes are authorized by this registration.

## Resume

The owner activated the ordered workstream in the canonical index. RFC-BPR-0018,
RFC-BPR-0024 and RFC-BPR-0025 are Auto-Verified; [[BPR-0598]] recorded the
2026-09-22 planning reconciliation and allocated [[BPR-0987]], [[BPR-0988]],
[[BPR-0989]] and [[BPR-0990]], which this closeout now depends on.
[BPR-0598](../60-done/BPR-0598-inventory-personal-discovery-url-flow-and-plan.md) reconciled
RFC-0028's superseding Home presentation, the owner's 2026-09-11 Home
direction and the existing URL/setup behavior before planning new authority
([record](../../rfcs/plans/ux-redesign-pack-0028/rfc-0026-planning-reconciliation.md)).
This closeout becomes startable when the four slices are Done; it then re-runs
the no-slice rows at the closeout head, records superseded parts as
superseded, and reconciles the RFC and index. This record claims no full RFC
acceptance.
