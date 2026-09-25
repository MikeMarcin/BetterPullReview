---
schema: "implement-rfc.card.v1"
id: "BPR-1134"
title: "The right tray's tabs"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: []
priority: "P2"
milestone: "Review workbench scaffolding"
metadata: {"origin":"owner decision on BPR-1106, 2026-09-24","design":"docs/design/BPR-1106-scaffolding-v8.html","criteria":["AC-09"],"reservation":"docs/issues/_reservations/BPR-1129--BPR-1135--89a65c75ff1a.json"}
---

# BPR-1134: The right tray's tabs

## Outcome

The conversations drawer's header becomes tabs: Comments and Findings, and
Tour when RFC-BPR-0039 provides it. Each tab's tools sit beneath the tabs.

- **Comments** keeps its scope and filters, and the new-comment count.
- **Findings** takes the AI tools, the finding queue and the defect controls,
  which today stack under the threads.

The drawer's toggle, its width and its modality presets are unchanged.

## Acceptance

- Each tab shows only its own content and tools.
- Switching tabs keeps each tab's scroll and selection.
- Keyboard and accessibility reach every tab.
- The Comments tab keeps the aligned cards and their behaviour exactly.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as drawn in [round v8](../../design/BPR-1106-scaffolding-v8.html). The
core decides, the host performs, Qt presents. It changes no publication,
authority or review semantics and no provider, Git or storage capability. It
starts in canonical order, after RFC-BPR-0028 to RFC-BPR-0038 close. Run the
focused courses named here and the documentation gate, and record exact
commands, results and the producing revision. The owner's attended look at
the built chrome on macOS and Windows is evidence this card cannot supply by
itself.
