---
schema: "implement-rfc.card.v1"
id: "BPR-1509"
title: "Style Files and Search tabs like Comments and Notes tabs"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"source":"Owner testing note, 2026-10-14","owner_note":"Lets makes files and search tabs more like comments notes tabs style","milestone_plan":{"id":"bpr1509-drawer-tabs","path":"docs/rfcs/plans/bpr1509-drawer-tabs/milestone-plan.json"}}
---

# BPR-1509: Style Files and Search tabs like Comments and Notes tabs

Use the existing Comments and Notes tabs as the visual reference for Files and Search tabs. Match the established tab treatment, spacing and selected/hover/focus states while retaining the Files and Search content and behavior.

## Acceptance

- AC1: Files and Search tabs use a coherent visual treatment matching the existing Comments and Notes tabs.
- AC2: Selected, hover, focus and disabled states remain clear and keyboard/accessibility behavior is preserved.
- AC3: Switching tabs preserves existing file selection, search state and drawer behavior.
- AC4: Verify actual offscreen rendered states and the composed tab-switch interaction, and prepare any required owner visual acceptance on an exact frozen candidate.

## Coordination

Load frontend-design:frontend-design before UX work as required by AGENTS.md. BPR-1508 owns L/R result marker indentation inside Search; coordinate shared files without taking over that scope. Never operate or capture the owner’s live desktop.

## Milestone plan and design

Mandate `bpr1509-owner-testing-note-20261014-r1`; baseline
`2f4aed1b4`. This isolated worktree owns only BPR-1509. No prior handoff
exists for this new note. The adopted one-card plan is
`docs/rfcs/plans/bpr1509-drawer-tabs/milestone-plan.json`.

Required `frontend-design:frontend-design` was read from
`/Users/jaeger/.claude/plugins/marketplaces/claude-plugins-official/plugins/frontend-design/skills/frontend-design/SKILL.md`.
The brief fixes the design reference: Comments/Notes, so Files/Search takes
its native document mode, content-sized labels, base line and keyboard focus.
Color tokens remain the inherited QPalette Window, WindowText, Base, Highlight,
HighlightedText and Disabled roles; the native style supplies their actual
colors. Type remains the inherited UI font. No new palette or font is needed.
The left-aligned row stays `Files | Search` above the existing page. Existing
page widgets retain their selection, filters, query, results and scroll state.
The plan review rejects decorative changes because they would depart from the
owner's named reference. BPR-1508's result marker indentation is independent.

MASK registration was initially missing; reviewed register plan
`sha256:e05dd2f7868b3b1769aeccc6724c1ecdb97dbb3e696e1579f902cf1b622f8cc2`
created declared local links and native role files. Preflight is ready; host
loading is not certified by it. Runtime evidence: `/tmp/bpr1509-20261014`.
Canonical claim first refused the stale counter, then the plan-edited card.
Coordinator authorized importing main's four testing-note commits through
`9c80010a0`; their allocation bytes were cherry-picked unchanged. This setup
commit preserves the plan before canonical claim retries.
