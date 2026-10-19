---
schema: "implement-rfc.card.v1"
id: "BPR-1659"
title: "Share the Overview button treatment and migrate Settings"
status: "Ready"
rfcs: ["RFC-BPR-0087"]
depends_on: []
metadata: {"source": "Owner request 2026-10-19; isolated worktree, no merge/push/release", "milestone_plan": {"id": "shared-action-buttons-v1", "path": "docs/rfcs/plans/shared-action-buttons-v1/milestone-plan.json"}}
---

# BPR-1659: Share the Overview button treatment and migrate Settings

## Acceptance

- Reference Open/All uses the shared surface painter without losing its accepted appearance or behavior.
- Review and Comments choices/Reset plus Appearance tab choices use the same treatment; primary and specialized controls retain their meanings.
- Four-theme focused rendering, interaction, disabled/focus/hover checks and relevant composed tests pass; record exact commands and platform limits.

## Execution

The current delegated implementer owns the first milestone on
`codex/shared-action-buttons`. The diagnostic audit is independent and read-only.
Do not change release work or merge, push or release.
