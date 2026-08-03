---
id: BPR-0105
title: Implement host appearance persistence and caches
status: Backlog
rfc: RFC-BPR-0006
depends_on: [BPR-0104]
---

# Implement host appearance persistence and caches

Persist and restore appearance mode, semantic colors, and typography through a narrow settings port, and hold the host-side caches the RFC requires.

A theme or color change must invalidate presentation caches without touching review semantics. Token spans cache by blob and definition version independently of the active color theme, so switching theme cannot re-run lexical parsing.
