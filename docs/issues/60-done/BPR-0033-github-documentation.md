---
schema: "implement-rfc.card.v1"
id: "BPR-0033"
title: "Document RFC-BPR-0002 provider and credential behavior"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0029","BPR-0031","BPR-0032"]
completed: "2026-07-30"
---

# Document RFC-BPR-0002 provider and credential behavior

Update affected agent and product documentation for architecture ownership,
typed state flow, host configuration, GitHub.com and GHES compatibility,
authentication modes, native credential stores, corporate CA/proxy behavior,
capability probing, errors, security limits, testing, troubleshooting, and
operator-safe read-only verification.

Record explicit no-impact regions and ensure no document selects GitHub
Actions as a product build or verification surface.

## Result

- Reconciled the product, architecture, UX, development, reference, packaging,
  fixture, and verification documentation against the immutable RFC base and
  the completed BPR-0029 through BPR-0032 evidence.
- Corrected the appearance narrative to record the completed owner-authorized
  native macOS same-process system Light-to-Dark gate. Application-local and
  offscreen exercises remain explicitly narrower evidence.
- Added operator-safe troubleshooting for fixed CLI unavailability, canonical
  GHES configuration, authoritative CA/proxy behavior, fail-closed native
  vault outcomes, non-mutating recorded/loopback verification, exact
  credential deletion, and the abrupt-crash orphan limitation.
- Updated the external baseline with the audited GitHub CLI 2.96.0 source and
  retained 2.95.0 only as a historical comparison.
- Resolved every documentation-ledger disposition and recorded the complete
  no-impact/deferred surface. Windows target-native execution remains
  `Planned` in BPR-0036 and is nonblocking for all other work.
- Confirmed that GitHub Actions has no build, test, package, release, or
  verification role and that no workflow file exists.

## Evidence

- Inspected `git diff --name-status
  bfa9ab3ad6b08fbb22b697ed6fe6688202d3335a..HEAD` and the live working-tree
  delta, then reviewed every applicable `AGENTS.md` boundary.
- `python3 scripts/verify_documentation.py --root .` reported
  `DOCUMENTATION_OK`; its command-line help confirmed that the validator has
  no hidden immutable-base mode, so the semantic base review was performed
  directly.
- The focused `dev` documentation/platform-configuration matrix passed 2/2,
  and the `core-only` documentation test passed 1/1.
- The original downloaded pack validator reported `OK: 14 RFCs, 17 normative
  documents, architecture v2 enforced`.
- Stale-claim, tracked/on-disk workflow-absence, current-delta, immutable-range
  `git diff --check`, and issue/document agreement gates passed.
