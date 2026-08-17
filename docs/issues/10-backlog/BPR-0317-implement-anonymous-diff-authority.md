---
id: BPR-0317
title: Implement anonymous Git cache epoch and diff authority
status: Backlog
rfc: RFC-BPR-0017
depends_on: [BPR-0301, BPR-0303]
---

# Implement anonymous Git cache epoch and diff authority

## Required final state

- Public GitHub.com smart HTTP uses explicit credential-free authority with
  zero Authorization/vault effect and the existing trust/proxy/redirect limits.
- A discriminated repository authority replaces the live path's assumption
  that every cache/catalog/epoch subject is a nonzero viewer/account: the
  authenticated branch retains its exact account identity, while the anonymous
  public branch carries only canonical GitHub.com repository/profile identity.
  No zero/default/synthetic account is representable, and the two namespaces
  cannot alias or leak objects or projections.
- The SQLite epoch catalog receives a versioned authority discriminator and a
  transactional schema migration that maps every existing row to the
  authenticated branch without changing its account/repository identity.
  Unknown versions, malformed discriminators, partial migration, substitution,
  collision, downgrade, and mixed-authority rows fail closed.
- The selected file reaches the existing diff computation and Qt projection
  with exact repository/path/base/head/content identity and bounded cancellation.
- Private/GHES/mutation/push, stale switch, malformed pack, substitution,
  cache-reuse, and authenticated non-regression tests fail closed.
- Exact core/application/Git/SQLite tests plus a real installed-Git and SQLite
  two-process restart course prove migrated authenticated rows remain readable,
  anonymous rows reopen without viewer identity, mode switching never reuses the
  other namespace, and no credential is discovered or persisted.
