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

## Source checkpoint — 2026-10-19

Initial source commit `2660e4c63096479bb5d78090017e9f0e8d898e28` extracts
`button_surface`, adds `quiet_tool_button`, and migrates the Review/Comments
choice/Reset factories and Appearance tab-width choices. The reference Open/All
keeps its own content. The initial app-wide inventory and composed regression
test source are written.

The user's disk-space steering stopped large build/download work before it
started; about 1.0–1.1 GiB is free. This task has no build processes or large
build outputs. Source is preserved in
`/Users/jaeger/Documents/Codex/2026-10-19/task-4/shared-action-buttons`, branch
`codex/shared-action-buttons`.

All six touched translation units pass syntax-only checks. The new implementation
passes clang-tidy 22; all touched C++ passes clang-format 22; architecture,
documentation and whitespace gates pass. No build/link/runtime pass or new
visual verdict is claimed. [Evidence and exact continuation](../../verification/RFC-BPR-0087.md).

Resume with the focused build and serial runtime/visual checks once disk
headroom is confirmed. Do not finish this card from source/syntax evidence alone.
