---
id: BPR-0287
title: Record Dif asset authority and pre-creation provenance
status: In Progress
rfc: RFC-BPR-0019
depends_on: [BPR-0286]
---

# Record Dif asset authority and pre-creation provenance

Obtain a dated, digest-bound product-owner or rightsholder decision before any
production asset is derived from the supplied Dif references. The decision is
separate from the later visual verdict and cannot be inferred from possession,
repository access, or the instruction to implement the RFC.

## Confirm exact asset authority

Prepare one operator packet bound to the recorded hashes of `dif_ref_sheet.png`,
`mascot_group.png`, and `nib-animation-bootstrap.zip`. The decision must name
the creator or rightsholder basis, authorize or refuse reference-informed
derivative creation and repository distribution, state commercial and source
distribution scope, identify required attribution/notices/SBOM treatment, and
confirm that no Nib, Mel, or AllTheThings art or identity may enter BPR.

## Completion evidence

- The immutable decision subject and operator packet are checked in under
  `docs/verification/reviews/dif-provenance-v001/`.
- The returned decision is dated, names the decision-maker capacity, selects
  exactly one outcome, and is validated against the subject digest.
- The initial `provenance.json` authority fields reproduce the decision without
  asserting rights beyond its exact scope.
- Refusal or insufficient authority leaves BPR-0289 blocked and adds no product
  asset; approval authorizes creation but not final visual acceptance.
