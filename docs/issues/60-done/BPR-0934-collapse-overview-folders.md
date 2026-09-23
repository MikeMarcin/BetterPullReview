---
schema: "implement-rfc.card.v1"
id: "BPR-0934"
title: "Collapse folders in the Overview file list"
status: "Done"
rfcs: ["RFC-BPR-0037"]
depends_on: []
priority: P2
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
acceptance: [{"criterion":"Each folder in the Overview file list has an accessible, keyboard-operable expand/collapse control; collapsing hides all descendant files.","outcome":"pass","revision":"a88c0d19e00cbf035bafd282c2701c862ddac623","evidence":{"kind":"command","detail":"qt_pull_request_overview_page passed against the exact candidate: implicit Engine ancestors and nested Renderer/Shaders folders expose native mouse/Left-Right controls; QAccessible reports the folder name and expandable/expanded state; collapse hides every descendant without file activation."}},{"criterion":"Reopening restores the same file identities and order, with selection and focus handled without dispatching an unintended file selection.","outcome":"pass","revision":"a88c0d19e00cbf035bafd282c2701c862ddac623","evidence":{"kind":"command","detail":"The focused Qt test retains persistent folder/file indexes and sibling order through collapse/reopen and structural insert/remove/reorder refresh. Collapsing an ancestor of the current file moves current focus to that visible ancestor and dispatches no file selection."}},{"criterion":"Qt owns the local expansion state and preserves it across an incremental same-authority refresh when the folder still exists.","outcome":"pass","revision":"a88c0d19e00cbf035bafd282c2701c862ddac623","evidence":{"kind":"command","detail":"The retained Qt-only tree updates metadata in place and reconciles structural changes with row insertion/move/removal, never model reset. Tests retain nested collapsed states across same-authority refresh and clear old identities/selection/expansion on authority clearing. No core, host, provider or publication behavior changes."}},{"criterion":"Focused nested-folder, refresh and keyboard checks plus a rendered public Overview course demonstrate the behavior.","outcome":"pass","revision":"a88c0d19e00cbf035bafd282c2701c862ddac623","evidence":{"kind":"observation","detail":"Final restored-candidate batch passed qt_pull_request_overview_page (2.20s) and application_setup_destination_public_path (39.58s). Agent-inspected collapsed/expanded full-window PNGs show native disclosure and correct file visibility. The actual loaded-PR route then completes original source, update, conversation and local-draft checks with no publication. This is Qt 6.11.2 offscreen on macOS arm64/DPR1, not attended native or other-platform acceptance. Baseline smoke failures and inherited tidy diagnostics are separately recorded in milestone-overview-folder-collapse-evidence.md."}}]
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

Implemented by `a88c0d19e00cbf035bafd282c2701c862ddac623` on
`codex/rfc0037-overview-folders`. Overview now has a complete nested folder
hierarchy with native mouse and Left/Right disclosure. Surviving folder/file
nodes retain their model identities across incremental metadata and structural
refresh; collapse moves a hidden current item to its visible ancestor without
dispatching file selection. Authority clearing discards expansion and selection.

`qt_pull_request_overview_page` and
`application_setup_destination_public_path` pass against this candidate.
The former covers nested ancestry, accessibility, native mouse/keyboard input,
retained focus and identities, insert/remove/reorder refresh and authority reset.
The latter traverses the actual loaded-PR Overview, captures collapsed/expanded
rendering and continues the existing source/conversation/draft course with no
publication effects. Captures are task-local build artifacts, not repository
assets. This is offscreen Qt automation on macOS, not a new human verdict.

The exact acceptance records and broader baseline-failure qualifications are in
`docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/acceptance-BPR-0934.json` and
`milestone-overview-folder-collapse-evidence.md`. RFC-BPR-0037 remains Proposed
because its canonical dependencies are not all Auto-Verified. The active
BPR-0954 main checkout was not changed; safe main landing remains pending.
