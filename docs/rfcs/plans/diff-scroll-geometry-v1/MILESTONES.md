# Milestones

> Generated from `milestone-plan.json`; edit the JSON, not this view.

Plan: **diff-scroll-geometry-v1** · Coverage: **complete**

These are integration outcomes, not compulsory agent-launch boundaries.

## stationary-diff-geometry

Unified gutters track rendered source rows and floating Find stays anchored to the displayed pane through scrolling and layout changes.

**Work:** `BPR-1947`, `RFC-BPR-0095`

**Prerequisites:** None

**Completion:**

- AC01–AC03 pass rendered geometry and interaction checks at 100%, 125% and 200% scale.
- AC04 passes the production composition through installed Git/cache with a full-window render and bounded scroll work.
- Documentation gates pass and the exact candidate, evidence, commits and quiescent command state are recorded.

**Boundary:** Local Qt geometry and regression coverage only; preserve existing scrolling, search and layout contracts.
