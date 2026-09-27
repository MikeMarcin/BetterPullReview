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
| **AC39-01:** An author completes and locally saves Problem → Solution → manually ordered stops → preview with no AI profile, child agent process, or inference request.  | Pass — composed no-agent brief, manual route, preview and real SQLite restart. | `791decea` |
| **AC39-02:** The source editor preserves Markdown; previews are non-mutating and use the existing safe native rendering policy.  | Pass — native raw Markdown/preview and resource-denial negative control. | `791decea; 3b86f047` |
| **AC39-03:** Add/insert/reorder/split/merge/remove operations preserve stable identity, text, related links and ordinary undo where applicable.  | Pass in automation — stable IDs and structural/native undo; standard Cmd+Z, context actions and drag correction verified on Cocoa. Revised human acceptance remains AC39-06. | `791decea; be54b56b` |
| **AC39-04:** A stop can represent an actual source line/range and related unchanged code without inventing native write support.  | Pass — exact inclusive source intervals, related base code and explicit fallback subjects. | `757e8b9b` |
| **AC39-05:** Draft autosave, restart, account/profile isolation and same-draft multiwindow conflicts are exercised with real admitted private storage.  | Pass — admitted SQLite CAS/reopen/killed COMMIT, real profile isolation and visible conflicting copies. | `ae59878e; 3b86f047` |
| **AC39-06:** Current workbench composition is keyboard-accessible and usable at supported sizes/scales; navigation and readiness change no remote review state.  | Pending — original VoiceOver/large layout/responsiveness accepted at hand-back54; keyboard, native undo and ordinary layout rejected and corrected. Three revised candidates await judgment; other claimed native stations remain open. | `28622c5d; be54b56b` |
| **AC39-07:** Manual drafts remain usable when rendering, source acquisition, authentication or later AI assistance is unavailable; missing state is not silently dropped.  | Pass — refused/unsaved text retained; unavailable source/auth/agent does not discard the private draft. | `791decea; 37919b21` |
| **AC39-08:** Bounds and their refusal paths are tested with short, ordinary, long and maximum-admitted tours.  | Pass — core field/encoded limits and actual 1/8/64/256-stop Cocoa route with 257th-stop refusal; measured scope below. | `5ca53796` |
| **AC40-01:** Every shared stop and index is ordinary readable Markdown in normal GitHub comments, with real links and no required BPR-specific metadata.  | Pass in fixture scope — ordinary Markdown root/comments and returned same-PR URLs; live website course is AC40-07. | `cba9064c; 505c942d` |
| **AC40-02:** Index order, comment identity and visible stop numbering remain distinct; duplicate titles and reordered server collections do not scramble the route.  | Pass — paged cold importer distinguishes index order, stable identity and duplicate displayed titles. | `f44c95dd; 3b86f047` |
| **AC40-03:** A fresh local database reconstructs a published tour and its brief from paged GitHub data without AI or original private IDs.  | Pass in fixture scope — fresh admitted profile imports shared raw objects across batches without private provenance or AI; exact manual-span restart has no invented website delta. | `5b9080fb; 28622c5d` |
| **AC40-04:** Markdown round trips preserve all untouched source bytes, including unrelated description/template content and CRLF/LF boundaries.  | Pass — golden spans/newlines and initial-publication preservation correction; full manual course retains unrelated template bytes. | `5ca53796` |
| **AC40-05:** Ambiguous mapping, duplicate/missing/foreign links, partial pages and third-party comments remain visible and do not grant mutation authority.  | Pass — ambiguous/foreign/credential-bearing/missing/partial/mixed-owner links remain inert or read-only; no credential-forwarding fetch. | `3b86f047` |
| **AC40-06:** Line/range/source-version identity and native/file/conversation fallback subjects remain explicit and exact.  | Pass — exact original/mapped source and separate native/file/PR subjects, with qualification refusal. | `757e8b9b; 22fd2150` |
| **AC40-07:** Website-only reviewers can follow Start here → comments → Next/Previous → overview and reply normally; no invented heading fragment is needed.  | Pending — approved safe GitHub.com/GHES target, actual website navigation and independent reviewer reply have not been supplied. | `No live producing revision` |
| **AC40-08:** BPR excursions/return, resolved/outdated threads and source-unavailable states retain the tour without altering review state.  | Pass in fixture scope — real workbench excursions/return, resolved/outdated and unavailable sources; independent receipt/mutation counts unchanged. | `f3232019; 5b9080fb` |
| **AC41-01:** Exact native line/range batches remain pending until one explicit COMMENT submission; no per-stop visible publish loop substitutes for this path.  | Pass in production-port fixture — event omitted during staging, exact COMMENT submission once; live visibility remains unqualified. | `505c942d; 3b86f047` |
| **AC41-02:** Returned comment IDs/URLs drive every link, and complete staged membership/bodies/anchors are verified before submission.  | Pass in production-port fixture — returned IDs/URLs, complete multiset/body/source/actor verification before submission. | `505c942d; 3b86f047` |
| **AC41-03:** Existing/contaminated pending reviews, head/actor changes and external early submission are handled without unauthorized inclusion, deletion or duplication.  | Pass — contaminated/existing/ambiguous pending, changed actor/head and early submission refuse unauthorized effects. | `3b86f047` |
| **AC41-04:** Native HTTPS and admitted `gh api` operations share normalized semantics and preserve their existing credential/authority boundaries.  | Pending — finite HTTPS/CLI normalization and authority canaries pass; exact installed/live host/transport operations are unqualified. | `505c942d; live pending` |
| **AC41-05:** Kill/timeout tests at every durable step prevent blind reposts and recover exact known pending/submitted identities; unresolved outcomes stay unresolved.  | Pass — 24 real process-death journal boundaries and independent server-state counters; unknown outcomes never blindly resend. | `5b9080fb; 3b86f047` |
| **AC41-06:** File-subject staging is qualified per exact operation; unbatchable destinations require an explicit compatibility plan and never an atomicity claim.  | Pending — pending file-thread staging is unavailable; each exact operation needs live qualification. Compatibility fixture preserves explicit non-atomic plan. | `505c942d; live pending` |
| **AC41-07:** Description/backlink failure is separate from tour success; retries do not recreate published comments.  | Pass in fixture scope — description failure remains separate; restart reuses confirmed review/comment identities without recreation. | `505c942d; 5ca53796` |
| **AC41-08:** Pagination, rate limits, denied/unsupported operations, wire budgets, redirects, ambiguous responses and cancellation are exercised on real production ports with fixtures.  | Pass — real provider/session/CLI ports with fixtures cover pages, budgets, foreign continuation, refused/ambiguous writes and cancellation. | `3b86f047` |
| **AC41-09:** Final Preview/Publish and Resume flows are composed in the running application and remote writes occur only after their explicit author commands.  | Pass in fixture scope — real Preview/Publish/Stop/Resume controls and explicit finite authority; production operations remain unavailable. | `505c942d; 5b9080fb` |
| **AC42-01:** Existing GitHub tour content repopulates all editors without AI; website changes are reflected without replacing conflicting local drafts.  | Pass in fixture scope — current website text repopulates editors, overlapping local work persists through restart. | `9a7ba459; 5b9080fb` |
| **AC42-02:** Pure reorder preserves remote IDs/replies and only updates affected index/navigation text; a published tour can span several batches.  | Pass in fixture scope — minimal affected body edits/index last; same IDs/replies and later batches preserved. | `5b9080fb` |
| **AC42-03:** Insert/remove/retarget/split/merge preserve prior shared discussion and correctly distinguish remote subject immutability from local order.  | Pass in fixture scope — local replacements and explicit source retarget preserve old discussions; retired membership leaves replies. | `5b9080fb` |
| **AC42-04:** B/L/R reconciliation preserves non-tour text, handles source-span drift, and reports concurrent update races honestly.  | Pass — B/L/R and fresh span mapping preserve unrelated bytes; no server CAS guarantee claimed. | `9a7ba459; 5ca53796` |
| **AC42-05:** Partial update/restart/ambiguous-write tests reuse confirmed destinations and never silently duplicate or roll back shared comments.  | Pass — actual durable restart and ambiguous maintenance/compatibility responses reuse known destinations; no automatic rollback. | `5b9080fb; 3b86f047` |
| **AC42-06:** Head changes, historical sources, renamed files and ambiguous mappings produce visible author-attention states without invented reanchoring.  | Pass — original versus proposed moved/renamed/changed/missing source; head attention/reaffirmation creates no remote reanchor. | `22fd2150; 5b9080fb` |
| **AC42-07:** Cold reimport and local-purge courses preserve shared tour usability; unknown provenance/history remains unknown.  | Pass in fixture scope — clean-profile cold import and admitted store purge inventory retain shared objects; unchanged manually mapped bytes create no website history. | `ae59878e; 5b9080fb; 28622c5d` |
| **AC42-08:** A website-only user can continue and reply throughout completed updates; BPR reader deltas never mutate review status.  | Pending — actual website continuation/reply across updates needs approved live target and reviewer. Fixture reader deltas produce no review-status effects. | `5b9080fb; live pending` |
| **AC43-01:** No action outside the explicit assistance controls launches AI; all manual features continue without an AI provider.  | Pass — normal preparation/import/settings save/navigation never dispatch AI; actual independent process/mutation counters. | `37919b21; 4b89411c` |
| **AC43-02:** Four separately configurable hooks are versioned, previewable/testable explicitly, resettable and correctly scoped to app profiles.  | Pass — four profile settings/default resources, effective version/reset, saved explicit Test and real store migration. | `37919b21` |
| **AC43-03:** Selection/whole-field/empty-field precedence is exact, including whitespace selection and toolbar focus preservation.  | Pass — real editor toolbar capture, selection including whitespace, whole and empty precedence. | `37919b21` |
| **AC43-04:** Only the captured raw-text interval changes; surrounding Markdown, Unicode and newline bytes are preserved.  | Pass — byte-boundary properties and real process/local editor preserve exact Unicode/CRLF prefix/suffix. | `37919b21; 4b89411c` |
| **AC43-05:** One successful invocation is one distinct undoable edit; undo restores original text/selection, redo replays bytes without inference, and equal/failed results create no entry.  | Pass — actual native history one distinct transaction, original selection restore and zero-inference redo/no-op failure. | `37919b21; 5ca53796` |
| **AC43-06:** Editing, undo-back-to-identical-text, supersession, IME, PR/account/window changes and stale responses cannot overwrite newer work or target a different field.  | Pass in automation scope — generation/owner/profile/window/source guards, IME events, same-byte undo, background and late responses refuse. | `37919b21; 3b86f047` |
| **AC43-07:** Empty fields recover existing PR explanations before inference; unsupported claims are not invented, and no Jira/issue lookup occurs.  | Pass in fixture scope — deterministic brief recovery before inference, bounded source provenance and no linked-ticket lookup; model truth/quality remains AC43-10. | `37919b21` |
| **AC43-08:** Outline output has no bodies; each generated stop requires a visible visit and current-content completion, and per-stop generation cannot run in bulk.  | Pass — strict body-free outline, retained skeletons, actual visible source/current-text completion and no bulk path. | `1ef7a98c; 3b86f047` |
| **AC43-09:** Skill overrides and prompt-injected source/model output cannot widen writes, invoke tools, auto-publish, acknowledge stops or leak credentials.  | Pass in process-fixture scope — malicious source/output, exact restricted argv/environment, zero publication/readiness grant and private diagnostic canaries. Installed enforcement remains unqualified. | `3b86f047; 4b89411c` |
| **AC43-10:** End-to-end native editor tests exercise the real undo path, profile settings and agent delivery seam; canned response tests are not represented as quality evidence for an untested model.  | Pending — actual process/native-widget undo/settings/delivery pass; no installed restricted-provider or real-model quality observation; four original native criteria accepted and three corrected candidates await new verdicts. | `4b89411c; 5ca53796` |

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

## Final reconciliation boundary — mandate r1

All 43 rows now identify direct implementation evidence and its producing source.
The [execution ledger](../../verification/RFC-BPR-0039-0043.md) records exact
commands, later unchanged-source integration, qualification metrics and the final
candidate. A fixture-scoped pass is implementation evidence, not live-host,
installed-model, human or release qualification. BPR-1013 remains open and this
card remains Backlog; no RFC status is advanced.

| RFC | Unmet explicit dependencies at qualification entry |
|---|---|
| RFC-BPR-0039 | RFC-BPR-0036 and RFC-BPR-0037 remain Proposed |
| RFC-BPR-0040 | RFC-BPR-0021 Deferred, RFC-BPR-0022 Proposed, and RFC-BPR-0039 Proposed |
| RFC-BPR-0041 | RFC-BPR-0040 Proposed |
| RFC-BPR-0042 | RFC-BPR-0041 Proposed |
| RFC-BPR-0043 | RFC-BPR-0042 Proposed |

The project now sequences by explicit dependencies. Index position and open
RFC-BPR-0052 are not additional dependencies of this pack. Historical intake
observations remain dated evidence; they do not override the live configuration.
No predecessor acceptance, safe live target, inference account or owner waiver
was supplied in this final milestone.
