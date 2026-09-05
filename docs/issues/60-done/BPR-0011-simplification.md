---
schema: "implement-rfc.card.v1"
id: "BPR-0011"
title: "Simplify the RFC-BPR-0001 change set"
status: "Done"
rfcs: ["RFC-BPR-0001"]
depends_on: ["BPR-0010"]
---

# Simplify the RFC-BPR-0001 change set

Run a conservative behavior-preserving simplification pass over the RFC change
set, remove avoidable policy duplication and ceremony, and repeat focused and
boundary verification. Record an explicit no-change result if appropriate.

## Result

- Removed the unused composition-root `dispatch` API and its unnecessary core
  header dependency.
- Extracted named public-path and render-evidence checks from the nested Qt
  timer callback.
- Made runtime-audit teardown tolerate a process that exits during inspection.

## Evidence

- Focused public-path, render, runtime-surface, and documentation tests: 4/4
  passed.
- Complete Qt-enabled `dev` test matrix: 11/11 passed.
- Complete `core-only` test matrix: 6/6 passed.
- `git diff --check`: passed.
