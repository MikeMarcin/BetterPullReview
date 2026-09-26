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

## Engineering source review — 2026-09-26

The candidate follows the delegated context-menu, badge and Comments-navigation
decision. The existing defect-card actions supply the verdict commands in a
thread-bound menu; they retain the captured defect identity when selection
changes. The existing severity/category editor opens in a local modal dialog.
Only visible cards receive native accessible badge buttons. An unclassified
thread has no badge. Existing conversation indexes and measurement caches remain.

The pinned application scope has no intervening commit after `30748ed6`. This
slice adds a per-accepted-snapshot join from both halves of the durable defect
identity to the exact thread. Ordinary defect projections visit retained defect
records; snapshot/ledger retirement invalidates the join. Ambiguous identities
receive no thread binding. It changes no core defect command, effect ownership,
provider admission, publication or ledger capability. Its tests-only composition
probe checks the accepted thread menu and badge and excludes the private tab.
The full-application and manifest pins are resealed with this recorded review;
the production mutation-branch pin is unchanged.

Native VoiceOver and owner placement judgments remain pending.

The first combined run found two corrected presentation defects: the new
navigation buttons lacked accessible descriptions, and BPR-1205's earlier-pair
filter also hid current threads from the unselected review-wide projection.
The latter stalled both composed courses before their first file selection.
The correction filters a selected file only, retains the complete unselected
forest, and invalidates the placed forest only when file presence changes,
so ordinary file-to-file projection retains BPR-0972's cache. A focused
transition test covers unselected → older file → unselected → older file →
Discuss. The old classification unit fixture now names its actual review.cpp
file; a thread from another file must not classify against the displayed file.
The first composed course exhausted its 40-second bound; the second was stopped
after showing the same initial stall to release the shared Qt slot. Neither is
claimed as passing. Existing core defect workflow and host correlation checks
pass 2/2 after building their previously absent test binaries.

The corrected initial course reaches `DEFECT_THREAD_CONTROLS_OK`. Its later
reversed-pair wait still expected selectable current threads, contradicting
BPR-1205's owner amendment. That test-only gate now requires those identities
while the current pair is used, then asserts their absence on the older pair
alongside the unchanged no-anchor/no-classification, durable correlation,
refused-persistence, explicit verification, reclassification and restart checks.
No product admission or original semantic expectation is relaxed. This
tests-only update is included in the application pin review above.

The same tests-only probe now enters Review through the shared public modality
step and opens Comments through its existing action before selecting a file.
The composed badge check requires actual visible cards, rather than inspecting
a retained but hidden Overview widget. This is included in the pinned source
review; the production mutation branch remains unchanged.

Final engineering checks pass: core/host 2/2, conversation panel/focus/reveal
3/3, the four-process GitHub/GHES defect composition (54.98 seconds), and
window/focus scales plus review-round-trip source pin 6/6 (79.04 seconds).
Logs are under `/Users/jaeger/.codex/tmp/bpr-rfc0052-m2b-it1v3wy6/evidence/1208-*`.
The top-card renders at 100/125/200 were inspected. The new bottom-card
capture covers a classified and an unclassified thread together; its first
three selector runs all used 100 percent because CTest overrides the caller's
scale variable. The 125/200 bottom captures remain to be rerun with explicit
isolated commands, rather than mislabelled as scale evidence. The screenshot
change is test-only. Native VoiceOver and the separate owner judgment remain
pending regardless of those captures.

The corrected direct network-isolated selector runs set and assert 1.25 and
2.0 explicitly; both passed (`1208-correct-scale-{125,200}.log`). Their
1600×1125 and 2560×1800 captures were inspected: the classified badge and
unclassified card, along with the Comments navigation, are readable at each
scale. This completes the automated/rendered part of AC6; VoiceOver remains
pending.
