---
schema: "implement-rfc.card.v1"
id: "BPR-1212"
title: "Rethink the Compose UX with frontend-design"
status: "In Progress"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac","win","linux"]
metadata: {"origin":"BPR-0966 final owner verdict, 2026-09-26","related_rfcs":["RFC-BPR-0037","RFC-BPR-0052"],"related_issues":["BPR-0966","BPR-1203","BPR-1207","BPR-1161","BPR-1083"],"requested_skill":"frontend-design","timing":"Later follow-up; not active m2b work or a blocker for accepted Dif placement","decision_id":"3bcafec9-7811-46dd-83dd-f7bda4cdac1b","candidate":"BPR-0966-compose-slot-v002","candidate_revision":"388f814c7be796b91c4dd6779422be3d4321e4f3","mandate_revision":"r4-with-owner-compose-20260925-r3"}
---

# Rethink the Compose UX with frontend-design

## Owner request and boundary

The owner accepted Dif's placement in the Compose workshop header, then requested
a separate, later redesign of the overall Compose experience. Their final note,
verbatim:

> Dif is fine.
> I utterly hate the compose UX and it needs a full rethink with $frontend-design as a later follow-up.

Attended session `54fd75833481882c3bb47745f8e30b75`, final Accept at sequence 5,
decision `3bcafec9-7811-46dd-83dd-f7bda4cdac1b`, handed back at sequence 6.
Candidate `BPR-0966-compose-slot-v002`, product
`388f814c7be796b91c4dd6779422be3d4321e4f3`, fingerprint
`87eef088bdcccaf17caeaa52833756bca5bb4bee56287ccbe81a1bc888f8e3fd`.
[The exact verdict](../../rfcs/plans/bpr0966-dif-cues/owner-verdict-compose.json)
retains the superseded earlier Reject as history.

This is a full UX rethink, not merely a label fix. Start from the integrated
product when this card is activated, including [[BPR-1203]]'s kept-draft state
and [[BPR-1207]]'s usable Comment entry. Preserve those independently owned m2b
fixes. This later card does not expand m2b, undo [[BPR-0966]]'s accepted Dif
placement, or reopen RFC-BPR-0037's historical acceptance by implication.
No redesign has been undertaken or accepted in this registration round.

## Intended work

Use the requested `frontend-design` skill in the later design round. It was
not available in the registration session; resolve the actual installed skill
before design work and report absence rather than silently substituting a
style. Develop concrete alternatives from the actual Compose workflows:
whole-review summary and disposition, new inline comment, thread reply, kept
or unavailable drafts, and AI workshop proposals with explicit acceptance.
Make the destination, scope, draft state, navigation and next action clear.
Translate the chosen design to native C++/Qt presentation; a browser prototype
is design evidence, not the implemented product.

Keep GitHub as shared authority, drafts private until explicit publication,
and the existing host/core gates for account, PR, head, line, session and draft
revision. Preserve review/comment/reply semantics, retained draft content,
keyboard return paths and accessible controls. Dif remains decorative with one
widget; redesigning the surrounding page does not authorize asset changes or
invented success cues.

## Related: sharing a Review Cycles round ([[BPR-1161]], 2026-10-02)

The owner rejected Review Cycles' Compose feedback page,
`BPR-1161-compose-share-v001`, at event 834. Their note: "I have no idea how
I got to this screen, why I would wnat to do any of these actions, and what
is important." The `frontend-design` round that followed lives on BPR-1161,
because it is that card's RC61-05 and RC61-06 and not this card's pull
request flows. The round is [design round
v1](../../design/BPR-1161-share-round-v1.html), with directions A (a share
sheet), B (sharing from the Notes tray) and C (the round as a letter), and
the owner's choice is asked as `BPR-1161-share-direction-v001`. When this
card is activated, start from that choice and its plain-word vocabulary. It
covers a labelled entry, purpose stated beside each action, one primary
action, and machine detail behind "exact text". Do not redraw it as a
separate convention. This note neither activates this card nor changes its
criteria.

