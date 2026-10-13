---
schema: "implement-rfc.card.v1"
id: "BPR-1426"
title: "Bind every composed CLI operation to one transient credential"
status: "Done"
rfcs: ["RFC-BPR-0078"]
depends_on: ["BPR-1425"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"Connection/remembered validation, complete paged provider fetch, capability probe and personal discovery begin one operation scope. Failed public Connect and remembered startup drain with no automatic ambient inventory. Stable profile/login/numeric identity and exact read authority are preserved.","path":"/tmp/bpr-strict-cli-run/m1-final-focused.log"}},{"criterion":"AC02","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"Inline, whole review and explicit tour run/resume hold one credential through validation, durable witness, dispatch and GET reconciliation. The named credential is removed and the stop token cancelled after POST; the held owner serves empty-stop reconciliation. Later acquisition refusal launches no API and restarted retries remain GET-only with no second POST. Twelve public publication/tour/restart courses pass.","path":"/tmp/bpr-strict-cli-run/m1-public-tests1.log"}},{"criterion":"AC03","outcome":"pass","revision":"29b4002e509991da38522a209d8a7f6e7be7765e","evidence":{"kind":"artifact","detail":"Exact GraphQL blob/batch reads and REST fallback share one acquired credential; clone/helper acquisitions use it across pre/post identity checks and clones. Local Git remains credential-free. API/clone hostname-class positive and wrong-family negative fixture controls pass, missing lookup makes zero API/helper launches, and launch records/pinned config contain no canary. Stock-gh native helper evidence belongs to M2 and is not claimed here.","path":"/tmp/bpr-strict-cli-run/m1-final-credentials.log"}}]
---

# BPR-1426: Bind every composed CLI operation to one transient credential

Implemented under `strict-cli-publication-r2`; M1 engineering acceptance is recorded below. Stock-gh native qualification and required independent implementation review remain separate RFC obligations.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Connection/remembered validation, pager and discovery operations.
- **AC02:** Review/inline/tour publication scopes and recovery.
- **AC03:** Exact blob batch/fallback and clone/helper scopes, no local Git token.

Cover token-free automatic failed Connect/startup recovery, paginated reads and personal discovery, same-credential post-witness GET reconciliation, exact blobs/batches/fallbacks and actual helper inheritance. Subsequent operations reacquire; session/profile transport handles retain no token.

## M1 implementation

Shared product candidate `29b4002e509991da38522a209d8a7f6e7be7765e` binds complete read/publication/blob/clone compositions to a transient operation. End-to-end integration was developed with BPR-1425 to prove the sensitive owner, before this card became dependency-eligible; this acceptance slice records the complete constituent obligation. No popup removal is included. Native stock-gh helper evidence and first RFC closeout remain BPR-1427/M2.
