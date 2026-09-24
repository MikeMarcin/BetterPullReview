---
id: "RFC-BPR-0045"
title: "Shared Local Tour Drafts and Cross-Interface Recovery"
status: "Proposed"
area: "BPR"
milestone: "Tour CLI and pr-tour skill"
depends_on: ["RFC-BPR-0039", "RFC-BPR-0040", "RFC-BPR-0041", "RFC-BPR-0042", "RFC-BPR-0044"]
pack_version: "pr-tour-cli-extension-v1"
metadata: {"milestone_plan":{"id":"bpr-pr-tour-cli-extension-v1","path":"docs/rfcs/plans/bpr-pr-tour-cli-extension-v1/milestone-plan.json"}}
---

# RFC-BPR-0045: Shared Local Tour Drafts and Cross-Interface Recovery

## Decision

Make an unfinished tour transferable among desktop, terminal, and `pr-tour` on the same machine, under the same BPR profile and author identity, without export/import and without uploading it. The skill never owns a separate draft. Make the published tour recoverable on a fresh machine from ordinary GitHub material through the parent's importer, not through copied application state.

These are different guarantees. GitHub is canonical for published content; the profile-private store is canonical for unsent work. A GitHub PENDING review is remote staging data created by an explicitly authorized publication operation, not the general-purpose draft store. This clarification extends, rather than replaces, RFC-BPR-0039–0042. [RECOVERY-MATRIX](../../../supporting/RECOVERY-MATRIX.md) states exactly what survives each boundary.

## Published recovery: no machine-local dependency

Opening the PR on a fresh machine or clean profile, with sufficient read access, recovers the Problem/Solution material exposed in the current PR description, the selected review-summary index, ordered stop membership, current comment bodies/titles, related visible links, source/subject bindings exposed by GitHub, and available discussion. IDs and links are interpreted by the same provider/parser as the desktop. `pr-tour` calls that importer through the CLI; it does not summarize GitHub into a substitute tour.

Do not publish new metadata to achieve recovery. No BPR marker, hidden JSON/HTML, custom URL, gist, repository branch, attachment, hosted manifest, bot, or companion service is introduced. Exact publication format is inherited from RFC-BPR-0040. A root plus its ordered links remains the route authority across multiple review batches. Browser edits are accepted observations with preserved local conflicts, never overwritten because the local cache was older.

Fetch completeness is part of the result: complete, partial, unavailable, deleted-after-confirmation, ambiguous, and stale-cached remain distinct. Page through advertised collections within explicit limits; an unfinished page set or permission failure cannot be treated as an empty tour. A missing or inaccessible blob retains the code reference and reports unavailable source rather than retargeting it. Current Markdown is recoverable; complete historical body versions or vanished source objects are not promised.

Several plausible indexes require explicit root selection, not choosing the most recent review by timestamp. Wrong-author/cross-PR links and malformed roots remain visible for resolution. Native and fallback comment kinds retain their canonical namespace; remote integer IDs alone are not globally unique. A derived local stop ID is a convenience, never a replacement for its host/repository/PR/comment identity.

Cold import does not recreate private notes, abandoned suggestions, local history/undo, unpublished text, AI model/effort provenance, author-visit receipts, reading progress, credentials, or prompt overrides. Display these as unknown/not present, not inferred. Viewing an already published tour must not require re-acknowledging every historic stop. New or changed local material still follows current readiness/publication rules. No read or cold import writes to GitHub.

## Local WIP and staging distinctions

Normal text, outline, state, order, readiness and source decisions autosave locally. Ordinary preparation issues zero remote writes. An explicit parent-plan publication can create private pending GitHub comments to obtain actual URLs before final submission; those bytes have left the machine even though the review is not yet visible to collaborators. GitHub documents author-only pending line comments and explicit submission (GH-REVIEW-UI/GH-REVIEWS in [EXTERNAL-AUTHORITY](../../../references/EXTERNAL-AUTHORITY.md)).

An interrupted upload may leave a pending review and a local journal. An authenticated author on another machine may be able to read the staged material, but that does not recover the complete local WIP state or prove ownership of a lost publication attempt. Discover pending material as a candidate; do not adopt, delete, edit, or submit an unrelated pending review automatically. Private staging is not an autosave/synchronization feature. Existing permission, capability, and competing-pending-review restrictions are retained.

Use precise status wording: Saved locally; Local changes not published; Staged on GitHub; Outcome unknown; Published; Partially updated; Recovered from GitHub. Pending server reads do not warrant “fully synced.” Profile purge removes local work only; it must not discard server reviews and must disclose unresolved remote staging before losing local receipts.

## One profile owner; more than one presentation

Reuse the existing profile-private admitted store and runtime lease. Do not open another SQLite writer from a CLI/skill while the desktop owns the profile. New store/schema version numbers must be allocated from live code, not invented in this pack.

When no owner exists, the CLI acquires the established exclusive lease, executes a bounded tour service transaction, commits, and releases. Do not retain this lease while waiting for user input, an external editor, a pager, or a language-model response. Interactive `walk` is a succession of short commands, not a permanent runtime daemon. A long remote operation may retain ownership while in flight; another startup gets bounded progress/explicit busy behavior rather than starting a second owner.

When the desktop owns the profile, the CLI forwards a typed tour request to that owner over an authenticated owner-private local command channel. The owner validates exact target/generations and runs the same service on its serialized lane. This is a narrow new command protocol, not permission to reinterpret the current fixed activation frames as arbitrary requests. Version/negotiate a separate message type or endpoint; old activation behavior must keep working. No TCP/HTTP listener or always-running helper is required.

