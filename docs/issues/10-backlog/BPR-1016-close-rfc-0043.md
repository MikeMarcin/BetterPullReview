---
schema: "implement-rfc.card.v1"
id: "BPR-1016"
title: "Verify and close RFC-BPR-0043"
status: "Backlog"
rfcs: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
depends_on: ["BPR-1013", "BPR-1015"]
closeout_for: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0043"]
metadata: {"pack":"bpr-review-tour-v1","pack_alias":"PACK-TOUR-26","milestone":"qualification-closeout","acceptance":[],"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# BPR-1016: Verify and close RFC-BPR-0043

Pack title: "Reconcile all five RFC acceptance matrices and close in order". The repository names a closeout card after one RFC; this single authored card closes RFC-BPR-0039 to RFC-BPR-0043 in canonical order and is named after the last.

Pack alias: **PACK-TOUR-26** (live `BPR-1016`, allocated at import).

RFC base: `3e12138ae8c603315b3d55c34872d7a4fc777a71`.

## Closeout checklist

| Criterion | Result | Revision |
|---|---|---|
| **AC39-01:** An author completes and locally saves Problem → Solution → manually ordered stops → preview with no AI profile, child agent process, or inference request. | Not run; import only | No producing revision |
| **AC39-02:** The source editor preserves Markdown; previews are non-mutating and use the existing safe native rendering policy. | Not run; import only | No producing revision |
| **AC39-03:** Add/insert/reorder/split/merge/remove operations preserve stable identity, text, related links and ordinary undo where applicable. | Not run; import only | No producing revision |
| **AC39-04:** A stop can represent an actual source line/range and related unchanged code without inventing native write support. | Not run; import only | No producing revision |
| **AC39-05:** Draft autosave, restart, account/profile isolation and same-draft multiwindow conflicts are exercised with real admitted private storage. | Not run; import only | No producing revision |
| **AC39-06:** Current workbench composition is keyboard-accessible and usable at supported sizes/scales; navigation and readiness change no remote review state. | Not run; import only | No producing revision |
| **AC39-07:** Manual drafts remain usable when rendering, source acquisition, authentication or later AI assistance is unavailable; missing state is not silently dropped. | Not run; import only | No producing revision |
| **AC39-08:** Bounds and their refusal paths are tested with short, ordinary, long and maximum-admitted tours. | Not run; import only | No producing revision |
| **AC40-01:** Every shared stop and index is ordinary readable Markdown in normal GitHub comments, with real links and no required BPR-specific metadata. | Not run; import only | No producing revision |
| **AC40-02:** Index order, comment identity and visible stop numbering remain distinct; duplicate titles and reordered server collections do not scramble the route. | Not run; import only | No producing revision |
| **AC40-03:** A fresh local database reconstructs a published tour and its brief from paged GitHub data without AI or original private IDs. | Not run; import only | No producing revision |
| **AC40-04:** Markdown round trips preserve all untouched source bytes, including unrelated description/template content and CRLF/LF boundaries. | Not run; import only | No producing revision |
| **AC40-05:** Ambiguous mapping, duplicate/missing/foreign links, partial pages and third-party comments remain visible and do not grant mutation authority. | Not run; import only | No producing revision |
| **AC40-06:** Line/range/source-version identity and native/file/conversation fallback subjects remain explicit and exact. | Not run; import only | No producing revision |
| **AC40-07:** Website-only reviewers can follow Start here → comments → Next/Previous → overview and reply normally; no invented heading fragment is needed. | Not run; import only | No producing revision |
| **AC40-08:** BPR excursions/return, resolved/outdated threads and source-unavailable states retain the tour without altering review state. | Not run; import only | No producing revision |
| **AC41-01:** Exact native line/range batches remain pending until one explicit COMMENT submission; no per-stop visible publish loop substitutes for this path. | Not run; import only | No producing revision |
| **AC41-02:** Returned comment IDs/URLs drive every link, and complete staged membership/bodies/anchors are verified before submission. | Not run; import only | No producing revision |
| **AC41-03:** Existing/contaminated pending reviews, head/actor changes and external early submission are handled without unauthorized inclusion, deletion or duplication. | Not run; import only | No producing revision |
| **AC41-04:** Native HTTPS and admitted `gh api` operations share normalized semantics and preserve their existing credential/authority boundaries. | Not run; import only | No producing revision |
| **AC41-05:** Kill/timeout tests at every durable step prevent blind reposts and recover exact known pending/submitted identities; unresolved outcomes stay unresolved. | Not run; import only | No producing revision |
| **AC41-06:** File-subject staging is qualified per exact operation; unbatchable destinations require an explicit compatibility plan and never an atomicity claim. | Not run; import only | No producing revision |
| **AC41-07:** Description/backlink failure is separate from tour success; retries do not recreate published comments. | Not run; import only | No producing revision |
| **AC41-08:** Pagination, rate limits, denied/unsupported operations, wire budgets, redirects, ambiguous responses and cancellation are exercised on real production ports with fixtures. | Not run; import only | No producing revision |
| **AC41-09:** Final Preview/Publish and Resume flows are composed in the running application and remote writes occur only after their explicit author commands. | Not run; import only | No producing revision |
| **AC42-01:** Existing GitHub tour content repopulates all editors without AI; website changes are reflected without replacing conflicting local drafts. | Not run; import only | No producing revision |
| **AC42-02:** Pure reorder preserves remote IDs/replies and only updates affected index/navigation text; a published tour can span several batches. | Not run; import only | No producing revision |
| **AC42-03:** Insert/remove/retarget/split/merge preserve prior shared discussion and correctly distinguish remote subject immutability from local order. | Not run; import only | No producing revision |
| **AC42-04:** B/L/R reconciliation preserves non-tour text, handles source-span drift, and reports concurrent update races honestly. | Not run; import only | No producing revision |
| **AC42-05:** Partial update/restart/ambiguous-write tests reuse confirmed destinations and never silently duplicate or roll back shared comments. | Not run; import only | No producing revision |
| **AC42-06:** Head changes, historical sources, renamed files and ambiguous mappings produce visible author-attention states without invented reanchoring. | Not run; import only | No producing revision |
| **AC42-07:** Cold reimport and local-purge courses preserve shared tour usability; unknown provenance/history remains unknown. | Not run; import only | No producing revision |
| **AC42-08:** A website-only user can continue and reply throughout completed updates; BPR reader deltas never mutate review status. | Not run; import only | No producing revision |
| **AC43-01:** No action outside the explicit assistance controls launches AI; all manual features continue without an AI provider. | Not run; import only | No producing revision |
| **AC43-02:** Four separately configurable hooks are versioned, previewable/testable explicitly, resettable and correctly scoped to app profiles. | Not run; import only | No producing revision |
| **AC43-03:** Selection/whole-field/empty-field precedence is exact, including whitespace selection and toolbar focus preservation. | Not run; import only | No producing revision |
| **AC43-04:** Only the captured raw-text interval changes; surrounding Markdown, Unicode and newline bytes are preserved. | Not run; import only | No producing revision |
| **AC43-05:** One successful invocation is one distinct undoable edit; undo restores original text/selection, redo replays bytes without inference, and equal/failed results create no entry. | Not run; import only | No producing revision |
| **AC43-06:** Editing, undo-back-to-identical-text, supersession, IME, PR/account/window changes and stale responses cannot overwrite newer work or target a different field. | Not run; import only | No producing revision |
| **AC43-07:** Empty fields recover existing PR explanations before inference; unsupported claims are not invented, and no Jira/issue lookup occurs. | Not run; import only | No producing revision |
| **AC43-08:** Outline output has no bodies; each generated stop requires a visible visit and current-content completion, and per-stop generation cannot run in bulk. | Not run; import only | No producing revision |
| **AC43-09:** Skill overrides and prompt-injected source/model output cannot widen writes, invoke tools, auto-publish, acknowledge stops or leak credentials. | Not run; import only | No producing revision |
| **AC43-10:** End-to-end native editor tests exercise the real undo path, profile settings and agent delivery seam; canned response tests are not represented as quality evidence for an untested model. | Not run; import only | No producing revision |

## Outcome and work

Map every AC39–AC43 requirement to direct evidence and producing revision, distinguish implementation completion from host/platform/human qualification, and close each RFC only under current canonical predecessor rules. Preserve explicit open rows and named blockers. Keep the original offline plan and alias mappings, and record justified execution adjustments without erasing plan lineage.

## Existing ownership seams

`Live RFC index, mapped issue board, milestone/acceptance evidence and verification ledgers`

Read the mapped RFCs and nearest current `AGENTS.md`. These are seams to extend, not permission to change unrelated work or copy embedded worktree versions. Resolve renamed paths in the one intake note.

## Direct acceptance

All acceptance IDs accounted for with no phantom passes; installed skill/board validators succeed. No Auto-Verified status is created while required rows or predecessor eligibility remain unresolved.

## Integration boundary

Milestone: **qualification-closeout**. Preserve “the core decides; the host performs; Qt presents.” Use the existing application, profile, provider, agent, storage and composition paths; no stand-alone demo that bypasses the actual product.

Run focused tests for this change and record exact commands, results and producing revision. Tests use current isolated fixtures by default. A live company PR, real credentials, arbitrary external command, paid service call or remote publication is not authorized by this card. Designated live/native acceptance follows its existing separate authority path. Missing evidence remains explicit rather than converted into a passing mock or an invented verdict.
