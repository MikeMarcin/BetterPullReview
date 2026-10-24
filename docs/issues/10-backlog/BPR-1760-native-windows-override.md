---
schema: "implement-rfc.card.v1"
id: "BPR-1760"
title: "Verify process-wide mimalloc on native Windows x64"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["win"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on native Windows x64

## Scope and acceptance

Implement and observe the Windows override with the actual dynamic CRT and
packaged DLL closure (AC04). Probe C, C++, Qt/loaded-library allocation and
cross-module release in the desktop and selected helpers/CLI on a native
Windows 11 x64 host. Include startup, shutdown and allocator-boundary stress
needed by AC06; record import/runtime identity rather than assuming a linked
import library proves redirection.
