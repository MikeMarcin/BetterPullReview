---
schema: "implement-rfc.card.v1"
id: "BPR-1520"
title: "Preserve and recover review notes after failed publication and navigation"
status: "Ready"
rfcs: []
depends_on: []
priority: "P0"
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Major bug. It failed to publish my review notes, possibly because my review wasn't latest? I refreshed and could no longer publish. I opened a different review and came back and all my review notes were gone","severity":"Reported user data loss","suspected_trigger":"Stale review/head is an owner hypothesis, not established fact"}
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
