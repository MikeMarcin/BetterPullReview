---
id: BPR-0039
title: Plan RFC-BPR-0003 implementation and verification
status: Done
rfc: RFC-BPR-0003
depends_on: [BPR-0034]
completed: 2026-07-30
---

# Plan RFC-BPR-0003 implementation and verification

Reconcile the imported RFC with the live RFC-BPR-0002 implementation, allocate
the complete issue set, and establish claim, architecture, authority,
documentation, platform, performance, and security ledgers before
implementation begins.

Record the product-owner-directed dependency exception explicitly:
RFC-BPR-0002 remains the closure frontier until BPR-0036 supplies real Windows
evidence, while RFC-BPR-0003 may become the implementation frontier. This
exception must not mark either RFC Auto-Verified out of canonical order or
launder an unverified Windows claim into a later RFC.

## Result

- Froze the implementation base at
  `a600ab2c29d2b4088a3f2598dbb817c35af2eafd` and recorded RFC-BPR-0002 as
  the closure frontier and RFC-BPR-0003 as the implementation frontier.
- Allocated BPR-0040 through BPR-0054 and advanced `NEXT_ID` to BPR-0055.
  BPR-0054 preserves canonical closure by depending on the RFC-BPR-0002
  closeout, while BPR-0053 isolates nonblocking real-Windows evidence.
- Assigned separate core, application-host, installed-Git, native smart
  transport, bare-cache, epoch-catalog, Qt, and composition responsibilities.
  Installed Git receives no credential or independent network authority.
- Specified exact authority and asynchronous identity, algorithm-tagged OIDs,
  epoch cause and alias decisions, authority-qualified private retained refs
  and catalog partitions, failure-atomic catalog/ref publication, paging,
  quotas, explicit deletion, cancellation, and crash recovery.
- Established a falsifiable claim matrix and platform/security/UX/
  documentation/performance ledgers. GitHub Actions remains prohibited as a
  build, test, package, release, or verification surface.
- Preserved RFC-BPR-0005's distinct final presentation-polish/native visual
  acceptance and RFC-BPR-0006's selectable appearance/font/syntax ownership,
  including a fresh-install System default.

## Evidence

- Three independent read-only reviews covered core/host allocation, installed
  Git and cache security, and Qt/public-path verification. Their material
  ambiguities and hostile cases are assigned to BPR-0040 through BPR-0050.
- The hardened documentation validator and its four-case dependency-graph
  contract pass; the refreshed focused CTest gates pass 3/3 for `dev` and 2/2
  for `core-only`.
- The original source-pack validator, tracked/on-disk workflow-absence scans,
  independent dependency/status and immutable-base audits, and
  `git diff --check` pass.
