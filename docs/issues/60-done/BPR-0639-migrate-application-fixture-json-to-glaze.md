---
schema: "implement-rfc.card.v1"
id: "BPR-0639"
title: "Migrate application fixture JSON to Glaze"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0638"]
acceptance: [{"criterion":"Replace remaining application fixture Qt JSON with Glaze while preserving replay admission","outcome":"pass","revision":"e05d00553d72731880df55072a434a9e526c6415","evidence":{"kind":"observation","detail":"Both owned files have no Qt JSON entry points. Replay uses typed manifests, strict method/catalog checks, sorted headers and unchanged body/head substitution; device projections preserve recorded values and synthetic stand-ins. Composed GitHub and GHES public read courses pass, as do registered-device and two-renewal courses including exact synthetic stored-material checks."}},{"criterion":"Preserve capture schema and source identities through the real composed path","outcome":"pass","revision":"e05d00553d72731880df55072a434a9e526c6415","evidence":{"kind":"observation","detail":"Initial four-profile capture contract passes in 72.32s. After final hex-buffer lifetime fix/rebuild, affected capture contract passes in 73.33s; source/blob/line/navigation/geometry/role/connector consumers pass. All four generated documents match retained v4 recursive member inventory and scalar kinds. Accepted recordings/captures are unchanged."}},{"criterion":"Preserve test/production boundaries and verify locally","outcome":"pass","revision":"e05d00553d72731880df55072a434a9e526c6415","evidence":{"kind":"observation","detail":"Glaze capture include/helpers stay in BPR_BUILD_TESTING and fixture source/link selection remains BUILD_TESTING-only. No new public JSON types or effect boundary. App builds; initial registered isolated 5/5 selection passes in 109.85s, followed by the affected final capture check. Unchanged non-capture evidence is reused. Documentation, changed-line formatting and diff checks pass. No live provider, production session, credential, Actions, push or paid CI operation ran."}}]
---

# Migrate application fixture JSON to Glaze

## Bounded acceptance and ownership

Own src/app/src/github_application.cpp and github_public_path_fixture.cpp plus
focused composition fixtures/manifests if affected. Replace remaining Qt JSON
with Glaze. Preserve exact fixture documents, fault injection, accepted source
identities and production/test boundaries. Verify normal composition and
selected synthetic public replay without live network or production sessions.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.

## Implementation and evidence

Selection `3d6f5b25`; claim `67f962e3d27a093a8bfdf87656afb62b21eb026e`.
Source `e05d00553d72731880df55072a434a9e526c6415` removes all Qt JSON from both
owned application files. Replay manifests now decode private typed Glaze
interaction records; operation order, strict method parsing, sorted headers,
nullable request bodies, head substitutions, source descriptors and failure
injection remain unchanged. Device projections decode typed status/field records
and owned retained JSON values, then serialize the same synthetic transient
stand-ins in sorted field order. Every read/write result is checked. Explicit
Glaze metadata keeps these records private without relying on automatic member
reflection for anonymous-namespace types.

Capture facts use owned Glaze object tuples, exact integer/double/bool values
and UTF-8 strings. All v4 schema fields and values remain; indentation remains
four spaces with a final newline, while object member order follows the named
construction order. No accepted capture or recording was rewritten. The hex
capture helper now retains its canonical-hex buffer while copying its view,
correcting a pre-existing temporary-buffer lifetime issue. All new Glaze code
remains within the existing test-enabled source/include/link boundaries.

Local Mac verification:

- Development application builds with both changed composition sources.
- Initial registered network-isolated integration: 5/5 pass in 109.85s:
  `application_device_authorization_registered_course` 5.96s,
  `application_device_authorization_refresh_course` 7.90s,
  `application_github_public_path_github_com` 11.91s,
  `application_uiforetw_selected_diff_contract` 72.32s and
  `application_github_public_path_ghes_3_18` 11.74s.
- After the final hex-buffer fix and rebuild, the affected
  `application_uiforetw_selected_diff_contract` passes again in 73.33s
  (73.34s total). It exercises the production drawer/selection path and consumes
  capture facts from light, dark and both high-contrast profiles. The other four
  passing checks do not enter the changed capture helper and retain their
  relevance without repetition.
- All four generated capture documents have the same complete recursive member
  inventory and scalar kinds as the retained v4 facts in the
  intraline-badge-and-dark-v001 packet. The contract independently validates
  source/blob identities, selected lines, geometry, roles, navigation and
  connector facts. This is parser/writer compatibility evidence, not a new
  attended visual verdict.
- Documentation gate, changed-line formatting and `git diff --check` pass.
  Unrelated pre-existing formatting in github_application.cpp was preserved.
- Source inventory finds no QJson, fromJson or toJson entry points under src.
  Nine C++ test/helper files remain in BPR-0640; the Python architecture guard
  remains intentional. BPR-0612 and final integration are still open.

All runs were local synthetic fixtures behind registered isolation wrappers.
Production sign-in/credentials were untouched; no live GitHub, Actions, push or
paid CI ran. Consumed temporary migration/build logs, the two public replay
images and the uiforetw-selected-diff-contract output root were removed after
their tests terminated. Reusable build trees remain. Further performance work
continues to wait for the complete migration.
