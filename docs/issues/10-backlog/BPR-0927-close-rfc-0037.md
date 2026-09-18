---
schema: "implement-rfc.card.v1"
id: "BPR-0927"
title: "Verify and close RFC-BPR-0037"
status: "Backlog"
rfcs: ["RFC-BPR-0037"]
depends_on: ["BPR-0918", "BPR-0919", "BPR-0920", "BPR-0921", "BPR-0922", "BPR-0923", "BPR-0924", "BPR-0925", "BPR-0926"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0037"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0037

RFC base: `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.

Planned cards: BPR-0918 through BPR-0927. Add implementation follow-ups from the
investigation/design cards to this ledger and dependency list before closeout.
Markdown cards BPR-0916 and BPR-0917 remain outside this RFC.

[RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md) is Proposed.
This registration consolidates owner feedback; no runtime evidence exists yet.

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-001`; mandate revision `1`; producer `/root/rfc0037_m1`.
- Adopted plan: `docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json`; current milestone `source-update-review-entry` is bound by `assignment-rfc0037-dispatch-001.json` in the same directory.
- Outcome: source candidate `5b95485c9432cd365e8cedffc3aee0ea7d52b164` preserves Start/Continue as a typed overview dispatch, makes Start select its projected file and enter Review, keeps file rows in Read, and maps an absent exact comparison to an actionable comparison-unavailable failure rather than falsely claiming that the file changed. Genuine stale selected-diff terminals retain the stale guard.
- Composed evidence: a confirmed CLI-typed GHES fixture records the exact unavailable acquisition boundary without source or credentials; supported single/reuse launches over one durable store retain two actual epochs and render both recorded files. Natural Start and Continue enter Review without publication. Evidence is in `docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-source-update-review-entry-evidence.md`.
- Investigation boundary: the course establishes current behavior and one shared recorded acquisition boundary, not the owner run's cause. The exact owner build, CLI/Git versions and live target remain unobserved; no live target was contacted.
- Verification/review: focused host, Qt, composed application, CLI/Git, GHES, history, multi-window, formatting and fixture checks passed. The critical correction review accepted the candidate with no remaining finding.
- Acceptance: BPR-0922, BPR-0921 and BPR-0919 are Done at `ae881081`, `e34649d6` and `34760e13` respectively. Documentation and RFC-scoped board validation pass.
- Ownership: both read-only scouts and the critical reviewer completed without edits; all test commands are complete, and no child writer or long-running command is active.
- Next bounded objective after this milestone: begin `overview-conversation-decisions` in a fresh integration context; RFC closeout remains blocked on that milestone, `thread-reading-integration`, combined evidence and any required human/native observations.

## Closeout checklist

| Criterion | Owning card and direct evidence | Result |
|---|---|---|
| AC-01: Long PR descriptions have a demonstrated reading layout that exposes the complete admitted body, retains place and keeps Files reachable across representative window sizes and font scales. | [[BPR-0918]]: composed long overview descriptions course and applicable native layout/input evidence | Not run |
| AC-02: Overview Start review enters Review rather than empty Read; Continue review preserves the intended position, and navigation publishes nothing. | [[BPR-0919]]: composed start review navigation course and applicable native layout/input evidence | Not run |
| AC-03: Review composition has an agreed, implemented placement with usable input space, no unexplained large gaps or clipping, and preserved drafts and explicit submission semantics. | [[BPR-0920]]: composed review composition and drawer redesign course and applicable native layout/input evidence | Not run |
| AC-04: Available update history appears for the loaded PR; loading, empty, unavailable and failed states are distinct, and stale history cannot replace another PR’s updates. | [[BPR-0921]]: composed missing updates course and applicable native layout/input evidence | Not run |
| AC-05: Supported stable comparisons open the intended source for multiple files; genuine stale selections remain rejected and unavailable comparisons explain the actual cause and recovery path. | [[BPR-0922]]: composed source-loading blocker course and applicable native layout/input evidence | Not run |
| AC-06: The thread action has an understandable visible effect or is replaced by a clear automatic/reachable flow; loading, filtering, empty results and failures are explicit. | [[BPR-0923]]: composed show review threads course and applicable native layout/input evidence | Not run |
| AC-07: Full admitted thread bodies are reachable without accidental nested scroll traps; wheel, trackpad and keyboard behavior is responsive and supported by observed or measured evidence. | [[BPR-0924]]: composed clipped threads and slow nested scrolling course and applicable native layout/input evidence | Not run |
| AC-08: The graph and thread detail form one understandable reading path with consistent body presentation and no unexplained duplicate full thread, while preserving identity and navigation. | [[BPR-0925]]: composed apparent duplicate thread rendering course and applicable native layout/input evidence | Not run |
| AC-09: Discuss displays known PR conversations without requiring a source file to load; its default scope and optional file filtering are decided explicitly, with clear thread-to-code navigation and loading/error states. | [[BPR-0926]]: composed empty discuss and file-independent reading course and applicable native layout/input evidence | Not run |

## Completion rules

Require actual implementation evidence for all runtime criteria even when an
owning card is an investigation. Preserve exact owner feedback, design decisions,
producing revisions and focused command/native results. Verify the combined
first-PR path and independent discussion reading with synthetic fixtures; keep
unavailable platform or authorized live-target evidence explicit.

Require all linked implementation work done and eligible RFC dependencies in
canonical index order before changing RFC status. Run the documentation gate,
its script tests, board validation and focused composed acceptance checks.
