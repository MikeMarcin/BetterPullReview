---
schema: "implement-rfc.card.v1"
id: "BPR-0638"
title: "Migrate GitHub read and pagination JSON to Glaze"
status: "Done"
rfcs: ["RFC-BPR-0002"]
depends_on: ["BPR-0637"]
acceptance: [{"criterion":"Replace Qt JSON on all three owned read surfaces","outcome":"pass","revision":"f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1","evidence":{"kind":"observation","detail":"Inventory finds no QJson includes or API usage in github_read_contract.cpp, github_read_normalizer.cpp or github_paged_read.cpp. Private Glaze wire records/indexes stay at the adapter boundary. Prior request and normalization source evidence is retained on the card."}},{"criterion":"Preserve exact identities, nullable and unknown fields, full-input bounds and canonical requests","outcome":"pass","revision":"f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1","evidence":{"kind":"observation","detail":"Final github_read_contract and github_paged_read pass. Raw fractional/overflow IDs and counts are refused; recorded and production viewer/repository/PR/review identities retain values above 2^53 and UINT64_MAX. Anonymous root/reply grouping and adjacent identity refusals pass. Required/unknown fields, missing/null, malformed Unicode, duplicates, EOF and trailing inputs are exercised. Canonical request evidence from 1e109cb2863100ef09b9253dfead04606a1e398e remains unchanged."}},{"criterion":"Preserve owned page lifetime, authority, exhaustion, atomic failure and rate tuples","outcome":"pass","revision":"f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1","evidence":{"kind":"observation","detail":"Final pager course passes full REST and nested GraphQL continuations, cursor/count and aggregate bounds, exact authority, cancellation and rate/error cases. All 101 files and comments are examined after transport destruction; malformed late continuation yields no snapshot. Independent read-only source review found no regressions in these claims."}},{"criterion":"Exercise provider and composed application read path locally","outcome":"pass","revision":"f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1","evidence":{"kind":"observation","detail":"Final registered network-isolated CTest selection passes 4/4 in 24.71s: github_read_contract 0.51s, github_paged_read 1.53s, github_provider 10.52s, application_github_public_path_github_com 12.14s. All four targets build. Documentation, touched-source formatting and diff checks pass. No live provider, production session, credential, Actions, push or paid CI operation ran."}}]
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
identity up to the current strict fixture ceiling (2^53-1). At that source revision, the Qt strict validator and pager still imposed the
old numeric ceiling. The strict-validator migration and full-width recorded-read
evidence are recorded below; the production pager remains open.

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

The normalization revision left strict fixture validation and the production
pager open. Current progress and the next remaining path follow below.

Temporary migration script and consumed public-course image were removed after
verification. Production sign-in/credentials were untouched. No GitHub, Actions,
push or paid CI ran; reusable build trees remain.

## Strict response validation implementation and evidence

Source `43050ce5d77ca334faec1157ba70e61c9e9a1656` removes all Qt JSON from
`github_read_contract.cpp`. Existing field-inventory/nullable/relationship checks
now use Glaze object/array indexes of original JSON values. Views borrow the
response only inside synchronous validation; decoded scalars and complete typed
normalization remain separate. Preflight byte/depth/member/record/duplicate
bounds and whole-document Unicode/trailing-input validation precede field checks.
Root JSON must remain an object or array. Number syntax still refuses non-finite
tokens; only pre-existing incomplete-count classification uses a real-number
comparison, never identity extraction. Identities decode exactly to uint64;
line/PR-number bounds remain narrower.

A contract failure exposed an empty root raw view when a bounded response ended
exactly at `]`. The final path validates the whole document with Glaze, then
explicitly borrows the caller-owned body. New object/array EOF tests verify the
complete normalized facts rather than relying on incidental trailing whitespace.
Decoded string equality preserves escaped comment/thread path equivalence.

New direct tests pass viewer, repository, PR and review IDs above 2^53 and at
UINT64_MAX through strict validation and typed normalization, then reject the
adjacent wrong viewer binding as identity_mismatch. Existing fractional/overflow
refusals remain. Malformed Unicode, non-finite number tokens, scalar roots and
trailing documents preserve the malformed-response category. Existing exact
field inventories, null handling, incomplete-page ordering, paired GitHub/GHES
facts and anonymous REST comment coverage also pass.

