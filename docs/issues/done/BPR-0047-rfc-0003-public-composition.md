---
id: BPR-0047
title: Compose the public Git cache and epoch path
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0042, BPR-0043, BPR-0044, BPR-0045, BPR-0046, BPR-0055]
---

# Compose the public Git cache and epoch path

Wire the concrete native smart transport, bounded app-owned protocol-v2
client, credential-free installed-Git adapter, cache/catalog ports, epoch
host, core state, and Qt comparison controls in the composition root with
explicit worker and shutdown order.

Drive a real QAction through the recorded GitHub fixture and a real installed
Git process against a local deterministic smart-protocol fixture into cache
creation/reuse, durable epochs, default/alias/arbitrary pair selection, and
visible exact OIDs for both GitHub.com and GHES authority shapes. A second
process invocation must prove persistence and cache reuse.

The tests-disabled Release application must select production factories while
excluding fixture flags, paths, symbols, and fake factories. No GitHub
mutation or live private repository is required.

## Implementation audit frontier

Begin at the live `github_application` composition root and its recorded public
fixture seam. Inventory construction/destruction order, accepted pull-request
handoff delivery, native smart-transport leases, application-data ownership,
worker/decision executors, test-only command-line admission, and the rendered
self-test path before adding the epoch host. Reuse the BPR-0042 through
BPR-0046 ports and controls; do not introduce a parallel cache, catalog,
selection reducer, fixture-only window, or GitHub Actions path.

## Outcome

- Added one narrow GitHub-to-epoch handoff port. The GitHub host emits the
  provider-free record only after the matching core snapshot acceptance on the
  serialized decision lane and emits exact invalidation before authority
  replacement. The record contains the validated host/account/repository/PR,
  snapshot/generation, typed base ref and OIDs, numeric current-viewer review,
  and a revocable smart-transport authority; it contains no provider object,
  credential, source observation, clock value, Qt object, or path.
- Composed the production installed-Git cache, SQLite epoch catalog, native
  entropy/clock ports, dedicated epoch worker/host, and existing Qt comparison
  control in `github_application`. Application-data and Git executable
  selection remain composition-owned. Orderly shutdown revokes the smart
  authority, waits for both hosts and actor queues, then drains the epoch,
  provider, and vault workers.
- Added exact REST review database IDs to the closed read contract so
  `My last reviewed` crosses the handoff as a stable numeric identity rather
  than a login, node-label approximation, timestamp, or container-local text.
- Added a deterministic local smart-protocol fixture backed by checked-in
  self-contained packs from a real synthetic three-commit graph. Seed and
  reuse phases drive the real public
  connect/open/refresh actions and Qt selectors against one durable
  application-data root for GitHub.com and GHES. Restart loads the retained
  catalog/cache with zero smart acquire, discovery, `ls-refs`, or fetch calls.
- Extracted the shared core SHA-256 primitive used by authority/base-branch
  derivation and maintenance target sealing. Canonical empty and `abc` vectors
  plus the existing exact maintenance digest guard the implementation.

## Direct verification

- `cmake --build --preset dev -j 8` passed with the complete application,
  host, adapters, and test graph.
- The focused core, GitHub host/contract, and real rendered public-path matrix
  passed, with the final host plus two rendered cases passing 3/3 in 46.48
  seconds. Both
  GitHub.com and GHES public paths reached
  an exact visible base/head comparison through the real application.
- `application_epoch_persistence_github-com` and
  `application_epoch_persistence_ghes-3_18` passed 2/2 in 191.05 seconds. Each
  test ran seed and reuse as separate processes, exercised Whole PR, Previous
  upload, My last reviewed, arbitrary, reverse, and identical selections, and
  required zero smart calls during reuse.
- `application_composition_contract` passed in 85.74 seconds from a fresh
  tests-disabled Release tree. Its compile graph and executable contain none
  of the fixture translation unit, factory, CLI flags, phase markers,
  recordings, fixture paths, or test macros.
- The focused epoch core/catalog/storage/maintenance checks pass, including
  canonical SHA-256 vectors and the immutable maintenance target digest. No
  GitHub endpoint, private repository, credential, mutation, or GitHub Actions
  workflow participates in any of this evidence.
- The final Qt-free `core-only` matrix passed 27/27 in 116.29 seconds. The
  architecture/protocol/fixture-security/documentation/dependency/runtime
  closeout group passed 7/7, `verify_documentation.py` reported
  `DOCUMENTATION_OK`, tracked and on-disk GitHub Actions workflow counts were
  0/0, and `git diff --check` was clean.

## Documentation and simplification

Updated the system and provider architecture, security model, build/test
guide, fixture catalog, issue board, and RFC-BPR-0003 verification ledger.
The code-simplifier pass extracted the narrow handoff header and shared hash,
replaced the forwarding transport wrapper with weak shared revocation leases,
removed app-layer fixture process ownership in favor of fixed pack bytes, and
kept the explicit authority, state-machine, cancellation, and shutdown
branches whose distinctions are verified behavior.

BPR-0048 retains the broad combined failure/reordering/resource matrix,
BPR-0049 retains target-native UX and accessibility evidence, and BPR-0053
retains real Windows execution for the product owner's machine. RFC-BPR-0005
retains final presentation polish; RFC-BPR-0006 retains selectable
System/Light/Dark/high-contrast themes, fonts, and syntax presentation. This
card adds no GitHub mutation, hosted workflow, diff computation, or package
format.
