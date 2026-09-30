---
schema: "implement-rfc.card.v1"
id: "BPR-1248"
title: "The RFC-0016 device capture contract's read-only route inventory and GraphQL digest drifted"
status: "Done"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json"}
acceptance: [{"criterion":"rfc0016_device_capture_security passes on macOS","outcome":"pass","revision":"b6208f07f","evidence":{"kind":"command","detail":"ctest --test-dir build/dev -R '^rfc0016_' : 7/7 pass on macOS arm64 dev preset, rfc0016_device_capture_security 39 cases OK; verify_rfc0016_production_identity_tests 122 OK"}},{"criterion":"The card names the reviewed commits","outcome":"pass","revision":"b6208f07f","evidence":{"kind":"observation","detail":"f44c95dd7 is the only commit to touch the capture tool or fixture since 46e2f3c2a; the card reviews its three query copies and explains why the recorded bytes were restored rather than resealed (attended review binds route-probes.json by digest bfbd901e)."}}]
---

# BPR-1248: The RFC-0016 device capture contract's read-only route inventory and GraphQL digest drifted

## Observation

At `main` 83edd361b on macOS arm64, `rfc0016_device_capture_security` fails
four cases: two with `rfc0016_capture.ContractError: read-only route
inventory mismatch` (`test_cli_has_no_network_or_write_primitive`,
`test_tracked_fixture_is_exact_recorded_attended_capture`), one exit-code
assertion, and `test_exact_graphql_document_and_read_only_inventory_are_pinned`,
which compares `sha256:bbcf3ccd…dea74aa` with `sha256:e4991e65…255793e` and
finds them different.

The pin is a review tripwire over the provider's read-only routes and exact
GraphQL document. A change to those routes since the pin was taken has not
been reviewed against the capture contract.

## Outcome

The commits that changed the read-only inventory or the document are named and
reviewed against RFC-BPR-0016's capture contract, and the pin is resealed in
the commit that records that review, or the unreviewed route is removed.

## Direct acceptance

- `rfc0016_device_capture_security` passes on macOS.
- The card names the reviewed commits.

## Resolution (red-test backlog milestone, macOS arm64, 2026-09-30)

### Review of the pinned scope

One commit touched the capture tool or its fixture after BPR-0276 closed at
46e2f3c2a: f44c95dd7 ("Reconstruct review tours from paged raw GitHub
objects", 2026-09-26). It extended the provider's review-thread query in
`github_read_contract.cpp` with read-only comment fields (`body url updatedAt
fullDatabaseId commit{oid} originalCommit{oid} startLine originalStartLine
pullRequestReview{id}`). The same edit was applied as text to three copies of
the query:

| Copy | Owner | Review |
|---|---|---|
| `scripts/build_recorded_pull_request_contract.py`, contract manifests | Recorded pull request contracts, which must match the provider's query | Correct to follow the provider. Still a `query`, same endpoint and variables, no mutation. Unchanged here |
| `scripts/capture_rfc0016_device_contracts.py` `GRAPHQL_DOCUMENT` | RFC-BPR-0016's designated read-only probe, which BPR-0276's owner-attended capture sent on 2026-08-24 | Not the provider's query. Each probe's `request_contract_sha256` binds this exact text |
| `tests/fixtures/github/device-authorization/v1/route-probes.json` `graphql_document` | The recorded attended capture | Edited recorded evidence: the text changed, but its `graphql_document_sha256` still named the document actually sent |

### Why no reseal

A reseal is not possible without a new attended capture. The attended review
`rfc0016-attended-live-capture-v001` binds `route-probes.json` by digest
(`sha256:bfbd901e…52a2`, `attended-review-manifest.json`), and
`verify_rfc0016_production_identity.py` requires the recorded production
course's document to equal `GRAPHQL_DOCUMENT`. The capture's probe does not
need to follow the provider's query: it proves a device token reads the
designated repository, and it is never used to read review data.

So the capture tool's document and the fixture return to their recorded bytes
(`e4991e65…793e`; the fixture's digest is `bfbd901e…` again). A comment on
`GRAPHQL_DOCUMENT` says it is recorded evidence and that a new probe needs a
new attended capture. The provider's extended query is unchanged.

### Checks

| Check | Result |
|---|---|
| `ctest --test-dir build/dev -R '^rfc0016_'` | 7 of 7 pass, including `rfc0016_device_capture_security` (39 cases) |
| `python3 tests/scripts/verify_rfc0016_production_identity_tests.py` | 122 pass |
