# Review Cycle domain and authority contract

## Ownership and integration

Keep the governing architecture: **the core decides, the host performs, Qt presents**.
Add a provider-independent local-cycle subject, not nullable GitHub IDs scattered
through the PR model. Use a tagged subject at shared application seams:

```text
review_subject = github_pull_request(exact existing authority)
               | local_cycle(local_profile_id, cycle_id, cycle_generation)
```

A PR retains every existing host/account/repository/PR restriction. A cycle has
local profile/workspace authority and optional observed Git/PR provenance. Reading a
cycle cannot reopen authenticated cached PR material after its authority was revoked.
Explicitly copied local source is governed as local source, with recorded origin and
enterprise classification; do not relabel a protected cache as an unprotected folder.

Reuse exact file-material, diff and anchor algorithms behind narrow source-provider
ports. Do not invent synthetic PR number zero, a synthetic account, fake GitHub
comments, or a second diff renderer. Catalog collections remain paged host-owned
records. The deterministic core holds selections, identities, revisions, pending
requests and decisions, not every source file or every conversation.

The existing PR epoch implementation is explicitly PR-authority scoped. Reuse its
comparison mechanics without renaming existing persisted IDs in place or giving a
local revision GitHub epoch chronology. Introduce a separate cycle revision catalog
and a lossless typed bridge at comparison/material selection. Existing database
migrations and old PR caches remain independently valid.

## Identities and stored records

Use strong opaque IDs with explicit local profile and catalog incarnation. Machine
wire IDs and monotonic counters are strings, never JSON floating-point numbers.
Display aliases such as `RC-17` are conveniences mapped in that scope, not globally
unique keys. Every mutation includes expected record revision and request ID.

| Record | Minimum content |
|---|---|
| Cycle | identity/incarnation, title, workspace binding, scope specification and versions, initial revision, lifecycle, optional PR associations |
| Workspace binding | user-selected root handle identity, logical repository/folder identity, object format if known, path mapping, classification, rebind history |
| Source revision | immutable manifest, observed origin/parent links, captured path entries, raw content hashes, mode/encoding facts, coverage and capture-quality facts |
| Feedback item | stable ID, immutable original subject(s), versioned prose, optional label, related references, private provenance, current correspondence observations |
| Suggestion revision | original source revision/blob and exact range; exact original and replacement bytes; exact/example intent; proposal/already-applied status |
| Round draft | versioned overall action, goal, constraints, chosen item revisions and order, included context, private scratch exclusion, optional response expectations |
| Frozen round | exact canonical payload, digest, input source/item revisions, creation observation, completeness and inclusion manifest |
| Delivery receipt | round/digest, channel, observed state, operation ID, time when known, adapter receipt when actually observed |
| Response | response/report ID, round/digest/item IDs, author/provenance evidence, prose, claimed disposition and optional named result revision/evidence |
| Verification | reviewer identity, item revision, checked source revision/material, explicit decision, time and evidence when supplied |
| Publication plan | exact candidate tree, parent/base identities, destination/branch, title/body, attribution, scope selection, authority revision and operation journal |
| PR association | observed provider identities, published head/base, known created PR, original cycle/round lineage; no copied public truth ownership |

Identity equality, material equality and chronology are different. Two source
revisions may reuse identical content blobs without becoming one observation.
A repeated capture of identical selected bytes is ordinarily a no-op and reports it;
a user may deliberately record another handoff observation without manufacturing
new changes. Wall clocks are display evidence, not ordering or uniqueness authority.

## Lifecycle: independent dimensions

Cycle lifecycle is `active | archived`; linking/publishing is an association, not a
terminal state that destroys local content. A cycle can be unarchived explicitly.
Deleting is a distinct confirmed data-management action.

Draft edits advance a draft revision. Freezing returns an immutable round; editing
thereafter creates or resumes a successor draft, never changes that round. Repeated
freezing of identical inputs reuses the existing ready artifact. A successor can be
an explicitly labelled amendment to a previous round. A discussion-only round may
reference the same source revision and does not create an artificial code revision.

Delivery observations include `copied`, `saved`, `retrieved`, `accepted_by_adapter`,
`recipient_acknowledged`, `failed`, and `uncertain`. Only evidence justifies the
stronger states. Reading through a CLI proves local retrieval, not model comprehension
or a human reading the notes. Saving a file proves neither receipt nor completion.

Track item response and reviewer judgment separately. A recipient can report
`addressed | partially_addressed | disagreed | blocked | question | not_attempted`.
Reviewer decisions include `verified | needs_work | withdrawn | deferred` and may be
cleared explicitly. Unknown/no response is not `not_attempted` unless reported.
A source change may invalidate a prior verification's current applicability but does
not erase its historical record. Plain notes and questions need not become defects.

## Persistence and concurrency

Use one local profile owner for serialized mutation. Reuse the tour draft ownership
service once available; extend finite typed operations, not a generic command server.
A standalone CLI can own the store when no desktop owner exists. It must forward or
refuse safely when the desktop owns it, never open a competing write connection.
Dirty text/IME remains protected by the shared draft revision contract; freeze must
flush confirmed editor transactions or return `draft_busy`/conflict, not omit text
visible in an editor. Cross-interface undo uses persisted transactions and does not
undo remote effects, publication or sent round contents.

Write immutable source blobs/manifests to temporary private objects, durably install
and verify them, then commit catalog references and pins in a bounded transaction.
Recovery removes only proven orphan temporary artifacts. Missing/corrupt objects are
explicitly unavailable; never substitute current workspace bytes. A killed freeze
cannot expose a half-complete round. Operation IDs and expected revisions make
retry safe; reuse with different inputs returns a conflict.

Pin source and item versions used by active rounds, unresolved correspondence,
verification and publication journals. Automatic budget reclamation may evict only
unreferenced content. If protected material fills the budget, give inventory and
refuse a new capture rather than prune the evidence. Explicit archive/delete tools
preview impact. Copy/file export is not a full-history backup. Publication never
triggers local history deletion. Document local-data-loss consequences truthfully.

## Authority separation

Snapshot capture is permission to read named local material, not to write the source.
Round freezing is a local operation, not permission to export. Export is permission to
disclose the selected bytes through a selected channel, not permission to implement.
The assignment action is explicit (`discuss`, `plan`, or `implement`) and does not
expand the harness's own sandbox or organizational policy. Verification and PR
publication each need separate real authority. No imported payload can grant it.

A user-facing action may prepare and copy a round in one interaction; these distinctions
are internal correctness boundaries, not a wizard of redundant confirmation screens.
