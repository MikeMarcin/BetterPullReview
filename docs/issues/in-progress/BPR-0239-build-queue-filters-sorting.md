---
id: BPR-0239
title: Build review-queue filters and sorting presentation
status: In Progress
rfc: RFC-BPR-0010
depends_on: [BPR-0231]
---

# Build review-queue filters and sorting presentation

RFC-BPR-0010 allocates filters and sorting to the Qt landing surface. The
existing model presents the core order incrementally, but no filter or sorting
presentation was built.

## Required final state

- Provide explicit filters for the RFC-defined roles, observer reasons,
  attention buckets, repositories, and quiet/current state.
- Preserve the core's deterministic priority order unless the reviewer selects
  a documented alternate sort; never silently reorder by presentation state.
- Keep stable pull-request identity and incremental model signals across
  filtering and sorting changes.
- Persist only reviewer preferences, not derived queue membership.
- Prove accessible controls, empty/filter explanations, focus retention, and
  large-queue bounded updates through the composed landing surface.
