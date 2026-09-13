---
schema: "implement-rfc.card.v1"
id: "BPR-0729"
title: "A single-pane Read layout in the manner of p4merge"
status: "Done"
rfcs: ["RFC-BPR-0028"]
depends_on: ["BPR-0734"]
priority: "P1"
metadata: {"milestone_plan":{"id":"ux-redesign-pack-0028","path":"docs/rfcs/plans/ux-redesign-pack-0028/milestone-plan.json"},"owner_direction":"2026-09-11","attended_session":"cbd48b91000b00f1e72e1ad62c7c384c","attended_decision":"205ac8b2-30fd-4339-8d08-51c621ab7fc8"}
acceptance: [{"criterion":"One-pane rows and identity at three scales","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"qt_diff_workbench_scale_{100,125,200} and inspected rendered crops; build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"Gutter and empty-side ribbons at three scales","outcome":"pass","revision":"83d7e676","evidence":{"kind":"artifact","detail":"Three-scale source and gutter crops inspected under build/dev/tests/adapters_qt; build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"View menu, Review one-pane and core policy","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"qt_workbench_window_scale_{100,125,200} and core_workbench_interaction in build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"SQLite schema 7 and version 6 migration","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"sqlite_workbench_settings_storage and legacy/interrupted migration checks; build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"Per-window choice and restart","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"application_profile_window_layout_public_path; build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"Composed GitHub side-specific thread anchors","outcome":"pass","revision":"83d7e676","evidence":{"kind":"artifact","detail":"qt_composed_github_scale_{100,125,200} including one-pane collisions and inspected crops; build/logs/bpr-0729-d26-final-focused.log"}},{"criterion":"Host composition and unchanged diff public path","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"Application composition contracts and diff public path in build/logs/bpr-0729-d26-final-contracts.log; core/boundary checks in card evidence"}},{"criterion":"Format, tidy, boundaries and documentation","outcome":"pass","revision":"83d7e676","evidence":{"kind":"command","detail":"build/logs/bpr-0729-d26-{boundaries,format-final,tidy-correction,doc-gate}.log; card records whitespace check"}},{"criterion":"Owner accepts Review one-pane and empty-side layout on signed bundle","outcome":"pass","revision":"app-release-a6dc1565-2026-09-12.6","evidence":{"kind":"observation","detail":"Session cbd48b91000b00f1e72e1ad62c7c384c sequence 367 candidate BPR-0729-one-pane-layout fingerprint 70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c decision 0f6d7348-e230-41ce-a815-af50ebf2bb0d outcome accept, empty note; sequence 368 hand-back pending []"}}]
---

# A single-pane Read layout in the manner of p4merge

The owner chose, on the options candidate `BPR-0724-read-width` in the
v006 attended session (decision `205ac8b2`, 18:42:22Z), "A single-pane
Read layout (one document with the other side's changes marked) as a new
slice", and wrote: "Toggle to single pane view like p4merge's layout would
be good". Their two screenshots of p4merge
([1](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-1-2026-09-11.png),
[2](../../verification/reviews/rfc-bpr-0028-attended-v006/captures/owner-p4merge-single-pane-2-2026-09-11.png))
show the shape: one document in reading order; removed lines kept in place,
struck through, on a green field; added lines in place on a purple field;
unchanged text on the plain background; a change map in the left gutter;
a toolbar toggle between the two-pane and one-pane presentations.

## Why this is a new slice

RFC-BPR-0028's Read is two whole-document panes over the DiffLinkMap
(`diff_workbench.cpp`, `Comparison->setSizes({500, 500})`), and the diff
colour themes and the link map are settled work the RFC says are rendered
as they ship. A one-pane presentation is a new diff surface: it needs a
row model that interleaves both sides in one column (the core already
computes the aligned groups; the surface is presentation over that
result), its own thread alignment for Read's comment cards (AC-05), a
toggle in the strip or the Read preset, per-window persistence of the
choice, and the diff themes applied to full-row fields rather than to the
two panes. The RFC is amended before the slice starts: a design round on
the accepted asset (a one-pane state beside the two-pane one), then the
Modalities paragraph and AC-05.

## Corrected by the accepted state, 2026-09-12

The two paragraphs above are left as they were written, because they are the
record of how this card was carded. Four of their sentences read differently
now that the state is accepted, and they are corrected here rather than
quietly edited.

- "a change map in the left gutter". The gutter is the **link map at its
  limit**, not a change map and not a new element beside one. The owner
  rejected that premise in the v008 session: "The one-page should also use a
  link map as the p4merge example provided. Relax the non-goals restriction
  to allow extending the link-map in this limit direction" ([[BPR-0734]],
  decision `bd627bcb`, 2026-09-12T04:32:44Z). With one column the map has no
  second pane to cross to, so it joins the two halves of each change.
