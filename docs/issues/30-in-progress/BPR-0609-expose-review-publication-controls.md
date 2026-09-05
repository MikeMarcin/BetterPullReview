---
id: BPR-0609
title: Expose review publication controls
status: In Progress
rfc: RFC-BPR-0008
depends_on: [BPR-0608]
---

# Expose review publication controls

The draft editor creates Publish/Discard QActions but exposes neither through
buttons, shortcuts nor a context menu. Fixture automation triggers the action
directly, masking the missing public control. Fix this before the one-comment
live course consumes another candidate.

## Required final state

- Visible keyboard-accessible Publish and Discard buttons bind the existing
  QActions, preserving core-driven enablement and semantic intents.
- Clicking the public Publish button emits exactly the existing intent; no
  optimistic state update or alternate authority/effect path is introduced.
- Controls remain visible within the composed editor's 220-pixel height bound.
- Focused Qt editor/composition checks, scoped format/tidy and documentation gate
  pass; render the public editor and inspect its visible controls.
- No publication, credential, protocol, workshop redesign or full matrix.

## Verification

`qt_review_draft_editor` and the composed workbench path; build ordinary Release
and regenerate BPR-0256's capsule only after verification. BPR-0256 v010 was not
launched or materialized; prior spent approvals remain intact.
