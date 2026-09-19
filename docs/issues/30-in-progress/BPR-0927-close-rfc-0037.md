---
schema: "implement-rfc.card.v1"
id: "BPR-0927"
title: "Verify and close RFC-BPR-0037"
status: "In Progress"
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

[RFC-BPR-0037](../../rfcs/RFC-BPR-0037-first-pr-review-workflow.md) remains
Proposed after technical closeout because five direct dependencies are not
Auto-Verified. All implementation and design follow-up cards are Done; this
card records their combined evidence without claiming RFC-BPR-0036 Markdown work.

## Current frontier

- Run/dispatch: `rfc0037-run-20260918` / `rfc0037-dispatch-004`; mandate
  revision `1`; producer `/root/rfc0037_closeout`.
- Binding: authored milestone `combined-first-pr-closeout`, recorded in
  `assignment-rfc0037-dispatch-004.json`; registration baseline
  `9bd2002b859ce4c5599d86dcc51771aa365fa90b`.
- Reconciliation: source/update/entry, design and thread-reading milestones
  remain accepted. Their live fingerprint drift is the expected result of
  accepted cards moving to Done; every producing/correction commit resolves and
  no concrete engineering contradiction was found.
- Combined candidate: `e4096e050b856eed1f103c215701d3cf289d159f` adds one
  continuous network-isolated GHES/CLI course from PR load and unavailable
  acquisition through retained update history, Overview Start review, exact
  `review.cpp` and `secondary.cpp`, file-independent PR-wide Discuss, and
  retained local Compose draft/source selection with zero publication effects.
- Verification: the direct combined selector passed after a narrow
  code-simplifier pass, and the five-selector integration boundary passed 5/5
  in 145.31 seconds. Historical per-card tests are reused where their inputs are
  unchanged. Current documentation/board gates are recorded in the closeout
  evidence file.
- Native evidence: macOS attended session
  `c44d1fe1c8245aa53959afc9fc9f4b27` accepted Overview, Discuss and Compose;
  native Cocoa long-detail coverage ran at DPR 2. Windows, Linux, live
  GitHub/GHES, literal `gh`, private-service and successful in-process Refresh
  recovery are explicitly unclaimed.
- Formal status: RFC-BPR-0003 is Auto-Verified; RFC-BPR-0008, RFC-BPR-0013,
  RFC-BPR-0015 and RFC-BPR-0028 are Proposed, and RFC-BPR-0029 is Accepted.
  Therefore RFC-BPR-0037 truthfully remains Proposed even though this technical
  closeout can finish.
- Evidence: `milestone-combined-first-pr-closeout-evidence.md` and
  `acceptance-BPR-0927.json` in the RFC plan directory. All delegated writers,
  scouts, reviewer and commands must be quiescent before the final handoff.

## Closeout checklist

| Criterion | Owning card and direct evidence | Result |
|---|---|---|
| AC-01: Long PR descriptions have a demonstrated reading layout that exposes the complete admitted body, retains place and keeps Files reachable across representative window sizes and font scales. | [[BPR-0918]] design evidence; [[BPR-0928]] composed runtime and native macOS verdict | Pass at `8939de71` |
| AC-02: Overview Start review enters Review rather than empty Read; Continue review preserves the intended position, and navigation publishes nothing. | [[BPR-0919]] accepted Start/Continue course; combined Start-review/zero-publication course | Pass at `5b95485c` and `e4096e05` |
| AC-03: Review composition has an agreed, implemented placement with usable input space, no unexplained large gaps or clipping, and preserved drafts and explicit submission semantics. | [[BPR-0920]] design evidence; [[BPR-0929]] composed runtime/native evidence; combined retained-draft course | Pass at `8939de71` and `e4096e05` |
| AC-04: Available update history appears for the loaded PR; loading, empty, unavailable and failed states are distinct, and stale history cannot replace another PR’s updates. | [[BPR-0921]] accepted update-history course plus combined retained-history replay | Pass at `5b95485c` and `e4096e05` |
| AC-05: Supported stable comparisons open the intended source for multiple files; genuine stale selections remain rejected and unavailable comparisons explain the actual cause and recovery path. | [[BPR-0922]] accepted unavailable/stale/two-file evidence plus combined exact-source replay | Pass at `5b95485c` and `e4096e05` |
| AC-06: The thread action has an understandable visible effect or is replaced by a clear automatic/reachable flow; loading, filtering, empty results and failures are explicit. | [[BPR-0923]] design evidence; [[BPR-0929]] automatic Discuss/state matrix | Pass at `8939de71` |
| AC-07: Full admitted thread bodies are reachable without accidental nested scroll traps; wheel, trackpad and keyboard behavior is responsive and supported by observed or measured evidence. | [[BPR-0924]] deterministic/native macOS thread-reading evidence | Pass at `8939de71` and `e3258509` |
| AC-08: The graph and thread detail form one understandable reading path with consistent body presentation and no unexplained duplicate full thread, while preserving identity and navigation. | [[BPR-0925]] integrated graph/detail evidence and independent correction review | Pass at `cc038748` and `e3258509` |
| AC-09: Discuss displays known PR conversations without requiring a source file to load; its default scope and optional file filtering are decided explicitly, with clear thread-to-code navigation and loading/error states. | [[BPR-0926]] design evidence; [[BPR-0929]] runtime/native evidence; combined no-file Discuss course | Pass at `8939de71`, `e3258509` and `e4096e05` |

## Completion rules

Require actual implementation evidence for all runtime criteria even when an
owning card is an investigation. Preserve exact owner feedback, design decisions,
producing revisions and focused command/native results. Verify the combined
first-PR path and independent discussion reading with synthetic fixtures; keep
unavailable platform or authorized live-target evidence explicit.

Require all linked implementation work done and eligible RFC dependencies in
canonical index order before changing RFC status. Run the documentation gate,
its script tests, board validation and focused composed acceptance checks.

## Formal result

Every RFC-BPR-0037 technical criterion and all four authored closeout exits have
direct evidence. BPR-0927 closes as the technical reconciliation card. The RFC
and index remain `Proposed` until every listed dependency is `Auto-Verified`;
obeying that gate by not changing status is part of this accepted result, not an
omitted test. A later dependency-only status reconciliation need not repeat the
unchanged engineering course.