Local Mac evidence:

- Read-contract, pager, provider and development app targets build.
- Final registered network-isolated integration: 4/4 pass in 24.14s:
  `github_read_contract` 0.55s, `github_paged_read` 0.68s,
  `github_provider` 10.49s and composed
  `application_github_public_path_github_com` 12.41s.
- Touched C++ formatted; documentation gate and `git diff --check` pass.
- Source inventory now has three Qt JSON files: `github_paged_read.cpp`,
  `github_application.cpp` and `github_public_path_fixture.cpp`.

BPR-0638 remains In Progress for the production pager. Convert its owned parsed
pages and numeric/shape/rate/pagination decoders, preserving required-versus-
unknown fields, exact authority, cursor progress, atomic failure and aggregate
limits. Parsed pages must own their typed records or a stable backing buffer;
never let a view of a temporary transport response survive that response. The
recorded-read uint64 evidence above does not establish full-width identities in
the production paged path. BPR-0612, application fixtures and final test-helper
migration remain open; performance still waits.

All execution was local and synthetic. No production session, credential, live
GitHub, Actions, push or paid CI operation ran. The consumed migration script
and public-course image were removed; reusable build trees remain.

## Production pager completion and evidence

Source `f1d1ed1545697db9bfb759f5f374b6fe7a29dfe1` removes all Qt JSON from
`github_paged_read.cpp`, completing this card's three owned source surfaces.
Glaze validates each bounded response and indexes its original values; every
page and indexed child shares an immutable owned body, so copied connection
nodes cannot borrow a destroyed transport completion. Field inventories,
required/unknown and missing/null behavior, REST authority, GraphQL cursors and
counts, complete resource-scoped rate tuples and atomic snapshot admission
retain their existing checks. Typed uint64 extraction replaces the floating-point
identity ceiling. Finite-number preflight remains syntax validation only.

Direct new pager cases retain viewer, repository, PR, review and anonymous root
IDs above 2^53 and at UINT64_MAX, verify exact anonymous reply grouping, and
refuse adjacent wrong repository/PR/root identities. Raw fractional identities,
reply links, file counts, coordinates and connection totals cannot round into
valid records; uint64 overflow is refused. Viewer tests cover exact integer
exponents, escaped Unicode, null names, unknown malformed values, duplicate
keys, scalar roots and trailing documents. Malformed nested-comment continuation
tails refuse the entire snapshot. All 101 files and 101 comment identities/text
are checked after the fixture transport and its temporary completions are gone.
Existing anonymous, GitHub/GHES, full REST/GraphQL exhaustion, bounds, authority,
cancellation and rate-classification cases remain passing.

Local Mac evidence on the source above:

- Pager, read-contract, provider and development application targets build.
- Final registered network-isolated integration: 4/4 pass in 24.71s:
  `github_read_contract` 0.51s, `github_paged_read` 1.53s,
  `github_provider` 10.52s and composed
  `application_github_public_path_github_com` 12.14s.
- Documentation gate, touched-source format check and `git diff --check` pass.
- One independent read-only reviewer inspected page lifetime, exact scalar
  decoding, missing/null behavior, authority, exhaustion and rate/error flow
  against `7820e45e`; no blocking or non-blocking finding. That review did not
  cover the new tests or duplicate execution; the final test evidence above
  supplies their results.
- Source inventory now contains only two Qt JSON files, both application
  composition/fixtures owned by BPR-0639. Nine C++ test/helper files remain for
  BPR-0640; the Python architecture guard is intentional, not parser usage.

BPR-0638 is complete. BPR-0612 remains open for BPR-0639, BPR-0640 and final
migration integration; performance work remains deferred. Earlier source
revisions and their unchanged request/normalizer evidence remain applicable.
All execution was local and synthetic; production session and credentials were
untouched and no live GitHub, Actions, push or paid CI ran. The consumed migration
script and public-course image were removed; reusable build trees remain.
