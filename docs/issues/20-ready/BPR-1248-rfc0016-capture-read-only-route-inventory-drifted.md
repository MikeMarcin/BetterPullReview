---
schema: "implement-rfc.card.v1"
id: "BPR-1248"
title: "The RFC-0016 device capture contract's read-only route inventory and GraphQL digest drifted"
status: "Ready"
rfcs: []
depends_on: []
metadata: {"origin":"triaged by the main stabilization milestone, macOS arm64, 2026-09-30","reservation":"docs/issues/_reservations/BPR-1245--BPR-1249--6920ac689bb3.json"}
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
