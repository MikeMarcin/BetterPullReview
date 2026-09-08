---
schema: "implement-rfc.card.v1"
id: "BPR-0696"
title: "Expose profile creation, rename and open in the desktop"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0695"]
---

# Expose profile creation, rename and open in the desktop

## Scope

Add an explicit desktop composition owner for different-profile application runtimes,
with one in-process runtime/startup per profile ID. A profile menu opens the
management surface, creates/renames through the durable registry host, opens
another profile without replacing existing windows, and raises an already-open
profile. New profiles begin empty with standard defaults. Accepted labels update
chrome and Settings; rejected edits retain their text. No Qt-owned persistence.

Desktop owns runtimes and startup handles independently of its initial window;
closing the initial window must not strand another profile or shut its services
down. Per-profile close drains that profile; desktop Quit drains all. Public
production and fixture desktop paths share the same management composition.
Same-profile New Window, cross-process owner arbitration and restoration are
later slices and are not advertised as completed here.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, shared contracts, bookkeeping and acceptance.
Workers receive exclusive source/test ownership and preserve others.

## Acceptance

Actual profile-menu/management course creates Work from tuned Default, opens
both concurrently, renames Work, reopens by ID without duplicate runtime, and
closes Default while Work remains usable. Unknown/failed startup preserves other
windows; rename/create refusal preserves user text. Focused Qt stable identity,
keyboard and accessibility checks; one desktop public-path fixture and affected
main startup/production graph check. No live credentials or provider mutations.

## Evidence

Read-only inventory informed this slice; no implementation or new check yet.
