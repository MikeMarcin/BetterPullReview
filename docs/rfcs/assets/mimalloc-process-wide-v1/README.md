# Process-wide mimalloc integration pack

Draft v1, authored 2026-10-24 against repository commit `e82537ad1`.
The owner requested a pack to integrate mimalloc throughout BetterPullReview and
confirmed that “throughout” means process-wide standard allocation in every
shipped BPR executable, including eligible Qt and other loaded-library calls.
The inventory includes the macOS update helper as well as the desktop, `bpr`,
MCP context helper and process supervisor.
This is an unadopted proposal; no allocator has been integrated by this pack.

- [RFC-DRAFT-0028](RFC-DRAFT-0028-process-wide-mimalloc.md) owns behavior and
  acceptance AC01–AC09.
- [Decision record](DECISIONS.md) records the owner's scope answer, remaining
  engineering choices, structural validity, design readiness and confirmation.
- [Proposed cards](issues/) hold six implementation slices and one closeout.
- [Canonical milestone plan](milestone-plan.json) and its generated
  [view](MILESTONES.md) group the work. The [portable schema](schemas/milestone-plan.schema.json)
  comes from the installed `implement-rfc` milestone kit.
- [Implementation entrypoint](IMPLEMENTATION_PROMPT.md) points intake to these
  artifacts.

The first milestone produces a pinned, measured macOS integration. Windows and
target-platform Linux courses follow independently, then package/source reconciliation
and closeout. External Git, `gh`, agents and system processes are outside the
allocator replacement: BPR controls the allocators of its own executables.

The draft source identity and acceptance wording are proposal inputs, not
runtime evidence. Import must reconcile live RFC/card IDs and current source.
