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

## Request boundary implemented; response migration remains

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

BPR-0638 remains In Progress. Response validation in github_read_contract.cpp,
normalization in github_read_normalizer.cpp and the paged response path in
github_paged_read.cpp still use Qt JSON. Convert these together through typed
wire records/private helpers, preserving strict fixture field inventories versus
production required-field/unknown-field policy, malformed-versus-shape failure
categories, and preflight/resource bounds. Numeric identity conversion currently
passes through double in both normalizers; raw fractional/large/overflow cases
must exercise the actual returned identities and authority comparisons. Carry
nullable actor/repository/line fields and page completion/rate/cursor checks
through the selected public read path. Do not call the full card or BPR-0612
complete on request-only evidence.

All execution used local synthetic fixtures. The production remembered session
and credentials were untouched; no GitHub, Actions, push or paid CI ran. The
consumed public-course image was removed; reusable build trees remain.
