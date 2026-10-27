---
schema: "implement-rfc.card.v1"
id: "BPR-1790"
title: "On the owner's Windows machine, agent workspace recovery is refused as repair-required"
status: "Backlog"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["windows"]
metadata: {"reservation": "docs/issues/_reservations/BPR-1789--BPR-1790--55a3d3d1a1c6.json", "kind": "defect", "found_by": "owner chat, 2026-10-27", "related_rfcs": ["RFC-BPR-0033"]}
acceptance: []
---

# BPR-1790: Agent workspace recovery refused on Windows (repair-required)

## Observed

The owner, 2026-10-27, launching the installed app on Windows: the console
(BPR-1789) showed `AGENT_WORKSPACE_RECOVERY_REFUSED:path=omitted:repair-required`.

## What it means

At startup `recover_installed_agent_workspaces` reads the profile's
`agent-workspace-ownership-v1` registry and the private workspace folder
beside it (`src/app/src/github_application.cpp`, BPR-0817/BPR-0875). It
returned `workspace_ownership_failure::repair_required`, so every window says
"Disposable agent workspaces are unavailable: their folder needs repair or has
reached its limit" and the agent cannot make workspaces on that machine. The
app offers no repair action.

## To find out

- On that machine, which record or directory the recovery refused, and why
  (identity mismatch, unreadable record, leftover lock, a workspace from the
  BPR-1772 failure); read the app's own diagnostics, not personal data.
- Whether it is a Windows-specific defect or a real leftover that needs a
  repair path the reviewer can use.

Needs access to the owner's Windows machine (SSH, or a wizard).
