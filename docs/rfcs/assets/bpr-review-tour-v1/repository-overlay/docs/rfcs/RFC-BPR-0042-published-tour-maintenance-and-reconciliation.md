---
id: "RFC-BPR-0042"
title: "Published Tour Maintenance and Reconciliation"
status: "Proposed"
area: "BPR"
milestone: "Review tours"
depends_on: ["RFC-BPR-0029", "RFC-BPR-0041"]
pack_version: "review-tour-v1"
metadata: {"milestone_plan":{"id":"bpr-review-tour-v1","path":"docs/rfcs/plans/bpr-review-tour-v1/milestone-plan.json"}}
---

# RFC-BPR-0042: Published Tour Maintenance and Reconciliation

## Decision

Publication does not freeze a tour. Authors can reopen the current GitHub PR, change Problem/Solution, insert or reorder stops, revise their explanations, and publish explicit updates while preserving existing comment identities and discussions. This is required manual functionality, not an AI extension.

The canonical shared state is what GitHub currently reports. Local drafts and remote editing baselines do not override website edits. The index defines route membership/order across publication batches. Private preparation history and reading progress are not reconstructed as facts from published prose.

## Reopen and refresh

Read the selected root, its ordered destinations, raw bodies and PR description through RFC-BPR-0040. Distinguish Loading, Complete, Partial, Unavailable, Deleted and Conflict. A missing item in a failed or incomplete read is not deleted. Network failure leaves the last observed text available, visibly stale.

For each editable object use three values: last observed remote baseline B, local draft L, and refreshed remote R. If L equals B, accept R into the observed/draft view without a write. If R equals B, keep L as the local proposed edit. If both changed, attempt only an explicitly supported nonoverlapping span merge; otherwise present B/L/R and preserve all versions until the author resolves the conflict. Do not invoke AI to select a winner.

A merge preserves byte-exact unrelated Markdown. Section mapping is checked again after website changes; a stale source offset cannot overwrite a different section. The root index's non-tour prose and each stop's narrative are not collateral damage from refreshing derived navigation. Multiple windows serialize by draft generation and remote entity version; account/profile replacement invalidates pending reads, AI results and publication intents.

A fresh installation can recover the published tour by normal GitHub data alone. It cannot recover discarded drafts, private rationale notes, AI model identity, original prompt text, author-visit receipts, or a deleted root that GitHub no longer exposes. Missing provenance is Unknown, not inferred from writing style. Provide explicit manual index selection/reconstruction from currently available comments without falsely claiming lost historical order was recovered.

## Edit semantics

| Operation | Required behavior |
|---|---|
| Reorder | Preserve remote comments and replies. Publish a revised index and affected human-readable ordinals/navigation. Do not reorder GitHub's timeline. |
| Insert | Create new comments in a fresh pending batch when supported, then join their actual URLs into the existing root index. The route may span several submitted reviews. |
| Revise | Edit only the owned root comment's body, not replies. If meaning changes after discussion, make the correction visible rather than rewriting history deceptively. |
| Remove from tour | Remove membership/navigation; leave the published comment and discussion intact. An optional ordinary note may explain retirement. Deleting shared content is a separate explicitly destructive action, not needed for this feature. |
| Retarget | Preserve original source binding and discussion. If the remote subject cannot be changed, create a replacement comment at the intended destination, link old/new visibly, and update the index. Never relabel the old thread as if it originally referred to the new code. |
| Split/merge published stops | Compose new local candidates; publish replacements and update membership. Do not merge unrelated GitHub threads or erase replies. |

BPR should not automatically change or remove third-party tour links, permissions, thread resolution, viewed-file state or review disposition. Imported tour roots with unsupported ownership are read-only; the author can prepare their own new route without impersonating the original author.

## Update publication order and honesty

Updates are a resumable multi-operation plan, not atomic. Freeze the intended route/content changes, compute the minimal set of affected objects, and preview those changes. Reuse RFC-BPR-0041's durable per-step journal and actor/head observations. Do not issue no-op updates, patch every comment merely because one stop moved, or recreate existing comments to get a new order.

