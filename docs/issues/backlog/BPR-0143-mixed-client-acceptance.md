---
id: BPR-0143
title: Mixed-client acceptance against a designated repository
status: Backlog
rfc: RFC-BPR-0008
depends_on: [BPR-0142]
---

# Mixed-client acceptance against a designated repository

RFC-BPR-0008 requires that mixed-client edits, replies, deletion, resolution,
and reopening are preserved. BPR-0138 proves the reconciliation against a fake
provider, which establishes that the projection handles each shape correctly.
It does not establish that GitHub produces those shapes.

## Exactly what is missing

The designated repository and token from BPR-0142, plus a **second client**
acting on the same pull request — the web UI is sufficient — while the
workbench is open.

## Required direct evidence

- A comment created externally appears after refresh, with its GitHub id.
- A comment edited externally updates rather than duplicating.
- A comment deleted externally disappears rather than lingering.
- A thread resolved and reopened externally reaches the workbench in both
  directions.
- A local assumption is shown never to overwrite an unknown external change
  without an explicit user action.
