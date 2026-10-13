---
schema: "implement-rfc.card.v1"
id: "BPR-1429"
title: "Present strict connection disclosure and typed publication failures"
status: "In Progress"
rfcs: ["RFC-BPR-0079"]
depends_on: ["BPR-1428"]
platforms: ["mac"]
metadata: {"mandate_revision":"strict-cli-publication-r2","milestone_plan":{"id":"strict-cli-account-publication-v1","path":"docs/rfcs/plans/strict-cli-account-publication-v1/milestone-plan.json"}}
---

# BPR-1429: Present strict connection disclosure and typed publication failures

Imported under `strict-cli-publication-r2`; current acceptance remains open.

RFC baseline: `aa3d71fe470627e1e78488eb51a9114a788f33bd`. Reservation: `docs/issues/_reservations/BPR-1425--BPR-1430--c807b6b9a5ec.json`.

## Acceptance

- **AC01:** Neutral existing account/server connection text.
- **AC02:** Missing/inaccessible/tool/unknown outcome distinctions without guesses.
- **AC03:** Draft retention, native focus and accessible status.

Critical plan review finds missing versus denied Keychain access unobservable through stock gh. Mandate `strict-cli-publication-r2` supplies the explicit owner amendment: one honest `credential_unavailable` outcome covers missing and inaccessible credentials; tool failure and attempted unknown publication stay distinct.

## Engineering checkpoint (M3 partial)

Product candidate `611c756bdd62ac2bc9af8ed21ead57c7fc0df1e9` implements the neutral
chosen-account/server question, existing sign-in/transient/no-storage/reuse details
and typed fresh unavailable/tool/cancel text bound to the failed actor/host.
Focused integration14/14, post-style composed5/5, Qt/offscreen courses and exact
format22 pass; [durable handoff](../_checkpoints/strict-cli-account-publication-v1/m3-handoff.md)
and [evidence](../_checkpoints/strict-cli-account-publication-v1/m3-evidence.json)
bind the actual scope and private receipts.

Acceptance remains open. Independent review at frozen611c identifies an inline
unknown-result retry/reconciliation followed by credential refusal that can fail
before loading recovery. Composed reproduction confirms a second POST after
restoring credentials. The initial inline capture was premature; independent
review reports fully readable status after900ms settle, so no layout repair is
required from that image. AC02 needs journal/unknown precedence and no-second-POST proof across that
transition, including the earlier coordinator path. AC03 needs the ambiguous inline draft identity, editability and retry state
preserved through credential refusal, with focus/accessibility/deduplication.
Settled inline captures now show full readable status; the initial image was
premature and no layout defect is claimed. The final critical receipt confirms
one blocking P1, with all reviewer commands terminal. Source and this card
ownership are released for fresh post-review M3 repair. Existing lost-result
success-acquisition and fresh failure courses are not substituted for this seam.
Coordinator requires a fresh post-review M3 repair owner; no terminal acceptance
or popup main integration is asserted.

## Post-critical-review repair ownership — 2026-10-13

Fresh sole M3 repair owner `inline_recovery_publication_repair` adopts the released
In Progress card at `33a77bcc117ea6ff36459b5b16445351e2b7e3e8`, under mandate
`strict-cli-publication-r2`. Existing acceptance/history is preserved. The exact
composed probe independently reproduced posts1/stored1 → acquisition refusal →
editable draft → posts2/stored2. The repair must preserve the original ambiguous
publication and durable draft identity through early admission and coordinator
refusals; recovery after credentials return must be GET-only. Required independent
recheck remains pending until supplied. M4/BPR-1430 is outside this assignment.
