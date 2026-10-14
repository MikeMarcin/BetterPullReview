---
schema: "implement-rfc.card.v1"
id: "BPR-1520"
title: "Preserve and recover review notes after failed publication and navigation"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P0"
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Major bug. It failed to publish my review notes, possibly because my review wasn't latest? I refreshed and could no longer publish. I opened a different review and came back and all my review notes were gone","severity":"Reported user data loss","suspected_trigger":"Stale review/head is an owner hypothesis, not established fact","milestone_plan":{"id":"bpr-1520-review-note-preservation","path":"docs/rfcs/plans/bpr-1520-review-note-preservation/milestone-plan.json"}}
---

# BPR-1520: Preserve and recover review notes after failed publication and navigation

Owner observed failed publication of review notes, then Refresh removed publication availability. After opening a different review and returning, all review notes were missing. Treat this as a critical local-data preservation and publication recovery bug. The owner suspects an outdated review, but the actual trigger is unknown.

## Acceptance

- AC1: Trace and reproduce failed publish -> refresh -> switch review -> return with controlled stale-head, rejected and indeterminate publication cases; establish which local draft/notes surface the report corresponds to without assuming the hypothesis.
- AC2: Unpublished user-authored notes remain durably recoverable across failure, refresh, review switching and restart. Invalidation of publishing authority must not delete or silently overwrite user content.
- AC3: Stale/unavailable publish state gives a coherent path to preserve, reconcile or explicitly rebind user content without silently publishing to another head/review or discarding it.
- AC4: Ambiguous publication is reconciled against server truth without duplicate writes; confirmed publication remains distinguishable from retained unpublished material.
- AC5: Exercise the composed editing/publish/refresh/navigation/restart path and failure injection, asserting exact content and authority durability. Include a regression for navigation away after failure.
- AC6: Investigate whether existing stored content is recoverable through product-owned state, using bounded read-only inspection or an explicit safe snapshot. Do not alter/purge/migrate the owner’s live profile during diagnosis. Report recovery evidence and limits honestly; never infer recoverability from a fix for future drafts.

## Safety and coordination

Highest-priority testing note; dispatch before queued polish/navigation cards. Use fixtures for publication failures and designated BPR-Test only if live validation is essential. No mutation of the owner’s real review, no publishing on their behalf. Never log credentials/private repository content or raw user notes into committed artifacts. Inspect draft/persistence/publication contracts and relevant RFCs. Load frontend-design:frontend-design before any UX edits. Never operate/capture owner desktop or inspect browser personal data.

## Owner clarification

The lost content was **file/line comment drafts** (owner reply, 2026-10-14). The exact Publish error remains unknown. Investigate inline draft durability first.

## Restart-persistent commenting failure

Owner additionally reports that creating comments remains unavailable even after reopening the app. Their supplied screenshot shows the status message: “No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment.” Treat this as part of this P0 investigation, without presuming a shared root cause is already proven.

- AC7: After the failed-publish/refresh/navigation sequence and app restart, an otherwise eligible active pull request/account can create and edit a new inline draft, or has a precise actionable recovery path. Resolve durable draft/publication state without losing prior content or bypassing authority checks.

The screenshot is transient owner evidence; do not commit it or its private source-code content. No owner profile has been inspected or changed by the coordinator.

## Owner recovery and retention amendment

The owner reported recovering the comments themselves with a SQLite database browser. They observed very old already-published comments and many rows with no comment text, and asked whether the table grows forever. This is owner-reported recovery, not recovery performed or independently inspected by the agent. No owner profile has been read, changed or migrated during this work.

- AC8: Incidental source-line selection creates no needless durable blank rows. Retention has explicit bounded active and historical allowances, preserving authored text and publication lineage. Existing published history must not permanently consume the 64 active comment slots; any cleanup of legacy empties is explicit and excludes authored, pending and referenced records.

The exact original Publish error remains unknown. Owner recovery does not itself resolve comment creation or establish that a stale head caused the failure.

## Diagnosis and implementation

The old schema counted every draft record against a lifetime limit of 64 per complete profile/incarnation/host/account/repository/PR scope. Ordinary line selection persisted empty rows; published and discarded history also consumed this allowance. Quota refusal survived restart. Separately, current-head restoration hid earlier-head records, and whole-review confirmation settled only the current-head shared cache. A confirmed earlier-head comment could therefore remain locally marked publishing.

The fix keeps ordinary empty line selections in runtime state and atomically creates a normal durable identity with the first actual edit. Saving failure retains the editor candidate with a retry path; typing during first save is coalesced onto the committed identity. Schema 9 preserves all rows, identity frontiers, bindings and lineage while separating 64 active authored drafts from 4,096 retained records within the existing 64 MiB payload bound. Authored and publication history has no automatic age expiry. Explicit cleanup removes only never-edited empty ordinary placeholders, checks references and races transactionally, and prevents late restoration from resurrecting them. An explicit local discard frees an active slot while preserving text.

Finish review > Saved comments exposes retained text under its original scope and head, with distinct unpublished, published, discarded and unconfirmed states. Local recovery carries the complete private scope and selected row revision, so a head update does not silently swallow confirmation. Whole-review settlement uses the original held key and exact frozen comment correlation on the durable lane across retained heads. Restore discovers held keys before current-head filtering; existing publication lookup and reconciliation remain read-only. No publication grant or silent retargeting is added.

The owner's recovered content was reported by the owner; no agent inspected, changed, migrated or purged the owner profile. Storage location and safe backup instructions are in the storage guide. The original Publish error and exact original incident trigger remain unknown. These reproduced mechanisms are not forensic proof of that trigger.

## Verification and source review

- Dev application and focused test targets built with warnings treated as errors. Core-only revisioned-draft build/test passed.
- Nine focused tests passed: both architecture gates, core revisioned draft, shared draft host, review-ledger storage, private storage, private host, draft editor and private review panel. New host cases cover atomic first-save failure, no-op rejection, exact CRLF/UTF-8 text, migration, active/history capacity, cleanup races and stale restores, actor/head/prose settlement refusals and failed local settlement followed by restart.
- The full `application_label_publication_recovery` course passed: rejected/lost-response whole review, refresh, PR17/18/17 navigation, restart with a head advance, exact original text, original-key GET-only confirmation and one POST; 200 selections allocate no rows, first-save typing persists after restart, pending Publish sends nothing and is not replayed, and explicit cleanup preserves authored content.
- `application_start_review_contract` and `application_review_round_trip_composition_contract` passed, including separate-process restart. Clang-format 22 check, documentation gate and 67 documentation tests passed. Offscreen native dialog/window renders were inspected; no owner desktop was operated or captured.
- Self-review covers baseline `9f2ebc10a` through this product slice; no intervening pinned-scope source commits. The changed production app manifest paths are `src/app/src/github_application.cpp`, `src/app/src/private_review_workflow.cpp` and `.hpp`. Durable-before-projection ordering, original scope/head/content correlation, cancellation fences and existing sole mutation host were reviewed. Full-app and manifest pins were resealed; the production mutation-branch digest remains unchanged.
- The required frontend-design skill was read before UX edits. No live GitHub calls or mutations were needed; fixtures use only synthetic content. Windows/Linux native runtime evidence is not claimed.

The durable checkpoint records the candidate, commands, verification and integration handoff. Main integration belongs to the coordinator.
