---
schema: "implement-rfc.card.v1"
id: "BPR-1204"
title: "The tray's Findings tab does not say what it holds"
status: "Backlog"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1134", "BPR-1208", "BPR-1209", "BPR-1210"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner rejection of attended candidate BPR-1134-tray, 2026-09-25", "session": "06ad5734ed9f7c2fd6d0a62c8328fa0a", "candidate": "BPR-1134-tray", "decision": "6c6d2c52-56a8-41dd-9709-943300785898", "reviewed_revision": "ba9d272339ad235f1d99688c5c1af198b0c9c7ef", "reviewed_executable_sha256": "a2e5128a8990ccf35cda27b5bebfbbc2c76c2096d97c0fd85e3e7857a0fd8a83", "criteria": ["AC-09"], "related_cards": ["BPR-1134", "BPR-1135", "BPR-1208", "BPR-1209", "BPR-1210"], "reservation": "docs/issues/_reservations/BPR-1199--BPR-1207--b05c1525088e.json", "owner_direction": "2026-09-25, in chat, relayed by the coordinator", "decided_name": "Notes", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
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

## The name waited on a purpose discussion (2026-09-25)

The owner's note asks what the tab is for, not only what it should be called.
The coordinator discussed the tab's purpose with the owner first; the outcome
is the owner direction below, which replaces this card's earlier acceptance.

For the discussion, what the tab gathered:

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

## Owner direction on the tab's purpose (2026-09-25)

In chat, relayed by the coordinator, the owner gave this direction, verbatim:

> I don't think thread controls make any sense there, that's a tab for the whole file not thread. Findings is a bad heading for AI -- may as well just call it AI or Agent; if the UX even makes sense. Dif living in a tab that's likely hidden forever makes no sense, definitely doesn't belong there. Off hand my thoughts are move the thread controls to well the thread, right click menu maybe or a button. Move Dif out of the right drawer entirely, he can live in the lower left corner of the status bar by default just like AllTheThings unless he's got a prime spot on a screen like on the first time experience home. The "Findings" tab becomes Local or Notes or Personal. It's the place for your non-public notes and AI conversations/review feedback. You can promote things from there to draft in the Comments / Public tab to publish them.

**Resolved intent.**

- **The tab is repurposed.** It is the reviewer's private, local space for
  their non-public notes and for AI conversations and review feedback. From
  it, items are promoted to drafts in the public Comments tab, and they are
  published from there. Nothing in the tab is published.
- **AI or Agent alone is rejected as the framing.** The owner raised it
  ("may as well just call it AI or Agent; if the UX even makes sense"), then
  named the tab by what it is for: a private space that holds AI material
  alongside the reviewer's own notes.
- **The name is one of Local, Notes or Personal.** The owner delegated the
  choice (below).
- **What leaves the tab.** A thread's controls move onto the thread
  ([[BPR-1208]]), and Dif leaves the right drawer entirely ([[BPR-1209]]).
- **What the tab gains.** Private notes are written there, and notes and AI
  replies are promoted to drafts in Comments ([[BPR-1210]]). Findings already
  promote to drafts through RFC-BPR-0012's queue, and that flow is reused
  unchanged.

## Decisions made under the owner's delegation (2026-09-25)

Relayed by the coordinator, the owner delegated the name ("We can always
change it later if we don't like it") and then every other open choice in this
batch. Each decision below is the implementer's, made under that delegation.
Changing one later takes a new owner decision.

- **The name is Notes.** Rationale: Notes names what the tab holds, as its
  neighbours Comments and Tour do, and it is already the product's word for
  this material ("Add to private notes"). "Local" would equally describe the
  unpublished drafts that live in Comments, and "Personal" names an owner
  rather than a content.
- **Tooltip.** Wording: "Your private notes, AI conversations and review
  feedback. Nothing here is published; promote an item to a draft in Comments
  to publish it." Rationale: the tooltip states the tab's contents and the
  one rule that separates it from Comments.
- **Empty state.** Wording: "Private to you and never published. Write a note
  or ask an agent about this pull request, then promote what you want to say
  to a draft in Comments." Rationale: an empty tab should say what it is for
  and what to do first.
- **With no agent configured, Notes still shows.** The AI tools say that no
  agent is set up, and notes stay usable. Rationale: notes need no agent, so
  hiding the tab would hide them.
- **This card lands last.** It depends on [[BPR-1208]], [[BPR-1209]] and
  [[BPR-1210]], so the owner looks at a tab whose contents match its name. It
  moves from Ready to Backlog until they are Done. Rationale: asking about
  "Notes" while the tab still holds the defect controls and Dif would ask
  about the wrong tab.

## Acceptance

- AC1 The tray's second tab is **Notes**. The tab's label, tooltip and
  accessible name, the tray's accessible description, and the text of the
  action that selects the tab (`showWorkbenchAiToolsAction`, today
  "Findings") all say Notes. The tooltip is the one decided above.
- AC2 Notes holds the reviewer's private space and nothing else: the agent
  launch panel, the side chat, the finding queue and the reviewer's notes
  ([[BPR-1210]]). It holds no thread's controls ([[BPR-1208]]) and no Dif
  ([[BPR-1209]]). It shows whether or not an agent is configured, and its
  empty state is the sentence decided above.
- AC3 RFC-BPR-0052 AC-09 and its design text name Notes; the owner amendment
  of 2026-09-25 recorded that. The workbench specification's tray section and
  layout sketch, and the window, focus-order, accessibility and composed
  courses that name Findings, follow Notes. A negative control shows that a
  course still expecting "Findings" fails.
- AC4 The owner looks at the built Notes tab on macOS, with one question:
  does the tab read as your private space for notes and AI feedback? The
  renamed tab changes the spoken name, so the VoiceOver listen is repeated
  for "Notes" only. The name is not asked again, because it was delegated.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
as the owner amended or decided it on 2026-09-25. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
the originating card and [[BPR-1135]].
