---
schema: "implement-rfc.card.v1"
id: "BPR-0612"
title: "Replace all remaining Qt JSON parsing and serialization with Glaze"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0610","BPR-0636","BPR-0637","BPR-0638","BPR-0639","BPR-0640"]
acceptance: [{"criterion":"complete-inventory","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"All eight production/composed-fixture and nine test/helper Qt JSON migrations are complete in BPR-0636 through BPR-0640. Fresh src/tests inventory finds only the intentional Python boundary guard."}},{"criterion":"typed-boundaries-and-provenance","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Scoped diff against migration base preserves pure core and public ports. Private Glaze wire records/raw ownership use exact integers; pinned Glaze 8.3.0 provider and MIT provenance are unchanged."}},{"criterion":"contract-compatibility","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Slice records retain required/null/unknown/duplicate, Unicode, full-input, byte/depth/record/authority/pagination/error/privacy coverage. Frozen literal request bytes and restart recovery pass; runner overwritten-member review counterexamples are fixed and tested."}},{"criterion":"final-integration","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Five final helper checks pass. Final integration retains twelve passing lanes plus the corrected round-trip source-binding rerun (50.97s), including local auth, refresh, read, anonymous, provider, two recovery paths, GitHub/GHES composition and large-review replay."}},{"criterion":"performance-order-and-closeout","outcome":"pass","revision":"e80f7a15c1bfcb98c123805d65b44efff5009feb","evidence":{"kind":"observation","detail":"Parser-sensitive benchmark and loaded-close timings/native memory refreshed on the unchanged large-review fixture before resuming RFC13 work. Existing 160/96 MiB budgets pass. Documentation and scoped board checks pass; no live session, Actions, push or paid CI operation."}}]
---

# Replace all remaining Qt JSON parsing and serialization with Glaze

The owner explicitly requested a backlog card for the full Qt-to-Glaze JSON
migration on 2026-09-05. BPR-0610 introduces pinned Glaze and converts the new
whole-review contract. This card tracks the remaining production code and test
helpers. It is a later migration under the foundational adapter architecture,
not a reopening of RFC-BPR-0002's already verified acceptance or a prerequisite
for BPR-0610. Select and split it into bounded slices before implementation.

## Scope and starting inventory

Replace Qt JSON parsing, DOM construction, conversion and serialization with
Glaze across repository-owned C++ code, including fixtures and test helpers.
Qt remains the UI framework. Python/CMake JSON tooling and third-party source
are outside this Qt-to-Glaze migration.

Production and composed-fixture sources at allocation:

- `src/adapters/github/src/github_capability_probe.cpp`
- `src/adapters/github/src/github_device_authorization_contract.cpp`
- `src/adapters/github/src/github_mutation_contract.cpp` (remaining old contracts)
- `src/adapters/github/src/github_paged_read.cpp`
- `src/adapters/github/src/github_read_contract.cpp`
- `src/adapters/github/src/github_read_normalizer.cpp`
- `src/app/src/github_application.cpp`
- `src/app/src/github_public_path_fixture.cpp`

Test sources:

- `tests/adapters_qt/github_thread_map_tests.cpp`
- `tests/github_contract/github_anonymous_read_contract_tests.cpp`
- `tests/github_contract/github_capability_probe_tests.cpp`
- `tests/github_contract/github_device_authorization_contract_tests.cpp`
- `tests/github_contract/github_mutation_contract_tests.cpp`
- `tests/github_contract/github_paged_read_tests.cpp`
- `tests/github_contract/github_provider_tests.cpp`
- `tests/github_contract/github_read_contract_tests.cpp`
- `tests/network_isolation/designated_live_mutation_runner.cpp`

Refresh this inventory when selected:
`rg -n 'QJson(Document|Object|Array|Value|ParseError)' src tests`.
Inspect other Qt JSON entry points/includes as well; this starting inventory is
not permission to leave a newly discovered caller behind.

## Acceptance and migration constraints

- All repository-owned Qt JSON parser/serializer usage is replaced by Glaze;
  remaining textual references are intentional documentation or boundary guards.
- Prefer typed wire records and exact integer types. Do not carry Qt or Glaze
  provider types into the functional core or public application ports.
- Preserve required/nullable/unknown field semantics, full-input consumption,
  duplicate-key policy, UTF-8 and surrogate handling, byte/depth/record bounds,
  pagination completion, error categories and credential/log privacy.
- Preserve canonical outbound request bytes where required. Audit any changed
  serialization against stored authority fingerprints, recovery records and
  existing exact-body fixtures; do not silently invalidate durable state.
- Prioritize the old mutation parser's known double-normalization behavior:
  raw `user.id: 22.0000000000000001` can normalize to integer `22`. BPR-0610
  fixes this in its new typed review parser; its raw fractional, large-integer,
  overflow, malformed-Unicode and control-character fixtures are patterns for
  the remaining contracts. Extend them to the selected production path.
- Verify each bounded migration through its provider/host/public composition
  tests, including anonymous reads, capability discovery, device sign-in,
  pagination and mutation/recovery as applicable. Finish with a fresh inventory.
- Retain the pinned Glaze provider and license provenance. The owner-supplied
  EntoleGlazeProvider.cmake was consulted as a reference; do not depend on its
  absolute local path in this project's build.

## Status

The full migration is complete through BPR-0636–BPR-0640. Repository-owned C++
production code, composed fixtures and test/diagnostic helpers use Glaze. The
final inventory, focused integration and refreshed parser-sensitive measurements
are recorded below and on BPR-0640. This completes the owner's requested
prerequisite for further RFC-BPR-0013 work; it does not reopen historical RFC
acceptance or close unrelated attended/platform gates.

## Selected migration plan

Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`; reservation commit `4e6a6013`.
Refreshed inventory agrees with the eight production/composed-fixture and nine
C++ test/helper files above. The architecture Python test contains an intentional
Qt boundary guard, not a parser implementation.

- BPR-0636: legacy mutation contracts and raw identity regression coverage.
- BPR-0637: device authorization and capability discovery.
- BPR-0638: read contracts, normalizer and pagination.
- BPR-0639: application composition and stored public fixtures.
- BPR-0640: remaining test helpers, final inventory and integration.

This is the migration closeout anchor; canonical RFC closure order is unchanged.
Further performance work waits for completion of these slices.

## Migration progress

BPR-0636 completes the mutation contract in `25b72eaafbf6132937da00be67d59475e18a9c8d`: typed target/viewer/comment
records, one-pass recovery parsing, exact uint64 identities and byte-compatible
line-comment serialization. Contract, synthetic provider, restart recovery and
public application round-trip evidence are recorded on that card, including a
corrected stale composition-test link inventory. This completed the first migration slice; its acceptance remains on BPR-0636.

BPR-0637 completes device authorization in `1211a9f0` and capability discovery
in `5a7494ab0ef6ef5c67096f8c1a6226eb29368c4c`. Exact duration/string decoding, nine
canonical query bodies, schema facts, bounded sanitized errors, synthetic
provider/sign-in/renewal and native loopback evidence are recorded on that card.
Five production/composed-fixture files still use Qt JSON, with test/helper
migration also required. Next: BPR-0638 read contracts, normalizer and pagination.
The remembered production session is preserved. Performance work remains waiting
for the full migration.

BPR-0638 request boundary is implemented in `1e109cb2863100ef09b9253dfead04606a1e398e`:
typed GraphQL serialization and outbound JSON admission preserve canonical bytes
and pass read/pager/provider/public-composition checks. Its response validators,
normalizers and pagination JSON conversion remain In Progress; the full source
inventory still has five files using Qt JSON. Performance remains deferred until
all migration slices and final integration are complete.

BPR-0638 recorded-response normalization is implemented in `3f25f90d90fa6b55fd5ed89366e2582583ae53c8`:
all eleven operation projections use private typed Glaze records, with raw
numeric refusal and direct nine-REST-response grouping/nullness evidence. Four
production/composed-fixture files still use Qt JSON. Strict fixture validation
and the production pager remain in BPR-0638; their existing double-based numeric
ceilings still prevent claiming full-width read identity migration. Next work
stays on those two paths before application fixtures and final helper cleanup.

BPR-0638 strict response validation is implemented in `43050ce5d77ca334faec1157ba70e61c9e9a1656`.
Recorded reads now retain full-width uint64 identities through validation and
normalization, including UINT64_MAX and adjacent-identity refusal. Three source
files still use Qt JSON: production pager and the two composition/fixture files.
The pager is the remaining BPR-0638 surface and still needs its own full-width,
ownership, rate and pagination evidence before the card can close.

BPR-0638 production paging is implemented in `f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1`,
completing read-contract, normalization and pagination migration. Owned immutable
page buffers keep indexed Glaze values valid across transport completion and
connection copies. Direct full-width identity, anonymous linkage, fractional
refusal and malformed-continuation cases pass alongside the existing exhaustion,
authority and rate tests. Final local read/pager/provider/composed-public-path
integration passes 4/4 in 24.71s. A bounded independent source review found no
regressions. Detailed evidence is retained on BPR-0638.

Two source files still use Qt JSON: application composition and public fixtures
in BPR-0639. Nine C++ test/helper files remain for BPR-0640; its final inventory
and migration integration are still required. Further performance work remains
deferred until BPR-0612 is complete. The production remembered session remains
untouched; all migration verification is local and synthetic.

BPR-0639 completes the two application fixture surfaces in
`e05d00553d72731880df55072a434a9e526c6415`. Typed replay/device projections and
owned capture objects replace their Qt JSON usage while preserving recorded
request bytes, synthetic rotation material, accepted source descriptors and v4
capture fields. Initial local composed GitHub/GHES, registered device, renewal
and four-profile capture integration passes 5/5; after a final capture hex-buffer
lifetime fix, the affected capture contract passes again. Details and unchanged
evidence reuse are recorded on BPR-0639. No production session or live provider
operation ran.

The source tree now has no Qt JSON entry points. BPR-0640 still owns nine C++
test/helper files: thread-map, anonymous-read, capability, device, mutation,
paged-read, provider, read-contract and designated-live-runner fixtures. Replace
the temporary Qt writer oracles with independent literal expectations, complete
the final inventory and focused migration integration, then close BPR-0612.
Performance work remains deferred until that acceptance is established.

BPR-0640 is In Progress. Source `7de8c974f109754889976035c05a241da61f2feb`
freezes the independent Qt writer oracles into fifteen literal canonical bodies
and migrates read-contract, anonymous provenance, provider and thread-map test
readers to Glaze. The affected six-test local selection passes; the final
read-contract helper change also passes its rebuilt focused check. Detailed
revision/evidence reuse and cleanup are recorded on BPR-0640.

Five C++ helper files still use Qt JSON: mutation, capability, device and pager
contract tests, and the designated-live mutation runner. Complete them and the
final migration integration before closing this anchor or resuming performance
work. Production source remains free of Qt JSON, and production sign-in remains
untouched.

BPR-0640 final source `e80f7a15c1bfcb98c123805d65b44efff5009feb` completes the five
remaining C++ helpers and designated-runner parser. Fresh src/tests inventory
finds only the intentional Python Qt boundary guard. The five focused helper
checks pass; final auth/read/provider/recovery/GitHub/GHES and large-review
integration passes using the original twelve passing results plus the corrected
round-trip source-binding rerun. The independent review's duplicate-member
counterexample is fixed and covered by the local parser fixture. Full evidence,
platform context, source-binding audit and cleanup are recorded on BPR-0640.

Parser-sensitive timing and memory observations have been refreshed on the
unchanged 515-file/1,000-thread large-review fixture after the migration. The
loaded-close course retains the existing 160/96 MiB growth/cleanup budgets and
confirms object/cache/job release. All five slices are Done. This closeout does not change historical RFC closure
or outstanding attended/platform gates.

## Migration closeout

Selection `566cf5c3`; claim `d640a01d606570be6b83fb1ace3ec911855764e2`.
Final source `e80f7a15c1bfcb98c123805d65b44efff5009feb`; BPR-0640 finish
`879190668c6c7520e7cca13308d57c808241c01a`. The scoped source diff against
`4e6a6013f3589711f614fba7b9c46b8967313796` contains the planned eight production
and nine helper migrations, raw regression coverage and associated fixture/link
checks. No functional core or public application port gained a provider type.
The pinned Glaze 8.3.0 provider and MIT license hashes are unchanged.

BPR-0636–BPR-0638 retain the detailed exact-integer, nullable/required/unknown,
duplicate, Unicode, full-input, authority, bound and atomic-pagination evidence.
BPR-0639 retains capture-field/identity compatibility. BPR-0640 retains the
independent canonical request literals, final helper fidelity checks and actual
public-path/recovery integration. The existing twelve passing integration lanes
plus the corrected round-trip rerun establish all selected migration lanes;
there is no claim that the initial aggregate run was all green.

The final source inventory and documentation gate pass. All evidence is local
to this Mac using synthetic fixtures, with the remembered production session
preserved. No Actions, push or paid CI ran. Windows/Linux and unresolved human
RFC acceptance remain outside this migration closeout. BPR-0635 may now proceed
against Glaze using the refreshed timing/memory observations on BPR-0640.
