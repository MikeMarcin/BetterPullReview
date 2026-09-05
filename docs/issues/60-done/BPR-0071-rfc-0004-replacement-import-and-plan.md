---
schema: "implement-rfc.card.v1"
id: "BPR-0071"
title: "Import and reconcile the RFC-BPR-0004 replacement"
status: "Done"
rfcs: ["RFC-BPR-0004"]
depends_on: ["BPR-0067"]
---

# Import and reconcile the RFC-BPR-0004 replacement

Record immutable RFC base `243295a70756b2e63a785ed11b14647b6b0a4c6d`
and import replacement pack
`betterpullreview-rfc-bpr-0004-diff-alignment-scroll-replacement-v1`.
Replace the canonical RFC and imported issue plan in place, preserve the RFC
ID/filename/dependency order and downstream numbers, and allocate BPR-0071
through BPR-0083 from the live next-ID source.

Reconcile the RFC index, system/diff architecture, workbench UX, dedicated
claim/fixture/manual-verification ledgers, master verification matrix,
downstream RFC-BPR-0005 through RFC-BPR-0014 contracts, board dependencies,
and the product-owner scheduling exception for open RFC-BPR-0003 closeout
claims. Do not import the non-normative P4Merge assets as product resources.

## Result

- The pack validator accepted all 22 source files and the one canonical
  RFC-BPR-0004 replacement identity.
- The live canonical RFC and plan are replaced in place; exactly one active
  RFC-BPR-0004 remains and every downstream RFC ID is unchanged.
- The alignment-space architecture, natural-layout/link-map UX, direct claim
  matrix, fixture catalog, and rendered verification protocol are live.
- No combined RFC document exists in this repository; the canonical RFC index,
  live plan, and issue board are the applicable aggregate surfaces.
- Read-only source/test audit found no early RFC-BPR-0004 code, follower-scroll
  coupling, normalized scrollbar coupling, synthetic alignment rows, competing
  mappings, or static-mockup-only implementation claim to remediate.
- RFC-BPR-0003 remains honestly open for BPR-0068/BPR-0069/BPR-0070 and Windows
  closeout. Product-owner direction permits RFC-BPR-0004 implementation but
  does not permit out-of-order Auto-Verified closeout.
