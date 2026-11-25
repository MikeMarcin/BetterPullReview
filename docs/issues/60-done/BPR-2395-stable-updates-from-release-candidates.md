---
schema: "implement-rfc.card.v1"
id: "BPR-2395"
title: "Find a newer stable build from a release candidate"
status: "Done"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-1799"]
acceptance: [{"criterion":"AC1","outcome":"pass","revision":"e6fb9d709aa8d3ea8470ec1e23e9f458595b6133","evidence":{"kind":"observation","detail":"The exact published build 95 core returns current for stable 0.5.3 build 99. The revised application_release_update_public_path and its isolated --numbered-build course exercise Check for Updates, show the installed build 95 and target build 99, then download, verify and refresh the exact candidate before one installer handoff."}},{"criterion":"AC2","outcome":"pass","revision":"e6fb9d709aa8d3ea8470ec1e23e9f458595b6133","evidence":{"kind":"command","detail":"core_release_update and application_release_update pass. Equal and lower builds, older marketing versions, drafts, prereleases, malformed and ambiguous assets, SemVer metadata, the legacy ZIP and skipped-version behavior are covered."}},{"criterion":"AC3","outcome":"pass","revision":"e6fb9d709aa8d3ea8470ec1e23e9f458595b6133","evidence":{"kind":"observation","detail":"Native staging and transaction replay pass on Apple Silicon. Staging the published ZIP against the signed build 95 copy returned success for build 99, and selecting build 100 for those same bytes returned invalid_bundle. The optional signed course subsequently refused a separate startup eligibility assertion during slow Gatekeeper assessment. The fixture helper rehydrates the full candidate including its digest and requires the healthy-start receipt before cleanup. No published installation was replaced."}}]
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
