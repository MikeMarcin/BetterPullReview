---
schema: "implement-rfc.card.v1"
id: "BPR-0734"
title: "A design round for the single-pane Read layout"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: []
priority: "P2"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","sequenced_after":"rfc-bpr-0028-attended-v007 verdicts"}
acceptance: [{"criterion":"The owner accepts the one-pane state and the two amendments in an attended session or the round's own review page; recorded on this card and on RFC-BPR-0028","outcome":"pass","revision":"bbc4db98","evidence":{"kind":"artifact","detail":"Attended session cbd48b91000b00f1e72e1ad62c7c384c, candidate BPR-0734-read-one-pane (a design page, no resource claimed), candidate revision design-v7-2026-09-12, fingerprint 1f5c6ab4133593592701203351699f493645590401a981a314f97ed5f4938f05 equal to the prepared candidate's in docs/verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json, decision 31462e78-51ea-4ef5-ba85-231a18098b68, actor human, 2026-09-12T16:03:03Z, sequence 358: accept, note EMPTY. An empty-note accept answers no question; it accepts the candidate's four criteria as written. Criteria 2 and 3 each named a consequence and invited a refusal that did not come, so Review shows one pane too while One pane is checked, and the one-pane gutter carries no whole-file overview. Accepted-as-shown is narrower than accepted-as-written and the difference is recorded on the card: drawn on direction v7 are the spine and wedges (:497-508), the View menu with both items checked and its caption (:559-564) and unified block order with base/head number columns (:502-525); written but never drawn are 'Review takes one pane too' (v7 has four <h2> states, none of them Review, and its own prose at :565 offers the alternative reading with the ask) and the empty-side single ribbon (both drawn changes are two-sided replacements). The amendments landed on RFC-BPR-0028 at bbc4db98: the reworded Modalities text verbatim from BPR-0745 with one punctuation change at the join (the preceding clause's semicolon became a full stop because the insert is four sentences), and AC-05 replaced whole by this card's proposed text. python3 scripts/verify_documentation.py --root . is DOCUMENTATION_OK."}},{"criterion":"BPR-0729's scope and acceptance are rewritten from the accepted state before BPR-0729 is claimed","outcome":"pass","revision":"bbc4db98","evidence":{"kind":"observation","detail":"docs/issues/10-backlog/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md at bbc4db98: the 'Scope, to be fixed by the design round' and 'Acceptance, to be fixed by the design round' placeholders are replaced by concrete Scope and Acceptance written from direction v7, and a 'Corrected by the accepted state, 2026-09-12' section names the four sentences of the card's original prose that the accepted state falsified (the change map in the gutter, the toolbar/strip toggle, the link map being settled work, and the p4merge capture's colours) rather than quietly editing them. The three items accepted in words and never drawn are carried as work needing a drawing or an ask, not as settled pictures. ctest selector names were checked against tests/*/CMakeLists.txt rather than invented; the old acceptance's qt_workbench_window and qt_composed_github were not ctest names at all. One constraint found during the rewrite and recorded there: a new boolean reaches window_layout only if placed before ',open_modality', because window_table() truncates at that literal (src/adapters/sqlite/src/workbench_settings_database.cpp:93-101), and the profile_table version ladder cuts by suffix (:110-125). The card stays Backlog behind its own dependency and is not claimed. issue_board.py validate --rfc RFC-BPR-0028 is valid over 740 cards."}}]
---

# A design round for the single-pane Read layout

[BPR-0729](../30-in-progress/BPR-0729-a-single-pane-read-layout-in-the-manner-of-p4merge.md)
is a new diff surface and RFC-BPR-0028 says it is amended before the slice
starts: a design round on the accepted asset, then the Modalities paragraph
and AC-05. Asked on 2026-09-11 whether that round should start now or after
the v007 verdicts, the owner answered: "Card it, can happen after the
verdict". This card is the round. It is sequenced after the owner's v007
verdicts on the four parked candidates (BPR-0646, BPR-0663, BPR-0676,
BPR-0720) and is not a dependency of any of them; BPR-0729 depends on it.

## Scope

