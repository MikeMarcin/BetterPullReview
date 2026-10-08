---
schema: "implement-rfc.card.v1"
id: "BPR-1310"
title: "Verify rendered repository onboarding and prepare owner review"
status: "Parked"
rfcs: ["RFC-BPR-0062"]
depends_on: ["BPR-1309"]
priority: "P1"
metadata: {"milestone_plan": {"id": "bpr-four-drafts-v1", "path": "docs/rfcs/plans/bpr-four-drafts-v1/milestone-plan.json"}, "mandate_revision": "four-drafts-r1"}
parked_on: "human-review"
acceptance: [{"criterion":"C1","outcome":"pass","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01","evidence":{"kind":"artifact","detail":"72 actual-composition offscreen Home/dialog renders across six supported appearances and 100/125/200%; native palette inheritance asserted; long exact wrapped account/host, narrow geometry, accessible editable input, Return Add and Escape/discard/invoking focus pass Qt courses at three scales. Actual Home floor is 900 logical px, dialogs 360/480."}},{"criterion":"C2","outcome":"pending","revision":"8e2f855e9061b4ce9cc150c1668356c8301b9b01:9179cb17b777e2c38b00bad7eaed844067662f30af25780be5bbaa893119c194","evidence":{"kind":"artifact","detail":"Fresh ready/protected session 5317162ad2f023440b559ad4145ed579 sequence 6; BPR-1310-home-v001 and BPR-1310-addition-v001 each ask one Accept/Reject judgment. Both owner verdicts pending. Coordinator starts loopback server after cancelled child approval; owns intake."}}]
---

# Verify rendered repository onboarding and prepare owner review

## Scope

Four-draft mandate r1, registered at `f3e866e9b90505d4604c55b6ad7e1ef598e5bbc2`. Preserve the owning RFC's full boundaries and pending human/native evidence; completion of this slice does not close the RFC.

## Acceptance

- C1: Rendered keyboard/accessibility courses cover long identities, narrow widths and 100–200% scale in supported appearances.
- C2: Owner judges Home and the addition dialog as separate Accept/Reject criteria in a fresh short attended session with an exact self-contained frozen build.

## Prepared review handoff

Implementation source: `8e2f855e9061b4ce9cc150c1668356c8301b9b01`.
Frozen binary SHA-256:
`9179cb17b777e2c38b00bad7eaed844067662f30af25780be5bbaa893119c194`.
The source, build, dependency cache and fixtures are inside
`artifacts/four-drafts-r1/home-frozen-v001/`; the frozen public course passes
10.71s. Launcher checks every path absolutely and reports early startup failure.
No owner desktop was captured or driven.

Fresh attended session `5317162ad2f023440b559ad4145ed579`, prepared sequence 6:
Home (`BPR-1310-home-v001`) and dialog (`BPR-1310-addition-v001`) are separate
one-criterion Accept/Reject candidates, both ready/protected, no verdicts.
State: `/private/tmp/bpr-repository-home-biqu9sse/state`.
Registry: `/private/tmp/bpr-repository-home-biqu9sse/registry`.
Manifest: `artifacts/four-drafts-r1/home-review-manifest.json`.
Only the frozen root/installation is protected; independent main source work
can proceed after writer release. The coordinator owns decision intake.

The child socket bind was refused by the execution sandbox. Its escalation
request was cancelled by coordinator interruption before any server started;
the coordinator owns starting the exact same session's loopback server.
Automatic wait/wake has not been qualified on this runtime. Decisions are
saved independently; if no qualified coordinator wait is sustained, hand back
and send “Resume the attended review” in the coordinator chat.

## Owner verdicts on v001 (dispatch r5, 2026-10-08)

Session `5317162ad2f023440b559ad4145ed579` was handed back at cursor 10 with
both candidates rejected; consumed by dispatch `parallel-integrator-r5-001`.

- `BPR-1310-addition-v001` (fingerprint `c27d5c2d…ed5a`), decision
  `c0939eaa-0bf6-4fc8-80e0-748ed6583ba6` (it supersedes an earlier identical
  note without the last sentence, `ca34dca7…`): "This information is still
  way hidden down the bottom of a settings page that is full of mostly
  irrelvant details. We probably need to move this out into a new settings
  page just focused on managing repositories. And with a better UX"
- `BPR-1310-home-v001` (fingerprint `4e1ef338…cd9d`), decision
  `5b1c12d0-70a8-49b4-8999-54b43f52926b`: "I want a clearer call to action for
  first time experience."

These amend RFC0062's "Settings → Reviews & queue remains the management
location" and the "same row with stronger emphasis" first-run wording; the
latest owner direction wins and the original criteria stay. The next round
gives saved repositories their own Settings page with the shared dialog, and
gives signed-in Home a clearer first-time call to action. C2 stays pending on
a new frozen candidate and a fresh session; BPR-1311 keeps its dependency.
