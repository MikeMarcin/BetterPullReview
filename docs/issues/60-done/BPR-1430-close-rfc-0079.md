---
schema: "implement-rfc.card.v1"
id: "BPR-1430"
title: "Verify and close RFC-BPR-0079"
status: "Done"
rfcs: ["RFC-BPR-0079"]
depends_on: ["BPR-1429"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
closeout_for: ["RFC-BPR-0079"]
acceptance: [{"criterion":"AC01","outcome":"pass","revision":"ff1143b5b3dd8d312e247eb37cc23237a8539b54","evidence":{"kind":"observation","detail":"All RFC0079 AC01-AC05: criterion map in durable M4 evidence; focused current/reused public-path evidence and final docs gate."}},{"criterion":"AC02","outcome":"pass","revision":"ff1143b5b3dd8d312e247eb37cc23237a8539b54","evidence":{"kind":"artifact","detail":"Independent critical tests-only and successor receipts; accepted unchanged M3 implementation P1/P2 disposition."}},{"criterion":"AC03","outcome":"pass","revision":"ff1143b5b3dd8d312e247eb37cc23237a8539b54","evidence":{"kind":"observation","detail":"Offscreen/native focused evidence reused with original limits. No new required owner look. Main-only terminal metadata deferred honestly to actual monitor landing."}}]
---

# BPR-1430: Verify and close RFC-BPR-0079

Qualified under `strict-cli-publication-r2` on the exact lane candidate
`ff1143b5b3dd8d312e247eb37cc23237a8539b54`, whose production bytes are unchanged
from accepted M3 `34b6a43458659747648e4039a99ca52407d14624`. Final acceptance
records are in [the M4 evidence](../_checkpoints/strict-cli-account-publication-v1/m4-evidence.json)
and [the companion ledger](../../verification/RFC-BPR-0079.md). Main integration
and the synchronized RFC/index terminal metadata belong to the coordinator and
main monitor; this lane does not assert that landing has occurred.

RFC base: `aa3d71fe470627e1e78488eb51a9114a788f33bd`.

Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** All companion AC01–AC05.
- **AC02:** Independent critical implementation review.
- **AC03:** Offscreen/native focused evidence; attended owner look if required; truthful pack closeout.

## Closeout checklist

| Criterion | Status and evidence |
|---|---|
| **AC01:** Composed CLI whole-review Comment, Approve and Request changes, inline Publish and explicit review-tour publication complete from their ordinary gestures without a CLI-specific modal. Their visible account, target, content and operation remain accurate; keyboard focus and status announcements follow the existing surface. | Pass: ordinary composed CLI review/inline/tour gestures, focus and status checks at `ff1143b5b`; accepted M3 full composition and final independent recheck at `34b6a4345` are unchanged. |
| **AC02:** Connection/read-trust acceptance, startup reuse and account discovery make zero writes. A changed account, head, target or frozen content refuses the original submission; a valid gesture still requires its one-use exact-operation grant. | Pass: retained zero-write trust/discovery/preview guards, exact-operation grants and account/head/target/content negatives. Repaired remembered public path and every fresh-process phase pass at `ff1143b5b`; login/numeric mismatch, no native/device/preview fallback and prompt counts remain. |
| **AC03:** Missing or inaccessible chosen credentials and unavailable strict capability refuse before a write, keep drafts and show the defined actionable status without another-account fallback or a warning bypass. An ambiguous attempted publication still reconciles without a second POST. | Pass: accepted M3 typed fresh failure/draft retention and unknown-result GET-only recovery. Independent P1/P2 recheck at `34b6a4345` clears both findings; the two M4 test commits change no product path. |
| **AC04:** Connection and Accounts & hosts text reflects strict selection, transient credentials and remembered reuse without stale tokenless or ambient-account claims. Residual trust remains accessible in the existing disclosure and outside the publication flow. | Pass: neutral chosen login/server, existing gh sign-in, no BPR token storage, current revalidation and reuse until sign out/replacement are asserted by real composed connection tests. Existing residual-trust disclosure is unchanged. |
| **AC05:** Focused composed publication and Qt accessibility/focus checks, offscreen presentation evidence and the documentation gate pass. If an owner visual judgement remains necessary, prepare its own short attended session with a checked frozen build; this draft supplies no such verdict. | Pass: focused final composition, Qt focus/accessibility, accepted M3 full offscreen geometry at 1/1.25/2 and documentation gate. Offscreen is not Cocoa appearance or VoiceOver; no new required owner look is introduced. Historical BPR-1373 remains separate. |


## Qualification and critical dispositions

The combined-main monitor found two obsolete remembered-consent string checks.
An exact rebuilt lane baseline reproduced both failures. Tests now bind the
actual chosen login/server and current strict connection/reuse explanation,
without weakening any fresh-process or account-isolation control. A first
qualification run passed nine of ten rows; the new exact-login guard correctly
failed a wrong fixture expectation in `cancel-first`, whose default active
choice is account 78. The successor fixes only that expectation and retains
the Cancel and no-authority checks. All failures remain in the M4 evidence.

Independent review of `86cdbe7cbea19db00ad700f675fee3451ff1fbcb` and bounded
successor recheck of `ff1143b5b3dd8d312e247eb37cc23237a8539b54` report no blocker.
The review explicitly does not claim that these read-connection tests count
publication POSTs; unchanged accepted M3 publication courses supply that evidence.
The monitor's combined-main reseal is separately bounded to a test-only C++
identifier fixture server-selection case and retains all existing hostile
inventory and source-content counterexamples. No lane reseal was needed.

Final acceptance uses the existing one honest credential-unavailable outcome
for absent or inaccessible chosen credentials. Tool failure and an attempted
unknown publication stay distinct. M2 native credential/helper/hostname/lifetime
evidence is reused only with its original platform, instrumentation and memory
limits. No native full-Qt live publication, Cocoa/VoiceOver verdict, Windows/Linux
CLI support, release, remote push or installed-app replacement is claimed.

All task-owned commands are terminal at handoff; no review server or attended
session exists. Source/card ownership releases with terminal bookkeeping.