- One new state on the accepted asset
  ([workbench states v5](../../design/RFC-BPR-0028-workbench-direction-v5.html)
  or a v6 beside it): Read in one pane, in the shape of the owner's two
  p4merge screenshots
  ([1](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png),
  [2](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-2-2026-09-11.png)):
  one document in reading order; removed lines kept in place, struck
  through, on their theme field; added lines in place on theirs; unchanged
  text plain; the change map in the gutter; the toggle between two-pane and
  one-pane in the strip or the Read preset. The diff colour themes are
  settled and off-limits ([RFC-BPR-0028 non-goals](../../rfcs/RFC-BPR-0028-review-workbench-direction.md));
  the round applies them to full-row fields, it does not redesign them.
- Where Read's aligned comment cards sit against one column (AC-05), and
  what next/previous change and line identity mean in one column.
- The amendment text for the Modalities paragraph and AC-05, drafted for
  the owner with the state, so BPR-0729's scope and acceptance can be
  fixed on the card the day the round is accepted.
- Owned paths: `docs/design/` (a new asset version, never an edit to an
  accepted one), RFC-BPR-0028, BPR-0729's card. No product source.

## Acceptance

- The owner accepts the one-pane state and the two amendments in an
  attended session, or in the round's own review page; the decision is
  recorded on this card and on RFC-BPR-0028.
- BPR-0729's "to be fixed by the design round" scope and acceptance are
  rewritten from the accepted state before BPR-0729 is claimed.

## Round output, 2026-09-11

