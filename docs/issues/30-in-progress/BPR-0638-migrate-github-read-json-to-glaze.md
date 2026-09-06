---
schema: "implement-rfc.card.v1"
id: "BPR-0638"
title: "Migrate GitHub read and pagination JSON to Glaze"
status: "In Progress"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0637"]
---

# Migrate GitHub read and pagination JSON to Glaze

## Bounded acceptance and ownership

Own github_read_contract.cpp, github_read_normalizer.cpp and github_paged_read.cpp
under src/adapters/github/src, plus corresponding contract/provider tests.
Replace all Qt JSON usage with Glaze, favoring typed wire records. Preserve
nullable/unknown semantics, exact integers, bounded full-input validation,
canonical request bytes, authority and pagination exhaustion, atomic failures,
rate resource tuples and sanitized diagnostics. Verify anonymous/authenticated
reads, paging/provider and the public application read path with stored fixtures.

Migration anchor: BPR-0612. Migration base `4e6a6013f3589711f614fba7b9c46b8967313796`.
Preserve core/host boundaries and pinned dependency provenance. All execution
is local with synthetic fixtures; no Actions, push, paid CI or production
account/process operations. Full migration acceptance remains on BPR-0612.

## Request boundary implementation and evidence

Selection `57d87d8d`; claim `01089a449555d69426a48bd770161abd72d2336d`.
Request source `1e109cb2863100ef09b9253dfead04606a1e398e`.

Legacy and paged GraphQL requests now serialize typed Glaze query/variable
records. Explicit member order preserves the prior canonical bytes; optional
cursor/thread fields are omitted for the same shapes. Variables use bounded
uint32 PR numbers. Outbound validation uses Glaze after existing structural
preflight, validates the entire envelope and variable object, applies the shared
operation scanner, then decodes required variable types. Raw variable views
remain inside the synchronous call. Canonical body equality, semantic
revalidation and authority/header/path admission remain required.

Verification on local Mac:

- Development app, read-contract, pager and provider targets build.
- Final registered network-isolated CTest selection: 4/4 pass in 24.81s.
  `github_read_contract` 0.51s, `github_paged_read` 0.67s,
  `github_provider` 10.78s, and composed
  `application_github_public_path_github_com` 12.84s.
- Contract evidence covers paired GitHub/GHES recorded requests/facts,
  every legacy/initial/files/threads/comments request shape, maximum PR number
  and all printable ASCII cursor/thread characters. An independent prior Qt
  writer matches complete Glaze body bytes; BPR-0640 will replace that temporary
  test oracle with literal fixtures. Existing operation-scanner and canonical
  body rejection cases remain; raw fractional/overflow numbers, invalid Unicode,
  missing/null envelope fields, duplicate keys and trailing documents are refused.
- Touched C++ formatted; documentation gate and `git diff --check` pass.

The request-only commit left response validation, normalization and pagination
open. The normalization portion is recorded below; the strict validator and
paged parser still require migration before this card can close.

All execution used local synthetic fixtures. The production remembered session
and credentials were untouched; no GitHub, Actions, push or paid CI ran. The
consumed public-course image was removed; reusable build trees remain.

## Recorded response normalization implementation and evidence

Source `3f25f90d90fa6b55fd5ed89366e2582583ae53c8` removes every Qt JSON and QString
use from `github_read_normalizer.cpp`. Private Glaze wire records now extract
all eleven supported operations into exact integer, string, bool and optional
fields. Existing strict fixture validation still establishes complete field
inventories, nullable relationships, structural limits and page completeness
before extraction. The normalizer validates complete application records and
retains viewer/repository/head correlation, REST reply grouping, duplicate
identity refusal and atomic snapshot failure. Owned subtrees are referenced
during projection rather than repeatedly copied. No JSON record crosses inward.

Raw numeric tests refuse fractional viewer/repository/review/comment identities,
file counts and line numbers that the old double conversion could round to an
integer, plus uint64 overflow. Exact integer exponent spellings retain account
identity up to the current strict fixture ceiling (2^53-1). This is not evidence
for accepting the full uint64 range through the entire read path: the remaining
Qt strict validator and pager still impose the old numeric ceiling and must be
converted with end-to-end identity tests.

The new direct nine-REST-response normalization course uses the supplied viewer,
omits viewer/GraphQL operations, and checks exact root/reply identities, nullable
authors, unavailable GraphQL threads, missing versus null position, explicit-null
coordinates and invalid missing coordinates. Position contributes only nullness;
opaque non-null values keep that existing meaning. Its first run correctly
refused a fixture that omitted the page-completeness marker. Supplying that
required fixture fact fixed the course without weakening admission.

Local Mac evidence:

- Development app, read-contract, anonymous-admission and provider targets build.
- Initial changed-surface integration: 4/4 pass, 23.46s total:
  `github_read_contract` 0.48s; `github_anonymous_read_contract` 0.42s;
  `github_provider` 10.46s; composed
  `application_github_public_path_github_com` 12.09s. All registered network
  isolation wrappers; no live provider access.
- After adding direct anonymous comment normalization and preserving its
  missing/null details, final `github_read_contract` passes in 0.51s, including
  paired GitHub/GHES normalized facts, numeric regressions and all prior contract
  cases. The other integration inputs did not reach the changed REST comment
  details, so their passing evidence remains applicable. Final app/provider
  targets were relinked after the source changes.
- Formatting, documentation gate and `git diff --check` pass. Owning architecture
  documentation describes typed extraction and nullness semantics.
- Source inventory now contains four Qt JSON files: read contract, pager,
  application composition and public fixture composition. Test-helper migration
  remains BPR-0640.

BPR-0638 remains In Progress. Next replace strict fixture-response validation in
`github_read_contract.cpp` and response decoding in `github_paged_read.cpp` with
Glaze. Preserve strict fixture inventories versus production required/unknown
field policy, missing/null semantics, malformed-versus-shape failure categories,
preflight/aggregate limits, rate tuples, exact authority and complete pagination.
Reuse or extend the private typed record approach where it fits; full uint64
identity acceptance needs proof through both validation and normalization.
BPR-0612 and all subsequent migration slices remain open; performance waits.

Temporary migration script and consumed public-course image were removed after
verification. Production sign-in/credentials were untouched. No GitHub, Actions,
push or paid CI ran; reusable build trees remain.
