---
id: BPR-0599
title: Verify and close RFC-BPR-0026
status: Backlog
rfc: RFC-BPR-0026
milestone: Post-Pack v2
depends_on: [BPR-0598]
---

# Verify and close RFC-BPR-0026

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0026](../../rfcs/RFC-BPR-0026-personal-review-home-pr-url-navigation.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 4 of 4.
Planned cards: BPR-0598, BPR-0599. Allocate additional implementation cards only
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
| AC-01: Home is reachable and opens according to the profile/window startup contract, with Resume restoring the exact retained review/tab without destroying other sessions. | Profile startup, Home/Resume and new/restored-window public course | Not run; implementation deferred | None |
| AC-02: Home deduplicates PRs and explains attention, waiting, and quiet states; authored PRs merely awaiting another reviewer do not inflate the user's attention count, and unknown detail is not presented as quiet. | Core attention/role fixtures plus rendered section, reason and count assertions | Not run; implementation deferred | None |
| AC-03: Authored and directly requested PRs are discovered in saved repositories without prior PR opening, while previously reviewed and explicitly followed PRs retain meaningful membership across request changes and restart. | Author/direct-request search fixtures with reviewed/followed membership and restart | Not run; implementation deferred | None |
| AC-04: Saved repositories, recents, follow membership, and window filters preserve their separate ownership, and external PR opening does not silently enroll its repository or mutate GitHub subscriptions. | Save/remove repo, recent external PR, follow/unfollow and independent window filters | Not run; implementation deferred | None |
| AC-05: Discovery is independent of PR loading, paginated, bounded, and shared across profile windows; a fixture with thousands of unrelated open PRs requires no broad all-PR fetch or full-review hydration. | Recording provider with thousands of unrelated PRs and two-window scheduler accounting | Not run; implementation deferred | None |
| AC-06: Rate limits, incomplete pages, offline state, auth failures, removed repositories, and stale completions preserve honest availability and cached results without cross-profile/account publication or false membership removal. | Manual-executor partial/offline/rate/auth/repo-removal and stale-generation tests | Not run; implementation deferred | None |
| AC-07: Canonical PR URLs and supported PR subpages resolve in the receiving profile/window, reuse an existing PR there, and navigate exact supported locations without manual owner/repository/number entry. | URL normalization tables and public Open PR gestures for canonical and supported locations | Not run; implementation deferred | None |
| AC-08: Unsupported or unavailable sublocations open the resolved PR with an explanation, while malformed or non-PR targets never guess a review, line, or revision. | Malformed/non-PR input and unavailable exact target explanation course | Not run; implementation deferred | None |
| AC-09: Unknown-server and signed-out navigation preserve the pending destination through explicit server setup and supported sign-in, cancel safely, and send no credentials to unaccepted authorities. | Unknown-host acceptance/cancel, signed-out continuation and recording transport/vault checks | Not run; implementation deferred | None |
| AC-10: The composed Home, filtering, Refresh, Resume, Open PR, and onboarding flows preserve keyboard/accessibility and responsive layout behavior without adding Qt effects or weakening existing review/preview authority. | Rendered Home/Refresh/filter/Resume/Open PR/setup keyboard and responsive-layout course | Not run; implementation deferred | None |

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

Next task: [BPR-0598](BPR-0598-inventory-personal-discovery-url-flow-and-plan.md) after a later explicit implementation
request and the applicable RFC eligibility/dependency checks. No product check
has run, no product slice is complete, and no temporary worktree is retained.
