---
id: BPR-0044
title: Implement review-epoch catalog persistence and aliases
status: Backlog
rfc: RFC-BPR-0003
depends_on: [BPR-0041, BPR-0043]
---

# Implement review-epoch catalog persistence and aliases

Implement a narrow host-owned epoch catalog with schema/version authority,
transactions, serialized writes, crash recovery, and exact pull-request and
account scope. Bind every catalog partition, ordinal, and retained-ref identity
to the same validated nonreversible authority-scope ID used by the shared
physical cache. Persist ordinal chronology, exact base/head and branch
identities, structured lineage/change facts, deterministic cause, observed
time, provider snapshot, material-equivalence identity, review association,
availability, and retained-ref identity without source bodies or credentials.

Coordinate catalog and private-ref commit so a stale, canceled, duplicate, or
failed job cannot publish an epoch. A different observed revision with
equivalent material remains a distinct epoch. Resolve Branch base, Latest,
Previous upload, My last reviewed, and arbitrary retained anchors to exact
OIDs; never silently remap an explicit unavailable or pruned selection.

Add bounded/paged history observations and explicit prune/delete transactions.
Automatic retention loss is forbidden.
