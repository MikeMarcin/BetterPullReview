---
schema: "implement-rfc.card.v1"
id: "BPR-0697"
title: "Separate profile authentication from window review selection"
status: "In Progress"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
---

# Separate profile authentication from window review selection

## Scope

Extract a profile-owned authenticated session boundary from the GitHub host,
leaving independent pull-request requests, snapshots, generations and epoch
handoffs in window read sessions. One owner performs device/vault/binding
lifecycle and publishes exact session observations; consumers bind to profile,
server, account generation and window/request identity. Do not duplicate the
combined GitHub host against one private root.

This first same-profile sharing seam does not by itself compose multiple Qt
windows or close RFC0024. Appearance settings/classification, queue projections,
window layout persistence and per-selection ledger sharing are later bounded
work identified in the authority inventory.

Closeout: [[BPR-0595]]. RFC_BASE_SHA: `e851eb8a851ab605cf5a85260a1317ecdf66773b`.
Routing: standard | Reason: default implementation.
Primary owns architecture, CMake, shared contracts, bookkeeping and acceptance.
Workers receive exclusive source/test ownership and preserve others.

## Acceptance

Authenticate once, attach two read sessions, open different PRs and interleave
completions without moving the sibling. Shared sign-out invalidates both and
rejects their late reads. Existing exact-account/vault/refusal paths remain
intact. Focused core/host tests and one risk-focused independent review.

## Evidence

Read-only inventory informed this slice; no implementation or new check yet.
