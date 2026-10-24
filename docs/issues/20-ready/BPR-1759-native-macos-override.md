---
schema: "implement-rfc.card.v1"
id: "BPR-1759"
title: "Verify process-wide mimalloc on native macOS"
status: "Ready"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["mac"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on native macOS

## Scope and acceptance

Implement and observe the macOS override in the selected desktop, CLI,
update helper and other helpers (AC03). Probe actual C, C++, Qt/loaded-library allocation and
cross-module release on Apple Silicon and Intel. Check the signed installed
candidate, startup and shutdown. Include allocator-boundary stress needed by
AC06; a linked binary or stats report alone is insufficient.
