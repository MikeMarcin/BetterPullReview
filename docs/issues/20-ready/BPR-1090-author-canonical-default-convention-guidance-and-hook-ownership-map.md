---
schema: "implement-rfc.card.v1"
id: "BPR-1090"
title: "Author canonical default convention guidance and hook ownership map"
status: "Ready"
rfcs: ["RFC-BPR-0051"]
depends_on: ["BPR-1076", "BPR-1086"]
metadata: {"pack":"betterpullreview-conventional-comments-rfc-pack-v1","pack_alias":"PACK-CC-16","milestone":"hooks-cli","acceptance":["R34","R35","R36","R37"],"milestone_plan":{"id":"betterpullreview-conventional-comments-rfc-pack-v1","path":"docs/rfcs/plans/betterpullreview-conventional-comments-rfc-pack-v1/milestone-plan.json"}}
---

# BPR-1090: Author canonical default convention guidance and hook ownership map

Pack alias: **PACK-CC-16** (live `BPR-1090`, allocated at import).

## Outcome and work

Refine the supplied fragment against the actual canonical shared skills and effective imported hook configuration. Identify relevant feedback/workshop tasks and excluded tour problem/solution/outline tasks. Record version and override behavior without overwriting consumer copies.

## Acceptance

- All twelve labels and native review/suggestion distinctions are taught without a model requirement.
- The hook ownership map uses real installed task names and canonical sources.
- User/project overrides and disabled modes have explicit non-destructive upgrade behavior.

## Evidence and boundaries

Record the direct test/gesture, actual result, producing revision and limitations in the live card. Product evidence is **not run** at pack authoring. Follow [`verification/PRODUCT-VERIFICATION.md`](../../rfcs/assets/betterpullreview-conventional-comments-rfc-pack-v1/verification/PRODUCT-VERIFICATION.md) and current project isolation/authority rules. Focus checks on this slice; reuse unchanged evidence. Import and local fixtures grant no live GitHub, credential, model or unrelated source-change authority.

## Owner decision, 2026-10-08 (chat): a fifth profile hook, "Review feedback"

Question, as the coordinator relayed it: "Add a fifth profile writing hook,
'Review feedback', holding the default conventions guidance (editable in Agents
& context and via `bpr tour hooks`, off when the label helper is off) — instead
of putting it in the shared AgentSkills repo …?"

Owner's answer, verbatim: "Accept (Recommended)"

This settles the canonical home the run ledger left open (Hook and skill
owners, R35): the guidance is a BPR-owned profile hook under the existing
writing-hook settings authority, not a shared AgentSkills edit. Tour hooks keep
their purposes. Sending the guidance with workshop requests is BPR-1091's.
