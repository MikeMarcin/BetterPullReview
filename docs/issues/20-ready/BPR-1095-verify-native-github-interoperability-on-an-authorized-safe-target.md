---
schema: "implement-rfc.card.v1"
id: "BPR-1095"
title: "Verify native GitHub interoperability on an authorized safe target"
status: "Ready"
rfcs: ["RFC-BPR-0048", "RFC-BPR-0049", "RFC-BPR-0050"]
depends_on: ["BPR-1084", "BPR-1085", "BPR-1086", "BPR-1089", "BPR-1093"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-21","milestone":"verification","acceptance":["R12","R13","R14","R16","R19","R45"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"},"triage_20261026":{"date":"2026-10-26","disposition":"updated","reason":"The authorized safe target now exists (BPR-Test, owner 2026-10-05) and all card dependencies are Done."}}
---

# BPR-1095: Verify native GitHub interoperability on an authorized safe target

Pack alias: **PACK-CC-21** (live `BPR-1095`, allocated at import).

## Outcome and work

Prepare the exact mixed-client course in verification/PRODUCT-VERIFICATION.md. Execute mutations only when the user has separately named an authorized safe repository/PR and actions. Publish labelled native suggestions and native review decisions, edit/resolve on the website and refresh BPR. An unavailable target parks only this card.

## Acceptance

- A website-only reviewer sees complete understandable labels and genuine native suggestion behavior.
- Native review events and website edits/resolution return to BPR without duplicate counts or automatic private verification.
- Record exact target/build/actions/evidence or the outstanding authorization/service wait; fixtures are not substituted for live proof.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Triage, 2026-10-26

The authorization this card waits for exists. On
2026-10-05 the owner designated `github.com/MikeMarcin/BPR-Test` for "any
testing needs". Live cards may create pull requests, reviews, comments and
threads there, and resolve or close what they made (AGENTS.md). A second
identity, `MikeMarcinTest`, is available per command for the website-side
reviewer. Every card dependency is Done. The card moves to Ready. The course
publishes only on BPR-Test, checks `X-RateLimit-Remaining`, and leaves to
the owner only a judgement that only a person can make.
