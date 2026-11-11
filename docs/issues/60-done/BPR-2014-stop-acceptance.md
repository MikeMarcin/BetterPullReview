---
schema: "implement-rfc.card.v1"
id: "BPR-2014"
title: "Accept a tour stop the way a file is accepted"
status: "Done"
rfcs: ["RFC-BPR-0106"]
metadata: {"mandate_revision":"v0-5-0-prerelease-targets-r1","reservation":"docs/issues/_reservations/BPR-1989--BPR-2035--77a75f3eb099.json","milestone_plan":{"id":"v0-5-0-prerelease-targets-v1","path":"docs/rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json"},"imported_from":"docs/rfcs/assets/v0-5-0-prerelease-targets-v1/cards/DRAFT-0026-stop-acceptance.md"}
depends_on: ["BPR-2013"]
acceptance: [{"criterion":"RFC-BPR-0106 AC08","outcome":"pass","revision":"3285ec887627035f0d1ac76b1d6f3521ea158f0e","evidence":{"kind":"observation","detail":"Owner looks: arrival b331f132, ring states 58e14b67, Accept 7efc74c5 (session 679d26ab); stop at rest v2 54a1ceca (session 5ea0ce22); stop editing v3 5465deb7 and tour panel layout 950eef0f (session 7c9206759d935f663d6a13ce0e008afc, frozen from 3285ec887)"}},{"criterion":"BPR-2014 critical review","outcome":"pass","revision":"3285ec887627035f0d1ac76b1d6f3521ea158f0e","evidence":{"kind":"artifact","detail":"Independent critical review of 93b1045a8 (pass-with-findings, all fixed test first) and independent re-review of d02f13efb/fb94545ec (pass-with-findings, nothing blocking, all fixed and pinned); docs/verification/RFC-BPR-0106.md 'Critical review' and 'Re-review of the fixes'"}}]
---

# Accept a tour stop the way a file is accepted

## Scope and acceptance

Implements
[RFC-BPR-0106](../../rfcs/RFC-BPR-0106-a-tour-stop-shows-its-code-and-is-accepted-like-a-file.md).

Record in the core what the author accepted for a stop (its text, its
destination, and the code there and at each reference in its text) and decide
Accepted, Not accepted yet and Changed since accepted from it. Make Accept on
the current stop, Save of an edit and Accept on the stop's own proposal accept
it, and nothing else. Keep an unchanged stop Accepted across a new head and
turn yellow only the stops whose material changed. Require every stop Accepted
to publish, replacing the Ready mark, the Head changed chip and the separate
confirmation of lines. Carry saved drafts across: a stop that was Ready and
reaffirmed stays Accepted, and every other stop is Not accepted yet.

Covers AC02 to AC05 and the ring states of the owner's look in AC08. The owner
accepted these rules as drawn in the stop mockup on 2026-11-10; the look at
the running stop is still owed. This amends
publication eligibility under RFC-BPR-0042, 0043 and 0096: give it critical
review, since it decides what may be published.

This is part of `tour-stops` in the [plan](../../rfcs/plans/v0-5-0-prerelease-targets-v1/milestone-plan.json).

## Evidence

Product candidate `93b1045a8`; record
[RFC-BPR-0106 verification](../../verification/RFC-BPR-0106.md).
Frontend-design was loaded before the Qt work; the accepted stop mockup is the
design.

