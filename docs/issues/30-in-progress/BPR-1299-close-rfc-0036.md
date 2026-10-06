---
schema: "implement-rfc.card.v1"
id: "BPR-1299"
title: "Verify and close RFC-BPR-0036"
status: "In Progress"
rfcs: ["RFC-BPR-0036"]
depends_on: ["BPR-0917"]
milestone: "Post-Pack v2"
closeout_for: ["RFC-BPR-0036"]
metadata: {"kind": "status-only closeout", "predecessor_closeout": "BPR-0917", "milestone_plan": {"id": "rfc0036-native-markdown", "path": "docs/rfcs/plans/rfc0036-native-markdown/milestone-plan.json"}, "allocated": "2026-10-06"}
---

# Verify and close RFC-BPR-0036

RFC base: `efa4388396128698e3cd737dd48efb9c364e5441`.

## Why this card exists

[[BPR-0917]] accepted RFC-BPR-0036's technical implementation and all six
criteria at `a3ab46ee7c2b3fb26e3c43d71c1658ea64ba1d5f` and left the RFC
Proposed, because RFC-BPR-0008 and the canonical predecessors were not yet
Auto-Verified. Its continuation named the remaining step: "reconcile
RFC-BPR-0036 ... after RFC-BPR-0008 and all required predecessors become
Auto-Verified. Do not change status now or repeat unchanged implementation
evidence merely because a predecessor later closes." BPR-0917 is Done, which is
terminal, and no open card owned that step.
[[BPR-1226]] keeps the review-tour pack's reconciliation against 0036 and says
the original acceptance "remains owned by their existing RFCs/cards"; it does
not close 0036. Found while closing RFC-BPR-0055 under [[BPR-1188]].

The RFC's own dependencies, RFC-BPR-0005, 0006 and 0008, are all
Auto-Verified. The owner removed the global canonical-order rule on 2026-09-27
(amendment r3), so dependency closure is the gate.

## Outcome and bounded work

Reconcile each criterion against BPR-0917's recorded evidence and the
[verification ledger](../../verification/RFC-BPR-0036.md). Check whether any
input to that evidence changed since `a3ab46ee` in a way that bears on a
criterion (the shared native reader, its link and image policy, the overview
and public-conversation presentation). Rerun only the affected focused checks.
If nothing bears on a criterion, set RFC-BPR-0036 and its index row to
Auto-Verified, record the result in the ledger and run DOCUMENTATION_OK.

Out of scope: new product work, VoiceOver certification, physical input,
Windows or Linux native qualification, and live services. BPR-0917 recorded
those as limits, not as failed criteria. Broader failures already owned by
[[BPR-0940]] stay with it.

## Closeout checklist

| Criterion | Planned direct check | Result | Producing commit |
|---|---|---|---|
| AC-01: Overview and public conversations render the declared Markdown subset through the shipped composition; fixtures cover headings, nested lists, tasks, quotes, code, tables and links. | BPR-0917's recorded course; focused rerun only if the reader or its composition changed | Pass at BPR-0917; not reconciled | `a3ab46ee` |
| AC-02: Source, copy and preview preserve original body bytes and publication payloads; empty, malformed and unsupported input stays readable with explicit fallback. | As above | Pass at BPR-0917; not reconciled | `a3ab46ee` |
| AC-03: Hostile HTML, local-file URLs, executable schemes and image/resource references execute nothing and cause no automatic filesystem or network access; admitted link gestures traverse the host-owned path. | As above | Pass at BPR-0917; not reconciled | `a3ab46ee` |
| AC-04: Keyboard reading, selection, links and source switching work with appropriate accessibility, light/dark/high-contrast appearance and representative display scales; wide blocks remain usable. | As above | Pass on available macOS at BPR-0917; not reconciled | `a3ab46ee` |
| AC-05: Declared bounds and measured long/pathological-body behavior keep the UI responsive; unchanged refresh preserves position and stale completions cannot replace another body's content. | As above | Pass at BPR-0917; not reconciled | `a3ab46ee` |
| AC-06: Composed long-description reading reaches the final content without a fixed six-line viewport constraint; documentation describes the supported subset, fallback, limits and image policy. | As above | Pass at BPR-0917; not reconciled | `a3ab46ee` |