- "a toolbar toggle between the two-pane and one-pane presentations", and "a
  toggle in the strip or the Read preset" below it. p4merge has that
  toolbar; this product does not take it. Asked where the choice lives, the
  owner answered "Should be in the view menu like line numbers" (2026-09-12,
  [[BPR-0745]]). The strip is not touched and there is no Read-preset
  property. The Scope this rewrite replaced repeated the strip toggle and no
  longer does.
- "the diff colour themes and the link map are settled work the RFC says are
  rendered as they ship". Still true of the themes, no longer true of the
  link map: RFC-BPR-0028's Non-goals now permit extending it to the one-pane
  presentation, and to nothing wider.
- "removed lines kept in place, struck through, on a green field; added
  lines in place on a purple field" describes the owner's screenshots
  accurately and is not this product's palette. The accepted state applies
  the shipped `--del` to struck rows and `--add` to added rows, so green
  here means added, the opposite of the capture. It invents no colour.

## Scope

One pane is a presentation over the diff the core already computes, chosen
from the View menu and remembered per window. What it must look like is
fixed by
[direction v7, "Read, one pane"](../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane),
accepted 2026-09-12 (below); what follows is that state read as work.

- **The row model.** One column over the existing aligned groups. Unchanged
  text is plain: no field, no strike. A change is one block, its struck rows
  and then its added rows, unified-diff order. Asked which order to draw,
  the owner answered "don't care, unifed diff order is fine" (2026-09-12),
  so the block order is deliberately *not* the p4merge captures' order,
  which puts the kept side's block first. The field covers the whole row,
  number columns included, and is the shipped `--del` on struck rows and
  `--add` on added rows applied to the row rather than to a pane; the
  word-level marks inside a changed line are the ones the two panes already
  draw. The state invents no colour.
- **Line identity.** Two number columns, base and head. An unchanged row
  carries both; a struck row carries only its base number and is the old
  side's identity; an added row only its head number. Comment anchoring,
  quoting and Open in code use the row's own side and number, so nothing
  written against a removed line is re-attributed to the new file.
- **Navigation.** `‹` and `›` step whole blocks, not rows, and land on the
  block's first row, which is the struck row whenever the change removes
  anything. The ordinal is the same number in both presentations, so "change
  3 of 11" names the same change in either and switching presentation does
  not move the reader.
- **Threads, AC-05.** A thread names a side and a line and sits against the
  row carrying that identity: a thread on a removed line against its struck
  row, one on an added line against the added row. Threads that would
  collide keep document order and are pushed down with their leader still on
  the anchored row. That is the AC-05 amendment text accepted with the
  state; AC-05 itself is otherwise unchanged.
- **The gutter is the link map at its limit.** For each change the struck
  rows and the added rows leave the code's edge over their own rows and
  converge on a short vertical spine; the spine is the change. The current
  change's spine takes the signal blue the current row's edge already takes
  and the others take `--ink-3`; the ribbon fills and edges are the two-pane
  ribbons' own. The map spans the rows on screen and nothing more. A change
  with an empty side draws one ribbon rather than two.
- **No whole-file overview.** v6 drew one in this gutter and it is gone,
  because the two-pane state has never had one. If one is ever wanted it is
  a separate element and a separate decision, not part of this slice.
