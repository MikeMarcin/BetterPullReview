---
schema: "implement-rfc.card.v1"
id: "BPR-0678"
title: "Keep Settings usable at minimum size"
status: "Done"
rfcs: ["RFC-BPR-0018", "RFC-BPR-0028"]
depends_on: ["BPR-0592"]
acceptance: [{"criterion":"Bounded Settings pages, keyboard scrolling, stable identity and Escape","outcome":"pass","revision":"9e4461637c05285f062b64090317590bed438f2a plus finish","evidence":{"kind":"observation","detail":"New regression failed without scroll container, then passed. Real workbench Settings renders Accounts, Reviews and Appearance at 760x480; tall page Tab reaches last control inside viewport, category switch retains scrollbar and panel identity, Escape hides without deletion. Title identifies default profile; close returns focus when original widget survives. Inspected actual offscreen page captures in build/dev/settings-review. Wide existing account rows remain horizontally scrollable; their redesign is BPR-0679, not a false fit claim."}},{"criterion":"Focused composed checks and code/documentation checks","outcome":"pass","revision":"9e4461637c05285f062b64090317590bed438f2a plus finish","evidence":{"kind":"command","detail":"ctest --preset dev -R ^(qt_settings_window|qt_composed_github_scale_(100|125|200))$ --output-on-failure -j 1: 4/4 pass (6.99s). Canonical tidy.py --paths settings_window.cpp settings_window_tests.cpp --warnings-as-errors passed; direct clang-tidy lacked SDK include hints, canonical helper resolved them. clang-format dry-run, git diff --check and documentation gate pass. No full RFC platform/subjective claim."}}]
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
