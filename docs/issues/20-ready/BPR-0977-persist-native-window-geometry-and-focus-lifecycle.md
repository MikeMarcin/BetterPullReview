---
schema: "implement-rfc.card.v1"
id: "BPR-0977"
title: "Persist independent native window geometry and verify focus lifecycle"
status: "Ready"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0707", "BPR-0708", "BPR-0711", "BPR-0932"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"origin":"rfc0024-closeout-r2"}
---

# Persist independent native window geometry and verify focus lifecycle

## Scope

Complete RFC-0024 AC-03/06/07/09's per-window geometry and native focus/lifetime
obligations. Retain bounded window-local geometry through Quit/restart while
explicit Close removes only that window. Restore usable geometry on changed
displays and keep the MRU focus target distinct from stable identity.
Native presentation remains Qt-owned; host ports own durable persistence.

Owned surfaces: window presentation/geometry contracts, profile window
restoration/layout storage and composition, focused Qt/storage/public courses,
and candidate-bound native evidence. Do not change RFC-0018 follow-ups
BPR-0974/0975 or claim offscreen evidence as native acceptance.

## Acceptance

- Two same-profile windows retain distinct usable native geometry across
  Quit/restart; corrupt/unavailable or obsolete-display geometry has a bounded
  visible/reachable fallback without borrowing another window's record.
- Explicit Close versus Quit preserves the intended restoration set; repeated
  activation selects the MRU window and closing one leaves the sibling usable.
- Focus, geometry and lifecycle are observed on the required available native
  surfaces with exact candidate identity; unperformed native observations
  remain pending rather than being replaced by offscreen assertions.
- Focused persistence, Qt and composed restart checks pass.

## Evidence

Pending. Existing tests establish independent workbench layout, not durable
native top-level geometry.