The round's state is
[workbench states v6, "Read, one pane"](../../design/RFC-BPR-0028-workbench-direction-v6.html#read-one-pane)
(state 2b, anchor `read-one-pane`). v6 is a copy of the accepted v5 with
that one state added beside Read. v5's other states, its diff colours and
its CSS variables are unchanged; the new state applies the shipped
`--add`, `--del`, `--add2` and `--del2` fields to whole rows and invents
no colour.

### Proposed amendment, the Modalities paragraph

To follow "and keeps comments beside the code;" in RFC-BPR-0028's
Modalities paragraph, as proposed text:

> Read presents the comparison in one of two ways: two whole-document
> panes over the link map, or one document in reading order with the
> other side's lines kept in place and struck through on their theme
> field. The presentation is chosen from the strip or from the Read
> preset, it is remembered per window alongside the modality, and it
> changes nothing about which lines the comparison contains
> ([[BPR-0729]], owner direction 2026-09-11).

### Proposed amendment, AC-05

Replacing AC-05 whole, as proposed text:

> - AC-05 Read shows this file's threads aligned to their lines with
>   unread state and drafts, in both presentations; in one pane a thread
>   names a side and a line and sits against the row carrying that
>   identity, so a thread on a removed line sits against its struck row
>   and a thread on an added line against the added row, and threads
>   that would collide keep document order and are pushed down with
>   their leader still on the anchored row; Discuss shows the list, map
>   and feed with the listed filters.

### What the state decides

- Line identity. Every row keeps the side and the line number it came
  from, in two number columns, base and head. An unchanged row carries
  both numbers; a struck row carries only its base number and is the old
  side's identity; an added row carries only its head number. Comment
  anchoring, quoting and Open in code use the row's own side and number,
  so nothing written against a removed line is re-attributed to the new
  file.
- Next and previous change. A change is one block: its struck rows
  followed by its added rows. `‹` and `›` step between blocks, not rows,
  and the ordinal is the same number in both presentations, so "change 3
  of 11" names the same change in either and switching presentation does
  not move the reader. Stepping lands on the block's first row, which is
  the struck row whenever the change removes anything.
- Unchanged text is plain: no field, no strike. Only change blocks carry
  a field, and the field covers the whole row, number columns included.
- The gutter. One column has nothing to link across, so the strip the
  link map held between the panes becomes a change map at the left: the
  whole file's changes as ticks coloured by kind, the visible window as
  a lens, and the on-screen changes as bands beside the rows they mark.
  The two-pane state's link map is untouched.
- The toggle is two icons in the strip beside the change stepper, on the
  Read preset, remembered per window with the modality.

### Open questions for the owner

1. Block order. The state puts the struck old rows above the new rows,
   the unified-diff order every review tool here uses. The p4merge
   captures put the kept side's block first and the struck block below
   it. The state flips in one line if the owner prefers p4merge's order.
2. Where the choice lives. The state makes one pane a property of Read,
   remembered per window, so leaving for Review and coming back returns
   to one pane while Review keeps two panes. The alternative is a
   property of the window that Review inherits too.
3. The gutter map's standing. RFC-BPR-0028's non-goals rule out changes
   to the link map. The one-pane gutter map is a new element rather than
   a changed one; if the non-goal is to stay literal, the Modalities
   amendment should name it as new.

## Attended candidate, 2026-09-11 (v008)

State 2b, the two amendments and the three open questions above are
candidate `BPR-0734-read-one-pane` in
[rfc-bpr-0028-attended-v008](../../verification/reviews/rfc-bpr-0028-attended-v008/operator-packet.md),
a design page rather than the bundle (no resource claimed), candidate
revision `app-release-e4fdb0ea-2026-09-11.4`; prepared and protected at
sequence 341, undecided, no host running until the owner says so.

## Attended verdict, 2026-09-11 (v008): rejected

Session `cbd48b91…`, candidate revision `app-release-e4fdb0ea-2026-09-11.4`
(a design page; the candidate claimed no resource), actor human; handed back
at sequence 348 with all six candidates decided. Candidate
`BPR-0734-read-one-pane`, decision `bd627bcb`, 2026-09-12T04:32:44Z, one
screenshot
([p4merge in its single-pane mode](../../verification/reviews/rfc-bpr-0028-attended-v008/captures/owner-p4merge-single-pane-link-map-2026-09-11.png)):
**reject**. Verbatim:

> The one-page should also use a link map as the p4merge example provided. Relax the non-goals restriction to allow extending the link-map in this limit direction.

The capture is p4merge showing one merged document with its connector column
still at the window's left edge. It answers the third open question by
rejecting its premise: the one-pane map is not a new element beside a
protected link map, it is the link map at its limit, one column instead of
two. "The gutter" bullet above — "the strip the link map held between the
panes becomes a change map at the left … The two-pane state's link map is
untouched" — is what the owner is correcting, and the Modalities amendment
does not name a new element after all.

RFC-BPR-0028's non-goal against changes to the link map is relaxed for that
direction on the owner's instruction; the relaxation is recorded on the RFC
with this verdict.

The two amendment texts were neither accepted nor refused: they were rejected
with the state they describe and come back with it.

Open questions 1 (block order) and 2 (where the choice lives) are
**unanswered**. The note settles neither, and neither is settled by silence.
They are carried into the next round's ask unchanged.

The state is redrawn over the link map under
[BPR-0745](BPR-0745-the-one-pane-read-state-is-redrawn-over-the-link-map.md).
This card stays parked on human-review; its first criterion is asked again
from the redrawn state, and its second — BPR-0729 rewritten from an accepted
state — stays pending behind it.

## Owner answers and the redrawn state, 2026-09-12 (mandate r11)

The two questions this round left open are answered, verbatim:

1. **Block order**: "don't care, unifed diff order is fine". The round's own
   order stands; no row moves.
2. **Where the choice lives**: "Should be in the view menu like line
   numbers". The strip toggle this round drew is wrong, and so, on the
   faithful reading of the analogy, is "a property of Read with Review
   keeping two panes": Line numbers is one scalar with no modality dimension,
   persisted per window and seeded from a profile default
   (`workbench_interaction.hpp:345`,
   `workbench_settings_database.cpp:44-101`,
   `tests/end_to_end/profile_window_layout_public_path_tests.cpp:191-215`).
   One pane follows it, which gives Review one pane too while it is checked.
   [[BPR-0745]] carries the derivation and offers the flip with the ask.

Question 3, the gutter map's standing, was answered by the v008 verdict
itself and is closed.

The state is redrawn as
[workbench states v7, "Read, one pane"](../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane):
the gutter carries the link map joining each change's struck and added rows
at a spine, the whole-file overview is gone, the strip toggle is replaced by
a View menu drawn below the window, and the Modalities amendment is reworded
accordingly. This card's first criterion is asked again from that state in
the v009 packet; v6 and this round's own text stay recorded against their own
revision, and BPR-0745's candidate does not replace this one.

## Attended candidate, 2026-09-12 (v009)

This card's first criterion is asked again as candidate `BPR-0734-read-one-pane`
in [rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md),
candidate revision `design-v7-2026-09-12`, from
[direction v7](../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane)
rather than from a new candidate replacing it; prepared and protected at
sequence 357, undecided, no host running until the owner says so. The v008
rejection stays recorded against its own revision, and v6 stays where it is as
the round it belonged to.

## Attended verdict, 2026-09-12 (v009): accepted

The owner accepted `BPR-0734-read-one-pane` (revision `design-v7-2026-09-12`,
fingerprint `1f5c6ab4133593592701203351699f493645590401a981a314f97ed5f4938f05`
equal to the prepared candidate's in
[session-identity.json](../../verification/reviews/rfc-bpr-0028-attended-v009/session-identity.json),
decision `31462e78-51ea-4ef5-ba85-231a18098b68`, actor human,
2026-09-12T16:03:03Z), **note empty**.

An empty-note accept answers no question. What it accepts is the candidate's
four criteria as written, and two of those criteria named a consequence and
asked him to refuse it if it was wrong:

1. "The gutter is the link map at its limit: each change's struck and added
   rows converge on a spine, and a change with an empty side draws one ribbon
   rather than two"
2. "One pane is a View item like Line numbers, so it is per window with no
   modality of its own and checking it gives Review one pane too; **say if
   Review should stay two-pane**"
3. "The whole-file overview v6 drew in that gutter is gone, because the
   two-pane state has none; **say if you want one back as its own element**"
4. "The reworded Modalities amendment, and AC-05 unchanged"

Neither refusal came. **Review shows one pane too while One pane is checked**,
and the one-pane gutter carries **no whole-file overview**. Those stand.

### Accepted as shown, and accepted as written: the difference

The mandate for this round was to treat the two consequences as accepted only
so far as the candidate actually showed them. It did not show them, and that is
recorded rather than glossed:

- **Drawn on the page**, so accepted as shown: the spine, two wedges converging
  on a short vertical rule, blue for the current change and `--ink-3` otherwise
  (v7 `:497-508`); the View menu with `Line numbers` and `One pane` both
  checked, with its caption (`:559-564`); unified block order with the `base`
  and `head` number columns (`:502-525`).
- **Written, never drawn**: "Review takes one pane too". v7 has four `<h2>`
  states and **none of them is Review** (`:296`, `:365`, `:466`, `:567`); the
  drawn window is in Read (`:474`); and the page's own prose at `:565` presents
  the Review reading as an ask with the alternative named — "the other reading
  — Read only, Review always two panes — is offered with the ask". He accepted
  a sentence, not a picture.
- **Written, never drawn**: the empty side drawing one ribbon. Both changes on
  the page are two-sided replacements (`:504-507`), so neither a pure insertion
  nor a pure deletion is depicted.
- **An absence, not a removal**: the whole-file overview is simply not in v7.
  Nothing on the page is captioned as having lost it.

[[BPR-0729]]'s rewritten scope carries all three as work that still needs
drawing or an ask when it is built, not as settled pictures.

### What landed on the strength of this accept

- **RFC-BPR-0028's Modalities paragraph** now carries the reworded amendment
  verbatim from [[BPR-0745]], with one punctuation change: the clause it
  follows ended "and keeps comments beside the code;" and the semicolon is now
  a full stop, because the inserted text is four sentences and cannot sit
  inside a semicolon list. No word of the accepted text was altered.
- **RFC-BPR-0028's AC-05** is replaced whole by the amendment text proposed on
  this card and accepted unchanged with the state.
- **[[BPR-0729]]'s scope and acceptance** are rewritten from the accepted
  state, which is this card's second criterion. Its "to be fixed by the design
  round" placeholders are gone, four sentences of its original prose that the
  accepted state falsified are corrected by name rather than quietly edited,
  and one constraint found during that rewrite is recorded there: a new boolean
  reaches `window_layout` only if it is placed before `,open_modality`, because
  `window_table()` truncates at that literal
  (`src/adapters/sqlite/src/workbench_settings_database.cpp:93-101`).
