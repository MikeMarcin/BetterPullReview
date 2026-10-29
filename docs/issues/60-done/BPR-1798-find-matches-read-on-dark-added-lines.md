---
schema: "implement-rfc.card.v1"
id: "BPR-1798"
title: "Find matches stand out on dark added lines"
status: "Done"
rfcs: []
depends_on: []
priority: "P2"
platforms: ["mac", "windows", "linux"]
metadata: {"reservation":"docs/issues/_reservations/BPR-1798--BPR-1798--9ff2c4300d24.json","kind":"bug","found_by":"owner, 2026-10-28","related_cards":["BPR-1503"]}
acceptance: [{"criterion": "AC1", "outcome": "pass", "revision": "3ea16fee141e7d8ff65974d1ed142cb39e35104a", "evidence": {"kind": "command", "detail": "qt_appearance_profile test_every_builtin_profile_rings_find_matches_on_every_ground: light, dark and both high-contrast profiles, six row grounds bare and under the selection wash, match and current, all at least 3:1 against the ground and the washed ground. core_appearance_contrast test_find_ring_reaches_the_indicator_floor_on_its_ground."}}, {"criterion": "AC2", "outcome": "pass", "revision": "3ea16fee141e7d8ff65974d1ed142cb39e35104a", "evidence": {"kind": "command", "detail": "No palette value moved, so the published contrast audit still holds the washes to the text floor (qt_appearance_profile). application_source_find (and its 1.25 and 2.0 scale runs) still counts one match wash and one current wash; the current ring is 2 px, the others 1 px; qt_diff_workbench_scale_100 and _200 pass."}}, {"criterion": "AC3", "outcome": "pass", "revision": "3ea16fee141e7d8ff65974d1ed142cb39e35104a", "evidence": {"kind": "command", "detail": "application_source_find samples the painted dark pane: the current ring #FFAA47 against the selected added row #1A3E40 is 6.13:1. With BPR_FIND_RENDER_ROOT set it writes 04-dark-added-match-pane, 04-dark-many-matches-pane and the light 06-*-pane renders (outside the repository). The owner's own look stays recommended as an attended Accept/Reject."}}]
---

# BPR-1798: Find matches stand out on dark added lines

Owner, 2026-10-28: "The find result highlight is very hard for me to see on
the dark mode theme when it lands in an added block of code. Like say search
for "valid" and get the result hit in a line `const bool fresh =frame.valud;`"

[BPR-1503](BPR-1503-find-in-pane-becomes-a-floating-vs-code-style-find-widget.md)
lays Find's match and current-match washes over the row at
`selection_wash_alpha` (72/255) and holds them to the text floor with syntax
colours read through them. That floor keeps a dark-theme wash dark, so it
barely moves the row: the dark match wash (`#5C4214`) composites to `#29391E`
on the added row (`#153622`), 1.07:1 against the row, and the current-match
wash (`#754D00`) to `#303C18`, 1.13:1. On the intraline-added mark the match
reaches 1.00:1, which is no difference at all. Only the current match had an
outline (`#825E18`, 1.5 px), and it was tuned against the bare canvas, not the
row: 2.25:1 on the added row.

## Acceptance

- AC1. In every built-in theme, every Find match carries a ring that
  reaches the indicator ratio (3:1) against both the ground the match lands
  on and that ground under the match's wash, on code, added, removed,
  intraline, moved and selected rows.
- AC2. The current match stays distinct: a heavier ring and its stronger
  wash. The washes keep their text floor, and the light theme keeps its
  wash colours.
- AC3. The painted dark pane shows a ring at the indicator ratio against an
  added row, and offscreen renders of the dark and light cases exist for the
  owner to judge.

## Built

The washes are unchanged; the ring does the work, since no dark wash can
gain 3:1 on a dark row and still leave light syntax text at 4.5:1.

- `core::find_match_ring(palette, ground, current)` takes the theme's
  attention amber (`state_attention`) and moves it with
  `readable_signal_ink` to 3:1 against the ground and the ground under the
  match's wash. In the dark theme the amber `#D29922` already reads on the
  added row, so it is kept: 5.26:1 against the row, 4.90:1 against a match
  wash and 4.66:1 against the current wash. Where the ground is bright (the
  high-contrast dark intraline marks) the ring turns dark instead.
- The diff workbench reads each match's ground from the row selections
  painted under it, composited in paint order, so a selected added line is
  measured as the added fill under the selection wash. It derives one ring
  colour per distinct ground and draws every ring after the text: 2 px for
  the current match, 1 px for the others. Each ring walks only its own
  blocks.
- Light theme: the rings are the darker amber `#AD7A00`, or a burnt orange
  where a darker ground needs one. The light washes did not move.

## Owner review

Attended review session 2063d344831eda3c7c7012ecd339bb77 (state /Users/jaeger/bpr-attended/bpr-1798-find-rings-v001/state, closed 2026-10-29): owner accepted both criteria at 3ff95dc21 — dark added-line matches visible with a distinct current match, and ringing every match in light theme. Frozen build removed.
