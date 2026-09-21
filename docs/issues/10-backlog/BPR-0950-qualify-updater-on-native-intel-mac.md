---
schema: "implement-rfc.card.v1"
id: "BPR-0950"
title: "Qualify the release updater on native Intel Mac hardware"
status: "Backlog"
rfcs: ["RFC-BPR-0035"]
depends_on: ["BPR-0948"]
platforms: ["mac"]
metadata: {"native_architecture":"x86_64","owner_deferral":{"date":"2026-09-20","mandate_revision":"r3","source":"Explicit owner amendment after the M4 blocker","nonblocking_for":["RFC-BPR-0035","BPR-0949","BPR-0911","BPR-0912"]}}
---

# Qualify the release updater on native Intel Mac hardware

## Scope and owner amendment

On 2026-09-20 the owner explicitly deferred Intel Mac updater validation from
RFC-BPR-0035 milestone 4 and closeout to this separate follow-up. This is an
acceptance amendment, not an observation that Intel execution passed. This card
is outside the adopted `rfc0035-auto-updater` plan and must not gate BPR-0949,
BPR-0911, BPR-0912, M4 or RFC-BPR-0035 closeout. It creates no reverse dependency
from those cards. Live private-release/vault evidence and canonical RFC dependency
rules were not waived.

The current evidence is native Apple Silicon execution of production-signed,
notarized universal packages. Their x86_64 slices and any Rosetta execution do
not establish native Intel behavior. The release operator must supply an Intel
Mac and explicitly scoped disposable installation roots for this later course.

## Acceptance

- AC-01: Bind the actual Intel Mac/OS, committed source, signed/notarized baseline
  and strictly newer universal candidate, archive/executable digests and helper
  identity. Include the current native menu correction, or explicitly account for
  every source difference from the qualified candidate.
- AC-02: Exercise the actual menu/Settings, explicit Download and Install gestures,
  cooperative multi-window/process shutdown, exact healthy-start receipt and
  truthful progress/result. Verify retained window identities, synthetic drafts
  and indeterminate publication records without retrying a remote publication.
- AC-03: Exercise helper timeout without forced termination, missing-receipt
  rollback, interrupted-rename recovery and transaction-owned cleanup on native
  Intel hardware. Record exact commands, outcomes and retained recoverable state.
- AC-04: Update the updater ledger and operator/platform claims with the observed
  Intel evidence and its limits; leave commands quiescent and clean only owned
  disposable resources. Do not infer live private-service/vault coverage from
  fixture authority or hardware coverage from universal headers/Rosetta.

## Verification and execution record

Not run. Allocation and owner deferral occur in run `rfc0035-20260920`, segment
`m4-post-block-intel-deferral`, mandate `r3`. Existing Apple Silicon evidence is
on [[BPR-0948]] and in the [ledger](../../verification/RFC-BPR-0035.md). This card
authorizes no publication, tag, push, live credential/service access or replacement
of the developer's installed application. Later execution needs its own bounded
assignment; no new unattended hardware work or attended session starts here.
