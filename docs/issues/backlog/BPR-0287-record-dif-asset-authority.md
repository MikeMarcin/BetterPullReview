---
id: BPR-0287
title: Record Dif asset authority and pre-creation provenance
status: Backlog
rfc: RFC-BPR-0019
depends_on: [BPR-0286]
---

# Record Dif asset authority and pre-creation provenance

Obtain a dated, digest-bound product-owner or rightsholder decision before any
production asset is derived from the supplied Dif references. The decision is
separate from the later visual verdict and cannot be inferred from possession,
repository access, or the instruction to implement the RFC.

## Confirm exact asset authority

Prepare one local offline attended-review page bound to the recorded hashes of
`dif_ref_sheet.png`, `mascot_group.png`, and `nib-animation-bootstrap.zip`.
The page must present one identity-only subject with an exact accept-or-reject
export. The decision must name
the creator or rightsholder basis, authorize or refuse reference-informed
derivative creation and repository distribution, state commercial and source
distribution scope, identify required attribution/notices/SBOM treatment, and
confirm that no Nib, Mel, or AllTheThings art or identity may enter BPR.

## Completion evidence

- The immutable decision subject, page input, and generated offline package are checked in under
  `docs/verification/reviews/dif-provenance-v001/`.
- The returned complete verdict export is dated when recorded, binds the
  decision-maker capacity representation, selects exactly one outcome, and is
  validated against the generated batch manifest.
- The initial `provenance.json` authority fields reproduce the decision without
  asserting rights beyond its exact scope.
- Refusal or insufficient authority leaves BPR-0289 blocked and adds no product
  asset; approval authorizes creation but not final visual acceptance.

## Awaiting user

Status: `awaiting-user` for a complete accept-or-reject export from the
[digest-bound local review page](../../verification/reviews/dif-provenance-v001/offline-package/index.html).
The package copies and binds decision subject
`sha256:d86ec61323c4f7304810253b392ed0482563b4f2f382fb64f4a7ff0b668a909d`;
the package manifest records the resulting subject digest and batch identity.
No production asset is created until the returned export validates exactly.
