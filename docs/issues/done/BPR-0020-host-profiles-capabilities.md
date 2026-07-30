---
id: BPR-0020
title: Implement host profiles and capability probing
status: Done
rfc: RFC-BPR-0002
depends_on: [BPR-0019]
completed: 2026-07-29
---

# Implement host profiles and capability probing

Implement explicit GitHub.com and GHES host profiles, GHES 3.18 REST version
selection, HTTPS-only hostname/API-root validation, account identity, bounded
capability probes, cache keys and expiry policy, and explicit unavailable
results for review threads, viewed-file state, resolve/unresolve, and
unsupported features. Reject roots, profiles, and host changes derived from
pull-request content or redirect input.

Capabilities must come from recorded or observed contracts rather than a host
name assumption.

## Result

- Added copyable, non-movable validated GitHub.com and GHES 3.18 profile
  types with exact canonical host, HTTPS root, REST-version, provenance,
  proxy/trust, authentication, and redirect policy.
- Added one typed-POST, query-only, bounded capability probe for the exact
  review-thread, viewed-state, mark/unmark, and resolve/unresolve schema
  contracts. Incomplete or operational evidence remains explicitly
  unavailable and all-`Unknown`.
- Added independent complete GitHub.com and GHES 3.18 probe responses derived
  from official public schema snapshots. Manifests pin source, query, and
  response digests plus actual field counts.
- Added an application-owned exact-authority capability cache with bounded
  capacity and logical freshness, generated revisions, no unavailable-result
  caching, and exact/profile/identity invalidation.
- Added normalized file viewed state and per-thread resolve/unresolve target
  facts while keeping them independent from host schema support.
- Assigned capability-shaped production GraphQL selection, authenticated
  account observation, execution, paging, and unavailable-target
  normalization explicitly to BPR-0027.
- Preserved local and target-native build ownership; GitHub Actions has no
  build, test, package, release, or verification role.

## Evidence

- Clean Qt-enabled `dev` matrix: 23/23 passed.
- Clean Qt-free `core-only` matrix: 15/15 passed.
- Architecture boundary checks: 22/22 passed.
- Core boundary checks: 4/4 passed.
- Fixture security and provenance checks: 12/12 passed.
- Platform configuration checks: 4/4 passed.
- Documentation and original source-pack validators passed.
- `git diff --check`: passed.
- GitHub Actions workflow files and tracked workflow files: zero.
