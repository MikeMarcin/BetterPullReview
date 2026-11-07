---
schema: "implement-rfc.card.v1"
id: "BPR-1906"
title: "Split github_application.cpp's composition adapters into responsibility files"
status: "In Progress"
rfcs: []
depends_on: ["BPR-1905"]
priority: "P2"
metadata: {"baseline":"2fc1cb2a395abcdcdf9a679833e6652d76d6d3dd","mandate":"Owner, 2026-11-06: keep going until the behavior-preserving refactor of the largest C++ files is done, round by round, landing on local main without pushing. Amendment the same day: aim for at most about 2,000 physical lines per file for this initial refactoring; subfolders are welcome for organization."}
---

# Split github_application.cpp's composition adapters into responsibility files

Round 2 of the `github_application.cpp` refactor ([[BPR-1905]] was round 1).
Lines 348–3738 hold the small adapters, policies and constants the profile
runtime and `github_application::implementation` compose. Both classes keep
these as members or construct them, so the later splits of those classes need
them declared in headers rather than in the root's anonymous namespace.

## Acceptance

- AC1: Move the composition adapters into responsibility-named header/source
  pairs under `src/app/src/github_application/`: review-mutation selection,
  queue notifications, host profiles, device authorization, GitHub
  presentation projections, session settings, blob sources, workbench
  composition, window review lanes and agent composition. Headers declare;
  member and function bodies move to the sources, unchanged.
- AC2: Each moved definition keeps its preprocessor conditions, comments,
  initialization and platform branches. Items the composition contracts read
  in the root (`inside_automated_test_boundary`, `cli_read_offered`, the
  provider using-declarations) stay there.
- AC3: Preserve behavior: dev (tests on) and tests-off composition builds;
  architecture boundaries; the round-trip, composition, public-preview,
  device-authorization, appearance and workbench composition courses; format
  and documentation gates. Reseal the moved code's pins only.
- AC4: Land the change, the completed card and a refactoring journal line on
  local main.
