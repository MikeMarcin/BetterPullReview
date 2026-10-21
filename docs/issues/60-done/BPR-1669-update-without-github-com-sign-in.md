---
schema: "implement-rfc.card.v1"
id: "BPR-1669"
title: "Check for updates without a github.com sign-in"
status: "Done"
rfcs: []
depends_on: []
priority: "P1"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner report, 2026-10-20: the auto updater requires a signed-in github.com account, so a GHE-only setup never updates; releases are public now","reservation":"docs/issues/_reservations/BPR-1669--BPR-1669--adaf50a56cfe.json"}
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"bc856835c6fcef2594645ac3cb02b1a86fd4a8d0","evidence":{"kind":"observation","detail":"Native desktop composition offers a credential-free 'Public releases (no sign-in)' account first and selects it whenever no chosen account is on offer, so a GHE-only or signed-out profile checks and downloads. Anonymous GET of the pinned latest-release route returns 200."}},{"criterion":"AC2","outcome":"pass","revision":"bc856835c6fcef2594645ac3cb02b1a86fd4a8d0","evidence":{"kind":"observation","detail":"release_update_transport_contract covers the public lease (identity, kind, revocation, zero identity refused); before the transport fix it failed because the wire refused empty credentials. All 9 release_update tests and the format gate pass."}}]
---

# Check for updates without a github.com sign-in

The updater only received release access from an accepted github.com
native-HTTPS profile, so GHE-only and signed-out setups never checked.
Releases are public, so the pinned release routes need no credential.

## Acceptance

- AC1: With no signed-in github.com account, the updater checks and downloads
  from the same pinned public routes without an Authorization header. A
  signed-in github.com account stays selectable.
- AC2: A contract test covers the credential-free lease; updater tests and
  coding/documentation checks pass.

## Execution

`make_public_release_update_access` builds the existing release-only lease
with no credential; the native transport now accepts an empty credential and
simply omits Authorization. `release_update_desktop` offers that lease as the
first account and falls back to it when the stored binding is empty or gone.

The BPR-1670 native composition gate found the production-app review pin still
at its pre-change value. Review of `c5a906abe..aac127fa4` finds only
`c0d404c2c` inside that pinned scope, in `release_update_desktop.cpp`: the public
release-only account and missing-account fallback retain the existing capability,
closing and generation checks. Review mutation/publication composition and the
other two pins are unchanged. The manifest pin is resealed with this recorded
review; the original failed cold run and focused reseal controls are retained in
[the BPR-1670 evidence report](../../verification/BPR-1670-windows-suite-runtime.md).
