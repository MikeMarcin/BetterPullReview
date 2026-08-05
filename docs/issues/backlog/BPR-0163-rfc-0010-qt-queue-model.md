---
id: BPR-0163
title: Qt: queue model, filters, sorting, and empty states
status: Backlog
rfc: RFC-BPR-0010
depends_on: [BPR-0158]
---

# Qt: queue model, filters, sorting, and empty states

The landing page. Models update incrementally by stable pull-request identity,
filters and sorting are presentation, and every empty state says what would put
something there.

## Required direct evidence

- Replacing a projection with one differing in a few items produces exact
  insert, remove, move, and data signals, and no routine reset.
- Persistent indexes survive a replacement or are invalidated legally, by
  identity rather than by row.
- The model has no mutating entry point: the presentation cannot decide a
  bucket, and there is no method by which it could.
- Every state reads without colour and through a screen reader, as the defect
  card does.
- Each new assertion is shown to fail when the behaviour it constrains is
  neutered.
