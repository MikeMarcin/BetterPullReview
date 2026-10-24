---
schema: "implement-rfc.card.v1"
id: "BPR-1762"
title: "Reconcile mimalloc runtime and corresponding source in packages"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1759", "BPR-1760", "BPR-1761"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Reconcile mimalloc runtime and corresponding source in packages

## Scope and acceptance

Integrate the actual selected mimalloc runtime file graph with BPR's macOS,
Windows and Ubuntu package closure and standalone CLI install (AC07). Update
the exact dependency inventory, installed MIT notice, SPDX and separate
versioned corresponding-source asset. Preserve native signing/hardening,
load-path, source digest and release refusal checks; prove removal and reject
missing or extra runtime files. This is package qualification, not release
publication or a security-owner vulnerability disposition.
