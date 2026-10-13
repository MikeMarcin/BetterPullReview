---
schema: "implement-rfc.card.v1"
id: "BPR-1425"
title: "Build the operation-owned sensitive CLI credential path"
status: "Done"
rfcs: ["RFC-BPR-0078"]
depends_on: []
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"},"critical_review_repair":{"source_revision":"cb41d1a1609d69373871bc3dd9321f59dcd60cc0","prior_revision":"29b4002e509991da38522a209d8a7f6e7be7765e","prior_acceptance":"preserved as historical; reproduced critical review findings supersede affected claims","records":[{"criterion":"AC01","outcome":"pass","revision":"cb41d1a1609d69373871bc3dd9321f59dcd60cc0","evidence":{"kind":"artifact","detail":"Dedicated sensitive capture/environment and generic canary confinement unchanged; real runner descendant timeout/cancellation and strict operation canary generic-output/persistence negatives pass.","path":"/tmp/bpr-strict-cli-run/m1-repair-focused-final.log"}},{"criterion":"AC02","outcome":"pass","revision":"cb41d1a1609d69373871bc3dd9321f59dcd60cc0","evidence":{"kind":"artifact","detail":"Only a completed named lookup refusal reports credential unavailable. Timeout, crash, reap, cancelled, startup, invalid/oversized and local-probe errors stay distinct tool/cancel categories. Direct reads/inline and strict SQLite review/tour controls pass with zero downstream API on refusal.","path":"/tmp/bpr-strict-cli-run/m1-repair-strict-review.log"}},{"criterion":"AC03","outcome":"pass","revision":"cb41d1a1609d69373871bc3dd9321f59dcd60cc0","evidence":{"kind":"artifact","detail":"Actual credential-bearing descendants verify inheritance before timeout/cancel, the sensitive owner remains live, and process/worker drain precedes owner release. The existing pre-execution quarantine test proves queued reap-failure ownership only; actual credential-bearing reap_failure is not forced or claimed.","path":"/tmp/bpr-strict-cli-run/m1-repair-focused-final.log"}}],"independent_recheck":{"status":"supplied","revision":"cb41d1a1609d69373871bc3dd9321f59dcd60cc0","report":"/tmp/bpr-strict-cli-run/critical-m1-recheck.md","disposition":"both blockers no longer reproduce; no blocking repair finding"},"evidence":"docs/issues/_checkpoints/strict-cli-account-publication-v1/m1-repair-evidence.json"}}
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

## M1 critical-review correction — 2026-10-13

Historical acceptance above remains bound to its original candidate; Done is terminal. The coordinator authorized this bounded same-milestone correction without a new issue allocation. Current engineering evidence is recorded in metadata and [the repair checkpoint](../_checkpoints/strict-cli-account-publication-v1/m1-repair-evidence.json). Independent critical recheck was supplied for the revised source candidate, with neither prior blocker reproducing. Stock-gh native qualification remains M2; tracker status and fixture success do not supply it.

P2 is reproduced and repaired. A completed named lookup refusal means credential unavailable; probe/nonzero, timeout, crash, reap, cancellation and malformed/oversized process results retain tool or cancellation categories. Actual sensitive descendants now prove timeout/cancel lifetime and drain; the queued quarantine course remains pre-execution evidence, with no claim of a forced actual credential-bearing reap failure.