- **Where the choice lives.** A checkable `One pane` in the View menu beside
  `&Line numbers` (`src/adapters/qt/src/workbench_window.cpp:5919-5951`).
  No shortcut is proposed. Like `Line numbers` it dispatches an intent and
  never decides its own check mark, and the core refuses it while no
  comparison surface is active
  (`src/core/src/workbench_interaction.cpp:154-161`).
- **One scalar, no modality dimension.** It sits beside `LineNumbers` in
  `workbench_interaction_state`
  (`src/core/include/bpr/core/workbench_interaction.hpp:345`) with its own
  set and toggle intents and its delta (`:198-208`, `:304-321`), and
  `apply(set_workbench_modality_intent)` does not touch it. **So Review
  shows one pane too while it is checked**, and Read does not reclaim two
  panes when the modality changes. That consequence was put to the owner
  with the flip offered and was accepted, not refused.
- **Persistence, and the schema move.** Per window in `window_layout`,
  seeded for a new window from the single-row `workbench_preferences`, both
  from the same DDL
  (`src/adapters/sqlite/src/workbench_settings_database.cpp:44-89`,
  `:93-101`), and carried to the window by the host's projection
  (`src/application/src/workbench_application_host.cpp:1337-1344`, `:1388`).
  The new boolean lands in **both** tables and moves
  `workbench_settings_schema_version` from 6 to 7
  (`src/adapters/sqlite/src/workbench_settings_database.hpp:35`, ladder at
  `.cpp:110-125` and `:139-157`), with precedent at v3, v4 and v6. Its
  default is the behaviour a window had before the preference existed:
  **two panes**.
- **Where that column has to sit**, read off the file rather than assumed.
  `window_table()` truncates the shared DDL at `,open_modality` (`:99`), so
  a column appended after `drawer_controls` (`:88`) would reach
  `workbench_preferences` only. To land in both it goes before
  `,open_modality`, beside `line_numbers` — which makes it a column in the
  middle, while `profile_table`'s version ladder cuts by suffix today
  (`:110-125`), so the ladder needs a way to say that version 6's table is
  today's without this column. `migrate_from`'s column list (`:141-143`)
  does not need the new name, because a migrated row takes the default. Two
  further facts from the same file: the connection caps a table at sixteen
  columns (`:196`) and the profile table has fourteen, so this is the
  fifteenth.
- **Accepted in words, never drawn.** Two parts of the accepted state are
  stated criteria with no picture behind them. The builder draws them first
  or brings them back as an ask; neither is invented quietly. **Review in
  one pane**: v7 draws four states (`v7:296`, `:365`, `:466`, `:567`) and
  none of them is Review — the one-pane mock is Read (`v7:474`) — and the
  page's own prose presents the Review reading as an ask with the other
  reading named (`v7:565`). **The empty-side ribbon**: both changes drawn
  are two-sided replacements (`v7:514-515`, `:524-525`, ribbons at
  `:498-507`), so neither a pure insertion nor a pure deletion is depicted.
  What *is* drawn, and so is settled picture rather than prose, is the spine
  with its two converging wedges (`v7:497-508`), the View menu with `Line
  numbers` and `One pane` both checked and its caption (`v7:559-564`), and
  the unified block order with the `base` and `head` number columns
  (`v7:510-525`). The whole-file overview's absence is an absence and not a
  captioned removal: nothing on the page marks where it was.
- **Owned paths.** `src/adapters/qt/src/diff_workbench.cpp` and
  `src/adapters/qt/include/bpr/qt/diff_workbench.hpp` (the one-pane surface
  beside the two panes; the two-pane splitter at `:826` and the link-map
  wiring at `:852-884` are its neighbours); the link map
  (`src/adapters/qt/include/bpr/qt/diff_link_map.hpp` and its source)
  extended to the one-column case; `src/adapters/qt/src/workbench_window.cpp`
  for the View item; the core scalar and its intents; the workbench settings
  record, its SQLite store and the host projection that carries it; their
  tests and render evidence.
