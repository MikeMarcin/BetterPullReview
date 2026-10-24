---
schema: "implement-rfc.card.v1"
id: "BPR-1761"
title: "Verify process-wide mimalloc on Ubuntu x64"
status: "Backlog"
rfcs: ["RFC-BPR-0093"]
depends_on: ["BPR-1758"]
platforms: ["linux"]
metadata: {"milestone_plan":{"id":"mimalloc-process-wide-v1","path":"docs/rfcs/plans/mimalloc-process-wide-v1/milestone-plan.json"}}
---

# Verify process-wide mimalloc on Ubuntu x64

## Scope and acceptance

Implement and observe the Linux override and installed ELF closure (AC05).
Probe C, C++, Qt/loaded-library allocation and cross-module release in the
desktop and selected helpers/CLI on the selected Ubuntu 26.04 x64 WSL2/WSLg
station. Show that Git, `gh` and
user agent children receive no allocator injection, and run allocator-boundary
stress needed by AC06. Distinguish an installed Linux course from a cross
build or link-only check.
