---
schema: "implement-rfc.card.v1"
id: "DRAFT-0002"
title: "Verify process-wide mimalloc on native macOS"
status: "Backlog"
rfcs: ["RFC-DRAFT-0028"]
depends_on: ["DRAFT-0001"]
platforms: ["mac"]
---

# Verify process-wide mimalloc on native macOS

## Scope and acceptance

Implement and observe the macOS override in the selected desktop, CLI,
update helper and other helpers (AC03). Probe actual C, C++, Qt/loaded-library allocation and
cross-module release on Apple Silicon and Intel. Check the signed installed
candidate, startup and shutdown. Include allocator-boundary stress needed by
AC06; a linked binary or stats report alone is insufficient.
