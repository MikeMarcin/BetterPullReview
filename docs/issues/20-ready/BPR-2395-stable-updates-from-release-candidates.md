---
schema: "implement-rfc.card.v1"
id: "BPR-2395"
title: "Find a newer stable build from a release candidate"
status: "Ready"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-1799"]
---

The macOS updater compares only the marketing version, so build 95 of 0.5.3
reports that it is current when the stable 0.5.3 release contains build 99.

- AC1. The normal macOS updater offers and carries an exact newer numbered
  build from the stable release through verification and the install refresh,
  even when the marketing version is unchanged. Cover the 95-to-99 case.
- AC2. Equal or older cohorts and older marketing versions cannot replace the
  installation. Drafts, prereleases and ambiguous or malformed candidates
  retain fail-closed behavior. Legacy stable checks and skipped marketing
  versions retain their existing semantics.
- AC3. Native validation binds any selected build number to the signed
  bundle, and the replacement helper reconstructs the same candidate from
  its journal. Preserve the existing digest, trust and explicit install
  checks.
