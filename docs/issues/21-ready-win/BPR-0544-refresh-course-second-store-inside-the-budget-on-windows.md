---
id: BPR-0544
title: The composed refresh course lands its second store inside the budget on Windows
status: Ready
rfc: RFC-BPR-0016
depends_on: []
---

# The composed refresh course lands its second store inside the budget on Windows

## Why

While closing [[BPR-0543]] on the JaegAW Windows station (2026-09-03),
`application_device_authorization_refresh_course` failed in the first run of
the card's selector set and was then placed at the pristine merge head
`8f9d5616` with every BPR-0543 file reverted: it passed once immediately
after a relink and then failed eight of eight. Every failure is the single
marker `DEVICE_FLOW_VAULT_STORES:1` against the required `2`, with
`DEVICE_FLOW_STAGE:authorized`, `DEVICE_FLOW_ROTATIONS:1`,
`DEVICE_FLOW_REFRESH_STATE:scheduled`, and `DEVICE_FLOW_REFRESH_BOUND:true`
all as pinned — the rotation's second store does not land inside
`bpr_self_test_budget_ms`. The course runs over the fixture vault and never
constructs a Credential Manager vault; the fixture data root is a fresh
`QTemporaryDir` per process. The station repaired nothing. This row is one
of the 24 selectors every final-head platform replay of RFC-BPR-0016 runs,
so until it is understood the Windows replay's green on this row is a coin
flip and the ledger's refresh-lifecycle claim cannot quote it for Windows.

## Required final state

- The cause is named with evidence from the Windows host: whether the
  second store is issued late (the course-only clock offset, the timer
  lead, or the refresh transport's rotation), lands late (the fixture
  vault's effect lane), or lands after the evidence snapshot is printed;
  and why a run immediately after a relink differs from the next.
- The course pins its markers at a point that is deterministic on every
  platform — the evidence waits on the second store's completion (or a
  typed refusal) rather than on wall-clock luck — without weakening what
  `DEVICE_FLOW_VAULT_STORES:2` proves; if the budget itself is the cause,
  the bound is raised once with the measured reason stated on the row.
- Ten consecutive runs pass on the Windows station at the closing head,
  and the row still passes on macOS and Linux (the integrator reproduces
  on macOS at integration).
- The card records host, toolchain, the exact commands, and results.

## Non-goals

- Any change to the refresh lifecycle's product behaviour or to the
  markers' meaning.
- The orphan course ([[BPR-0540]]) beyond keeping it green if the shared
  course scaffolding moves.

## Lane — 2026-09-03

Carded by the integrator straight into `21-ready-win/` with no open
dependency: the flake reproduces only on the Windows host so far, and its
diagnosis needs that host. Dispatched to `bpr-win` on `windows-lane`. The
edit will touch `src/app/main.cpp`, which [[BPR-0540]]'s open slice also
edits; the integrator resolves that merge.
