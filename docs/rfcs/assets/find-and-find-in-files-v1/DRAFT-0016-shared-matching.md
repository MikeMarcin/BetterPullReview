---
schema: "implement-rfc.card.v1"
id: "DRAFT-0016"
title: "Shared Unicode source matcher and repository path filters"
status: "Backlog"
rfcs: ["RFC-DRAFT-0015"]
depends_on: ["DRAFT-0015"]
metadata: {"milestone": "shared-matching", "provisional": true}
---

# DRAFT-0016 — Shared Unicode source matcher and repository path filters

## Acceptance

- **AC1:** Both commands share deterministic literal and bounded RE2-subset semantics: Unicode simple fold, Unicode word characters, nonoverlap, zero-width progress, multiline LF view and exact original UTF-8 spans.
- **AC2:** Include/exclude glob parsing follows comma/braces/classes/escaping and either-side include/exclude-wins rules; invalid patterns are refused before reads.
- **AC3:** Unified scanning follows actual version-contiguous source ranges, removes duplicate unchanged hits and cannot fabricate cross-version multiline matches; identical-blob same-entry deduplication is exact.
- **AC4:** Compile, scan, preview and result memory have explicit ceilings and cancellable bounded work; source-span tests need no QApplication.