- **Not in scope.** The core's computed diff result, which is not changed:
  this is presentation over the aligned groups it already produces. The
  two-pane ribbons, the diff themes and the thread map, which stay settled
  work ([RFC-BPR-0028 non-goals](../../rfcs/RFC-BPR-0028-review-workbench-direction.md)).
  The strip, which gains nothing. A whole-file overview. A shortcut for the
  View item.

## Acceptance

- `qt_diff_workbench_scale_{100,125,200}`: over a fixture carrying a
  two-sided replacement, a pure insertion and a pure deletion, the one-pane
  surface shows the same lines the two panes show, in one column, in
  unified-diff order, and each row's displayed identity is asserted against
  the expected data — both numbers on an unchanged row, the base number only
  on a struck row, the head number only on an added row. The rendered
  `diff-workbench-{100,125,200}.png` is inspected, not only produced.
- `qt_diff_workbench_scale_{100,125,200}`, the gutter: a two-sided change
  draws both ribbons converging on one spine; a pure insertion draws the
  added wedge against the spine alone and a pure deletion the struck one
  alone; the current change's spine is the signal colour and the others
  `--ink-3`; the map spans the rows on screen and carries no whole-file
  overview. The two empty-side cases are drawn here for the first time,
  because v7 depicts neither, so if their shape does not follow from the
  two-sided case it comes back as an ask before it is pinned.
- `qt_workbench_window_scale_{100,125,200}`: the View menu carries a
  checkable `One pane` beside `Line numbers`, with no shortcut; triggering
  it dispatches the intent and the check mark follows the published
  projection rather than the click; it is disabled with no active comparison
  surface, for the reason `Line numbers` is; and with it checked, changing
  the modality to Review leaves Review in one pane.
- `core_workbench_interaction`: the scalar defaults to two panes, the set
  and toggle intents move it, `set_workbench_modality_intent` does not move
  it, and the intent is refused with `no_active_workbench` while no
  comparison surface is active.
- `sqlite_workbench_settings_storage`: the flag round-trips through schema
  version 7 in both `window_layout` and `workbench_preferences`; a literal
  version-6 file migrates with every existing column intact and reads as two
  panes afterwards; a value past the column's check is refused with
  `schema_mismatch`; the version 1 to 5 migration tests and the
  interrupted-migration test still pass.
- `application_profile_window_layout_public_path`: window A checking One
  pane does not move window B, A's choice survives a restart, and a fresh
  window takes the profile default. That is the course which already proves
  it for `line_numbers`
  (`tests/end_to_end/profile_window_layout_public_path_tests.cpp:191-215`),
  extended rather than copied.
- `qt_composed_github_scale_{100,125,200}`: with One pane checked, a thread
  anchored to a removed line sits against its struck row and one anchored to
  an added line against the added row; two threads that would collide keep
  document order and the lower one is pushed down with its leader still on
  its anchored row. Each state is pinned in a rendered crop. This is AC-05
  in one pane.
- `application_workbench_host` and
  `application_workbench_preferences_consumers` carry the flag from the
  record to the window; `application_diff_workbench_public_path` and
  `core_diff_compute` pass unchanged, because the computed result is not
  touched; `application_composition_contract` passes with digests repinned;
  `check_boundaries`, strict tidy, `clang-format --Werror` and the
  documentation gate are clean.
- **Attended, and last.** The owner accepts the layout on the signed bundle
  in an attended session. Two of its parts were accepted as words and never
  drawn, so they are shown rather than described in that ask: Review in one
  pane, and a change with an empty side.

## Implementation and verification, 2026-09-12

Source commit `3dcbf237` implements the accepted one-pane presentation. The
View menu projects the per-window choice without a shortcut; the unified
source shows base-then-head rows with their own number columns, full-row
fields, strike and intraline marks, and a left link map with empty-side
ribbons. Thread anchors remain side-specific in Review. Core, application and
SQLite carry the setting through schema 7; literal version-6 and interrupted
migrations preserve older settings. No computed diff policy changed.

