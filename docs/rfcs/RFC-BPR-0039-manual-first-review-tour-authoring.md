---
id: "RFC-BPR-0039"
title: "Manual-First Review Tour Authoring"
status: "Proposed"
area: "BPR"
milestone: "Review tours"
depends_on: ["RFC-BPR-0003", "RFC-BPR-0008", "RFC-BPR-0024", "RFC-BPR-0036", "RFC-BPR-0037"]
pack_version: "review-tour-v1"
metadata: {"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# RFC-BPR-0039: Manual-First Review Tour Authoring

## Decision and scope

Implement **Prepare review tour** for an existing pull request, including a draft PR. The complete primary workflow works without an AI provider, AI account, agent launch, issue-tracker integration, or generated prose. The author writes a problem statement, a solution summary, and an ordered route of guiding comments attached to relevant code. Reviewers using only the GitHub website receive the actual tour as ordinary comments and ordinary links, not an export pointing back to BPR.

The product objective is to move reviewer effort from reconstructing intent to evaluating a change. A tour explains context, non-obvious choices, invariants, tradeoffs, uncertainty, and evidence. It does not recite every changed file or present the author's reasoning as proof. Code-owner familiarity is not assumed to include the author's change-specific context. Durable contracts still belong in source/documentation; change-specific explanation and requests for scrutiny belong in the tour.

This RFC is a new additive proposal. It narrowly extends the product contract to author preparation and explicit PR-description-body updates. Creating a PR, marking a draft ready for review, assigning reviewers, changing labels, merging, closing, rebasing, running repository code, and publishing automatically remain outside scope. Merely opening a draft PR must not change its draft status.

The source baseline and identified integration seams are in [SOURCE-AUDIT](assets/bpr-review-tour-v1/supporting/SOURCE-AUDIT.md). Existing implementation facts and this pack's proposed behavior are deliberately distinguished there.

## Primary flow

**Problem → Solution → Tour stops → Preview and publish** is a revisitable roadmap, not a modal sequence of required forms. Opening the preparation surface normally focuses Problem; a selection-based Add tour stop gesture may go directly to that stop. Back/forward navigation, source exploration, and switching between sections retain draft text and selection. Saving locally and navigating publish nothing.

### Problem

Prompt: **What problem does this change solve?** Supporting text: “Describe what was wrong, missing, or unnecessarily difficult before this change.” Use a native Markdown source editor and optional read-only rendered preview. A simple sentence is sufficient when it conveys the actual problem. Do not mandate severity, production impact, incident numbers, headings, bullets, or a minimum length. The exact user example, “Crash when reading data that's been concurrently deleted.”, is a valid starting draft.

### Solution

Prompt: **How does this change solve the problem?** Supporting text: “Explain the central approach and any important limits. Save detailed implementation decisions for the tour.” Provide an independent Markdown editor with the same mechanics. The problem is presented before the solution in both the authoring roadmap and generated description layout. Do not overwrite one field when changing the other.

### Tour stops

The primary action is **Add tour stop**. A stop is a thought, not a file: it has a short title, one primary source destination, a Markdown explanation, and optional related source links. A logical point may involve several files; a file may have several stops. Support current changed lines and contiguous source ranges; unchanged/current, old, or untouched supporting code must remain expressible through the compatibility representations in RFC-BPR-0040. Do not silently broaden existing native-write authority to make a location work.

Selecting source and adding a stop captures a semantic source destination, not a painted diff row or a scroll offset. Keep the code and explanation visible together, with the ordered outline beside them. Authors may add, insert, reorder, duplicate, split, merge, and remove **local** stops. Duplicate creates a new local ID. Split/merge preserve text through undo; neither action silently loses related links. Local stop order is independent of file order and comment creation order.

Owner amendment, attended session `7834eb10dc5260f8cc14257a669c1109` hand-back 70
(2026-09-27): Add and Insert act after the current selection and select the new
stop, including on Redo. Use the stop context menu, with no separate Arrange,
move-up/down, split-at-cursor or arrangement-undo menu entries. Provide drag
handles, multiple selection, Merge for adjacent selected stops, and ordinary
Duplicate/Delete shortcuts. The keyboard reorder alternative remains available;
the split operation's underlying preservation and undo tests remain evidence.
This amendment narrows the visible controls and does not waive native acceptance.

Optional coaching asks: “Where would you start explaining this?”, “What should the reviewer notice?”, “Which decision is not obvious?”, and “What would you like checked?” These are hints, not required form fields. A stop may be informational or explicitly invite scrutiny; neither label changes GitHub's thread-resolution or review-disposition behavior.

## Composition and presentation

Use a task-focused preparation surface reached from Overview and source-selection actions. Fit it into the current RFC-BPR-0037 Compose/Review/Discuss design instead of resurrecting the retired narrow composition drawer or obsolete RFC-BPR-0025 layout. The implementation card must show the final placement using the running workbench. Do not redesign Home, accounts, epochs, source rendering, or general review composition to deliver this feature.

Manual Markdown editing has ordinary undo/redo and native clipboard, selection, keyboard, IME, and accessibility behavior. Preview reuses `qt::markdown_body_view` and the RFC-BPR-0036 policy. A preview is not the publication source. Unsupported HTML/disclosure rendering stays available in source; do not claim pixel parity with GitHub or run HTML to approximate it. GitHub `<details>` content may be authored and preserved; the supported BPR preview must state its own limitations.

The tour outline is keyboard reorderable and not dependent on drag-and-drop. Keep the current stop selected after movement, and expose its new ordinal accessibly. Titles, validation messages, and long code paths wrap or have explicit readable overflow; code retains its whitespace. Validate representative narrow and wide windows, large fonts, and existing theme/display-scale policies.

## Model and ownership

Use stable local `tour_draft_id` and `tour_stop_id` values, distinct from eventual GitHub review/comment IDs. The exact C++ names are implementation choices under the existing strong-ID conventions; these concepts are normative:

| Record | Required meaning |
|---|---|
| Tour draft | Host/repository/PR, acting account, application profile, base/head comparison identity, problem and solution fields, ordered stop IDs, local generation, remote bindings, and draft status. |
| Stop | Local identity, title/body, primary destination, related links, private preparation status, optional remote comment binding, original source binding, and explicit current mapping state. |
| Field baseline | Original raw Markdown, selected source span when imported, remote entity identity, observed version/digest, and local-edit generation. |
| Publication binding | Remote kind, server/repository/PR identity, review/comment ID or opaque node ID, returned web URL, author, and observed body/version. |
| Preparation status | Suggested, Editing, Ready, or Needs attention; applies to the current human-authored or AI-edited text and destination, not to code correctness. |

Host/repository/PR/account identity is immutable for a draft. Switching identity does not rebind a draft; it suspends/cancels scoped operations and retains the old private draft under its original owner. Profile/window generations prevent a completion from reaching a replacement surface.

Core owns deterministic draft operations, ordering, readiness, destination eligibility and stale-completion decisions. Application owns raw-body observations, bounded source acquisition, persistence and serialized command/effect completion. Qt owns text interaction, focus and presentation; it receives no GitHub, database or agent authority. Preserve incremental projections and do not reset a text editor on every acknowledgement.

Use the existing profile-owned private-store and revisioned-draft patterns. Add purpose-specific tour records to an appropriate admitted store; do not build a second collaborative database. Private authored text is an explicit retention class. Raw remote baselines retained for editing/recovery require a documented bounded retention extension; they must not be silently placed in a metadata-only ledger. See [INTEGRATION-MAP](assets/bpr-review-tour-v1/supporting/INTEGRATION-MAP.md).

Autosave bounded local drafts through the existing serialized host lane. A save failure must remain visible and must not claim durable success. Multiple windows use revision-checked edits; a conflicting writer creates a visible conflict, not last-writer-wins corruption. Ordinary source browsing does not require loading every stop's source blob.

## Readiness without bureaucracy

For manual stops, **Save and next** or an equivalent deliberate completion command may mark the current text Ready. No mandatory separate AI proposal acceptance UI exists. Empty skeleton entries and unresolved destinations cannot be published as completed stops. Local incomplete drafts can always be saved.

The final preview shows Problem, Solution, the actual comment route, all proposed remote operations, and any compatibility limitations. A short author checklist is guidance, not a scored gate:

1. Explain the problem before the solution.
2. Give a useful start and conceptual order.
3. Explain non-obvious decisions and relevant constraints.
4. Identify evidence, uncertainty, and requested scrutiny.
5. Follow the route against the revision being sent.

Do not require a stop for every file, a fixed tour length, a time-on-screen quota, or token edits to AI output. Warn about missing essential content or unresolved state; do not manufacture a “review quality” score. A trivial change may have one short stop. An author may prepare only a description without calling it a complete tour.

## Reviewer use

RFC-BPR-0040 defines the shared comment route. BPR may project it into an outline with Previous/Next stop and Return to tour. This must use the current normal conversation surface and navigation history. The reviewer may inspect unmentioned code, follow related locations, or leave the route and return. Do not automatically mark files viewed, accept a change, resolve a conversation, or submit a review from tour navigation. A visited stop is not reviewed code, agreement, or approval.

## Bounds

Initial proposed application ceilings are 256 local stops, 64 KiB UTF-8 per explanation/brief field, and 4 MiB total retained tour prose plus editable baselines. These are safety bounds, not writing targets. Existing smaller provider/transport/persistence limits remain effective until explicitly extended and measured. Serialized Markdown/JSON overhead counts. An oversized batch must be refused or explicitly repartitioned by the author; never silently truncate it or split a promised single submission. Load source lazily under existing Git/Markdown limits. Re-evaluate ceilings from the required scale fixtures rather than presenting unmeasured latency claims.

## Acceptance criteria

- **AC39-01:** An author completes and locally saves Problem → Solution → manually ordered stops → preview with no AI profile, child agent process, or inference request.
- **AC39-02:** The source editor preserves Markdown; previews are non-mutating and use the existing safe native rendering policy.
- **AC39-03:** Add/insert/reorder/split/merge/remove operations preserve stable identity, text, related links and ordinary undo where applicable.
- **AC39-04:** A stop can represent an actual source line/range and related unchanged code without inventing native write support.
- **AC39-05:** Draft autosave, restart, account/profile isolation and same-draft multiwindow conflicts are exercised with real admitted private storage.
- **AC39-06:** Current workbench composition is keyboard-accessible and usable at supported sizes/scales; navigation and readiness change no remote review state.
- **AC39-07:** Manual drafts remain usable when rendering, source acquisition, authentication or later AI assistance is unavailable; missing state is not silently dropped.
- **AC39-08:** Bounds and their refusal paths are tested with short, ordinary, long and maximum-admitted tours.

## Verification and status

Implement the manual path before AI milestones. Use core, host, Qt and actual composition tests, not static source-string matches as substitutes for behavior. Keep formal dependency-ordered RFC closure separate from feature implementation eligibility; this request authorizes this pack's feature work but does not declare predecessor RFCs Auto-Verified. Required native and website observations remain explicitly pending until performed. No application change or runtime result is claimed by the presence of this pack.
