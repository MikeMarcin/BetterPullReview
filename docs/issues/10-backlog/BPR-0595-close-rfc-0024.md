---
schema: "implement-rfc.card.v1"
id: "BPR-0595"
title: "Verify and close RFC-BPR-0024"
status: "Backlog"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0594"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0024"]
---

# Verify and close RFC-BPR-0024

## Objective

Reconcile the complete accepted behavior in [RFC-BPR-0024](../../rfcs/RFC-BPR-0024-bpr-profiles-multiple-windows.md)
against direct evidence, all linked issue cards, and the final public path.
This closeout depends on the entry card now and must acquire all subsequently
allocated required slice dependencies before it can become startable.

RFC base: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.

Requested RFC set: RFC-BPR-0018, RFC-BPR-0024, RFC-BPR-0025, RFC-BPR-0026; workstream position: 2 of 4.
Planned cards: BPR-0594, BPR-0595. Allocate additional implementation cards only
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
| AC-01: Default, profile creation/rename, profile identification, Open Profile, and New Window are reachable through the shipped application without replacing another profile's windows. | Public profile menu, create/rename, Open Profile and New Window course | Not run; implementation deferred | None |
| AC-02: Two profiles can concurrently use different accounts on the same server with isolated credentials, repositories, caches, epochs, private review data, and preferences. | Concurrent two-profile same-server fixture accounts and partition inspection | Not run; implementation deferred | None |
| AC-03: Windows in one profile share accepted review data and preferences while retaining independent open PRs, navigation, layout, and restoration records. | Two-window shared data and independent navigation/restore course | Not run; implementation deferred | None |
| AC-04: Shared draft/note edit conflicts retain the rejected candidate text and never silently overwrite accepted work; publishing still uses the existing exact-authority mutation path. | Concurrent expected-revision draft/note edits and existing publication-boundary checks | Not run; implementation deferred | None |
| AC-05: Default migration preserves existing appearance, sign-in bindings, private data, and retained history through interruption and restart without duplicate adoption or cross-profile fallback. | Seeded legacy Default migration with interrupted/repeated adoption and fake vault | Not run; implementation deferred | None |
| AC-06: Repeated launches admit one runtime owner per profile; explicit new windows and concurrent different profiles work, and closing one window leaves other consumers usable. | Repeated launches, explicit New Window, owner contention and consumer-aware close | Not run; implementation deferred | None |
| AC-07: Window restoration preserves independent sessions, distinguishes explicit window close from application quit, and exposes the retained Resume destination without fabricating unavailable content. | Quit/close/reopen multi-window sessions, retained Resume and unavailable-content cases | Not run; implementation deferred | None |
| AC-08: Settings and sign-out are scoped to the invoking profile, with one Settings window per profile and no effects or late completions reaching another profile. | Same-profile Settings singleton and sign-out with stale cross-profile completion attempts | Not run; implementation deferred | None |
| AC-09: Typed core/host/Qt boundaries, bounded persistence, stale-result rejection, and failure isolation hold through the composed multi-window application path. | Core replay, manual-executor host and composed multi-window failure/identity tests | Not run; implementation deferred | None |

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

Next task: [BPR-0594](BPR-0594-inventory-profile-window-authority-and-plan.md) after a later explicit implementation
request and the applicable RFC eligibility/dependency checks. No product check
has run, no product slice is complete, and no temporary worktree is retained.