For insertion, publish new destinations first, then revise affected existing navigation, then update the root index after the destinations exist. For pure reorder, patch affected visible ordinals/footers and publish the new index last. During the sequence some derived navigation may be temporarily stale; the original root remains the authoritative route until its update. BPR must expose progress/partial state and retain old/desired plans. There is no way to make distributed website rendering of all comment edits instantaneously consistent through these separate endpoints.

Keep a valid Tour overview link in every stop throughout the update. It is the website recovery path during a partially completed reorder. Avoid deleting the old destination during replacement. If the root update fails after new comments exist, report the precise partial result and resume from those identities rather than recreating them.

Substantive text changes may use a short visible correction/update note when existing replies would otherwise become misleading. Do not automatically append verbose changelogs to every comment; preserve authors' deliberate wording and ordinary edit history semantics. Do not promise access to a complete version history the provider does not supply.

PR-description edits are narrow body-only operations. Read just before writing, map/merge the requested sections, retain the baseline and proposed complete body, then reconcile the result. Do not invent ETag/If-Match or expected-body compare-and-swap support for a write endpoint that does not document it. Without an evidenced conditional write, disclose the residual concurrent-writer race; local preflight and post-read detection reduce risk but cannot eliminate it. Detected conflict preserves the author's proposed text and never triggers an automatic restoration that might overwrite a newer edit.

## Revision awareness

A tour is authored against an exact comparison/head. Keep original source identity separate from a mapping onto a later revision. Mapping outcomes include Exact original, Moved with unchanged material, Changed material, Ambiguous, Missing and Source unavailable. Symbol/fingerprint heuristics are suggestions, not authority to silently move a public thread.

After head changes, identify potentially affected stops using existing line correspondence and retained epochs. A byte-identical selected snippet does not prove its explanation is still true; callers/contracts elsewhere may have changed. Mark suspected dependencies or changed context for attention. The author must deliberately reaffirm or revise a stop before publishing new claims against the new revision. Do not rewrite existing published comments merely because GitHub marks them outdated.

Returning reviewers can see locally evidenced changes since their last observed tour version: new/removed/reordered stops, changed text, changed source bindings, or unknown history. This is optional enhanced navigation over normal shared comments. It must not invent what a reviewer saw on another machine, imply unchanged means correct, or auto-mark changed stops read. Missing local history produces “No prior local tour version,” not “Nothing changed.”

## Persistence and privacy

Persist only bounded local drafts, canonical remote identifiers, relevant body baselines/desired updates and recovery step state under the correct profile/account. Document the extension from metadata-only ledger assumptions and include it in storage/privacy inventories, purge and retention behavior. Shared GitHub comments are never deleted by profile purge or uninstall. BPR local loss cannot damage them; local unsent work may be lost and must not be presented as server-backed.

Recovery of a known pending update is read-first. On restart show what may already have changed remotely, what is confirmed, and what remains unresolved. No remote edit resumes on launch or refresh without an explicit Resume command. Unknown pending reviews or comments edited by another client need reconciliation, not automatic takeover.

## Acceptance

- **AC42-01:** Existing GitHub tour content repopulates all editors without AI; website changes are reflected without replacing conflicting local drafts.
- **AC42-02:** Pure reorder preserves remote IDs/replies and only updates affected index/navigation text; a published tour can span several batches.
- **AC42-03:** Insert/remove/retarget/split/merge preserve prior shared discussion and correctly distinguish remote subject immutability from local order.
- **AC42-04:** B/L/R reconciliation preserves non-tour text, handles source-span drift, and reports concurrent update races honestly.
- **AC42-05:** Partial update/restart/ambiguous-write tests reuse confirmed destinations and never silently duplicate or roll back shared comments.
- **AC42-06:** Head changes, historical sources, renamed files and ambiguous mappings produce visible author-attention states without invented reanchoring.
- **AC42-07:** Cold reimport and local-purge courses preserve shared tour usability; unknown provenance/history remains unknown.
- **AC42-08:** A website-only user can continue and reply throughout completed updates; BPR reader deltas never mutate review status.

## Verification

Exercise a mixed-client course: publish → reply on website → edit on website → refresh in BPR → reorder and insert → interrupt the update → reopen and reconcile → finish → cold import on an empty local profile. Count individual remote creations, edits and submissions independently. Complete the manual course before the AI milestone; do not let a missing model/provider block it. Each claimed live host/transport needs direct evidence rather than extrapolation from an in-memory fixture.
