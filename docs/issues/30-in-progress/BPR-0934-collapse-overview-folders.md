---
schema: "implement-rfc.card.v1"
id: "BPR-0934"
title: "Collapse folders in the Overview file list"
status: "In Progress"
rfcs: ["RFC-BPR-0037"]
depends_on: []
priority: P2
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Collapse folders in the Overview file list

The owner requested this follow-up in RFC-BPR-0013 v005 review decision 441
(session `cbd48b91000b00f1e72e1ad62c7c384c`, 2026-09-20): collapse a folder
in Overview to hide every file beneath it. This is separate from the current
selected-diff failure and does not change BPR-0794 or BPR-0795 acceptance.

## Acceptance

- Each folder in the Overview file list has an accessible, keyboard-operable
  expand/collapse control; collapsing hides all descendant files.
- Reopening restores the same file identities and order, with selection and
  focus handled without dispatching an unintended file selection.
- Qt owns the local expansion state and preserves it across an incremental
  same-authority refresh when the folder still exists.
- Focused nested-folder, refresh and keyboard checks plus a rendered public
  Overview course demonstrate the behavior.

## Evidence

Allocated on main using `scripts/allocate_issue_id.py`, receipt
`BPR-0934--BPR-0934--69339afaef80.json`. Existing Overview drawer-collapse
cards address drawers rather than folders and do not own this request.
