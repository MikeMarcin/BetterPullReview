---
schema: "implement-rfc.card.v1"
id: "BPR-0708"
title: "Restore independent window drawer layouts"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0707", "BPR-0700"]
---

# Restore independent window drawer layouts

## Scope

Bind each actual desktop window to the existing schema5 workbench window-layout
row under its stable ID and profile-private root. Restore drawer states/widths
and line-number visibility independently; shared behaviour and explicit profile
New Window Defaults keep their existing authority. New windows seed from defaults
without copying a sibling's layout. Keep the standalone compatibility fixtures
explicit and preserve their existing defaults behaviour.

All creation/load/save/cleanup filesystem and SQLite work stays on the effect
lane. Exact window lifetime guards reject late layout projection. Orderly Quit
must retain the latest accepted layout, including a change immediately followed
by Quit; explicit Close discards its row after pending writers drain. Reconcile
orphan rows against the authoritative retained/live ID set at profile startup so
an interrupted Close cannot exhaust the bounded32-row store. Reconciliation
never changes global allocator metadata or profile defaults and never removes a
live sibling's row. Optional layout failure keeps windows usable with an explicit
existing settings error rather than claiming restoration success.

This slice covers workbench drawer layout, not native window position/size,
private PR/comparison destinations or semantic reading location. Browser tabs
remain unscheduled. Closeout [[BPR-0595]], immutable RFC_BASE_SHA
`e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

Primary owns architecture, public restart course, CMake, board and commits.
Workers receive exclusive source ownership after the port/lifecycle seam is fixed.

## Acceptance

Actual desktop restart preserves different layouts for two stable window IDs;
a fresh window starts with profile defaults, and layout changes in one window do
not alter its sibling. Immediate Quit after a drawer change persists the final
accepted layout. Explicit Close removes only its layout row; restart reconciliation
removes an orphan without touching retained rows or profile defaults. At capacity,
closed/orphan rows cannot permanently block new windows. Busy/corrupt/future
optional layout storage yields visible failure without allocator reset or startup
failure. Relevant host/storage/direct public checks, changed-TU tidy, documentation
and production composition guards pass. Required native/full RFC acceptance stays
open.

## Evidence

BPR-0700 already implements bounded schema5 keyed layout storage and separates
shared preferences; BPR-0707 supplies durable lifecycle identities. Current actual
composition still uses defaults for its first window and session memory for later
windows. This card integrates the existing persistence through actual lifecycle.
