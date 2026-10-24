---
schema: "implement-rfc.card.v1"
id: "DRAFT-0005"
title: "Reconcile mimalloc runtime and corresponding source in packages"
status: "Backlog"
rfcs: ["RFC-DRAFT-0028"]
depends_on: ["DRAFT-0002", "DRAFT-0003", "DRAFT-0004"]
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