The first Qt assertion failed before the unified source existed
(`build/logs/bpr-0729-d26-surface-red-{build,test}.log`). Independent review
found that a pure-deletion reading position could jump after a two-pane
round trip; the added regression reproduced row 70 becoming row 8
(`build/logs/bpr-0729-d26-review-roundtrip-red.log`), then passed with row
70 retained exactly at all three scales. Review also found that the
`qt_composed_github_scale_*` selectors had no one-pane thread case; the new
GitHub-projected case checks left/right anchors, collision order and a
rendered crop in each scale.

At the source commit, the 14 focused selectors pass
(`build/logs/bpr-0729-d26-final-focused.log`), as do
`application_composition_contract`, `application_workbench_composition_contract`
and `application_diff_workbench_public_path`
(`build/logs/bpr-0729-d26-final-contracts.log`). `core_diff_compute` and
`architecture_boundaries` passed in the pre-correction contract batch and
were unaffected by the Qt-only correction. `check_boundaries`, touched-file
`clang-format --Werror`, strict tidy, the documentation gate and whitespace
checks pass (`build/logs/bpr-0729-d26-{boundaries,format-final,tidy-correction,doc-gate}.log`).
The fresh one-pane surface, gutter and composed thread crops at 100%, 125%
and 200% were inspected under `build/dev/tests/adapters_qt/`. These offscreen
renders do not answer the signed-bundle criterion. The remaining criterion is
the owner's attended verdict with **Review in one pane** and **an empty-side
change** shown; no verdict is inferred here. The card is Parked on
`human-review` until that evidence arrives.

The signed bundle and one-question candidate are ready in
[attended v010](../../verification/reviews/rfc-bpr-0028-attended-v010/operator-packet.md):
session `cbd48b91000b00f1e72e1ad62c7c384c`, candidate
`BPR-0729-one-pane-layout`, revision `app-release-a6dc1565-2026-09-12.6`,
fingerprint `70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c`,
ready/protected at sequence 366 with no decision. The tests-disabled bundle
was built from `83d7e676`, executable sha256
`a6dc1565908cfacc5eb7d573d8d05983c51aa10780db2052e1c138682870eaf2`,
and `codesign --verify --deep --strict` passed. No host or watch is running.

## Signed-bundle acceptance, 2026-09-12 (d27, mandate r19)

The attended session recorded sequence 367 for the exact protected candidate
`BPR-0729-one-pane-layout`, revision `app-release-a6dc1565-2026-09-12.6`,
fingerprint `70a2a9417b2532450cf66f71228fbabe1f627de748ed8ea25767cecec875228c`.
Decision `0f6d7348-e230-41ce-a815-af50ebf2bb0d` is the owner's explicit
**accept** with no note. It answers the one question showing Review in one
pane and the empty-side ribbons on the signed bundle. Sequence 368 is the
session hand-back with no pending candidates; it adds no second verdict.
The eight engineering criteria above retain their d26 evidence and the last,
attended criterion now passes on this digest. This decision says nothing
about [[BPR-0646]]'s epoch handle criterion.

All nine recorded acceptance criteria pass. The card remains in Parked with
`parked_on: immutable-review-link` solely because the board's Done move would
rewrite the protected v010 operator packet's relative link to this card.
`issue_board.py finish --dry-run` refused that protected-content change. The
packet is historical signed evidence and has not been edited; moving the card
without retaining its path would also break the documentation link gate. The
board disposition needs an explicit safe historical-link migration before the
card can enter Done. This is a tracker constraint, not another owner ask.

## Owner direction, 2026-09-11

Asked whether the design round should start now or after the v007
verdicts: "Card it, can happen after the verdict". The round is
[BPR-0734](BPR-0734-a-design-round-for-the-single-pane-read-layout.md),
sequenced after the v007 verdicts; this card depends on it and its scope
and acceptance are fixed from the accepted state before it is claimed.

## Packet pointer, 2026-09-11 (v007)

The current attended packet is
[rfc-bpr-0028-attended-v007](../../verification/reviews/rfc-bpr-0028-attended-v007/operator-packet.md);
this card is not a v007 candidate. It waits for the design round
[[BPR-0734]], which follows the v007 verdicts. The v006 packet above is
historical.

