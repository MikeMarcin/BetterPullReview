---
schema: "implement-rfc.card.v1"
id: "BPR-1293"
title: "A PR-wide conversation finder you can summon"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "win", "linux"]
metadata: {"origin":"Owner chat, 2026-10-06, during BPR-1290","reservation":"docs/issues/_reservations/BPR-1293--BPR-1293--370e927e88e9.json","related_rfcs":["RFC-BPR-0005","RFC-BPR-0028"],"related_cards":["BPR-1290","BPR-1294","BPR-0418","BPR-0211","BPR-0649"],"implementation_note":"Use the frontend-design skill when designing; owner review is required","open_questions":["main-pane tab or summoned window"]}
---

# BPR-1293: A PR-wide conversation finder you can summon

## Origin

While redesigning the Conversations drawer ([[BPR-1290]]), the owner said in
chat on 2026-10-06, verbatim:

> what's the point of the conversation view if the inline comments view shows
> the whole thread like the first mockup before I said split it? Seems like
> they show the same thing except for navigation.

> I kind of feel like all of these tree view mockups are worse than what we had
> before the redesign. At least they all confuse me and most encroach on the
> screen real-estate heavily. I'd almost rather just have the tree view be a
> tab in the main pane or a window you can summon like the home screen.

> Honestly I'm not too attached to the modalities right now, i'm not sure how
> long I want to retain them. (beyond overview and review of course)

The agreed plan split the drawer's two jobs. The drawer becomes inline only
(BPR-1290). Finding the right conversation across the pull request moves out
of the drawer into a place of its own: this card.

The coordinator first filed this card as "A PR-wide conversations view you can
summon", holding the finder, pull request comments, review summaries and deep
threads. The owner then narrowed it, verbatim:

> Well I dont necessarily want to summon non-file comments like that. Just the
> navigation I was thinking. We might need a different better solution for PR
> wide comments that arent on a file.

## Outcome

A view for **navigation only**: the Treeverse finder ([[BPR-0418]], the
owner's [[BPR-0211]] choice) across every conversation in the pull request,
grouped by file, used to jump to the right conversation. Picking one opens its
file in Read at its line with the thread open in the drawer (BPR-1290). It is
not tied to the Read or Discuss modalities.

It does not hold or read pull request comments or review summaries that are not
on a file. Those are [[BPR-1294]]'s open problem.

## Open question for the owner

Where the finder lives: **a tab in the main pane**, or **a window you summon
like Home**. Not designed yet. Ask with drawings when this card is taken up.

## Not decided here

- The finder's drawing. Round v3 of BPR-1290 drew a map of the whole pull
  request, and the owner found all of those layouts confusing and too greedy
  for space. Start from that verdict, not from v3's frames.
- Deep threads (`branching-thread-v1`, RFC-BPR-0028 AC-08): whether the finder
  shows a conversation's branches or only finds the conversation, which then
  reads in the drawer.
- What happens to Discuss (RFC-BPR-0028 AC-05/AC-08, [[BPR-0649]]), given the
  owner's note on the modalities.

## Owner direction, 2026-10-06

In chat, about the inline drawer prototype
([`BPR-1290-inline-drawer-v1`](../../design/BPR-1290-inline-drawer-v1/index.html)),
verbatim as relayed by the coordinator:

> I like it

> Of course it didnt show the tree nav

The coordinator took this card up from there: extend the same prototype so its
finder button and a hotkey open the Treeverse finder over the same pull request,
and let the owner try both placements (a main-pane tab beside the file, and a
view summoned over the window like Home) in the prototype itself. The placement
stays the owner's open question; the prototype answers nothing on its own.
Feedback comes in chat, with no attended session.

## Acceptance

To be written with the owner when this card is taken up. Owner review is
required, not self-certified.
