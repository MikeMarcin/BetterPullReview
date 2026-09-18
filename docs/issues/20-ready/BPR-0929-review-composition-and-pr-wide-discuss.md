---
schema: "implement-rfc.card.v1"
id: "BPR-0929"
title: "Implement dedicated review composition and PR-wide Discuss"
status: "Ready"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028", "RFC-BPR-0008"]
depends_on: ["BPR-0920", "BPR-0923", "BPR-0926"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"},"implements":["BPR-0920","BPR-0923","BPR-0926","RFC-BPR-0037:AC-03","RFC-BPR-0037:AC-06","RFC-BPR-0037:AC-09"]}
---

# Implement dedicated review composition and PR-wide Discuss

This is the coupled runtime follow-up selected by [[BPR-0920]], [[BPR-0923]]
and [[BPR-0926]] for RFC-BPR-0037 AC-03, AC-06 and AC-09. The evidence and
selected interaction are in
[`conversation-surface-layout-study.md`](../../rfcs/plans/rfc-bpr-0037-first-pr-workflow/conversation-surface-layout-study.md).

## Selected design and scope

Move whole-review composition out of the permanent Conversations drawer into
one dedicated in-window **Compose review** page. Keep the same editor/workflow
instance alive, open it explicitly from Review, Read or Discuss, and restore
the invoking place and focus on Back. Give inline/reply drafting an explicitly
opened surface rather than compressing the body and workshop fields under one
220-pixel maximum.

Make Discuss a PR-wide main-content conversation surface by default, independent
of file selection or comparison availability. Keep an explicit optional file
scope plus Unread and Unresolved filters. The selected detail owns complete
bodies; the graph summarizes/navigates identity without duplicating full text.
BPR-0924 and BPR-0925 retain detailed body sizing, input routing and graph/detail
integration ownership.

## Acceptance

- At 900×900 and 1400×900 windows with 13/20 point fonts and supported display
  scales, Compose review gives the exact target, review type, summary, actions
  and status usable space without horizontal escape, unexplained gaps or
  compressed 23-pixel editors. Short-height fallback has one intentional outer
  scroll owner and a usable summary viewport.
- Opening, closing and navigating never submits. Draft text, type, cursor,
  accepted/candidate/conflict state and recovery identity survive mode changes,
  same-authority refresh and terminal reconciliation. PR/account/head
  replacement keeps the existing fail-closed rules. Submit still uses explicit
  confirmation, exact authority, one-use grants, idempotency and reconciliation.
- Entering Discuss with no selected file or no available comparison shows all
  admitted PR conversations. The index includes top-level PR comments, review
  summaries and inline threads/replies with kind, resolved/outdated state and
  stable identity. Private/local provenance remains distinct.
- Optional file scope is visibly named, explains which pathless content it
  excludes and clears without changing code selection. Unread and Unresolved
  are additional filters off by default. Off-file selection and returning from
  code preserve the PR-wide scope and selected conversation.
- Initial loading, accepted empty, filtered empty with Clear filters,
  refreshing, failed refresh with recovery, unavailable capability and
  PR/account replacement are distinct. Prior-authority content clears before
  replacement and stale completions cannot repopulate it.
- Remove the confusing focus-only **Show review threads** drawer action. Discuss
  automatically reveals accepted results; any retained shortcut is labelled
  **Open discussions** and routes to the same state. No silent no-op remains.
- **Open in code** is explicit and preserves exact endpoint/comparison guards.
  Outdated, removed or unavailable anchors leave discussion readable with a
  specific reason; no nearby line or stale source is guessed.
- Keyboard order, focus restoration, accessible state text and inherited
  light/dark/high-contrast presentation are verified. Record supported
  target-native layout/input observations separately from deterministic tests.

## Exclusions

Do not change authentication, provider/mutation authority, review semantics,
Markdown policy or cross-PR unsent-draft persistence. Do not duplicate the
thread-body/graph work owned by BPR-0924/BPR-0925.

## Status

Allocated by RFC-BPR-0037's overview/conversation design milestone. Runtime
implementation and native evidence remain pending.
