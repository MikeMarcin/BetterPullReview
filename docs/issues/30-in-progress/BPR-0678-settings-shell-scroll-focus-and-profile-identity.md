---
schema: "implement-rfc.card.v1"
id: "BPR-0678"
title: "Keep Settings usable at minimum size"
status: "In Progress"
rfcs: ["RFC-BPR-0018", "RFC-BPR-0028"]
depends_on: ["BPR-0592"]
---

# Keep Settings usable at minimum size

## Scope

Settings shell, per-page scrolling, default profile identity, Escape/focus return and removal of obsolete roadmap notices. Preserve adopted panels and accepted state.

Owned paths: src/adapters/qt/src/settings_window.cpp; src/adapters/qt/include/bpr/qt/settings_window.hpp; focused Qt Settings tests; stale Settings notices in workbench_window.cpp.

## Acceptance

AC-01/02/07/08/09/10: rendered composed Settings stays within 760x480, lower controls are keyboard reachable, category and page scroll survive switching, Escape hides without destroying controls. No domain effects in Qt.

See [the inventory](../../development/settings-authority-inventory.md) for exact existing selectors and authority.
Closeout: [[BPR-0593]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.

## Evidence

Not run. Created by the authorized BPR-0592 planning pass.
