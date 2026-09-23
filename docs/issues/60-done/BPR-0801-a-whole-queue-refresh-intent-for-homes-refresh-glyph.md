---
schema: "implement-rfc.card.v1"
id: "BPR-0801"
title: "A whole-queue refresh intent for Home's refresh glyph"
status: "Done"
rfcs: ["RFC-BPR-0032"]
depends_on: []
metadata: {"pack":"bpr-shell-surfaces-v1","origin":"RFC-BPR-0032 A7 divergence 3, accepted as shipped by the owner on 2026-09-16 (bpr-0779-filter-and-refresh-wording, 910af65f); named as a follow-up by d3/d4 and by d8, allocated on main in d9","acceptance":["A7"]}
acceptance: [{"criterion":"The first question is answered before an intent is written: whether the queue should be refreshable at all, and by whom","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"artifact","detail":"Answered on the card's Resolution: yes, by the reviewer, through RFC-BPR-0026's explicit Refresh of personal discovery; the review strip's refresh keeps acting on the open review.","path":"docs/issues/60-done/BPR-0801-a-whole-queue-refresh-intent-for-homes-refresh-glyph.md#resolution--2026-09-23"}},{"criterion":"If a whole-queue refresh is wanted: one typed intent, decided in the core and performed by the host, over the existing queue projection; no widget reaches a provider","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"queue_discovery_intent::refresh from Home's own refreshQueueAction, performed by personal_discovery_host; answers' membership decided in core queue_membership/bucket_for; scheduling is the host's per review-queue.md ownership. qt_workbench_home_discovery_scale_* pin the exact intent; application_personal_discovery_public_path drives it composed."}},{"criterion":"The glyph's enabled state tells the truth about what it will do","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"qt_workbench_home_discovery_scale_{100,125,200}: disabled with a reason without an account session and while a refresh runs, enabled once current with no pull request open; composed course finds it enabled in both windows."}},{"criterion":"Concurrent and repeated presses are bounded, and a refusal is reported under the bar rather than in a modal","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"command","detail":"application_review_queue_enrollment: Refresh during a refresh adds nothing; Refresh inside a retry-after is refused (RefusedRefreshes) with the remaining wait published; the Home sentence under the bar says 'Refresh again in N seconds.' (qt_workbench_home_discovery_scale_*)."}},{"criterion":"RFC-BPR-0032 A7's closeout row on BPR-0781 is amended to point at the outcome, without rewriting its history","outcome":"pass","revision":"0d56cee5","evidence":{"kind":"artifact","detail":"BPR-0781's A7 row gains a dated amendment naming BPR-0801/BPR-0990; its acceptance and history are unchanged.","path":"docs/issues/60-done/BPR-0781-close-rfc-0032.md"}}]
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

## Resolution — 2026-09-23

**The first question, answered.** Yes: the queue is refreshable, by the
reviewer, and RFC-BPR-0026 already names how - personal discovery of the
saved repositories with "an explicit Refresh action" beside refresh on
activation subject to freshness and backoff. The glyph on Home is that
action. It does not keep acting on the open review there; the review strip's
refresh still does, with its shortcut.

Implemented by [[BPR-0990]] (`0d56cee5`, Done `aef2c455`) over
[[BPR-0989]]'s scheduler:

- **One typed intent.** Home's glyph is its own action, `refreshQueueAction`,
  that sends `application::queue_discovery_intent::refresh`; arriving at Home
  sends `activate`. The profile's `personal_discovery_host` performs it. The
  membership an answer produces is decided in the core (`queue_membership`,
  `bucket_for`) and the queue register; the scheduling - freshness, backoff,
  in-flight bound - is the application host's, as `review-queue.md`'s
  ownership table assigns clocks, refresh scheduling and backoff to the host
  rather than the core. No widget reaches a provider.
- **Enablement tells the truth.** Enabled whenever an account session can run
  discovery, including with no pull request open; disabled, and saying so,
  while a refresh runs or without an account session.
- **Bounded presses, refusals under the bar.** A press during a refresh adds
  nothing (the glyph is disabled and the host ignores it); a press inside a
  rate limit's retry-after is refused and the Home sentence under the bar says
  how long to wait.
- **The A7 row** on [[BPR-0781]] is amended to point here, with its history
  unchanged.

Evidence: `qt_workbench_home_discovery_scale_{100,125,200}` pin the glyph's
place, enablement, tooltip and exact intent; `application_personal_discovery_public_path`
drives the second window's glyph to one bounded refresh of personal reads
only; `application_review_queue_enrollment` pins the refused refresh inside a
backoff (`test_discovery_failures_keep_rows_and_back_off`) and the no-op
during a refresh (`test_discovery_freshness_and_forgotten_repositories`).
Limitation: no attended look at the changed glyph was taken.
