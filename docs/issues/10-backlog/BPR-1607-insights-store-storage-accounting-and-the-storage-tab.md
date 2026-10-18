---
schema: "implement-rfc.card.v1"
id: "BPR-1607"
title: "Insights store, storage accounting and the Storage tab"
status: "Backlog"
rfcs: ["RFC-BPR-0086"]
depends_on: ["BPR-1606"]
platforms: ["mac", "win", "linux"]
metadata: {"milestone_plan":{"id":"insights-window-v1","path":"docs/rfcs/plans/insights-window-v1/milestone-plan.json"}}
---

# Insights store, storage accounting and the Storage tab

## Scope and acceptance

Implement RFC-BPR-0086 "Storage", the store part of "Data, privacy and
lifetime", AC05, and AC11's store classification.

- `insights.sqlite3` in `state/v1`, classified as a profile state store, so
  profile purge removes it.
- Byte accounting for every store, the Git cache by repository (from the
  adapter's existing inventory), logs, Markdown images and Review Cycles,
  against their budgets. Measured at most once per 60 s, only while Summary
  or Storage is visible.
- Manage cached repositories opens the existing epoch and cache controls (D8).
- The anonymized export gains an Insights section of byte sizes only.