## Packet pointer, 2026-09-11 (v008)

The current attended packet is
[rfc-bpr-0028-attended-v008](../../verification/reviews/rfc-bpr-0028-attended-v008/operator-packet.md);
this card is not a v008 candidate. The design round [[BPR-0734]] it waits
for is asked there; this card is rewritten from the accepted state. The
v006 and v007 packets above are historical.

## Packet pointer, 2026-09-12 (v009)

The current attended packet is
[rfc-bpr-0028-attended-v009](../../verification/reviews/rfc-bpr-0028-attended-v009/operator-packet.md);
this card is not a v009 candidate. [[BPR-0734]] is asked there again from the
redrawn state, [direction v7](../../design/RFC-BPR-0028-workbench-direction-v7.html#read-one-pane),
which [[BPR-0745]] produced; this card is still rewritten from whatever the
owner accepts, and that rewrite is BPR-0734's second criterion. Earlier packets
are historical.

Two facts landed under [[BPR-0745]] that this card's rewrite will need. The
owner's answer on where the presentation choice lives is "should be in the view
menu like line numbers", and Line numbers in this codebase is one scalar with no
modality of its own (`workbench_interaction.hpp:345`), persisted per window in
`window_layout` and seeded for a new window from the single-row
`workbench_preferences`, both from the same DDL
(`workbench_settings_database.cpp:44-101`), proved by
`tests/end_to_end/profile_window_layout_public_path_tests.cpp:191-215`. So the
presentation is per window and Review inherits it. And a new boolean in that
DDL lands in **both** tables and moves `workbench_settings_schema_version` from
6 to 7 (`workbench_settings_database.hpp:35`, ladder at `.cpp:113-116,141-143`),
with precedent at v3, v4 and v6; the default to choose is the behaviour a window
had before the preference existed, which is two panes.

## Scope and acceptance rewritten from the accepted state, 2026-09-12

The two placeholder sections this card carried, "Scope, to be fixed by the
design round" and "Acceptance, to be fixed by the design round", are replaced
by the `## Scope` and `## Acceptance` above. That is [[BPR-0734]]'s second
criterion, and it is done on this authority and no other: candidate
`BPR-0734-read-one-pane`, revision `design-v7-2026-09-12`, fingerprint
`1f5c6ab4133593592701203351699f493645590401a981a314f97ed5f4938f05`, decision
`31462e78-51ea-4ef5-ba85-231a18098b68`, actor human, 2026-09-12T16:03:03Z,
session `cbd48b91000b00f1e72e1ad62c7c384c`: **accept**, with an empty note.

The candidate's four stated criteria were the link-map gutter with its
empty-side case; One pane as a View item like Line numbers, per window with
no modality of its own, "so ... checking it gives Review one pane too"; the
whole-file overview gone; and the reworded Modalities amendment with AC-05
unchanged. An empty-note accept answers no question: it accepts those
criteria as written. Two of them invited a refusal that did not come — "say
if Review should stay two-pane" and "say if you want one back as its own
element" — so both consequences stand, and the Scope above carries them as
decided rather than as open.

Nothing else on this card moves. It is still Backlog behind [[BPR-0734]], and
this rewrite claims no work.

## Current packet pointer, 2026-09-12 (v011)

The current [v011 attended packet](../../verification/reviews/rfc-bpr-0028-attended-v011/operator-packet.md) asks only [[BPR-0646]]'s epoch-handle criterion. This card's v010 acceptance remains bound to its protected packet and is not asked again; its immutable-review-link board constraint is unchanged.

## Board finish with historical links preserved, 2026-09-12 (d30, mandate r20)

All nine current acceptance records pass, including the owner's exact v010
one-pane verdict. This card is **Done** at `73371ea1`. The board finish
repaired live links while leaving the protected v010 packet byte-identical;
the documentation gate resolves its historical `40-parked` locator through
this unique canonical card filename in `60-done`. The preceding packet
pointer records the d29 constraint, not this final status. No criterion was
re-asked.
