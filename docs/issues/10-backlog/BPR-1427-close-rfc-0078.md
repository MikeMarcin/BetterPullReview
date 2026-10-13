---
schema: "implement-rfc.card.v1"
id: "BPR-1427"
title: "Verify and close RFC-BPR-0078"
status: "Backlog"
rfcs: ["RFC-BPR-0078"]
depends_on: ["BPR-1426"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0078"]
---

# BPR-1427: Verify and close RFC-BPR-0078

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC base: `aa3d71fe470627e1e78488eb51a9114a788f33bd`.

Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** All strict AC01–AC05.
- **AC02:** Official-gh macOS negatives and BPR-Test attribution/helper evidence.
- **AC03:** Independent critical review and narrow contract/documentation acceptance.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** With two accounts available, switching the global active account before retrieval, between validation and publication, and during helper use leaves reads and writes attributed to the chosen account. Removing its credential before retrieval causes zero API/network Git launches, despite a usable unnamed credential. A wrongly labelled credential causes zero writes. | Pending: no implementation/native acceptance supplied. |
| **AC02:** The composed review, inline-comment and tour publication paths, paginated reads, exact-blob reads and clone/helper path all use strict acquisition. Unsupported required behavior, missing account choice and credential failures expose typed refusal without another authority. | Pending: no implementation/native acceptance supplied. |
| **AC03:** A synthetic canary is present only in permitted transient credential buffers and credentialed descendant environments. It is absent from arguments, retained outputs/logs, settings, configuration, object stores, recovery state and all BPR-created persistent stores. Failure, cancellation and shutdown observations cover sensitive-buffer and child lifetime handling; third-party memory limits remain explicit. | Pending: no implementation/native acceptance supplied. |
| **AC04:** Direct official-gh evidence on supported macOS confirms named lookup failure, selected-account API use, helper inheritance and unchanged global account state. Use disposable synthetic credentials for local negatives and designated BPR-Test for any live read/publication proof; evidence records outcomes and rate headers without recording tokens. | Pending: no implementation/native acceptance supplied. |
| **AC05:** Existing focused identity, admission, recovery, cancellation and architecture checks pass with narrowly revised credential-boundary rules. Documentation states the transient exception and supported-tool/native evidence accurately. Historical tokenless evidence is preserved with its original scope. | Pending: no implementation/native acceptance supplied. |
