# Integration and import map

## Owning seams

| Work | Existing reference | Required extension |
|---|---|---|
| Local review identity | domain/state, epoch identity and review ledger contracts | A tagged review subject and local-cycle authority. Keep GitHub authority exact and distinct; no optional-zero ID widening throughout unrelated code. |
| Workspace observations | application Git cache and source material ports | Root-bound, read-only local capture host plus content store. Share a bounded material observation with diff/source rendering, not arbitrary filesystem access in the cache. |
| Revision comparisons | epoch, diff, workbench/tab services | Local immutable revisions, exact pairs and last-explicitly-reviewed observations. No renamed GitHub upload event or fake commit required. |
| Private notes and suggestions | review ledger/round-trip and current composer; RFC-0023 concepts | Narrow common versioned private thread, related anchors and provenance; byte-exact suggestions. Record which deferred scope is not activated. |
| Native UI | RFC-0037 composition and RFC-0052 scaffold | Local identity/title, source mode, round share, capture and publication views in existing regions. Preserve tab and keyboard contracts. |
| Frozen handoff | existing explicit-selected context supplement patterns | Provider-independent round builder/renderer with full selected content and immutable digest. It is not the agent launch supplement with required PR fields removed. |
| Local CLI/owner | RFC-0044/0045 headless services and owner protocol | Reuse the actual shared runtime once present. Extend finite verbs and caller identity; never scrape SQLite or open a competing writer while desktop owns the profile. |
| External consumer | RFC-0046 thin skill distribution pattern | New bpr-cycle skill in canonical shared AgentSkills source, calling verified CLI; no forked per-provider runtimes or new coordinator. |
| Publication | native GitHub transport/authority/provider and existing mutation journals | Explicitly admit Git object/new-ref/PR operations and permissions. No authenticated push or shell fallback hidden inside the old cache port. |
| Agent-free package | src/app/CMakeLists.txt composition and install/resource manifests | Optional agent adapter dependency closure, separately optional automation. Tests and manifests must inspect absence, not just feature flags. |

All paths above are discovery anchors, not a license for broad refactoring. Record
actual owned paths and public type/ABI changes on each live card. Preserve the
repository's coding standards, allocator/build conventions and effect isolation.
Use a compact new capability seam rather than making every legacy GitHub identity
nullable. Shared abstractions should emerge from these concrete sources, not a
pluggable-provider framework unrelated to the requested workflow.

## Safe import sequence

1. Read current AGENTS/project instructions and installed implement-rfc contracts.
   Validate this archive offline, inspect current IDs/status/admission gates, and
   locate the prior tour, CLI, conventions and scaffold mappings. Do not re-import them.
2. Allocate all 52 PACK-CYCLE aliases through the actual repository allocator. Commit
   its counter/reservations/receipts as required; never reserve BPR-1143 merely because
   it was next in the snapshot. Reconcile proposed RFC-0053–0061 similarly.
3. Archive the original payload at `docs/rfcs/assets/bpr-review-cycles-v1/` (or the
   established equivalent). Retain original pack hashes and a complete live remap.
   Imported assets are provenance, not a second live issue board.
4. Install the nine remapped RFCs at canonical `docs/rfcs/` paths. The archive's RFC
   links use `../../../specs/...` and `../../../supporting/...`; rewrite those in the
   **canonical installed copies only** to `assets/bpr-review-cycles-v1/specs/...` and
   corresponding support paths. Verify all links from their destination directories.
5. Import the remapped 52 cards into the canonical issue lanes with live IDs and
   dependencies. Rewrite their support links to the retained assets from their actual
   lane depth. Adapt front matter only through the installed schema contract. Keep
   `closeout_for` on exactly one closeout card per RFC.
6. Install the remapped native milestone plan at
   `docs/rfcs/plans/bpr-review-cycles-v1/milestone-plan.json`, with accompanying human
   plan and source lineage. Rewrite all work IDs, scope IDs and metadata references;
   preserve eleven groups. Do not execute the original alias plan alongside it.
7. Update the live RFC index and current architecture/product documents additively.
   Record native import and documentation/board validation separately from the archive
   validation. Historic settled RFC text/status/evidence remains unchanged.

This pack's internal milestones group concrete slices across its new RFCs to deliver
the manual round trip early. Intake must explicitly register that grouping under the
live project rules; it does not override older admission restrictions. Resolve real
blocking predecessors before implementation. Do not interpret a late-numbered UI RFC
as requiring the whole feature backend to close before any native UI slice can begin.
The closeout milestone is per-RFC and canonical, not a blanket exception.

## Proposed placement of new code

Choose names according to live conventions; examples are `cycle_state`,
`cycle_application_host`, `workspace_capture_port`, `cycle_store_port`,
`frozen_round`, `cycle_read_authority` and `publication_plan`. These are proposals.
Core owns intent legality, exact bindings, conflicts, completeness and state transitions.
Host owns files/objects/SQLite/IPC/network/cancellation and immutable observations.
Qt owns view realization/focus/IME and passes semantic edits through shared services.
CLI uses those same services. Optional agent adapters depend on the round contract,
not the other way around. The skill is documentation plus CLI calls, not runtime authority.

## Compatibility work, not silent scope expansion

Keep PR comments/tours and their GitHub-only collaborator behavior unchanged. Broaden
current product documentation for local history and new-PR creation, but leave merging,
closing existing PRs, arbitrary Git operations, hosted sharing, code execution inside
BPR and full IDE services out of scope. A read-only source inspection may end without
sharing or a PR. A local cycle may associate with a preexisting PR without inheriting
publication authority or making its private rounds public.
