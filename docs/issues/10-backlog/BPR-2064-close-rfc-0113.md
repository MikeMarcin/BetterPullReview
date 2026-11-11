---
schema: "implement-rfc.card.v1"
id: "BPR-2064"
title: "Verify and close RFC-BPR-0113"
status: "Backlog"
rfcs: ["RFC-BPR-0113"]
metadata: {"criteria":["AC01","AC02","AC03","AC04","AC05","AC06","AC07"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0003-close-edit-and-save.md","draft_title":"Close editing and saving in a local draft"}
depends_on: ["BPR-2062", "BPR-2063"]
closeout_for: ["RFC-BPR-0113"]
---

# BPR-2064: Verify and close RFC-BPR-0113

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0113](../../rfcs/RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md), criteria `AC01`, `AC02`, `AC03`, `AC04`, `AC05`, `AC06`, `AC07`.

Acceptance assessment for editing and saving a file of a local draft.

## Intended verification

Judge every criterion on the composed application; keep the owner's look as its own recorded answer.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).

## Closeout checklist

RFC base: `b0d54697a6db6b17b459a751515de60aff14d65d`.

| Criterion | Check | Result | Producing commit |
|---|---|---|---|
| **AC01:** In a local draft, Edit on a listed file opens the whole file in the existing code editor with several carets, box selection, virtual space and undo, and a saved edit is on disk byte for byte as edited, including CRLF and LF endings, tabs and a missing final newline, with the file's mode unchanged and nothing staged or committed. | Pending | Pending | Not implemented |
| **AC02:** Save writes only when the file on disk is what the editor last read or wrote. A file changed on disk since is not overwritten, the author's text is kept, and reload and keep-editing are offered. An interrupted save leaves a complete old or new file. | Pending | Pending | Not implemented |
| **AC03:** A deleted file, a link, a submodule, a path outside the root, a non-UTF-8 or NUL-bearing file, a file over 1 MiB and a filter- or LFS-qualified file each refuse Edit with that specific reason, and none is opened truncated or converted. | Pending | Pending | Not implemented |
| **AC04:** After a save the draft offers one refresh with the saved files selected. Accepting creates a successor candidate holding the saved bytes and retains the previous candidate; before that, the current candidate, its notes, evidence and tour are unchanged. | Pending | Pending | Not implemented |
| **AC05:** Leaving the editor, the draft or the application with unsaved edits asks first, and Discard writes nothing. | Pending | Pending | Not implemented |
| **AC06:** Opening, reading and resuming a local draft without pressing Save writes nothing to the worktree; a pull request review offers no Edit; and no Qt widget or model performs the write. | Pending | Pending | Not implemented |
| **AC07:** On a frozen build serving a local draft, the owner accepts the editor in Changes, its not-editable reasons and its changed-on-disk state. | Pending | Pending | Not implemented |
