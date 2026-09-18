---
schema: "implement-rfc.card.v1"
id: "BPR-0927"
title: "Verify and close RFC-BPR-0037"
status: "Backlog"
rfcs: ["RFC-BPR-0037"]
depends_on: ["BPR-0918", "BPR-0919", "BPR-0920", "BPR-0921", "BPR-0922", "BPR-0923", "BPR-0924", "BPR-0925", "BPR-0926", "BPR-0928", "BPR-0929"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0037"]
metadata: {"milestone_plan":{"id":"rfc-bpr-0037-first-pr-workflow","path":"docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json"}}
---

# Verify and close RFC-BPR-0037

RFC base: `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.

Planned cards: BPR-0918 through BPR-0927, plus the canonically allocated
implementation follow-ups BPR-0928 and BPR-0929. Those follow-ups remain in
this ledger and dependency list until their runtime acceptance is complete.
Markdown cards BPR-0916 and BPR-0917 remain outside this RFC.

[RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md) is Proposed.
The accepted source/update/entry milestone now supplies runtime evidence for
AC-02, AC-04 and AC-05. Design decisions for AC-01, AC-03, AC-06 and AC-09 do
not substitute for the open BPR-0928/BPR-0929 runtime work.

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-002`; mandate revision `1`; producer `/root/rfc0037_m2`.
- Adopted plan: `docs/rfcs/plans/rfc-bpr-0037-first-pr-workflow/milestone-plan.json`; current milestone `overview-conversation-decisions` is bound by `assignment-rfc0037-dispatch-002.json` in the same directory. The prior accepted milestone was reconciled without reopening its engineering.
- Selected Overview design: a retained expanded central reader with a compact preview, one scroll owner, fixed Return to Files and source-anchor retention across reflow. Synthetic body/layout/input evidence is in `overview-description-layout-study.md`; BPR-0928 owns runtime AC-01.
- Selected conversation design: a dedicated in-window Compose review page plus PR-wide Discuss with explicit optional file/Unread/Unresolved filters and distinct loading/empty/filtered/failure states. Synthetic geometry and hidden-page evidence is in `conversation-surface-layout-study.md`; BPR-0929 owns runtime AC-03/06/09.
- Allocation/linkage: BPR-0928 and BPR-0929 were reserved at `f021c138`, link every design-only card to bounded runtime acceptance and are dependencies of this closeout. The authored complete plan remains byte-unchanged after its amendment checker refused new work atoms; its existing current/future exits already require and carry these follow-ups.
- Verification boundary: focused Overview, conversation panel/cards, workbench, review-editor and review-submission public-path checks pass. `qt_composed_github_scale_100` deterministically aborts before conversation assertions on unrelated BPR-0915 shared-sign-in expectation drift; it is not passing integration evidence. Native trackpad, accessibility/appearance and the owner's original two clipped-box identities remain unobserved.
- Ownership: both workers and the read-only failure scout are complete; their disposable probes were removed and all commands are quiescent.
- Next bounded objective after this milestone: execute `thread-reading-integration`, including BPR-0924/BPR-0925 and the linked BPR-0928/BPR-0929 runtime obligations. RFC closeout remains blocked on those implementations, the combined course and required native observations.

## Closeout checklist

| Criterion | Owning card and direct evidence | Result |
|---|---|---|
| AC-01: Long PR descriptions have a demonstrated reading layout that exposes the complete admitted body, retains place and keeps Files reachable across representative window sizes and font scales. | [[BPR-0918]] design evidence; [[BPR-0928]] composed runtime and applicable native layout/input evidence | Design selected; runtime pending |
| AC-02: Overview Start review enters Review rather than empty Read; Continue review preserves the intended position, and navigation publishes nothing. | [[BPR-0919]]: accepted composed start/continue course at `5b95485c` and applicable focused evidence | Pass for accepted milestone; combined closeout pending |
| AC-03: Review composition has an agreed, implemented placement with usable input space, no unexplained large gaps or clipping, and preserved drafts and explicit submission semantics. | [[BPR-0920]] design evidence; [[BPR-0929]] composed review composition course and applicable native layout/input evidence | Design selected; runtime pending |
| AC-04: Available update history appears for the loaded PR; loading, empty, unavailable and failed states are distinct, and stale history cannot replace another PR’s updates. | [[BPR-0921]]: accepted recorded CLI/GHES update-history course at `5b95485c` | Pass for accepted milestone; combined closeout pending |
| AC-05: Supported stable comparisons open the intended source for multiple files; genuine stale selections remain rejected and unavailable comparisons explain the actual cause and recovery path. | [[BPR-0922]]: accepted recorded CLI/GHES two-file and unavailable/stale course at `5b95485c` | Pass for accepted milestone; combined closeout pending |
| AC-06: The thread action has an understandable visible effect or is replaced by a clear automatic/reachable flow; loading, filtering, empty results and failures are explicit. | [[BPR-0923]] design evidence; [[BPR-0929]] composed Discuss entry/state course | Design selected; runtime pending |
| AC-07: Full admitted thread bodies are reachable without accidental nested scroll traps; wheel, trackpad and keyboard behavior is responsive and supported by observed or measured evidence. | [[BPR-0924]]: composed clipped threads and slow nested scrolling course and applicable native layout/input evidence | Not run |
| AC-08: The graph and thread detail form one understandable reading path with consistent body presentation and no unexplained duplicate full thread, while preserving identity and navigation. | [[BPR-0925]]: composed apparent duplicate thread rendering course and applicable native layout/input evidence | Not run |
| AC-09: Discuss displays known PR conversations without requiring a source file to load; its default scope and optional file filtering are decided explicitly, with clear thread-to-code navigation and loading/error states. | [[BPR-0926]] design evidence; [[BPR-0929]] composed PR-wide Discuss and applicable native layout/input evidence | Design selected; runtime pending |

## Completion rules

Require actual implementation evidence for all runtime criteria even when an
owning card is an investigation. Preserve exact owner feedback, design decisions,
producing revisions and focused command/native results. Verify the combined
first-PR path and independent discussion reading with synthetic fixtures; keep
unavailable platform or authorized live-target evidence explicit.

Require all linked implementation work done and eligible RFC dependencies in
canonical index order before changing RFC status. Run the documentation gate,
its script tests, board validation and focused composed acceptance checks.
