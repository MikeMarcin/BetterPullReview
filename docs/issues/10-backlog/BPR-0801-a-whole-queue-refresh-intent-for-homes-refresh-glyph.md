---
schema: "implement-rfc.card.v1"
id: "BPR-0801"
title: "A whole-queue refresh intent for Home's refresh glyph"
status: "Backlog"
rfcs: ["RFC-BPR-0032"]
depends_on: []
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0032 A7 divergence 3, accepted as shipped by the owner on 2026-09-16 (bpr-0779-filter-and-refresh-wording, 910af65f); named as a follow-up by d3/d4 and by d8, allocated on main in d9","acceptance":["A7"]}
---

# BPR-0801: A whole-queue refresh intent for Home's refresh glyph

## Outcome

RFC-BPR-0032's Home strip holds Group, State headers, Filter and a refresh
glyph. The glyph shipped, and it does something narrower than the strip it sits
in suggests: **it acts on the pull request open beneath Home, and is disabled
when none is open.** There is no whole-queue refresh intent in the application at
all, so there is nothing for it to send.

This was found while building the strip ([[BPR-0776]] records it as "(b) No
whole-queue refresh intent exists for the strip's refresh glyph") and carried
through [[BPR-0783]] and [[BPR-0781]] A7 as divergence 3. It was put to the owner
on 2026-09-16 alongside the two other A7 divergences, and they chose **"Accept
all three as shipped"** (decision 910af65f).

**Accepted as shipped is not the same as absent.** A7's row says so in as many
words: the divergences become the behaviour, and the absent whole-queue refresh
intent stays a named follow-up for main. d8 left it un-carded at a closeout
boundary. This card is where it lives, and it starts from the owner's acceptance
rather than against it: the glyph is not a defect, and nobody is owed a fix. What
is owed is a decision made once, on purpose, instead of a gap rediscovered.

## Acceptance

- **The first question is whether the queue should be refreshable at all, and by
  whom.** Answer it before writing an intent. A reviewer looking at Home wants to
  know their queue is current; whether that is a button, an observer, or already
  covered by RFC-BPR-0010's notifications is the thing to settle. If the answer
  is that the glyph should keep acting on the open review, say so and retire the
  question rather than leaving it open a third time.
- If a whole-queue refresh is wanted: one typed intent, decided in the core and
  performed by the host, over the existing queue projection. No widget reaches a
  provider — the core decides, the host performs, Qt presents.
- The glyph's enabled state tells the truth about what it will do. Today it is
  disabled with no review open, which is honest about the narrow behaviour and
  would be wrong for the broad one.
- Concurrent and repeated presses are bounded, and a refusal is reported where
  Home reports refusals now, under the bar, rather than in a modal.
- RFC-BPR-0032 A7's closeout row on [[BPR-0781]] is amended to point at the
  outcome, whichever way it goes. That row is Done and its acceptance stands; the
  amendment names the resolution, it does not rewrite the history.

## Evidence

Record the focused command or gesture, the direct surface, the result, the
producing revision and the remaining limitation. Evidence is **not run** at card
authoring: nothing here has been implemented or observed.

Where the existing record lives, for whoever takes this:

- [[BPR-0781]] A7, divergence 3, with the owner's decision 910af65f.
- [[BPR-0776]] Decisions, item (b).
- The strip itself and its glyph: [[BPR-0783]].

## Boundaries

No GitHub or GHES mutation, credential access, provider change or new preference
is authorized by this card. Do not widen it into RFC-BPR-0010's observers and
notifications; if that is where the answer lives, record it here and close this
card against that RFC rather than implementing it twice.
