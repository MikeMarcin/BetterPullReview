---
schema: "implement-rfc.card.v1"
id: "BPR-0697"
title: "Separate profile authentication from window review selection"
status: "Done"
rfcs: ["RFC-BPR-0024"]
depends_on: ["BPR-0696"]
acceptance: [{"criterion":"Shared profile authentication with independent window reads and exact lifecycle","outcome":"pass","revision":"HEAD plus explicit finish paths","evidence":{"kind":"observation","detail":"Four focused tests4/4 1.52s; reentrant captured-fetch repair directregression0.36s; shared signout/reauth, >32 lifetimechurn, delayed primary/detached read drain and authority tests pass. Six-TU tidy andformatpass. Rebuilt appsetup7.04s andanonymouspreview0.75s pass, botharchitecturechecks8.58/81.73s. Independentreview correction narrowly verified. MultipleQtwindowcomposition remains later."}}]
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

Implemented: the existing facade delegates its actual PR lane to
an independent window read owner. Its profile authentication/device/vault owner
remains singular. Attachments carry distinct profile/window/application-session
identity and exact server/account authority; reauthentication may resume only a
matching authority. Window cancellation revokes its own snapshot leases without
revoking the shared account transport.

Required lifecycle checks include sequential window churn beyond the active
capacity, closing the initial read consumer, shared sign-out/reauthentication,
and pending primary/detached reads during profile shutdown. Profile quiescence
must retain read-port lifetime through every delayed cancellation terminal.
Four focused core/GitHub/anonymous/window tests pass (1.52 s), including the
shared-auth two-window fixture, exact authority, churn and delayed-drain cases.
Independent medium review identified an obsolete captured fetch after reentrant
publication/session replacement. Exact pending-request revalidation fixes it;
the targeted reentrant retirement/reauthentication regression passes (0.36 s).
Public completion injection is also refused. Six changed TUs pass canonical
tidy; format and diff checks pass.

The rebuilt application passes exact setup navigation (7.04 s) and anonymous
public preview (0.75 s), including zero vault/header effects. Both architecture
checks pass (8.58 s / 81.73 s). Existing facade APIs remain on the extracted read
lane; actual same-profile Qt windows are not claimed. No live provider/vault
operation was used. Temporary extraction scripts were removed; ordinary build
outputs remain.