## Owner amendment: an inline composer beside the code ([[BPR-1083]], 2026-10-05)

Accepting `BPR-1083-chip-menu-v002` in attended session
`c2a2442c5ce16c3fbb77598fc87360bb` (decision
`0df6d634-f0bf-49c2-b41c-d786bc3e3f64`, 2026-10-06T02:50Z), the owner wrote,
verbatim:

> Functionality works as expected.
>
> I hate this full screen comment page being the default. I want to be able
> to still see the code I'm commenting on. Maybe it has an expand button to
> get something similar to this.

Their screenshot,
[`BPR-1083-owner-ref-full-page-composer.png`](../../design/owner-refs/BPR-1083-owner-ref-full-page-composer.png),
shows Read after Comment on a line of `renderer.cpp`: the comment page
(Back, the target line, Publish and Discard, the label chip and the body)
fills the whole comparison area, and no code is visible. The note's third
paragraph, about the decorations, is BPR-1083's and was built there.

This card's "new inline comment" flow already covers the ask, so it is
recorded here and not filed separately (coordinator, 2026-10-05: "if 1212
already covers it, amend 1212"). It adds AC5 below. The rest of this card's
scope, timing and Backlog lane are unchanged.

## Acceptance

- AC1: Use `frontend-design` to inventory the integrated flows and present concrete native-feasible layout and interaction alternatives, including normal, empty, unavailable/conflicted and kept-draft states. Record the chosen design and rationale against the owner's full-rethink request.
- AC2: Implement the chosen composition flow with visible target and draft context, usable authoring space and clear local accept/reject versus publish actions. Preserve exact identity, draft retention, explicit publication, keyboard navigation and accessibility through the existing core/host/Qt boundaries.
- AC3: Exercise the actual composed entry-to-edit-to-return paths using safe fixtures, including comments, replies, whole reviews and workshop proposals; prove draft preservation, stale-target refusal and zero publication from navigation or local acceptance. Verify rendered narrow/wide and larger-font states on each claimed platform and keep unavailable native evidence explicit.
- AC4: Obtain an owner judgment on the built Compose UX at its exact candidate, with one question per independent criterion and Accept/Reject for keep-or-change decisions. Record any resulting follow-ups without treating the earlier Dif Accept as Compose acceptance.
- AC5 (owner amendment, above): A new inline comment, and a reply on a thread, open their composer inline beside the code by default, so the commented line and its neighbours stay visible while writing. An expand control, with a keyboard route and an accessible name, opens the full-page composer with the same draft, and returns from it to the inline one. Both keep the draft, label controls, Publish and Discard, and neither adds a scroll trap or a clipped composer (RFC-BPR-0050).

## Owner direction: compose in the drawer, one flow with Review Cycles ([[BPR-1290]], 2026-10-06)

Handing back BPR-1290's built drawer v003, the owner wrote in chat, verbatim
(the BPR-1290 verdict itself is recorded on that card):

> I want add comment from the right-click menu. I want add comment to show up
> in the drawer instead of the full screen add comment window. Fold add
> comment window into discuss. Drawer comment should have something like a
> maximize button to go view it in discuss.
>
> I want to try to unify a bunch of this interaction with the local project
> flow. Maybe we do right click to add comment / suggest change with enter /
> shift+enter as shortcuts?

Earlier, accepting BPR-1083 (quoted in full under AC5's amendment above):

> I hate this full screen comment page being the default. I want to be able
> to still see the code I'm commenting on.

The coordinator activated this card on 2026-10-06 for a design round: an
inventory of today's compose entry points and keys in both flows, a unified
right-click and keyboard flow with the new comment drafted in the inline
drawer and a maximize into Discuss, a clickable prototype extending
[`BPR-1290-inline-drawer-v2`](../../design/BPR-1290-inline-drawer-v2/index.html),
and owner questions answered in chat. No attended session.

## Scheduling

Activated 2026-10-06 (above). Before that: Backlog, explicitly for later
activation. No new predecessor status, remote mutation, installed-package or
release approval follows from this card. The design round neither builds
anything in `src/` nor claims a native or human result.
