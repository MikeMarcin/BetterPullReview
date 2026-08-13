---
id: BPR-0283
title: Simplify the device-authorization implementation
status: Backlog
rfc: RFC-BPR-0016
depends_on: [BPR-0281, BPR-0282]
---

# Simplify the device-authorization implementation

## Required final state

- A focused simplifier pass removes duplicated policy, broad service access,
  unnecessary serialization, stale comments, whole-state refreshes, and test
  seams that can bypass production selection.
- Core, host, adapter, Qt, and composition ownership remains explicit; security
  checks, erasure, cancellation, and bounded-state code are not collapsed into
  clever or unauditable helpers.
- Scoped format, strict tidy, focused tests, Release contracts, documentation,
  and `git diff --check` pass after simplification.
