---
schema: "implement-rfc.card.v1"
id: "BPR-0307"
title: "Simplify the anonymous public-preview implementation"
status: "Done"
rfcs: ["RFC-BPR-0017"]
depends_on: ["BPR-0314","BPR-0315","BPR-0316"]
---

# Simplify the anonymous public-preview implementation

## Required final state

- A focused simplifier pass removes duplicated normalization, broad service
  access, unnecessary serialization, stale comments, and fixture/test bypasses.
- Credential-free authority, header/vault absence, unavailable observations,
  cache invalidation, 404 neutrality, rate behavior, and zero-request refusals
  remain explicit and independently auditable.
- The pass occurs after macOS enablement and Linux/Windows unavailability
  selection, so it covers the complete cross-platform implementation.
- Scoped format, strict tidy, focused tests, Release contracts, docs, and diff
  checks pass after simplification.

## Claimed — 2026-09-04

BPR-0314/BPR-0315 retain unavailable selections; BPR-0316 enabled macOS at
`a319dd51`. Review the complete anonymous admission/normalization, host/cache
authority, projection, and composition surfaces using code-simplifier. Reuse
the just-passing 19-selector Mac/Release course and strict app tidy where
inputs remain unchanged. The BPR-0316 isolated build root is retained only for
this immediate verification consumer and removed afterwards.

## Completed — 2026-09-04

Reviewed `anonymous_read_policy`, the shared `signed_out_read_identity` and
operation admission, provider/pager correlation, host activation and projection,
epoch handoff, installed-Git mode-bound cache identity, SQLite mode-bound keys,
and Qt availability/conversation projection. Their distinct guards encode
different authority boundaries; no duplicated normalizer, broad service access,
unnecessary serialization, or newly introduced fixture bypass needed removal.
The BPR-0316 composition cleanup already removed the stale all-platforms-
unavailable commentary. This pass corrects two remaining comments: anonymous
GHES refusal is product policy, not a claim about every enterprise server.

Only comments changed, so the 19/19 Mac course, tests-disabled Release gates,
and 3/3 composition/isolation checks from `a319dd51` remain valid. Strict tidy
on the changed host translation unit passed; the two app translation units
already passed strict tidy before this comment-only pass. Scoped format,
documentation structure, and diff checks pass. No behavior or public API changed.

Removed `/Users/jaeger/Documents/GitHub/BetterPullReview/build/bpr0316-macos-20260904`
after its course/contract/tidy consumers completed, plus the consumed BPR-0316
and BPR-0307 logs and temporary BPR-0541 response files. No task-created worktree
remains. BPR-0308 now owns the exact public-preview documentation update.
