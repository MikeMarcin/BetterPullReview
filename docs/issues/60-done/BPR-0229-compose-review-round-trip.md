---
id: BPR-0229
title: Compose review synchronization and the mutation coordinator
status: Done
rfc: RFC-BPR-0008
depends_on: [BPR-0228]
---

# Compose review synchronization and the mutation coordinator

The core, coordinator, synchronization, and Qt editor components exist, but
the shipped application constructs none of them.

## Required final state

- The shipped editor dispatches semantic draft and publish intents and holds no
  provider.
- The composition constructs the coordinator with an explicit designated
  fixture target for automated evidence; no test gains a live target.
- Publish, retry, stale-head refusal, capability fallback, and refresh return
  through typed completions and GitHub remains the shared authority.
- A public fixture trace proves QAction/gesture to proposal, coordinator,
  provider fixture, result event, refresh, and Qt projection.
- The standing live-mutation-isolation gate remains green and non-vacuous.

Live web-UI, mixed-client, and acting-identity acceptance remain in BPR-0142,
BPR-0143, and BPR-0144.

## Implemented state

- The shipped `workbench_window` contains the Qt draft editor. Text edits and
  Publish actions emit semantic draft commands; the widget has no provider or
  coordinator authority.
- The composition root creates a mutation coordinator only for the
  test-build fixture path and fixes it to the exact accepted
  host/repository/pull-request/head and acting-account identity. Production has
  no live mutation provider.
- The fixture provider models server authority, capability absence, stale-head
  refusal, one lost result, and idempotent retry. After each attempt the
  application re-reads fixture server comments before projecting the typed
  result to Qt.
- The shipped-executable contract drives the real editor and Publish action,
  proves stale-head refusal with zero submissions, then proves a lost-result
  retry returns the same comment and leaves exactly one server comment. Its
  source guard rejects direct coordinator/provider calls from the probe.
- The standing mutation-isolation verifier remains part of the registered
  executable contract and inventories the test-only provider without
  weakening the forbidden live-provider symbol and construction checks.

## Verification

- `core_review_round_trip`
- `application_review_mutation`
- `application_review_synchronization`
- `qt_review_draft_editor`
- `qt_workbench_window_scale_100`
- `application_review_round_trip_composition_contract`
- `live_mutation_isolation`
- `live_mutation_isolation_contract`

The public contract requires `REVIEW_ROUND_TRIP_COMPOSITION_EVIDENCE:111111111`
and `REVIEW_ROUND_TRIP_COMPOSITION_CONTRACT_OK`. No live mutation is performed.
