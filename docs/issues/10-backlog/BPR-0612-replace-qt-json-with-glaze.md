---
schema: "implement-rfc.card.v1"
id: "BPR-0612"
title: "Replace all remaining Qt JSON parsing and serialization with Glaze"
status: "Backlog"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0610","BPR-0636","BPR-0637","BPR-0638","BPR-0639","BPR-0640"]
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

Implementation is underway through the bounded cards below. On 2026-09-05 the
owner questioned doing performance work before
finishing this migration. This card is now selected for bounded planning and
implementation ahead of further RFC-BPR-0013 performance work. Split the
contracts and helpers into dependency-ordered slices before claiming source
ownership. BPR-0610 is complete; preserve its typed whole-review contract.
Existing cache-eviction and lifetime tests remain regression coverage. Refresh
parser-sensitive timing and memory evidence after the migration rather than
tuning the outgoing Qt JSON implementation.

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
