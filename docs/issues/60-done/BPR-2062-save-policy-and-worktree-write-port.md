---
schema: "implement-rfc.card.v1"
id: "BPR-2062"
title: "Save policy and the worktree write port"
status: "Done"
rfcs: ["RFC-BPR-0113"]
metadata: {"criteria":["AC02","AC03","AC06"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0001-save-policy-and-worktree-write-port.md"}
depends_on: ["BPR-2020"]
acceptance: [{"criterion":"AC02","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"local_changes: a file changed on disk is not overwritten (changed_on_disk, Reload offered); the write is a temporary file and a rename. An interrupted save has no course"}},{"criterion":"AC03","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"core_local_changes: every Edit refusal with its reason"}},{"criterion":"AC06","outcome":"pass","revision":"af839e73d","evidence":{"kind":"command","detail":"Opening reads only; the write is the host port; a review not in author state offers no Edit (qt_workbench_window_*); owner accepted the Edit view in session 767b9233 (edit-view, with a note on the underrail, follow-up card)"}}]
---

# Save policy and the worktree write port

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0113](../../rfcs/RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md), criteria `AC02`, `AC03`, `AC06`.

The core save decision (the identity comparison, the editable-file policy, each refusal reason and the typed outcome) and the application's narrow write port: one named regular file beneath a local draft's worktree root, no link followed, a complete old or new file after any failure. The existing workspace adapter stays read-only, and `architecture_boundaries` holds the port to its named callers.

## Intended verification

Core tests over the save decision and every not-editable reason. Host tests against a real temporary worktree: byte-exact writes across line endings and a missing final newline, a file changed underneath, a link, a path outside the root, and an interrupted write. The boundary test fails when a widget or another adapter reaches the port.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).

## Pulled forward (owner redesign, 2026-11-12)

Delivered on lane `claude/v0-5-0-prerelease-targets-52265d` under the owner's
redesign of local drafts ([accepted design](../../design/local-draft-pr-window-v1/README.md));
the RFC's amendment of 2026-11-12 is authoritative for scope. The
local-draft-authoring milestones must not build this again. Evidence:
[RFC-BPR-0113 ledger](../../verification/RFC-BPR-0113.md),
[RFC-BPR-0114 ledger](../../verification/RFC-BPR-0114.md).

## Owner look (session 6d00826a…, 2026-11-13)

Rejected: the local draft page, the Edit view, Local Changes with commit and push, the agent commit message; Home 1 accepted with a selection fault. Verdicts verbatim in the [RFC-BPR-0113 ledger](../../verification/RFC-BPR-0113.md) and the [RFC-BPR-0114 ledger](../../verification/RFC-BPR-0114.md). Reworked in the PR window round.
