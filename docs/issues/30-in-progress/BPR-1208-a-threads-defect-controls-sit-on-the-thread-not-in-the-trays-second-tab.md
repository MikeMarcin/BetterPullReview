---
schema: "implement-rfc.card.v1"
id: "BPR-1208"
title: "A thread's defect controls sit on the thread, not in the tray's second tab"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1133", "BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner direction on BPR-1204's tab, 2026-09-25, in chat, relayed by the coordinator", "criteria": ["AC-09"], "related_rfcs": ["RFC-BPR-0009"], "related_cards": ["BPR-1204", "BPR-1133", "BPR-1134", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1208--BPR-1210--a47a9558dfa6.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1208: A thread's defect controls sit on the thread, not in the tray's second tab

## Owner direction

From the owner's direction of 2026-09-25 on the tray's second tab, recorded
verbatim on [[BPR-1204]]:

> I don't think thread controls make any sense there, that's a tab for the whole file not thread. [...] Off hand my thoughts are move the thread controls to well the thread, right click menu maybe or a button.

The second tab ([[BPR-1134]], today "Findings", to be Notes by [[BPR-1204]])
holds RFC-BPR-0009's thread controls, which act on the selected conversation:

- "Classify selected conversation as defect";
- the defect position ("Defects N/M") with Previous defect and Next defect;
- the defect card, which shows the state, the local classification with its
  severity and category selectors, and the context availability;
- the verdicts "Apply local classification", "Verify this fix", "Reopen this
  defect", "Withdraw this defect" and "Accept as is".

Today these are reachable only from that tab. No menu or key reaches them.

## Decision made under the owner's delegation (2026-09-25)

The owner's "right click menu maybe or a button" left the form open. The
coordinator then relayed an owner amendment that delegates the choice: a
context menu, a button on the thread card, or both, to be decided by the
implementer and not left open. The implementer decided **both, split by
purpose**:

- **A context menu on every thread card.** It lists the card's commands:
  the card's existing actions, then "Classify as defect…" for an unclassified
  thread, or, for a defect, its verdicts and "Reclassify…".
- **A defect badge button on a defect's own card.** It shows the state and
  classification as text and opens the same defect actions. An unclassified
  thread gets no new button.
- **Defect navigation joins Next new comment.** Previous defect, Next defect
  and the position move beneath the Comments tab's header, beside Next new
  comment.

Rationale: the menu keeps a rarely used command out of every card while
leaving it one right-click or key away. The badge shows the state the
reviewer tracks and puts its verdicts one click away, the way Reply already
sits on the card. Defect navigation moves between the threads that Comments
lists, as Next new comment does.

## Acceptance

- AC1 The tray's second tab holds none of these controls: Classify, the
  defect position, Previous and Next defect, the defect card and its
  verdicts. A negative control that puts them back fails the tab's content
  course.
- AC2 Every thread card has a context menu, wherever the card appears: in the
  Comments tab, in Discuss and beside the code. The menu opens by right-click,
  and by the context-menu key or Shift+F10 on the selected card. It lists the
  card's existing actions (Reply, Mark read, Resolve or Unresolve, Open in
  code). For an unclassified thread it adds "Classify as defect…". For a
  defect it adds "Verify this fix", "Reopen this defect", "Withdraw this
  defect", "Accept as is" and "Reclassify…". An action the core would refuse
  is disabled, and its tooltip says why.
- AC3 A defect's card shows its state and local classification as a badge
  button. The badge uses text, not colour alone, and it opens the same defect
  actions. Its tooltip and accessible description carry the state's
  explanation and the context availability. "Reclassify…" offers the
  severity and category choices that the defect card offers today. An
  unclassified thread's card gains no button.
- AC4 Previous defect, Next defect and the "Defects N/M" position sit beneath
  the Comments tab's header, beside Next new comment. They move the selection
  between defect threads exactly as today.
- AC5 Every control dispatches the same core defect command, with the same
  conversation, defect and revision identities as the tab's controls did.
  RFC-BPR-0009's defect courses pass unchanged, and nothing publishes.
- AC6 Evidence: the conversation-cards, window, focus-order and composed
  courses assert the menu, the badge and the navigation where this card puts
  them. Keyboard and VoiceOver reach every defect action without a pointer.
  Renders at 100, 125 and 200 percent cover an unclassified card and a
  defect's card.
- AC7 The owner acts on a defect from its thread on macOS, and judges it with
  one question: are the thread's defect controls where you expect them?

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as the owner amended it on 2026-09-25. It moves
[RFC-BPR-0009](../../rfcs/RFC-BPR-0009-defect-fix-verification-workflow.md)'s
controls without changing their semantics. The core decides, the host
performs, Qt presents. No publication, authority, provider, Git or storage
capability changes. Windows and Linux station runs follow the owed commands on
[[BPR-1135]].
