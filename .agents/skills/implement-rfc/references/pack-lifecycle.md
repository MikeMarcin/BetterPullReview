# Pack Lifecycle

How an RFC pack moves through the repository: imported on `main`, worked in a
lane, integrated at milestones, and cleaned up. These are the local rules every
session follows; a remote station is a lane whose transport happens to be
another machine, and it obeys this file plus
[remote-routing.md](remote-routing.md). Paths and commands named in angle
brackets are the values `config/project.json` holds under that key.

Roles:

- **Project integrator** — the single session with authority over `main`,
  the next-ID source (`<next_id.path>`), ID reservations, the tracker rules
  READMEs, merging lanes, and final `<terminal_rfc_status>` confirmation.
  Exactly one is active per repository.
- **Lane** — any branch other than `main`, normally one worktree per pack. A
  lane commits and pushes only its own branch; it never merges, rebases onto,
  or pushes `main`.

## Import

The integrator imports a pack on `main` so lanes branch from a reconciled
state:

1. Inventory the pack and read its README, manifest, order documents, RFCs,
   cards, and supporting material. When the user names no order, the pack's
   own order documents define the requested RFC set and order.
2. Reconcile every pack RFC and issue against the live board. Assign live IDs
   collision-free with `<allocate_id_command>`; the live board outranks any
   snapshot the pack carries. Rewrite filenames, front matter, and every
   cross-reference together; the retained pack copy keeps the original IDs.
3. Do not copy a top-level implementation-order document as a second mutable
   tracker; encode sequencing in card dependencies. Exclude editor files,
   build output, caches, transient logs, credentials, and screenshots made
   only for local inspection.
4. Retain the pack itself as provenance under `<rfc_root>/assets/<pack>/` and
   record the mapping, baseline SHA, live destinations, and deliberate
   exclusions beside it as `<pack>.LIVE_IMPORT.md`.
5. Advance the next-ID source past every imported ID.
6. Commit the RFCs, cards, counter, and `LIVE_IMPORT` record together, and
   run the `<structure_gate>` commands before starting any lane.

## Lane Setup

- One worktree per pack, branched from `main` at or after the import commit.
- The integrator reserves an ID block the lane will need
  (`<allocate_id_command> --count N --reserve-for <lane>`; the receipt lands
  under `<reservations_dir>`). The reservation commit must be reachable from
  the lane branch before its first block card is filed.

### Lane commit authority

A lane MAY commit:

- source, tests, build files, and documentation inside its pack's RFC scopes;
- board cards whose IDs fall in its reserved blocks or its program's
  registered ranges — creation, edits, and folder moves, including the
  claim-move to the in-progress lane;
- its RFCs' files and closeout cards.

A lane MUST NOT commit:

- the next-ID source in any form;
- reservation receipts under `<reservations_dir>` (integrator-authored on
  `main`);
- the tracker rules READMEs under `<issue_root>` and `<rfc_root>`;
- other programs' cards or RFCs;
- `.agents/**`, `.claude/**`, `.codex/**`, `scripts/**` — unless one of its
  RFCs explicitly owns the path.

A lane may flip an RFC to `<terminal_rfc_status>` on its branch only as a
proposal; the integrator revalidates and confirms the status on `main` at
integration.

## Discovery Mid-Work

Working a pack legitimately surfaces issues and RFCs the import did not know.
Mint new cards only from the lane's reserved block. When the block runs out,
request another from the integrator or defer card creation to integration —
never improvise IDs or edit the next-ID source from the lane. Unused reserved
IDs are burned, not recycled.

## Milestone Integration

Milestones are the natural moments to integrate or hand off. Integration is
integrator-only, one lane at a time:

- Prefer rebase-then-fast-forward so `main` keeps a straight-line history;
  fall back to a merge commit when the rebase does not apply cleanly or the
  lane's history is worth preserving as-is.
- Verify at the integrated head. Build and test at one commit; verifying
  across mid-integration commits fakes regressions.
- Run the `<structure_gate>` commands and the affected RFC checks after each
  landing, before starting the next lane.
- A lane diff that touches the next-ID source, reservation receipts, or
  another program's files is an ownership violation: stop and resolve with
  the lane rather than integrating.

## Renumbering

When mid-work discovery collides with IDs allocated elsewhere, the integrator
renumbers at integration:

1. Rename the card or RFC file to the new ID.
2. Update the front-matter `id` and the title line.
3. Grep for the old ID and update every reference — cards, RFCs,
   `LIVE_IMPORT` records, index rows.
4. Advance the counter past the new ID if needed; never rewind it.
5. Note the renumber and its reason on the card or RFC.
6. Re-run the `<structure_gate>` commands.

## Handoff

A stopping point hands the lane to a fresh session or another harness. Before
handing off:

- push the lane branch;
- make board folders truthful — no card sitting in a folder its state has
  outgrown;
- record on the closeout card: the next slice, verification already run with
  outcomes, and open decisions;
- land crucial discoveries in agent documentation while the context that
  knows them still exists: if the work made a purpose, ownership, hazard, or
  verification rule in an affected instruction file untrue — or surfaced a
  hazard it dangerously omits — apply the smallest source-backed update and
  run the `<structure_gate>`; everything else stays out of the docs;
- commit or explicitly note any remaining working-tree state — nothing left
  silently uncommitted.

The receiving session reorients from the live board, the closeout card, and
`git log` (the Start And Reorientation procedure in `SKILL.md`); it needs no
conversation memory.

## Cleanup

At handoff or closeout, remove what the work created:

- `git worktree remove` each task worktree and verification checkout whose
  result has been consumed, then `git worktree prune`;
- compare `git worktree list` against the session's starting state — the
  session removes what it created and reports anything stale it noticed but
  did not create;
- never retain a checkout as evidence.
