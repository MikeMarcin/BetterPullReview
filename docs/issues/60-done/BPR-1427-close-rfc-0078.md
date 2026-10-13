---
schema: "implement-rfc.card.v1"
id: "BPR-1427"
title: "Verify and close RFC-BPR-0078"
status: "Done"
rfcs: ["RFC-BPR-0078"]
depends_on: ["BPR-1426"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0078"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"2793606c5a7785d0b671b81a31f016b401cab1b5","evidence":{"kind":"artifact","detail":"All strict RFC AC01–AC05 accepted with unchanged M1 execution plus M2 native/lifetime evidence and supplied bounded critical review; ledger preserves scope limits.","path":"docs/verification/RFC-BPR-0078.md"}},{"criterion":"AC02","outcome":"pass","revision":"2793606c5a7785d0b671b81a31f016b401cab1b5","evidence":{"kind":"artifact","detail":"Stock-gh named absence, two selected live /user identities, three host-class gh-owned HTTPS helper positives/auth-dependent negatives, transience, unchanged global state and exact synthetic cleanup. No live publication/full-Qt-native claim.","path":"docs/issues/_checkpoints/strict-cli-account-publication-v1/m2-evidence.json"}},{"criterion":"AC03","outcome":"pass","revision":"2793606c5a7785d0b671b81a31f016b401cab1b5","evidence":{"kind":"artifact","detail":"Final bounded critical review no blocker; independent runner 1/1 and evidence/executable hashes pass. Focused gates and narrow contract/upkeep accepted; historical plans/BPR-1373 preserved.","path":"/tmp/bpr-strict-cli-run/critical-m2-review.md"}}]
---

# BPR-1427: Verify and close RFC-BPR-0078

Imported under `strict-cli-publication-r2`; all strict criteria and M2 exits are accepted on frozen candidate `2793606c5a7785d0b671b81a31f016b401cab1b5`.

RFC base: `aa3d71fe470627e1e78488eb51a9114a788f33bd`.

Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** All strict AC01–AC05.
- **AC02:** Official-gh macOS negatives and BPR-Test attribution/helper evidence.
- **AC03:** Independent critical review and narrow contract/documentation acceptance.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** With two accounts available, switching the global active account before retrieval, between validation and publication, and during helper use leaves reads and writes attributed to the chosen account. Removing its credential before retrieval causes zero API/network Git launches, despite a usable unnamed credential. A wrongly labelled credential causes zero writes. | Pass on `2793606c5a7785d0b671b81a31f016b401cab1b5`: scoped M1 execution evidence plus M2 native/lifetime evidence and final independent critical acceptance; see the strict ledger and M2 checkpoint. |
| **AC02:** The composed review, inline-comment and tour publication paths, paginated reads, exact-blob reads and clone/helper path all use strict acquisition. Unsupported required behavior, missing account choice and credential failures expose typed refusal without another authority. | Pass on `2793606c5a7785d0b671b81a31f016b401cab1b5`: scoped M1 execution evidence plus M2 native/lifetime evidence and final independent critical acceptance; see the strict ledger and M2 checkpoint. |
| **AC03:** A synthetic canary is present only in permitted transient credential buffers and credentialed descendant environments. It is absent from arguments, retained outputs/logs, settings, configuration, object stores, recovery state and all BPR-created persistent stores. Failure, cancellation and shutdown observations cover sensitive-buffer and child lifetime handling; third-party memory limits remain explicit. | Pass on `2793606c5a7785d0b671b81a31f016b401cab1b5`: scoped M1 execution evidence plus M2 native/lifetime evidence and final independent critical acceptance; see the strict ledger and M2 checkpoint. |
| **AC04:** Direct official-gh evidence on supported macOS confirms named lookup failure, selected-account API use, helper inheritance and unchanged global account state. Use disposable synthetic credentials for local negatives and designated BPR-Test for any live read/publication proof; evidence records outcomes and rate headers without recording tokens. | Pass on `2793606c5a7785d0b671b81a31f016b401cab1b5`: scoped M1 execution evidence plus M2 native/lifetime evidence and final independent critical acceptance; see the strict ledger and M2 checkpoint. |
| **AC05:** Existing focused identity, admission, recovery, cancellation and architecture checks pass with narrowly revised credential-boundary rules. Documentation states the transient exception and supported-tool/native evidence accurately. Historical tokenless evidence is preserved with its original scope. | Pass on `2793606c5a7785d0b671b81a31f016b401cab1b5`: scoped M1 execution evidence plus M2 native/lifetime evidence and final independent critical acceptance; see the strict ledger and M2 checkpoint. |

## M2 native frontier

The [strict ledger](../../verification/RFC-BPR-0078.md) and
[M2 evidence](../_checkpoints/strict-cli-account-publication-v1/m2-evidence.json)
record actual stock-gh named absence, exact chosen live API identity, gh-owned
HTTPS helper inheritance and an auth-dependent negative, correct hostname-token
classes, no persistent canary, unchanged global state and exact synthetic cleanup.
The actual-child watchdog `reap_failure` course retains sensitive ownership while
a credential-bearing descendant is alive and drains it before release. Final independent critical acceptance is supplied in
`/tmp/bpr-strict-cli-run/critical-m2-review.md`: no blocking findings; independent runner
1/1 and recorded hash checks pass. Main integration owns terminal RFC/index
status confirmation; actual acceptance is retained independently of tracker state. Popup removal remains the next milestone.
