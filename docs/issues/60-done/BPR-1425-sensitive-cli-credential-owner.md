---
schema: "implement-rfc.card.v1"
id: "BPR-1425"
title: "Build the operation-owned sensitive CLI credential path"
status: "Done"
rfcs: ["RFC-BPR-0078"]
depends_on: []
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"The dedicated sensitive capture/child-environment port keeps the eight-KiB credential out of generic request/output records. Runner clean/failure, exact boundary, full/split echo and overlapping prefix courses pass; generic token-variable conflicts refuse.","path":"/tmp/bpr-strict-cli-run/m1-final-focused.log"}},{"criterion":"AC02","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"Closed named lookup uses a token-free private configuration after five local probes. Empty/oversized/unpinned/missing/mislabelled credentials refuse. Missing acquisition makes only those five probes plus the lookup, zero API/helper launches; tool failure and credential unavailable remain distinct. Host-class API/helper negatives pass.","path":"/tmp/bpr-strict-cli-run/m1-final-credentials.log"}},{"criterion":"AC03","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"Bounded sensitive storage destructor overwrites the whole object; the runner releases its weak credential owner after drain and holds it across watchdog refusal/quarantine until worker terminal. Generic launch records exclude the synthetic canaries and token-variable entries, and private configurations are removed. Qt/OS/child copies remain explicitly outside precise-erasure claims.","path":"/tmp/bpr-strict-cli-run/m1-final-focused.log"}}]
---

# BPR-1425: Build the operation-owned sensitive CLI credential path

Implemented under `strict-cli-publication-r2`; M1 engineering acceptance is recorded below. Full RFC acceptance, stock-gh native qualification and independent implementation review remain separate obligations.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Sensitive capture and execution ownership.
- **AC02:** Closed named-account retrieval and typed refusal.
- **AC03:** Queued/process/descendant cleanup and no retained generic credentials.

Sensitive results and environment inputs never enter generic copyable request/output records. Clear owned buffers after queued work and descendants drain, including timeout/reap failure/shutdown. Stock-gh hostname mapping is exact or refuses.

## M1 implementation

Shared source candidate `29b4002e509991da38522a209d8a7f6e7be7765e` implements the sensitive owner and its public compositions together, so the credential lifetime can be exercised end to end before acceptance. This intentionally coupled source commit also carries BPR-1426 composition work; constituent acceptance is committed separately. The source and tests are frozen for independent critical review. Full repository format/tidy baseline limitations are recorded in the milestone checkpoint, not counted as passing gates.
