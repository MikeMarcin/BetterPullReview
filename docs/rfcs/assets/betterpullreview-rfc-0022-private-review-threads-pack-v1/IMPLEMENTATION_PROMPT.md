# Implementation Steering Prompt

Use this after extracting the pack into an accessible local path.

```text
Use the repository's $implement-rfc skill to import and implement the attached
BetterPullReview RFC pack for RFC-BPR-0022, Private Review Threads, Provenance,
and Comment Composition.

Treat the pack's RFC as normative and its issue templates as provisional source
material. First inventory the pack, compare its source SHA/baseline to the live
tree, reconcile RFC/schema/issue IDs, retain the original pack under
`docs/rfcs/assets/<pack>/`, write the LIVE_IMPORT mapping, update the RFC index,
allocate collision-free live BPR issue IDs from docs/issues/NEXT_ID, rewrite all
card dependencies together, run the structure/documentation gates, and commit
the import on main before opening implementation lanes.

Follow the repository architecture exactly: the core decides, the host
performs, Qt presents. Do not put Qt/provider/filesystem/process/clock/thread/
network/database/credential dependencies in core. Do not let Qt read SQLite,
launch agents, or call GitHub. Use the existing agent invocation host and the
existing RFC-BPR-0008 mutation coordinator; create no second launch or publish
path.

The central product invariant is:

  private thread/source selection -> Squash to Draft -> inspect/edit local
  draft -> separate explicit Publish

Squash to Draft must never mutate GitHub. Instrument all mutation capabilities
and prove zero calls. Preserve the source thread and immutable derivation
lineage. Editing AI text creates a reviewer-authored successor; it does not
rewrite or relabel the AI-authored source. Legacy notes migrate as
legacy_unknown because the old schema did not retain origin.

Add a durable agent_run_id per invocation/turn. Keep requested, configured,
protocol-reported, and content-self-reported model/effort values as separate
facts. Unknown remains unknown. Never infer model or effort from executable,
profile, provider, output style, or prose. Snapshot enough session/binding
metadata on the run that retained provenance survives pruning of the bounded
agent-session catalog.

Use RFC-BPR-0004 semantic identities for thread subjects and draft targets. No
Qt index, visual row, y-coordinate, ribbon path, nearest-line guess, or silent
reanchor. Multi-target selections require an explicit target or Split into
Drafts.

Allocate the next live SQLite schema version only after checking the live tree
(expected 6 on the supplied baseline). Implement a forward-only atomic
migration of every legacy local_note into a one-turn private thread preserving
text and attachment exactly, assigning legacy_unknown, advancing high-water
sequences, and removing dual authority. Add paged narrow store ports; do not
expand private_ledger_observation into an all-transcript load. Enforce explicit
count/byte/page quotas without silent truncation.

Make accepted compositions/drafts durable across restart. After the separate
RFC-BPR-0008 publish succeeds and reconciles, correlate the exact local
composition/draft revision to ServerCommentId and server state. Do not add a
hidden or mandatory BetterPullReview/AI marker to the GitHub body.

Build the real Qt workflow in the composed BetterPullReview application:
private/shared navigation, persistent local-only disclosure, incremental
transcript, revision history, provenance chips/details, exact source selection,
composition workspace, manual/exact-copy/explicit agent-assisted modes, Accept
as Draft, and the separate Publish action. Avoid stacking another unbounded
panel into the drawer. Preserve keyboard, focus, accessibility, scaling, and
changed-set-only model updates.

Execute the issue graph in IMPLEMENTATION_ORDER.md, adapting only after recording
live source evidence. Complete direct core, SQLite, protocol, host, Qt,
composition-root, migration, crash, quota, security, performance, rendered UX,
and designated mutation evidence. Use synthetic/secret-free fixtures. Do not
mark the RFC Auto-Verified before all dependencies and direct evidence permit
ordered closeout. If target-native, owner-attended, or live-service evidence is
unavailable, complete everything available now and leave a precise live card
naming the missing evidence and supplier.
```
