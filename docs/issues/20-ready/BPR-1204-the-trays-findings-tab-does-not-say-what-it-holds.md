---
schema: "implement-rfc.card.v1"
id: "BPR-1204"
title: "The tray's Findings tab does not say what it holds"
status: "Ready"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner rejection of attended candidate BPR-1134-tray, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1134-tray", "decision": "6c6d2c52-56a8-41dd-9709-943300785898", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-09"], "related_cards": ["BPR-1134", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1204: The tray's Findings tab does not say what it holds

## Owner decision

In attended session `06ad5734` (packet [owner-review-20260925-fresh-v001](../../verification/reviews/owner-review-20260925-fresh-v001/operator-packet.md)), candidate `BPR-1134-tray` on the
frozen m2 app (`ba9d2723`), asked whether Comments with its count and then
Findings read as the tray header chosen in round v8, the owner answered
(decision `6c6d2c52-56a8-41dd-9709-943300785898`, sequence 52, `actor: human`)
**reject**, with the note, verbatim:

> I have no idea what the hell findings is supposed to be.

The Findings tab holds the AI tools (the agent panel and side chat), the
finding queue and the defect controls ([[BPR-1134]]). The name does not tell
the owner that. In the same session the owner accepted the tray's keyboard and
VoiceOver behaviour (decision `6ee64abe`): "Comments, N threads with new
comments" is announced and Right Arrow reaches the second tab.

## The name waits on a purpose discussion (2026-09-25)

The owner's note asks what the tab is for, not only what it should be called.
The coordinator will discuss the tab's purpose with the owner first. No name
is chosen here, and AC1's alternatives are not prepared, until that
discussion's outcome is recorded on this card as an owner decision.

For the discussion, what the tab gathers today:

- the agent launch panel, side chat and finding queue of
  [RFC-BPR-0012](../../rfcs/RFC-BPR-0012-ai-review-chat-findings-mcp.md)
  (on RFC-BPR-0011's agent boundary), where a "finding" is an agent's
  structured claim about the code;
- the defect classification, navigation and verdict controls of
  [RFC-BPR-0009](../../rfcs/RFC-BPR-0009-defect-fix-verification-workflow.md),
  which act on review threads that the Comments tab lists;
- Dif's decorative AI-tools slot from RFC-BPR-0031.

Before [[BPR-1134]], an action labelled "AI tools and Findings" revealed the
same area; RFC-BPR-0031 calls it the "explicit AI tools area". BPR-1134
(`2770ebb1`) shortened the tab to "Findings", as round v4 of [[BPR-1106]]
drew it ("Tabs in the right tray: Comments, Tour, Findings"). None of the
owner's answers quoted in BPR-1106 uses the word.

## Acceptance

- AC1 Prepare a few named alternatives for the second tab, each rendered in
  the built tray. For each, say what the tab holds, whether it shows when no
  agent is configured, and what its empty state says. Ask the owner which one:
  a single question.
- AC2 Build the chosen header. If the name changes, RFC-BPR-0052 AC-09 and its
  design text are amended to it, and the window, focus-order, accessibility and
  composed courses that name "Findings" follow, with a negative control.
- AC3 The owner accepts the built header on macOS: one question. A renamed
  tab changes the spoken name, so the VoiceOver listen is repeated for the
  new name only.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].
