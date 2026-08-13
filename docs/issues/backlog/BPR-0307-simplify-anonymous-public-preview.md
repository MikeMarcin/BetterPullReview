---
id: BPR-0307
title: Simplify the anonymous public-preview implementation
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0305, BPR-0306]
---

# Simplify the anonymous public-preview implementation

## Required final state

- A focused simplifier pass removes duplicated normalization, broad service
  access, unnecessary serialization, stale comments, and fixture/test bypasses.
- Credential-free authority, header/vault absence, unavailable observations,
  cache invalidation, 404 neutrality, rate behavior, and zero-request refusals
  remain explicit and independently auditable.
- Scoped format, strict tidy, focused tests, Release contracts, docs, and diff
  checks pass after simplification.