- AC02: `core_tour_stop_acceptance` (each row of the state table, with its
  reason), `qt_tour_stop_acceptance_card` (each ring and row's words) — pass.
- AC03: `core_tour_stop_acceptance`, `qt_tour_stop_acceptance_card`,
  `qt_tour_chat_in_tab` (the stop's own proposal accepts; Accept all does
  not), `application_review_tour_writing` (a hidden Accept accepts nothing) —
  pass.
- AC04: `core_diff_semantics` (`test_tour_acceptance_after_push`, real
  comparisons: exactly the two stops whose lines or linked lines changed turn
  yellow, each with its reason) — pass at the core. Through the window the
  only fixture push has no readable second head, so the course shows "not
  compared yet" rather than a real comparison; recorded as a gap.
- AC05: `core_tour_stop_acceptance` (`test_publication`), and all fourteen
  publication and CLI courses publish all-accepted tours with their write
  assertions unchanged — pass.
- AC08 (with BPR-2013's stop frames): the owner's look, filed and not waited
  on (below).
- Critical review of the publication rule: an independent reviewer returned
  pass-with-findings, provisional, on `93b1045a8`. Each finding was
  reproduced by a test and fixed or recorded in the post-review round (the
  [record's table](../../verification/RFC-BPR-0106.md#critical-review)). An
  independent re-review of the fixes returned pass-with-findings, nothing
  blocking; its five findings were taken test first and fixed or covered
  ([re-review table](../../verification/RFC-BPR-0106.md#re-review-of-the-fixes-2026-11-10)).

## Owner look (filed, not waited on)

Attended session `679d26abbfa93b3357729061432131ba`, state
`/Users/jaeger/.attended-review-state/tour-stops-v002-2026-11-10/state`,
cursor 15, host PID 61912 (its `tailscale serve` child 61936), five
candidates, one question each, ready and protected: `BPR-2013-arrival`,
`BPR-2013-stop-at-rest`, `BPR-2013-stop-editing`, `BPR-2014-accept` (on the
frozen app, conventional-comments-v1 renderer.cpp with `--attended-tour-look`)
and `BPR-2014-ring-states` (the frozen `tour_stop_acceptance_card --look`:
each ring state after a push). Frozen root
`/Users/jaeger/tour-stops-v001.5rt9c5gn` from `93b1045a8` (app SHA-256
`c5e2a178c2ea…`, card test `6683038724ed…`; the launcher's own check reached
`ATTENDED_TOUR_SHOWN`). The URL stays in the session state.

## Owner hand-back, session `679d26ab…` (2026-11-10)

Read with `attended_review.py status` (sequence 44); host 61912/61936
stopped, root `tour-stops-v001.5rt9c5gn` removed.

| Candidate | Verdict | Note |
|---|---|---|
| `BPR-2013-arrival` | Accept (`b331f132`) | |
| `BPR-2013-stop-at-rest` | Accept (`c13c4ad5`) | "Not sure why it says \"Not accepted yet.\" Next to accept. That feels pretty obvious." |
| `BPR-2013-stop-editing` | **Reject** (`2c1229e8`) | "I don't understand what these do or how to use these at all. Cancel is like a different size to save, looks weird. There's some kind of theme disparity going on between what I see and your offscreen renders that I wish we could sort out." (screenshots: the stop's `…` menu with "Related source" / "Remove related link"; Cancel smaller than a blue Save) |
| `BPR-2014-accept` | Accept (`7efc74c5`) | "It's kind of weird that save turns it green. Maybe save should just be accept then." |
| `BPR-2014-ring-states` | Accept (`58e14b67`) | |

The ring states and Accept pass. AC08 is not complete while the stop frames
are reworked: the redundant "Not accepted yet." beside Accept goes, Save
becomes Accept, the editing tools are labelled and explain themselves, and
Cancel and Accept are one size. The card stays parked for that look.

## Rework after the owner's look (2026-11-10, `90785ae34`)

Frontend-design was loaded before the Qt work.

- A stop never accepted shows Accept alone: "Not accepted yet." is gone
  (the reason line stays for a stop changed since accepted, and for what
  Accept needs when it cannot accept).
- The editor's Save reads Accept (it keeps the edit and accepts the stop),
  with tooltip "Keep this edit and accept the stop (⌘↩)"; Cancel is the
  platform's own push button like it, and both take the wider one's width.
- The editing tools say what they do: Use selected lines, Link selected
  lines (on the row in the route too while there is room) and Unlink; each
  tooltip starts "Select lines in the code first."; one hint under them
  while the stop is edited; More's items read "Link selected lines" and
  "Unlink the chosen linked code".

Tests: `qt_tour_stop_acceptance_card` (no reason beside a never-accepted
stop's Accept; Cancel and Accept one kind and one size; the hint and each
tool's words), `application_review_tour_composition_public_path`.

## Owner hand-back, session `5ea0ce22…` (2026-11-11)

Read with `attended_review.py status --after 18` (handed back at sequence
49); host 78319/78332 stopped, root `owner-look-v003.w0k3mfr3` removed with
`freeze_attended_build.py remove`.

- `BPR-2013-stop-at-rest-v2`: Accept (`54a1ceca`).
- `BPR-2013-stop-editing-v2`: **Reject** (`1d39ba1e`, with a screenshot):
  "That text at the bottom is a giant waste of space. No one can or wants to
  read that. We also don't need the \"Enhance\" label if we're using the new
  enhance icon." Not implemented here; it goes to the next layout round
  (drop the explanatory text under the editing stop; Enhance as the icon
  alone).

RFC-BPR-0106 AC08 stays pending on the stop-editing look; the critical
review is still owed. The card stays parked on `human-review`.

## Layout round (2026-11-11, `f51063b5b`)

Frontend-design was loaded before the Qt work.

- Owner look `1d39ba1e`: the hint under the editing tools is gone; Enhance
  on a written stop (and on the Problem and Solution) is the nib alone, its
  name its accessible name and the head of its tooltip.
- Owner on the live build ("Things are moving around as I click them,
  buttons packed in like a clown car"): every chat toggle on the route
  shares one right edge, whether or not a step is done; opening a stop for
  editing moves neither its title, its chat toggle nor its lines link (the
  pencil keeps its room; the card's fields are top-aligned); Use selected
  lines and Link selected lines are one kind of quiet link; the routine save
  line ("Saved locally. Nothing published.") and the empty notices band are
  gone from the panel, and writing notices go to the window's status line;
  Draft Tour is no longer cut at the bottom. The status line no longer says
  "Nothing published." while the tour publishes (live-course bug a).

Tests: `qt_tour_stop_acceptance_card` (no hint; icon-only Enhance; chat
toggle and lines link stay put; no routine status text),
`qt_review_tour_agent_help`, `qt_review_tour_agent_setup_call`,
`qt_tour_side_chat`, `qt_tour_stop_card_cleanup`, `qt_scoped_prose_editor`,
`qt_workbench_window_scale_*`, `application_review_tour_composition_public_path`.

Filed, not waited on (the URL stays in the session state's `serve.log`): session `7c9206759d935f663d6a13ce0e008afc`, state `/Users/jaeger/.attended-review-state/layout-round-v001-2026-11-11/state`, cursor 12, host PID 73758 (its `tailscale serve` child 73768), frozen root `/Users/jaeger/layout-round-v001.5q22yahv` from `3285ec887` (app SHA-256 `97d0861e19dc…`, launcher check reached `ATTENDED_TOUR_SHOWN`). Candidates: `BPR-2014-stop-editing-v3` (the `tour_stop_acceptance_card --look`) and `tour-panel-layout-v1` (the frozen app). RFC-BPR-0106 AC08 stays pending on these looks; the critical review is still owed. The card stays parked on `human-review`.

## Owner hand-back, session `7c920675…` (2026-11-11)

Read from `session.json` (handed back at sequence 27); host 73758/73768
stopped, root `layout-round-v001.5q22yahv` removed.

- `BPR-2014-stop-editing-v3`: Accept (`5465deb7`).
- `tour-panel-layout-v1`: Accept (`950eef0f`).

RFC-BPR-0106 AC08 is met: arrival, ring states and Accept (`679d26ab…`),
the stop at rest (`5ea0ce22…`) and the stop being edited (here). The critical
review this card asks for was done in two rounds, each by an independent
reviewer, with every finding fixed test first
([RFC-BPR-0106 verification](../../verification/RFC-BPR-0106.md#critical-review)).
Nothing else is open. The card is Done.
