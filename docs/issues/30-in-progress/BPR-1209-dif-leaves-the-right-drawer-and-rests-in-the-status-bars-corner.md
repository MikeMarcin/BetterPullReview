---
schema: "implement-rfc.card.v1"
id: "BPR-1209"
title: "Dif leaves the right drawer and rests in the status bar's corner unless a screen gives him a prime spot"
status: "In Progress"
rfcs: ["RFC-BPR-0052"]
depends_on: ["BPR-1134"]
priority: "P2"
milestone: "Review workbench scaffolding"
platforms: ["mac", "win", "linux"]
metadata: {"origin": "owner direction on BPR-1204's tab, 2026-09-25, in chat, relayed by the coordinator", "criteria": ["AC-09"], "related_rfcs": ["RFC-BPR-0031", "RFC-BPR-0032"], "amends": ["RFC-BPR-0031 placement (B6 and the explicit AI tools area)"], "carries": ["BPR-0800"], "related_cards": ["BPR-0800", "BPR-0966", "BPR-0775", "BPR-1134", "BPR-1204", "BPR-1135"], "reservation": "docs/issues/_reservations/BPR-1208--BPR-1210--a47a9558dfa6.json", "milestone_plan": {"id": "rfc-bpr-0052-review-workbench-scaffolding", "path": "docs/rfcs/plans/rfc-bpr-0052-review-workbench-scaffolding/milestone-plan.json"}}
---

# BPR-1209: Dif leaves the right drawer and rests in the status bar's corner unless a screen gives him a prime spot

## Owner direction

From the owner's direction of 2026-09-25 on the tray's second tab, recorded
verbatim on [[BPR-1204]]:

> Dif living in a tab that's likely hidden forever makes no sense, definitely doesn't belong there. [...] Move Dif out of the right drawer entirely, he can live in the lower left corner of the status bar by default just like AllTheThings unless he's got a prime spot on a screen like on the first time experience home.

