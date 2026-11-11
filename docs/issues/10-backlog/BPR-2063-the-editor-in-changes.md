---
schema: "implement-rfc.card.v1"
id: "BPR-2063"
title: "The editor in a local draft's Changes"
status: "Backlog"
rfcs: ["RFC-BPR-0113"]
metadata: {"criteria":["AC01","AC04","AC05","AC07"],"milestone_plan":{"id":"local-draft-authoring-v1","path":"docs/rfcs/plans/local-draft-authoring-v1/milestone-plan.json"},"reservation":"docs/issues/_reservations/BPR-2062--BPR-2071--3110069c9cbb.json","imported_from":"docs/rfcs/assets/local-draft-authoring-v1/cards/DRAFT-0002-the-editor-in-changes.md"}
depends_on: ["BPR-2062", "BPR-2021"]
---

# The editor in a local draft's Changes

## Scope and acceptance

Authoritative requirements: [RFC-BPR-0113](../../rfcs/RFC-BPR-0113-edit-and-save-files-in-a-local-pr-draft.md), criteria `AC01`, `AC04`, `AC05`, `AC07`.

Edit on a listed file, the whole-file editor over the existing editing model, Save, the changed-on-disk and not-editable states, the unsaved-edits question, the edited-since mark and the refresh offer with saved files selected. Load `frontend-design:frontend-design` before the Qt work and say so in the report.

## Intended verification

Qt tests at the supported scales and the composed local-draft path on a fixture worktree: edit, save, refresh, successor candidate. The owner's look (AC07) is an attended review on a frozen build that opens on a local draft with a listed file, one question per state.

## Draft boundary

This is proposed work, not a live claim or acceptance record. No baseline,
implementation revision or passing result has been assigned. Follow the
[implementation entrypoint](../../rfcs/assets/local-draft-authoring-v1/IMPLEMENTATION_PROMPT.md).
