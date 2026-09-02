---
id: BPR-0317
title: Implement anonymous Git cache epoch and diff authority
status: In Progress
rfc: RFC-BPR-0017
depends_on: [BPR-0301, BPR-0303, BPR-0531]
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

## Lane — 2026-09-02

[[BPR-0301]], [[BPR-0303]], and [[BPR-0531]] are terminal; claimed straight
into `30-in-progress/` by the integrator, its worker started in a worktree on
the head that carries all three, under the transition exception. What they
leave at this card's door: core's `pull_request_authority.Mode` (zero account
and viewer under public preview by rule; the comparison digest appends the
mode byte only for a non-authenticated authority so persisted anchors keep
matching — making that unconditional belongs with a domain bump and a
migration here); the application's `epoch_catalog_authority_key` and epoch
handoff records carrying `Mode` with the same two-rule validity; a host that
under public preview opens no epoch catalog and no Git cache and reports the
handoff `unavailable` through `epoch_handoff_unavailable`; and a transport
whose `can_acquire_smart` refuses every credential-free binding, so public
smart HTTP needs its own typed binding and factory beside
`anonymous_read_request`.
