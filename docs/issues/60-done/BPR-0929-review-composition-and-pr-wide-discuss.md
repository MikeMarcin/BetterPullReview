---
schema: "implement-rfc.card.v1"
id: "BPR-0929"
title: "Implement dedicated review composition and PR-wide Discuss"
status: "Done"
rfcs: ["RFC-BPR-0037", "RFC-BPR-0028", "RFC-BPR-0008"]
depends_on: ["BPR-0920", "BPR-0923", "BPR-0926"]
priority: "P1"
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"},"implements":["BPR-0920","BPR-0923","BPR-0926","RFC-BPR-0037:AC-03","RFC-BPR-0037:AC-06","RFC-BPR-0037:AC-09"]}
acceptance: [{"criterion":"Compose review provides usable exact target, type, summary, actions and status at required sizes, fonts and scales with one short-height scroll owner.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Review-editor/workbench tests pass at 1x/1.25x/2x, and the review-submission public path passes with a central retained Compose page. The 1280x900 render shows the complete target/type/body/actions/status without horizontal clipping."}},{"criterion":"Opening, closing and navigation never submit; retained draft and exact authority/reconciliation semantics survive allowed transitions and fail closed on replacement.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Review submission, inline draft, agent-finding workshop and public composition paths pass. Navigation only reparents the existing editors; publication remains explicit and the existing authority, grant, idempotency and reconciliation checks remain the submit path."}},{"criterion":"Discuss is PR-wide without a selected file/comparison and indexes all admitted conversation kinds with stable state and provenance.","outcome":"pass","revision":"e3258509","evidence":{"kind":"command","detail":"Panel/workbench tests admit both files, pathless PR comments, review summaries and inline threads despite absent comparison anchors; roots/replies, kind, resolved/outdated and selected identities remain stable. After the attended note exposed Previous/Next change clearing the selected conversation, e3258509 makes comparison navigation independent of the PR-wide Discuss selection and the three scale rows pass."}},{"criterion":"Optional file, Unread and Unresolved filters are explicit, composable and clear without changing retained code selection.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Focused panel/workbench tests cover named file scope, pathless exclusion explanation, clear filters, independent unread/unresolved filters and selection preservation."}},{"criterion":"Loading, accepted empty, filtered empty, refresh/failure recovery, unavailable capability and authority replacement remain distinct and stale-safe.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Panel clear/adopt and composed fixture tests preserve distinct availability, empty/filter notices and clear prior model/card/map identities before replacement; stale content cannot revive through retained projections."}},{"criterion":"The focus-only Show review threads action is removed and accepted conversations are automatically reachable through Discuss.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Composed and workbench tests assert no reveal action/control remains; Discuss directly presents accepted results in the central surface."}},{"criterion":"Open in code is explicit and uses exact path, side, endpoint and comparison guards without guessing stale or nearby lines.","outcome":"pass","revision":"8939de71","evidence":{"kind":"command","detail":"Regression coverage refuses off-file Reply without dispatch or selection change, hides Open in code for unavailable endpoints, preserves selected identity and asserts the exact right-side line-21 anchor for the line-22 same-file action. Independent correction review found no remaining issue."}},{"criterion":"Keyboard order, focus restoration, accessible state text and inherited native appearances are verified.","outcome":"pass","revision":"8939de71","evidence":{"kind":"observation","detail":"Native candidates rfc0037-discuss-thread-reading and rfc0037-compose-pages received explicit accept decisions e5968d67-c5b5-49e9-b0f0-f869a73737d0 and 7e3a3b36-e1d0-417a-ba8e-7343b5f6ffbd at the reviewed 8939de71/5b8b23dc revision after the requested focus, VoiceOver and light/dark/Increase contrast course. Deterministic tab/focus/accessibility and all scale rows also pass."}}]
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
