---
id: BPR-0056
title: Implement bounded epoch history paging
status: In Progress
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0044, BPR-0045]
---

# Implement bounded epoch history paging

Complete the RFC-BPR-0003 history algebra that is absent from the initial
core slice. Add an exact `request_epoch_history_page` intent, core validation
of catalog identity/cursor/limit, a typed host request and matching terminal
events, and provider/Git/SQLite-free incremental page projections.

The application host must execute only bounded catalog pages, serialize each
completion, and reject stale authority, source, generation, incarnation,
revision, cursor, duplicate, failed, and cancelled results. Initial load and
continuation pages must preserve never-reused stable epoch identities and
must not copy the complete history into core state or reset an established Qt
model. The Qt page trigger and row model remain BPR-0046.

Use core transition tables and manual-executor fake-catalog tests for empty,
one-page, multi-page, exact-bound, invalid-cursor, revision-change,
identity-switch, cancellation, and reordered completion cases.