It restates, for every surface, the owner's direction of 2026-09-15 on
[[BPR-0800]] ("He can show up on the home page for the first time login
experience, but otherwise he should mostly sit in the very bottom left corner
and stay out of the way"). [[BPR-0775]] built that direction for Home only.

Today the one Dif widget has three places. On signed-out Home he stands
beside the first-launch card at 96 px. On every other Home he rests at 40 px
in `workbenchDifCornerSlot`, the status bar's first widget. On review surfaces
he sits in RFC-BPR-0031's explicit AI tools slot, which [[BPR-1134]] put at
the top right of the tray's second tab. On all other surfaces he is hidden.

## Conflicts with earlier records, and how each is resolved

The coordinator relayed that the owner delegates the Dif conflict and every
other open choice in this batch. Each resolution below marked "decided" is
the implementer's, made under that delegation. Changing one later takes a
new owner decision.

1. **[[BPR-0800]]'s open question is answered by the owner's direction.**
   BPR-0800 asked, before any code, whether the corner replaces the AI tools
   slot on review surfaces, or whether the slot keeps its cues with the
   corner as Dif's resting place between them. "Move Dif out of the right
   drawer entirely" removes the slot from the drawer. So the corner replaces
   it. This is the owner's answer, not a delegated decision.
2. **[[BPR-0966]]'s Compose workshop slot.** In session `06ad5734` the owner
   chose (decision `dba44cfd`) "A visible workshop slot on Compose: Dif moves
   there and plays ready and acknowledge", over the corner, which that
   question offered. The new direction says "corner by default … unless he's
   got a prime spot". **Decided:** Compose's workshop slot is a prime spot.
   It holds Dif while a workshop proposal is shown on Compose and after that
   proposal is accepted. BPR-0966's verdict and acceptance are unchanged.
   Rationale: the owner picked that slot over the corner for Compose on the
   same day, and the new direction keeps a prime spot wherever a screen has
   one. Read together, both verdicts stand.
3. **RFC-BPR-0031's placement rules.** RFC-BPR-0031 is Auto-Verified. It
   places Dif in "one explicit visible AI tools slot" in the conversations
   drawer (B6), says "All other surfaces hide it", and admits "No persistent
   toolbar/status mascot". The owner's direction supersedes those placement
   rules. RFC-BPR-0031 now carries an owner amendment of 2026-09-25 recording
   the direction. Its accepted criteria are not rewritten, and this card's
   evidence is recorded against the amended placement.
4. **Where the cues play.** **Decided:** every RFC-BPR-0031 cue keeps its
   trigger, identity and deduplication, and plays wherever Dif is. That
   includes `compare_glance`, whose trigger stays the reviewer explicitly
   selecting the tray's AI tab (Notes, [[BPR-1204]]) with its content shown.
   B7 (all ten clips reachable) is not amended. Rationale: the direction
   moves Dif, not what his cues mean, and on Compose the owner kept the cues
   too (BPR-0966).
5. **[[BPR-0800]] folds into this card.** **Decided:** BPR-0800's acceptance
   (the review-surface corner, the one-sprite rule, the RFC-BPR-0031
   amendment and its own attended verdict) is carried here. BPR-0800 stays in
   Backlog, depends on this card and closes on its evidence. Rationale: both
   cards move the same single sprite from the slot to the corner, and two
   cards would build it twice.
6. **Sequencing with [[BPR-0966]].** **Decided:** neither card waits for the
   other. Both change `application_agent_finding_dif_context_contract`, and
   whichever lands second updates it. Under this card, the consider and
   attention observations move from the tray tab to the corner. BPR-0966's
   isolated Mac follow-up exposes the tray tab's slot for its consider paint.
   Compose's ready and acknowledge stay BPR-0966's. Rationale: the two
   placements do not overlap on screen, so ordering them would only delay
   one.
7. **[[BPR-1134]] and [[BPR-0775]] stay as recorded.** BPR-1134 put Dif's
   slot in the tab, and that verdict stays bound to the frozen m2 app.
   BPR-0775's Home corner and first-launch placement are the rule this card
   extends. They are not rebuilt.

## Acceptance

- AC1 Dif is not in the right drawer. The tray's AI tab has no Dif slot, and
  selecting that tab never shows Dif in the drawer.
- AC2 On every workbench surface without a prime spot, the one Dif widget
  rests in the status bar's leading-corner slot (`workbenchDifCornerSlot`),
  at the 40 px Home already uses. Those surfaces are signed-in Home, Home over
  a review, Overview, Read, Discuss, and Compose with no workshop proposal
  shown. In the corner he has no pointer or focus path and takes no space
  from a control. He steps aside while the status bar carries a transient
  message, as RFC-BPR-0032 records for Home.
- AC3 The prime spots are the signed-out first-time Home, at 96 px beside the
  card's sentence (unchanged), and Compose's workshop slot as [[BPR-0966]]
  builds it. The one-sprite rule holds: Dif is never in two places at once,
  in one window or across windows. The single decoded retained catalog is
  still shared (RFC-BPR-0031 B10).
- AC4 Each RFC-BPR-0031 cue plays where Dif is, with its unchanged trigger
  and deduplication. `compare_glance` plays in the corner when the reviewer
  explicitly selects the tray's AI tab. The Dif context course observes
  consider and attention in the corner.
- AC5 The courses that assert the old placement follow the amendment. These
  include `test_dif_moves_to_the_explicit_ai_area_without_owning_it` in
  `qt_workbench_window_scale_{100,125,200}` and RFC-BPR-0031's B5 and B6
  courses. A negative control that puts Dif back in the drawer fails them.
  None is loosened.
- AC6 The owner judges the corner on a review surface on macOS, with one
  question: does Dif at 40 px in the corner, beside live review text, stay
  out of the way? This is BPR-0800's attended criterion.

## Boundaries

This card implements [RFC-BPR-0052](../../rfcs/RFC-BPR-0052-review-workbench-scaffolding.md)
AC-09 as the owner amended it on 2026-09-25, together with the owner's
amendment of
[RFC-BPR-0031](../../rfcs/RFC-BPR-0031-dif-workbench-animation-integration.md)'s
placement. A mascot is presentation. It reaches no store, provider or
process, and it gets no new preference. There is no sprite work (RFC-BPR-0030),
and the Appearance page's Dif controls ([[BPR-0790]]) are not reopened. The
core decides, the host performs, Qt presents. Windows and Linux station runs
follow the owed commands on [[BPR-1135]].