Both sides bind OS-user identity, canonical profile root/identity, owner instance, protocol version, request ID, and permitted command type. Peer-user authentication excludes other users, not malicious code already running as the same user. A PID, socket pathname, JSON actor string, or request ID is not authentication. Exact parent publication authorization remains separate from transport admission. The channel cannot execute arbitrary processes, URLs, SQL, or generic provider operations.

Standalone/forwarded local mutations share a durable request-deduplication record: same ID and same canonical payload returns the stored outcome; same ID with another payload refuses. If connection loss makes delivery uncertain, query that request under the current owner before considering replay. Never blindly rerun creation, ready, undo, or publish. Lifetime/retention and unknown-after-compaction results are explicit. Remote operation uncertainty still uses RFC-BPR-0041's journal; a local IPC receipt is not proof that GitHub accepted a comment.

Forwarding while the desktop remains open is in scope for this pack. A busy/unsupported-bridge diagnostic is a compatibility fallback for an older owner, not acceptance of the new normal flow. Exact behavior and rejection paths are in [LOCAL-HANDOFF](../../../specs/LOCAL-HANDOFF.md).

## Edits, autosave, and undo across presentations

Keep one bounded application-owned semantic edit journal for committed tour fields and structure. This narrowly extends the parent's local editor transaction contract to survive presentation changes. Native editor keystroke grouping may exist as an uncommitted editing buffer, but must not become an independent competing history of committed tour state. Do not change ordinary non-tour editor behavior.

Each committed operation records before/after bytes or bounded reversible deltas, exact field/structure revision, relevant selection hints, original source binding, operation kind, origin presentation, and optional known provenance. A successful explicit AI rewrite remains one undoable transaction. Redo replays stored bytes with zero AI/network calls. No-op/failure/stale responses add no history. Revisions always advance, including undo-back-to-identical-text, preventing ABA-style acceptance of old edits.

The desktop saves incrementally. Before another presentation reads/mutates the draft, the owner flushes pending ordinary edits on the owning lane, or reports `editor_busy` for incomplete IME composition; it must not silently force-commit an IME sequence. A delayed response carries a baseline; if the user types while it is outstanding, compare revisions and preserve both texts on conflict. Applying a remote projection must not steal focus or replace a dirty editor. CLI external-editor save uses the same compare-and-set rule and retains the edited candidate if stale.

Two simultaneous local editors can both inspect, but cannot silently overwrite a shared field. The first accepted revision wins admission, not truth; the second keeps its text in a conflict candidate with a readable remedy. Nonconflicting fields may proceed if their own revisions and required structure context still match. Structural actions validate the outline revision and stable stop IDs rather than ordinal positions. Undo from another surface targets the latest reversible committed transaction for this draft and displays its label; it never reverses a published GitHub operation.

Persist minimal workflow position (current section and stop) as an optional resumption hint. It is not a global mutation target, source identity, proof of an author visit, or reason to move another active presentation. Keep private preparation acknowledgements tied to current content/destination. Generated material changed by another interface loses readiness. Preserve source/draft IDs and bindings when moving interfaces.

Bound journal history within an explicit documented storage budget. Eviction can remove old reversible history with a visible undo limit, but never current text, conflict candidates still awaiting resolution, or unresolved publication records. Keep inherited effective content/retention limits; an extension does not silently widen them.

## Acceptance

- **AC45-01:** Cold open from only GitHub reconstructs all available shared content/order/bindings in desktop, CLI and skill without local metadata or AI.
- **AC45-02:** Missing private context is reported as unknown; complete/partial/ambiguous/deleted/stale observations never collapse to empty or fabricated content.
- **AC45-03:** Local WIP saves and cross-interface switches produce zero GitHub writes; staged and submitted material have visibly distinct status and recovery rules.
- **AC45-04:** The same unfinished draft moves CLI → desktop → skill → CLI, retaining text, order, IDs, source/readiness state, baselines and reversible committed edits without copy/import.
- **AC45-05:** CLI commands work against a running desktop through an authenticated, bounded, versioned owner channel, with no second store writer and no mandatory daemon.
- **AC45-06:** Owner/peer replacement, protocol mismatch, duplicate request, uncertain delivery, crash, and stale endpoint cases refuse/reconcile without duplicate local or remote effects.
- **AC45-07:** Dirty buffers, IME, external-editor saves, concurrent fields/structure and stale AI replies preserve authors' text and report conflicts instead of last-writer-wins.
- **AC45-08:** Cross-interface undo/redo is one coherent local transaction history with monotonically advancing revisions, no duplicate editor application, and no network/AI replay.
- **AC45-09:** Cold pending-review discovery, local purge and missing journals never auto-submit/adopt/delete or claim recovery of absent private WIP/authorization.
- **AC45-10:** Website edits, multiple batches/roots, historical anchors, second-machine recovery and profile/account isolation pass common production-path fixtures plus separately recorded native/website evidence.

## Non-goals

No cross-machine replication of unsent drafts, no CRDT/collaborative coauthoring protocol, no automatic upload of provenance or private notes, no filesystem sync/export requirement for local handoff, and no resurrection of deleted GitHub history. These are deliberately outside the extension. The canonical shared story belongs on GitHub; local-only history remains local.
